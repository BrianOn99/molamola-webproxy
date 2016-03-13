#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include "parser_state.h"
#include "http_parser.h"
#include "readwrite.h"

static void close_serving_thread(struct parser *req)
{
        parser_free(req);
        free(req);
        close(req->sockfd);
}

static int forward_request(struct parser *req)
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
        for (rp = result; ; rp = rp->ai_next) {
                if (rp == NULL)  /* none of the ip(s) can be connected */
                        return -1;
                sfd = socket(rp->ai_family, SOCK_STREAM, 0);
                if (sfd != -1) {
                        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
                                break;
                        close(sfd);
                }
        }

        freeaddrinfo(result);
        syslog(LOG_INFO, "openned connection to remote http");

        /* start forwarding the http request */
        struct parser *reply = new_parser(sfd);
        swrite(sfd, req->recv_buf, req->parse_start - req->recv_buf);
        if (parse_response(reply) == 0) {
                int header_len = reply->parse_start - reply->recv_buf;
                char *content_len_str = header_to_value(reply, "Content-Length");
                int content_len = 0;
                if (content_len_str) {
                        content_len = strtol(content_len_str, 0, 10);
                }

                /* write all things we have received yet */
                int loaded_len = reply->parse_end - reply->recv_buf;
                swrite(req->sockfd, reply->recv_buf, loaded_len);
                /* write the remainings */
                transfer_file_copy(req->sockfd, sfd, content_len + header_len - loaded_len);
        }

        parser_free(reply);
        free(reply);
        syslog(LOG_INFO, "closed connection to remote http");
        return 0;
}

/*
 * the thread subroutine to serve 1 client
 */
void *serve_request(void *p)
{
        int sockfd = *((int*)p);

        struct parser *req = new_parser(sockfd);

        if (parse_request(req) == 0)
                forward_request(req);

        syslog(LOG_INFO, "closed connection");
        close_serving_thread(req);
        return NULL;
}
