CC=gcc
OBJECTS=cpu.o mmu.o cart.o display.o serial.o main.o timer.o audio.o input.o debug.o terminal.o opengl.o util.o
CFLAGS=-DMACOSX -O0 -g -Wall -Wno-deprecated-declarations -Werror
LDFLAGS=-L/usr/X11R6/lib -framework OpenGL -framework GLUT -framework Cocoa
EXECUTABLES=gb

all: gb

gb: $(OBJECTS)
	gcc $(LDFLAGS) -o gb $^

clean:
	-rm -f *.o $(EXECUTABLES)

.PHONY: clean tests
