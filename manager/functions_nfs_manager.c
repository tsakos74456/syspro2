#include "functions_nfs_manager.h"
#define BUFSIZE 48000

char* get_timestamp() {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    char *buffer=malloc(256*(sizeof(char)));  // Enough space for "YYYY-MM-DD HH:MM:SS\0"
    strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}

bool check_flags_nfs_manager(const int argc, char **argv){
    if (argc != 11 && argc != 9){
        fprintf(stderr, "Use the arguments correctly!\n");
        return 0;
    }
    if (strcmp(argv[1],"-l") != 0){
        fprintf(stderr, "The flag -l is not used correctly!\n");
        return 0;
    }
    if (strcmp(argv[3],"-c") != 0){
        fprintf(stderr, "The flag -c is not used correctly!\n");
        return 0;
    }
    if (strcmp(argv[5],"-n") != 0 && strcmp(argv[5],"-p") != 0){
        fprintf(stderr, "The flag -n (if exists) or -p is not used correctly!\n");
        return 0;
    }
    if (strcmp(argv[7],"-p") != 0 && strcmp(argv[7],"-b") != 0){
        fprintf(stderr, "The flag -p or -b is not used correctly!\n");
        return 0;
    }
    if (argc == 11 && strcmp(argv[9],"-b") != 0){
        fprintf(stderr, "The flag -b is not used correctly!\n");
        return 0;
    }
    return 1;
}

void perror_exit(const char *msg){
    perror(msg);
    exit(EXIT_FAILURE);
}

int create_socket(int port){
    // create the socket with the console
    int sock;
    struct sockaddr_in server;
    struct sockaddr * serverptr = (struct sockaddr*) &server;

    if((sock = socket(AF_INET,SOCK_STREAM,0)) < 0)
        perror_exit("Socket creation failed!\n");
    
    server.sin_port = htons(port);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    
    // reuse address of the socket if needed
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(EXIT_FAILURE);
    }

    if(bind(sock,serverptr,sizeof(server)) < 0)
        perror_exit("Failed to bind the socket\n");
    
    if(listen(sock,5))
        perror_exit("Failed to listen on the socket");
    return sock;
}

void destroy_sync_info(Pointer info){
    Sync_info *curr = info;

    free(curr->source_dir);
    free(curr->target_dir);
    free(curr->source_host);
    free(curr->target_host);
    if(curr->status != NULL)
        free(curr->status);
    if(curr->last_sync_time != NULL)
        free(curr->last_sync_time);
    free(curr);
}

Admin *create_admin(Queue scheduled, const int capacity){
    Admin *curr = malloc(sizeof(Admin));
    if(curr == NULL){
        printf("Failed to allocate memory for admin\n");
        exit(1);
    }
    curr->scheduled_tasks = scheduled;
    curr->max_capacity = capacity; 
    curr->shutdown_flag = 0;
    if (pthread_mutex_init(&curr->mutex, NULL) != 0) {
        free(curr);
        printf("pthread_mutex_init failed in create admin\n");
    }
    if (pthread_cond_init(&curr->cond, NULL) != 0) {
        pthread_mutex_destroy(&curr->mutex);
        free(curr);
        printf("pthread_cond_init failed in create admin\n");
    }
    return curr;
}



void destroy_threadpool_info(Pointer info){
    Threadpool_info *curr = info;
    free(curr->filename);
    free(curr->operation);
    free(curr);
}


void read_config_file(List mem_info_store, const int config_fd, Admin *admin){
    char *buffer = malloc(BUFSIZE);
    if (buffer == NULL) {
        perror("Allocation failed for buffer");
        exit(1);
    }

    char ch;
    int pos = 0;
    Sync_info *info;
    bool flag_for_source = 1;  // flag to know if in the config we are in the source or in the target if it is 1 -> source, 0 ->target
    while(read(config_fd,&ch,1) >0){
        if(pos < BUFSIZE - 1)
            buffer[pos++] = ch;
        
        // make the sync info and read the source dir
        if(ch == '@' && flag_for_source){
            buffer[pos - 1] = '\0';
            // initialize info and allocate memory
            info = malloc(sizeof(Sync_info));
            info->source_dir = strdup(buffer);
            info->active = 0;
            info->last_sync_time = NULL;
            info->status = NULL;
            info->error_count = 0;
            if (info->source_dir == NULL) {
                perror("Failed to duplicate source_dir");
                free(info);  
                exit(1);
            }
            pos = 0;
        }
        // source ip
            else if(ch == ':' && flag_for_source){
                buffer[pos - 1] = '\0';
                info->source_host = strdup(buffer);
                if (info->source_host == NULL) {
                    perror("Failed to duplicate source_host");
                    free(info);  
                    exit(1);
                }
                pos = 0;
            }
        
        // source port
        else if(ch == ' '){
            buffer[pos - 1] = '\0';
            int port = atoi(buffer);
            info->source_port = port;
            pos = 0;
            flag_for_source = 0;
        }

        // target dir
        else if(ch == '@' && !flag_for_source){
            buffer[pos - 1] = '\0';
            info->target_dir = strdup(buffer);
            if (info->target_dir == NULL) {
                perror("Failed to duplicate target_dir");
                free(info);  
                exit(1);
            }
            pos = 0;
        }
        // target ip
            else if(ch == ':' && !flag_for_source){
                buffer[pos - 1] = '\0';
                info->target_host = strdup(buffer);
                if (info->target_host == NULL) {
                    perror("Failed to duplicate target_dir");
                    free(info);  
                    exit(1);
                }
                pos = 0;
            }
        
        // target port
        else if(ch == '\n'){
            buffer[pos - 1] = '\0';
            int port = atoi(buffer);
            info->target_port = port;
            pos = 0;
            flag_for_source = 1;
            
            //if the source dir is not already in the list, insert the pair otherwise print according message and continute
            if(!check_existance_pair(info->source_dir,mem_info_store)){
                list_insert_next(mem_info_store,list_first(mem_info_store),info);
                Threadpool_info *job = malloc(sizeof(Threadpool_info));
                if(job == NULL){
                    printf("failed to allocate job\n");
                    destroy_sync_info(info);
                    exit(1);
                }
                job->info = info;
                job->filename = NULL;
                job->operation = strdup("LIST");
                pthread_mutex_lock(&admin->mutex);
                while (queue_size(admin->scheduled_tasks) >= admin->max_capacity)
                    pthread_cond_wait(&admin->cond, &admin->mutex);
                
                queue_insert_back(admin->scheduled_tasks,job);
                pthread_cond_signal(&admin->cond);
                pthread_mutex_unlock(&admin->mutex);
            }
            else{
                char *time = get_timestamp();
                char *update = malloc(1024);
                if(update == NULL){
                    printf("Failed memory allocation for update");
                    exit(1);
                }
                snprintf(update,1024,"[%s] Already in queue: %s@%s:%d\n",time,info->source_dir,info->source_host,info->source_port);
                printf("%s",update);
                free(update);
                free(time);
                destroy_sync_info(info);
            }
        }
    }
    free(buffer);
}

bool check_existance_pair(const char *source_dir, const List mem_info_store){
    for(ListNode node = list_first(mem_info_store) ; node != LIST_EOF ; node = list_next(mem_info_store,node)){
        Sync_info *info = (Sync_info *)list_node_value(mem_info_store,node);
        if(strcmp(source_dir,info->source_dir) == 0){
            // match found
            return true;
        }
    }
    // no match found
    return false;
}


int connect_with_socket(const char *host_name, const int port){
    int sockfd;
    struct sockaddr_in server;
    struct sockaddr *server_ptr = (struct sockaddr *)&server;
    struct hostent *rem;
    if((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0){
        printf("Socket creation failed!\n");
        return -1;
    }
    if((rem = gethostbyname(host_name)) == NULL){
        printf("gethostbyname error");
        return -1;
    }

    server.sin_family = AF_INET;
    memcpy(&server.sin_addr,rem->h_addr_list[0],rem->h_length);
    server.sin_port = htons(port);

    if(connect(sockfd,server_ptr,sizeof(server)) < 0){
        printf("connect to socket error");
        return -1;
    }
    return sockfd;
}

void create_threads(const int capacity, Admin *admin){
    worker_tids = malloc(capacity* sizeof(pthread_t));
    for (int i = 0 ; i < capacity ; i++){
        
        int err;
        if((err = pthread_create(&worker_tids[i],NULL,worker_thread,admin)))
            perror_exit("pthread_create");
        
    }

}

void* worker_thread(void* args){
    Admin *admin = (Admin *)args;
         
    while (1) {
        pthread_mutex_lock(&admin->mutex);
        while (queue_size(admin->scheduled_tasks) == 0 && !admin->shutdown_flag ){

            pthread_cond_wait(&admin->cond, &admin->mutex);
        }

        if (admin->shutdown_flag) {
            pthread_mutex_unlock(&admin->mutex);
            return NULL;
        }
        Threadpool_info* task = queue_front(admin->scheduled_tasks);
        // i am copying these cause when i remove front task is freed
        Sync_info *sync = task->info;
        char *operation = strdup(task->operation);
        char *filename = NULL;
        if(strcmp(task->operation,"PUSH/PULL") == 0)
            filename = strdup(task->filename);
        sync->active = 1;
        queue_remove_front(admin->scheduled_tasks);
        // a thread might be in sleep cause cannot insert info in the threadpool
        pthread_cond_broadcast(&admin->cond);
        pthread_mutex_unlock(&admin->mutex);

        if(strcmp(operation,"LIST") == 0){
            
            int source_fd = connect_with_socket(sync->source_host,sync->source_port);
            char *buffer  = malloc(BUFSIZE);
            if(buffer == NULL){
                printf("Failed to allocate memory for buffer in worker thread\n");
                exit(1);
            }
            sprintf(buffer,"LIST %s",sync->source_dir);
            
            pthread_mutex_lock(&admin->mutex);
            write(source_fd,buffer,strlen(buffer));
            pthread_mutex_unlock(&admin->mutex);

            int i = 0;
            char ch;
            int number_read = 0;
            while (read(source_fd,&ch,1) > 0){
                number_read++;  
                char next;
                // the list of files has been ended
                if(ch == '.'){
                    ssize_t peek = recv(source_fd, &next, 1, MSG_PEEK);
                    if(!peek)
                        break;
                }
                    
                // copying
                if (i < BUFSIZE - 1) 
                    buffer[i++] = ch;
                if(ch == '\n'){
                    // terminate the string
                    buffer[i-1] = '\0'; 
                    Threadpool_info *job = malloc(sizeof(Threadpool_info));
                    job->info = sync;
                    job->filename = strdup(buffer);
                    job->operation = strdup("PUSH/PULL");
                    // put all the jobs inside and the broadcast
                    pthread_mutex_lock(&admin->mutex);
                    while (queue_size(admin->scheduled_tasks) >= admin->max_capacity) {
                        pthread_cond_wait(&admin->cond, &admin->mutex);
                    }
                    queue_insert_back(admin->scheduled_tasks,job);
                    pthread_cond_signal(&admin->cond);
                    pthread_mutex_unlock(&admin->mutex);
                    i = 0;
                }
                
            }
            free(buffer);
            close(source_fd);
            
        }

        if(strcmp(operation,"PUSH/PULL") == 0){
            int source_fd = connect_with_socket(sync->source_host,sync->source_port);
            int target_fd = connect_with_socket(sync->target_host,sync->target_port);
            if (source_fd == -1 || target_fd == -1) {
                perror_exit("Failed connection");
            }
            char *msg = malloc(BUFSIZE);
            if(msg == NULL){
                printf("Failed to allocate memory for msg in worker thread\n");
                exit(1);
            }
            char *msg2 = malloc(BUFSIZE);
            if(msg2 == NULL){
                printf("Failed to allocate memory for msg2 in worker thread\n");
                exit(1);
            }
            // PULL from source
            snprintf(msg,BUFSIZE,"PULL %s/%s",sync->source_dir,filename);
            
            pthread_mutex_lock(&admin->mutex);
            write(source_fd,msg,strlen(msg));
            pthread_mutex_unlock(&admin->mutex);

            ssize_t num;
            bool first_packet_flag = 1;
            int filesize, i = 0;
            char size[12];
            memset(msg,0,BUFSIZE);
            
            // reading from pull and push to target
            // BUFSIZE - 256 cause we need some space for push dir/filename and chunksize so we don't overflow
            while((num = read(source_fd, msg, BUFSIZE - 256))){
                memset(msg2,0,BUFSIZE);
                msg[num] = '\0';
                int header;
                
                // the first is for create file etc the 2nd pack etc will be with the chunk size 
                if(first_packet_flag){
                    char *data = msg; 
                    while(*data != ' ')
                        size[i++] = *(data++);
                    // to skip the space
                    if(*data == ' ')
                        data++; 

                    size[i] = '\0';
                    filesize = atoi(size);
                    // future when i upgrade it for my own 
                    (void)filesize;
                    header = snprintf(msg2,BUFSIZE,"PUSH %s/%s -1 ",sync->target_dir,filename);
                    // num - i - 1 cause i is the charactes of filesize and 1 is the space 
                    memcpy(msg2 + header, data, num - (data - msg));
                    
                    pthread_mutex_lock(&admin->mutex);
                    write(target_fd, msg2, num + header - (data - msg ));
                    pthread_mutex_unlock(&admin->mutex);
                    first_packet_flag = 0;
                }
                else{
                    int target_fd1 = connect_with_socket(sync->target_host,sync->target_port);

                    header = snprintf(msg2, BUFSIZE, "PUSH %s/%s %ld ", sync->target_dir, filename, num);
                    memcpy(msg2 + header, msg, num);

                    pthread_mutex_lock(&admin->mutex);
                    write(target_fd1, msg2, num + header);
                    pthread_mutex_unlock(&admin->mutex);

                    close(target_fd1);
                }

                if(num == 0)
                    break;
            }
            int header = snprintf(msg2,BUFSIZE,"PUSH %s/%s 0",sync->target_dir,filename);
            pthread_mutex_lock(&admin->mutex);
            write(target_fd,msg2,header);
            pthread_mutex_unlock(&admin->mutex);

            // free, leaks etc
            free(msg);
            free(msg2);
            close(source_fd);
            close(target_fd);
            free(filename);
        }
        sync->active = 0;
        free(operation);
    }
}

void add_pair(char *console_cmd, Admin *admin, List sync_info_mem_store,const int console_fd){
       
    char *input = strdup(console_cmd);
    if (input == NULL) {
        perror_exit("Allocation failed for input");
    }  

    char *buffer = malloc(1024);
    if (buffer == NULL) {
        perror_exit("Allocation failed for buffer");
    }
    // initialize info and allocate memory
    Sync_info *info = malloc(sizeof(Sync_info));
    info->active = 0;
    info->last_sync_time = NULL;
    info->status = NULL;
    info->error_count = 0;
    // get source_dir
    char *s_dir = strchr(input, '@');
    *s_dir = '\0';  //here source dir ends
    info->source_dir = strdup(input);
    if (info->source_dir == NULL) 
        perror_exit("Failed to duplicate source_dir");
    

    // source ip
    char *s_ip = strchr(s_dir + 1, ':');
    *s_ip = '\0';  //here source ip ends
    info->source_host = strdup(s_dir + 1);
    if (info->source_host == NULL) 
        perror_exit("Failed to duplicate source_host");
    
    // source port
    char *s_port = strchr(s_ip + 1, ' ' );
    *s_port = '\0';  
    int port = atoi(s_ip + 1);
    info->source_port = port;

    // target dir
    char *t_dir = strchr(s_port + 1, '@');
    *t_dir = '\0';  
    info->target_dir = strdup(s_port + 1);
    if (info->target_dir == NULL) 
        perror_exit("Failed to duplicate target_dir");

    // target ip
    char *t_ip = strchr(t_dir + 1, ':');
    *t_ip = '\0';
    info->target_host = strdup(t_dir + 1);
    if (info->target_host == NULL) 
        perror_exit("Failed to duplicate target_dir");

    // target port
    int t_port = atoi(t_ip + 1);
    info->target_port = t_port;

     
    //if the source dir is not already in the list, insert the pair otherwise print according message and continute
    if(!check_existance_pair(info->source_dir,sync_info_mem_store)){
        list_insert_next(sync_info_mem_store,list_first(sync_info_mem_store),info);
        Threadpool_info *job = malloc(sizeof(Threadpool_info));
        if(job == NULL){
            printf("failed to allocate job\n");
            destroy_sync_info(info);
            exit(1);
        }
        job->info = info;
        job->filename = NULL;
        job->operation = strdup("LIST");
        pthread_mutex_lock(&admin->mutex);

        while (queue_size(admin->scheduled_tasks) >= admin->max_capacity)
            pthread_cond_wait(&admin->cond, &admin->mutex);    
            
        queue_insert_back(admin->scheduled_tasks,job);
        pthread_cond_signal(&admin->cond);
        pthread_mutex_unlock(&admin->mutex);
    }
    else{
        char *time = get_timestamp();
        char *update = malloc(1024);
        if(update == NULL){
            printf("Failed memory allocation for update");
            exit(1);
        }
        snprintf(update,1024,"[%s] Already in queue: %s@%s:%d\n",time,info->source_dir,info->source_host,info->source_port);
        printf("%s",update);
        write(console_fd,update,strlen(update));
        free(update);
        free(time);
        destroy_sync_info(info);
    }
    free(buffer);
    free(input);
}

void shutdown_all(Admin *admin, List sync_mem_info_store,const int console_fd,const int man_fd){
    char *time =get_timestamp();
    char *update = malloc(1024);
    if(update == NULL){
        printf("Failed memory allocation for update");
        exit(1);
    }
    snprintf(update,1024,"[%s] Shutting down manager...\n",time);
    printf("%s",update);
    write(console_fd,update,strlen(update));
    write(man_fd,update,strlen(update));   
    free(time);

    time =get_timestamp();
    snprintf(update,1024,"[%s] Waiting for all active workers to finish.\n",time);
    printf("%s",update);
    write(console_fd,update,strlen(update));
    write(man_fd,update,strlen(update));   
    free(time);
    // wait for the threads to finish and don't waste resources so sleep
    
    while(queue_size(admin->scheduled_tasks) != 0){
        sleep(1);
    }

    time = get_timestamp();
    snprintf(update,1024,"[%s] Processing remaining queued tasks.\n",time);
    printf("%s",update);
    write(console_fd,update,strlen(update));
    write(man_fd,update,strlen(update));   
    free(time);
    
    // close the clients
    for(ListNode node = list_first(sync_mem_info_store) ; node != LIST_EOF ; node = list_next(sync_mem_info_store,node)){
        Sync_info *info = (Sync_info*) list_node_value(sync_mem_info_store,node);
        int source_fd = connect_with_socket(info->source_host,info->source_port);
        if(source_fd != 1){
            pthread_mutex_lock(&admin->mutex);
            write(source_fd,"SHUT",4);
            pthread_mutex_unlock(&admin->mutex);
            close(source_fd);
        }   

        int target_fd = connect_with_socket(info->target_host,info->target_port);
        if(target_fd != -1 ){
            pthread_mutex_lock(&admin->mutex);
            write(target_fd,"SHUT",4);
            pthread_mutex_unlock(&admin->mutex);
            close(target_fd);
        }
        
        
    }
    admin->shutdown_flag = 1;
    pthread_cond_broadcast(&admin->cond);
    pthread_mutex_destroy(&admin->mutex);
    pthread_cond_destroy(&admin->cond);

    free(update);
}

void cancel(Admin *admin, const char *source_dir, const int port, const char *ip, const int console_fd,const int man_fd,List sync_mem_info_store){
    pthread_mutex_lock(&admin->mutex);
    bool monitored = 0;
    for(Node current = queue_first(admin->scheduled_tasks) ;  current != QUEUE_EOF ; ){
        Node next = queue_next(admin->scheduled_tasks, current);  // save next before removal

        Threadpool_info *t_info =(Threadpool_info *) queue_node_value(admin->scheduled_tasks,current);
        if(strcmp(t_info->info->source_dir,source_dir) == 0 && strcmp(t_info->info->source_host,ip) == 0 && port == t_info->info->source_port){
            queue_remove_node(admin->scheduled_tasks,current);
            monitored = 1;
        }
            
        current = next;
    }
    for(ListNode current = list_first(sync_mem_info_store) ;  current !=  LIST_EOF ; current = list_next(sync_mem_info_store,current) ){

        Sync_info *t_info = (Sync_info*)list_node_value(sync_mem_info_store,current);
        if(strcmp(t_info->source_dir,source_dir) == 0 && strcmp(t_info->source_host,ip) == 0 && port == t_info->source_port){
            monitored = 1;
        }
    }
    // broadcast cause sth change and one thread could wait cause the buffer is full
    pthread_cond_broadcast(&admin->cond);
    pthread_mutex_unlock(&admin->mutex);
    char *update = malloc(1024);
    if(update == NULL){
        printf("Failed memory allocation for update");
        exit(1);
    }
     // messages for manager conaolse and monitor
    if (monitored){
        
        char *time = get_timestamp();
        snprintf(update,1024,"[%s] Synchronization stopped for %s@%s:%d\n",time,source_dir,ip,port);
        printf("%s",update);
        write(console_fd,update,strlen(update));
        write(man_fd,update,strlen(update));
        free(time);
    }
    else{
        char *time = get_timestamp();
        snprintf(update,1024,"[%s] Directory not being synchronized: %s@%s:%d\n",time,source_dir,ip,port);
        write(console_fd,update,strlen(update));
        printf("%s",update);
        free(time); 
    }
    free(update);
    return;
}