#  CPE 550 - NLCBSMM
#  #  -------------------
#
#  #
#  Last Modified: Sat Jan 10 14:01:43 PST 2009
#  #  @author Stephen Holsapple
CC=gcc
CPP=g++
CFLAGS=-g

LDFLAGS=-lpthread -lrt
HOARD_LDFLAGS=-Wall -Wl,--no-as-needed -L. -lhoard

ALL=benchmark-malloc benchmark-hoard mmult-malloc mmult-hoard counter-malloc counter-hoard

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

counter-malloc: counter.o
	$(CPP) -o $@ $^ $(LDFLAGS)

counter-hoard: counter.o
	$(CPP) -o $@ $^ $(HOARD_LDFLAGS) $(LDFLAGS)

counter.o: counter.cpp
	$(CPP) $(CFLAGS) -c $<

clean:
	rm -rf core* *.o *.gch go-pipe libhoard.so $(ALL)

test:
	cd ../../src; make; cd ../experiments/benchmark; make clean; cp ../../src/libhoard.so .; make $(ALL)
