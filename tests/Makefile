# Makefile for running tests of BASIC commands

PROGRAM=../basic

.PHONY: test-bye test-print test-rem

all: test-bye test-print test-rem

test-bye: BYE.BASIC
	cat BYE.BASIC | $(PROGRAM)

test-print: PRINT.BASIC
	cat PRINT.BASIC | $(PROGRAM)

test-rem: REM.BASIC
	cat REM.BASIC | $(PROGRAM)
