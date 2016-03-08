#define MAX_HEADER 10  /* We will only store important headers */
enum method { GET, OTHER };

struct headers_fields {
        /* value points to parsed token, which is malloced */
        /* if not exist, they it will be NULL */
        char field_name[32];
        char *value;
};

struct request {
        int sockfd;
        char *recv_buf;     /* buffer to hold raw request */
        char *recv_buf_end; /* upper limit of the buffer (exclusive) */
        char *parse_start;  /* where next parsing start */
        char *parse_end;    /* only data upto this (exclusive) is meaningful */
        enum method method;
        struct headers_fields headers[MAX_HEADER];
        int headers_num;
};
