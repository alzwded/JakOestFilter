VERSION = '"v1.1"'
CC = gcc
CFLAGS = -c -O3 -I. --std=c99 -DVERSION=$(VERSION)
LD = gcc
LDOPTS = -ljpeg -lm -lrt

OBJS = main.o ds800.o gs.o pixelio.o rc.o frame.o

EXE = jakoest

$(EXE): libjw.so $(OBJS)
	$(LD) -o $(EXE) $(OBJS) $(LDOPTS) -Wl,--rpath=. -L. -ljw 

libjw.so:
	stat ../JakWorkers/JakWorkers.c || git clone http://github.com/alzwded/JakWorkers.git ../JakWorkers
	cd ../JakWorkers && make
	cp ../JakWorkers/libjw.so .
	cp ../JakWorkers/JakWorkers.h .

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(EXE) *.o *.so JakWorkers.h
