/* Test wrapper for interpreting the BASIC language */

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include "basic.tab.h"
#include "tables.h"
extern int yyparse (void);
extern int yydebug;
extern int yy_flex_debug;
extern FILE *yyin;
jmp_buf return_point;

/* Signal handler for `Break' */
void
sig_break (int signum)
{
  executing = 0;
}

int
main (int argc, char **argv)
{
  int i, status;
  struct sigaction sa;

  i = 1;
  yydebug = 0;
  yy_flex_debug = 0;
  // FIXME: Document this option and/or replace with getopt(3)
  if ((i < argc) && (argv[i][0] == '-'))
    {
      if (argv[i][1] == 'd')
	{
	  if (argv[i][2] != 'l') {
	    yydebug = 1;
	    tracing |= TRACE_GRAMMAR;
	  }
	  if (argv[i][2] != 'p') {
	    yy_flex_debug = 1;
	    tracing |= TRACE_PARSER;
	  }
	}
      i++;
    }

  /*
   * FIXME: Instead of switching the primary input to the given file,
   * generate the psuedo-statement 'LOAD "filename": RUN' and execute it.
   */
  if (i < argc)
    {
      yyin = fopen (argv[i], "r");
      if (yyin == NULL)
	{
	  perror (argv[i]);
	  return 1;
	}
    }

  initialize_tables ();
  sa.sa_handler = sig_break;
  sa.sa_flags = 0;
  sigemptyset (&sa.sa_mask);
  sigaction (SIGINT, &sa, NULL);

  if ((yyin == NULL) || (yyin == stdin))
    puts ("\nREADY");

  setjmp (return_point);
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Calling yyparse():\n");
  status = yyparse ();
  if (tracing & TRACE_PARSER)
    fprintf (stderr, "Returned %d from yyparse().\n", status);

  if (yyin != stdin)
    fclose (yyin);
  return 0;
}
