OBJS = main.o DVDRipper.o
CC = g++
LFLAGS := -Wall $(shell pkg-config --cflags --libs libdvdcss libiso9660)
CFLAGS := $(LFLAGS) -c
OUTPUT = dvdripper

$(OUTPUT): $(OBJS)
	$(CC) $(OBJS) $(LFLAGS) -o $(OUTPUT)

%.o: DVDRipper.h %.cpp
	$(CC) $(CFLAGS) -c $*.cpp

clean:
	rm -f *.o *~

realclean: clean
	rm -f $(OUTPUT)
