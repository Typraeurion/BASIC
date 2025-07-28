#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tables.h"
#include "basic.tab.h"


/* DEBUG */ extern int tracing;

/* Local Global */
static int suppress_prompt = 0;


/* Like gets(), but dynamically allocates the buffer
 * and returns a `struct string_value' */
static struct string_value *
sgets (void)
{
  struct string_value *new_string;
  int current_size, next_read;

  /* Allocate the string's initial size */
  current_size = 80;
  new_string = (struct string_value *) malloc
    (sizeof (struct string_value) + current_size);
  new_string->length = 0;
  next_read = 0;
  while (1)
    {
      /* Read in the next string segment */
      fgets (&new_string->contents[next_read],
	     current_size - next_read, stdin);
      /* If we were interrupted, don't return anything */
      if (feof (stdin) || (ferror (stdin) && (errno == EINTR)))
	{
	  free (new_string);
	  clearerr (stdin);
	  executing = 0;
	  return NULL;
	}
      next_read += strlen (&new_string->contents[next_read]);
      new_string->length = next_read;

      /* We can stop when we reach a newline */
      if (new_string->contents[next_read - 1] == '\n')
	break;
      /* Otherwise, extend the string and read again. */
      current_size += 80;
      new_string = (struct string_value *) realloc
	(new_string, sizeof (struct string_value) + current_size);
    }

  /* Remove the trailing newline */
  new_string->contents[new_string->length = next_read - 1] = '\0';

  /* The string is probably much larger than we really need,
   * so shrink it before returning. */
  current_size = WALIGN (next_read);
  new_string = (struct string_value *) realloc
    (new_string, sizeof (struct string_value) + current_size);
  return new_string;
}

/* Read a number from standard input. */
int
getdouble (double *value)
{
  int ch;

  /* Skip any leading whitespace */
  while (isspace (ch = getchar ()))
    {
      /* If the user pressed Enter without typing a number,
       * print another prompt. */
      if (ch == '\n')
	{
	  fputs ("? ", stdout);
	  fflush (stdout);
	}
    }
  ungetc (ch, stdin);
  /* If we were interrupted, don't return anything */
  if (feof (stdin) || (ferror (stdin) && (errno == EINTR)))
    {
      clearerr (stdin);
      executing = 0;
      return -1;
    }

  /* Try to get a number */
  if (scanf ("%lg", value) <= 0)
    {
      executing = 0;
      /* Check for interruption */
      if (feof (stdin) || (ferror (stdin) && (errno == EINTR)))
	{
	  clearerr (stdin);
	} else {
	  puts ("ERROR - INPUT: EXPECTED A NUMBER");
	  /* Read the rest of the line so it doesn't confuse the interpreter */
	  free (sgets ());
	}
      return -1;
    }

  /* Skip any trailing whitespace */
  while (isspace (ch = getchar ()))
    {
      /* If the user pressed Enter after typing a number, stop here. */
      if (ch == '\n')
	return 0;
    }

  /* If the number is followed by a comma, eat it up.
   * Otherwise, put the character back. */
  if (ch != ',')
    ungetc (ch, stdin);

  /* If we were interrupted clear the error */
  if (feof (stdin) || (ferror (stdin) && (errno == EINTR)))
    {
      clearerr (stdin);
      executing = 0;
      return -1;
    }

  /* Suppress the next prompt. */
  suppress_prompt = 1;
  return 0;
}

/* Ask the user for input, printing an optional prompt. */
void
cmd_input (struct statement_header *stmt)
{
  unsigned short *tp;
  double *numptr;
  struct string_value **strptr, *newstr;
  struct list_header *list;
  struct list_item *item;
  int i, var, type;

  tp = (unsigned short *) &stmt[1];
  /* If the first token is a string constant,
   * print it as a prompt */
  if (*tp == STRING)
    {
      tp++;
      fputs (((struct string_value *) tp)->contents, stdout);
      /* Skip past the string and the ';' separator */
      tp = (unsigned short *) &((char *) tp)
	[WALIGN (sizeof (struct string_value)
		 + ((struct string_value *) tp)->length + 1) + sizeof (short)];
    }

  /* The next token should start a variable list */
  if (*tp++ != ITEMLIST)
    {
      fputs ("cmd_input: Unknown token after INPUT - ", stderr);
      list_token (--tp, stderr);
      fputc ('\n', stderr);
      return;
    }
  list = (struct list_header *) tp;
  item = &list->item[0];
  for (i = 0; i < list->num_items; i++)
    {
      tp = &item->tokens[0];
      /* Get the variable type and identifier */
      type = *tp++;
      if ((type != IDENTIFIER) && (type != STRINGIDENTIFIER))
	{
	  fputs ("cmd_input: Unknown token in INPUT list - ", stderr);
	  list_token (--tp, stderr);
	  fputc ('\n', stderr);
	  return;
	}
      var = *tp++;

      /* Print out the standard prompt */
      if (!suppress_prompt)
	{
	  fputs ("? ", stdout);
	  fflush (stdout);
	} else {
	  suppress_prompt = 0;
	}

      /* Assume a simple variable */
      if (type == IDENTIFIER)
	numptr = &variable_values[var].num;
      else
	strptr = &variable_values[var].str;
      if (tracing & 2)
	fprintf (stderr, "%s", name_table[var]->contents);

      /* If this is an array, we need to do more work */
      if (*tp == '(')
	{
	  if (tracing & 2)
	    {
	      fputc ('(', stderr);
	      tracing |= 4;
	    }
	  if (*(++tp) != ITEMLIST)
	    {
	      fputs ("cmd_let(): Unexpected token ", stderr);
	      list_token (tp, stderr);
	      fprintf (stderr, " after identifier %s(\n",
		       name_table[var]->contents);
	      break;
	    }
	  ++tp;
	  if (type == IDENTIFIER)
	    {
	      numptr = num_array_lookup (var, (struct list_header *) tp);
	      	      if (numptr == NULL)
		return;
	    } else {
	      strptr = str_array_lookup (var, (struct list_header *) tp);
	      if (strptr == NULL)
		return;
	    }
	  /* Skip past the argument list and closing parenthesis */
	  tp = (unsigned short *) &((char *) tp)
	    [((struct list_header *) tp)->length];
	  ++tp;
	  if (tracing & 2)
	    {
	      fputc (')', stderr);
	      tracing &= ~4;
	    }
	}

      /* Read a value from the user, and assign it to the variable. */
      if (type == IDENTIFIER)
	{
	  /* Allow whitespace around it and a trailing comma.
	   * Note that suppress_prompt is set only if a comma is present. */
	  if (getdouble (numptr) < 0)
	    return;
	  if (tracing & 2)
	    fprintf (stderr, "=%g\n", *numptr);
	} else {
	  newstr = sgets ();
	  if (newstr == NULL)
	    return;
	  free (*strptr);
	  *strptr = newstr;
	  if (tracing & 2)
	    fprintf (stderr, "=\"%s\"\n", newstr->contents);
	}

      /* Skip to the next variable in the variable list. */
      tp = (unsigned short *) &((char *) item)[item->length];
      item = (struct list_item *) &tp[1];
    }
}

/* Find the next line containing data to read. */
static int
find_data (struct statement_header **stmtp)
{
  unsigned short *tp;
  struct line_header *line;
  struct statement_header *stmt;

  while (1)
    {
      /* Does the current data line exist? */
      line = find_line (current_data_line, 1);
      if (line == NULL)
	{
	  /* No more data. */
	  current_data_line = (unsigned long) -1;
	  return -1;
	}
      if (line->line_number != current_data_line)
	{
	  /* Line not found; go on to the next line. */
	  current_data_line = line->line_number;
	  current_data_statement = 0;
	  current_data_item = 0;
	}

      /* Does the current data statement exist? */
      stmt = find_statement (line, current_data_statement);
      if (stmt == NULL)
	{
	  /* No more statements on this line; go to the next line. */
	  current_data_line++;
	  current_data_statement = 0;
	  current_data_item = 0;
	  continue;
	}
      /* Is the current statement `DATA'? */
      while (stmt->command != DATA)
	{
	  /* Wrong statement; go to the next one */
	  current_data_statement++;
	  current_data_item = 0;
	  stmt = (struct statement_header *) &((char *) stmt)[stmt->length];
	  if ((char *) stmt >= &((char *) line)[line->length])
	    {
	      /* No `DATA' statements on this line. */
	      stmt = NULL;
	      break;
	    }
	}
      if (stmt == NULL)
	{
	  /* No `DATA' statements on this line; go to the next line. */
	  current_data_line++;
	  current_data_statement = 0;
	  continue;
	}

      /* Are there any more data items on this statement? */
      tp = (unsigned short *) &stmt[1];
      if (*tp++ != ITEMLIST)
	{
	  fputs ("find_data: Unexpected token ", stderr);
	  list_token (--tp, stderr);
	  fprintf (stderr, " following DATA statement on line %d\n",
		   line->line_number);
	  current_data_statement++;
	  current_data_item = 0;
	  continue;
	}
      if (current_data_item >= ((struct list_header *) tp)->num_items)
	{
	  /* Ran out of data; go to the next `DATA' statement. */
	  current_data_item = 0;
	  do
	    {
	      current_data_statement++;
	      stmt = (struct statement_header *)
		&((char *) stmt)[stmt->length];
	      if ((char *) stmt >= &((char *) line)[line->length])
		{
		  /* No more `DATA' statements on this line. */
		  stmt = NULL;
		  break;
		}
	    } while (stmt->command != DATA);
	  if (stmt == NULL)
	    {
	      /* No more `DATA' statements on this line; */
	      /* go to the next line. */
	      current_data_line++;
	      current_data_statement = 0;
	      continue;
	    }
	}

      /* At this point, we have some data.  Return the statement. */
      *stmtp = stmt;
      return 0;
    }
}

void
cmd_data (struct statement_header *stmt)
{
  /* When we encounter a DATA statement during execution,
   * we simply ignore the statement and move on. */
  return;
}

/* Get a value from a list of data. */
void
cmd_read (struct statement_header *stmt)
{
  unsigned short *tp;
  struct statement_header *data_stmt;
  struct list_header *data_list;
  struct list_item *data_item;
  struct list_header *read_list;
  struct list_item *read_item;
  int var_index, type, i;
  double *numptr;
  struct string_value **strptr, *newstr;

  /* Find the statement containing the next data to read */
  if (find_data (&data_stmt) < 0)
    {
      puts ("ERROR - READ: NO MORE DATA");
      executing = 0;
      return;
    }
  /* The data list structure follows the ITEMLIST token. */
  data_list = (struct list_header *) &((unsigned short *) &data_stmt[1])[1];
  /* Skip ahead to the `current_data_item'th element in the data list. */
  data_item = &data_list->item[0];
  for (var_index = 0; var_index < current_data_item; var_index++)
    data_item = (struct list_item *)
      &((char *) data_item)[data_item->length + sizeof (short)];

  tp = (unsigned short *) &stmt[1];
  if (*tp++ != ITEMLIST)
    {
      fputs ("cmd_read(): Unexpected token ", stderr);
      list_token (--tp, stderr);
      fputs (" following READ statement\n", stderr);
      return;
    }
  read_list = (struct list_header *) tp;

  read_item = &read_list->item[0];
  for (var_index = 0; var_index < read_list->num_items; var_index++)
    {
      /* Get the next variable to read into. */
      tp = &read_item->tokens[0];
      type = *tp++;
      if ((type != IDENTIFIER) && (type != STRINGIDENTIFIER))
	{
	  list_token (--tp, stderr);
	  fprintf (stderr, "at item %d of READ statement\n", var_index);
	  return;
	}
      i = *tp++;
      if (tracing & 2)
	fprintf (stderr, "%s", name_table[i]->contents);
      if (type == IDENTIFIER)
	numptr = &variable_values[i].num;
      else
	strptr = &variable_values[i].str;
      /* If this is an array, we need to do more work */
      if (*tp == '(')
	{
	  if (tracing & 2)
	    {
	      fputc ('(', stderr);
	      tracing |= 4;
	    }
	  if (*(++tp) != ITEMLIST)
	    {
	      fputs ("cmd_let(): Unexpected token ", stderr);
	      list_token (tp, stderr);
	      fprintf (stderr, " after identifier %s(\n",
		       name_table[i]->contents);
	      return;
	    }
	  ++tp;
	  if (type == IDENTIFIER)
	    {
	      numptr = num_array_lookup (i, (struct list_header *) tp);
	      if (numptr == NULL)
		return;
	    } else {
	      strptr = str_array_lookup (i, (struct list_header *) tp);
	      if (strptr == NULL)
		return;
	    }
	  if (tracing & 2)
	    {
	      fputc (')', stderr);
	      tracing &= ~4;
	    }
	}

      /* Check that the next data type matches the variable type */
      tp = &data_item->tokens[0];
      if ((type == STRINGIDENTIFIER) != (*tp == STRING))
	{
	  puts ("ERROR - READ: WRONG DATA TYPE");
	  executing = 0;
	  return;
	}
      /* Assign the value */
      if (type == IDENTIFIER)
	{
	  *numptr = eval_number (&tp);
	  if (tracing & 2)
	    fprintf (stderr, "=%g\n", *numptr);
	} else {
	  newstr = eval_string (&tp);
	  if (newstr == NULL)
	    return;
	  free (*strptr);
	  *strptr = newstr;
	  if (tracing & 2)
	    fprintf (stderr, "=\"%s\"\n", newstr->contents);
	}

      /* Advance to the next `READ' variable and `DATA' item.
       * Skip the `,` token in both cases. */
      read_item = (struct list_item *)
	&((char *) read_item)[read_item->length + sizeof (short)];
      data_item = (struct list_item *)
	&((char *) data_item)[data_item->length + sizeof (short)];
      current_data_item++;

      /* Skip actually looking up the next item
       * if we're not going to read any more. */
      if (var_index + 1 >= read_list->num_items)
	break;
      if (current_data_item >= data_list->num_items)
	{
	  /* Find the next `DATA' statement. */
	  current_data_item = 0;
	  current_data_statement++;
	  if (find_data (&data_stmt) < 0)
	    {
	      puts ("ERROR - READ: NO MORE DATA");
	      executing = 0;
	      return;
	    }

	  /* Reset the data list and data item pointers */
	  data_list = (struct list_header *)
	    &((unsigned short *) &data_stmt[1])[3];
	  data_item = &data_list->item[0];
	}
    }
}
