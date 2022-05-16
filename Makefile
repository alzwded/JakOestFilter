PLA = x86-64
CC = gcc
LD = gcc
EXE = jakoest
OBJEXT = o
ADD_CFLAGS = 
ADD_LDOPTS = -lrt -ljpeg
include Makefile.common
.PHONY: dist
dist: $(EXE) predist
	tar cjvf JakOestFilter-$(RVERSION)-$(PLA).tbz dist
