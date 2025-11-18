# Known Bugs

* When tracing expressions is enabled and assigning a value to a
  variable (either with `LET`, `INPUT`, or `READ`), the variable is
  displayed before the value to be assigned is evaluated; in addition,
  any array indices are displayed as they are evaluated.  Thus
  evaluation of the value to assign is disjointed from the variable
  being assigned to.

* `DEF` (define function) is not yet implemented.  It is recognized by
  the parser though.

* Dartmouth BASIC allowed PRINT statements to contain string constants
  next to variables without any `;` separator, which was treated the
  same as `;`.  This would be difficult to add to the Bison grammar,
  so this version of BASIC does not support it.

* When a parse or syntax error occurs, the interpreter does not
  indicate where in the line the error was detected.  If the line was
  numbered, the content of the line is not added to the program so it
  can be lost entirely when loading a program from a file.  The parser
  _should_ save the line with the leading token `ERROR` but I haven't
  yet figured out how to get the failed line from the parser.
