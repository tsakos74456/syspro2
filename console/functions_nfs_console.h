#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
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

// functions which checks if the given console's flag are correct
bool check_flags_nfs_console(const int argc, char **argv);

// functon which returns the needed timestamp and the buffer which is returned has to be freed
char* get_timestamp();

void perror_exit(const char *msg);

// help function to print the output to the unser and the console-logfile
void print_command(const int fd,const char *source_dir,const char *target_dir, const char *command,const char* time);

// prints to the console log file the manager response
void print_manager_response(const int fd, const char *msg);

// function which reads what the manager sends
void read_from_manager(char *response, const int fd_out, const int console_file, bool *flag, bool *last_flag);

// connect with a created socket and make it also non-block
int connect_with_socket(const char *hostname, const int port);