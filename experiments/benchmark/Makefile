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
HOARD_PC_LDFLAGS=-Wall -Wl,--no-as-needed -L. -lhoard-pc
HOARD_LRC_LDFLAGS=-Wall -Wl,--no-as-needed -L. -lhoard-lrc

ALL=mmult-malloc mmult-hoard-pc mmult-hoard-lrc mmult-lock-row-malloc mmult-lock-row-hoard-pc mmult-lock-row-hoard-lrc mmult-lock-col-malloc mmult-lock-col-hoard-pc mmult-lock-col-hoard-lrc mmult-lock-strict-malloc mmult-lock-strict-hoard-pc mmult-lock-strict-hoard-lrc

all:	$(ALL)

# MMULT.C
mmult-malloc: mmult.o
	$(CPP) -o $@ $^ $(LDFLAGS)
mmult-hoard-pc: mmult.o
	$(CPP) -o $@ $^ $(HOARD_PC_LDFLAGS) $(LDFLAGS)
mmult-hoard-lrc: mmult.o
	$(CPP) -o $@ $^ $(HOARD_LRC_LDFLAGS) $(LDFLAGS)
mmult.o: mmult.c
	$(CPP) $(CFLAGS) -c $<

# MMULT-LOCK-ROW.C
mmult-lock-row-malloc: mmult-lock-row.o
	$(CPP) -o $@ $^ $(LDFLAGS)
mmult-lock-row-hoard-pc: mmult-lock-row.o
	$(CPP) -o $@ $^ $(HOARD_PC_LDFLAGS) $(LDFLAGS)
mmult-lock-row-hoard-lrc: mmult-lock-row.o
	$(CPP) -o $@ $^ $(HOARD_LRC_LDFLAGS) $(LDFLAGS)
mmult-lock-row.o: mmult-lock-row.c
	$(CPP) $(CFLAGS) -c $<

# MMULT-LOCK-COL.C
mmult-lock-col-malloc: mmult-lock-col.o
	$(CPP) -o $@ $^ $(LDFLAGS)
mmult-lock-col-hoard-pc: mmult-lock-col.o
	$(CPP) -o $@ $^ $(HOARD_PC_LDFLAGS) $(LDFLAGS)
mmult-lock-col-hoard-lrc: mmult-lock-col.o
	$(CPP) -o $@ $^ $(HOARD_LRC_LDFLAGS) $(LDFLAGS)
mmult-lock-col.o: mmult-lock-col.c
	$(CPP) $(CFLAGS) -c $<

# MMULT-LOCK-STRICT.C
mmult-lock-strict-malloc: mmult-lock-strict.o
	$(CPP) -o $@ $^ $(LDFLAGS)
mmult-lock-strict-hoard-pc: mmult-lock-strict.o
	$(CPP) -o $@ $^ $(HOARD_PC_LDFLAGS) $(LDFLAGS)
mmult-lock-strict-hoard-lrc: mmult-lock-strict.o
	$(CPP) -o $@ $^ $(HOARD_LRC_LDFLAGS) $(LDFLAGS)
mmult-lock-strict.o: mmult-lock-strict.c
	$(CPP) $(CFLAGS) -c $<

clean:
	rm -rf core* *.o *.gch go-pipe libhoard-*.so bin/ $(ALL)

test:
	cd ../../src; make; cd ../experiments/benchmark; make clean; cp ../../src/libhoard.so .; make $(ALL)

test64:
	cd ../../src; make linux-gcc-x86_64; cd ../experiments/benchmark; make clean; cp ../../src/libhoard-*.so .; make $(ALL); mkdir bin; mv $(ALL) bin; mv libhoard-*.so bin;
