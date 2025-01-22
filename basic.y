/* A bison grammar file describing the BASIC language */
%{
  /* C declarations */
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tables.h"
extern int yylex (void);
void yyerror (const char *);
extern jmp_buf return_point;
static unsigned short *add_tokens (const char *, ...);
static void dump_tokens (unsigned short *);
#define YYERROR_VERBOSE
#define YYDEBUG 1
%}

/* Bison declarations */
%union {
  unsigned long integer;
  double floating_point;
  struct string_value *string;
  struct list_header *token_list;
  struct statement_header *token_statement;
  struct line_header *token_line;
  /* Generic pointer to tokenized data;
   * the first element is always the current length. */
  unsigned short *tokens;
}
/* BASIC keywords */
%token BYE
%token CONTINUE
%token DATA
%token DEF
%token DIM
%token ELSE
%token END
%token FOR
%token GOSUB
%token GOTO
%token IF
%token INPUT
%token LET
%token _LET_	/* Implied `LET' */
%token LIST
%token LOAD
%token NEW
%token NEXT
%token ON
%token PRINT
%token READ
%token REM
%token <string> RESTOFLINE
%token RESTORE
%token RETURN
%token RUN
%token SAVE
%token STEP
%token STOP
%token THEN
%token TO
/* Arithmetic operators */
%left OR
%left AND
%token LESSEQ "<="
%token GRTREQ ">="
%token NOTEQ "<>"
%nonassoc LESSEQ "<="
%nonassoc GRTREQ ">="
%nonassoc NOTEQ "<>"
%left '+' '-'
%left '*' '/'
%left '^'
%right NEG
/* Non-keyword tokens */
%token <floating_point> FLOATINGPOINT
%token <integer> IDENTIFIER
%token <integer> STRINGIDENTIFIER
%token <integer> INTEGER
%token <string> STRING
%token TAB
/* These tokens are used in the parser, not the lexical scanner */
%token <token_list> ITEMLIST
%token <tokens> NUMEXPR  /* Numeric expression */
%token <tokens> STREXPR  /* String expression */
%token <tokens> NUMLVAL  /* Numeric variable to assign (pass by reference) */
%token <tokens> STRLVAL  /* String variable to assign (pass by reference) */

%%
/* Grammar rules */
session:                /* Terminate */
    | session line      /* Continue */
    ;

line: '\n'              /* Do nothing */
	{ ; }
    | INTEGER lineofcode '\n' /* Add/change a line in the BASIC program */
	{
	  /* Complete the line of code with a newline */
	  $<tokens>$ = add_tokens ("*t", $<tokens>2, '\n');
	  /* Change the line number */
	  $<token_line>$->line_number = $<integer>1;
#ifdef DEBUG
	  fprintf (stderr, "Adding line %d to program\n", $<integer>1);
#endif
	  add_line ($<token_line>$);
	  /* DO NOT delete this line, since it is now part of the program. */
	}
    | INTEGER '\n'      /* Delete a line in the BASIC program */
	{
#ifdef DEBUG
	  fprintf (stderr, "Deleting line %d from program\n", $<integer>1);
#endif
	  remove_line ($<integer>1);
	}
    | lineofcode '\n'   /* Execute a line of code immediately */
	{
	  /* Complete the line of code with a newline */
	  $<tokens>$ = add_tokens ("*t", $<tokens>1, '\n');
#ifdef DEBUG
	  fprintf (stderr, "Executing line of BASIC code\n");
#endif
	  /* Execute this line */
	  execute ($<token_line>$);
	  /* We're done with this line, so free the token array */
	  free ($<token_line>$);
	}
    | error '\n' { /* yyerrok */; } /* Continue reading after an error */
    ;

lineofcode: statement	/* Create a new line */
	{
#ifdef DEBUG
	  fprintf (stderr, "Create a new line from the first statement\n");
#endif
	  /* ELSE and IF statements need to be handled specially,
	   * because they actually contain two statements
	   * preceded with a dummy statement. */
	  if ($<token_statement>1->command == ELSE)
	    {
	      $<tokens>$ = add_tokens
		("it&t&", -1, $<token_statement>1[1].length,
		 &$<token_statement>1[1],
		 ((struct statement_header *)
		  ((unsigned long) &$<token_statement>1[1]
		   + $<token_statement>1[1].length))->length + sizeof (short),
		 ((unsigned long) &$<token_statement>1[1]
		  + $<token_statement>1[1].length));
	      free ($<tokens>1);
	    }
	  else
	    {
	      /* Save the length of the completed statement +1,
	       * as the end-of-statement marker will be included later. */
	      $<tokens>$ = add_tokens
		("it*", -1, $<tokens>1[0] + sizeof (short), $<tokens>1);
	    }
	}
    | lineofcode ':' statement /* Statements are separated by colons */
	{
	  /* Enlarge the current line by the size of the next statement.
	   * Append a colon to complete the previous statement.
	   * Save the length of the new statement +1, as its
	   * end-of-statement marker will be included later. */
#ifdef DEBUG
	  fprintf (stderr, "Added another statement to the line\n");
#endif
	  /* ELSE and IF statements need to be handled specially,
	   * because they actually contain two statements
	   * preceded with a dummy statement. */
	  if ($<token_statement>3->command == ELSE)
	    {
	      $<tokens>$ = add_tokens
		("*tt&t&", $<tokens>1, ':', $<token_statement>3[1].length,
		 &$<token_statement>3[1],
		 ((struct statement_header *)
		  ((unsigned long) &$<token_statement>3[1]
		   + $<token_statement>3[1].length))->length + sizeof (short),
		 ((unsigned long) &$<token_statement>3[1]
		  + $<token_statement>3[1].length));
	      free ($<tokens>3);
	    }
	  else
	    {
	      /* Save the length of the completed statement +1,
	       * as the end-of-statement marker will be included later. */
	      $<tokens>$ = add_tokens
		("*tt#", $<tokens>1, ':',
		 $<tokens>3[0] + sizeof (short), $<tokens>3);
	    }
	}
    ;

/* This section describes all BASIC commands, and the various
 * syntaxes for each.  The first line is an implied `LET'. */
statement: assignment
	{
	  int len;

#ifdef DEBUG
	  fprintf (stderr, "Assign a value to a variable\n");
#endif
	  /* Convert the expression token list into a statement */
	  $<tokens>$ = add_tokens ("t*", _LET_, $<tokens>1);
	}
    | BYE		/* Bye-bye */
	{
#ifdef DEBUG
	  fprintf (stderr, "Bye-bye\n");
#endif
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("t", BYE);
	}
    | CONTINUE          /* Continue program execution from last `STOP' */
	{
#ifdef DEBUG
	  fprintf (stderr, "Continue program from where we left off\n");
#endif
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("t", CONTINUE);
	}
    | DATA datalist     /* Define data to be read with `READ' */
	{
#ifdef DEBUG
	  fprintf (stderr, "Data list defined\n");
#endif
	  /* Convert the data token list into a statement */
	  $<tokens>$ = add_tokens ("t*", DATA, $<tokens>2);
	}
    | DEF numfuncdecl '=' numexpression /* Define a numeric function */
	{
#ifdef DEBUG
	  fprintf (stderr, "Numeric function defined\n");
#endif
	  /* convert the expression token lists into a statement */
	  $<tokens>$ = add_tokens ("tt*tt#", DEF, NUMLVAL, $<tokens>2,
				   '=', NUMEXPR, $<tokens>4);
	}
    | DEF strfuncdecl '=' stringexpression /* Define a string function */
	{
#ifdef DEBUG
	  fprintf (stderr, "String function defined\n");
#endif
	  /* convert the expression token lists into a statement */
	  $<tokens>$ = add_tokens ("tt*tt#", DEF, STRLVAL, $<tokens>2,
				   '=', STREXPR, $<tokens>4);
	}
    | DIM dimlist       /* Define the size of array variables */
	{
#ifdef DEBUG
	  fprintf (stderr, "Array variable defined\n");
#endif
	  /* Convert the variable token list into a statement */
	  $<tokens>$ = add_tokens ("t*", DIM, $<tokens>2);
	}
    | ELSE statement    /* Execute here iff previous `IF' expr was false */
	{
#ifdef DEBUG
	  fprintf (stderr, "Execute this statement iff the previous IF result was false\n");
#endif
	  /* Extend the current statement to precede it with `ELSE'.
	   * Since we need to keep `ELSE' separate from the following
	   * statement, precede this with a dummy statement. */
	  $<tokens>$ = add_tokens
	    ("tttt#", ELSE, sizeof (struct statement_header), ELSE,
	     $<token_statement>2->length + sizeof (short), $<tokens>2);
	}
    | END               /* Stop program execution */
	{
#ifdef DEBUG
	  fprintf (stderr, "End the program's execution\n");
#endif
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("t", END);
	}
    | FOR simplenumvar '=' arithexpr TO arithexpr
	{
#ifdef DEBUG
	  fprintf (stderr,
		   "Begin an iteration of the following statements up to NEXT\n");
#endif
	  /* Condense all token lists into one statement */
	  $<tokens>$ = add_tokens ("tt*tt#tt#", FOR, NUMLVAL, $<tokens>2,
				   '=', NUMEXPR, $<tokens>4, TO, NUMEXPR,
				   $<tokens>6);
	}
    | FOR simplenumvar '=' arithexpr TO arithexpr STEP arithexpr
	{
#ifdef DEBUG
	  fprintf (stderr,
		   "Begin an iteration using an alternative increment\n");
#endif
	  /* Condense all token lists into one statement */
	  $<tokens>$ = add_tokens ("tt*tt#tt#tt#", FOR, NUMLVAL, $<tokens>2,
				   '=', NUMEXPR, $<tokens>4, TO, NUMEXPR,
				   $<tokens>6, STEP, NUMEXPR, $<tokens>8);
	}
    | GOSUB arithexpr   /* Save the current position, then transfer */
	{
#ifdef DEBUG
	  fprintf (stderr, "Go to a subroutine\n");
#endif
	  /* Convert the expression token list into a statement */
	  $<tokens>$ = add_tokens ("t*", GOSUB, $<tokens>2);
	}
    | GOTO arithexpr    /* Transfer control to a different line */
	{
#ifdef DEBUG
	  fprintf (stderr, "Go to another line in the program\n");
#endif
	  /* Convert the expression token list into a statement */
	  $<tokens>$ = add_tokens ("t*", GOTO, $<tokens>2);
	}
    | IF numexpression THEN INTEGER /* `GOTO' a line if expression is true */
	{
#ifdef DEBUG
	  fprintf (stderr, "Go to another line if this expression is true\n");
#endif
	  /* Convert the expression token list into a statement */
	  $<tokens>$ = add_tokens ("t*tti", IF, $<tokens>2, THEN,
				   INTEGER, $<integer>4);
	}
    | IF numexpression THEN statement /* Continue execution if expr is true */
	{
#ifdef DEBUG
	  fprintf (stderr,
		   "Execute the next statement if this expression is true\n");
#endif
	  /* Convert the expression into a statement. */
	  $<tokens>$ = add_tokens ("t*t", IF, $<tokens>2, THEN);
	  /* Attach the following statement.
	   * Since we need to keep `IF-THEN' separate from the following
	   * statement, precede this with a dummy statement. */
	  $<tokens>$ = add_tokens
	    ("tt*t#", ELSE, $<tokens>$[0], $<tokens>$,
	     $<token_statement>4->length, $<tokens>4);
	}
    | INPUT varlist /* Read a value from the terminal and store in vars */
	{
#ifdef DEBUG
	  fprintf (stderr, "Read a value from the user\n");
#endif
	  /* Convert the variable token list into a statement */
	  $<tokens>$ = add_tokens ("t*", INPUT, $<tokens>2);
	}
    | INPUT STRING ';' varlist /* Print a prompt, then read a value to vars */
	{
#ifdef DEBUG
	  fprintf (stderr,
		   "Print a prompt %s and read a value from the user\n",
		   $<string>2->contents);
#endif
	  /* Convert the variable list into a statement */
	  $<tokens>$ = add_tokens ("ttst*", INPUT, STRING, $<string>2,
				   ';', $<tokens>4);
	}
    | LET assignment    /* Assign a value to a variable */
	{
#ifdef DEBUG
	  fprintf (stderr, "Assign a value to a variable\n");
#endif
	  /* Convert the expression token list into a statement */
	  $<tokens>$ = add_tokens ("t*", LET, $<tokens>2);
	}
    | LIST              /* Display the entire program */
	{
#ifdef DEBUG
	  fprintf (stderr, "List the entire program\n");
#endif
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("t", LIST);
	}
    | LIST INTEGER      /* Display one line of the program */
	{
#ifdef DEBUG
	  fprintf (stderr, "List line %d of the program\n", $<integer>2);
#endif
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("tti", LIST, INTEGER, $<integer>2);
	}
    | LIST INTEGER ',' INTEGER /* Display a range of lines of the program */
	{
#ifdef DEBUG
	  fprintf (stderr,
		   "List a portion of the program from line %d to line %d\n",
		   $<integer>2, $<integer>4);
#endif
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("ttitti", LIST, INTEGER, $<integer>2,
				   ',', INTEGER, $<integer>4);
	}
    | LOAD STRING       /* Read input from a file */
	{
#ifdef DEBUG
	  fprintf (stderr, "Read a program from file %s\n",
		   $<string>2->contents);
#endif
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("tts", REM, STRING, $<string>2);
	}
    | NEW               /* Erase the current program from memory */
	{
#ifdef DEBUG
	  fprintf (stderr, "Erase the program and start over\n");
#endif
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("t", NEW);
	}
    | NEXT simplenumvar /* Step a variable; transfer control to `FOR' */
	{
#ifdef DEBUG
	  fprintf (stderr,
		   "Do the next iteration of the statement following FOR\n");
#endif
	  /* Convert the variable token list into a statement */
	  $<tokens>$ = add_tokens ("t*", NEXT, $<tokens>2);
	}
    | ON numvariable GOSUB linelist /* `GOSUB' the Nth line in the list */
	{
#ifdef DEBUG
	  fprintf (stderr, "Compute the line to go subroutine\n");
#endif
	  /* Make a statement out of the existing tokens */
	  $<tokens>$ = add_tokens ("t*t#", ON, $<tokens>2, GOSUB, $<tokens>4);
	}
    | ON numvariable GOTO linelist /* `GOTO' the Nth line in the list */
	{
#ifdef DEBUG
	  fprintf (stderr, "Compute the line to go to\n");
#endif
	  /* Make a statement out of the existing tokens */
	  $<tokens>$ = add_tokens ("t*t#", ON, $<tokens>2, GOTO, $<tokens>4);
	}
    | PRINT             /* Print a blank line */
	{
#ifdef DEBUG
	  fprintf (stderr, "Skip a line of output\n");
#endif
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("t", PRINT);
	}
    | PRINT printlist   /* Print something */
	{
#ifdef DEBUG
	  fprintf (stderr, "Print a line\n");
#endif
	  /* Make a statement out of the list */
	  $<tokens>$ = add_tokens ("t*", PRINT, $<tokens>2);
	}
    | PRINT printlist ';' /* Print something, but don't follow with NL */
	{
#ifdef DEBUG
	  fprintf (stderr, "Print a line and stay at the line's end\n");
#endif
	  /* Make a statement out of the list */
	  $<tokens>$ = add_tokens ("t*t", PRINT, $<tokens>2, ';');
	}
    | PRINT printlist ',' /* Print something, follow with tab instead of NL */
	{
#ifdef DEBUG
	  fprintf (stderr, "Print a line and tab over a bit from the end\n");
#endif
	  /* Make a statement out of the list */
	  $<tokens>$ = add_tokens ("t*t", PRINT, $<tokens>2, ',');
	}
    | READ varlist  /* Read next item from a `DATA' line, store in var */
	{
#ifdef DEBUG
	  fprintf (stderr, "Read the next DATA item\n");
#endif
	  /* Make a statement out of the list */
	  $<tokens>$ = add_tokens ("t*", READ, $<tokens>2);
	}
    | REM RESTOFLINE    /* Comment -- store in program, but do nothing */
	{
#ifdef DEBUG
	  fprintf (stderr, "Remark noted: %s\n", $<string>2->contents);
#endif
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("tts", REM, RESTOFLINE, $<string>2);
	}
    | RESTORE           /* Reset `DATA' pointer to the beginning */
	{
#ifdef DEBUG
	  fprintf (stderr,
		   "Restore DATA pointer to the beginning of the program\n");
#endif
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("t", RESTORE);
	}
    | RESTORE arithexpr /* Reset `DATA' pointer to a given line */
	{
#ifdef DEBUG
	  fprintf (stderr,
		   "Restore DATA pointer to a specific line in the program\n");
#endif
	  /* Convert the expression token list into a statement */
	  $<tokens>$ = add_tokens ("t*", RESTORE, $<tokens>2);
	}
    | RETURN            /* Transfer control back to previous `GOSUB' */
	{
#ifdef DEBUG
	  fprintf (stderr,
		   "Return to the statement following the last GOSUB call\n");
#endif
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("t", RETURN);
	}
    | RUN               /* Reset program counters and execute program */
	{
#ifdef DEBUG
	  fprintf (stderr, "Run the program from the beginning\n");
#endif
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("t", RUN);
	}
    | SAVE STRING       /* List the entire program to a file */
	{
#ifdef DEBUG
	  fprintf (stderr, "Write the program listing to file %s\n",
		   $<string>2->contents);
#endif
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("tts", SAVE, STRING, $<string>2);
	}
    | STOP              /* Stop program execution temporarily */
	{
#ifdef DEBUG
	  fprintf (stderr, "Stop the program at this point\n");
#endif
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("t", STOP);
	}
    ;

/* Break down the different lists */
datalist: anyconstant
    {
      /* Make the constant into a new token list. */
      $<tokens>$ = add_tokens
	("tttt*", ITEMLIST, 0, 1,
	 sizeof (struct list_item) + $<tokens>1[0] - sizeof (short),
	 $<tokens>1);
      ((struct list_header *) &$<tokens>$[2])->length
	= $<tokens>$[0] - 2 * sizeof (short);
    }
    | datalist ',' anyconstant
    {
      /* Enlarge the array to include the next constant and comma. */
      $<tokens>$ = add_tokens
	("*tt#", $<tokens>1, ',',
	 sizeof (struct list_item) + $<tokens>3[0] - sizeof (short),
	 $<tokens>3);
      ((struct list_header *) &$<tokens>$[2])->length
	= $<tokens>$[0] - 2 * sizeof (short);
      ((struct list_header *) &$<tokens>$[2])->num_items++;
    }
    ;

dimlist: dimdecl
    {
      /* Make the variable into a new token list. */
      $<tokens>$ = add_tokens
	("tttt*", ITEMLIST, 0, 1,
	 sizeof (struct list_item) + $<tokens>1[0] - sizeof (short),
	 $<tokens>1);
      ((struct list_header *) &$<tokens>$[2])->length
	= $<tokens>$[0] - 2 * sizeof (short);
    }
    | dimlist ',' dimdecl
    {
      /* Enlarge the array to include the next variable and comma. */
      $<tokens>$ = add_tokens
	("*tt#", $<tokens>1, ',',
	 sizeof (struct list_item) + $<tokens>3[0] - sizeof (short),
	 $<tokens>3);
      ((struct list_header *) &$<tokens>$[2])->length
	= $<tokens>$[0] - 2 * sizeof (short);
      ((struct list_header *) &$<tokens>$[2])->num_items++;
    }
    ;

linelist: INTEGER
    {
#ifdef DEBUG
      fprintf (stderr, "Line number %u\n", $<integer>1);
#endif
      /* Create a new list */
      $<tokens>$ = add_tokens
	("ttttti", ITEMLIST, 0, 1,
	 sizeof (struct list_item) + sizeof (short) + sizeof (long),
	 INTEGER, $<integer>1);
      ((struct list_header *) &$<tokens>$[2])->length
	= $<tokens>$[0] - 2 * sizeof (short);
    }
    | linelist ',' INTEGER
    {
#ifdef DEBUG
      fprintf (stderr, "Line number %u\n", $<integer>3);
#endif
      /* Extend the list with a new line number */
      $<tokens>$ = add_tokens
	("*ttti", $<tokens>1, ',',
	 sizeof (struct list_item) + sizeof (short) + sizeof (long),
	 INTEGER, $<integer>3);
      ((struct list_header *) &$<tokens>$[2])->length
	= $<tokens>$[0] - 2 * sizeof (short);
      ((struct list_header *) &$<tokens>$[2])->num_items++;
    }
    ;

printlist: printitem
    {
      /* Make the print item into a new token list. */
      $<tokens>$ = add_tokens
	("tttt*", ITEMLIST, 0, 1,
	 sizeof (struct list_item) + $<tokens>1[0] - sizeof (short),
	 $<tokens>1);
      ((struct list_header *) &$<tokens>$[2])->length
	= $<tokens>$[0] - 2 * sizeof (short);
    }
    | printlist ';' printitem
    {
      /* Enlarge the array to include the next print item and semicolon. */
      $<tokens>$ = add_tokens
	("*tt#", $<tokens>1, ';',
	 sizeof (struct list_item) + $<tokens>3[0] - sizeof (short),
	 $<tokens>3);
      ((struct list_header *) &$<tokens>$[2])->length
	= $<tokens>$[0] - 2 * sizeof (short);
      ((struct list_header *) &$<tokens>$[2])->num_items++;
    }
    | printlist ',' printitem
    {
      /* Enlarge the array to include the next print item and comma. */
      $<tokens>$ = add_tokens
	("*tt#", $<tokens>1, ',',
	 sizeof (struct list_item) + $<tokens>3[0] - sizeof (short),
	 $<tokens>3);
      ((struct list_header *) &$<tokens>$[2])->length
	= $<tokens>$[0] - 2 * sizeof (short);
      ((struct list_header *) &$<tokens>$[2])->num_items++;
    }
    ;

arglist: simplevar
    {
      /* Make the variable into a new token list. */
      $<tokens>$ = add_tokens
	("tttt*", ITEMLIST, 0, 1,
	 sizeof (struct list_item) + $<tokens>1[0] - sizeof (short),
	 $<tokens>1);
      ((struct list_header *) &$<tokens>$[2])->length
	= $<tokens>$[0] - 2 * sizeof (short);
    }
    | arglist ',' simplevar
    {
      /* Enlarge the array to include the next variable and comma. */
      $<tokens>$ = add_tokens
	("*tt#", $<tokens>1, ',',
	 sizeof (struct list_item) + $<tokens>3[0] - sizeof (short),
	 $<tokens>3);
      ((struct list_header *) &$<tokens>$[2])->length
	= $<tokens>$[0] - 2 * sizeof (short);
      ((struct list_header *) &$<tokens>$[2])->num_items++;
      /* The number of indices must not exceed 32 */
      if (((struct list_header *) &$<tokens>$[2])->num_items > 32)
	{
	  fputs ("ERROR - TOO MANY ARGUMENTS\n", stderr);
	  YYERROR;
	}
    }
    ;

varlist: anyvariable
    {
      /* Make the variable into a new token list. */
      $<tokens>$ = add_tokens
	("tttt*", ITEMLIST, 0, 1,
	 sizeof (struct list_item) + $<tokens>1[0] - sizeof (short),
	 $<tokens>1);
      ((struct list_header *) &$<tokens>$[2])->length
	= $<tokens>$[0] - 2 * sizeof (short);
    }
    | varlist ',' anyvariable
    {
      /* Enlarge the array to include the next variable and comma. */
      $<tokens>$ = add_tokens
	("*tt#", $<tokens>1, ',',
	 sizeof (struct list_item) + $<tokens>3[0] - sizeof (short),
	 $<tokens>3);
      ((struct list_header *) &$<tokens>$[2])->length
	= $<tokens>$[0] - 2 * sizeof (short);
      ((struct list_header *) &$<tokens>$[2])->num_items++;
    }
    ;

/* Define the meaning of an `expression' */
assignment: numvariable '=' arithexpr
    {
      /* Enlarge the first token list to include the second
       * and the equals sign */
      $<tokens>$ = add_tokens ("t*tt#", NUMLVAL, $<tokens>1, '=',
			       NUMEXPR, $<tokens>3);
    }
    | strvariable '=' stringexpression
    {
      /* Enlarge the first token list to include the second
       * and the equals sign */
      $<tokens>$ = add_tokens ("t*tt#", STRLVAL, $<tokens>1, '=',
			       STREXPR, $<tokens>3);
    }
    ;

printitem: anyexpression
      /* `TAB()' is a special case -- it's a string function without a `$',
       * but it only makes sense in `PRINT' statements anyway. */
    | TAB '(' numexpression ')'
    {
#ifdef DEBUG
      fprintf (stderr, "Spacing over to tab stop\n");
#endif
      /* Enlarge the token list to include `TAB' and both parentheses. */
      $<tokens>$ = add_tokens ("tt*t", TAB, '(', $<tokens>3, ')');
    }
    ;

anyexpression: numexpression
    {
      $<tokens>$ = add_tokens ("t*", NUMEXPR, $<tokens>1);
    }
    | stringexpression
    {
      $<tokens>$ = add_tokens ("t*", STREXPR, $<tokens>1);
    }
    ;

numexpression: arithexpr | condexpr ;

stringexpression: STRING
    {
#ifdef DEBUG
      fprintf (stderr, "String constant %s\n", $<string>1->contents);
#endif
      /* Start a new token list */
      $<tokens>$ = add_tokens ("ts", STRING, $<string>1);
    }
    | strvariable
    | stringfunction
    | stringexpression '+' stringexpression
    {
#ifdef DEBUG
      fprintf (stderr, "Concatenate two strings\n");
#endif
      /* Enlarge the first token list to include the second
       * and the plus sign */
      $<tokens>$ = add_tokens ("*t#", $<tokens>1, '+', $<tokens>3);
    }
    ;

arithexpr: number
    | numvariable
    | '(' numexpression ')'
    {
      /* Enlarge the token list to include both parentheses. */
      $<tokens>$ = add_tokens ("t*t", '(', $<tokens>2, ')');
    }
    | numfunction
    | arithexpr '+' arithexpr
    {
#ifdef DEBUG
      fprintf (stderr, "Adding two numbers; new tokens are ");
#endif
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens ("t*t#t", '{', $<tokens>1, '+', $<tokens>3, '}');
    }
    | arithexpr '-' arithexpr
    {
#ifdef DEBUG
      fprintf (stderr, "Subtracting two numbers; new tokens are ");
#endif
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens ("t*t#t", '{', $<tokens>1, '-', $<tokens>3, '}');
    }
    | arithexpr '*' arithexpr
    {
#ifdef DEBUG
      fprintf (stderr, "Multiplying two numbers; ");
#endif
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens ("t*t#t", '{', $<tokens>1, '*', $<tokens>3, '}');
    }
    | arithexpr '/' arithexpr
    {
#ifdef DEBUG
      fprintf (stderr, "Dividing two numbers; ");
#endif
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens ("t*t#t", '{', $<tokens>1, '/', $<tokens>3, '}');
    }
    | arithexpr '^' arithexpr
    {
#ifdef DEBUG
      fprintf (stderr, "Expenentiating two numbers; ");
#endif
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens ("t*t#t", '{', $<tokens>1, '^', $<tokens>3, '}');
    }
    | '-' arithexpr %prec NEG
    {
#ifdef DEBUG
      fprintf (stderr, "Negating a number; ");
#endif
      /* Enlarge the token list to include the operator
       * and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens ("tt*t", '{', NEG, $<tokens>2, '}');
    }
    ;

condexpr:
      arithexpr '=' arithexpr
    {
#ifdef DEBUG
      fprintf (stderr, "Comparing two numbers for equality\n");
#endif
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("tt*t#t", '{', NUMEXPR, $<tokens>1, '=', $<tokens>3, '}');
    }
    | arithexpr '<' arithexpr
    {
#ifdef DEBUG
      fprintf (stderr, "Comparing two numbers for less\n");
#endif
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("tt*t#t", '{', NUMEXPR, $<tokens>1, '<', $<tokens>3, '}');
    }
    | arithexpr '>' arithexpr
    {
#ifdef DEBUG
      fprintf (stderr, "Comparing two numbers for more\n");
#endif
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("tt*t#t", '{', NUMEXPR, $<tokens>1, '>', $<tokens>3, '}');
    }
    | arithexpr "<=" arithexpr
    {
#ifdef DEBUG
      fprintf (stderr, "Comparing two numbers for not more\n");
#endif
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("tt*t#t", '{', NUMEXPR, $<tokens>1, LESSEQ, $<tokens>3, '}');
    }
    | arithexpr ">=" arithexpr
    {
#ifdef DEBUG
      fprintf (stderr, "Comparing two numbers for not less\n");
#endif
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("tt*t#t", '{', NUMEXPR, $<tokens>1, GRTREQ, $<tokens>3, '}');
    }
    | arithexpr "<>" arithexpr
    {
#ifdef DEBUG
      fprintf (stderr, "Comparing two numbers for inequality\n");
#endif
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("tt*t#t", '{', NUMEXPR, $<tokens>1, NOTEQ, $<tokens>3, '}');
    }
    | stringexpression '=' stringexpression
    {
#ifdef DEBUG
      fprintf (stderr, "Comparing two strings for equality\n");
#endif
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("tt*t#t", '{', STREXPR, $<tokens>1, '=', $<tokens>3, '}');
    }
    | stringexpression '<' stringexpression
    {
#ifdef DEBUG
      fprintf (stderr, "Comparing two strings for before\n");
#endif
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("tt*t#t", '{', STREXPR, $<tokens>1, '<', $<tokens>3, '}');
    }
    | stringexpression '>' stringexpression
    {
#ifdef DEBUG
      fprintf (stderr, "Comparing two strings for after\n");
#endif
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("tt*t#t", '{', STREXPR, $<tokens>1, '>', $<tokens>3, '}');
    }
    | stringexpression "<=" stringexpression
    {
#ifdef DEBUG
      fprintf (stderr, "Comparing two strings for not after\n");
#endif
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("tt*t#t", '{', STREXPR, $<tokens>1, LESSEQ, $<tokens>3, '}');
    }
    | stringexpression ">=" stringexpression
    {
#ifdef DEBUG
      fprintf (stderr, "Comparing two strings not before\n");
#endif
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("tt*t#t", '{', STREXPR, $<tokens>1, GRTREQ, $<tokens>3, '}');
    }
    | stringexpression "<>" stringexpression
    {
#ifdef DEBUG
      fprintf (stderr, "Comparing two strings for inequality\n");
#endif
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("tt*t#t", '{', STREXPR, $<tokens>1, NOTEQ, $<tokens>3, '}');
    }
    | condexpr AND condexpr
    {
#ifdef DEBUG
      fprintf (stderr, "Make sure both conditions are true\n");
#endif
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("t*t#t", '{', $<tokens>1, AND, $<tokens>3, '}');
    }
    | condexpr OR condexpr
    {
#ifdef DEBUG
      fprintf (stderr, "Succeed if either condition is true\n");
#endif
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("t*t#t", '{', $<tokens>1, OR, $<tokens>3, '}');
    }
    ;

number: INTEGER
    {
#ifdef DEBUG
      fprintf (stderr, "Integer %u; ", $<integer>1);
#endif
      /* Start a new token list */
      $<tokens>$ = add_tokens ("ti", INTEGER, $<integer>1);
    }
    | FLOATINGPOINT
    {
#ifdef DEBUG
      fprintf (stderr, "Floating-point number %G; ", $<floating_point>1);
#endif
      /* Start a new token list */
      $<tokens>$ = add_tokens ("tf", FLOATINGPOINT, $<floating_point>1);
    }
    ;

anyconstant: number
    | STRING
    {
#ifdef DEBUG
      fprintf (stderr, "String constant %s; ", $<string>1->contents);
#endif
      /* Start a new token list */
      $<tokens>$ = add_tokens ("ts", STRING, $<string>1);
    }
    ;

/* Define variable types */
anyvariable: numvariable | strvariable ;

numvariable: simplenumvar
    | IDENTIFIER '(' arrayindex ')'
    {
#ifdef DEBUG
      fprintf (stderr, "Numeric array element in %s; ",
	       name_table[$<integer>1]->contents);
#endif
      /* Enlarge the arrayindex token list to include the identifier
       * and parentheses tokens */
      $<tokens>$ = add_tokens
	("ttt*t", IDENTIFIER, $<integer>1, '(', $<tokens>3, ')');
    }
    ;

strvariable: simplestringvar
    | STRINGIDENTIFIER '(' arrayindex ')'
    {
#ifdef DEBUG
      fprintf (stderr, "String array element in %s; ",
	       name_table[$<integer>1]->contents);
#endif
      /* Enlarge the arrayindex token list to include the identifier
       * and parentheses tokens */
      $<tokens>$ = add_tokens
	("ttt*t", STRINGIDENTIFIER, $<integer>1, '(', $<tokens>3, ')');
    }
    ;

simplevar: simplenumvar | simplestringvar ;

simplenumvar: IDENTIFIER
    {
#ifdef DEBUG
      fprintf (stderr, "Numeric variable %s; ",
	       name_table[$<integer>1]->contents);
#endif
      /* Start a new token list */
      $<tokens>$ = add_tokens ("tt", IDENTIFIER, $<integer>1);
    }
    ;

simplestringvar: STRINGIDENTIFIER
    {
#ifdef DEBUG
      fprintf (stderr, "String variable %s; ",
	       name_table[$<integer>1]->contents);
#endif
      /* Start a new token list */
      $<tokens>$ = add_tokens ("tt", STRINGIDENTIFIER, $<integer>1);
    }
    ;

arrayindex: numexpression
    {
      /* Make the expression into a new token list. */
      $<tokens>$ = add_tokens
	("tttt*", ITEMLIST, 0, 1,
	 sizeof (struct list_item) + $<tokens>1[0] - sizeof (short),
	 $<tokens>1);
      ((struct list_header *) &$<tokens>$[2])->length
	= $<tokens>$[0] - 2 * sizeof (short);
    }
    | arrayindex ',' numexpression
    {
      /* Enlarge the array to include the next expression and comma. */
      $<tokens>$ = add_tokens
	("*tt#", $<tokens>1, ',',
	 sizeof (struct list_item) + $<tokens>3[0] - sizeof (short),
	 $<tokens>3);
      ((struct list_header *) &$<tokens>$[2])->length
	= $<tokens>$[0] - 2 * sizeof (short);
      ((struct list_header *) &$<tokens>$[2])->num_items++;
      /* The number of indices must not exceed 32 */
      if (((struct list_header *) &$<tokens>$[2])->num_items > 32)
	{
	  fputs ("ERROR - TOO MANY DIMENSIONS\n", stderr);
	  YYERROR;
	}
    }
    ;

/* Define functions.  Note that a function and an array are ambiguous;
 * the only way to tell them apart is by their declaration. */
numfunction: IDENTIFIER '(' arguments ')'
    {
#ifdef DEBUG
      fprintf (stderr, "Calling numeric function %s\n",
	       name_table[$<integer>1]->contents);
#endif
      $<tokens>$ = add_tokens ("ttt*t", IDENTIFIER,
			       $<integer>1, '(', $<tokens>3, ')');
    }
    ;

stringfunction:
      STRINGIDENTIFIER '(' arguments ')'
    {
#ifdef DEBUG
      fprintf (stderr, "Calling string function %s\n",
	       name_table[$<integer>1]->contents);
#endif
      $<tokens>$ = add_tokens ("ttt*t", STRINGIDENTIFIER,
			       $<integer>1, '(', $<tokens>3, ')');
    }
    ;

arguments: anyexpression
    {
      /* Make a new list */
      $<tokens>$ = add_tokens
	("tttt*", ITEMLIST, 0, 1,
	 sizeof (struct list_item) + $<tokens>1[0] - sizeof (short),
	 $<tokens>1);
      ((struct list_header *) &$<tokens>$[2])->length
	= $<tokens>$[0] - 2 * sizeof (short);
    }
    | arguments ',' anyexpression
    {
      /* Add a new item to the list */
      $<tokens>$ = add_tokens
	("*tt#", $<tokens>1, ',',
	 sizeof (struct list_item) + $<tokens>3[0] - sizeof (short),
	 $<tokens>3);
      ((struct list_header *) &$<tokens>$[2])->length
	= $<tokens>$[0] - 2 * sizeof (short);
      ((struct list_header *) &$<tokens>$[2])->num_items++;
    }
    ;

/* Define declarations */

numfuncdecl: IDENTIFIER '(' arglist ')'
    {
#ifdef DEBUG
      fprintf (stderr, "Declare numeric function %s\n",
	       name_table[$<integer>1]->contents);
#endif
      $<tokens>$ = add_tokens ("ttt*t", IDENTIFIER,
			       $<integer>1, '(', $<tokens>3, ')');
    }
    ;

strfuncdecl: STRINGIDENTIFIER '(' arglist ')'
    {
#ifdef DEBUG
      fprintf (stderr, "Declare string function %s\n",
	       name_table[$<integer>1]->contents);
#endif
      $<tokens>$ = add_tokens ("ttt*t", STRINGIDENTIFIER,
			       $<integer>1, '(', $<tokens>3, ')');
    }
    ;

dimdecl: IDENTIFIER '(' arrayindex ')'
    {
#ifdef DEBUG
      fprintf (stderr, "Declare numeric array %s\n",
	       name_table[$<integer>1]->contents);
#endif
      $<tokens>$ = add_tokens ("ttt*t", IDENTIFIER,
			       $<integer>1, '(', $<tokens>3, ')');
    }
    | STRINGIDENTIFIER '(' arrayindex ')'
    {
#ifdef DEBUG
      fprintf (stderr, "Declare string array %s\n",
	       name_table[$<integer>1]->contents);
#endif
      $<tokens>$ = add_tokens ("ttt*t", STRINGIDENTIFIER,
			       $<integer>1, '(', $<tokens>3, ')');
    }
    ;

%%

/* Additional C code */

void
yyerror (const char *msg)
{
  fprintf (stderr, "\007Parse error: %s\n", msg);
  /*  fputs ("Aborting line\n", stderr);
      longjmp (return_point, 1); */
}

/* Add a number of tokens to an existing token list.
 * The list is reallocated to accomodate the new tokens,
 * and the size (at the head of the list) adjusted.
 * The pattern indicates what the tokens are and in what order
 * place them:
 *	* - the original list (must be present no more than once)
 *	t - short token (character or token name)
 *	i - long integer
 *	f - floating-point number
 *	s - string (struct string_value *)
 *	# - token list (unsigned short *, first element is length)
 *	& - token list - same as above, but don't free the pointer
 */
static unsigned short *
add_tokens (const char *pattern, ...)
{
  va_list vp;
  unsigned short *tp, *new_list, *old_list = NULL;
  int i, len, old_len = 0, move_len = 0;

#ifdef DEBUG
  fprintf (stderr, "add_tokens() called with pattern \"%s\"\n", pattern);
#endif

  /* Run through the pattern and figure out where things will go. */
  va_start (vp, pattern);
  for (len = i = 0; pattern[i]; i++)
    {
      switch ((int) pattern[i])
	{
	case '*':
	  if (old_list != NULL)
	    {
	      fprintf (stderr, "add_tokens(): Invalid pattern - \"%s\"\n",
		       pattern);
	      fprintf (stderr, "                                 %*s^\n",
		       i, "");
	      abort ();
	    }

	  old_list = va_arg (vp, unsigned short *);
	  /* We'll need to move the original tokens by the current length */
	  move_len = len;
	  old_len = old_list[0] - sizeof (short);
	  len += old_len;
	  break;

	case 't': len += sizeof (short); va_arg (vp, int); break;
	case 'i': len += sizeof (long); va_arg (vp, long); break;
	case 'f': len += sizeof (double); va_arg (vp, double); break;

	case 's':
	  {
	    struct string_value *sv = va_arg (vp, struct string_value *);
	    len += WALIGN (sizeof (struct string_value) + sv->length + 1);
	    break;
	  }

	case '&':
	case '#':
	  {
	    unsigned short *sp = va_arg (vp, unsigned short *);
	    len += sp[0] - sizeof (short);
	    break;
	  }

	default:
	  fprintf (stderr, "add_tokens(): Invalid pattern - \"%s\"\n",
		   pattern);
	  fprintf (stderr, "                                 %*s^\n", i, "");
	  abort ();
	}
    }
  va_end (vp);

  /* Reallocate the token list to the new size */
  new_list = (unsigned short *) realloc (old_list, sizeof (short) + len);
  new_list[0] = sizeof (short) + len;
  if (move_len)
    /* Move the old tokens to their new place */
    memmove (&((char *) new_list)[sizeof (short) + move_len],
	     &new_list[1], old_len);

  /* Add the new tokens */
  tp = &new_list[1];
  va_start (vp, pattern);
  for (i = 0; pattern[i]; i++)
    {
      switch ((int) pattern[i])
	{
	case '*': /* The old pattern was already moved, so skip it. */
	  tp = (unsigned short *) ((unsigned long) tp + old_len);
	  va_arg (vp, unsigned int *);
	  break;
	case 't': *tp++ = (unsigned short) va_arg (vp, int); break;
	case 'i':
	  *((unsigned long *) tp) = va_arg (vp, unsigned long);
	  tp = (unsigned short *) &((unsigned long *) tp)[1];
	  break;
	case 'f':
	  *((double *) tp) = va_arg (vp, double);
	  tp = (unsigned short *) &((double *) tp)[1];
	  break;
	case 's':
	  {
	    struct string_value *string = va_arg (vp, struct string_value *);
	    memcpy (tp, string, WALIGN (sizeof (struct string_value)
					+ string->length + 1));
	    tp = (unsigned short *) &((char *) tp)
	      [WALIGN (sizeof (struct string_value) + string->length + 1)];
	    /* Free the string */
	    free (string);
	    break;
	  }
	case '&':
	case '#':
	  {
	    unsigned short *tp2 = va_arg (vp, unsigned short *);
	    memcpy (tp, &tp2[1], tp2[0] - sizeof (short));
	    tp = (unsigned short *) &((char *) tp)[tp2[0] - sizeof (short)];
	    if (pattern[i] == '#')
	      /* Free the token list */
	      free (tp2);
	    break;
	  }
	}
    }
  va_end (vp);

#ifdef DEBUG
  fprintf (stderr, "New token list ");
  dump_tokens (new_list);
#endif

  return new_list;
}

#ifdef DEBUG
static void
dump_tokens (unsigned short *tokens)
{
  int i, num_tokens = tokens[0] / sizeof (short);

  fprintf (stderr, "(%u tokens):", num_tokens - 1);
  for (i = 1; i < num_tokens; i++)
    fprintf (stderr, " %04X", tokens[i]);
  fprintf (stderr, "\n");
}
#endif
