# Simple makefile for the BASIC documentation

.PHONY: all dvi pdf info clean

all: pdf info

dvi: basic.dvi

pdf: basic.pdf

info: basic.info

basic.dvi: basic.texinfo
	texi2dvi basic.texinfo

basic.pdf: basic.texinfo
	texi2pdf basic.texinfo

basic.info: basic.texinfo
	makeinfo basic.texinfo

clean:
	rm -f basic.aux basic.cp basic.cps basic.dvi basic.fn basic.fns \
	      basic.info basic.ky basic.log basic.pdf basic.pg \
	      basic.st basic.sts basic.toc basic.tp basic.vr
