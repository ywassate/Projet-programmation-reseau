CC = gcc
CFLAGS = -Wall -g -O0
# Fichiers sources
SRCS = $(wildcard *.c)
# Fichiers exécutables
EXECS = client server

all: $(EXECS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(EXECS)