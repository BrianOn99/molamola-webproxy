#ifndef CACHE_H
#define CACHE_H

enum cache_type { CACHE_NOTHING, CACHE_WRITE, CACHE_READ };
typedef struct {
        enum cache_type type;
        int fd;  /* file to write or read */
} cache_t;

void mk_cache(cache_t *cache, struct parser *req);
int transfer_c(cache_t *cache, struct parser *req, struct parser *reply, size_t len);
int swrite_c(cache_t *cache, struct parser *req, void *buf, size_t len);

#endif
