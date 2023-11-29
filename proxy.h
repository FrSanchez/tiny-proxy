#ifndef __PROXY_H__
#define __PROXY_H__

#include "csapp.h"
#include "hash_map.h"

typedef struct _url
{
    char *scheme;
    char *host;
    char *port;
    char *path;
    char *query;
    char *fragment;
} URL;

void doit(int fd);
void read_requesthdrs(rio_t *rp, HashMap *headers);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg);
int parse_url(char *url, URL *urlInfo);
void bytes2md5(const char *data, int len, char *md5buf);
#endif