# Known Bugs

* When tracing expressions is enabled and assigning a value to a
  variable (either with `LET`, `INPUT`, or `READ`), the variable is
  displayed before the value to be assigned is evaluated; in addition,
  any array indices are displayed as they are evaluated.  Thus
  evaluation of the value to assign is disjointed from the variable
  being assigned to.

* `DEF` (define function) is not yet implemented.  It is recognized by
  the parser though.

* `NEXT` does not support more than one variable in the same
  statement.  It appears that this feature was introduced in Altair
  BASIC (by Microsoft) in 1975, and was not a part of the original
  language but became common in other Microsoft dialects.

* When a parse or syntax error occurs, the interpreter does not
  indicate where in the line the error was detected.  If the line was
  numbered, the content of the line is not added to the program so it
  can be lost entirely when loading a program from a file.  The parser
  _should_ save the line with the leading token `ERROR` but I haven't
  yet figured out how to get the failed line from the parser.
