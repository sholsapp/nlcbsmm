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

ALL=benchmark-malloc benchmark-hoard mmult-malloc mmult-hoard

all:	$(ALL)

benchmark-malloc: benchmark.o
	$(CPP) -o $@ $^ $(LDFLAGS)

benchmark-hoard: benchmark.o
	$(CPP) -o $@ $^ $(HOARD_LDFLAGS) $(LDFLAGS)

benchmark.o: benchmark.cpp
	$(CPP) $(CFLAGS) -c $<

mmult-malloc: mmult.o
	$(CPP) -o $@ $^ $(LDFLAGS)

mmult-hoard: mmult.o
	$(CPP) -o $@ $^ $(HOARD_LDFLAGS) $(LDFLAGS)

mmult.o: mmult.c
	$(CPP) $(CFLAGS) -c $<

clean:
	rm -rf core* *.o *.gch go-pipe libhoard.so $(ALL)

test:
	cd ../../src; make; cd ../experiments/benchmark; make clean; cp ../../src/libhoard.so .; make $(ALL)
