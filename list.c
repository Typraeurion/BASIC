#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef YY_HEADER_EXPORT_START_CONDITIONS
// Force lex.yy.h to define INITIAL, which we use in the LOAD command.
#define YY_HEADER_EXPORT_START_CONDITIONS 1
#endif
#include "lex.yy.h"
#include "tables.h"
#include "basic.tab.h"


/* Global data */

/* The program. */
int program_size = 0;		/* Number of lines in the program */
struct line_header **program = NULL;


/* Constant data */

/* Token number -> name conversion */
const struct {
  unsigned short number;
  const char *name;
} token_map[] = {
  { BYE, "BYE" },
  { CONTINUE, "CONTINUE" },
  { DATA, "DATA " },
  { DEF, "DEF " },
  { DIM, "DIM " },
  { ELSE, " ELSE " },
  { EXPRESSIONS, "EXPRESSIONS " },
  { END, "END" },
  { ERROR, "ERROR" },
  { FOR, "FOR " },
  { GOSUB, "GOSUB " },
  { GOTO, "GOTO " },
  { _GOTO_, "" },	/* Implied `GOTO' */
  { GRAMMAR, "GRAMMAR " },
  { IF, "IF " },
  { INPUT, "INPUT " },
  { LET, "LET " },
  { _LET_, "" },	/* Implied `LET' */
  { LINES, "LINES " },
  { LIST, "LIST " },
  { LOAD, "LOAD " },
  { NEW, "NEW" },
  { NEXT, "NEXT " },
  { OFF, "OFF" },
  { ON, "ON " },
  { PAUSE, "PAUSE " },
  { PARSER, "PARSER " },
  { PRINT, "PRINT " },
  { READ, "READ " },
  { REM, "REM " },
  { RESTORE, "RESTORE " },
  { RETURN, "RETURN" },
  { RUN, "RUN" },
  { SAVE, "SAVE " },
  { STATEMENTS, "STATEMENTS " },
  { STEP, " STEP " },
  { STOP, "STOP" },
  { THEN, " THEN " },
  { _THEN_, " " },
  { TO, " TO " },
  { TRACE, "TRACE " },
  /* Arithmetic operators */
  { OR, " OR " },
  { AND, " AND " },
  { LESSEQ, "<=" },
  { GRTREQ, ">=" },
  { NOTEQ, "<>" },
  { NEG, "-" },
  /* Non-keyword tokens */
  { TAB, "TAB" },
};


/* This function returns the line with the given number.
 * If the flag is true and the given line does not exist,
 * the next existing line is returned. */
struct line_header *
find_line (unsigned long number, int after)
{
  int i;

  if ((signed long) number == -1) {
    /* The reference is to the immediate (unnumbered) line. */
    if ((immediate_line != NULL) || !after)
    return immediate_line;
  }

  for (i = 0; i < program_size; i++)
    {
      if (program[i]->line_number >= number)
	{
	  if (!after && (program[i]->line_number > number))
	    return NULL;
	  return program[i];
	}
    }
  return NULL;
}

/* This function adds a new line to the program.
 * If a line with the same number already exists, it is deleted first. */
void
add_line (struct line_header *line)
{
  int i;

  for (i = 0; i < program_size; i++)
    {
      if (program[i]->line_number < line->line_number)
	continue;
      if (program[i]->line_number == line->line_number)
	{
	  /* This line already exists; replace it. */
	  free (program[i]);
	  program[i] = line;
	  return;
	}
      else /* (program[i]->line_number > line->line_number) */
	{
	  /* Add another line to the program array */
	  program = (struct line_header **) realloc
	    (program, sizeof (struct line_header *) * ++program_size);
	  /* Insert the new line at `i'. */
	  memmove (&program[i + 1], &program[i],
		   sizeof (struct line_header *) * (program_size - (i + 1)));
	  program[i] = line;
	  return;
	}
    }
  /* This line goes at the end of the program. */
  program = (struct line_header **) realloc
	    (program, sizeof (struct line_header *) * (++program_size));
  program[i] = line;
  return;
}

/* Remove a line from the program. */
void
remove_line (unsigned long number)
{
  int i;

  for (i = 0; i < program_size; i++)
    {
      if (program[i]->line_number < number)
	continue;
      if (program[i]->line_number == number)
	{
	  free (program[i]);
	  memmove (&program[i], &program[i + 1],
		   sizeof (struct line_header *) * (--program_size - i));
	  return;
	}
      else /* Line not found */
	return;
    }
}

int
list_token (unsigned short *tp, FILE *to)
{
  int i;

  if ((*tp < CHAR_MAX) && ((*tp >= ' ') || (*tp == '\n')))
    {
#ifndef DEBUG
      /* Don't print implied parentheses */
      if ((*tp == '{') || (*tp == '}'))
	return 1;
#endif
      fputc (*tp, to);
      return 1;
    }
  for (i = 0; i < sizeof (token_map) / sizeof (token_map[0]); i++)
    {
      if (token_map[i].number == *tp)
	{
	  /* Special case: If a `GOTO' or `GOSUB' appears here,
	   * it's part of an ON...GO statement, and needs a leading space. */
	  if ((*tp == GOTO) || (*tp == GOSUB))
	    fputc (' ', to);
	  fputs (token_map[i].name, to);
	  return 1;
	}
    }
  switch ((int) *tp)
    {
    case RESTOFLINE:
      fputs (((struct string_value *) &tp[1])->contents, to);
      return (2 + (WALIGN (((struct string_value *) &tp[1])->length + 1)
		   / sizeof (short)));

    case STRING:
      fprintf (to, "\"%s\"", ((struct string_value *) &tp[1])->contents);
      return (2 + (WALIGN (((struct string_value *) &tp[1])->length + 1)
		   / sizeof (short)));

    case INTEGER:
      fprintf (to, "%u", *((unsigned long *) &tp[1]));
      return (1 + sizeof (long) / sizeof (short));

    case FLOATINGPOINT:
      fprintf (to, "%G", *((double *) &tp[1]));
      return (1 + sizeof (double) / sizeof (short));

    case IDENTIFIER:
    case STRINGIDENTIFIER:
      fputs (name_table[tp[1]]->contents, to);
      return 2;

    case ITEMLIST:
      {
	/* Recursively list the tokens and separators in this list. */
	int j, k;
	struct list_header *lhp;
	struct list_item *lip;

#ifdef DEBUG
	fputc ('[', to);
#endif
	tp++;
	lhp = (struct list_header *) tp;
	lip = &lhp->item[0];
	for (j = 0; j < lhp->num_items; j++)
	  {
	    for (k = 0; k < ((lip->length - sizeof (struct list_item))
			     / sizeof (short)); )
		k += list_token (&lip->tokens[k], to);
	    if ((char *) &lip->tokens[k] >= &((char *) tp)[lhp->length])
	      break;
	    /* This should be the separator; it's
	     * not included in the list item length. */
	    fputc (lip->tokens[k++], to);
	    lip = (struct list_item *)
	      &((char *) lip)[lip->length + sizeof(short)];
	  }
#ifdef DEBUG
	fputc (']', to);
#endif
	return (lhp->length / sizeof (short) + 1);
      }

    case NUMEXPR:
#ifdef DEBUG
      fputs ("<#>", to);
#endif
      return 1;

    case PRINTLIST:
      {
	/* Recursively list the tokens in this list */
	int j, k;
	struct list_header *lhp;
	struct list_item *lip;

#ifdef DEBUG
	fputc ('[', to);
#endif
	tp++;
	lhp = (struct list_header *) tp;
	lip = &lhp->item[0];
	for (j = 0; j < lhp->num_items; j++)
	  {
	    for (k = 0; k < ((lip->length - sizeof (struct list_item))
			     / sizeof (short)); )
	      k += list_token (&lip->tokens[k], to);
	    if ((char *) &lip->tokens[k] >= &((char *) tp)[lhp->length])
	      break;
	    lip = (struct list_item *) &((char *) lip)[lip->length];
	  }
#ifdef DEBUG
	fputc (']', to);
#endif
	return (lhp->length / sizeof (short) + 1);
      }

    case STREXPR:
#ifdef DEBUG
      fputs ("<$>", to);
#endif
      return 1;

    case NUMLVAL:
#ifdef DEBUG
      fputs ("<&#>", to);
#endif
      return 1;

    case STRLVAL:
#ifdef DEBUG
      fputs ("<&$>", to);
#endif
      return 1;

    default:
      fflush (to);
      fprintf (stderr, "## Unhandled token %04X at %p ##", tp[0], tp);
      return 1;
    }
}

void
list_statement (struct statement_header *sp, FILE *to)
{
  int i;
  unsigned short *tp;

  for (i = 0; i < sizeof (token_map) / sizeof (token_map[0]); i++)
    {
      if (token_map[i].number == sp->command)
	{
	  fputs (token_map[i].name, to);
	  /* IF statements need special handling, as they
	   * include additional offset fields to nested
	   * statements for their THEN and ELSE clauses. */
	  if (sp->command == IF) {
	    struct if_header *ifp = (struct if_header *) sp;
	    struct statement_header *then_sp = (struct statement_header *)
	      &((char *) ifp)[ifp->then_offset];
	    /* struct statement_header *else_sp = (struct statement_header *)
	    &((char *) ifp)[ifp->else_offset + sizeof(short)]; */
	    for (tp = &ifp->tokens[0]; (void *) tp < (void *) then_sp; )
	      tp += list_token(tp, to);
	    /* list_statement (then_sp, to);
	    if ((char *) else_sp < &((char *) ifp)[ifp->length])
	      list_statement (else_sp, to); */
	  }
	  else {
	    for (tp = &sp->tokens[0]; (char *) tp < &((char *) sp)[sp->length]; )
	      {
		tp += list_token (tp, to);
	      }
	  }
	  return;
	}
    }
  printf ("#UNKNOWN TOKEN %04X#", sp->command);
}

void
list_line (struct line_header *lp, FILE *to)
{
  struct statement_header *sp;

  if (lp->line_number != (unsigned long) -1)
    fprintf (to, "%d ", lp->line_number);
  for (sp = &lp->statement[0]; (char *) sp < &((char *) lp)[lp->length];
       sp = (struct statement_header *) &((char *) sp)[sp->length])
    {
      list_statement (sp, to);
    }
}


void
cmd_list (struct statement_header *stmt)
{
  unsigned long start_line, end_line;
  unsigned short *tp = &stmt->tokens[0];
  struct line_header *lp;

  if ((*tp == ':') || (*tp == '\n') || (*tp == ELSE))
    {
      start_line = 0;
      end_line = (unsigned long) -1;
    } else {
      if (*tp != INTEGER)
	{
	  fputs ("Error: LIST command followed by token ", stderr);
	  list_token (tp, stderr);
	  fputc ('\n', stderr);
	  return;
	}
      start_line = *((unsigned long *) (++tp));
      tp = (unsigned short *) &((unsigned long *) tp)[1];
      if ((*tp == ':') || (*tp == '\n') || (*tp == ELSE))
	{
	  end_line = start_line;
	}
      else if ((*tp != ',') || (*(++tp) != INTEGER))
	{
	  fputs ("Error: LIST command followed by token ", stderr);
	  list_token (tp, stderr);
	  fputc ('\n', stderr);
	  return;
	} else {
	  end_line = *((unsigned long *) (++tp));
	  tp = (unsigned short *) &((unsigned long *) tp)[1];
	  if ((*tp != ':') && (*tp != '\n') && (*tp != ELSE))
	    {
	      fputs ("Error: LIST command followed by token ", stderr);
	      list_token (tp, stderr);
	      fputc ('\n', stderr);
	      return;
	    }
	}
    }

  for (lp = find_line (start_line, 1);
       (lp != NULL) && (lp->line_number <= end_line);
       lp = find_line (lp->line_number + 1, 1))
    list_line (lp, stdout);
}

void
cmd_load (struct statement_header *stmt)
{
  struct string_value *filename;
  FILE *load_file;
  struct line_header *lp;

  if (stmt->tokens[0] != STRING)
    {
      fputs ("Error: LOAD command followed by token ", stderr);
      list_token (&stmt->tokens[0], stderr);
      fputc ('\n', stderr);
      return;
    }
  if (current_load_nesting >= MAX_LOAD_NESTING)
    {
      fputs ("Error: Maximum LOAD nesting exceeded\n", stderr);
      return;
    }
  filename = (struct string_value *) &stmt->tokens[1];
  load_file = fopen (filename->contents, "r");
  if (load_file == NULL)
    {
      perror ("ERROR - LOAD: ");
      return;
    }

  if (tracing & (TRACE_PARSER | TRACE_STATEMENTS))
    fprintf (stderr, "Switching parser input to %s\n", filename);

  yypush_buffer_state(yy_create_buffer(load_file, YY_BUF_SIZE));
  current_load_nesting++;
}

void
cmd_save (struct statement_header *stmt)
{
  struct string_value *filename;
  FILE *save_file;
  struct line_header *lp;

  if (stmt->tokens[0] != STRING)
    {
      fputs ("Error: SAVE command followed by token ", stderr);
      list_token (&stmt->tokens[0], stderr);
      fputc ('\n', stderr);
      return;
    }
  filename = (struct string_value *) &stmt->tokens[1];
  save_file = fopen (filename->contents, "w");
  if (save_file == NULL)
    {
      perror ("ERROR - SAVE: ");
      return;
    }

  if (tracing & TRACE_STATEMENTS)
    fprintf (stderr, "Saving program to %s\n", filename);
  for (lp = find_line (0, 1); lp != NULL;
       lp = find_line (lp->line_number + 1, 1))
    list_line (lp, save_file);

  fclose (save_file);
  if (tracing & TRACE_STATEMENTS)
    fprintf (stderr, "Finished saving %s\n", filename);
}

void
cmd_new (struct statement_header *stmt)
{
  cmd_end (stmt);
  initialize_tables ();
}
