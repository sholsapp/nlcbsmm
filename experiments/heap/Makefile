#  CPE 550 - NLCBSMM
#  #  -------------------
#
#  #
#  Last Modified: Sat Jan 10 14:01:43 PST 2009
#  #  @author Stephen Holsapple
CC=gcc
CPP=g++
SUS3=-D_POSIX_SOURCE -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600
HARDEN=-D_FORTIFY_SOURCE
CFLAGS=-g -pedantic $(SUS3) $(HARDEN)
LDFLAGS=-L.

ALL=heap

all:	$(ALL)

heap: heap.o
	$(CPP) $(LDFLAGS) -o $@ $^

heap.o: heap.cpp
	$(CPP) $(CFLAGS) -c $<

clean:
	rm -rf core* *.o *.gch $(ALL)
