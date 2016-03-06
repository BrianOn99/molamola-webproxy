#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "syslog.h"
#include "accepter.h"

void usage_exit()
{
        puts("Usage: ./myproxy port");
        exit(1);
}

/* parse the port from string s*/
int getport(char *s)
{
        char *endptr;
        int port = strtol(s, &endptr, 10);
        if (*endptr != '\0' || port <= 0 || port > 65535)
                return -1;
        return port;
}

int main(int argc, char **argv)
{
        int port;
        if (argc == 1 || (port = getport(argv[1])) == -1) {
                puts("Error: Invalid port number");
                usage_exit();
        }
        openlog(argv[0], LOG_CONS | LOG_PID, LOG_LOCAL1);
        serve(port);
        closelog();

        return 0;
}
