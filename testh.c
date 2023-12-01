#include <stdio.h>
#include <string.h>
#include "hash.h"

#define HASH_SIZE 20

int size_of_array(char *array[])
{
    int i = 0;
    while (array[i] != NULL)
        i++;
    return i;
}

int main(int argc, char **argv)
{
    char *strings[] = {
        "hello world",
        "http://www.google.com",
        "http://www.bing.com",
        "https://www.baidu.com",
        "https://dss0.bdstatic.com/5aV1bjqh_Q23odCf/static/superman/img/topnav/newfanyi-da0cea8f7e.png",
        NULL};

    int sz = size_of_array(strings);

    char hash[sz][HASH_SIZE];
    printf("Computing hashes\n");
    for (int i = 0; i < sz; i++)
    {
        calculate_hash(strings[i], hash[i]);
        printf("%s: %s\n", hash[i], strings[i]);
    }
    printf("Done\nValidating they are all different\n");
    for (int i = 0; i < sz; i++)
    {
        for (int j = 0; j < i; j++)
        {
            if (strcmp(hash[i], hash[j]) == 0)
            {
                printf("Hashes %d and %d are the same\n", i, j);
                return 1;
            }
        }
    }
    return 0;
}