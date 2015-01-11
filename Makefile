PLA = i686
CC = gcc
LD = gcc
EXE = jakoest
OBJEXT = o
ADD_CFLAGS = 
ADD_LDOPTS = -lrt -ljpeg
include Makefile.common
.PHONY: dist
dist: $(EXE)
	mkdir -p dist/bin dist/lib dist/doc dist/src
	cp $(EXE) dist/bin/
	cp LICENSE README.md dist/doc/
	cp $(SRCS) dist/src/
	tar cjvf JakOestFilter-$(RVERSION)-$(PLA).tbz dist
