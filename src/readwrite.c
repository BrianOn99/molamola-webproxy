/*
 * Read and write to socket safely.
 * Lots of the code is consulted from vsftp and yafc
 */

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sendfile.h>
#include "readwrite.h"

/*
 * guarantee len bytes read from fd
 */
int swrite(int fd, void *buf, unsigned int len)
{
        ssize_t ret;
        while (len > 0 && (ret = write(fd, buf, len)) != 0) {
#ifdef _DEBUG
                printf("somthing written with swrite, size %d\n", ret);
#endif
                /* if other side closed connection, -1 will be recieved
                 * with errno EPIPE */
                if (ret == -1) {
                        if (errno == EINTR)
                                continue;
                        perror("Error writing fd");
                        return -1;
                }
                buf += ret;
                len -= ret;
        }
        return 0;
}

/*
 * guarantee len bytes written to fd
 */
int sread(int fd, void *buf, unsigned int len)
{
        ssize_t ret;
        while (len > 0) {
                ret = read(fd, buf, len);
#ifdef _DEBUG
                printf("somthing read with sread, size %d\n", ret);
#endif
                if (ret == -1) {
                        if (errno == EINTR)
                                continue;
                        perror("Error reading fd");
                        return -1;
                } else if (ret == 0) {
                        /* the other side closed connection */
                        return -1;
                }
                buf += ret;
                len -= ret;
        }
        return 0;
}

/*
 * copy file descriptor content
 */
int transfer_file_copy(int out_fd, int in_fd, off_t size_remain)
{
        void *buf = malloc(DATA_BUFSIZE);
        if (!buf) {
                perror("malloc error");
                exit(1);
        }
        while (size_remain > 0) {
                unsigned int send_this_time;
                if (size_remain > DATA_BUFSIZE) {
                        send_this_time = DATA_BUFSIZE;
                } else {
                        send_this_time = (unsigned int) size_remain;
                }
                if (sread(in_fd, buf, send_this_time) == -1)
                        return -1;
                if (swrite(out_fd, buf, send_this_time) == -1)
                        return -1;
                size_remain -= send_this_time;
        }
        return 0;
}

/*
 * Same as above except copy to 2 fd
 * code duplication is ugly.  I have think hard in design pattern to refactor.
 * closure callback is an obvious solution, which we don't have in C.  The
 * adaptor pattern is ok, but might need ugly casting to mock interface.
 * So I give up
 */
int transfer_file_copy_dual(int out_fd1, int out_fd2, int in_fd, off_t size_remain)
{
        void *buf = malloc(DATA_BUFSIZE);
        if (!buf) {
                perror("malloc error");
                exit(1);
        }
        while (size_remain > 0) {
                unsigned int send_this_time;
                if (size_remain > DATA_BUFSIZE) {
                        send_this_time = DATA_BUFSIZE;
                } else {
                        send_this_time = (unsigned int) size_remain;
                }
                if (sread(in_fd, buf, send_this_time) == -1)
                        return -1;
                if (swrite(out_fd1, buf, send_this_time) == -1)
                        return -1;
                if (swrite(out_fd2, buf, send_this_time) == -1)
                        return -1;
                size_remain -= send_this_time;
        }
        return 0;
}
