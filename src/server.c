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

int main(int argc, char **argv)
{
        if (argc < 2) {
                puts("Error: port number not given");
                usage_exit();
        }
        openlog(argv[0], LOG_CONS | LOG_PID, LOG_LOCAL1);
        serve(argv[1]);
        closelog();

        return 0;
}
