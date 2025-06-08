#include "functions_client.h"
#define BUFSIZE 2626560



void perror_exit(const char *msg){
    perror(msg);
    exit(EXIT_FAILURE);
};

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

int connect_with_socket(const char *host_name, const int port){
    int sockfd;
    struct sockaddr_in server;
    struct sockaddr *server_ptr = (struct sockaddr *)&server;
    struct hostent *rem;

    if((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0)
        perror_exit("Socket creation failed!\n");
    if((rem = gethostbyname(host_name)) == NULL){
        herror("gethostbyname");
        exit(1);
    }

    server.sin_family = AF_INET;
    memcpy(&server.sin_addr,rem->h_addr_list[0],rem->h_length);
    server.sin_port = htons(port);

    if(connect(sockfd,server_ptr,sizeof(server)) < 0)
        perror_exit("Failed connection to socket!\n");

    int flags = fcntl(sockfd, F_GETFL, 0);
    if((fcntl(sockfd, F_SETFL,flags | O_NONBLOCK)) == -1)
        perror_exit("Failed to make the socket_fd non block\n");
    return sockfd;

}

int accept_new_socket_connection(const int sock_fd){
    struct sockaddr_in client;
    struct sockaddr *client_ptr = (struct sockaddr*) &client;
    int new_sockfd;
    socklen_t client_len = sizeof(client);
    if((new_sockfd = accept(sock_fd, client_ptr, &client_len)) < 0)
        perror_exit("Failed to accept the socket!\n");
    int size;
    socklen_t len = sizeof(size);
    getsockopt(new_sockfd, SOL_SOCKET, SO_SNDBUF, &size, &len);
    return new_sockfd;
}

void list(char *buffer, const int sock_fd){
    char *name_dir = buffer;
    char *response = malloc(BUFSIZE);
    
    if(buffer == NULL){
        perror("Allocation failed for response");
        exit(1);
    }
    response[0] = '\0'; // initialize the buffer for strcat
    DIR* my_dir = opendir(name_dir);
    if(my_dir == NULL){
        snprintf(response,BUFSIZE,"ERROR:%s\n",strerror(errno));
        write(sock_fd,response,strlen(response));
        free(response);
        free(name_dir);
        close(sock_fd);
        return;
    }
    struct dirent *current;

    // send with the correct format the name of the files
    while((current = readdir(my_dir))){
        // these are folders which represent the current and the parent folder and they exist in every folder so skip them
        if(strcmp(current->d_name, ".") == 0 || strcmp(current->d_name, "..") == 0)
            continue;
        strcat(response, current->d_name);
        strcat(response, "\n");
    }
    strcat(response,".");
    write(sock_fd,response,strlen(response));
    close(sock_fd);
    closedir(my_dir);
    // free allocated space
    free(response);
}



void push(char *buffer, const int sock_fd){
    char filename[256];
    memset(filename, 0, 256);
    char chunksize[12];
    int chunk_size;
    // get filename, chunk size and data
    int i = 0;
    sscanf(buffer, "%255s %11s", filename, chunksize);
    chunk_size = atoi(chunksize);
    char *data = buffer + strlen(filename) + 1 + strlen(chunksize) + 1;
    // create the dir if it doesn't exist
    char *dir = strdup(filename);

    i = strlen(dir);
    while(i >= 0 &&  dir[i] != '/')
        dir[i--] = '\0';

    // make sure the target dir doesn't exist and if not create it
    struct stat buf;
    if (stat(dir, &buf) == -1) {
        if (mkdir(dir, 0744) == -1) {
            char *EXEC_REP = malloc(1024);
            if (!EXEC_REP) {
                perror("Failed to allocate EXEC_REP");
                exit(1);
            }
            snprintf(EXEC_REP, 1024,
                     "EXEC_REPORT_START\nSTATUS:ERROR\nDETAILS: 0 files copied, ALL files skipped\nERRORS:\n-DIR %s: %s\nEXEC_REPORT_END\n",
                     dir, strerror(errno));
            write(1, EXEC_REP, strlen(EXEC_REP));
            free(EXEC_REP);
            free(dir);
            close(sock_fd);
            return;
        }
    }
    
    int file_fd;
    if(chunk_size == -1){
        file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (file_fd == -1){
            char *msg = malloc(1024);
            if(msg == NULL){
                perror("Allocation failed for msg");
                exit(1);
            }
            snprintf(msg,1024,"-1 %s",strerror(errno));
            write(sock_fd,msg,strlen(msg));
            free(msg);  
            close(sock_fd);
            return;
        }
        write(file_fd,data,strlen(data));
        close(file_fd);
            
    }
    else if(chunk_size == 0){
        close(sock_fd);
        return;
    }
    else if(chunk_size > 0){
        file_fd = open(filename, O_WRONLY | O_APPEND, 0644);
        if (file_fd == -1 ) {
            char *msg = malloc(1024);
            if(msg == NULL){
                perror("Allocation failed for msg");
                exit(1);
            }
            snprintf(msg,1024,"-1 %s",strerror(errno));
            write(sock_fd,msg,strlen(msg));
            free(msg);  
            close(sock_fd);
            return;
        }
        write(file_fd,data,chunk_size);
        close(file_fd);
            
    }
    free(dir);
}

void pull(char *buffer, const int sock_fd){
    char *filename = buffer;
    char *msg = malloc(BUFSIZE);
    if(msg == NULL){
        perror("Allocation failed for msg");
        exit(1);
    }

    int file_fd = open(filename, O_RDONLY);
    if (file_fd == -1) {
        snprintf(msg,BUFSIZE,"-1 %s",strerror(errno));
        write(sock_fd,msg,strlen(msg));
        free(msg);
        close(sock_fd);
        return;
    }

    // get file's size 
    struct stat st;
    if (fstat(file_fd, &st) == -1) {
        snprintf(msg,BUFSIZE,"-1 %s",strerror(errno));
        write(sock_fd,msg,strlen(msg));
        free(msg);
        close(sock_fd);
        close(file_fd);
        return;
    }
    snprintf(msg,BUFSIZE,"%ld ",st.st_size);
    write(sock_fd,msg,strlen(msg));

    
    ssize_t n;
    // read from file and write to the manager
    while((n = read(file_fd,msg,BUFSIZE - 2))){
        write(sock_fd,msg,n);
    }
    close(file_fd);
    free(msg);
}