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

    create_tree(arr, L, 0,NP-1, &result);

    free(arr);


    return 0;
}

int create_tree(int16_t* partition_ptr, int partition_length, int tree_id, int processes_budget, struct search_result* result)
{
    int pipefds[m][2];
    pid_t pids[m];
    int16_t* my_partition = partition_ptr;

    log_msg("ECE 434 Sp26: Process with PID %ld is creating a tree node with partition starting at %p with length %d and processes budget %d", getpid(), partition_ptr, partition_length, processes_budget);

    // create subpartitions and assign children to them
    for (int i =0; i < m; i++)
    {
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
            read(pipefds[i][0], &my_partition, sizeof(my_partition));
            close(pipefds[i][0]);
            log_msg("ECE 434 Sp26: Child with PID %d received a subpartition starting at subindex %d with length %d", getpid(), (int)((void*)my_partition - (void*)partition_ptr) >> 1, partition_length/m);
            break;
        }
        else //parent
        {
            int16_t* assigned_partition_ptr = partition_ptr + i*partition_length/m;
            write(pipefds[i][1], &assigned_partition_ptr, sizeof(assigned_partition_ptr));
            log_msg("ECE 434 Sp26: Parent with PID %ld assigned partition starting at subindex %d with length %d to child with PID %d", getpid(), (int)((void*)assigned_partition_ptr - (void*)partition_ptr) >> 1, partition_length/m, pids[i]);
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

        // if each child can make m children
        if (processes_budget > m*m) 
        {
            create_tree(my_partition,partition_length/m,m*tree_id+i,processes_budget/m, result);
            write(pipefds[i][1],(void*)result,sizeof(*result));
            close(pipefds[i][1]);
            exit(m*i + tree_id);
        }
        else // if I am a leaf worker
        {
            usleep(100000);
            pid_t mypid = getpid();
            struct search_result s = {
                .max = 0,
                .average = 0,
                .keys_found = 0
            };
            log_msg("ECE 434 Sp26: Leaf process with PID %ld is now searching through the partition starting at %p with length %d", mypid, my_partition, partition_length/m);
            for (int j=0; j < partition_length/m; j++)
            {
                s.average += my_partition[j];
                if (my_partition[j] > s.max)
                {
                    s.max = my_partition[j];
                }
                if (my_partition[j] < 0)
                {
                    //log_msg("ECE 434 Sp26: Process with PID %ld found a hidden key at index %d", mypid, (int)((void*)my_partition + j - (void*)base_ptr) >> 1);
                    s.keys_found++;
                }
            }   
            s.average *= m / partition_length; 

            write(pipefds[i][1],(void*)&s,sizeof(s));
            close(pipefds[i][1]);               
            exit(m*i + tree_id);
        }
    }

    // Parent
    pid_t mypid = getpid();
    struct pollfd pollfds[m];
    for (int i = 0; i < m; i++)
    {
        pollfds[i].fd = pipefds[i][0];
        pollfds[i].events = POLLIN | POLLERR;
    }

    int remaining_children = m;
    log_msg("ECE 434 Sp26: Process with PID %ld is waiting for its children to report back", mypid);
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

                waitpid(pids[i], NULL, WNOHANG | WUNTRACED);
                if (pollfds[i].revents & POLLIN)
                {
                    struct search_result child_res = {
                        .max = 0,
                        .average = 0,
                        .keys_found = 0
                    };
                    int status;
                    read(pollfds[i].fd,&child_res,sizeof(child_res));
                    waitpid(pids[i], &status, 0);
                    explain_wait_status(pids[i], status);
                    log_msg("ECE 434 Sp26: Process %ld received data from child %d: max=%d, average=%d, keys_found=%d\n", mypid, pids[i], child_res.max, child_res.average, child_res.keys_found);
                    log_msg("ECE 434 Sp26: Polling error on child %d in process %ld", pids[i], mypid);
                    waitpid(pids[i], NULL, 0);
                    remaining_children--;
                }
                else if (pollfds[i].revents & POLLERR)
                {
                    log_msg("ECE 434 Sp26: Polling error on child %d in process %ld", pids[i], mypid);
                    waitpid(pids[i], NULL, 0);
                    remaining_children--;
                }
            }
        }
        
    }

    result-> average /= m;
    return tree_id;
}

void log_msg(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

// this value is computed by mapping the start of this child's unique partition to an integer 0 from to 255 (the range of valid return codes)
void explain_wait_status(pid_t pid, int status)
{
    if (WIFEXITED(status))
    {
        log_msg("ECE 434 Sp26: Child with PID %ld exited normally with code: %d\n", pid, WEXITSTATUS(status));
    }
    else if (WIFSIGNALED(status))
    {
        log_msg("ECE 434 Sp26: Child with PID %ld terminated by signal: %d\n", pid, WTERMSIG(status));
    }
    else if (WIFSTOPPED(status))
    {
        log_msg("ECE 434 Sp26: Child with PID %ld stopped by signal: %d\n", pid, WSTOPSIG(status));
    }
    else
    {
        log_msg("ECE 434 Sp26: Child with PID %ld: status unknown\n", pid);
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

