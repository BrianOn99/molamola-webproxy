#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include "stdio.h"
#include "syslog.h"
#include "xmalloc.h"
#include "request_state.h"
#include "http_parser.h"

#define MAXFD 4096
#define RECV_BUF_SIZE 8192

/* the array to store socket fd for threads */
static int thread_sockfd[MAXFD];

static void close_serving_thread(int sockfd)
{
        close(sockfd);
}

/*
 * initialize socket
 */
static int init_sock(long port)
{
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);

        /* make the port reusable immediately after program killed */
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

        struct sockaddr_in my_addr = {
                .sin_family = AF_INET,
                .sin_port = htons(port),
                .sin_addr = {.s_addr = INADDR_ANY}
        };

        int ret = bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr));
        if (ret == -1 || listen(sockfd, 0) == -1)
                return -1;
        else
                return sockfd;
}

/*
 * the thread subroutine to serve 1 client
 */
static void *dedicated_serve(void *p)
{
        int sockfd = *((int*)p);

        char *buf = xmalloc(RECV_BUF_SIZE + 1);
        struct request req = {
                .sockfd = sockfd,
                .recv_buf = buf,
                .recv_buf_end = buf + RECV_BUF_SIZE,
                .parse_start = buf,
                .parse_end = buf,
                .headers_num = 0,
        };

        int ret = parse_request(&req);
        if (ret != -1) {}

        for (int i=0; i < req.headers_num; i++)
                free(req.headers[i].value);

        syslog(LOG_INFO, "closed connection");
        close_serving_thread(sockfd);
        return NULL;
}

/*
 * make a new thread to serve this sockfd (not implemented)
 */
static void mkthread_serve(int sockfd)
{
        if (sockfd >= MAXFD) {
                syslog(LOG_CRIT, "cannot handle large file descriptor");
                close(sockfd);
                return;
        }
        thread_sockfd[sockfd] = sockfd;
        dedicated_serve(&thread_sockfd[sockfd]);
}

/*
 * initialize the socket (listen to it), and accept connections
 */
int serve(int port)
{
        int sockfd = init_sock(port);
        if (sockfd == -1) {
                perror("Error making socket");
                return -1;
        }

        struct sockaddr_in addr;
        socklen_t sin_size = sizeof(struct sockaddr_in);

        syslog(LOG_INFO, "starting accept client loop");
        while (1) {
                int spawned_sockfd = accept(sockfd, (struct sockaddr *)&addr, &sin_size);
                if (spawned_sockfd == -1) {
                        perror("Error accept client");
                        continue;
                }

                syslog(LOG_INFO, "received connection");
                mkthread_serve(spawned_sockfd);
        }
}
