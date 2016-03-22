#ifndef CACHE_H
#define CACHE_H

typedef int cache_t;

cache_t mk_cache(struct parser *req);
int transfer_c(cache_t cache_fd, struct parser *req, struct parser *reply, int len);
int swrite_c(cache_t cache, int fd, void *buf, unsigned int len);

#endif
