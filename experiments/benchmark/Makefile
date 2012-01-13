#  CPE 550 - NLCBSMM
#  #  -------------------
#
#  #
#  Last Modified: Sat Jan 10 14:01:43 PST 2009
#  #  @author Stephen Holsapple
CC=gcc
CPP=g++
CFLAGS=-g

LDFLAGS=-lpthread 
HOARD_LDFLAGS=-Wl,--no-as-needed -lrt -L. -lhoard

ALL=benchmark-malloc benchmark-hoard

all:	$(ALL)

benchmark-malloc: benchmark.o
	$(CPP) -o $@ $^ $(LDFLAGS)

# Order that we include the libraries matters?! WTF
benchmark-hoard: benchmark.o
	$(CPP) -o $@ $^ $(HOARD_LDFLAGS) $(LDFLAGS) 

benchmark.o: benchmark.cpp
	$(CPP) $(CFLAGS) -c $<

clean:
	rm -rf core* *.o *.gch $(ALL)
