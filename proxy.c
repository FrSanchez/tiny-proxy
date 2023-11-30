#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "proxy.h"
#include "hash_map.h"
#include "hash.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3";

int main(int argc, char **argv)
{
    // printf("%s", user_agent_hdr);
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    struct stat st = {0};

    if (stat("cache", &st) == -1)
    {
        printf("Creating cache folder\n");
        mkdir("cache", 0700);
    }
    printf("Initiating Proxy, accepting connections on port %s\n", argv[1]);
    listenfd = Open_listenfd(argv[1]);
    while (1)
    {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // line:netp:tiny:accept
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE,
                    port, MAXLINE, 0);
        printf("----------------------------------------\nAccepted connection from (%s, %s)\n", hostname, port);
        doit(connfd); // line:netp:tiny:doit
        printf("Closing connection from (%s, %s)\n\n", hostname, port);
        Close(connfd); // line:netp:tiny:close
    }
}

void send_file(int fd, char *filename, int filesize)
{
    /* Send response body to client */
    int srcfd = Open(filename, O_RDONLY, 0);                          // line:netp:servestatic:open
    char *srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // line:netp:servestatic:mmap
    Close(srcfd);                                                     // line:netp:servestatic:close
    Rio_writen(fd, srcp, filesize);                                   // line:netp:servestatic:write
    Munmap(srcp, filesize);                                           // line:netp:servestatic:munmap
}
/*
 * serve_static - copy a file back to the client
 */
/* $begin serve_static */
void serve_static(int fd, char *filename, int filesize)
{
    char buf[MAXBUF];
    char meta[MAXLINE];
    sprintf(meta, "%s.meta", filename);

    /* Send response headers to client */
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); // line:netp:servestatic:beginserve
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Proxy Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "X-Proxied-data: true\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n", filesize);
    Rio_writen(fd, buf, strlen(buf));

    int meta_fd = Open(meta, O_RDONLY, 0);
    rio_t rio_meta;
    Rio_readinitb(&rio_meta, meta_fd);
    int rc = 0;
    do
    {
        rc = Rio_readlineb(&rio_meta, buf, MAXLINE);
        if (rc > 0)
        {
            Rio_writen(fd, buf, strlen(buf));
        }
    } while (rc > 0);
    Close(meta_fd);
    sprintf(buf, "\r\n");
    Rio_writen(fd, buf, strlen(buf));
    send_file(fd, filename, filesize);
}

void send_headers(int fd, HashMap *headers)
{
    char buf[MAXLINE];
    printf("Sending %d headers\n", headers->len);
    for (int i = 0; i < headers->cap; i++)
    {
        Pair *pair = headers->list[i];
        while (pair)
        {
            buf[0] = '\0'; // reset buf
            sprintf(buf, "%s: %s\r\n", pair->key, pair->val);
            printf("%s", buf);
            Rio_writen(fd, buf, strlen(buf));
            pair = pair->next;
        }
    }
    sprintf(buf, "\r\n");
    Rio_writen(fd, buf, strlen(buf));
}

void relay_data(int out_b, rio_t *in_b)
{
    char buf[MAXLINE];
    size_t bytes = 0;
    do
    {
        bytes = Rio_readnb(in_b, buf, MAXLINE);
        printf("Read %ld bytes\n", bytes);
        if (bytes > 0)
        {
            Rio_writen(out_b, buf, bytes);
        }
    } while (bytes > 0);
}

void fix_headers(HashMap *headers, const char *host)
{
    if (hash_get(headers, "User-Agent") == NULL)
    {
        hash_set(headers, "User-Agent", user_agent_hdr);
    }
    hash_set(headers, "Connection", "close");
    hash_set(headers, "Proxy-Connection", "close");
    if (hash_get(headers, "Host") == NULL)
    {
        hash_set(headers, "Host", host);
    }
}

void send_request(int fd, char *method, URL url_info)
{
    char buf[MAXLINE];
    sprintf(buf, "%s %s", method, url_info.path);
    if (url_info.query)
    {
        sprintf(buf, "%s?%s", buf, url_info.query);
    }
    sprintf(buf, "%s HTTP/1.0\r\n", buf);
    Rio_writen(fd, buf, strlen(buf));
}

void write_meta(char *filename, HashMap *headers)
{
    char *headersToKeep[] = {"Content-Type", NULL};
    char buf[MAXLINE];
    sprintf(buf, "%s.meta", filename);
    int meta_fd = Open(buf, O_WRONLY | O_CREAT, 0644);
    rio_t rio_meta;
    Rio_readinitb(&rio_meta, meta_fd);
    for (int i = 0; headersToKeep[i] != NULL; i++)
    {
        char *key = headersToKeep[i];
        char *val = hash_get(headers, key);
        if (val)
        {
            sprintf(buf, "%s: %s\r\n", key, val);
            Rio_writen(meta_fd, buf, strlen(buf));
        }
    }
    Close(meta_fd);
}

void save_data(char *filename, rio_t *response)
{
    int fd = Open(filename, O_WRONLY | O_CREAT, 0644);
    relay_data(fd, response);
    Close(fd);
}
/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd)
{
    char buf[MAXLINE], method[MAXLINE], url[MAXLINE], version[MAXLINE];
    char filename[MAXLINE];
    rio_t rio;
    HashMap *headers;
    char hash[32];
    URL url_info;

    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    if (!Rio_readlineb(&rio, buf, MAXLINE)) // line:netp:doit:readrequest
        return;
    sscanf(buf, "%s %s %s", method, url, version); // line:netp:doit:parserequest
    printf("[%s] => %s %s %s\n", buf, method, url, version);

    if (strcasecmp(method, "GET") != 0)
    { // line:netp:doit:beginrequesterr
        clienterror(fd, method, "501", "Not Implemented",
                    "Proxy does not support this method");
        return;
    } // line:netp:doit:endrequesterr
    calculate_hash(url, hash);
    printf("%s -> %s\n", url, hash);
    parse_url(url, &url_info);
    if (strcasecmp(url_info.scheme, "http") != 0)
    {
        clienterror(fd, url_info.scheme, "501", "Not Implemented",
                    "Proxy does not support this scheme");
        return;
    }

    headers = newHashMap(32);

    read_requesthdrs(&rio, headers); // line:netp:doit:readrequesthdrs

    char hostHeader[MAXLINE];
    sprintf(hostHeader, "%s:%s", url_info.host, url_info.port);
    fix_headers(headers, hostHeader);

    sprintf(filename, "cache/%s", hash);
    printf("Looking for cached file %s\n", filename);
    struct stat st = {0};
    if (stat(filename, &st) == -1)
    {
        printf("\nOpening %s:%s\n", url_info.host, url_info.port);
        int upstream_fd = open_clientfd(url_info.host, url_info.port);
        if (upstream_fd > 0)
        {
            char status_code[4];
            char status_text[MAXLINE];
            rio_t response;
            HashMap *response_headers = newHashMap(32);
            printf("Connected to %s:%s\n", url_info.host, url_info.port);
            send_request(upstream_fd, method, url_info);
            send_headers(upstream_fd, headers);

            // read response
            Rio_readinitb(&response, upstream_fd);
            Rio_readlineb(&response, buf, MAXLINE);
            sscanf(buf, "%s %s %s", version, status_code, status_text);
            printf("Status: %s %s\n", status_code, status_text);
            read_requesthdrs(&response, response_headers);

            printf("Sending response to client\n");
            Rio_writen(fd, buf, strlen(buf));
            send_headers(fd, response_headers);
            if (strcasecmp(status_code, "200") != 0)
            {
                relay_data(fd, &response);
            }
            else
            {
                printf("Saving data to %s\n", filename);
                save_data(filename, &response);
                write_meta(filename, response_headers);
                stat(filename, &st);
                sprintf(buf, "\r\n");
                // Rio_writen(fd, buf, strlen(buf));
                send_file(fd, filename, st.st_size);
            }
            // relay_data(rio.rio_fd, &response);
            Close(upstream_fd);
        }
        else
        {
            printf("Failed to connect to %s:%s\n", url_info.host, url_info.port);
        }
        freeHashMap(headers);
    }
    else
    {
        printf("*** Serving cached data**\n");
        serve_static(fd, filename, st.st_size);
    }
}

/* $end doit */

void store_header(HashMap *headers, char *header)
{
    if (strlen(header) == 0)
    {
        return;
    }
    char *raw_header = strdup(header);
    char *value = strstr(raw_header, ": ");
    if (!value)
    {
        return;
    }
    *value = '\0';
    value += 2;
    char *nl = strstr(value, "\r\n");
    if (nl)
    {
        *nl = '\0';
    }
    if (strlen(raw_header) == 0)
    {
        printf("Skip empty header\n");
        return;
    }
    hash_set(headers, raw_header, value);
    printf("[%s: %s]\n", raw_header, value);
}

/*
 * read_requesthdrs - read HTTP request headers
 */
/* $begin read_requesthdrs */
void read_requesthdrs(rio_t *rp, HashMap *headers)
{
    char buf[MAXLINE];

    int rc = Rio_readlineb(rp, buf, MAXLINE);
    if (rc == 0)
    {
        return;
    }
    store_header(headers, buf);
    while (strcmp(buf, "\r\n"))
    { // line:netp:readhdrs:checkterm
        rc = Rio_readlineb(rp, buf, MAXLINE);
        if (rc == 0)
        {
            return;
        }
        store_header(headers, buf);
    }
    return;
}
/* $end read_requesthdrs */

int parse_url(char *url, URL *urlInfo)
{
    // https://en.wikipedia.org/wiki/URL
    // scheme:[//[user[:password]@]host[:port]][/]path[?query][#fragment]
    // http://www.example.com:80/path/to/myfile.html?key1=value1&key2=value2#SomewhereInTheDocument
    // http://www.example.com:80/path/to/myfile.html
    if (!url || strlen(url) == 0)
    {
        return -1;
    }
    char *scheme = url;
    char *host = strstr(scheme, "://");
    host[0] = '\0';
    host += 3;
    char *fragment = strstr(host, "#");
    if (fragment)
    {
        fragment[0] = '\0';
        fragment++;
    }
    char *query = strstr(host, "?");
    if (query)
    {
        query[0] = '\0';
        query++;
    }
    char *path = strstr(host, "/");
    if (path)
    {
        char *dup = strdup(path);
        path[0] = '\0';
        path = dup;
    }
    else
    {
        path = "/";
    }
    char *port = strstr(host, ":");
    if (port)
    {
        port[0] = '\0';
        port++;
    }
    else
    {
        port = "80";
    }

    printf("scheme: %s\n", scheme);
    printf("host: %s\n", host);
    printf("port: %s\n", port);
    printf("path: %s\n", path);
    printf("query: %s\n", query);
    printf("fragment: %s\n", fragment);
    urlInfo->scheme = scheme;
    urlInfo->host = host;
    urlInfo->port = port;
    urlInfo->path = path;
    urlInfo->query = query;
    urlInfo->fragment = fragment;
    return 0;
}

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Tiny Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor="
                 "ffffff"
                 ">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
    printf("Return %s %s\n%s\n", errnum, shortmsg, longmsg);
}
/* $end clienterror */
