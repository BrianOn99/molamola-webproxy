#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include "stdio.h"
#include "syslog.h"
#include "xmalloc.h"

#define MAXFD 4096
#define RECV_BUF_SIZE 4096

static int thread_sockfd[MAXFD];

struct request {
        int sockfd;
        char *recv_buf;     /* buffer to hold raw request */
        char *recv_buf_end; /* upper limit of the buffer (exclusive) */
        char *parse_start;  /* where next parsing start */
        char *parse_end;    /* only data upto this (exclusive) is meaningful */
};

void close_serving_thread(int sockfd)
{
        close(sockfd);
}

int init_sock(long port)
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
 * try to receive request message, upto but not neccesary max_len
 */
int try_read(struct request *req, unsigned int max_len)
{
        int ret;
        if (req->parse_end + max_len > req->recv_buf_end) {
                fprintf(stderr, "In try_read: buffer overflow\n");
                return -1;
        }
        while ((ret = read(req->sockfd, req->parse_end, max_len)) == -1) {
                if (errno == EINTR)
                        continue;
                perror("Error reading");
                return -1;
        }
        req->parse_end += ret;
        return ret;
}

void parse_request(struct request *req)
{
        printf("%p %p\n", req->recv_buf, req->parse_end);
        fwrite(req->recv_buf, req->parse_end - req->recv_buf, 1, stdout);
}

void *dedicated_serve(void *p)
{
        int sockfd = *((int*)p);

        char *buf = xmalloc(RECV_BUF_SIZE);
        struct request req = {
                .sockfd = sockfd,
                .recv_buf = buf,
                .recv_buf_end = buf + RECV_BUF_SIZE,
                .parse_start = buf,
                .parse_end = buf,
        };

        int ret = try_read(&req, RECV_BUF_SIZE);
        if (ret == 0 || ret == -1) {
                syslog(LOG_INFO, "early close");
                close_serving_thread(sockfd);
        }

        parse_request(&req);

        syslog(LOG_INFO, "closed connection");
        close_serving_thread(sockfd);
        return NULL;
}

/* make a new thread to serve this sockfd (not implemented) */
void mkthread_serve(int sockfd)
{
        if (sockfd >= MAXFD) {
                syslog(LOG_CRIT, "cannot handle large file descriptor");
                close(sockfd);
                return;
        }
        thread_sockfd[sockfd] = sockfd;
        dedicated_serve(&thread_sockfd[sockfd]);
}

/* initialize the socket (listen to it), and accept connections */
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
