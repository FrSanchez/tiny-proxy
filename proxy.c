#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "proxy.h"
#include <openssl/evp.h>
#include "hash_map.h"

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

/*
 * serve_static - copy a file back to the client
 */
/* $begin serve_static */
void serve_static(int fd, char *filename, int filesize)
{
    int srcfd;
    char *srcp, buf[MAXBUF];

    /* Send response body to client */
    srcfd = Open(filename, O_RDONLY, 0);                        // line:netp:servestatic:open
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // line:netp:servestatic:mmap
    Close(srcfd);                                               // line:netp:servestatic:close
    Rio_writen(fd, srcp, filesize);                             // line:netp:servestatic:write
    Munmap(srcp, filesize);                                     // line:netp:servestatic:munmap
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

void relay_data(rio_t *out_b, rio_t *in_b)
{
    char buf[MAXLINE];
    size_t bytes = 0;
    do
    {
        bytes = Rio_readnb(in_b, buf, MAXLINE);
        printf("Read %ld bytes\n", bytes);
        if (bytes > 0)
        {
            Rio_writen(out_b->rio_fd, buf, bytes);
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
/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd)
{
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], url[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;
    HashMap *headers;

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
    URL url_info;
    char md5[33];
    bytes2md5(url, strlen(url), md5);
    printf("URL MD5: %s\n", md5);
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

    printf("\nOpening %s:%s\n", url_info.host, url_info.port);
    int upstream_fd = open_clientfd(url_info.host, url_info.port);
    if (upstream_fd > 0)
    {
        rio_t response;
        HashMap *response_headers = newHashMap(32);
        printf("Connected to %s:%s\n", url_info.host, url_info.port);
        send_request(upstream_fd, method, url_info);
        send_headers(upstream_fd, headers);
        Rio_readinitb(&response, upstream_fd);
        Rio_readlineb(&response, buf, MAXLINE);
        printf("Status: %s\n", buf);
        Rio_writen(fd, buf, strlen(buf));
        read_requesthdrs(&response, response_headers);
        send_headers(fd, response_headers);
        relay_data(&rio, &response);
        Close(upstream_fd);
    }
    else
    {
        printf("Failed to connect to %s:%s\n", url_info.host, url_info.port);
    }
    freeHashMap(headers);
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

// https://stackoverflow.com/questions/7627723/how-to-create-a-md5-hash-of-a-string-in-c
void bytes2md5(const char *data, int len, char *md5buf)
{
    // Based on https://www.openssl.org/docs/manmaster/man3/EVP_DigestUpdate.html
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    const EVP_MD *md = EVP_md5();
    unsigned char md_value[EVP_MAX_MD_SIZE];
    unsigned int md_len, i;
    EVP_DigestInit_ex(mdctx, md, NULL);
    EVP_DigestUpdate(mdctx, data, len);
    EVP_DigestFinal_ex(mdctx, md_value, &md_len);
    EVP_MD_CTX_free(mdctx);
    for (i = 0; i < md_len; i++)
    {
        snprintf(&(md5buf[i * 2]), 16 * 2, "%02x", md_value[i]);
    }
}