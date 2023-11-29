#include <stdio.h>
#include "proxy.h"
#include "hash_map.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

int main(int argc, char **argv)
{
    printf("%s", user_agent_hdr);
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

    listenfd = Open_listenfd(argv[1]);
    while (1)
    {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // line:netp:tiny:accept
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE,
                    port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        doit(connfd); // line:netp:tiny:doit
        printf("Closing connection from (%s, %s)\n", hostname, port);
        Close(connfd); // line:netp:tiny:close
    }
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
            sprintf(buf, "%s: %s\r\n", pair->key, pair->val);
            printf("%s", buf);
            Rio_writen(fd, buf, strlen(buf));
            pair = pair->next;
        }
    }
    sprintf(buf, "\r\n");
    Rio_writen(fd, buf, strlen(buf));
}

void fix_headers(HashMap *headers)
{
    hash_set(headers, "User-Agent", user_agent_hdr);
    hash_set(headers, "Connection", "close");
    hash_set(headers, "Proxy-Connection", "close");
}
/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd)
{
    HashMap *headers = newHashMap(32);

    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], url[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    if (!Rio_readlineb(&rio, buf, MAXLINE)) // line:netp:doit:readrequest
        return;
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, url, version); // line:netp:doit:parserequest
    if (strcasecmp(method, "GET"))
    { // line:netp:doit:beginrequesterr
        clienterror(fd, method, "501", "Not Implemented",
                    "Proxy does not support this method");
        return;
    }                                // line:netp:doit:endrequesterr
    read_requesthdrs(&rio, headers); // line:netp:doit:readrequesthdrs
    fix_headers(headers);

    URL url_info;
    parse_url(url, &url_info);
    if (strcasecmp(url_info.scheme, "http"))
    {
        clienterror(fd, url_info.scheme, "501", "Not Implemented",
                    "Proxy does not support this scheme");
        return;
    }
    printf("Opening %s:%s\n", url_info.host, url_info.port);
    int upstream_fd = open_clientfd(url_info.host, url_info.port);
    if (upstream_fd > 0)
    {
        printf("Connected to %s:%s\n", url_info.host, url_info.port);
        sprintf(buf, "GET %s HTTP/1.0\r\n", url_info.path);
        Rio_writen(upstream_fd, buf, strlen(buf));
        send_headers(upstream_fd, headers);
        close(upstream_fd);
    }
    else
    {
        printf("Failed to connect to %s:%s\n", url_info.host, url_info.port);
    }
}
/* $end doit */

void add_header(HashMap *headers, char *header)
{
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

    Rio_readlineb(rp, buf, MAXLINE);
    add_header(headers, buf);
    while (strcmp(buf, "\r\n"))
    { // line:netp:readhdrs:checkterm
        Rio_readlineb(rp, buf, MAXLINE);
        add_header(headers, buf);
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
}
/* $end clienterror */
