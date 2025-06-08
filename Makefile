# Compiler and flags
CC      = gcc
CFLAGS  = -Wall -Wextra -g -MMD

# Files
SRCS_MANAGER = manager/nfs_manager.c lib/ADTQueue.c manager/functions_nfs_manager.c lib/ADTList.c 
SRCS_CONSOLE = console/nfs_console.c console/functions_nfs_console.c
SRCS_CLIENT =  client/nfs_client.c client/functions_client.c

OBJS_MANAGER = $(SRCS_MANAGER:.c=.o)
OBJS_CONSOLE = $(SRCS_CONSOLE:.c=.o)
OBJS_CLIENT = $(SRCS_CLIENT:.c=.o)

TARGET_MANAGER = nfs_manager
TARGET_CONSOLE = nfs_console
TARGET_CLIENT = nfs_client

# Default target: build the executable
all: $(TARGET_CONSOLE) $(TARGET_MANAGER) $(TARGET_CLIENT)



# Link object files into the executable related to nfs_manager
$(TARGET_MANAGER): $(OBJS_MANAGER)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

# Run the nfs_manager
run_manager: $(TARGET_MANAGER)
	./$(TARGET_MANAGER) -l ./testing/manager_logfile.txt -c ./testing/config_file.txt -n 100 -p 8080 -b 10

# Run the nfs_manager using valgrind for memory checking
valgrind_manager: $(TARGET_MANAGER)
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET_MANAGER) -l ./testing/manager_logfile.txt -c ./testing/config_file.txt -n 10 -p 8080 -b 10

# Clean up compiled files related to nfs_manager
clean_manager:
	rm -f $(OBJS_MANAGER) $(TARGET_MANAGER) $(OBJS_MANAGER:.o=.d)




# Link object files into the executable related to console
$(TARGET_CONSOLE): $(OBJS_CONSOLE)
	$(CC) $(CFLAGS) -o $@ $^

# Run the nfs_console
run_console: $(TARGET_CONSOLE)
	./$(TARGET_CONSOLE) -l ./testing/console_logfile.txt -h 127.0.0.1 -p 8080

# Run the nfs_console using valgrind for memory checking
valgrind_console: $(TARGET_CONSOLE)
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET_CONSOLE) -l ./testing/console_logfile.txt -h 127.0.0.1 -p 8080

# Clean up compiled files related to nfs_console
clean_console:
	rm -f $(OBJS_CONSOLE) $(TARGET_CONSOLE) $(OBJS_CONSOLE:.o=.d)




# Link object files into the executable related to nfs_client
$(TARGET_CLIENT): $(OBJS_CLIENT)
	$(CC) $(CFLAGS) -o $@ $^

# Run the nfs_client
run_client: $(TARGET_CLIENT)
	./$(TARGET_CLIENT) -p 8081

# Run the nfs_client using valgrind for memory checking
valgrind_client: $(TARGET_CLIENT)
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET_CLIENT) -p 8081
# Clean up compiled files related to nfs_client
clean_client:
	rm -f $(TARGET_CLIENT) $(OBJS_CLIENT) $(OBJS_CLIENT:.o=.d)


# Clean up compiled files
clean:
	$(MAKE) clean_console
	$(MAKE) clean_manager
	$(MAKE) clean_client
	rm -f fss_in fss_out 
	rm -f ./testing/manager_logfile.txt ./testing/console_logfile.txt

#dependency files
-include $(OBJS_MANAGER:.o=.d) $(OBJS_CONSOLE:.o=.d) $(OBJS_CLIENT:.o=.d)

# commands names
.PHONY: all clean clean_m clean_c clean_w run_m run_c valgrind_m valgrind_c