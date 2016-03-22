CC = gcc
CFLAGS = -std=gnu99 -Wall -I.
CC_CMD = $(CC) $(CFLAGS) -o $@ -c $<

#UNAME := $(shell uname)
#LDFLAGS = -lpthread
#ifeq ($(UNAME), SunOS)
#LDFLAGS += -lsocket -lnsl
#endif

%.o: src/%.c
	$(CC_CMD)

myproxy: server.o accepter.o xmalloc.o http_parser.o serve_request.o readwrite.o cache.o
	$(CC) $(CFLAGS) -lcrypt -o $@ $^

serve_request.o: http_parser.o readwrite.o cache.o

accepter.o: serve_request.o

debug: CFLAGS += -D_DEBUG -g
debug: myproxy

.PHONY: clean
clean:
	rm *.o myproxy
