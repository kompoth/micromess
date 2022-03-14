PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

PROJ = umess
CC = gcc
CFLAGS = -std=c99 -Wall -pedantic -Os -D_POSIX_C_SOURCE=200809L
LIBS = -lX11 -lXft -lfontconfig -lXrandr
INCS = -I/usr/include/freetype2

all: ${PROJ}

install: all
	mkdir -p ${PREFIX}/bin
	cp -f ${PROJ} ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/${PROJ}
	mkdir -p ${MANPREFIX}/man1
	cp -f ${PROJ}.1 ${MANPREFIX}/man1
	chmod 644 ${MANPREFIX}/man1/${PROJ}.1

${PROJ}: ${PROJ}.c
	${CC} ${CFLAGS} ${INCS} $< -o $@ ${LIBS}

uninstall:
	rm -f ${PREFIX}/bin/${PROJ}
	rm -f ${MANPREFIX}/man1/${PROJ}.1

clean:
	rm -f ${PROJ}

.PHONY: all clean install uninstall
