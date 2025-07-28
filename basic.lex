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
unsigned int current_load_nesting = 0;
%}

/* Start condition for REM statements */
%x REMark

%%

[0-9]*"."[0-9]*(E[-+][0-9]+)?	{
  /* The number must contain at least one digit
   * on either side of the decimal point */
  if ((yytext[0] == '.') && ((yytext[1] < '0') || (yytext[1] > '9')))
    {
      REJECT;
    }
  yylval.floating_point = strtod (yytext, NULL); return FLOATINGPOINT;
}

[0-9]+	{
  yylval.floating_point = strtod (yytext, NULL);
  /* If the number is out of range, we have to use FLOATINGPOINT. */
  if (yylval.floating_point > 4294967295.0)
    return FLOATINGPOINT;
  yylval.integer = (unsigned long) yylval.floating_point;
  return INTEGER;
}

"\""[^\"\n]*"\""	{
  yylval.string = (struct string_value *) calloc
    (1, WALIGN (sizeof (struct string_value) + yyleng - 2 + 1));
  yylval.string->length = yyleng - 2;
  memcpy (yylval.string->contents, &yytext[1], yyleng - 2);
  return STRING;
}

"<="	return LESSEQ;

">="	return GRTREQ;

"<>"	return NOTEQ;

"\n"|"$"|"("|")"|"*"|"+"|","|"-"|"/"|":"|";"|"<"|"="|">"|"^"	{
  return yytext[0];
}

AND	return AND;

BYE	return BYE;

CONTINUE	return CONTINUE;

DATA	return DATA;

DEF	return DEF;

DIM	return DIM;

ELSE	return ELSE;

END	return END;

FOR	return FOR;

GOSUB	return GOSUB;

GOTO	return GOTO;

IF	return IF;

INPUT	return INPUT;

LET	return LET;

LIST	return LIST;

LOAD	return LOAD;

NEW	return NEW;

NEXT	return NEXT;

ON	return ON;

OR	return OR;

PRINT	return PRINT;

READ	return READ;

RESTORE	return RESTORE;

RETURN	return RETURN;

REM	{
  BEGIN(REMark);
  return REM;
}

RUN	return RUN;

SAVE	return SAVE;

STEP	return STEP;

STOP	return STOP;

THEN	return THEN;

TO	return TO;

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
  BEGIN(0);
  return RESTOFLINE;
}

<REMark>"\n"	{
  /* Empty comment */
  yylval.string = (struct string_value *) calloc
    (1, WALIGN (sizeof (struct string_value) + 1));
  unput ('\n');
  BEGIN(0);
  return RESTOFLINE;
}

TAB	return TAB;

[A-Z][0-9A-Z]*\$?	{
#ifdef DEBUG
  fprintf (stderr, "Identified identifier %s\n", yytext);
#endif
  yylval.integer = find_var_name (yytext);
  return ((yytext[yyleng - 1] == '$') ? STRINGIDENTIFIER : IDENTIFIER);
}

[\t ]+	/* Ignore whitespace, except to separate tokens */

[^-\t\n \"$()*+,./:;<=>0-9A-Z^].*	{
  /* Any unrecognized character should result in
   * the entire line being an error. */
  printf ("ERROR- %s\n", yytext);
  return RESTOFLINE;
}

(".")|("."[^\n0-9].*)	{
  /* `Floating-point number' patterns without any digits (i.e.,
   * a single period) should fall through here. */
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
    yyterminate();
  }
  else {
#ifdef DEBUG
    fprintf (stderr, "End of file reached; switching back to the previous input\n");
#endif
    yypop_buffer_state();
  }
}
