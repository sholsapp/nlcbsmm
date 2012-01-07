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
CFLAGS=-pedantic $(SUS3) $(HARDEN)
LDFLAGS=-lelf

ALL=clone_test elf_test

all:	$(ALL)

clone_test: clone_test.o
	$(CPP) $(LDFLAGS) -o $@ $^

clone_test.o: clone_test.cpp
	$(CPP) $(CFLAGS) -c $<

elf_test: elf_test.o
	$(CPP) $(LDFLAGS) -o $@ $^

elf_test.o: elf_test.cpp
	$(CPP) $(CFLAGS) -c $<
clean:
	rm -rf core* *.o *.gch $(ALL)
