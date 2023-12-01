#include "hash.h"
#include <stdio.h>

#define HASH_SIZE 20

int main(int argc, char **argv)
{
    char *strings[] = {
        "hello world",
        "http://www.google.com",
        "http://www.bing.com",
        "https://www.baidu.com",
        "https://dss0.bdstatic.com/5aV1bjqh_Q23odCf/static/superman/img/topnav/newfanyi-da0cea8f7e.png",
        NULL};
    char hash[HASH_SIZE];
    for (int i = 0; strings[i] != NULL; i++)
    {
        calculate_hash(strings[i], hash);
        printf("%s: %s\n", hash, strings[i]);
    }
    return 0;
}