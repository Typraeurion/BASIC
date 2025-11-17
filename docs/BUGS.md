# Known Bugs

* When tracing expressions is enabled and assigning a value to a
  variable (either with `LET`, `INPUT`, or `READ`), the variable is
  displayed before the value to be assigned is evaluated; in addition,
  any array indices are displayed as they are evaluated.  Thus
  evaluation of the value to assign is disjointed from the variable
  being assigned to.

* `DEF` (define function) is not yet implemented.  It is recognized by
  the parser though.

* It is currently an error to reference an array variable without
  first declaring it with a `DIM` statement.  In Dartmouth basic,
  undeclared arrays had a default dimension of 10.
