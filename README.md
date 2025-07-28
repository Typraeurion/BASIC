# Beginner's All-purpose Symbolic Instruction Code

BASIC is one of the oldest high-level computer languages.  It was
designed to be easy to learn and use, and (in this author's opinion)
is the best language for an aspiring programmer to start learning how
to tell the computer to do things.

See the accompanying [PDF document](docs/basic.pdf) for information
about using the BASIC interpreter.

## Building

The BASIC interpreter should compile on any BSD or Linux system with a
[GNU C](https://gcc.gnu.org/) compiler, [GNU
Bison](https://www.gnu.org/software/bison/), and
[Flex](https://en.wikipedia.org/wiki/Flex_(lexical_analyser_generator))
(including the `libfl` library, typically provided by “libfl-devel”).
You may also need some version of
[Make](https://www.gnu.org/software/make/).

To build the interpreter, run:

    make basic

If all goes well, you can copy or link the `basic` executable to your
`~/bin/` directory or the system `/usr/local/bin/` directory.

## Running BASIC

When you run `basic`, the interpreter will wait for BASIC commands.
You can enter statements to be run immediately or enter a program by
entering statements with line numbers.

Several example BASIC programs are included with the `basic` source
code in the `examples` directory.  To load these programs, use the `LOAD` command within the BASIC interpreter; for example:

    LOAD "examples/Blackjack.BASIC"

When the program has been loaded, type “`RUN`” to run it.

To exit out of the BASIC interpreter, enter the command “`BYE`”.

## Debugging

There are sure to be bugs in this interpreter, but there may be bugs
in your BASIC code as well.

_(BASIC code tracing functionality is not yet enabled; for now, you'll
have to use `gdb`.  Good luck.)_
