CC = gcc
CFLAGS = -c -g -I. --std=c99
LD = gcc
LDOPTS = -ljpeg

OBJS = main.o ds800.o gs.o pixelio.o rc.o

EXE = jakoest

$(EXE): $(OBJS)
	$(LD) -o $(EXE) $(OBJS) $(LDOPTS) 

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(EXE) *.o
