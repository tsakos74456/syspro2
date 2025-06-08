#include "functions_client.h"
#define BUFSIZE 48000  // this is the max amount which can be sent from a socket with a package

int main(int argc, char **argv){
    if(argc != 3){
        printf("Wrong args!\n");
        return -1;
    }

    int port = atoi(argv[2]);

    int initial_sockfd = create_socket(port);
    char *buffer = malloc(BUFSIZE);
    if(buffer == NULL){
        perror("Allocation failed for buffer");
        exit(1);
    }

    
    while(1){
        // empty buffer
        memset(buffer,0,BUFSIZE);
        int sockfd = accept_new_socket_connection(initial_sockfd);
        size_t num = read(sockfd,buffer,BUFSIZE - 1);
        
        if(strncmp(buffer,"LIST",4) == 0){
            buffer[num] = '\0';
            list(buffer + 5 ,sockfd);
        }
        else if(strncmp(buffer,"PUSH",4) == 0)
            push(buffer + 5,sockfd);
        else if(strncmp(buffer,"PULL",4) == 0){
            buffer[num] = '\0';
            pull(buffer + 5,sockfd);
            
        }
        else if(strncmp(buffer,"SHUT",4) == 0){
            close(sockfd);
            break;
        }
    }
    free(buffer);
    close(initial_sockfd);
}