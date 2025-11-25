BASIC is a tokenized language, meaning when you enter a statement it
parses the text into tokens that it recognizes, converts them to
codes, and then stores or executes this series of tokens.  Listing a
program converts these tokens back into text.  The internal data
structure of a BASIC statement is described here.

## Lines

A line consists of a 2-byte size of the line in bytes (including the
header), 4-byte line number (unsigned long), and a variable number of
statement structures.  A stored line of code is terminated by a
newline (`'\n'`) which is stored as a character token (000A).

## Statements

A statement consists of a 2-byte size of the statement in bytes
(including the header), 2-byte command token, and a variable number of
additional tokens depending on the command.  All tokens are shorts, so
the entire statement is word-aligned.

During parsing, the parser adds a placeholder for the line number in
front of the first statement on a line which is initialized to -1
(FFFF FFFF).  This is replaced with the actual line number when the
parser is finished with the whole line.

When multiple statements are present on the same line, they are
separated by a colon (`':'`) which is stored as a character token (003A).

There is special handling for `IF` statements, as these are followed
by a `THEN` with another statement or line number instead of a colon
and optionally an `ELSE` and a third statement or line number.  This
format is described in more detail below.

### Constants

#### Numbers

##### Integers

Integers are stored with the internal token `INTEGER` followed by a
4-byte signed long value.

##### Floating Point

Floating-point numbers are stored with the internal token
`FLOATINGPOINT` followed by an 8-byte double-precision value.

#### Strings

String literals are stored with the internal token `STRING`, a 2-byte
length of the string _not_ including the header, terminator, or
padding, and a variable length string value terminated by a nul
character and padded to a word boundary.

### Variables

Simple variables are tokenized by the internal token `IDENTIFIER` for
numeric variables or `STRINGIDENTIFIER` for string variables, followed
by a 2-byte index into a table of variable names.

Array variables are tokenized by the same internal token and name
index to start with, followed by the `'('` character (token 0028), a
numeric expression for the first index, zero or more groups of a `','`
character (token 002C) followed by another numeric expression, then
terminated by the `')'` character (token 0029).

Note: numeric function calls have the same syntax and tokenization as
array variables.  The two are disambiguated internally by a structure
that contains a function pointer for built-in functions or data
pointer for arrays declared with the `DIM` statement.

### Lists

Commands that use lists of items store them with an `ITEMLIST` token
followed by a header containing a 2-byte length of the list in bytes
(_not_ including the `ITEMLIST` token), a 2-byte count of the number
of items in the list, and a variable number of items.

Each item is specified by a 2-byte length of the item in bytes
followed by the item tokens.  Each item _except_ the last one is
followed by a separator token (typically `','`) which is _not_
included in the item's length.

The `PRINT` statement requires a modified form of this list structure
since it doesn't require a separator between each print item and the
`','` character is treated as a special print instruction (advance to
the next tab stop) which may appear at the beginning or multiple times
in sequence unlike its use in normal lists.  This is stored as a
`PRINTLIST` token followed by the same list structure as above, but
the items in the list are _not_ followed by separator tokens; instead,
any `','` or `';'` separators are added to the list as independent
items.

### Arithmetic Expressions

Explicitly parenthesized expressions are tokenized by the `'('`
character (token 0028) followed by the expression tokens and ending
with the `')'` character (token 0029).

The unary negation operator is tokenized by a “phantom” parenthesis
`'{'` (token 007B), the `NEG` token, the arithmetic expression tokens
to negate, and ending with a closing phantom parenthesis `'}'` (token
007D).

All binary operators are tokenized by a “phantom” parenthesis `'{'`
(token 007B), the arithmetic expression tokens for the first operand,
the arithmetic operator character (e.g. `'+'` as token 002B), the
arithmetic expression tokens for the second operand, and finally the
closing phantom parenthesis `'}'` (token 007D).  The grammar rules
ensure that operators are processed in order of precedence; the
phantom parentheses added during tokenization ensure that this
precedence is followed when the expression is evaluated.

### Simple Commands

Commands with no parameters or text after them (e.g. `CONTINUE`,
`END`) just have the length and command token; no other tokens follow.

### `DATA` Statement

This consists of the `DATA` token, the internal token `ITEMLIST`, a
list header, and variable number of constants (numbers or strings).

Due to the grammar, negative numbers are handled specially, because
the `'-'` prefix is normally treated as a unary operator and integer
literals stored an unsigned numbers.  If there is a negative integer
it is tokenized with the short expression `'{'` (phantom brace), NEG,
INTEGER, _absolute integer value_, `'}'`.  If there is a negative
floating-point number (or integer exceeding the sizef of an `unsigned
long`) its value is negated during tokenization.

### `DEF`ine Function

This statement starts with the usual command header: an unsigned short
length of the statement followed by the `DEF` token.

The next item is the function name and argument list.  This begins
with the tokens `NUMLVAL`, `IDENTIFIER`, and a short variable ID in
the case of a numeric function; or `STRLVAL`, `STRIDENTIFIER`, and a
short variable ID for a string function.

In both cases the function identifier is followed by the tokens `'('`,
`ITEMLIST`, the length in bytes of the item list (including the length
shorts), the number of items in the list, then the arguments.  Each
argument is tokenized with the length of the list item (6), the token
`IDENTIFIER` or `STRINGIDENTIFIER`, and a short variable ID.  Each
argument except the last is followed by a `','` token, and the last
argument is followed by the `')'` token.

Following the argument list are the tokens `'='` and either `NUMEXPR`
or `STREXPR`, matching the type of the function.  Finally there are
the tokens for the expression that defines the function; see
“Arithmetic Expressions” above for details.

### `IF` / `THEN` / [`ELSE`]

There are two forms for the `THEN` and `ELSE` clauses of the `IF`
statement: one which targets line numbers with an implied `GOTO` and
another which uses full statement syntax.  In both cases, a numeric
expression (beginning with the `NUMEXPR` token) runs from the `IF`
command token to the `THEN` token.  This initial statement consists of
a 2-byte length, the `IF` command token, a 2-byte offset to the
statement following `THEN`, a 2-byte offset to the statement following
`ELSE` or the end of the line if there is no `ELSE` part, the numeric
expression tokens, and finally the `THEN` token.  It does not include
the following statements.

Following `THEN` is a new statement header for either the implied
`GOTO` or a regular statement.  In the first case This starts with a
2-byte length of the statement in bytes (12), the implied `GOTO`
token, an `INTEGER` token, the target line number (long), and ending
token.  In the second case the length will vary and the rest of the
statement will follow regular tokenization.  If the `IF` statement
includes an `ELSE` clause then the ending token will be the `ELSE`
token; otherwise it will be set to either a colon (`':'`, token 003A)
or newline (`'\n'`, token 000A) as the rest of the line is parsed.

When there is an `ELSE` clause the above `THEN` statement is followed
by a second implied `GOTO` or regular statement.  It is constructed
the same way as above, only the ending token must be either `':'` or
`'\n'`.
