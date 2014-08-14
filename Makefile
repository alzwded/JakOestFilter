PLA = i386
RVERSION = 1.3.1
VERSION = '"'$(RVERSION)'"'
CC = gcc
CFLAGS = -c -O3 -I. --std=c99 -DVERSION=$(VERSION)
LD = gcc
LDOPTS = -ljpeg -lm -lrt
SRCS = main.c ds800.c gs.c pixelio.c rc.c frame.c common.h mosaic.c mobord.c faith.c Makefile

OBJS = main.o ds800.o gs.o pixelio.o rc.o frame.o mosaic.o mobord.o faith.o

EXE = jakoest

$(EXE): libjw.so $(OBJS)
	$(LD) -o $(EXE) $(OBJS) $(LDOPTS) -Wl,--rpath=.,--rpath=../lib -L. -ljw 

libjw.so:
	stat ../JakWorkers/JakWorkers.c || git clone http://github.com/alzwded/JakWorkers.git ../JakWorkers
	cd ../JakWorkers && make
	cp ../JakWorkers/libjw.so .
	cp ../JakWorkers/JakWorkers.h .

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: dist
dist: $(EXE)
	mkdir -p dist/bin dist/lib dist/doc dist/src
	cp $(EXE) dist/bin/
	cp libjw.so dist/lib/
	cp LICENSE README.md dist/doc/
	cp $(SRCS) dist/src/
	tar cjvf JakOestFilter-$(RVERSION)-$(PLA).tbz dist

clean:
	rm -f $(EXE) *.o *.so JakWorkers.h

dist-clean:
	rm -rf dist JakOestFilter-v*.tbz
