# benjamin Maitland
CC=gcc
GL_LIBS = -lGLEW -lglfw3 -lGL -lX11 -lXxf86vm -lpthread -lXrandr -lXinerama -lXcursor -lXi -ldl -lm
CFLAGS=-std=c99 -c -Wall -Wextra -g -O3 -flto -march=native
LIBS=-lSDL2 -lm -pthread libthreadpool.a $(GL_LIBS)
OBJECTS=MySim.o DiamondSquare.o xoroshiro128plus.o utilities.o interpolation.o sobel.o map2d.o erosion.o tectonics.o

all: main

main: $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBS) -o MySim

clean:
	rm -rf *.o
