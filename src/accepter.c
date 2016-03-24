#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "stdio.h"
#include "syslog.h"
#include "serve_request.h"
#include <pthread.h>

#define MAXFD 4096

/* the array to store socket fd for threads */
static int thread_sockfd[MAXFD];

/*
 * initialize socket
 */
static int init_sock(char *port)
{
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;
        getaddrinfo(NULL, port, &hints, &res);

        int sockfd = socket(res->ai_family, SOCK_STREAM, 0);

        /* make the port reusable immediately after program killed */
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

        int ret = bind(sockfd, res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);
        if (ret == -1 || listen(sockfd, 0) == -1)
                return -1;
        else
                return sockfd;
}
/*
 * make a new thread to serve this sockfd (not implemented)
 */

  

static void mkthread_serve(int sockfd)
{
        if (sockfd >= MAXFD) {
                syslog(LOG_CRIT, "cannot handle large file descriptor");
                close(sockfd);
                return;
        }
        thread_sockfd[sockfd] = sockfd;
        pthread_t thread_id;
        pthread_create(&thread_id ,NULL, serve_request, &thread_sockfd[sockfd]);
        pthread_detach(thread_id);

}

/*
 * initialize the socket (listen to it), and accept connections
 */
int serve(char *port)
{
        int sockfd = init_sock(port);
        if (sockfd == -1) {
                perror("Error making socket");
                return -1;
        }

        struct sockaddr_in addr;
        socklen_t sin_size = sizeof(struct sockaddr_in);

        syslog(LOG_INFO, "starting accept client loop");
        while (1) {
                int spawned_sockfd = accept(sockfd, (struct sockaddr *)&addr, &sin_size);
                if (spawned_sockfd == -1) {
                        perror("Error accept client");
                        continue;
                }

                syslog(LOG_INFO, "received connection");
                mkthread_serve(spawned_sockfd);
        }
}

void close_serving_thread(int sockfd)
  {
          close(sockfd);
          pthread_exit(NULL);
  }
