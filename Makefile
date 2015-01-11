PLA = i386
RVERSION = 1.4.0
VERSION = '"'$(RVERSION)'"'
CC = gcc
CFLAGS = -c -O3 -I. --std=c99 -DVERSION=$(VERSION) -msse -msse2 -msse3 -mssse3 -ffast-math -fopenmp -march=prescott
LD = gcc
LDOPTS = -ljpeg -lm -lrt -lgomp
SRCS = main.c ds800.c gs.c pixelio.c rc.c frame.c common.h mosaic.c mobord.c faith.c Makefile

OBJS = main.o ds800.o gs.o pixelio.o rc.o frame.o mosaic.o mobord.o faith.o

EXE = jakoest

$(EXE): $(OBJS)
	$(LD) -o $(EXE) $(OBJS) $(LDOPTS) -Wl,--rpath=.,--rpath=../lib

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: dist
dist: $(EXE)
	mkdir -p dist/bin dist/lib dist/doc dist/src
	cp $(EXE) dist/bin/
	cp LICENSE README.md dist/doc/
	cp $(SRCS) dist/src/
	tar cjvf JakOestFilter-$(RVERSION)-$(PLA).tbz dist

clean:
	rm -f $(EXE) *.o JakWorkers.h

dist-clean:
	rm -rf dist JakOestFilter-v*.tbz
