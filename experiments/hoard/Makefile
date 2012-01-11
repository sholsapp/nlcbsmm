#  CPE 550 - NLCBSMM
#  #  -------------------
#
#  #
#  Last Modified: Sat Jan 10 14:01:43 PST 2009
#  #  @author Stephen Holsapple
CC=gcc
CPP=g++
SUS3=-D_POSIX_SOURCE -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600
HARDEN=
CFLAGS=-g
LDFLAGS=-Wl,--no-as-needed -lrt -L. -lhoard

#ALL=hoard_test bench1-hoard bench1-malloc network_client
ALL=hoard_test 

all:	$(ALL)

hoard_test: hoard_test.o
	$(CPP) $(LDFLAGS) -o $@ $^

bench1-hoard: bench1.o
	$(CPP) $(LDFLAGS) -o $@ $^

bench1-malloc: bench1.o
	$(CPP) -O0 -o $@ $^

hoard_test.o: hoard_test.cpp
	$(CPP) $(CFLAGS) -c $<

bench1.o: bench1.cpp
	$(CPP) $(CFLAGS) -c $<

clean:
	rm -rf core* *.o *.gch $(ALL)
