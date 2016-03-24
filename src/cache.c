#define _GNU_SOURCE

#include <string.h>
#include <crypt.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <stdio.h>
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

int is_cachable_file_ext(char **name)
{
        static char * ext_list[] = { ".html", ".jpg", ".gif", ".txt", ".pdf" };
        char *ext;
        char *name_end = name[1];
        for (ext = name_end-1; ext > name[0]; ext--) {
                if (ext - name_end > 6) return 0;  /* not interested in long extension */
                if (*ext == '.') break;
        }

        for (int i=0; i < sizeof(ext_list)/sizeof(char*); i++) {
                if (strncmp(ext_list[i], ext, name_end-ext) == 0)
                        return 1;
        }

        return 0;  /* no one match */
}

/*
 * return a file descriptor for caching
 */
void mk_cache(cache_t *cache, struct parser *req)
{
        char filename[23];
        mk_filename(req, filename);

	if (access(filename, F_OK) == -1) {
                /* cache not exist */

                if (!is_cachable_file_ext(req->extra.req_line.url)) {
#ifdef _DEBUG
                        syslog(LOG_DEBUG, "the file extension is not cachable");
#endif
                        cache->type = CACHE_NOTHING;
                        return;
                }

#ifdef _DEBUG
                syslog(LOG_DEBUG, "the file extension is cachable");
#endif
                cache->type = CACHE_WRITE;
                cache->fd = creat(filename, S_IWUSR | S_IRUSR);
        } else {
                /* cache exist */
                char **ims = header_to_value(req, "If-Modified-Since");
                char **cache_control = header_to_value(req, "Cache-Control");

                if (!ims && !cache_control) {
                        cache->type = CACHE_READ;
                        cache->fd = open(filename, O_RDONLY);
                } else if (ims && !cache_control) {
                        cache->type = RESPONSE304;
                        cache->fd = -1;
                } else if (!ims && cache_control) {
                        cache->type = CACHE_WRITE;
                        cache->fd = creat(filename, S_IWUSR | S_IRUSR);
                } else if (ims && cache_control) {
                        cache->type = CACHE_WRITE;
                        cache->fd = creat(filename, S_IWUSR | S_IRUSR);
                }
        }
}

int transfer_c(cache_t *cache, struct parser *req, struct parser *reply, size_t len)
{
        if (cache->type == CACHE_WRITE) {
                return transfer_file_copy_dual(cache->fd, req->sockfd, reply->sockfd, len);
        } else if (cache->type == CACHE_NOTHING) {
                return transfer_file_copy(req->sockfd, reply->sockfd, len);
        } else {
                fprintf(stderr, "transfer_c: should not handle this cache here");
                return -1;
        }
}

int swrite_c(cache_t *cache, struct parser *req, struct parser *reply, size_t len)
{
        char *buf = reply->recv_buf;
        if (cache->type == CACHE_WRITE) {
                return swrite(cache->fd, buf, len) ||
                       swrite(req->sockfd, buf, len);
        } else if (cache->type == CACHE_NOTHING) {
                return swrite(req->sockfd, buf, len);
        } else {
                fprintf(stderr, "swrite_c: should not handle this cache here");
                return -1;
        }
}
