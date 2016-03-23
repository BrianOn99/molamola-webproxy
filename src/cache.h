#ifndef CACHE_H
#define CACHE_H

typedef struct {
        int use_cache;  /* boolean */
        int fd;  /* file to write or read */
} cache_t;

void mk_cache(cache_t *cache, struct parser *req);
int transfer_c(cache_t *cache, struct parser *req, struct parser *reply, size_t len);
int swrite_c(cache_t *cache, struct parser *req, void *buf, size_t len);

#endif
