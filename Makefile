CC=gcc
OBJECTS=cpu.o mmu.o cart.o display.o serial.o main.o timer.o audio.o input.o debug.o terminal.o util.o crc32.o gl_text.o
CFLAGS=-DMACOSX -O0 -g -Wall -Wno-deprecated-declarations -Werror -I/usr/local/include
CXXFLAGS=-DMACOSX -O0 -g -Wall -Wno-deprecated-declarations -Werror -I/usr/local/include -std=c++11
LDFLAGS=-framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -g -std=c++11 -L/usr/local/lib -lglfw3
EXECUTABLES=gb

all: gb

gb: $(OBJECTS)
	g++ $(LDFLAGS) -o gb $^

clean:
	-rm -f *.o $(EXECUTABLES)

.PHONY: clean tests
