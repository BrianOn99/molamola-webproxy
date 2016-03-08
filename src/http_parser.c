#define _GNU_SOURCE

#include <regex.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "syslog.h"
#include "request_state.h"

/*
 * find where this http line end
 */
static char *line_end(struct request *req)
{
        for (char *p=req->parse_start; p < req->parse_end; p++) {
                if (*p == '\r' && *(p+1) == '\n')
                        return p;
        }
        return NULL;
}

/*
 * consume 1 line from req->parse_start, and store the HTTP method in request
 */
static int parse_request_line(struct request *req)
{
        char *str = req->parse_start;
        static regex_t regex;
        /* store the match. [0] is the whole string. [1] is GET|POST|... */
        static regmatch_t match[2];
        regcomp(&regex, "^(GET|POST|PUT|DELETE) [^ \r\n]+ HTTP/[0-9]\\.[0-9]\r\n", REG_EXTENDED);
        int ret = regexec(&regex, str, 2, match, 0);
        if (ret)
                return -1;

        regmatch_t met = match[1];
        req->method = (strcmp("GET", str + met.rm_so) == 0) ? GET : OTHER;
        req->parse_start += match[0].rm_eo;

        regfree(&regex);
        return 0;
}

static char *dup_second_match(char *orig, regmatch_t match[])
{
        return strndup(orig+match[2].rm_so, (match[2].rm_eo - match[2].rm_so));
}

/*
 * consume 1 line from req->parse_start, and store the header field if necessary
 */
static int parse_header_line(struct request *req)
{
        char *str = req->parse_start;
        static regex_t regex;
        /* store the match. [0] is the whole string.
         * [1] is Host|User-agent... [2] is the corresponding value [3] is
         * useless*/
        static regmatch_t match[4];
        regcomp(&regex, "^([a-zA-Z_-]+): (([^\r]|\r[^\n])+)\r\n", REG_EXTENDED);
        int ret = regexec(&regex, str, 2, match, 0);
        if (ret)
                return -1;

        if (strncmp("If-Modified-Since", str + match[1].rm_so, 17) == 0 ||
            strncmp("Host", str + match[1].rm_so, 4) == 0 ||
            strncmp("Cache-Control", str + match[1].rm_so, 13) == 0) {
                int i = req->headers_num;
                if (i >= MAX_HEADER)  /* this should not happen, but try to be secure */
                        return -1;
                int len = match[1].rm_eo-match[1].rm_so;
                syslog(LOG_INFO, "got %s", strndupa(str, match[1].rm_eo-match[1].rm_so));
                memcpy(req->headers[i].field_name, (str + match[1].rm_so), len);
                req->headers[i].value = dup_second_match(str, match);
                req->headers_num++;
        }

        req->parse_start += match[0].rm_eo;  /* update where next parse should start */

        regfree(&regex);
        return 0;
}

/*
 * try to receive request message, upto but not neccesary max_len
 */
static int try_read(struct request *req, unsigned int max_len)
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
        syslog(LOG_DEBUG, "something read: %s", req->parse_end - ret);
#endif
        return ret;
}

/*
 * parse the request line and headers
 * return -1 if the request is too long or non-standard
 */
int parse_request(struct request *req)
{
        int ret = try_read(req, (req->recv_buf_end - req->parse_end));
        if (ret == 0 || ret == -1) {  /* 0 means client closed */
                syslog(LOG_INFO, "early close");
                return -1;
        }

        char *p = line_end(req);
        if (!p) {
                syslog(LOG_CRIT, "The request line is too long to handle");
                return -1;
        }

        if (parse_request_line(req) == -1) {
                syslog(LOG_CRIT, "The request line is bad");
                return -1;
        }

        /* start parsing headers */
        while (1) {
                if (req->parse_start >= req->parse_end) {
                        /* TODO: refactor this code */
                        ret = try_read(req, (req->recv_buf_end - req->parse_end));
                        if (ret == 0 || ret == -1) {  /* 0 means client closed */
                                syslog(LOG_INFO, "early close 2");
                                return -1;
                        }
                }

                char *p = line_end(req);
                if (!p) {
                        syslog(LOG_CRIT, "The request headers are too long to handle");
                        return -1;
                }
                if (p == req->parse_start) /* meeting 2 CRLF: end of headers */
                        break;

                if (parse_header_line(req) == -1) {
                        syslog(LOG_CRIT, "The header line is bad");
                        return -1;
                }
        }
        return 0;
}
