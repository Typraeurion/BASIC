#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "basic.tab.h"
#include "tables.h"


/* Global data */
int current_column;

void
cmd_print (struct statement_header *stmt)
{
  unsigned short *tp;
  struct list_item *lp;
  struct string_value *str;
  double num;
  int i, items;
  unsigned short last_token = 0;

  /* If there is no print list in this statement,
   * print a blank line. */
  tp = &stmt->tokens[0];
  if (*tp != PRINTLIST)
    {
      fputc ('\n', stdout);
      current_column = 0;
      return;
    }
  tp++;
  items = ((struct list_header *) tp)->num_items;
  lp = &((struct list_header *) tp)->item[0];
  for (i = 0; i < items; i++)
    {
      tp = &lp->tokens[0];
      last_token = *tp;
      switch ((int) last_token)
	{
	case ';':
	  /* Semantic separator; no output */
	  break;

	case ',':
	  /* Skip to the next tab stop */
	  printf ("%*s", ((current_column + 8) & ~7) - current_column, "");
	  current_column = (current_column + 8) & ~7;
	  break;

	case TAB:
	  /* Skip past the open parenthesis, evaluate the numeric argument */
	  tp += 2;
	  num = eval_number (&tp);
	  /* Move ahead to the given column */
	  if ((int) num > current_column)
	    {
	      printf ("%*s", (int) num - current_column, "");
	      current_column = (int) num;
	    }
	  break;

	case NUMEXPR:
	  /* Evaluate the numeric print item.  The Darthmoth BASIC
	   * documentation states that numbers are printed with a
	   * leading space for positive numbers and a trailing space
	   * so there is at least one space between numeric values
	   * separated by a semicolon. */
	  tp++;
	  num = eval_number (&tp);
	  current_column += printf ("% G ", num);
	  break;

	case STREXPR:
	  /* Evaluate the string print item */
	  tp++;
	  str = eval_string (&tp);
	  current_column += printf ("%s", str->contents);
	  /* The string is just temporary; release it now that we're done */
	  free (str);
	  break;

	default:
	  fputs ("cmd_print(): Unknown token in print string - ", stderr);
	  list_token (tp, stderr);
	}

      /* Next item */
      lp = (struct list_item *) &((char *) lp)[lp->length];
    }

  /* If the last print item was not a ',' or ';' separator, end with a newline. */
  switch ((int) last_token) {
  case ',':
  case ';':
    break;
  default:
    fputc ('\n', stdout);
    current_column = 0;
  }
}
