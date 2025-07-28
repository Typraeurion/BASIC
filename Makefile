# Simple makefile for the BASIC interpreter

CC=gcc
CFLAGS=-g

PROGRAM=basic
OBJS=basic.tab.o expression.o functions.o input.o lex.yy.o \
     list.o print.o run.o tables.o wrap.o
CFILES=basic.lex basic.y expression.c functions.c input.c \
     list.c print.c run.c tables.c wrap.c
BFILES=examples/*.BASIC examples/*.patches examples/Animal.more

.PHONY: all dvi pdf info clean distrib

all: ${PROGRAM} pdf

basic.tab.c basic.tab.h: basic.y tables.h
	bison -d basic.y

lex.yy.c lex.yy.h: basic.lex
	flex -d --header-file=lex.yy.h -i basic.lex

${PROGRAM}: ${OBJS}
	${CC} ${CFLAGS} -o ${PROGRAM} ${OBJS} -lfl -lm

expression.o: expression.c tables.h basic.tab.h

functions.o: functions.c tables.h

input.o: input.c tables.h basic.tab.h

lex.yy.o: lex.yy.c basic.tab.h

list.o: list.c lex.yy.h tables.h basic.tab.h

print.o: print.c basic.tab.h tables.h

run.o: run.c tables.h basic.tab.h

tables.o: tables.c lex.yy.h tables.h basic.tab.h

wrap.o: wrap.c basic.tab.h

dvi:
	$(MAKE) -C docs dvi

pdf:
	$(MAKE) -C docs pdf

info:
	$(MAKE) -C Docs info

distrib: BASIC.tar.gz

BASIC.tar.gz: COPYING LICENSE Makefile README.md docs/Makefile docs/basic.texinfo ${BFILES} ${CFILES}
	tar -czf BASIC.tar.gz README.md COPYING LICENSE Makefile docs/Makefile docs/basic.texinfo ${BFILES} ${CFILES}

clean:
	rm -f basic *.o lex.yy.c basic.tab.c basic.tab.h
