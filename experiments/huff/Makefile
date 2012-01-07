#  CPE 357
#  -------------------
#  makefile for proj1
#
#  Last Modified: Sat Jan 10 14:01:43 PST 2009
#  @author Stephen Holsapple
CC=gcc
SUS3=-D_POSIX_SOURCE -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600
HARDEN=-D_FORTIFY_SOURCE
CFLAGS=-Wall -g -std=c99 -pedantic $(SUS3) $(HARDEN)
LDFLAGS=
LDFLAGS_HOARD=-L. -lhoard

ALL=huff-e-malloc huff-d-malloc huff-e-hoard huff-d-hoard

all:	$(ALL)

huff-e-malloc: huff-e.o
	$(CC) $(LDFLAGS) -o $@ $^

huff-d-malloc: huff-d.o
	$(CC) $(LDFLAGS) -o $@ $^

huff-e-hoard: huff-e.o
	$(CC) $(LDFLAGS_HOARD) -o $@ $^

huff-d-hoard: huff-d.o
	$(CC) $(LDFLAGS_HOARD) -o $@ $^


huff-e.o: huff-e.c huff.h
	$(CC) $(CFLAGS) -c $<

huff-d.o: huff-d.c huff.h
	$(CC) $(CFLAGS) -c $<

test:
	MALLOC_CHECK=1 valgrind --leak-check=full --show-reachable=yes ./huff-e tests/test.in

clean:
	rm -rf core* *.o *.gch $(ALL)
