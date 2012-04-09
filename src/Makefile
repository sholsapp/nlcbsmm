# CPE 550 - NLCBSMM
#  -------------------
#
#  #
#  Last Modified: Sat Jan 10 14:01:43 PST 2009
#  #  @author Stephen Holsapple
CC=gcc
CPP=g++

CFLAGS=
LDFLAGS=

ALL=linux-gcc-x86
all:  $(ALL)

LINUX_GCC_x86_COMPILE := g++ -m32 -I/usr/include/nptl -ffast-math -g -malign-double -pipe -march=pentium4 -O0 -finline-limit=20000 -fomit-frame-pointer -finline-functions  -DNDEBUG  -I. -Iheaplayers -Iheaplayers/util -Inlcbsmm -D_REENTRANT=1 -shared -D'CUSTOM_PREFIX(x)=hoard\#\#x' nlcbsmm/mutex.cpp nlcbsmm/constants.cpp gnuwrapper.cpp libhoard.cpp nlcbsmm/vmmanager.cpp -Bsymbolic -o libhoard.so -ldl -lpthread
#LINUX_GCC_x86_COMPILE := g++ -Wl,--no-undefined -m32 -I/usr/include/nptl -ffast-math -g -malign-double -pipe -march=pentium4 -O3 -finline-limit=20000 -fomit-frame-pointer -finline-functions  -DNDEBUG  -I. -Iheaplayers -Iheaplayers/util -Inlcbsmm -D_REENTRANT=1 -shared -D'CUSTOM_PREFIX(x)=hoard\#\#x' gnuwrapper.cpp libhoard.cpp nlcbsmm/vmmanager.cpp -Bsymbolic -o libhoard.so -ldl -lpthread

linux-gcc-x86:
	$(LINUX_GCC_x86_COMPILE)

LINUX_GCC_x86_64_COMPILE := g++ -Wunused-parameter -march=nocona -m64 -g -I/usr/include/nptl -pipe -fPIC -O3 -finline-limit=20000 -fomit-frame-pointer -finline-functions  -DNDEBUG  -I. -Iheaplayers -Iheaplayers/util -Inlcbsmm -D_REENTRANT=1 -shared  -D'CUSTOM_PREFIX(x)=hoard\#\#x' nlcbsmm/mutex.cpp nlcbsmm/constants.cpp gnuwrapper.cpp libhoard.cpp nlcbsmm/vmmanager.cpp -Bsymbolic -o libhoard.so -ldl -lpthread
#LINUX_GCC_x86_64_COMPILE := g++ -W -Wconversion -Wall -march=nocona -m64 -msse2 -I/usr/include/nptl -pipe -fPIC -O3 -finline-limit=20000 -fomit-frame-pointer -finline-functions  -DNDEBUG  -I. -Iheaplayers -Iheaplayers/util -Inlcbsmm -D_REENTRANT=1 -shared  -D'CUSTOM_PREFIX(x)=hoard\#\#x' nlcbsmm/mutex.cpp nlcbsmm/constants.cpp gnuwrapper.cpp libhoard.cpp nlcbsmm/vmmanager.cpp -Bsymbolic -o libhoard.so -ldl -lpthread

linux-gcc-x86_64:
	$(LINUX_GCC_x86_64_COMPILE)



clean:
	rm -rf core* *.o *.gch libhoard.so $(ALL)

