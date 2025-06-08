#include "functions_nfs_console.h"
#define BUFSIZE 1024

int main(int argc, char **argv){

    // check if the flags/parameters are correct
    if(!check_flags_nfs_console(argc,argv)){
        printf("Pls try again!\n");
        return -1;
    }

    int console_file;
    // open the console log-file if it doesn't exist it will create it
    if((console_file = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0){
        perror("Error in opening the console-logfile");
        return -1;
    }

    // set up the socket to communicate with the manager
    char *host_name = argv[4];
    int port = atoi(argv[6]);
    int sock_fd = connect_with_socket(host_name,port);


    // cmd has the user's command, arg1 is the source dir and the arg2 is the target's dir if it exists
    char *cmd = malloc(BUFSIZE);
    if (cmd == NULL) {
        perror("Allocation failed for cmd");
        exit(1);
    }
    char *arg1 = malloc(BUFSIZE);
    if (arg1 == NULL) {
        perror("Allocation failed for arg1");
        exit(1);
    }
    char *arg2 = malloc(BUFSIZE);
    if (arg2 == NULL) {
        perror("Allocation failed for arg2");
        exit(1);
    }
    char *message = malloc(BUFSIZE);
    if (message == NULL) {
        perror("Allocation failed for message");
        exit(1);
    }
    char *output = malloc(BUFSIZE);
    if (output == NULL) {
        perror("Allocation failed for output");
        exit(1);
    }
    char *response = malloc(BUFSIZE );
    if (response == NULL) {
        perror("Allocation failed for response");
        exit(1);
    }
    
    bool flag = false; // prepare to shutdown
    bool last_flag = false; //when this is us the next action is shutdown no wait etc
    while(!flag){

        read_from_manager(response,sock_fd,console_file,&flag,&last_flag);
        // if flag is true we prepare for shutdown
        if(flag)
            break;

        printf("> ");
        // clear the buffer of message on each loop
        
        // read input from the user and make some checks in order to be sure that the given command is acceptable
        scanf("%s",cmd);

        // cmd add 
        if(strcmp(cmd,"add") == 0){
            scanf("%s",arg1);
            scanf("%s",arg2);
            // the message is the buffer that with the command which will  be passed to the manager
            snprintf(message, BUFSIZE, "%s %s %s", cmd, arg1, arg2);

            char *time = get_timestamp();
            //print in the console log file
            print_command(console_file,arg1,arg2,cmd,time);

            free(time);
        }       
        
        // cmd cancel
        else if(strcmp(cmd,"cancel") == 0){
            scanf("%s",arg1);
            // the message is the buffer that with the command which will  be passed to the manager
            snprintf(message, BUFSIZE, "%s %s", cmd, arg1);

            char *time = get_timestamp();
            //print in the console log file
            print_command(console_file,arg1,arg2,cmd,time);
                        
            free(time);
        }

        // cmd shutdown in order to close the console
        else if(strcmp(cmd,"shutdown") == 0){
            // the message is the buffer that with the command which will  be passed to the manager
            snprintf(message, BUFSIZE, "%s",cmd);
            
            char *time = get_timestamp();
            //print in the console log file
            print_command(console_file,arg1,arg2,cmd,time);
                                    
            free(time);   
                        
            flag = 1;
        }
        
        // if the commands is not one of the previous then inform the user that the given command is not acceptable and close manager as well 
        // before closing the manager send him that sth forced the console to shutdown
        else{
            write(sock_fd,"forced_shutdown",strlen("forced_shutdown") + 1);
            close(sock_fd);
            close(console_file);
            fprintf(stderr,"Give correct commands\n"); 
            last_flag = 1;
            break;
        }

        // through the fss_in send the user's input in the manager
        if (write(sock_fd, message, strlen(message)) < 0) {
            if (errno == EPIPE) {
                printf("Manager closed the pipe. Exiting console.\n");
                break;
            }
            perror("write to fss_in");
        }
        // do not waste resources
        usleep(50000); 
    }
    // wait for the manager to get the message that all the workers have finished
    while(!last_flag)
        read_from_manager(response,sock_fd,console_file,&flag,&last_flag);

    free(cmd);
    free(arg1);
    free(arg2);
    free(message);
    free(output);
    free(response);
    close(sock_fd);
    close(console_file);
    return 0;
}