VERSION = '"v1.0"'
CC = gcc
CFLAGS = -c -O3 -I. --std=c99 -DVERSION=$(VERSION)
LD = gcc
LDOPTS = -ljpeg -lm

OBJS = main.o ds800.o gs.o pixelio.o rc.o frame.o

EXE = jakoest

$(EXE): $(OBJS)
	$(LD) -o $(EXE) $(OBJS) $(LDOPTS) 

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(EXE) *.o
