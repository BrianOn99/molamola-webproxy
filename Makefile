CC = gcc
CFLAGS = -std=gnu99 -Wall -I. -Og
CC_CMD = $(CC) $(CFLAGS) -o $@ -c $<

#UNAME := $(shell uname)
#LDFLAGS = -lpthread
#ifeq ($(UNAME), SunOS)
#LDFLAGS += -lsocket -lnsl
#endif

%.o: src/%.c
	$(CC_CMD)

myproxy: server.o accepter.o xmalloc.o http_parser.o
	$(CC) $(CFLAGS) -o $@ $^

accepter.o: http_parser.o

debug: CFLAGS += -D_DEBUG -g
debug: myproxy

.PHONY: clean
clean:
	rm *.o myproxy
