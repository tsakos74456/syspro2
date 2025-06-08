#include <stdio.h>
#include <time.h>
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
#include "../lib/ADTList.h"
#include "../lib/ADTQueue.h"
#include <pthread.h>
#include <signal.h>

extern pthread_t *worker_tids;

typedef struct{
    // info about source host,file and port
    char *source_host;
    char *source_dir;
    int source_port;

    // info about target host,file and port
    char *target_host;
    char *target_dir;
    int target_port;    

    char *status;            // SUCCESS/ERROR
    char *last_sync_time;
    bool active;
    int error_count;     // 1 if it's active 0 if it isn't
} Sync_info;

typedef struct{
    Sync_info *info;    //the sync info we had
    char *filename;     //the filename we got from the client
    char *operation;    // PULL / PUSH / LIST
} Threadpool_info;

// struct to save the info around threads and the shared memory so we can
typedef struct{
    Queue scheduled_tasks;           // shared task queue
    pthread_mutex_t mutex;      // protects the queue
    pthread_cond_t cond;        // signals when a task arrives
    int max_capacity;           // max buffersize
    bool shutdown_flag;
} Admin;

// create admin
Admin *create_admin(Queue scheduled,const int capacity);

// functon which returns the needed timestamp. The buffer which is returned has to be freed afterwards
char* get_timestamp();

// function which checks if the manager's flags are correct
bool check_flags_nfs_manager(const int argc, char **argv);

// function for error printing
void perror_exit(const char *msg);

// creates the socket and returns the socket fd
int create_socket(int port);

// function so the list doesn't have leaks to free the sync info correctly 
void destroy_sync_info(Pointer info);

// reads from the config file, creates the sync info and insert into the sync_mem_info_store which is a list
void read_config_file(List mem_info_store, const int config_fd, Admin *admin);

// function to check if the source dir is already synced returns true if it exists
bool check_existance_pair(const char *source_dir, const List mem_info_store);

// connect with a created socket
int connect_with_socket(const char *hostname, const int port);

// destroys threadpool info properly with no leaks
void destroy_threadpool_info(Pointer info);

// creathe the threads
void create_threads(const int capacity, Admin *admin);

// the worker's job
void* worker_thread(void* args);

// add command a pair of dirs to be synchronized
void add_pair(char *console_cmd, Admin *admin, List sync_info_mem_store,const int console_fd);

// shutdown vommand wait for everyone to finish
void shutdown_all(Admin *admin, List sync_info_mem_store,const int console_fd,const int man_fd);

// cancel command stop synchronizing two dirs
void cancel(Admin *admin, const char *source_dir, const int port, const char *ip, const int console_fd,const int man_fd,List sync_mem_info_store);