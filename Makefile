OBJS = main.o DVDRipper.o
CC = g++
CFLAGS := -Wall $(shell pkg-config --cflags --libs libdvdcss libiso9660) -c
LFLAGS := -Wall $(shell pkg-config --cflags --libs libdvdcss libiso9660)

dvdripper: $(OBJS)
	$(CC) $(OBJS) $(LFLAGS) -o dvdripper

%.o: DVDRipper.h %.cpp
	$(CC) $(CFLAGS) -c $*.cpp

clean:
	rm -f *.o *~ dvdripper
