#define MAX_HEADER 50  /* We will only store important headers */
enum method { GET, OTHER };
struct req_line { enum method method; char *url; };
//union extra { struct req_line; int status_code; };

struct headers_fields {
        /* value points to parsed token, which is malloced */
        char field_name[32];
        char *value;
};

struct parser {
        int sockfd;
        char *recv_buf;     /* buffer to hold raw http message */
        char *recv_buf_end; /* upper limit of the buffer (exclusive) */
        char *parse_start;  /* where next parsing start */
        char *parse_end;    /* only data upto this (exclusive) is meaningful */
        int headers_num;
        struct headers_fields headers[MAX_HEADER];
        union { struct req_line req_line; int status_code; } extra;
};
