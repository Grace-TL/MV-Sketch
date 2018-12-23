EXEC += main_hitter main_changer main_simd
all: $(EXEC)

CFLAGS = -Wall -std=c++11 -O3
CFLAGS += -D TRACE_DIR_RAM

HEADER += hash.h datatypes.hpp  inputadaptor.hpp util.h 
SRC += hash.c inputadaptor.cpp
SKETCHHEADER += mvsketch.hpp
SKETCHSRC += mvsketch.cpp
LIBS= -static-libstdc++ -lPcap++ -lPacket++ -lCommon++ -lpcap -lpthread -lgmp -std=gnu++11
INCLUDES=-I/usr/local/include/pcapplusplus/
#INCLUDESIMD= -I/home/ltang/Workspace/research/MV-Sketch/lib/FastMemcpy


main_changer: main_changer.cpp $(SRC) $(SKETCHSRC) $(HEADER) $(SKETCHHEADER)
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRC) $(SKETCHSRC) $(LIBS) -lm -ldl

main_hitter: main_hitter.cpp $(SRC) $(SKETCHSRC) $(HEADER) $(SKETCHHEADER) 
	g++ $(CFLAGS) $(INCLUDES) -o $@ $< $(SRC) $(SKETCHSRC) $(LIBS) -lm -ldl 

main_simd: main_simd.cpp $(SRC) $(HEADER)
	g++ $(CFLAGS) -mavx2 -mfma $(INCLUDES) $(INCLUDESIMD) -o $@ $< $(SRC) $(SKETCHSRC) mvsketch_simd.cpp $(LIBS) -ldl -lm 



clean:
	rm -rf $(EXEC)
	rm -rf *log*
	rm -rf *out*
