/*
 * Starter code for proxy lab.
 * Feel free to modify this code in whatever way you wish.
 */

/* Some useful includes to help you get started */

#include "csapp.h"

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "cache.h"
#include "http_parser.h"
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>

/*
 * Debug macros, which can be enabled by adding -DDEBUG in the Makefile
 * Use these if you find them useful, or delete them if not
 */
#ifdef DEBUG
#define dbg_assert(...) assert(__VA_ARGS__)
#define dbg_printf(...) fprintf(stderr, __VA_ARGS__)
#else
#define dbg_assert(...)
#define dbg_printf(...)
#endif

/*
 * Max cache and object sizes
 * You might want to move these to the file containing your cache implementation
 */
#define MAX_CACHE_SIZE (1024 * 1024)
#define MAX_OBJECT_SIZE (100 * 1024)
#define HOSTLEN 256
#define SERVLEN 8

/*
 * String to use for the User-Agent header.
 * Don't forget to terminate with \r\n
 */
static const char *header_user_agent = "Mozilla/5.0"
                                       " (X11; Linux x86_64; rv:3.10.0)"
                                       " Gecko/20230411 Firefox/63.0.1";
static const char *connection =
    "Connection: close\r\nProxy-Connection: close\r\n";
pthread_mutex_t mutex;

/* Typedef for convenience */
typedef struct sockaddr SA;

typedef struct {
    struct sockaddr_in addr; // Socket address
    socklen_t addrlen;       // Socket address length
    int connfd;              // Client connection file descriptor
    char host[HOSTLEN];      // Client host
    char serv[SERVLEN];      // Client service (port)
} client_info;
void process_request(client_info *client);
void clienterror(int fd, const char *errnum, const char *shortmsg,
                 const char *longmsg);
void generate_request(char new_quest[], const char *host, const char *path,
                      const char *port);

// generate r
void generate_request(char new_request[], const char *host, const char *path,
                      const char *port) {

    char server_header[MAXLINE];
    char server_host[MAXLINE];
    char ua[MAXLINE];

    sprintf(server_header, "GET %s HTTP/1.0\r\n", path);
    strcat(new_request, server_header);
    sprintf(server_host, "Host: %s:%s\r\n", host, port);
    strcat(new_request, server_host);
    sprintf(ua, "User-Agent: %s\r\n", header_user_agent);
    strcat(new_request, ua);
    strcat(new_request, connection);
}

/*
 * clienterror - returns an error message to the client
 */
void clienterror(int fd, const char *errnum, const char *shortmsg,
                 const char *longmsg) {
    char buf[MAXLINE];
    char body[MAXBUF];
    size_t buflen;
    size_t bodylen;

    /* Build the HTTP response body */
    bodylen = snprintf(body, MAXBUF,
                       "<!DOCTYPE html>\r\n"
                       "<html>\r\n"
                       "<head><title>Proxy Error</title></head>\r\n"
                       "<body bgcolor=\"ffffff\">\r\n"
                       "<h1>%s: %s</h1>\r\n"
                       "<p>%s</p>\r\n"
                       "<hr /><em>The Proxy Web server</em>\r\n"
                       "</body></html>\r\n",
                       errnum, shortmsg, longmsg);
    if (bodylen >= MAXBUF) {
        return; // Overflow!
    }

    /* Build the HTTP response headers */
    buflen = snprintf(buf, MAXLINE,
                      "HTTP/1.0 %s %s\r\n"
                      "Content-Type: text/html\r\n"
                      "Content-Length: %zu\r\n\r\n",
                      errnum, shortmsg, bodylen);
    if (buflen >= MAXLINE) {
        return; // Overflow!
    }

    /* Write the headers */
    if (rio_writen(fd, buf, buflen) < 0) {
        fprintf(stderr, "Error writing error response headers to client\n");
        return;
    }

    /* Write the body */
    if (rio_writen(fd, body, bodylen) < 0) {
        fprintf(stderr, "Error writing error response body to client\n");
        return;
    }
}

void process_request(client_info *client) {
    int res = getnameinfo((SA *)&client->addr, client->addrlen, client->host,
                          sizeof(client->host), client->serv,
                          sizeof(client->serv), 0);
    if (res == 0) {
        printf("Accepted connection from %s:%s\n", client->host, client->serv);
    } else {
        fprintf(stderr, "getnameinfo failed: %s\n", gai_strerror(res));
    }

    rio_t rio, rio_server;

    rio_readinitb(&rio, client->connfd);

    char buf[MAXLINE];
    // reset buf
    memset(buf, 0, MAXLINE);
    char key[MAXLINE];
    memset(key, 0, MAXLINE);

    int n;
    const char *method, *path, *host, *port, *uri;
    parser_t *parser = parser_new();
    char new_request[MAXLINE];
    // reset request line
    memset(new_request, 0, MAXLINE);

    parser_state state;
    int server_fd = 0;

    while ((n = rio_readlineb(&rio, buf, sizeof(buf))) > 0 &&
           (strcmp(buf, "\r\n") != 0)) {

        state = parser_parse_line(parser, buf);
        // error case
        if (state == ERROR) {
            parser_free(parser);

            clienterror(client->connfd, "400", "Bad Request",
                        "Proxy received a malformed request");
            return;
        }
        // request case
        if (state == REQUEST) {

            parser_retrieve(parser, METHOD, &method);

            if (strcmp(method, "GET") != 0) {
                parser_free(parser);
                clienterror(client->connfd, "501", "Not Implemented",
                            "Proxy does not implement this method");
                return;
            }
            parser_retrieve(parser, URI, &uri);

            memcpy(key, uri, strlen(uri));
            sio_printf("This is uri\n%s\n", key);

            pthread_mutex_lock(&mutex);
            block_t *block = search_cache(key);

            if (block != NULL) {
                char tmp[MAX_OBJECT_SIZE];
                memset(tmp, 0, MAX_OBJECT_SIZE);

                size_t length = block->value_length;
                memcpy(tmp, block->value, length);
                pthread_mutex_unlock(&mutex);

                rio_writen(client->connfd, tmp, length);
                return;
            }
            pthread_mutex_unlock(&mutex);

            parser_retrieve(parser, PATH, &path);

            parser_retrieve(parser, HOST, &host);

            parser_retrieve(parser, PORT, &port);

            server_fd = open_clientfd(host, port);
            if (server_fd < 0) {
                sio_printf("Connection failed\n");
                close(server_fd);
                return;
            }
            rio_readinitb(&rio_server, server_fd);

            generate_request(new_request, host, path, port);
        }

        if (state == HEADER) {
            char current[MAXLINE];
            header_t *header;

            while ((header = parser_retrieve_next_header(parser)) != NULL) {
                // sio_printf("the header is %s\n", header->name);
                //  skip this host connection.. part
                if ((!strcmp(header->name, "Host") ||
                     !strcmp(header->name, "Connection") ||
                     !strcmp(header->name, "Proxy-Connection") ||
                     !strcmp(header->name, "User-Agent"))) {
                    continue;
                }

                sprintf(current, "%s:%s\r\n", header->name, header->value);
                strcat(new_request, current);
            }
        }
    }
    // client error
    if (strcmp(new_request, "") == 0) {

        parser_free(parser);

        clienterror(client->connfd, "400", "Bad Request",
                    "Proxy received a malformed request");
        close(server_fd);

        return;
    }

    strcat(new_request, "\r\n");
    parser_free(parser);

    if ((rio_writen(server_fd, new_request, MAXLINE)) == -1) {
        sio_printf("error\n");
    }
    int n2;
    char new_buf[MAXLINE];

    char value[MAX_OBJECT_SIZE];

    memset(new_buf, 0, MAXLINE);
    memset(value, 0, MAX_OBJECT_SIZE);
    size_t current_index = 0;

    while ((n2 = rio_readnb(&rio_server, new_buf, MAXLINE)) > 0) {
        if (current_index + n2 < MAX_OBJECT_SIZE) {
            memcpy(&value[current_index], new_buf, n2);
        }
        current_index += n2;
        // sio_printf("%s ref\n",new_buf);

        rio_writen(client->connfd, new_buf, n2);
    }

    pthread_mutex_lock(&mutex);
    sio_printf("Attention: The key is %s\n", key);
    if (current_index <= MAX_OBJECT_SIZE) {
        add_block(key, value, current_index);
    }

    pthread_mutex_unlock(&mutex);

    close(server_fd);
}
void *thread(void *vargp) {
    client_info *client = ((client_info *)vargp);
    pthread_detach(pthread_self());

    process_request(client);
    close(client->connfd);

    free(client);
    return NULL;
}

int main(int argc, char **argv) {

    int listenfd;

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    cache_init();
    pthread_mutex_init(&mutex, NULL);
    signal(SIGPIPE, SIG_IGN);

    // Open listening file descriptor
    listenfd = open_listenfd(argv[1]);
    if (listenfd < 0) {
        fprintf(stderr, "Failed to listen on port: %s\n", argv[1]);
        exit(1);
    }
    while (1) {
        /* Allocate space on the stack for client info */
        client_info *client = malloc(sizeof(client_info));

        /* Initialize the length of the address */
        client->addrlen = sizeof(client->addr);

        /* accept() will block until a client connects to the port */
        client->connfd =
            accept(listenfd, (SA *)&client->addr, &client->addrlen);
        if (client->connfd < 0) {
            perror("accept");
            continue;
        }
        pthread_t tid;
        pthread_create(&tid, NULL, thread, (void *)(client));
    }
    cache_free();
    pthread_mutex_destroy(&mutex);
    return 0;
}
