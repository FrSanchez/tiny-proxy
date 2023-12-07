#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hash_map.h"

// file from https://github.com/ksw2000/Medium/blob/main/implement-a-very-simple-hashmap-with-c/hashmap.c

HashMap *newHashMap(int capacity)
{
    HashMap *this = malloc(sizeof(this));
    this->cap = capacity; // set default capacity
    this->len = 0;        // no pair in map
    // set all pointer to null in this->list
    this->list = calloc((this->cap), sizeof(Pair *));
    return this;
}

void freeHashMap(HashMap *this)
{
    for (int i = 0; i < this->cap; i++)
    {
        Pair *current = this->list[i];
        while (current)
        {
            Pair *next = current->next;
            free(current);
            current = next;
        }
    }
    free(this->list);
    free(this);
}

unsigned hashcode(HashMap *this, char *key)
{
    unsigned code;
    for (code = 0; *key != '\0'; key++)
    {
        code = *key + 31 * code;
    }
    return code % (this->cap);
}

char *hash_get(HashMap *this, char *key)
{
    Pair *current;
    for (current = this->list[hashcode(this, key)]; current;
         current = current->next)
    {
        if (!strcmp(current->key, key))
        {
            return current->val;
        }
    }
    fprintf(stderr, "%s is not found\n", key);
    return NULL;
}

// If key is not in hashmap, put into map. Otherwise, replace it.
void hash_set(HashMap *this, char *key, char *val)
{
    unsigned index = hashcode(this, key);
    Pair *current;
    for (current = this->list[index]; current; current = current->next)
    {
        // if key has been already in hashmap
        if (!strcmp(current->key, key))
        {
            current->val = val;
            return;
        }
    }

    // key is not in hashmap
    Pair *p = malloc(sizeof(*p));
    p->key = key;
    p->val = val;
    p->next = this->list[index];
    this->list[index] = p;
    this->len++;
}