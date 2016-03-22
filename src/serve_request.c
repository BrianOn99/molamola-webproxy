#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/socket.h>
#include "parser_state.h"
#include "http_parser.h"
#include "readwrite.h"
#include "cache.h"

static void close_serving_thread(struct parser *req, struct parser *reply)
{
        parser_free(req);
        free(req);
        parser_free(reply);
        free(reply);

        close(req->sockfd);
        if (reply->sockfd != -1)
                close(reply->sockfd);
        exit(0);
}

/*
 * open a new connection to http server, by the information given in header
 * fields of req.  Store the resultant parser in *reply
 */
static int connect_remote_http(struct parser *req, struct parser *reply)
{
        char *hostname = strdup(header_to_value(req, "Host"));
        if (!hostname)  /* the Host field is not found */
                return -1;

        char *p;
        if ((p = strchr(hostname, ':'))) {  /* specific port given */
                *p = '\0';
                p++;  /* make p points to port number */
        } else {
                p = "80";
        }

        struct addrinfo hints;
        struct addrinfo *result, *rp;
        memset(&hints, 0, sizeof(hints));
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_family = AF_UNSPEC;

        if (getaddrinfo(hostname, p, &hints, &result) != 0) {
                syslog(LOG_INFO, "cannot convert %s to ip", hostname);
                return -1;
        }
        free(hostname);

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

        reply->sockfd = sfd;
        return 0;
}

static int get_chunk_size(char const *str, unsigned int *size, unsigned int *consumed)
{
        if (EOF == sscanf(str, "%x%n\r", size, consumed) ||
            str[*consumed] != '\r') {
                //write(0, str, 4);
                //printf("\nchunk: %d, consume: %d\n", *size, *consumed);
                fprintf(stderr, "cannot find chunk size\n");
                return -1;
        }
        return 0;
}

static int last_chunk_remain_size(struct parser *reply)
{
	unsigned int size, consumed;
	while (1) {
                if (get_chunk_size(reply->parse_start, &size, &consumed) == -1)
                        return -1;
		int loaded_remain = reply->parse_end - reply->parse_start;
                int bytes_to_next_chunk = (consumed+2) + size;
		if (bytes_to_next_chunk >= loaded_remain) {
			return bytes_to_next_chunk - loaded_remain;
		} else {
			reply->parse_start += bytes_to_next_chunk;
		}
	}
}

/*
 * Assume that a request is stored in req.  Forward it to remote http, and send
 * that back to client.
 */
static int forward_request(struct parser *req, struct parser *reply)
{
        swrite(reply->sockfd, req->recv_buf, req->parse_start - req->recv_buf);
        if (parse_response(reply) == 0) {
                int header_len = reply->parse_start - reply->recv_buf;
                char *content_len_str = header_to_value(reply, "Content-Length");
		char *transfer_encoding_str = header_to_value(reply, "Transfer-Encoding");

                char filename[23];
                mk_filename(req, filename);
		/* write all things we have received yet */
		int loaded_len = reply->parse_end - reply->recv_buf;
		swrite(req->sockfd, reply->recv_buf, loaded_len);

                if (content_len_str) {
                        int content_len = strtol(content_len_str, 0, 10);
			/* write the remainings */
			int ret = transfer_file_copy(req->sockfd, reply->sockfd, content_len + header_len - loaded_len);
                        if (ret == -1)
                                return -1;
                } else if (transfer_encoding_str) {
                        if (reply->parse_end > reply->parse_start) {
                                int last = last_chunk_remain_size(reply);
                                if (last == -1)
                                        return -1;
                                transfer_file_copy(req->sockfd, reply->sockfd, last+2);
                        }

                        char chunklen[8];
                        unsigned int size, consumed, this_transfer;
                        while (1) {
                                memset(chunklen, '\0', sizeof(chunklen)/sizeof(char));
                                int ret = recv(reply->sockfd, chunklen, sizeof(chunklen)/sizeof(char)-1, MSG_PEEK);
                                if (ret == 0 || ret == -1)
                                        return -1;
                                if (get_chunk_size(chunklen, &size, &consumed) == -1)
                                        return -1;

                                this_transfer = size ?
                                                (consumed+2) + (size+2) :  /* "hex-len CRLF chunk CRLF" */
                                                consumed + 4;  /* "end-chunk CRLF CRLF" */
                                if (transfer_file_copy(req->sockfd, reply->sockfd, this_transfer) == -1)
                                        return -1;
                                if (size == 0) {
                                        /* TODO: send remaining fields */
                                        break;
                                }
                        }
		} else {
			fprintf(stderr, "cannot find message length from http response\n");
			return -1;
		}
        }

        return 0;
}

/*
 * the thread subroutine to serve 1 client
 */
void *serve_request(void *p)
{
        int sockfd = *((int*)p);
        parser_init_global();

        struct parser *req = new_parser(sockfd);
        struct parser *reply = new_parser(-1);

        /* serve for one http request, establish connection to remote host */
        if ((parse_request(req) == -1) ||
            (connect_remote_http(req, reply) == -1) ||
            (forward_request(req, reply)) == -1) {
                close_serving_thread(req, reply);
        }

        /* assume http keep-alive, repeatedly serve for more http requests */
        do {
                parser_reset(req);
                parser_reset(reply);
        } while (parse_request(req) == 0 && forward_request(req, reply) == 0);

        syslog(LOG_INFO, "closed connection");
        close_serving_thread(req, reply);
        return NULL;
}
