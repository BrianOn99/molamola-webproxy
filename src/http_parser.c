#define _GNU_SOURCE

#include <regex.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "xmalloc.h"
#include "syslog.h"
#include "parser_state.h"

#define RECV_BUF_SIZE 8192

enum parse_state { REQUEST_LINE, HEADER_LINES };
static regex_t regex_status_line, regex_request_line, regex_header_line;

void parser_init_global()
{
        regcomp(&regex_request_line, "^(GET|POST|PUT|DELETE) ([^ \r\n]+) HTTP/[0-9]\\.[0-9]\r\n", REG_EXTENDED);
        regcomp(&regex_status_line, "^HTTP/[0-9]\\.[0-9] ([0-9]{3}) ([^\r]|\r[^\n])+\r\n", REG_EXTENDED);
        regcomp(&regex_header_line, "^([a-zA-Z_-]+): (([^\r]|\r[^\n])+)\r\n", REG_EXTENDED);
}

void parser_reset(struct parser *p)
{
        p->recv_buf_end = p->recv_buf + RECV_BUF_SIZE;
        p->parse_start = p->parse_end = p->recv_buf;
        p->headers_num = 0;
}

struct parser *new_parser(int sockfd)
{
        char *buf = xmalloc(RECV_BUF_SIZE + 1);
        struct parser *p = xmalloc(sizeof(struct parser));
        p->sockfd = sockfd;
        p->recv_buf = buf;
        parser_reset(p);
        return p;
}

void parser_free(struct parser *req)
{
        free(req->recv_buf);
        for (int i=0; i < req->headers_num; i++)
                free(req->headers[i].value);
}

/*
 * find where this http line end
 */
static char *line_end(struct parser *req)
{
        for (char *p=req->parse_start; p < req->parse_end; p++) {
                if (*p == '\r' && *(p+1) == '\n')
                        return p;
        }
        return NULL;
}

/*
 * this is called after the headers are paresed
 * to get the value associated with a header field
 */
char *header_to_value(struct parser *req, char field_name[])
{
        for (int i=0; i < req->headers_num; i++) {
                if (strcmp(field_name, req->headers[i].field_name) == 0)
                        return req->headers[i].value;
        }
        return NULL;
}

static char *dup_second_match(char *orig, regmatch_t match[])
{
        return strndup(orig+match[2].rm_so, (match[2].rm_eo - match[2].rm_so));
}

/*
 * Only use this for http request message
 * consume 1 line from req->parse_start, and store the HTTP method in request
 */
static int parse_request_line(struct parser *req)
{
        char *str = req->parse_start;
        /* store the match. [0] is the whole string. [1] is GET|POST|...
         * [2] is e.g. /index.html */
        static regmatch_t match[3];
        int ret = regexec(&regex_request_line, str, 3, match, 0);
        if (ret == REG_NOMATCH)
                return -1;

        regmatch_t met = match[1];
        req->extra.req_line.method = (strcmp("GET", str + met.rm_so) == 0) ? GET : OTHER;
        req->extra.req_line.url = dup_second_match(str, match);
        req->parse_start += match[0].rm_eo;

        return 0;
}

/*
 * Only use this for http response message
 * consume 1 line from req->parse_start, and store the status code in request
 */
static int parse_status_line(struct parser *req)
{
        char *str = req->parse_start;
        /* store the match. [0] is the whole string. [1] is status code e.g.200 */
        static regmatch_t match[2];
        int ret = regexec(&regex_status_line, str, 2, match, 0);
        if (ret == REG_NOMATCH)
                return -1;

        regmatch_t met = match[1];
        char *end;
        int code = strtol(str+met.rm_so, &end, 10);
        if (end-(str+met.rm_so) != 3)
                return -1;

        req->extra.status_code = code;
        req->parse_start += match[0].rm_eo;

        return 0;
}

/*
 * consume 1 line from req->parse_start, and store the header field if necessary
 */
static int parse_header_line(struct parser *req)
{
        char *str = req->parse_start;
        /* store the match. [0] is the whole string.
         * [1] is Host|User-agent... [2] is the corresponding value [3] is
         * useless*/
        static regmatch_t match[4];

        int ret = regexec(&regex_header_line, str, 4, match, 0);
        if (ret == REG_NOMATCH)
                return -1;

        /* store this header */
        int i = req->headers_num;
        if (i >= MAX_HEADER)  /* this should not happen in most cases */
                return -1;
        int len = match[1].rm_eo-match[1].rm_so;
#ifdef _DEBUG
        syslog(LOG_INFO, "got %s", strndupa(str, match[1].rm_eo-match[1].rm_so));
#endif
        memcpy(req->headers[i].field_name, (str + match[1].rm_so), len);
        req->headers[i].field_name[len] = '\0';
        req->headers[i].value = dup_second_match(str, match);
        req->headers_num++;
        /* finish storing header */

        req->parse_start += match[0].rm_eo;  /* update where next parse should start */

        return 0;
}

/*
 * try to receive message, upto but not neccesary max_len
 */
static int try_read(struct parser *req, unsigned int max_len)
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
        *(req->parse_end) = '\0';
#ifdef _DEBUG
        syslog(LOG_DEBUG, "something read: %d", ret);
#endif
        return ret;
}

/*
 * parse the first line (by first_line_fn) and headers
 * return -1 if the message is too long or non-standard
 */
static int
parse_general_http(struct parser *req, int (*first_line_fn)(struct parser *p))
{
        enum parse_state p_state = REQUEST_LINE;
        while (1) {
                if (req->parse_start >= req->parse_end) {
                        if (req->recv_buf_end - req->parse_end <= 0) {
                                syslog(LOG_CRIT, "The headers are too long to handle");
                                return -1;
                        }
                        int ret = try_read(req, (req->recv_buf_end - req->parse_end));
                        if (ret == 0 || ret == -1) {  /* 0 means client closed */
                                syslog(LOG_INFO, "early close");
                                return -1;
                        }
                }

                char *p = line_end(req);
                if (!p) {
                        syslog(LOG_CRIT, "The headers are too long to handle");
                        return -1;
                }

                switch (p_state) {
                case REQUEST_LINE:
                        if (first_line_fn(req) == -1) {
                                syslog(LOG_CRIT, "The 1st line is bad");
                                return -1;
                        }
                        p_state = HEADER_LINES;
                        break;
                case HEADER_LINES:
                        if (p == req->parse_start) {  /* meeting 2 CRLF: end of headers */
                                req->parse_start += 2;
                                return 0;
                        }
                        if (parse_header_line(req) == -1) {
                                syslog(LOG_CRIT, "The header line is bad");
                                return -1;
                        }
                        break;
                }
        }
}

int parse_response(struct parser *reply)
{
        return parse_general_http(reply, parse_status_line);
}

int parse_request(struct parser *reply)
{
        return parse_general_http(reply, parse_request_line);
}
