#include <sys/types.h>
#define DATA_BUFSIZE 65536
int sread(int sockfd, void *buf, unsigned int len);
int swrite(int sockfd, void *buf, unsigned int len);
int transfer_file_copy(int out_fd, int in_fd, off_t num_send, void (*progress)(double percent));
