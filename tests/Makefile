# Makefile for running tests of BASIC commands

PROGRAM=../basic

.PHONY: test-bye test-conditions test-continue test-data test-def test-dim \
	test-end test-for test-if test-input test-let \
	test-print test-rem test-restore test-stop

all:	test-bye test-rem test-print test-let test-dim \
	test-end test-stop test-continue \
	test-data test-read test-restore \
	test-for test-if \
	test-conditions test-def

test-bye: BYE.BASIC
	cat BYE.BASIC | $(PROGRAM)

test-conditions: condexpr.BASIC
	cat condexpr.BASIC | $(PROGRAM)

test-continue: CONTINUE.BASIC
	cat CONTINUE.BASIC | $(PROGRAM)

test-data: DATA.BASIC
	cat DATA.BASIC | $(PROGRAM)

test-def: DEF.BASIC
	cat DEF.BASIC | $(PROGRAM)

test-dim: DIM.BASIC
	cat DIM.BASIC | $(PROGRAM)

test-end: END.BASIC
	cat END.BASIC | $(PROGRAM)

test-for: FOR.BASIC
	cat FOR.BASIC | $(PROGRAM)

test-if: IF.BASIC
	cat IF.BASIC | $(PROGRAM)

test-input: INPUT.BASIC
	echo "Warning: automatic input will not work as expected."
	cat INPUT.BASIC | $(PROGRAM)

test-let: LET.BASIC
	cat LET.BASIC | $(PROGRAM)

test-print: PRINT.BASIC
	cat PRINT.BASIC | $(PROGRAM)

test-read: DATA.BASIC READ.BASIC
	cat DATA.BASIC READ.BASIC | $(PROGRAM)

test-rem: REM.BASIC
	cat REM.BASIC | $(PROGRAM)

test-restore: DATA.BASIC RESTORE.BASIC
	cat DATA.BASIC RESTORE.BASIC | $(PROGRAM)

test-stop: STOP.BASIC
	cat STOP.BASIC | $(PROGRAM)
