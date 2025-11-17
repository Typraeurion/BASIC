#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tables.h"
#include "basic.tab.h"


/* Global data */
int executing;
unsigned long current_line = (unsigned long) -1;
unsigned long current_data_line;
unsigned short current_statement;
unsigned short current_data_statement;
unsigned short current_data_item;
struct line_header *immediate_line = NULL;


/* Constant data */

/* Statement function pointers */
static const struct {
  unsigned short token;
  void (*command) (struct statement_header *);
} command_table[] = {
  { BYE, cmd_bye },
  { CONTINUE, cmd_continue },
  { DATA, cmd_data },
  //  { DEF, cmd_def },
  { DIM, cmd_dim },
  { END, cmd_end },
  { FOR, cmd_for },
  { GOSUB, cmd_gosub },
  { GOTO, cmd_goto },
  { _GOTO_, cmd_goto },
  { IF, cmd_if },
  { INPUT, cmd_input },
  { _LET_, cmd_let },
  { LET, cmd_let },
  { LIST, cmd_list },
  { LOAD, cmd_load },
  { NEW, cmd_new },
  { NEXT, cmd_next },
  { ON, cmd_on },
  { PRINT, cmd_print },
  { READ, cmd_read },
  { REM, cmd_rem },
  { RESTORE, cmd_restore },
  { RETURN, cmd_return },
  { RUN, cmd_run },
  { SAVE, cmd_save },
  { STOP, cmd_stop },
  { TRACE, cmd_trace },
};


/* Subroutine stack */
static struct gosub_stack_s {
  unsigned long previous_line;
  unsigned short previous_statement;
} *gosub_stack;
int gosub_stack_size;

/* FOR...NEXT stack */
static struct for_stack_s {
  unsigned short var_index;	/* The variable we are interating */
  unsigned long for_line;	/* The line containing the FOR statement */
  unsigned short for_statement;	/* The index of the FOR statement itself */
  struct statement_header *for_stmt; /* Pointer to the FOR statement for sanity check */
  unsigned short *to_expr;	/* Token pointer to the TO expression */
  unsigned short *step_expr;	/* Token pointer to the STEP expression if one exists; else NULL */
} *for_stack;
int for_stack_size;


/* Stop address; preserved only until a CONTINUE
 * or a statement which changes the current line/statement. */
static unsigned long stop_line;
static unsigned short stop_statement;
/* For debugging */
int tracing = 0;


/* Search a given line for the Nth statement */
struct statement_header *
find_statement (struct line_header *line, unsigned short statement_number)
{
  struct statement_header *stmt;
  int i;

  stmt = &line->statement[0];
  for (i = 0; i < statement_number; i++)
    {
      /* Skip to the next statement */
      stmt = (struct statement_header *) &((char *) stmt)[stmt->length];
      /* Check for the end of the line */
      if ((char *) stmt >= &((char *) line)[line->length])
	return NULL;
    }
  return stmt;
}

/* Search a line for the Nth statement */
struct statement_header *
find_line_statement (unsigned long line_number,
		     unsigned short statement_number)
{
  struct line_header *line;
  struct statement_header *stmt;
  int i;

  line = find_line (line_number, 0);
  if (line == NULL)
    return NULL;
  return find_statement (line, statement_number);
}

/* Execute the next statement */
static void
execute_statement (struct statement_header *stmt)
{
  int i;

  for (i = 0; i < sizeof (command_table) / sizeof (command_table[0]); i++)
    {
      if (command_table[i].token == stmt->command)
	{
	  if (tracing & TRACE_STATEMENTS)
	    {
	      list_statement (stmt, stderr);
	      // Print a newline if the statement does not already end with one
	      unsigned short *tail_tp = (unsigned short *)
		&((char *) stmt)[stmt->length - sizeof(short)];
	      if (*tail_tp != '\n')
		fputc('\n', stderr);
	    }
	  command_table[i].command (stmt);
	  return;
	}
    }
  fputs ("Unhandled command: ", stderr);
  list_token (&stmt->command, stderr);
  fputc ('\n', stderr);
}

/* Execute the statements in a line */
void
execute (struct line_header *command_line)
{
  struct line_header *line;
  struct statement_header *stmt;
  unsigned long last_line;
  unsigned short last_statement;

  executing = 1;
  line = command_line;
  current_statement = 0;
  current_line = line->line_number;
  stmt = &line->statement[0];
  while (executing)
    {
      if ((tracing & TRACE_LINES) && (current_statement == 0))
	list_line (line, stderr);
      current_statement++;
      last_line = current_line;
      last_statement = current_statement;
      execute_statement (stmt);
      unsigned short *tail_tp = (unsigned short *)
	&((char *) stmt)[stmt->length - sizeof(short)];
      /* If we haven't changed position (yet), check
       * whether the statement ends in an ELSE token;
       * in that case we can skip the rest of the line. */
      if ((current_line == last_line) && (current_statement == last_statement)
	  && (*tail_tp == ELSE))
	{
	  current_line++;
	  current_statement = 0;
	}
      /* Check for a change of position */
      if ((current_line != last_line) || (current_statement != last_statement))
	{
	  /* Reset the stop position */
	  stop_line = (unsigned long) -1;
	  /* Find out where the next statement is */
	  if (current_line == (unsigned long) -1)
	    line = command_line;
	  else
	    {
	      line = find_line (current_line, 1);
	      if (line == NULL)
		{
		  /* End of program */
		  cmd_end (command_line->statement);
		  break;
		}
	      if (line->line_number != current_line)
		{
		  current_line = line->line_number;
		  current_statement = 0;
		}
	    }
	}

      /* Get the next statement */
      stmt = find_statement (line, current_statement);
      /* If we've reached the end of this line, go on to the next. */
      if (stmt == NULL)
	{
	  line = (current_line == (unsigned long) -1) ? NULL
	    : find_line (++current_line, 1);
	  if (line == NULL)
	    {
	      /* End of program */
	      cmd_end (command_line->statement);
	      break;
	    }
	  current_line = line->line_number;
	  current_statement = 0;
	  stmt = &line->statement[0];
	}
    }

  /* End of execution; print "READY". */
  /* FIX ME: This should only be done if the program terminated normally. */
  puts ("\nREADY");
}

/* Push the current program location on the subroutine stack */
static void
push_sub (void)
{
  gosub_stack = (struct gosub_stack_s *) realloc
    (gosub_stack, sizeof (struct gosub_stack_s) * ++gosub_stack_size);
  memmove (&gosub_stack[1], gosub_stack,
	   sizeof (struct gosub_stack_s) * (gosub_stack_size - 1));
  gosub_stack->previous_line = current_line;
  gosub_stack->previous_statement = current_statement;
}

void
cmd_bye (struct statement_header *stmt)
{
  exit (0);
}

void
cmd_continue (struct statement_header *stmt)
{
  /* Do nothing if the program has not stopped */
  if (stop_line == (unsigned long) -1)
    return;
  current_line = stop_line;
  current_statement = stop_statement;
  executing = 1;
}

void
cmd_end (struct statement_header *stmt)
{
  current_line = (unsigned long) -1;
  executing = 0;

/* Clear the subroutine and FOR...NEXT stacks of all entries */
  gosub_stack_size = 0;
  for_stack_size = 0;
}

void
cmd_gosub (struct statement_header *stmt)
{
  struct line_header *line;
  unsigned long number;
  unsigned short *tp;

  /* Evaluate the line number expression */
  tp = &stmt->tokens[0];
  number = (unsigned long) eval_number (&tp);
  line = find_line (number, 0);
  if (line == NULL)
    {
      printf ("ERROR - GOSUB: NO LINE %u\n", number);
      executing = 0;
      return;
    }
  push_sub ();
  current_line = line->line_number;
  current_statement = 0;
}

void
cmd_goto (struct statement_header *stmt)
{
  struct line_header *line;
  unsigned long number;
  unsigned short *tp;

  /* Evaluate the line number expression */
  tp = &stmt->tokens[0];
  number = (unsigned long) eval_number (&tp);
  line = find_line (number, 0);
  if (line == NULL)
    {
      printf ("ERROR - GOTO: NO LINE %u\n", number);
      executing = 0;
      return;
    }
  current_line = line->line_number;
  current_statement = 0;
}

void
cmd_on (struct statement_header *stmt)
{
  int i, index, op;
  struct line_header *line;
  unsigned long number;
  unsigned short *tp;

  /* Evaluate the index expression */
  tp = &stmt->tokens[0];
  index = (unsigned long) eval_number (&tp);

  /* The next token should be `GOTO' or `GOSUB' */
  op = *tp++;
  if ((op != GOTO) && (op != GOSUB))
    {
      fputs ("cmd_on(): ON index followed by token ", stderr);
      list_token (--tp, stderr);
      fputc ('\n', stderr);
      return;
    }
  /* The next tokens should be an item list */
  if (*tp++ != ITEMLIST)
    {
      fputs ("cmd_on(): ON ... GO followed by token ", stderr);
      list_token (--tp, stderr);
      fputc ('\n', stderr);
      return;
    }

  /* If the index is less than 1, use the first item. */
  if (index < 1)
    index = 1;
  /* If the index is greater than the number of items,
   * skip to the next statement. */
  if (index > ((struct list_header *) tp)->num_items)
    return;

  /* Find the indexed line number */
  tp = (unsigned short *) &((struct list_header *) tp)->item[0];
  for (i = 1; i < index; i++)
    {
      /* Skip over this item and the separator (',') */
      tp+= ((struct list_item *) tp)->length / sizeof (short) + 1;
    }

  /* Get the line number */
  number = *((unsigned long *) &((struct list_item *) tp)->tokens[1]);
  line = find_line (number, 0);
  if (line == NULL)
    {
      printf ("ERROR - ON..%s: NO LINE %u\n",
	      (op == GOTO) ? "GOTO" : "GOSUB", number);
      executing = 0;
      return;
    }
  if (op == GOSUB)
    push_sub ();
  current_line = line->line_number;
  current_statement = 0;
}

void
cmd_rem (struct statement_header *stmt)
{
  /* Do nothing */
  return;
}

void
cmd_restore (struct statement_header *stmt)
{
  unsigned short *tp = &stmt->tokens[0];
  if ((*tp != ':') && (*tp != '\n') && (*tp != ELSE))
    {
      struct line_header *line;
      int number;

      /* Evaluate the numeric expression */
      number = (unsigned long) eval_number (&tp);
      line = find_line (number, 1);
      current_data_line = ((line == NULL)
			   ? (unsigned long) -1
			   : line->line_number);
    } else {
      current_data_line = 0;
    }
  current_data_statement = 0;
  current_data_item = 0;
}

void
cmd_return (struct statement_header *stmt)
{
  struct line_header *line;

  /* Make sure we have a place to go! */
  if (gosub_stack_size <= 0)
    {
      puts ("ERROR - RETURN: NOT IN SUBROUTINE");
      executing = 0;
      return;
    }

  /* Return to the next level on the subroutine stack */
  current_line = gosub_stack->previous_line;
  current_statement = gosub_stack->previous_statement;
  memmove (gosub_stack, &gosub_stack[1],
	   sizeof (struct gosub_stack_s) * --gosub_stack_size);

  /* Make sure the line still exists */
  line = find_line (current_line, 1);
  if (line == NULL)
    {
      /* End of program */
      current_line = (unsigned long) -1;
      executing = 0;
      return;
    }
  if (line->line_number != current_line)
    {
      /* Return line has been deleted; go to the next one. */
      current_line = line->line_number;
      current_statement = 0;
    }
}

void
cmd_run (struct statement_header *stmt)
{
  int i;

  /* Restore the program state to initial values */
  current_line = 0;
  current_statement = 0;
  cmd_restore (stmt);

  /* Zero all variables */
  for (i = 0; i < name_table_size; i++)
    {
      if (name_table[i]->contents[name_table[i]->length - 1] == '$')
	{
	  if (variable_values[i].str != NULL)
	    {
	      free (variable_values[i].str);
	      variable_values[i].str = NULL;
	    }
	} else {
	  variable_values[i].num = 0.0;
	}
    }

  /* Remove all functions */
  for (i = 0; i < function_table_size; i++)
    {
      if (function_table[i].expr != NULL)
	free (function_table[i].expr);
    }
  function_table_size = 0;
  /* But keep the built-in ones */
  initialize_builtin_functions ();

  /* Start executing */
  executing = 1;
}

void
cmd_stop (struct statement_header *stmt)
{
  printf ("STOPPED\n");
  executing = 0;
  stop_line = current_line;
  stop_statement = current_statement;
}

void
cmd_if (struct statement_header *stmt)
{
  struct if_header *if_stmt = (struct if_header *) stmt;
  struct line_header *line;
  unsigned short *tp;
  unsigned short terminator;
  double condition;

  /* Evaluate the conditional expression */
  tp = &if_stmt->tokens[0];
  condition = eval_number (&tp);

  /* The token pointer should now be pointing at THEN */
  terminator = *tp++;
  if (terminator != THEN)
    {
      fputs ("IF: Missing THEN; got ", stderr);
      list_token (--tp, stderr);
      fputc ('\n', stderr);
      return;
    }

  /* Continue to the next statement if the condition is true;
   * if the condition is false, search for an ELSE on this line,
   * or go to the next line if there is no ELSE. */
  if (condition != 0.0)
    return;
  current_statement++;
  stmt = (struct statement_header *) tp;
  terminator = *((unsigned short *) &((char *) stmt)[stmt->length - sizeof (short)]);
  while (terminator != '\n') {
    if ((terminator == THEN) || (terminator == ':')) {
      current_statement++;
      stmt = (struct statement_header *) &((char *) stmt)[stmt->length];
      terminator = *((unsigned short *) &((char *) stmt)[stmt->length - sizeof (short)]);
      continue;
    }
    if (terminator == ELSE) {
      /* Found an ELSE; the statement pointer
       * should be at the following statement. */
      return;
    }
    /* If we reach here, the current statement ends with something
     * other than a terminator token.  This is an internal error. */
    fprintf (stderr, "Line %d statement %d ends in unexpected token %04X\n",
	     current_line, current_statement, terminator);
    // Skip to the next line
    current_line++;
    current_statement = 0;
    return;
  }
  /* fall through when the end of the line is encountered; go to the next line. */
  current_line++;
  current_statement = 0;
}

void
cmd_for (struct statement_header *stmt)
{
  unsigned short *tp;
  int var;
  double to_value, step_value;
  struct line_header *line;

  /* Get the variable index.  Note that the order of tokens is:
   * FOR: NUMLVAL, IDENTIFIER, name index, '=', NUMEXPR, ...
   * stmt token[0]  token[1]   ---------- */
  tp = &stmt->tokens[2];
  var = *tp++;
  /* Skip past the '=' and NUMEXPR tokens */
  tp += 2;

  /* Push the iteration variable and location of the FOR statement
   * onto the FOR...NEXT stack. */
  //push_for (var);
  for_stack = (struct for_stack_s *) realloc
    (for_stack, sizeof (struct for_stack_s) * ++for_stack_size);
  for_stack[for_stack_size-1].var_index = var;
  for_stack[for_stack_size-1].for_line = current_line;
  /* Make sure we save the FOR statement itself,
   * NOT the following statement! */
  for_stack[for_stack_size-1].for_statement = current_statement - 1;
  for_stack[for_stack_size-1].for_stmt = stmt;
  for_stack[for_stack_size-1].to_expr = NULL;	/* Will determine after evaluating the initial value */
  for_stack[for_stack_size-1].step_expr = NULL;	/* Will fill in later if STEP is present */

  /* Assign the result of the first expression */
  variable_values[var].num = eval_number (&tp);

  /* tp should now be pointing to the TO token. */
  if (*tp++ != TO)
    {
      fputs ("FOR: Missing TO; got ", stderr);
      list_token (--tp, stderr);
      fputc ('\n', stderr);
      // Pop the stack and abort
      --for_stack_size;
      executing = 0;
      return;
    }

  for_stack[for_stack_size-1].to_expr = tp;

  /* Evaluate the TO end of the statement */
  to_value = eval_number (&tp);
  /* tp should now be pointing to either a STEP token
   * or an end-of-statement token. */
  if ((*tp != STEP) && (*tp != '\n') && (*tp != ':') && (*tp != ELSE))
    {
      fputs ("FOR: Unrecognized token at end - ", stderr);
      list_token (tp, stderr);
      fputc ('\n', stderr);
      return;
    }

  if (*tp++ == STEP)
    {
      for_stack[for_stack_size-1].step_expr = tp;
      /* Evaluate the STEP */
      step_value = eval_number (&tp);
    } else {
      /* The default step is always 1; some programs depend on this. */
      step_value = 1.0;
    }

  /* Check whether the initial value is already past
   * the TO value, according to the sign of the step. */
  if ((step_value >= 0.0)
      ? (variable_values[var].num <= to_value)
      : (variable_values[var].num >= to_value))
    /* If we're not past the end, proceed normally. */
    return;

  // Need to locate the NEXT statement matching this FOR variable
  while (1)
    {
      stmt = find_line_statement(current_line, current_statement);
      if (stmt == NULL)
	{
	  line = (current_line == (unsigned long) -1)
	    ? NULL : find_line(++current_line, 1);
	  if (line == NULL)
	    {
	      /* End of program */
	      cmd_end (for_stack[for_stack_size-1].for_stmt);
	      return;
	    }
	  current_line = line->line_number;
	  current_statement = 0;
	  stmt = &line->statement[0];
	}
      current_statement++;
      if (stmt->command != NEXT)
	continue;
      /* Found a NEXT; see if the variable matches this FOR */
      var = stmt->tokens[1];
      if (var != for_stack[for_stack_size-1].var_index)
	continue;
      /* Pop the FOR...NEXT stack and continue with the next statement. */
      --for_stack_size;
      return;
    }
}

void
cmd_next (struct statement_header *stmt)
{
  struct statement_header *for_stmt;
  unsigned short *tp;
  double to_value, step_value;
  int i, var;

  /* Get the variable index.  Note that the order of tokens is:
   * NEXT: IDENTIFIER, name index */
  var = stmt->tokens[1];

  /* Search the FOR...NEXT stack for the variable. */
  for (i = for_stack_size - 1; i >= 0; --i)
    {
      if (for_stack[i].var_index == var)
	break;
    }
  if (i < 0)
    {
      printf ("ERROR - NEXT: NOT IN %s LOOP", name_table[var]->contents);
      executing = 0;
      return;
    }
  if (i < for_stack_size - 1)
    {
      /* We're bumping off some other FOR loops */
      for_stack_size = i + 1;
    }

  /* Check the original FOR statement that started this loop. */
  for_stmt = find_line_statement (for_stack[for_stack_size-1].for_line,
				  for_stack[for_stack_size-1].for_statement);
  if (for_stmt != for_stack[for_stack_size-1].for_stmt)
    {
      printf ("ERROR - NEXT: LOST FOR %s", name_table[var]->contents);
      /* Pop the FOR...NEXT stack */
      --for_stack_size;
      executing = 0;
      return;
    }

  /* Evaluate the TO end of the statement */
  tp = for_stack[for_stack_size-1].to_expr;
  to_value = eval_number (&tp);

  if (for_stack[for_stack_size-1].step_expr != NULL)
    {
      /* Evaluate the STEP */
      tp = for_stack[for_stack_size-1].step_expr;
      step_value = eval_number (&tp);
    } else {
      /* The default step is always 1; some programs depend on this. */
      step_value = 1.0;
    }

  /* Increment the iteration variable */
  variable_values[var].num += step_value;

  /* Compare the new value to the TO value, and reiterate the loop
   * depending on the sign of the step. */
  if ((step_value >= 0.0)
      ? (variable_values[var].num <= to_value)
      : (variable_values[var].num >= to_value))
    {
      /* Return to the statement following the FOR statement */
      current_line = for_stack[for_stack_size-1].for_line;
      current_statement = for_stack[for_stack_size-1].for_statement + 1;
    } else {
      /* The loop has finished.  Pop the FOR...NEXT stack. */
      --for_stack_size;
    }
}

void
cmd_trace (struct statement_header *stmt)
{
  unsigned short *tp = &stmt->tokens[0];
  int target = (int) tp[0];
  int state = (int) tp[1];
  int mask = 0;

  switch (target) {
  case LINES:
    mask = TRACE_LINES;
    break;
  case STATEMENTS:
    mask = TRACE_STATEMENTS;
    break;
  case EXPRESSIONS:
    mask = TRACE_EXPRESSIONS;
    break;
  case GRAMMAR:
    mask = TRACE_GRAMMAR;
    break;
  case PARSER:
    mask = TRACE_PARSER;
    break;
  }

  if (state == OFF)
    tracing &= ~mask;
  else if (state == ON)
    tracing |= mask;
}
