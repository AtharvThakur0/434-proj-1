#include "main.h"

// B-tree parameter
#define m 2
#define CHILD_CONSTANT -0xC
#define POLL_TIMEOUT 5

int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        printf("Please provide all arguments. Usage: ./main {L} {H} {NP}\n");
        return -1;
    }
    int L = atoi(argv[1]);
    int H = atoi(argv[2]);
    int NP = atoi(argv[3]);

    if (L < 150 || H < 0 || H > 150 || NP < 1)
    {
        printf("Invalid Arguments.\n");
        return -1;
    }


    // We skip some tedious steps and directly use the underlying array of integers used to make the file
    // This is is equivalent to opening and parsing the file into an array if we didnt create the file
    int16_t* arr;
    if (generate_file(L, &arr) < 0)
    {
        free(arr);
        return -1;
    }

    struct search_result result = {0};
    create_tree(arr, arr, L, NP, L, &result);

    free(arr);


    return 0;
}

int create_tree(int16_t* base_ptr, int16_t* partition_ptr, int partition_length, int processes_budget, int L, struct search_result* result)
{
    int pipefds[m][2];
    pid_t pids[m];
    int16_t* sub_partitions[m];

    // create subpartitions and assign children to them
    for (int i =0; i < m; i++)
    {
        sub_partitions[i] = partition_ptr + i*partition_length/m;
        if (pipe(pipefds[i]) < 0)
        {
            err(-1, "Failed to create a pipe. Exiting.");
        }
        processes_budget--;
        pids[i] = fork();
        if (pids[i] == -1)
        {
            err(-1, "Failed to fork. Exiting.");
        }
        if (pids[i] == 0) // child
        {
            // using this instead of default 0 to make things easier in later sifting  
            pids[i] = CHILD_CONSTANT;
            close(pipefds[i][0]);
            break;
        }
        else //parent
        {
            close(pipefds[i][1]);
        }
    }

    // Now that all children are created, we sift through them and do processing
    for (int i =0; i < m; i++)
    {
        if (pids[i] != CHILD_CONSTANT)
        {
            continue;
        }

        // Child

        if (processes_budget > m*m) // if each child can make m children
        {
            int ret = create_tree(base_ptr,sub_partitions[i],partition_length/m,processes_budget/m,L, result);
            exit(ret);
        }
        else // if I am a leaf worker
        {
            sleep(1);
            struct search_result s;
            for (int j=0; j < partition_length/m; j++)
            {
                s.average += sub_partitions[i][j];
                if (sub_partitions[i][j] > s.max)
                {
                    s.max = sub_partitions[i][j];
                }
                if (sub_partitions[i][j] < 0)
                {
                    printf("Found a hidden key at index %d",(int)((void*)sub_partitions[i] + j - (void*)base_ptr) >> 1);
                    s.keys_found++;
                }
            }
            s.average = s.average * m / partition_length; 

            write(pipefds[i][1],(void*)&s,sizeof(s));
        }
        
        // return a unique identifying value 
        // this value is computed by mapping the start of this child's unique partition to an integer 0 from to 255 (the range of valid return codes)
        return get_return_code(base_ptr, sub_partitions[i], L);

    }

    // Parent

    struct pollfd pollfds[m];
    for (int i = 0; i < m; i++)
    {
        pollfds[i].fd = pipefds[i][0];
        pollfds[i].events = POLLIN | POLLERR;
    }

    int remaining_children = m;
    while (remaining_children)
    {
        int event_count = poll(pollfds, m, POLL_TIMEOUT);
        if (event_count < 0)
        {
            err(-1, "A polling error (%d) occured",event_count);
        }
        if (event_count)
        {
            for (int i = 0; i < m; i++)
            {
                if (pollfds[i].revents & POLLIN)
                {
                    struct search_result child_res;
                    int status;
                    read(pollfds[i].fd,&child_res,sizeof(child_res));
                    waitpid(pids[i], &status, 0);
                    explain_wait_status(pids[i], status);
                    if (result -> max < child_res.max)
                    {
                        result->max = child_res.max;
                    }
                    result->average += child_res.average;
                    result->keys_found += child_res.keys_found;
                    remaining_children--;
                }
            }
        }
        
    }

    result-> average /= m;
    return get_return_code(base_ptr, partition_ptr, L);
    

    
}

// this value is computed by mapping the start of this child's unique partition to an integer 0 from to 255 (the range of valid return codes)
int get_return_code(int16_t* base_ptr, int16_t* partition_ptr, int L)
{
    return (((void*)partition_ptr - (void*)base_ptr)) * 0x7F / L;
}

void explain_wait_status(pid_t pid, int status)
{
    if (WIFEXITED(status))
    {
        printf("Child (PID %d) exited normally with code: %d\n", pid, WEXITSTATUS(status));
    }
    else if (WIFSIGNALED(status))
    {
        printf("Child (PID %d) terminated by signal: %d\n", pid, WTERMSIG(status));
    }
    else if (WIFSTOPPED(status))
    {
        printf("Child (PID %d) stopped by signal: %d\n", pid, WSTOPSIG(status));
    }
    else
    {
        printf("Child (PID %d) status unknown\n", pid);
    }
}

int generate_file(int L, int16_t** out_array)
{
    *out_array = malloc(L*sizeof(int16_t));
    int16_t* arr = *out_array;
    for (int i = 0; i < L; i++)
    {
        arr[i] = (rand() & 0x7FFF);
    }
    for (int hidden = 0; hidden < 150; hidden++)
    {
        int i = rand() % L;
        if (arr[i] < 0)
            hidden--; // retry if collision with other hidden number
        else
            arr[i] = -1 - (rand() % 100); // range of [-100,-1]
    }

    FILE* fptr = fopen("nums.txt", "w");

    if (fptr == NULL)
    {
        printf("Error in opening file");
        return -1;
    }

    char buf[8];
    for (int i = 0; i < L; i++)
    {
        snprintf(buf,8,"%6d\n",arr[i]);
        fputs(buf,fptr);
    }

    fclose(fptr);
    return 0;

}

