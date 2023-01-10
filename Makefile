# sehr chaotisch, aber geht

#----------------------------
#        OS Specific
#----------------------------
RM=rm
PSEP=/

#----------------------------
#         Patric
#----------------------------
CC=gcc -g -DDEBUG -DPATRIC_ALT
CFLAGS=-c -Wall -std=c11 -Isrc
LDFLAGS=
LIBFLAGS=
SOURCES=$(wildcard src$(PSEP)*.c)
OBJECTS=src/patric.o src/triangle.o
OBJECTS_ALT=src/patric_alt.o src/triangle.o
BINARY=bin/patric
BINARY_ALT=bin/patric_alt

all: $(SOURCES) $(BINARY) $(BINARY_ALT)

$(BINARY): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@ $(LIBFLAGS)

$(BINARY_ALT): $(OBJECTS_ALT)
	$(CC) $(LDFLAGS) $(OBJECTS_ALT) -o $@ $(LIBFLAGS)

.c.o:
	$(CC) $(CFLAGS) $< -o $@ $(LIBFLAGS)

clean:
	$(RM) $(OBJECTS) src/patric_alt.o
