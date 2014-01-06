CC = gcc
CFLAGS = -c -O3 -I. --std=c99
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
