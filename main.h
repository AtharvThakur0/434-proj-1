#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <stdarg.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
#include<string.h>

struct search_result 
{
    int elements_processed;
    size_t bytes_sent;
    int max;
    float average;
    int keys_found;
    struct timespec start_time;
    struct timespec end_time;
};

int generate_file(int L, int16_t** out_array);
int create_tree(int16_t* partition_ptr, int partition_length, int tree_id, int processes_budget, struct search_result* result);
void log_msg(const char* format, ...);
void log_csv(pid_t PID, struct search_result* result);
void explain_wait_status(pid_t pid, int status);
