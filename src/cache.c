#define _GNU_SOURCE

#include <string.h>
#include <crypt.h>
#include <stdlib.h>
#include "xmalloc.h"
#include "parser_state.h"
#include "http_parser.h"

#define FILENAME_LEN 23

/* borrowed from stackoverflow, by David Heffernan */
char* concat(char *s1, char *s2)
{
        size_t len1 = strlen(s1);
        size_t len2 = strlen(s2);
        char *result = xmalloc(len1+len2+1);//+1 for the zero-terminator
        memcpy(result, s1, len1);
        memcpy(result+len1, s2, len2+1);//+1 to copy the null-terminator
        return result;
}

/*
 * generate a hash to filename, which has length 23
 */
void mk_filename(struct parser *req, char *filename)
{
        char *hostname = header_to_value(req, "Host");
        char *full_url = concat(hostname, req->extra.req_line.url);
#ifdef _DEBUG
        syslog(LOG_DEBUG, "going to hash %s", full_url);
#endif
        struct crypt_data data = { .initialized = 0 };
        char *c = crypt_r(full_url, "$1$00$", &data);  /* md5 */
        free(full_url);
        strncpy(filename, c+6, FILENAME_LEN);  /* the first 6 bytes are not secret */
        for (char *p = filename; *p != '\0'; p++) {
                if (*p == '/')
                        *p = '_';
        }
}
