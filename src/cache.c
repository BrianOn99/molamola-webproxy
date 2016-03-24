#define _GNU_SOURCE

#include <string.h>
#include <crypt.h>
#include <stdlib.h>
#include <fcntl.h>
#include <syslog.h>
#include "xmalloc.h"
#include "parser_state.h"
#include "http_parser.h"
#include "readwrite.h"
#include "cache.h"

#define FILENAME_LEN 23

/* borrowed from stackoverflow, by David Heffernan */
static char* concat(char **s1, char **s2)
{
        size_t len1 = ptr_strlen(s1);
        size_t len2 = ptr_strlen(s2);
        char *result = xmalloc(len1+len2+1);//+1 for the zero-terminator
        memcpy(result, s1[0], len1);
        memcpy(result+len1, s2[0], len2);
        result[len1+len2+1] = '\0';
        return result;
}

/*
 * generate a hash to filename, which has length 23
 */
static void mk_filename(struct parser *req, char *filename)
{
        char **hostname = header_to_value(req, "Host");
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

/*
 * return a file descriptor for caching
 */
void mk_cache(cache_t *cache, struct parser *req)
{
        cache->use_cache = 0;
        char filename[23];
        mk_filename(req, filename);
        cache->fd = creat(filename, S_IWUSR | S_IRUSR);
}

int transfer_c(cache_t *cache, struct parser *req, struct parser *reply, size_t len)
{
        return transfer_file_copy_dual(cache->fd, req->sockfd, reply->sockfd, len);
}

int swrite_c(cache_t *cache, struct parser *req, void *buf, size_t len)
{
        return swrite(cache->fd, buf, len) ||
               swrite(req->sockfd, buf, len);
}
