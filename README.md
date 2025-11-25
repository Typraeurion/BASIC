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
Bison](https://www.gnu.org/software/bison/) (version 3), and
[Flex](https://en.wikipedia.org/wiki/Flex_(lexical_analyser_generator))
(including the `libfl` library, typically provided by “libfl-devel”).
You may also need some version of
[Make](https://www.gnu.org/software/make/).

It _may_ also compile on MacOS if you have the proper build tools
installed.  XCode can provide most of them, but its version of Bison
is too old; you will need at least that tool from
[Homebrew](https://brew.sh/).

To build the interpreter, run:

     make basic

If all goes well, you can copy or link the `basic` executable to your
`~/bin/` directory or the system `/usr/local/bin/` directory.

## Running BASIC

When you run `basic`, the interpreter will wait for BASIC commands.
You can enter statements to be run immediately or enter a program by
entering statements with line numbers.

Several example BASIC programs are included with the `basic` source
code in the `examples` directory.  To load these programs, use the
`LOAD` command within the BASIC interpreter; for example:

     LOAD "examples/Blackjack.BASIC"

When the program has been loaded, type “`RUN`” to run it.

To exit out of the BASIC interpreter, enter the command “`BYE`”.

## Debugging

There are sure to be bugs in this interpreter, but there may be bugs
in your BASIC code as well.  This implementation of BASIC adds a
`TRACE` command which tells BASIC to show what it's doing with various
levels of detail.  The syntax is:

    TRACE {LINES|STATEMENTS|EXPRESSIONS|GRAMMAR|PARSER} {ON|OFF}

where the first argument is the type of detail to trace and the second
turns tracing for that aspect on or off.  The first three details that
can be traced help you debug your BASIC program:

* `LINES`: Lists each line of the program before executing its statements.
* `STATEMENTS`: Lists each statement before executing it.  This may be
  more useful than `LINES` if your program contains multiple
  statements on most lines; otherwise `LINES` is better at orienting
  your place in the program.
* `EXPRESSIONS`: Displays the result of evaluating expressions.

The next two details that can be traced are for debugging the BASIC
interpreter itself:

* `GRAMMAR`: Shows how Bison interprets the BASIC statements it is
  given and builds token lists from it.  Using this requires knowledge
  of the generated token values (which are enumerated in
  `basic.tab.h`) and the grammar (declared in `basic.y`).
* `PARSER`: Shows the result of Flex parsing input to BASIC into
  tokens, variable identifiers, and constants.  This level of detail
  should rarely be needed.
