/* Lexical scanner for BASIC */

%{
  /* Declaration for isctype() functions */
#include <ctype.h>
#include <stdio.h>
  /* Declaration for the strtox() functions */
#include <stdlib.h>
  /* Declaration for the strdup() function */
#include <string.h>
  /* Definitions of tokens to pass to the parser */
#include "basic.tab.h"
#include "tables.h"
  /* Keep track of how far we've nested LOAD commands */
signed int current_load_nesting = 0;
%}

/* Start condition for REM statements */
%x REMark

%%

[0-9]*"."[0-9]*(E[-+][0-9]+)?	{
  /* The number must contain at least one digit
   * on either side of the decimal point */
  if ((yytext[0] == '.') && ((yytext[1] < '0') || (yytext[1] > '9')))
    {
      if (tracing & TRACE_PARSER)
	fprintf (stderr, "Decimal point without a digit on either side; trying a different match\n");
      REJECT;
    }
  yylval.floating_point = strtod (yytext, NULL);
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed floating-point constant %g\n", yylval.floating_point);
  return FLOATINGPOINT;
}

[0-9]+	{
  yylval.floating_point = strtod (yytext, NULL);
  /* If the number is out of range, we have to use FLOATINGPOINT. */
  if (yylval.floating_point > 4294967295.0) {
    if (tracing & TRACE_PARSER)
      fprintf (stderr, "Parsed very large integer; using floating point %g instead\n", yylval.floating_point);
    return FLOATINGPOINT;
  }
  yylval.integer = (unsigned long) yylval.floating_point;
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed integer constant %d\n", yylval.integer);
  return INTEGER;
}

"\""[^\"]*"\""	{
  yylval.string = (struct string_value *) calloc
    (1, WALIGN (sizeof (struct string_value) + yyleng - 2 + 1));
  yylval.string->length = yyleng - 2;
  memcpy (yylval.string->contents, &yytext[1], yyleng - 2);
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed string constant \"%s\"\n", yylval.string->contents);
  return STRING;
}

"<="	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed less-or-equals comparator\n");
  return LESSEQ;
}

">="	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed greater-or-equals comparator\n");
  return GRTREQ;
}

"<>"	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed not-equals comparator\n");
  return NOTEQ;
}

"\n"|"$"|"("|")"|"*"|"+"|","|"-"|"/"|":"|";"|"<"|"="|">"|"^"	{
  if (tracing & TRACE_PARSER)
    {
      if (yytext[0] == '\n')
	fprintf (stderr, "Parsed newline symbol\n");
      else
	fprintf (stderr, "Parsed symbol '%c'\n", yytext[0]);
    }
  return yytext[0];
}

AND	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed AND operator\n");
  return AND;
}

BYE	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed BYE command\n");
  return BYE;
}

CONT("."|INUE)	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed CONTINUE command\n");
  return CONTINUE;
}

DATA	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed DATA command\n");
  return DATA;
}

DEF	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed DEF command\n");
  return DEF;
}

DIM	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed DIM command\n");
  return DIM;
}

ELSE	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed ELSE token\n");
  return ELSE;
}

END	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed END command\n");
  return END;
}

EXPR("."|ESSIONS)	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed EXPRESSIONS token\n");
  return EXPRESSIONS;
}

FOR	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed FOR command\n");
  return FOR;
}

GOSUB	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed GOSUB command\n");
  return GOSUB;
}

GOTO	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed GOTO command\n");
  return GOTO;
}

GRAMMAR	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed GRAMMAR token\n");
  return GRAMMAR;
}

IF	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed IF command\n");
  return IF;
}

INPUT	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed INPUT command\n");
  return INPUT;
}

LET	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed LET command\n");
  return LET;
}

LINES	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed LINES token\n");
  return LINES;
}

LIST	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed LIST command\n");
  return LIST;
}

LOAD	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed LOAD command\n");
  return LOAD;
}

NEW	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed NEW command\n");
  return NEW;
}

NEXT	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed NEXT command\n");
  return NEXT;
}

OFF	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed OFF token\n");
  return OFF;
}

ON	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed ON token\n");
  return ON;
}

OR	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed OR operator\n");
  return OR;
}

PARSER {
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed PARSER token\n");
  return PARSER;
}

PRINT	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed PRINT command\n");
  return PRINT;
}

READ	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed READ command\n");
  return READ;
}

RESTORE	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed RESTORE command\n");
  return RESTORE;
}

RETURN	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed RETURN command\n");
  return RETURN;
}

REM("."|ARK)?	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed REM command\n");
  BEGIN(REMark);
  return REM;
}

RUN	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed RUN command\n");
  return RUN;
}

SAVE	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed SAVE command\n");
  return SAVE;
}

STATEMENTS	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed STATEMENTS token\n");
  return STATEMENTS;
}

STEP	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed STEP token\n");
  return STEP;
}

STOP	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed STOP command\n");
  return STOP;
}

THEN	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed THEN token\n");
  return THEN;
}

TO	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed TO token\n");
  return TO;
}

TRACE	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed TRACE command\n");
  return TRACE;
}

<REMark>.+	{
  yylval.string = (struct string_value *) calloc
    (1, WALIGN (sizeof (struct string_value) + yyleng + 1));
  if (yytext[0] == ' ')
    {
      yylval.string->length = yyleng - 1;
      memcpy (yylval.string->contents, &yytext[1], yyleng - 1);
    } else {
      yylval.string->length = yyleng;
      memcpy (yylval.string->contents, yytext, yyleng);
    }
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed remark \"%s\"\n", yylval.string->contents);
  BEGIN(0);
  return RESTOFLINE;
}

<REMark>"\n"	{
  /* Empty comment */
  yylval.string = (struct string_value *) calloc
    (1, WALIGN (sizeof (struct string_value) + 1));
  unput ('\n');
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed empty remark\n", yylval.string->contents);
  BEGIN(0);
  return RESTOFLINE;
}

TAB	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed TAB\n");
  return TAB;
}

[A-Z][0-9A-Z]*\$?	{
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Parsed identifier %s;", yytext);
  yylval.integer = find_var_name (yytext);
  if (tracing & TRACE_PARSER)
    fprintf (stderr, " matches %s identifier #%d\n",
	     (yytext[yyleng - 1] == '$') ? "string" : "numeric",
	     yylval.integer);
  return ((yytext[yyleng - 1] == '$') ? STRINGIDENTIFIER : IDENTIFIER);
}

[\t ]+	/* Ignore whitespace, except to separate tokens */

[^-\t\n \"$()*+,./:;<=>0-9A-Z^].*	{
  /* Any unrecognized character should result in
   * the entire line being an error. */
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Unrecognized character during parsing; code point %d\n",
	     yytext[0]);
  printf ("ERROR- %s\n", yytext);
  return RESTOFLINE;
}

(".")|("."[^\n0-9].*)	{
  /* `Floating-point number' patterns without any digits (i.e.,
   * a single period) should fall through here. */
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Decimal point without a digit on either side\n");
  printf ("ERROR- %s\n", yytext);
  return RESTOFLINE;
}

  /*
   * On reaching End-of-File, check whether we are processing a LOAD
   * statement.  If so, close the file and switch to the previous
   * buffer. See the Flex manual on "Multiple input buffers".
   */
<<EOF>> {
  if (--current_load_nesting < 0) {
    if (tracing & TRACE_PARSER)
      fprintf(stderr, "End of file reached on primary input; quitting.\n");
    yyterminate();
  }
  else {
    if (tracing & TRACE_PARSER)
      fprintf (stderr, "End of file reached; switching back to the previous input\n");
    yypop_buffer_state();
  }
}
