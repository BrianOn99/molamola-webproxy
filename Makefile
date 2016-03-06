CC = gcc
CFLAGS = -std=gnu99 -Wall -I. -O1
CC_CMD = $(CC) $(CFLAGS) -o $@ -c $<

#UNAME := $(shell uname)
#LDFLAGS = -lpthread
#ifeq ($(UNAME), SunOS)
#LDFLAGS += -lsocket -lnsl
#endif

%.o: src/%.c
	$(CC_CMD)

myproxy: server.o accepter.o
	$(CC) $(CFLAGS) -o $@ $^

debug: CFLAGS += -D_DEBUG -g
debug: default

.PHONY: clean
clean:
	rm *.o myproxy
