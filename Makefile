# Simple makefile for the BASIC interpreter

CC=gcc
CFLAGS=-g

PROGRAM=basic
OBJS=basic.tab.o expression.o functions.o input.o lex.yy.o \
     list.o print.o run.o tables.o wrap.o
CFILES=basic.lex basic.y expression.c functions.c input.c \
     list.c print.c run.c tables.c wrap.c
BFILES=AceyDuecy.BASIC Amazing.BASIC Amazing.patches Animal.BASIC Animal.more \
     Awari.BASIC Bagels.BASIC Banner.BASIC Basketball.BASIC Batnum.BASIC \
     Battle.BASIC Battle.patches Blackjack.BASIC Blackjack.patches

all: ${PROGRAM}

basic.tab.c basic.tab.h: basic.y tables.h
	bison -d basic.y

lex.yy.c: basic.lex
	flex -d -i basic.lex

${PROGRAM}: ${OBJS}
	${CC} ${CFLAGS} -o ${PROGRAM} ${OBJS} -lfl -lm

expression.o: expression.c tables.h basic.tab.h

functions.o: functions.c tables.h

input.o: input.c tables.h basic.tab.h

lex.yy.o: lex.yy.c basic.tab.h

list.o: list.c tables.h basic.tab.h

print.o: print.c basic.tab.h tables.h

run.o: run.c tables.h basic.tab.h

tables.o: tables.c tables.h basic.tab.h

wrap.o: wrap.c basic.tab.h

dvi: basic.dvi

pdf: basic.pdf

info: basic.info

basic.dvi: basic.texinfo
	texi2dvi basic.texinfo

basic.pdf: basic.texinfo
	texi2pdf basic.texinfo

basic.info: basic.texinfo
	makeinfo basic.texinfo

distrib: BASIC.tar.gz

BASIC.tar.gz: basic.texinfo Makefile ${BFILES} ${CFILES}
	tar -czf BASIC.tar.gz basic.texinfo Makefile ${BFILES} ${CFILES}
