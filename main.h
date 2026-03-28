#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

int generate_file(int L, int16_t** out_array);
int create_tree(int16_t* partition_ptr, int partition_length, int processes_budget, int H);