PREFIX ?= /usr
BINDIR = ${PREFIX}/bin
MANDIR = ${PREFIX}/share/man/man1

PROJ = umess
CC = gcc
CFLAGS = -std=c99 -Wall -pedantic -Os -D_POSIX_C_SOURCE=200809L
LIBS = -lX11 -lXft -lfontconfig -lXrandr
INCS = -I/usr/include/freetype2

all: ${PROJ}

install: all
	mkdir -p ${BINDIR}
	cp -f ${PROJ} ${BINDIR}
	chmod 755 ${BINDIR}/${PROJ}
	mkdir -p ${MANDIR}
	cp -f ${PROJ}.1 ${MANDIR}
	chmod 644 ${MANDIR}/${PROJ}.1

${PROJ}: ${PROJ}.c
	${CC} ${CFLAGS} ${INCS} $< -o $@ ${LIBS}

uninstall:
	rm -f ${BINDIR}/${PROJ}
	rm -f ${MANDIR}/${PROJ}.1

clean:
	rm -f ${PROJ}

.PHONY: all clean install uninstall
