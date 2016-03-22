#include <sys/types.h>
#define DATA_BUFSIZE 4096
int sread(int sockfd, void *buf, unsigned int len);
int swrite(int sockfd, void *buf, unsigned int len);
int transfer_file_copy(int out_fd, int in_fd, off_t num_send);
int transfer_file_copy_dual(int out_fd1, int out_fd2, int in_fd, off_t num_send);
