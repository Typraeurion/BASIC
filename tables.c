#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tables.h"
#include "basic.tab.h"


/* Global data */

/* The variable name table. */
int name_table_size = 0;	/* Number of names defined	*/
struct string_value **name_table = NULL;

/* There is one variable for each name defined. */
var_u *variable_values = NULL;

/* The function table. */
int function_table_size = 0;	/* Number of functions defined	*/
struct fndef *function_table = NULL;


/* Free the memory used by a function or array */
void
free_function (struct fndef *fn)
{
  if (fn->expr != NULL)
    {
      free (fn->expr);
      fn->expr = NULL;
    }
  if (fn->array_dimension != NULL)
    {
      free (fn->array_dimension);
      fn->array_dimension = NULL;
    }
  if (fn->array_data != NULL)
    {
      free (fn->array_data);
      fn->array_data = NULL;
    }
}


/* Initialize the tables used in BASIC execution */
void
initialize_tables (void)
{
  int i;

  for (i = 0; i < name_table_size; i++)
    {
      if ((name_table[i]->contents[name_table[i]->length - 1] == '$')
	  && (variable_values[i].str != NULL))
	free (variable_values[i].str);
      free (name_table[i]);
    }
  name_table_size = 0;

  for (i = 0; i < function_table_size; i++)
    free_function (&function_table[i]);
  function_table_size = 0;

  for (i = 0; i < program_size; i++)
    free (program[i]);
  program_size = 0;

  initialize_builtin_functions ();
}

/* This function returns the index of a variable name in the name table.
 * If the name does not already exist, it is added. */
unsigned short
find_var_name (const char *name)
{
  int i, len;

  for (i = 0; i < name_table_size; i++)
    {
      if (strcasecmp (name, name_table[i]->contents) == 0)
	return i;
    }

  /* Name not found; add it to the table */
  name_table = (struct string_value **) realloc
    (name_table, sizeof (struct string_value *) * ++name_table_size);
  len = strlen (name);
  name_table[i] = (struct string_value *) calloc
    (1, WALIGN (sizeof (struct string_value) + len + 1));
  name_table[i]->length = len;
  *((short *) &name_table[i]->contents[len & ~1]) = 0;
  memcpy (name_table[i]->contents, name, len);

  /* Add to the variable arrays */
  variable_values = (var_u *) realloc
    (variable_values, sizeof (var_u) * (i + 1));
  if (name[len - 1] == '$')
    variable_values[i].str = NULL;
  else
    variable_values[i].num = 0.0;

  return i;
}


/* Common routine to map an identifier to a function table entry */
static struct fndef *
find_function (unsigned short id)
{
  int i;

  for (i = 0; i < function_table_size; i++)
    {
      if (function_table[i].name_index == id)
	return &function_table[i];
    }
  return NULL;
}

/* Common routine to evaluate a list of indices */
static int
get_indices (struct list_header *index_list, unsigned short *indices,
	     int nargs, int dim_or_index)
{
  int i;
  struct list_item *lp;
  unsigned short *tp;
  double d;

  if (nargs != index_list->num_items)
    {
      /* We don't give this error for DIM, since DIM defines the dimensions */
      if (current_line == (unsigned long) -1)
	printf ("ERROR - WRONG NUMBER OF INDICES");
      else
	printf ("ERROR - WRONG NUMBER OF INDICES ON LINE %d", current_line);
      executing = 0;
      return -1;
    }
  lp = &index_list->item[0];
  for (i = 0; i < nargs; i++)
    {
      tp = &lp->tokens[0];
      /* Is this a valid index? */
      if (*tp == STREXPR)
	{
	  if (current_line == (unsigned long) -1)
	    printf ("ERROR - INDEX #%d IS NOT A NUMBER", i + 1);
	  else
	    printf ("ERROR - INDEX #%d IS NOT A NUMBER ON LINE %d",
		    i + 1, current_line);
	  executing = 0;
	  return -1;
	}
      d = eval_number (&tp);
      if (tracing & TRACE_EXPRESSIONS) // FIXME: Changed from 4 (set by cmd_let)
	{
	  fprintf (stderr, "%g", d);
	  if (i < nargs - 1)
	    fputc (',', stderr);
	}
      /* Is this a valid index? */
      if ((d < 0.0) || (d > (double) UINT_MAX))
	{
	  if (current_line == (unsigned long) -1)
	    printf ((dim_or_index == DIM)
		    ? "ERROR - DIMENSION #%d OUT OF RANGE"
		    : "ERROR - INDEX #%d OUT OF RANGE", i + 1);
	  else
	    printf ((dim_or_index == DIM)
		    ? "ERROR - DIMENSION #%d OUT OF RANGE ON LINE %d"
		    : "ERROR - INDEX #%d OUT OF RANGE ON LINE %d",
		    i + 1, current_line);
	  executing = 0;
	  return -1;
	}
      indices[i] = (unsigned short) d;
      /* Skip the list delimiter (',') */
      tp = (unsigned short *) &((char *) lp)[lp->length];
      lp = (struct list_item *) &tp[1];
    }
  return 0;
}

/* Common routine to evaluate a list of numeric or string arguments */
static int
get_arguments (struct list_header *arg_list, var_u *args,
	       int nargs, unsigned long argtypes)
{
  int i;
  struct list_item *lp;
  unsigned short *tp;

  if (nargs != arg_list->num_items)
    {
      printf ("ERROR - WRONG NUMBER OF ARGUMENTS ON LINE %d", current_line);
      executing = 0;
      return -1;
    }
  lp = &arg_list->item[0];
  for (i = 0; i < nargs; i++)
    {
      tp = &lp->tokens[0];
      /* Is this a valid argument? */
      switch (*tp)
	{
	case NUMEXPR:
	  if (argtypes & (1 << i))
	    {
	      printf ("ERROR - ARGUMENT %d IS WRONG TYPE ON LINE %d",
		      i + 1, current_line);
	      executing = 0;
	      return -1;
	    }
	  args[i].num = eval_number (&tp);
	  break;

	case STREXPR:
	  if (!(argtypes & (1 << i)))
	    {
	      printf ("ERROR - ARGUMENT %d IS WRONG TYPE ON LINE %d",
		      i + 1, current_line);
	      executing = 0;
	      return -1;
	    }
	  args[i].str = eval_string (&tp);
	  break;

	default:
	  fputs ("get_arguments(): Unknown argument type", stderr);
	  list_token (tp, stderr);
	  return -1;
	}

      /* Skip the argument delimiter (',') */
      tp = (unsigned short *) &((char *) lp)[lp->length];
      lp = (struct list_item *) &tp[1];
    }
  return 0;
}


/* Define a variable as an array. */
static void
dim (unsigned short id, struct list_header *size_list)
{
  int i, dimensions;
  struct fndef *array;
  struct list_item *lp;
  unsigned short *tp;
  unsigned short dimension[32];
  unsigned long total_size;
  double d;

  /* If this variable is already being used, remove the current definition */
  array = find_function (id);
  if (array != NULL)
    {
      if (array->built_in != NULL)
	{
	  printf ("ERROR - DIM: %s IS A FUNCTION\n",
		  name_table[array->name_index]->contents);
	  executing = 0;
	  return;
	}
      free_function (array);
    } else {
      /* Add another entry to the function table */
      function_table = (struct fndef *) realloc
	(function_table, sizeof (struct fndef) * ++function_table_size);
      array = &function_table[function_table_size - 1];
      memset (array, 0, sizeof (struct fndef));
      array->name_index = id;
      array->type =
	(name_table[id]->contents[name_table[id]->length - 1] == '$')
	? '$' : 0;
    }

  /* Get the dimensions of the array */
  array->num_args = dimensions = size_list->num_items;
  if (get_indices (size_list, dimension, dimensions, DIM))
    return;

  total_size = 1;
  for (i = 0; i < dimensions; i++)
    {
      /* 1 million elements seems to be
       * a reasonable maximum limit for BASIC data */
      if (total_size >= (1 << 20) / ((unsigned int) dimension[i] + 1))
	{
	  puts ("ERROR - DIM: ARRAY TOO BIG");
	  executing = 0;
	  return;
	}
      /* The *real* dimension is d+1, since BASIC programs
       * may start indices at 0 as well as 1. */
      total_size *= (unsigned int) dimension[i] + 1;
    }

  /* Allocate the array */
  array->array_data = calloc (total_size,
			      ((array->type == '$')
			       ? sizeof (struct string_value *)
			       : sizeof (double)));
  if (array->array_data == NULL)
    {
      puts ("ERROR - DIM: OUT OF MEMORY");
      executing = 0;
      return;
    }
  /* If this is a numeric array, initialize the elements to 0.0 */
  if (!array->type)
    {
      double *dp = (double *) array->array_data;
      while (total_size--)
	*dp++ = 0.0;
    }

  /* Copy the dimensions */
  array->array_dimension = (unsigned short *) malloc
    (sizeof (short) * dimensions);
  memcpy (array->array_dimension, dimension, sizeof (short) * dimensions);
  array->argtypes = 0;
}


/* All array lookups use the same algorithm to find the item;
 * only the item size and return values are different. */
static unsigned long
array_lookup (struct fndef *array, unsigned short *index_list)
{
  int i, num_items = array->num_args;
  unsigned long total_index;
  double d;

  /* Get the indices */
  total_index = 0;
  for (i = 0; i < num_items; i++)
    {
      if (index_list[i] > array->array_dimension[i])
	{
	  if (current_line == (unsigned long) -1)
	    printf ("ERROR - INDEX #%d OUT OF RANGE", i + 1);
	  else
	    printf ("ERROR - INDEX #%d OUT OF RANGE ON LINE %d",
		    i + 1, current_line);
	  executing = 0;
	  return -1;
	}
      total_index = ((total_index
		      /* The *real* dimension is d+1, since BASIC programs
		       * may start indices at 0 as well as 1. */
		      * ((unsigned int) array->array_dimension[i] + 1))
		     + (unsigned int) index_list[i]);
    }

  /* We have the index; that's all we need here. */
  return total_index;
}

double *
num_array_lookup (unsigned short id, struct list_header *index_list)
{
  struct fndef *array;
  unsigned short indices[32];
  unsigned long index;

  /* Find the array element */
  array = find_function (id);
  if ((array == NULL) || (array->array_dimension == NULL))
    {
      printf ("ERROR - %s IS NOT AN ARRAY", name_table[id]->contents);
      executing = 0;
      return NULL;
    }
  if (array->type == '$')
    {
      fprintf (stderr, "num_array_lookup: %s is a string array\n",
	       name_table[id]->contents);
      return NULL;
    }
  if (get_indices (index_list, indices, array->num_args, 0))
    return NULL;
  index = array_lookup (array, indices);
  if (index == (unsigned long) -1)
    return NULL;

  /* Return the reference to the correct numeric value */
  return &((double *) array->array_data)[index];
}

struct string_value **
str_array_lookup (unsigned short id, struct list_header *index_list)
{
  struct fndef *array;
  unsigned short indices[32];
  unsigned long index;

  /* Find the array element */
  array = find_function (id);
  if ((array == NULL) || (array->array_dimension == NULL))
    {
      printf ("ERROR - %s IS NOT AN ARRAY", name_table[id]->contents);
      executing = 0;
      return NULL;
    }
  if (array->type != '$')
    {
      fprintf (stderr, "str_array_lookup: %s is a numeric array\n",
	       name_table[id]->contents);
      return NULL;
    }
  if (get_indices (index_list, indices, array->num_args, 0))
    return NULL;
  index = array_lookup (array, indices);
  if (index == (unsigned long) -1)
    return NULL;

  /* Return the reference to the correct string pointer */
  return &((struct string_value **) array->array_data)[index];
}


/* Evaluate a numeric or string function or find an array element.
 * We won't know which it is until we look up the identifier. */
var_u
eval_fn_or_array (unsigned short id, struct list_header *arg_list)
{
  int i;
  struct list_item *lp;
  unsigned short *tp;
  struct fndef *fn_or_array;
  var_u args[32], result;
  unsigned long arg_types = 0;

  fn_or_array = find_function (id);
  if (fn_or_array == NULL)
    {
      fprintf (stderr,
	       "eval_fn_or_array(): called for %s, which is not in the function table\n",
	       name_table[id]->contents);
      return (var_u) (struct string_value *) NULL;
    }

  /* Call the appropriate subroutine depending on whether this is
   * a built-in function, user-defined function, or array */
  if (fn_or_array->built_in != NULL)
    {
      /* Get the arguments */
      if (get_arguments (arg_list, args,
			 fn_or_array->num_args, fn_or_array->argtypes))
	return ((fn_or_array->type == '$')
		? (var_u) (struct string_value *) NULL : (var_u) 0.0);
      /* Compute the result */
      result = fn_or_array->built_in (args);
      /* Free any string arguments */
      for (i = 0; i < fn_or_array->num_args; i++)
	{
	  if (fn_or_array->argtypes & (1 << i))
	    free (args[i].str);
	}
      return result;
    }

  if (fn_or_array->expr != NULL)
    {
      fputs ("eval_fn_or_array(): User-defined expressions not yet implemented\n", stderr);
      return ((fn_or_array->type == '$')
	      ? (var_u) (struct string_value *) NULL : (var_u) 0.0);
    }

  if (fn_or_array->array_dimension != NULL)
    {
      unsigned long index;
      unsigned short indices[32];

      /* Find the index into the array first */
      if (get_indices (arg_list, indices, fn_or_array->num_args, 0))
	return ((fn_or_array->type == '$')
		? (var_u) (struct string_value *) NULL : (var_u) 0.0);
      index = array_lookup (fn_or_array, indices);
      if (index == (unsigned long) -1)
	return ((fn_or_array->type == '$')
		? (var_u) (struct string_value *) NULL : (var_u) 0.0);

      if (fn_or_array->type == '$')
	{
	  struct string_value *sresult, *sp;
	  sp = ((struct string_value **) fn_or_array->array_data)[index];
	  /* We must return a *copy* of the string, -not- the string itself! */
	  sresult = (struct string_value *) calloc
	    (1, WALIGN (sizeof (struct string_value)
			+ ((sp == NULL) ? 0 : sp->length) + 1));
	  if (sp != NULL)
	    memcpy (sresult, sp,
		    WALIGN (sizeof (struct string_value) + sp->length + 1));
	  return (var_u) sresult;
	} else {
	  return (var_u) ((double *) fn_or_array->array_data)[index];
	}
    }

  fprintf (stderr, "eval_fn_or_array(): Function table entry for\n"
	   " %s does not have a definition\n", name_table[id]->contents);
  return ((fn_or_array->type == '$')
	  ? (var_u) (struct string_value *) NULL : (var_u) 0.0);
}


void
cmd_dim (struct statement_header *stmt)
{
  struct list_header *dim_list;
  struct list_item *lp;
  unsigned short *tp;
  int i, id;

  tp = &stmt->tokens[0];
  if (*tp != ITEMLIST)
    {
      fputs ("cmd_dim(): unexpected token ", stderr);
      list_token (tp, stderr);
      fputc ('\n', stderr);
      return;
    }
  dim_list = (struct list_header *) &tp[1];
  lp = &dim_list->item[0];
  for (i = 0; i < dim_list->num_items; i++)
    {
      tp = &lp->tokens[0];
      if ((*tp != IDENTIFIER) && (*tp != STRINGIDENTIFIER))
	{
	  fputs ("cmd_dim(): unexpected token ", stderr);
	  list_token (tp, stderr);
	  fputc ('\n', stderr);
	  return;
	}
      ++tp;
      id = *tp++;
      if (*tp != '(')
	{
	  fputs ("cmd_dim(): unexpected token ", stderr);
	  list_token (tp, stderr);
	  fprintf (stderr, " after identifier %s\n", name_table[id]->contents);
	  return;
	}
      if (*(++tp) != ITEMLIST)
	{
	  fputs ("cmd_dim(): unexpected token ", stderr);
	  list_token (tp, stderr);
	  fprintf (stderr, " after identifier %s(\n",
		   name_table[id]->contents);
	  return;
	}
      ++tp;
      dim (id, (struct list_header *) tp);

      /* Skip the item delimiter (',') */
      tp = (unsigned short *) &((char *) lp)[lp->length];
      lp = (struct list_item *) &tp[1];
    }
}

/* DEBUG: Dump the current program's data structures */
void
dump (void)
{
  int i;

  printf ("BASIC: %d entries in the name table:\n", name_table_size);
  for (i = 0; i < name_table_size; i++)
    {
      if (name_table[i]->contents[name_table[i]->length - 1] == '$')
	printf ("\t%2d: %s=\"%s\"\n", i, name_table[i]->contents,
		(variable_values[i].str == NULL) ? ""
		: variable_values[i].str->contents);
      else
	printf ("\t%2d: %s = %G\n", i, name_table[i]->contents,
		variable_values[i].num);
    }

  printf ("%d entries in the function table:\n", function_table_size);
  for (i = 0; i < function_table_size; i++)
    {
      int j;

      printf ("\t%2d: %s(", i,
	      name_table[function_table[i].name_index]->contents);
      if (function_table[i].array_dimension == NULL)
	{
	  for (j = 0; j < function_table[i].num_args; j++)
	    printf ("%c,", (j < 3) ? ('X' + j)
		    : ((j < 26) ? ('A' + j - 3) : ('i' + j - 26)));
	} else {
	  for (j = 0; j < function_table[i].num_args; j++)
	    printf ("%d,", function_table[i].array_dimension[j]);
	}
      printf ("\b)\n");
    }

  printf ("%d lines in the program:\n", program_size);
  for (i = 0; i < program_size; i++)
    {
      int j;

      printf ("%5u: %d bytes:", program[i]->line_number, program[i]->length);
      for (j = 0; j < program[i]->length / sizeof (short); j++)
	printf (" %04X", ((unsigned short *) &program[i]->statement)[j]);
      printf ("\n");
    }

  printf ("End of list\n\n");
}
