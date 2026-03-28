#include "main.h"

// B-tree parameter
#define m 2

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
    if (!generate_file(L, &arr))
    {
        free(arr);
        return -1;
    }

    create_tree(arr, L, NP, H);

    free(arr);


    return 0;
}

int create_tree(int16_t* partition_ptr, int partition_length, int processes_budget, int H)
{
    int pipefds[m][2];

    // do somme recursive stuff here
    return 0;
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

