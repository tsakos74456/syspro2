#include "functions_nfs_console.h"


char* get_timestamp() {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    char *buffer=malloc(256*(sizeof(char)));  // Enough space for "YYYY-MM-DD HH:MM:SS\0"
    strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}


bool check_flags_nfs_console(const int argc, char **argv){
    if (argc != 7){
        fprintf(stderr, "Use the parameters correctly!\n");
        return 0;
    }
    if (strcmp(argv[1],"-l")){
        fprintf(stderr, "The -l flag is not used correctly!\n");
        return 0;
    }
    if (strcmp(argv[3],"-h")){
        fprintf(stderr, "The -h flag is not used correctly!\n");
        return 0;
    }
    if (strcmp(argv[5],"-p")){
        fprintf(stderr, "The -p flag is not used correctly!\n");
        return 0;
    }
    return 1;
}

void perror_exit(const char *msg){
    perror(msg);
    exit(EXIT_FAILURE);
}

void print_command(const int fd,const char *source_dir,const char *target_dir, const char *command,const char *time){
    // print the command to console log
    char msg[256];
    if(strcmp(command,"add")== 0)
        snprintf(msg,sizeof(msg), "[%s] Command %s %s -> %s\n",time,command,source_dir,target_dir);

    else if(strcmp(command,"shutdown") == 0)
        snprintf(msg,sizeof(msg), "[%s] Command %s \n",time,command);

    else
        snprintf(msg,sizeof(msg), "[%s] Command %s %s\n",time,command,source_dir);

    if (write(fd, msg, strlen(msg)) < 0) {
        perror("Error writing to log file");
    }
}

void print_manager_response(const int fd, const char *msg){
    
    if (msg == NULL || msg[0] == '\0') {
        return; 
    }

    char temp[1024];
    snprintf(temp, sizeof(temp), "%s\n", msg);
    if (write(fd, temp, strlen(temp)) < 0) {
        perror("Error writing to log file");
    }
    printf("%s\n",msg);

}

void read_from_manager(char *response, const int fd_out, const int console_file, bool *flag, bool *last_flag){
    ssize_t n;
    while ((n = read(fd_out, response, 1024 - 1)) > 0) {
        response[n] = '\0';
        char *line = strtok(response, "\n");
        while (line != NULL) {
            if (strstr(line, "Manager shutdown complete") != NULL) {
                print_manager_response(console_file, line);
                
                // it shutdowns now
                *last_flag = true;
                return;
            }
            if (strstr(line, "Processing remaining queued tasks") != NULL) {
                print_manager_response(console_file, line);

                // there are still active worker so prepare to shut down when they finish
                *flag = true;
            }
            else
                print_manager_response(console_file, line);
            line = strtok(NULL, "\n");
        }
    }
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