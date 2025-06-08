#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <netdb.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>

// creates a socket and returns the file descriptor
int create_socket(int port);

void perror_exit(const char *msg);

// connect with a created socket and make it non-block
int connect_with_socket(const char *hostname, const int port);

// when we get LIST from worker return the files of the dir and in the end put one .
void list(char *buffer, const int sock_fd);

// when we get PUSH from worker thread write the data into the file based on the according chunk
void push(char *buffer, const int sock_fd);

// we get PULL as cmd we read from the source file and we send to manager thread its context no matter how big is the file
void pull(char *buffer, const int sock_fd);

// it accepts a new socket connection
int accept_new_socket_connection(const int sock_fd);