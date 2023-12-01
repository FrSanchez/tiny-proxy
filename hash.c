#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef unsigned long int _data;
union code
{
    char *string;
    _data *code;
};

// https://en.wikipedia.org/wiki/2,147,483,647
#define PRIME 2147483647
/*
    Algorithm described in:
    https://en.wikipedia.org/wiki/Hash_function
    Under Word Length Folding and Character
* calculate_hash: calculate the hash of a string
* @string: the string to calculate
* @hash: the hash result
*/
void calculate_hash(char *string, char *hash)
{
    int slen = strlen(string);
    int sz = sizeof(_data);
    int len = (slen / sz) + 1;
    _data sum = 0;
    union code c;
    c.code = malloc(len * sizeof(_data));
    memset(c.code, 0, len * sizeof(_data));
    strcpy(c.string, string);
    for (int i = 0; i < len; i++)
    {
        sum *= PRIME;
        sum ^= c.code[i];
    }
    free(c.code);

    sprintf(hash, "%lx", sum);
}
