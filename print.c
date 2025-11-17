#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "basic.tab.h"
#include "tables.h"


/* Global data */
static int current_column;

void
cmd_print (struct statement_header *stmt)
{
  unsigned short *tp;
  struct list_item *lp;
  struct string_value *str;
  double num;
  int i, items;

  /* If there is no print list in this statement,
   * print a blank line. */
  tp = &stmt->tokens[0];
  if (*tp != ITEMLIST)
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
      switch ((int) *tp)
	{
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

      /* Find out which separator is being used */
      tp = (unsigned short *) &((char *) lp)[lp->length];
      switch ((int) *tp)
	{
	case ':':
	case '\n':
	case ELSE:
	  /* End of statement; print a newline */
	  fputc ('\n', stdout);
	  current_column = 0;

	case ';':
	  /* Keep the following text together */
	  break;

	case ',':
	  /* Skip to the next tab stop */
	  printf ("%*s", ((current_column + 8) & ~7) - current_column, "");
	  current_column = (current_column + 8) & ~7;
	  break;

	default:
	  fputs ("cmd_print(): Unknown separator in print string - ", stderr);
	  list_token (tp, stderr);
	}

      /* Next item */
      lp = (struct list_item *) ++tp;
    }
}
