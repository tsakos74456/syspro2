#include "functions_nfs_manager.h"
#include "../lib/ADTList.h"
#include "../lib/ADTQueue.h"
#define PERMS 0644

pthread_t *worker_tids;

int main(int argc, char **argv){
    
    // check if the args are correct
    if(!check_flags_nfs_manager(argc,argv)){
        printf("Pls try again!\n");
        return -1;
    }

    // pass the arguments to the variables, create the manager log and open the configuration file
    int worker_limit, thread_pool_capacity;
    if(argc == 11)
        worker_limit = atoi(argv[6]);
    else 
        worker_limit = 5;

    if(atoi(argv[10]) <= 0 ){
        printf("Give a valid number for buffersize!\n");
        return -1;
    }
    else 
        thread_pool_capacity = atoi(argv[10]);

    int man_fd, config_fd;
    char *manager_logfile = argv[2];
    char *config_file = argv[4];
    if((man_fd = open(manager_logfile, O_WRONLY | O_CREAT | O_TRUNC, PERMS)) == -1){
        perror("Failed to open the manager file");
        return -1;
    }
    if((config_fd = open(config_file, O_RDONLY)) == -1){
        perror("Failed to open the config file");
        return -1;
    }
    
    // create the socket with the console
    int port = atoi(argv[8]);
    int sock = create_socket(port);
    
    int console_sock;
    struct sockaddr_in client;
    struct sockaddr *client_ptr = (struct sockaddr*) &client;

    socklen_t client_len = sizeof(client);
    if((console_sock = accept(sock, client_ptr, &client_len)) < 0)
        perror_exit("Failed to accept the socket!\n");

    // set the socket non-block for read and write
    int flags = fcntl(console_sock, F_GETFL, 0);
    if((fcntl(console_sock, F_SETFL,flags | O_NONBLOCK)) == -1)
        perror_exit("Failed to make the socket_fd non block\n");


    
    // this is the list which we will save the general info about the pairs from the config and what we get from console
    List sync_info_mem_store = list_create(destroy_sync_info);

    // this is the queue which works as a threadpool for the workers and prepares the threads
    Queue thread_pool = queue_create(destroy_threadpool_info, thread_pool_capacity);

    Admin *admin = create_admin(thread_pool,thread_pool_capacity);

    create_threads(worker_limit,admin);

    // read from config and store the info
    read_config_file(sync_info_mem_store,config_fd,admin);
    

    char *console_cmd  = malloc(1024);
    if(console_cmd == NULL){
        perror_exit("Failed memory allocation for console cmd");
    }
    while(1){
        memset(console_cmd, 0, 1024);
        
        ssize_t bytes_read = read(console_sock, console_cmd, 1023);
        if(bytes_read > 0){
            console_cmd[bytes_read] = '\0';
            // add
            if(strncmp(console_cmd,"add",3) == 0){
                add_pair(console_cmd + 4,admin,sync_info_mem_store,console_sock);
            }
            else if(strncmp(console_cmd,"cancel",6) == 0){
                // the format is dir@ip:port
                char *copy = strdup(console_cmd + 7);
                if (copy == NULL) 
                    perror_exit("Failed to duplicate cmd");
                
                char *papaki = strchr(copy, '@');
                *papaki = '\0';

                char *s_dir = strdup(copy);
                if (!s_dir)
                    perror_exit("Failed to duplicate source_dir");

                char *colon = strchr(papaki + 1, ':');
                *colon = '\0';  

                char *s_ip = strdup(papaki + 1);
                if (!s_ip)
                    perror_exit("Failed to duplicate source_host");

                int s_port = atoi(colon + 1);

                cancel(admin,s_dir,s_port,s_ip,console_sock,man_fd,sync_info_mem_store);
                free(copy);
                free(s_ip);
                free(s_dir);
            }
            else if(strncmp(console_cmd,"shutdown",8) == 0){
                shutdown_all(admin,sync_info_mem_store,console_sock,man_fd);
                break;
            }

        }
        else if (bytes_read == 0){
            shutdown_all(admin,sync_info_mem_store,console_sock,man_fd);
            break;
        }
    }
    char *time =get_timestamp();
    char *update = malloc(1024);
    if(update == NULL){
        printf("Failed memory allocation for update");
        exit(1);
    }
    snprintf(update,1024,"[%s] Manager shutdown complete.\n",time);
    printf("%s",update);
    write(console_sock,update,strlen(update));
    write(man_fd,update,strlen(update));   
    
    // handle leaks etc
    for (int i = 0; i < worker_limit; i++) {
        pthread_join(worker_tids[i], NULL);
    }
    free(worker_tids);  // also free the global array
    queue_destroy(admin->scheduled_tasks);
    free(admin);
    free(update); 
    free(time);
    list_destroy(sync_info_mem_store);
    free(console_cmd);
    close(man_fd);
    close(config_fd);
    close(sock);
    close(console_sock);
    return 0;
}