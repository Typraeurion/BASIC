# Known Bugs

* `FOR` loops don't work in immediate mode.  (They only work for
  numbered lines.)

* When the initial value of a `FOR` loop exceeds the `TO` value, BASIC
  is supposed to skip the loop.  This has not been implemented yet.

* When tracing expressions is enabled and assigning a value to a
  variable (either with `LET`, `INPUT`, or `READ`), the variable is
  displayed before the value to be assigned is evaluated; in addition,
  any array indices are displayed as they are evaluated.  Thus
  evaluation of the value to assign is disjointed from the variable
  being assigned to.

* `DEF` (define function) is not yet implemented.  It is recognized by
  the parser though.
