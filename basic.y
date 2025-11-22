/* A bison grammar file describing the BASIC language */
%{
  /* C declarations */
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tables.h"
extern int yylex (void);
void yyerror (const char *);
extern jmp_buf return_point;
static unsigned short *add_tokens (const char *, ...);
static void adjust_if_statements (struct line_header *token_line);
static void dump_tokens (unsigned short *);
#define YYERROR_VERBOSE
#define YYDEBUG 1
%}

/* Bison declarations */
%define parse.error verbose

%union {
  unsigned long integer;
  double floating_point;
  struct string_value *string;
  struct list_header *token_list;
  struct statement_header *token_statement;
  struct if_header *token_if_phrase;
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
%token ERROR
%token EXPRESSIONS
%token FOR
%token GOSUB
%token GOTO
%token _GOTO_	/* Implied GOTO for IF / THEN [ ELSE ] statements */
%token GRAMMAR
%token IF
%token INPUT
%token LET
%token _LET_	/* Implied `LET' */
%token LINES
%token LIST
%token LOAD
%token NEW
%token NEXT
%token OFF
%token ON
%token PARSER
%token PRINT
%token READ
%token REM
%token <string> RESTOFLINE
%token RESTORE
%token RETURN
%token RUN
%token SAVE
%token STATEMENTS
%token STEP
%token STOP
%token THEN
%token TO
%token TRACE
/* Arithmetic operators */
%left OR
%left AND
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
%token <token_list> PRINTLIST
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
	{
	  current_column = 0;
	}
    | INTEGER lineofcode '\n' /* Add/change a line in the BASIC program */
	{
	  /* Complete the line of code with a newline */
	  $<tokens>$ = add_tokens ("*t", $<tokens>2, '\n');
	  /* Change the line number */
	  $<token_line>$->line_number = $<integer>1;
	  /*
	   * IF statements need to be handled specially, because
	   * they actually contain two or three statements
	   * separated by the THEN and ELSE tokens.  The following
	   * statements may themselves be IF statements, making this
	   * a recursive problem.  The token list we get includes all
	   * three clauses of the IF statement, which we need to break up.
	   */
	  adjust_if_statements ($<token_line>$);
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Adding line %d to program\n", $<integer>1);
	  add_line ($<token_line>$);
	  /* DO NOT delete this line, since it is now part of the program. */
	  current_column = 0;
	}
    | INTEGER '\n'      /* Delete a line in the BASIC program */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Deleting line %d from program\n", $<integer>1);
	  remove_line ($<integer>1);
	  current_column = 0;
	}
    | lineofcode '\n'   /* Execute a line of code immediately */
	{
	  /* Complete the line of code with a newline */
	  $<tokens>$ = add_tokens ("*t", $<tokens>1, '\n');
	  /* See note on IF statements above */
	  adjust_if_statements ($<token_line>$);
	  immediate_line = $<token_line>$;
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Executing line of BASIC code\n");
	  /* Execute this line */
	  execute ($<token_line>$);
	  /* We're done with this line, so free the token array */
	  immediate_line = NULL;
	  free ($<token_line>$);
	  current_column = 0;
	}
    | INTEGER error '\n'
	{
	  /* Handle the case where an error occurs on
	   * a numbered line by saving the line text */
	  // FIXME: How do we obtain the original (unparsed) line?
	  /* $<tokens>$ = add_tokens ("ttst", ERROR, RESTOFLINE, ???, '\n');
	  $<token_line>$->line_number = $<integer>1;
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Adding line %d to program\n", $<integer>1);
	  add_line ($<token_line>$); */
	  current_column = 0;
	}
    | error '\n'
	{
	  /* yyerrok */; /* Continue reading after an error */
	  current_column = 0;
	}
    ;

lineofcode: statement	/* Create a new line */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Create a new line from the first statement\n");
	  /* Save the length of the completed statement +1,
	   * as the end-of-statement marker will be included later. */
	  $<tokens>$ = add_tokens
	    ("it*", (long) -1, $<tokens>1[0] + sizeof (short), $<tokens>1);
	}
    | lineofcode ':' statement /* Statements are separated by colons */
	{
	  /* Enlarge the current line by the size of the next statement.
	   * Append a colon to complete the previous statement.
	   * Save the length of the new statement +1, as its
	   * end-of-statement marker will be included later. */
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Added another statement to the line\n");
	  /* Save the length of the completed statement +1,
	   * as the end-of-statement marker will be included later. */
	  $<tokens>$ = add_tokens
	    ("*tt#", $<tokens>1, ':',
	     $<tokens>3[0] + sizeof (short), $<tokens>3);
	}
    ;

/* This section describes all BASIC commands, and the various
 * syntaxes for each.  The first line is an implied `LET'. */
statement: assignment
	{
	  int len;

	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Assign a value to a variable\n");
	  /* Convert the expression token list into a statement */
	  $<tokens>$ = add_tokens ("t*", _LET_, $<tokens>1);
	}
    | BYE		/* Bye-bye */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Bye-bye\n");
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("t", BYE);
	}
    | CONTINUE          /* Continue program execution from last `STOP' */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Continue program from where we left off\n");
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("t", CONTINUE);
	}
    | DATA datalist     /* Define data to be read with `READ' */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Data list defined\n");
	  /* Convert the data token list into a statement */
	  $<tokens>$ = add_tokens ("t*", DATA, $<tokens>2);
	}
    | DEF numfuncdecl '=' numexpression /* Define a numeric function */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Numeric function defined\n");
	  /* convert the expression token lists into a statement */
	  $<tokens>$ = add_tokens ("tt*tt#", DEF, NUMLVAL, $<tokens>2,
				   '=', NUMEXPR, $<tokens>4);
	}
    | DEF strfuncdecl '=' stringexpression /* Define a string function */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "String function defined\n");
	  /* convert the expression token lists into a statement */
	  $<tokens>$ = add_tokens ("tt*tt#", DEF, STRLVAL, $<tokens>2,
				   '=', STREXPR, $<tokens>4);
	}
    | DIM dimlist       /* Define the size of array variables */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Array variable defined\n");
	  /* Convert the variable token list into a statement */
	  $<tokens>$ = add_tokens ("t*", DIM, $<tokens>2);
	}
    | END               /* Stop program execution */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "End the program's execution\n");
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("t", END);
	}
    | FOR simplenumvar '=' arithexpr TO arithexpr
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr,
		     "Begin an iteration of the following statements up to NEXT\n");
	  /* Condense all token lists into one statement */
	  $<tokens>$ = add_tokens ("tt*tt#tt#", FOR, NUMLVAL, $<tokens>2,
				   '=', NUMEXPR, $<tokens>4, TO, NUMEXPR,
				   $<tokens>6);
	}
    | FOR simplenumvar '=' arithexpr TO arithexpr STEP arithexpr
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr,
		     "Begin an iteration using an alternative increment\n");
	  /* Condense all token lists into one statement */
	  $<tokens>$ = add_tokens ("tt*tt#tt#tt#", FOR, NUMLVAL, $<tokens>2,
				   '=', NUMEXPR, $<tokens>4, TO, NUMEXPR,
				   $<tokens>6, STEP, NUMEXPR, $<tokens>8);
	}
    | GOSUB arithexpr   /* Save the current position, then transfer */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Go to a subroutine\n");
	  /* Convert the expression token list into a statement */
	  $<tokens>$ = add_tokens ("t*", GOSUB, $<tokens>2);
	}
    | GOTO arithexpr    /* Transfer control to a different line */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Go to another line in the program\n");
	  /* Convert the expression token list into a statement */
	  $<tokens>$ = add_tokens ("t*", GOTO, $<tokens>2);
	}
    | IF numexpression THEN INTEGER elseclause /* `GOTO' a line if expression is true */
	{

	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Go to another line if this expression is true\n");
	  /* Construct the IF statement from the expression and clause tokens.
	   * The IF header includes two additional offset values to the
	   * statements following the THEN and (optional) ELSE tokens. */
	  int then_offset = offsetof(struct if_header, tokens)
	    /* We'll be subtracting one short for the length of the
	     * numeric expression token list, but then adding one
	     * for the terminating THEN token, so they cancel out. */
	    + $<token_statement>2->length;
	  /* The length of the THEN clause includes:
	   *  - its length (including the length short)
	   *  - the implied GOTO command
	   *  - the token for an INTEGER value
	   *  - the line number (long)
	   *  - the statement terminator that will be added later */
	  const int then_length = 4 * sizeof(short) + sizeof(long);
	  int else_offset = then_offset + then_length;
	  $<tokens>$ = add_tokens
	    ("ttt*tttti", IF, then_offset, else_offset, $<tokens>2, THEN,
	     then_length, _GOTO_, INTEGER, $<integer>4);
	  if ($<tokens>5 != NULL) // ELSE clause
	    $<tokens>$ = add_tokens
	      ("*tt#", $<tokens>$, ELSE,
	       /* Add one to the original clause length to include
		* the statement terminator that will be added later. */
	       $<token_statement>5->length + sizeof(short), $<tokens>5);
	}
    | IF numexpression THEN statement elseclause /* Continue execution if expr is true */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr,
		     "Execute the next statement if this expression is true\n");
	  /* Construct the IF statement from the expression and clause tokens.
	   * The IF header includes two additional offset values to the
	   * statements following the THEN and (optional) ELSE tokens. */
	  int then_offset = offsetof(struct if_header, tokens)
	    + $<token_statement>2->length;	// as above
	  /* Add the size of another token to the length of the THEN clause
	   * to include the statement terminator that will be added later. */
	  int then_length = $<token_statement>4->length + sizeof(short);
	  int else_offset = then_offset + then_length;
	  $<tokens>$ = add_tokens
	    ("ttt*tt#", IF, then_offset, else_offset, $<tokens>2, THEN,
	     then_length, $<tokens>4);
	  if ($<tokens>5 != NULL) // ELSE clause
	    $<tokens>$ = add_tokens
	      ("*tt#", $<tokens>$, ELSE,
	       /* Add one to the original clause length to include
		* the statement terminator that will be added later. */
	       $<token_statement>5->length + sizeof(short), $<tokens>5);
	}
    | INPUT varlist /* Read a value from the terminal and store in vars */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Read a value from the user\n");
	  /* Convert the variable token list into a statement */
	  $<tokens>$ = add_tokens ("t*", INPUT, $<tokens>2);
	}
    | INPUT STRING ';' varlist /* Print a prompt, then read a value to vars */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr,
		     "Print a prompt %s and read a value from the user\n",
		     $<string>2->contents);
	  /* Convert the variable list into a statement */
	  $<tokens>$ = add_tokens ("ttst*", INPUT, STRING, $<string>2,
				   ';', $<tokens>4);
	}
    | LET assignment    /* Assign a value to a variable */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Assign a value to a variable\n");
	  /* Convert the expression token list into a statement */
	  $<tokens>$ = add_tokens ("t*", LET, $<tokens>2);
	}
    | LIST              /* Display the entire program */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "List the entire program\n");
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("t", LIST);
	}
    | LIST INTEGER      /* Display one line of the program */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "List line %d of the program\n", $<integer>2);
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("tti", LIST, INTEGER, $<integer>2);
	}
    | LIST INTEGER ',' INTEGER /* Display a range of lines of the program */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr,
		     "List a portion of the program from line %d to line %d\n",
		     $<integer>2, $<integer>4);
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("ttitti", LIST, INTEGER, $<integer>2,
				   ',', INTEGER, $<integer>4);
	}
    | LOAD STRING       /* Read input from a file */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Read a program from file %s\n",
		     $<string>2->contents);
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("tts", LOAD, STRING, $<string>2);
	}
    | NEW               /* Erase the current program from memory */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Erase the program and start over\n");
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("t", NEW);
	}
    | NEXT simplenumvar /* Step a variable; transfer control to `FOR' */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr,
		     "Do the next iteration of the statement following FOR\n");
	  /* Convert the variable token list into a statement */
	  $<tokens>$ = add_tokens ("t*", NEXT, $<tokens>2);
	}
    | ON numvariable GOSUB linelist /* `GOSUB' the Nth line in the list */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Compute the line to go subroutine\n");
	  /* Make a statement out of the existing tokens */
	  $<tokens>$ = add_tokens ("t*t#", ON, $<tokens>2, GOSUB, $<tokens>4);
	}
    | ON numvariable GOTO linelist /* `GOTO' the Nth line in the list */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Compute the line to go to\n");
	  /* Make a statement out of the existing tokens */
	  $<tokens>$ = add_tokens ("t*t#", ON, $<tokens>2, GOTO, $<tokens>4);
	}
    | PRINT             /* Print a blank line */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Skip a line of output\n");
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("t", PRINT);
	}
    | PRINT printlist   /* Print something */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Print a line\n");
	  /* Make a statement out of the list */
	  $<tokens>$ = add_tokens ("t*", PRINT, $<tokens>2);
	}
    | READ varlist  /* Read next item from a `DATA' line, store in var */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Read the next DATA item\n");
	  /* Make a statement out of the list */
	  $<tokens>$ = add_tokens ("t*", READ, $<tokens>2);
	}
    | REM RESTOFLINE    /* Comment -- store in program, but do nothing */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Remark noted: %s\n", $<string>2->contents);
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("tts", REM, RESTOFLINE, $<string>2);
	}
    | RESTORE           /* Reset `DATA' pointer to the beginning */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr,
		     "Restore DATA pointer to the beginning of the program\n");
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("t", RESTORE);
	}
    | RESTORE arithexpr /* Reset `DATA' pointer to a given line */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr,
		     "Restore DATA pointer to a specific line in the program\n");
	  /* Convert the expression token list into a statement */
	  $<tokens>$ = add_tokens ("t*", RESTORE, $<tokens>2);
	}
    | RETURN            /* Transfer control back to previous `GOSUB' */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr,
		     "Return to the statement following the last GOSUB call\n");
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("t", RETURN);
	}
    | RUN               /* Reset program counters and execute program */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Run the program from the beginning\n");
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("t", RUN);
	}
    | SAVE STRING       /* List the entire program to a file */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Write the program listing to file %s\n",
		     $<string>2->contents);
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("tts", SAVE, STRING, $<string>2);
	}
    | STOP              /* Stop program execution temporarily */
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Stop the program at this point\n");
	  /* Allocate a new statement */
	  $<tokens>$ = add_tokens ("t", STOP);
	}
    | TRACE tracetarget tracestate
	{
	  if (tracing & TRACE_GRAMMAR)
	    fprintf (stderr, "Tracing\n");
	  $<tokens>$ = add_tokens ("t##", TRACE, $<tokens>2, $<tokens>3);
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
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Line number %u\n", $<integer>1);
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
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Line number %u\n", $<integer>3);
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
	("tttt*", PRINTLIST, 0, 1,
	 sizeof (struct list_item) + $<tokens>1[0] - sizeof (short),
	 $<tokens>1);
      ((struct list_header *) &$<tokens>$[2])->length
	= $<tokens>$[0] - 2 * sizeof (short);
    }
    | printlist printitem
    {
      /* Enlarge the array to include the next print item. */
      $<tokens>$ = add_tokens ("*t#", $<tokens>1, $<tokens>2[0], $<tokens>2);
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
    | ';'
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Concatenating print items\n");
      $<tokens>$ = add_tokens ("t", ';');
    }
    | ','
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Advance print to next tab stop\n");
      $<tokens>$ = add_tokens ("t", ',');
    }
      /* `TAB()' is a special case -- it's a string function without a `$',
       * but it only makes sense in `PRINT' statements anyway. */
    | TAB '(' numexpression ')'
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Spacing over to tab stop\n");
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

numexpression: condexpr | arithexpr ;

stringexpression: STRING
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "String constant %s\n", $<string>1->contents);
      /* Start a new token list */
      $<tokens>$ = add_tokens ("ts", STRING, $<string>1);
    }
    | strvariable
    | stringfunction
    | stringexpression '+' stringexpression
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Concatenate two strings\n");
      /* Enlarge the first token list to include the second
       * and the plus sign */
      $<tokens>$ = add_tokens ("*t#", $<tokens>1, '+', $<tokens>3);
    }
    ;

arithexpr: number
    | numvariable
    | '(' numexpression ')'
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Parenthesized numeric expression\n");
      /* Enlarge the token list to include both parentheses. */
      $<tokens>$ = add_tokens ("t*t", '(', $<tokens>2, ')');
    }
    | numfunction
    | arithexpr '+' arithexpr
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Adding two numbers; new tokens are ");
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens ("t*t#t", '{', $<tokens>1, '+', $<tokens>3, '}');
    }
    | arithexpr '-' arithexpr
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Subtracting two numbers; new tokens are ");
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens ("t*t#t", '{', $<tokens>1, '-', $<tokens>3, '}');
    }
    | arithexpr '*' arithexpr
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Multiplying two numbers; ");
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens ("t*t#t", '{', $<tokens>1, '*', $<tokens>3, '}');
    }
    | arithexpr '/' arithexpr
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Dividing two numbers; ");
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens ("t*t#t", '{', $<tokens>1, '/', $<tokens>3, '}');
    }
    | arithexpr '^' arithexpr
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Expenentiating two numbers; ");
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens ("t*t#t", '{', $<tokens>1, '^', $<tokens>3, '}');
    }
    | '-' arithexpr %prec NEG
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Negating a number; ");
      /* Enlarge the token list to include the operator
       * and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens ("tt*t", '{', NEG, $<tokens>2, '}');
    }
    ;

condexpr: arithexpr
    |
      arithexpr '=' arithexpr
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Comparing two numbers for equality\n");
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("tt*t#t", '{', NUMEXPR, $<tokens>1, '=', $<tokens>3, '}');
    }
    | arithexpr '<' arithexpr
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Comparing two numbers for less\n");
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("tt*t#t", '{', NUMEXPR, $<tokens>1, '<', $<tokens>3, '}');
    }
    | arithexpr '>' arithexpr
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Comparing two numbers for more\n");
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("tt*t#t", '{', NUMEXPR, $<tokens>1, '>', $<tokens>3, '}');
    }
    | arithexpr LESSEQ arithexpr
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Comparing two numbers for not more\n");
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("tt*t#t", '{', NUMEXPR, $<tokens>1, LESSEQ, $<tokens>3, '}');
    }
    | arithexpr GRTREQ arithexpr
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Comparing two numbers for not less\n");
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("tt*t#t", '{', NUMEXPR, $<tokens>1, GRTREQ, $<tokens>3, '}');
    }
    | arithexpr NOTEQ arithexpr
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Comparing two numbers for inequality\n");
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("tt*t#t", '{', NUMEXPR, $<tokens>1, NOTEQ, $<tokens>3, '}');
    }
    | stringexpression '=' stringexpression
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Comparing two strings for equality\n");
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("tt*t#t", '{', STREXPR, $<tokens>1, '=', $<tokens>3, '}');
    }
    | stringexpression '<' stringexpression
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Comparing two strings for before\n");
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("tt*t#t", '{', STREXPR, $<tokens>1, '<', $<tokens>3, '}');
    }
    | stringexpression '>' stringexpression
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Comparing two strings for after\n");
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("tt*t#t", '{', STREXPR, $<tokens>1, '>', $<tokens>3, '}');
    }
    | stringexpression LESSEQ stringexpression
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Comparing two strings for not after\n");
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("tt*t#t", '{', STREXPR, $<tokens>1, LESSEQ, $<tokens>3, '}');
    }
    | stringexpression GRTREQ stringexpression
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Comparing two strings not before\n");
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("tt*t#t", '{', STREXPR, $<tokens>1, GRTREQ, $<tokens>3, '}');
    }
    | stringexpression NOTEQ stringexpression
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Comparing two strings for inequality\n");
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("tt*t#t", '{', STREXPR, $<tokens>1, NOTEQ, $<tokens>3, '}');
    }
    | condexpr AND condexpr
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Make sure both conditions are true\n");
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("t*t#t", '{', $<tokens>1, AND, $<tokens>3, '}');
    }
    | condexpr OR condexpr
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Succeed if either condition is true\n");
      /* Enlarge the first token list to include both expressions,
       * the operator, and surrounding `phantom' parentheses. */
      $<tokens>$ = add_tokens
	("t*t#t", '{', $<tokens>1, OR, $<tokens>3, '}');
    }
    ;

number: INTEGER
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Integer %u; ", $<integer>1);
      /* Start a new token list */
      $<tokens>$ = add_tokens ("ti", INTEGER, $<integer>1);
    }
    | FLOATINGPOINT
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Floating-point number %G; ", $<floating_point>1);
      /* Start a new token list */
      $<tokens>$ = add_tokens ("tf", FLOATINGPOINT, $<floating_point>1);
    }
    ;

anyconstant:
    '-' INTEGER %prec NEG
    {
      /* This needs to be converted to a short
       * expression that negates the value. */
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Negative numeric constant -%lu", $<integer>2);
      $<tokens>$ = add_tokens ("tttit", '{', NEG, INTEGER, $<integer>2, '}');
    }
    | '-' FLOATINGPOINT %prec NEG
    {
      /* We can simply negate the floating-point value inline. */
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Negative numeric constant %G", -$<floating_point>2);
      $<tokens>$ = add_tokens ("tf", FLOATINGPOINT, -$<floating_point>2);
    }
    | number
    | STRING
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "String constant %s; ", $<string>1->contents);
      /* Start a new token list */
      $<tokens>$ = add_tokens ("ts", STRING, $<string>1);
    }
    ;

/* Define variable types */
anyvariable: numvariable | strvariable ;

numvariable: simplenumvar
    | IDENTIFIER '(' arrayindex ')'
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Numeric array element in %s; ",
		 name_table[$<integer>1]->contents);
      /* Enlarge the arrayindex token list to include the identifier
       * and parentheses tokens */
      $<tokens>$ = add_tokens
	("ttt*t", IDENTIFIER, $<integer>1, '(', $<tokens>3, ')');
    }
    ;

strvariable: simplestringvar
    | STRINGIDENTIFIER '(' arrayindex ')'
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "String array element in %s; ",
		 name_table[$<integer>1]->contents);
      /* Enlarge the arrayindex token list to include the identifier
       * and parentheses tokens */
      $<tokens>$ = add_tokens
	("ttt*t", STRINGIDENTIFIER, $<integer>1, '(', $<tokens>3, ')');
    }
    ;

simplevar: simplenumvar | simplestringvar ;

simplenumvar: IDENTIFIER
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Numeric variable %s; ",
		 name_table[$<integer>1]->contents);
      /* Start a new token list */
      $<tokens>$ = add_tokens ("tt", IDENTIFIER, $<integer>1);
    }
    ;

simplestringvar: STRINGIDENTIFIER
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "String variable %s; ",
		 name_table[$<integer>1]->contents);
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

/* ELSE clauses are optional */
elseclause: /* empty */
    {
      $<tokens>$ = NULL;
    }
    | ELSE INTEGER
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Go to another line if the previous expression was false\n");
      $<tokens>$ = add_tokens ("tti", _GOTO_, INTEGER, $<integer>2);
    }
    | ELSE statement
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Execute the next statement if the previous expression was false\n");
      $<tokens>$ = $<tokens>2;
    }

/* Define trace types */
tracetarget: LINES
    {
      $<tokens>$ = add_tokens ("t", LINES);
    }
    | STATEMENTS
    {
      $<tokens>$ = add_tokens ("t", STATEMENTS);
    }
    | EXPRESSIONS
    {
      $<tokens>$ = add_tokens ("t", EXPRESSIONS);
    }
    | GRAMMAR
    {
      $<tokens>$ = add_tokens ("t", GRAMMAR);
    }
    | PARSER
    {
      $<tokens>$ = add_tokens ("t", PARSER);
    }
    ;
tracestate: OFF
    {
      $<tokens>$ = add_tokens ("t", OFF);
    }
    | ON
    {
      $<tokens>$ = add_tokens ("t", ON);
    }
    ;

/* Define functions.  Note that a function and an array are ambiguous;
 * the only way to tell them apart is by their declaration. */
numfunction: IDENTIFIER '(' arguments ')'
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Calling numeric function %s\n",
		 name_table[$<integer>1]->contents);
      $<tokens>$ = add_tokens ("ttt*t", IDENTIFIER,
			       $<integer>1, '(', $<tokens>3, ')');
    }
    ;

stringfunction:
      STRINGIDENTIFIER '(' arguments ')'
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Calling string function %s\n",
		 name_table[$<integer>1]->contents);
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
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Declare numeric function %s\n",
		 name_table[$<integer>1]->contents);
      $<tokens>$ = add_tokens ("ttt*t", IDENTIFIER,
			       $<integer>1, '(', $<tokens>3, ')');
    }
    ;

strfuncdecl: STRINGIDENTIFIER '(' arglist ')'
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Declare string function %s\n",
		 name_table[$<integer>1]->contents);
      $<tokens>$ = add_tokens ("ttt*t", STRINGIDENTIFIER,
			       $<integer>1, '(', $<tokens>3, ')');
    }
    ;

dimdecl: IDENTIFIER '(' arrayindex ')'
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Declare numeric array %s\n",
		 name_table[$<integer>1]->contents);
      $<tokens>$ = add_tokens ("ttt*t", IDENTIFIER,
			       $<integer>1, '(', $<tokens>3, ')');
    }
    | STRINGIDENTIFIER '(' arrayindex ')'
    {
      if (tracing & TRACE_GRAMMAR)
	fprintf (stderr, "Declare string array %s\n",
		 name_table[$<integer>1]->contents);
      $<tokens>$ = add_tokens ("ttt*t", STRINGIDENTIFIER,
			       $<integer>1, '(', $<tokens>3, ')');
    }
    ;

%%

/* Additional C code */

void
yyerror (const char *message)
{
  fprintf (stderr, "\007Parse error: %s\n", message);
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

  if (tracing & TRACE_GRAMMAR)
    fprintf (stderr, "add_tokens() called with pattern \"%s\"\n", pattern);

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

  if (tracing & TRACE_GRAMMAR) {
    fprintf (stderr, "New token list ");
    dump_tokens (new_list);
  }

  return new_list;
}

/*
 * Adjust the IF clauses in a line to make separate statements from them.
 * During parsing, the length of the if statement header includes both
 * the THEN and ELSE clauses; this needs to be changed to the then_offset.
 * If the statement did not have an ELSE clause, the else_offset needs to
 * be changed to the offset of the end of the line.
 */
static void
adjust_if_statements (struct line_header *token_line)
{
  struct statement_header *stmt;
  unsigned short *tp;

  stmt = &token_line->statement[0];
  char *end_of_line = &((char *) token_line)[token_line->length];
  while ((char *) stmt < end_of_line) {

    if (stmt->command == IF) {
      struct if_header *ifstmt = (struct if_header *) stmt;
      char *end_of_statement = &((char *) stmt)[stmt->length];
      struct statement_header *then_clause = (struct statement_header *)
	&((char *) stmt)[ifstmt->then_offset];
      struct statement_header *else_clause = (struct statement_header *)
	&((char *) stmt)[ifstmt->else_offset];

      /* If the ELSE offset is the end of the IF statement,
       * extend it to the end of the line. */
      if ((char *) else_clause >= end_of_statement) {
	ifstmt->else_offset = (unsigned short) (end_of_line - (char*) stmt);
	else_clause = NULL;
      }
      /* Cut the length of the IF statement to the THEN offset. */
      ifstmt->length = ifstmt->then_offset;
      if (tracing & TRACE_GRAMMAR) {
	fprintf (stderr, "Adjusted tokens for IF statement at line %d offset %d\n",
		 token_line->line_number,
		 (char *) stmt - (char *) &token_line->statement[0]);
	fprintf (stderr, "    IF clause ");
	dump_tokens ((unsigned short *) ifstmt);
	fprintf (stderr, "    THEN clause ");
	dump_tokens ((unsigned short *) then_clause);
	if (else_clause) {
	  fprintf (stderr, "    ELSE clause ");
	  dump_tokens ((unsigned short *) else_clause);
	}
      }
    }

    stmt = (struct statement_header *) &((char *) stmt)[stmt->length];
  }
}

static void
dump_tokens (unsigned short *tokens)
{
  int i, num_tokens = tokens[0] / sizeof (short);

  fprintf (stderr, "(%u tokens):", num_tokens - 1);
  for (i = 1; i < num_tokens; i++)
    fprintf (stderr, " %04X", tokens[i]);
  fprintf (stderr, "\n");
}
