#ifndef __HASH_MAP_H__
#define __HASH_MAP_H__

typedef struct pair
{
    char *key;
    char *val;
    struct pair *next;
} Pair;

typedef struct hashmap
{
    Pair **list;      // Pair* list
    unsigned int cap; // capacity, the length of list
    unsigned int len; // length, the number of pairs
} HashMap;

HashMap *newHashMap(int capacity);
void hash_set(HashMap *, char *key, char *val);
char *hash_get(HashMap *, char *key);
void freeHashMap(HashMap *this);

#endif