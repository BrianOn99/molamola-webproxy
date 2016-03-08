#include <stdlib.h>
#include <stdio.h>

void *xmalloc(size_t sz)
{
        void *p = malloc(sz);
        if (!p) {
                perror("malloc");
                exit(1);
        }
        return p;
}
