#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <poll.h>

struct search_result 
{
    int max;
    int average;
    int keys_found;
};

int generate_file(int L, int16_t** out_array);
int create_tree(int16_t* base_ptr, int16_t* partition_ptr, int partition_length, int processes_budget, int L, struct search_result* result);
int get_return_code(int16_t* base_ptr, int16_t* partition_ptr, int L);
void explain_wait_status(pid_t pid, int status);
