#include <stdio.h>
#include <string.h>

typedef unsigned long int _data;
union code
{
    char string[sizeof(_data)];
    _data code;
};

/*
Algorithm described in:
https://en.wikipedia.org/wiki/Hash_function
Under Word Length Folding
    * calculate_hash: calculate the hash of a string
    * @string: the string to calculate
    * @hash: the hash result
    */
void calculate_hash(char *string, char *hash)
{
    _data sum = 0;
    for (sum = 0; *string != '\0';)
    {
        union code c;
        strncpy(c.string, string, sizeof(_data));
        sum ^= c.code * c.code;
        int stp = 0;
        for (stp = 0; stp < sizeof(_data) && *string != '\0'; stp++)
        {
            string++;
        }
    }
    sprintf(hash, "%lx", sum);
}
