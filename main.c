#include "main.h"

// B-tree parameter
#define m 3
#define CHILD_CONSTANT -0xC
#define POLL_TIMEOUT 5000

FILE* log_file;
FILE* csv_file;

int my_tree_id;

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

    // part 2 modifications {

    
    signal(SIGQUIT, sigquit_handler);

    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);

    /*
    struct sigaction sa2;
    sa2.sa_handler = sigint_handler;
    sigemptyset(&sa2.sa_mask);
    sa2.sa_flags = SA_RESTART; 
    sigaction(SIGINT, &sa2, NULL);
    */
    // OR 
    signal(SIGINT, SIG_IGN);
    
    //}
    

    // assign global var file ptrs
    log_file = fopen("log.txt", "w");
    csv_file = fopen("trace.csv", "w");
    fprintf(csv_file, "PID,elements_processed,bytes_sent,max,average,keys_found,start_time_s,start_time_ns,end_time_s,end_time_ns\n");
    fflush(csv_file); // necessary to avoid multiple prints
    struct search_result result = {0};
    create_tree(arr, L, 0,NP-1, &result); // root call, processes  budget is NP - 1 since root counts as a process
    log_msg("ECE 434 Sp26: Final result: elements_processed=%d, bytes_sent=%zu, max=%d, average=%f, keys_found=%d,start_time=%ld.%09ld,end_time=%ld.%09ld\n", result.elements_processed, result.bytes_sent, result.max, result.average, result.keys_found, result.start_time.tv_sec, result.start_time.tv_nsec, result.end_time.tv_sec, result.end_time.tv_nsec);
    
    // calculate full timing
    long total_sec = result.end_time.tv_sec - result.start_time.tv_sec;
    long total_nsec = result.end_time.tv_nsec - result.start_time.tv_nsec;
    if (total_nsec < 0)
    {
        total_sec--;
        total_nsec += 1000000000;
    }
    log_msg("ECE 434 Sp26: Total execution time: %ld.%09ld seconds\n", total_sec, total_nsec);
    fclose(csv_file);
    csv_file = fopen("trace.csv", "r");
    if (csv_file == NULL)
    {
        err(-1, "Failed to open trace.csv for reading. Exiting.");
    }

    // Find the slowest child
    long max_time_sec = 0;
    long max_time_nsec = 0;
    int slowest_child_pid = -1;
    char line[256];
    while (fgets(line, sizeof(line), csv_file) != NULL)
    {
        if (line[0] == 'P') // skip header
            continue;   
        char* token = strtok(line, ",");
        if (token == NULL)
        {
            continue;
        }
        int pid = atoi(token);
        token = strtok(NULL, ",");
        token = strtok(NULL, ",");
        token = strtok(NULL, ",");
        token = strtok(NULL, ",");
        token = strtok(NULL, ",");
        token = strtok(NULL, ",");
        if (token == NULL)
            continue;

        long start_sec = atol(token);
        token = strtok(NULL, ",");
        if (token == NULL)
            continue;
        long start_nsec = atol(token);
        token = strtok(NULL, ",");
        if (token == NULL)
            continue;
        long end_sec = atol(token);
        token = strtok(NULL, ",");
        if (token == NULL)
            continue;
        long end_nsec = atol(token);

        long elapsed_sec = end_sec - start_sec;
        long elapsed_nsec = end_nsec - start_nsec;
        if (elapsed_nsec < 0)
        {
            elapsed_sec--;
            elapsed_nsec += 1000000000;
        }
        
        if (elapsed_sec > max_time_sec || (elapsed_sec == max_time_sec && elapsed_nsec > max_time_nsec))
        {
            max_time_sec = elapsed_sec;
            max_time_nsec = elapsed_nsec;
            slowest_child_pid = pid;
        }

    }
    log_msg("ECE 434 Sp26: Slowest child with PID %d with execution time: %ld.%09ld seconds\n", slowest_child_pid, max_time_sec, max_time_nsec);
    fclose(csv_file);
    fclose(log_file);
    free(arr);


    return 0;
}

// Part 2 {
void sigusr1_handler(int signum, siginfo_t* info, void* context)
{
    log_msg("ECE 434 Sp26: Process with PID %d received SIGUSR1 from process with PID %d\n", getpid(), info->si_pid);
    int termsig = info->si_value.sival_int;
    signal(termsig, termination_handler);
    raise(termsig);
}

void termination_handler(int signum)
{
    log_msg("ECE 434 Sp26: Process with PID %d received \"secret number\" / special termination signal %d\n", getpid(), signum);
    exit(my_tree_id);
}

void sigquit_handler(int signum)
{
    log_msg("ECE 434 Sp26: Process with PID %d received SIGQUIT\n", getpid());
    exit(my_tree_id);
}

void sigint_handler(int signum)
{
    log_msg("ECE 434 Sp26: Process with PID %d received SIGINT\n", getpid());   
}
//}


int create_tree(int16_t* partition_ptr, int partition_length, int tree_id, int processes_budget, struct search_result* result)
{
    clock_gettime(CLOCK_MONOTONIC, &result->start_time);
    int pipefds[m][2];
    pid_t pids[m];
    int16_t* my_partition = partition_ptr;

    log_msg("ECE 434 Sp26: Process with PID %d is creating a tree node with partition starting at %p with length %d and processes budget %d", getpid(), partition_ptr, partition_length, processes_budget);

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
            clock_gettime(CLOCK_MONOTONIC, &result->start_time);
            // using this instead of default 0 to make things easier in later sifting  
            pids[i] = CHILD_CONSTANT;
            int ret = read(pipefds[i][0], &my_partition, sizeof(my_partition));
            if (ret < sizeof(my_partition))
            {
                err(-1, "Process with PID %d failed to read assigned partition from pipe. Exiting.", getpid());
            }
            close(pipefds[i][0]);
            int start = (int)((void*)my_partition - (void*)partition_ptr) >> 1;
            log_msg("ECE 434 Sp26: Child with PID %d received subpartition [%d, %d)", getpid(), start, start+partition_length/m);
            break;
        }
        else //parent
        {
            int16_t* assigned_partition_ptr = partition_ptr + i*partition_length/m;
            int ret = write(pipefds[i][1], &assigned_partition_ptr, sizeof(assigned_partition_ptr));
            if (ret < sizeof(assigned_partition_ptr))
            {
                err(-1, "Process with PID %d failed to write to pipe. Exiting.", getpid());
            }
            int start = (int)((void*)assigned_partition_ptr - (void*)partition_ptr) >> 1;
            log_msg("ECE 434 Sp26: Parent with PID %d assigned partition [%d, %d) to child with PID %d", getpid(), start, start+partition_length/m, pids[i]);
            result->bytes_sent += sizeof(assigned_partition_ptr);
            close(pipefds[i][1]);
        }
    }

    // All children are now created, everyone hits these next line

    my_tree_id = tree_id;

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
            clock_gettime(CLOCK_MONOTONIC, &result->end_time);
            result->bytes_sent += sizeof(*result);
            int ret = write(pipefds[i][1],(void*)result,sizeof(*result));
            if (ret < sizeof(*result))
            {
                err(-1, "Process with PID %d Failed to write to pipe. Exiting.", getpid());
            }
        }
        else // if I am a leaf worker
        {
            usleep(100000); // as per instructions
            pid_t mypid = getpid();
            struct search_result s = {
                .max = 0,
                .average = 0,
                .keys_found = 0,
                .elements_processed = partition_length/m,
                .bytes_sent = sizeof(s)
            };
            s.start_time = result->start_time; // using actual start time of this child
            
            log_msg("ECE 434 Sp26: Leaf process with PID %d is now searching through the partition starting at %p with length %d", mypid, my_partition, partition_length/m);
            for (int j=0; j < partition_length/m; j++)
            {
                s.average += my_partition[j];
                if (my_partition[j] > s.max)
                {
                    s.max = my_partition[j];
                }
                if (my_partition[j] < 0)
                {
                    //log_msg("ECE 434 Sp26: Process with PID %d found a hidden key at index %d", mypid, (int)((void*)my_partition + j - (void*)base_ptr) >> 1);
                    s.keys_found++;
                }
            }   
            // not * -to properly use float operations
            s.average = (s.average*m) / partition_length; 

            clock_gettime(CLOCK_MONOTONIC, &s.end_time);
            int ret = write(pipefds[i][1],(void*)&s,sizeof(s));
            if (ret < sizeof(s))
            {
                err(-1, "Process with PID %d Failed to write to pipe. Exiting.", mypid);
            }
        }
   
        // New behavior for part 2
        raise(SIGSTOP);  
        sleep(100);
        log_msg("Child with PID %d finished its sleep(100) system call. Exiting.", getpid());
        exit(1 + m*i + tree_id);
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
    int isReaped[m] = {0};
    log_msg("ECE 434 Sp26: Process with PID %d is waiting for its children to report back", mypid);
    // Use a shell to print out pstree since the function isn't provided. Note this is not stored in the log file.
     // only print tree if children are leaf workers to avoid clutter
    print_pstree();

    struct search_result child_results[m];

    while (remaining_children)
    {
        int event_count = poll(pollfds, m, POLL_TIMEOUT);
        if (event_count < 0)
        {
            err(-1, "A polling error (%d) occured",event_count);
        }
        if (event_count == 0)
        {
            // TIMEOUT passed, killing children & reassigning task
            for (int i = 0; i < m; i++)
            {
                if (isReaped[i])
                {
                    continue;
                }
                kill(pids[i], SIGKILL);
                waitpid(pids[i], NULL, 0);
                return create_tree(my_partition,partition_length,tree_id,processes_budget, result);
            }
        }
        if (event_count)
        {
            for (int i = 0; i < m; i++)
            {

                if (isReaped[i] || !(pollfds[i].revents & (POLLIN | POLLERR)))
                {
                    continue;
                }

                int status;
                int ret = waitpid(pids[i], &status, WNOHANG | WUNTRACED);
                if (ret == 0)
                {
                    continue;
                }
                
                explain_wait_status(pids[i], status);
                
                isReaped[i] = 1;
                if (pollfds[i].revents & POLLIN)
                {
                    int readBytes = read(pollfds[i].fd,&child_results[i],sizeof(child_results[i]));
                    if (readBytes < sizeof(child_results[i]))
                    {
                        err(-1, "Process with PID %d failed to read the result of child with PID %d from pipe. Exiting.", mypid, pids[i]);
                    }   
                    log_msg("ECE 434 Sp26: Process %d received data from child %d: max=%d, average=%f, keys_found=%d,start_time=%ld.%09ld,end_time=%ld.%09ld\n", mypid, pids[i], child_results[i].max, child_results[i].average, child_results[i].keys_found, child_results[i].start_time.tv_sec, child_results[i].start_time.tv_nsec, child_results[i].end_time.tv_sec, child_results[i].end_time.tv_nsec);
                    
                    if (child_results[i].max > result->max)
                    {
                        result->max = child_results[i].max;
                    }
                    result->average += child_results[i].average;
                    result->keys_found += child_results[i].keys_found;
                    result->elements_processed += child_results[i].elements_processed;
                    result->bytes_sent += child_results[i].bytes_sent;

                    log_csv(pids[i], &child_results[i]);
                    
                    remaining_children--;
                }
                else if (pollfds[i].revents & POLLERR)
                {
                    err(-1, "Polling error on child %d in process %d", pids[i], mypid);
                }
            }
        }
        
    }

    int max_keys_found = 0;
    int max_keys_found_pid = -1;
    int min_keys_found = 150; // max possible keys found since there are only 150 hidden numbers
    int min_keys_found_pid = -1;

    for (int i = 0; i < m; i++)
    {
        if (child_results[i].keys_found > max_keys_found)
        {
            max_keys_found = child_results[i].keys_found;
            max_keys_found_pid = pids[i];
        }
        if (child_results[i].keys_found < min_keys_found)
        {
            min_keys_found = child_results[i].keys_found;
            min_keys_found_pid = pids[i];
        }
    }

    printf("Tree before process %d sends out signals to its children:\n", mypid);
    print_pstree();

    log_msg("Process %d is sending signals to its children. Child with PID %d found the most keys (%d) and child with PID %d found the least keys (%d). Other children will receive a random secret number with SIGUSR1.\n", mypid, max_keys_found_pid, max_keys_found, min_keys_found_pid, min_keys_found);
    kill(max_keys_found_pid, SIGCONT);

    for (int i = 0; i < m; i++)
    {
        if (pids[i] != max_keys_found_pid && pids[i] != min_keys_found_pid)
        {
            const union sigval sigval = { .sival_int = rand() % 22 + 10 }; // corresponding to random usable signal
            sigqueue(pids[i], SIGUSR1, sigval); // send random secret number
            kill(pids[i], SIGCONT);
        }
    }
    kill(min_keys_found_pid, SIGCONT);
    sleep(10);
    kill(min_keys_found_pid, SIGINT);
    // a bit later
    printf("Tree after process %d sends out signals to its children and before it issues to them SIGQUIT:\n", mypid);
    print_pstree();
    sleep(4);
    for (int i = 0; i < m; i++)
    {
        kill(pids[i], SIGQUIT);
    }

    result->average /= m;
    clock_gettime(CLOCK_MONOTONIC, &result->end_time);
    return tree_id;
}

void log_msg(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    printf("\n");
    fprintf(log_file, "\n");
}

void print_pstree()
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "pstree -p %d", getpid());
    system(cmd);
}

void log_csv(pid_t PID, struct search_result* result)
{
    fprintf(csv_file, "%d,%d,%ld,%d,%f,%d,%ld,%09ld,%ld,%09ld\n", PID, result->elements_processed, result->bytes_sent, result->max, result->average, result->keys_found, result->start_time.tv_sec, result->start_time.tv_nsec, result->end_time.tv_sec, result->end_time.tv_nsec);
}

// this value is computed by mapping the start of this child's unique partition to an integer 0 from to 255 (the range of valid return codes)
void explain_wait_status(pid_t pid, int status)
{
    if (WIFEXITED(status))
    {
        log_msg("ECE 434 Sp26: Child with PID %d exited normally with code: %d\n", pid, WEXITSTATUS(status));
    }
    else if (WIFSIGNALED(status))
    {
        log_msg("ECE 434 Sp26: Child with PID %d terminated by signal: %d\n", pid, WTERMSIG(status));
    }
    else if (WIFSTOPPED(status))
    {
        log_msg("ECE 434 Sp26: Child with PID %d stopped by signal: %d\n", pid, WSTOPSIG(status));
    }
    else
    {
        log_msg("ECE 434 Sp26: Child with PID %d: status unknown\n", pid);
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
    for (int hidden = 0; hidden < 150; hidden++) // 150 always according to instructions
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

