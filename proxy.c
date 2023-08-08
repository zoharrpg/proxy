/**
 * @file proxy.c
 * @brief A simple multithread proxy supporting LRU cache
 * @author Junshang Jia <junshanj@andrew.cmu.edu>
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

/**
 * The function generates an HTTP request with the specified host, path, and
 * port.
 *
 * @param new_request The `new_request` parameter is a character array that will
 * store the generated HTTP request.
 * @param host The `host` parameter represents the hostname or IP address of the
 * server you want to send the request to.
 * @param path The `path` parameter is a string that represents the path of the
 * resource on the server that you want to request. For example, if you want to
 * request the homepage of a website, the `path` could be "/index.html".
 * @param port The `port` parameter is a string that represents the port number
 * of the server you want to connect to.
 */
void generate_request(char new_request[], const char *host, const char *path,
                      const char *port) {

    char server_header[MAXLINE]; /*server header*/
    char server_host[MAXLINE];   /*host name*/
    char ua[MAXLINE];            /*user agent*/

    /*change to http 1.0*/
    sprintf(server_header, "GET %s HTTP/1.0\r\n", path);
    /*concate it to new_request*/
    strcat(new_request, server_header);
    /*store host and port*/
    sprintf(server_host, "Host: %s:%s\r\n", host, port);
    /*concate it to new_request*/
    strcat(new_request, server_host);
    /*stort header user agent*/
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

/**
 * The function `process_request` handles incoming client requests, retrieves
 * information from the request, sends it to a server, and caches the response
 * if necessary.
 *
 * @param client The `client` parameter is a pointer to a `client_info` struct.
 * This struct contains information about the client connection, such as the
 * client's address, connection file descriptor, and other relevant data.
 *
 */
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
    /*buffer*/
    char buf[MAXLINE];

    /*reset buf*/
    memset(buf, 0, MAXLINE);
    /*cache key*/
    char key[MAXLINE];
    /*reset cache key*/
    memset(key, 0, MAXLINE);

    int n;
    const char *method, *path, *host, *port, *uri;
    parser_t *parser = parser_new();
    char new_request[MAXLINE];
    // reset request line
    memset(new_request, 0, MAXLINE);

    parser_state state;

    int server_fd = 0; /*server fd*/
    /*read from client*/
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

            /*copy uri to key*/
            memcpy(key, uri, strlen(uri));

            /*check if key in the cache*/
            /*lock the global cache*/
            pthread_mutex_lock(&mutex);

            /*search key from the cache*/
            block_t *block = search_cache(key);
            /*if block is null, meaning it return the web object from cache*/
            if (block != NULL) {
                /*store cache value in the tmp*/
                char tmp[MAX_OBJECT_SIZE];
                memset(tmp, 0, MAX_OBJECT_SIZE);

                size_t length = block->value_length;
                memcpy(tmp, block->value, length);
                /*unlock*/
                pthread_mutex_unlock(&mutex);

                rio_writen(client->connfd, tmp, length);
                return;
            }
            /*unlock*/
            pthread_mutex_unlock(&mutex);

            int result;

            /*get path,host ,port*/
            if ((result = (parser_retrieve(parser, PATH, &path))) != 0) {
                parser_free(parser);

                clienterror(client->connfd, "400", "Bad Request",
                            "Proxy received a malformed request");
                return;
            }

            if ((result = parser_retrieve(parser, HOST, &host)) != 0) {
                parser_free(parser);

                clienterror(client->connfd, "400", "Bad Request",
                            "Proxy received a malformed request");
                return;
            }

            if ((result = parser_retrieve(parser, PORT, &port)) != 0) {
                parser_free(parser);

                clienterror(client->connfd, "400", "Bad Request",
                            "Proxy received a malformed request");
                return;
            }

            /*open server*/
            server_fd = open_clientfd(host, port);
            /*error on open server*/
            if (server_fd < 0) {
                sio_printf("Connection failed\n");
                close(server_fd);
                return;
            }
            rio_readinitb(&rio_server, server_fd);

            /*generate request*/
            generate_request(new_request, host, path, port);
        }
        /*parse header*/
        if (state == HEADER) {
            char current[MAXLINE];
            header_t *header;

            while ((header = parser_retrieve_next_header(parser)) != NULL) {
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

    /*send request to server*/
    if ((rio_writen(server_fd, new_request, MAXLINE)) == -1) {
        sio_printf("error\n");
    }
    int n2;
    char new_buf[MAXLINE];
    /*web object*/
    char value[MAX_OBJECT_SIZE];
    /*reset*/
    memset(new_buf, 0, MAXLINE);
    /*reset*/
    memset(value, 0, MAX_OBJECT_SIZE);
    /*index of valid data in the value array*/
    size_t current_index = 0;

    /*read data from server*/
    while ((n2 = rio_readnb(&rio_server, new_buf, MAXLINE)) > 0) {
        /*if size of data is greater than max_object_size, skip copy data*/
        if (current_index + n2 < MAX_OBJECT_SIZE) {
            memcpy(&value[current_index], new_buf, n2);
        }
        current_index += n2;

        rio_writen(client->connfd, new_buf, n2);
    }

    pthread_mutex_lock(&mutex);
    /*add block if size less than the MAX_OBJECT_SIZE*/
    if (current_index <= MAX_OBJECT_SIZE) {
        add_block(key, value, current_index);
    }

    pthread_mutex_unlock(&mutex);
    /*close serve connect*/
    close(server_fd);
}
/**
 * The function creates a new thread to process a client request and then closes
 * the connection and frees the memory.
 *
 * @param vargp The parameter `vargp` is a void pointer that is used to pass the
 * client information to the thread. It is casted to `client_info*` inside the
 * function to access the client information.
 *
 * @return a NULL pointer.
 */
void *thread(void *vargp) {
    /*client data*/
    client_info *client = ((client_info *)vargp);
    /*detach from main thread*/
    pthread_detach(pthread_self());
    /*process request*/
    process_request(client);
    /*close client*/
    close(client->connfd);
    /*free client resource*/
    free(client);
    return NULL;
}

/**
 * The main function is a server program that listens for incoming connections
 * on a specified port and creates a new thread to handle each client
 * connection.
 *
 * @param argc The argc parameter is an integer that represents the number of
 * command line arguments passed to the program.
 * @param argv port
 *
 */
int main(int argc, char **argv) {

    int listenfd;

    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    /*initialize cache*/
    cache_init();
    /*initialize lock*/
    pthread_mutex_init(&mutex, NULL);
    /*ignore SIGPIPE signal*/
    signal(SIGPIPE, SIG_IGN);

    // Open listening file descriptor
    listenfd = open_listenfd(argv[1]);
    if (listenfd < 0) {
        fprintf(stderr, "Failed to listen on port: %s\n", argv[1]);
        exit(1);
    }
    /*server rountine*/
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
        /*create a new thread to heandle request*/
        pthread_t tid;
        pthread_create(&tid, NULL, thread, (void *)(client));
    }
    /*clean resource*/
    cache_free();
    pthread_mutex_destroy(&mutex);
    return 0;
}
