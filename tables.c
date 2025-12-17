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
  if (fn->arg_ids != NULL)
    {
      free (fn->arg_ids);
      fn->arg_ids = NULL;
    }
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

/* Common routine to evaluate a list of indices.
 * `index_list' is the list of expressions for the indices;
 * the evaluated values will be stored in `indices'.
 * Returns 0 if all indices were evaluated successfully,
 * or -1 if an error occurred. */
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
	printf ("ERROR - WRONG NUMBER OF INDICES\n");
      else
	printf ("ERROR - WRONG NUMBER OF INDICES ON LINE %d\n", current_line);
      current_column = 0;
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
	    printf ("ERROR - INDEX #%d IS NOT A NUMBER\n", i + 1);
	  else
	    printf ("ERROR - INDEX #%d IS NOT A NUMBER ON LINE %d\n",
		    i + 1, current_line);
	  current_column = 0;
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
      if ((d < 0.0) || (d > (double) USHRT_MAX))
	{
	  if (current_line == (unsigned long) -1)
	    printf ((dim_or_index == DIM)
		    ? "ERROR - DIMENSION #%d OUT OF RANGE\n"
		    : "ERROR - INDEX #%d OUT OF RANGE\n", i + 1);
	  else
	    printf ((dim_or_index == DIM)
		    ? "ERROR - DIMENSION #%d OUT OF RANGE ON LINE %d\n"
		    : "ERROR - INDEX #%d OUT OF RANGE ON LINE %d\n",
		    i + 1, current_line);
	  executing = 0;
	  current_column = 0;
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
	case IDENTIFIER:
	case INTEGER:
	case FLOATINGPOINT:
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

	case STRING:
	case STRINGIDENTIFIER:
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


/* Define a variable as an array.  The number of dimensions
 * and size of each dimension are given directly.
 * Returns the new array if it was successfully allocated
 * or NULL on error. */
static struct fndef *
_dim_internal (unsigned short id,
	       int num_dimensions,
	       const unsigned short *dim_size)
{
  int i;
  struct fndef *array;
  unsigned long total_size;

  /* If this variable is already being used, remove the current definition */
  array = find_function (id);
  if (array != NULL)
    {
      if (array->built_in != NULL)
	{
	  printf ("ERROR - DIM: %s IS A FUNCTION\n",
		  name_table[array->name_index]->contents);
	  executing = 0;
	  return NULL;
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

  array->num_args = num_dimensions;
  total_size = 1;
  for (i = 0; i < num_dimensions; i++)
    {
      /* 1 million elements seems to be
       * a reasonable maximum limit for BASIC data */
      if (total_size >= (1 << 20) / ((unsigned int) dim_size[i] + 1))
	{
	  puts ("ERROR - DIM: ARRAY TOO BIG");
	  executing = 0;
	  return NULL;
	}
      /* The *real* dimension is d+1, since BASIC programs
       * may start indices at 0 as well as 1. */
      total_size *= (unsigned int) dim_size[i] + 1;
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
      return NULL;
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
    (sizeof (short) * num_dimensions);
  memcpy (array->array_dimension, dim_size, sizeof (short) * num_dimensions);
  array->argtypes = 0;

  return array;
}

/* Define a variable as an array.  The sizes are
 * given in a list of tokens to be evaluated. */
static void
dim (unsigned short id, struct list_header *size_list)
{
  int dimensions;
  // struct fndef *array;
  // struct list_item *lp;
  // unsigned short *tp;
  unsigned short dimension[32];
  // double d;

  /* Get the dimensions of the array */
  dimensions = size_list->num_items;
  if (get_indices (size_list, dimension, size_list->num_items, DIM))
    return;

  _dim_internal (id, size_list->num_items, dimension);
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
	    printf ("ERROR - INDEX #%d OUT OF RANGE\n", i + 1);
	  else
	    printf ("ERROR - INDEX #%d OUT OF RANGE ON LINE %d\n",
		    i + 1, current_line);
	  current_column = 0;
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

/* List of dimensions used for default array initialization.
 * We support a maximum of 1 million entries (technically 1,048,576)
 * which means we can only have 5 dimensions of the default size. */
const unsigned short SIZE_TEN[10] = {
  10, 10, 10, 10, 10, 10, 10, 10
};

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
      /* The original BASIC allowed undimensioned arrays
       * with a default size of 10 */
      array = _dim_internal (id, index_list->num_items, SIZE_TEN);
      if (array == NULL)
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
      /* The original BASIC allowed undimensioned arrays
       * with a default size of 10 */
      array = _dim_internal (id, index_list->num_items, SIZE_TEN);
      if (array == NULL)
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
  int i, arg_id;
  struct list_item *lp;
  unsigned short *tp;
  struct fndef *fn_or_array;
  var_u args[32], saved_vars[32], result;
  unsigned long arg_types = 0;

  fn_or_array = find_function (id);
  if (fn_or_array == NULL)
    {
      fprintf (stderr,
	       "eval_fn_or_array(): called for %s, which is not in the function table\n",
	       name_table[id]->contents);
      return (var_u) (struct string_value *) NULL;
    }

  /* Get the arguments */
  if (get_arguments (arg_list, args,
		     fn_or_array->num_args, fn_or_array->argtypes))
    return ((fn_or_array->type == '$')
	    ? (var_u) (struct string_value *) NULL : (var_u) 0.0);

  /* Call the appropriate subroutine depending on whether this is
   * a built-in function, user-defined function, or array */
  if (fn_or_array->built_in != NULL)
    {
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
      /* Guard against recursion, which would result in an infinite loop
       * since expressions don't do conditional evaluation. */
      if (fn_or_array->in_use)
	{
	  printf ("ERROR - %s CALLED RECURSIVELY, WHICH IS NOT SUPPORTED\n",
		  name_table[id]);
	  return ((fn_or_array->type == '$')
		  ? (var_u) (struct string_value *) NULL : (var_u) 0.0);
	}
      fn_or_array->in_use = 1;

      /* Save the existing value(s) of all function arguments,
       * replacing the function variables with the actual parameters. */
      for (i = 0; i < fn_or_array->num_args; i++)
	{
	  arg_id = fn_or_array->arg_ids[i];
	  if (fn_or_array->argtypes & (1 << i))
	    {
	      saved_vars[i].str = variable_values[arg_id].str;
	      variable_values[arg_id].str = args[i].str;
	    }
	  else
	    {
	      saved_vars[i].num = variable_values[arg_id].num;
	      variable_values[arg_id].num = args[i].num;
	    }
	}

      /* Evaluate the result according to whether
       * it's a numeric or string function. */
      tp = &fn_or_array->expr->tokens[0];
      if (fn_or_array->type)
	result.str = eval_string (&tp);
      else
	result.num = eval_number (&tp);

      /* Restore the previous value(s) of all function arguments. */
      for (i = 0; i < fn_or_array->num_args; i++)
	{
	  arg_id = fn_or_array->arg_ids[i];
	  if (fn_or_array->argtypes & (1 << i))
	    variable_values[arg_id].str = saved_vars[i].str;
	  else
	    variable_values[arg_id].num = saved_vars[i].num;
	}

      fn_or_array->in_use = 0;

      return result;
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


/* Define a user-supplied function. */
void
cmd_def (struct statement_header *stmt)
{
  unsigned short *tp;
  struct list_header *var_list;
  struct list_item *lip;
  int i, j, func_id;
  char func_type;	/* '$' for string, 0 for numeric function; */
  unsigned long argtypes = 0; /* see definition of fndef */
  unsigned short *arg_ids;
  struct fndef *new_function;

  tp = &stmt->tokens[0];
  if ((*tp != NUMLVAL) && (*tp != STRLVAL))
    {
    unexpected_token_error:	 /* We have a lot of cases that can end up here */
      fputs ("cmd_def(): unexpected token ", stderr);
      list_token (tp, stderr);
      fputc ('\n', stderr);
      return;
    }
  func_type = (*tp == STRLVAL) ? '$' : 0;
  ++tp;
  if ((*tp != IDENTIFIER) && (*tp != STRINGIDENTIFIER))
    goto unexpected_token_error;
  if (*tp != (func_type ? STRINGIDENTIFIER : IDENTIFIER))
    {
      fputs ("cmd_def(): INTERNAL ERROR: LVAL identifier type does not match DEF statement type\n", stderr);
      return;
    }
  func_id = *(++tp);
  if (*(++tp) != '(')
    goto unexpected_token_error;
  if (*(++tp) != ITEMLIST)
    goto unexpected_token_error;
  tp++;
  var_list = (struct list_header *) tp;
  arg_ids = (unsigned short *) calloc (var_list->num_items, sizeof (short));
  lip = &var_list->item[0];
  for (i = 0; i < var_list->num_items; i++)
    {
      if (lip->length != sizeof (struct list_item) + 2 * sizeof (short))
	{
	  tp = (unsigned short *) lip;
	  goto unexpected_token_error;
	}
      if (lip->tokens[0] == IDENTIFIER)
	{
	  arg_ids[i] = lip->tokens[1];
	}
      else if (lip->tokens[0] == STRINGIDENTIFIER)
	{
	  argtypes |= 1 << i;
	  arg_ids[i] = lip->tokens[1];
	}
      else {
	tp = &lip->tokens[0];
	goto unexpected_token_error;
      }
      for (j = 0; j < i; j++)
	{
	  if (arg_ids[i] == arg_ids[j])
	    {
	      printf ("ERROR - DEF: DUPLICATE ARGUMENT %s\n",
		      name_table[arg_ids[i]]->contents);
	    }
	}
      tp = (unsigned short *) &((char *) lip)[lip->length];
      if ((*tp != ',') && (*tp != ')'))
	goto unexpected_token_error;
      lip = (struct list_item *) ++tp;
    }

  /* We now have the full declaration side, and the token
   * pointer should be right after the closing ')' token. */
  if (*tp != '=')
    goto unexpected_token_error;
  tp++;
  if ((*tp != NUMEXPR) && (*tp != STREXPR))
    goto unexpected_token_error;
  if (*tp != (func_type ? STREXPR : NUMEXPR))
    {
      fputs ("cmd_def(): INTERNAL ERROR: expression type does not match DEF statement type\n", stderr);
      return;
    }

  /* This should be enough for us to allocate a function definition.
   * If a function by the same name has already been defined, clear it
   * UNLESS it's a built-in function. */
  new_function = find_function (func_id);
  if (new_function != NULL)
    {
      if (new_function->built_in != NULL)
	{
	  printf ("ERROR - DEF: %s IS A BUILT-IN FUNCTION\n",
		  name_table[func_id]->contents);
	  executing = 0;
	  return;
	}
      free_function (new_function);
    } else {
      /* Add another entry to the function table */
      function_table = (struct fndef *) realloc
	(function_table, sizeof (struct fndef) * ++function_table_size);
      new_function = &function_table[function_table_size - 1];
      memset (new_function, 0, sizeof (struct fndef));
      new_function->name_index = func_id;
    }
  new_function->type = func_type;
  new_function->num_args = var_list->num_items;
  new_function->argtypes = argtypes;
  new_function->arg_ids = arg_ids;

  /* Make a copy of the expression tokens.  This is needed in case
   * we're defining the function in immediate mode or if the user
   * changes or deletes the line containing the original expression
   * later; the function definition should persist. */

  /* The expression length includes the leading xxxEXPR token
   * and trailing statement terminator. */
  unsigned short expr_length = &((char *) stmt)[stmt->length] - ((char *) tp);
  new_function->expr = (struct statement_header *) calloc
    /* The statement header length includes its length field. */
    (1, sizeof (short) + expr_length);
  new_function->expr->length = sizeof (short) + expr_length;
  /* Copy the xxxEXPR token as the psuedo-statement `command' */
  new_function->expr->command = *tp++;
  /* The rest of the expression goes into the psuedo-statement token list */
  memcpy (&new_function->expr->tokens, tp, expr_length - sizeof (short));
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
