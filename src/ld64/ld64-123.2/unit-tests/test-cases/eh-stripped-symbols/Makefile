
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

#
# verify that if symbols are stripped out of .o files
# the linker can still process unwind info from .o files
# correctly

run: all

all:
	${CXX} ${CCXXFLAGS} main.cxx -g -c -o main1.o -Os
	#strip main1.o -u -s keep.exp -o main2.o
	${LD} main1.o -r -x -exported_symbols_list keep.exp -o main2.o
	${CXX} ${CCXXFLAGS} main1.o -o main1 
	${CXX} ${CCXXFLAGS} main2.o -o main2
	${UNWINDDUMP} -arch ${ARCH} -no_symbols main1 > main1.unwind
	${UNWINDDUMP} -arch ${ARCH} -no_symbols main2 > main2.unwind
	${PASS_IFF} diff main1.unwind main2.unwind

clean:
	rm -f main1* main2*
