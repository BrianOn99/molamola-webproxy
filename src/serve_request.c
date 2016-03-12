#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include "xmalloc.h"
#include "request_state.h"
#include "http_parser.h"
#include "readwrite.h"

#define RECV_BUF_SIZE 8192

static void close_serving_thread(int sockfd)
{
        close(sockfd);
}

static int forward_request(struct request *req)
{
        char *hostname = header_to_value(req, "Host");
        if (!hostname)  /* the Host field is not found */
                return -1;

        struct addrinfo hints;
        struct addrinfo *result, *rp;
        memset(&hints, 0, sizeof(hints));
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_family = AF_UNSPEC;

        if (getaddrinfo(hostname, "80", &hints, &result) != 0) {
                syslog(LOG_INFO, "cannot convert %s to ip", hostname);
                return -1;
        }

        int sfd = -1;
        for (rp = result; rp != NULL; rp = rp->ai_next) {
                sfd = socket(rp->ai_family, SOCK_STREAM, 0);
                if (sfd == -1)
                        continue;
                if (connect(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
                        break;

                close(sfd);
        }
        if (sfd == -1)
                return -1;

        freeaddrinfo(result);

        swrite(sfd, req->recv_buf, req->parse_start - req->recv_buf);
        sread(sfd, req->recv_buf, 64);
        fwrite(req->recv_buf, 64, 1, stdout);
        puts("");
        return 0;
}

/*
 * the thread subroutine to serve 1 client
 */
void *serve_request(void *p)
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

        if (parse_request(&req) == 0)
                forward_request(&req);

        /* cleanup */
        for (int i=0; i < req.headers_num; i++)
                free(req.headers[i].value);

        syslog(LOG_INFO, "closed connection");
        close_serving_thread(sockfd);
        return NULL;
}