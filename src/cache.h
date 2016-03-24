#ifndef CACHE_H
#define CACHE_H

enum cache_type { CACHE_NOTHING, CACHE_WRITE, CACHE_READ, RESPONSE304, CACHE_CONDITIONAL_304_200 };
typedef struct {
        enum cache_type type;
        int fd;  /* file to write or read */
        char filename[23];
} cache_t;

void mk_cache(cache_t *cache, struct parser *req);
int transfer_c(cache_t *cache, struct parser *req, struct parser *reply, size_t len);
int swrite_c(cache_t *cache, struct parser *req, struct parser *reply, size_t len);
void mk_nothing_cache(cache_t *cache);
void mk_write_cache(cache_t *cache);
void mk_read_cache(cache_t *cache);

#endif
