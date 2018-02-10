CC = gcc
EXECUTABLES = snc

LIBDIR = 
LIBS = 
CFLAGS = -g -pthread -Wall

all: $(EXECUTABLES)

clean:
	rm -f core $(EXECUTABLES) a.out

snc: snc.o
	$(CC) $(CFLAGS) -o snc snc.o $(LIBS)
