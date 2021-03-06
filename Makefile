srcdir=./
VPATH=

prefix=
exec_prefix=
bindir=./bin
mandir=./man
INSTALLCMD=@INSTALL@

CC=gcc -mno-cygwin -O2
CFLAGS=-I.
EXEEXT=

o=.o

# dmake magic
.SUFFIXES : .c .i $(o) .dll $(a) .exe .rc .res
.PHONY : test

all: ccache$(EXEEXT)

OBJOUT_FLAG=-o

.c$(o):
	$(CC) -c $(null,$(<:d) $(NULL) -I$(<:d)) $(CFLAGS_O) $(OBJOUT_FLAG)$@ $<

.c.i:
	$(CC) -c $(null,$(<:d) $(NULL) -I$(<:d)) $(CFLAGS_O) -E $< >$@

.y.c:
	$(NOOP)

OBJS= ccache.o mdfour.o hash.o execute.o util.o args.o stats.o \
	cleanup.o snprintf.o unify.o
HEADERS = ccache.h mdfour.h

docs: ccache.1 web/ccache-man.html

ccache$(EXEEXT): $(OBJS) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

ccache.1: ccache.yo
	-yodl2man -o ccache.1 ccache.yo

web/ccache-man.html: ccache.yo
	mkdir -p man
	yodl2html -o web/ccache-man.html ccache.yo

install: ccache$(EXEEXT) ccache.1
	${INSTALLCMD} -d $(DESTDIR)${bindir}
	${INSTALLCMD} -m 755 ccache$(EXEEXT) $(DESTDIR)${bindir}
	${INSTALLCMD} -d $(DESTDIR)${mandir}/man1
	${INSTALLCMD} -m 644 ${srcdir}/ccache.1 $(DESTDIR)${mandir}/man1/

clean:
	+$(RM) -f $(OBJS) *~ ccache$(EXEEXT)

test: ccache$(EXEEXT)
#	CC='$(CC)' ./test.sh
	.\ccache$(EXEEXT) $(CC) -Werror -DVERSION=\"1\" -c t\quote_passthrough.c

check: test

distclean: clean
	/bin/rm -f Makefile config.h config.sub config.log build-stamp config.status

# FIXME: To fix this, test.sh needs to be able to take ccache from the
# installed prefix, not from the source dir.
installcheck: 
	@echo "WARNING!  This is not really \"installcheck\" yet."
	$(MAKE) check
