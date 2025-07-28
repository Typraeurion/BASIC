/* Pre-defined functions in BASIC */
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "tables.h"

/* Trigonometric functions in BASIC use degrees (0-360).
 * Define this value as 180/pi. */
#define DEG_OVER_RAD 57.2957795130823208768

var_u fn_abs (var_u *);
var_u fn_asc (var_u *);
var_u fn_atn (var_u *);
var_u fn_chr (var_u *);
var_u fn_cos (var_u *);
var_u fn_exp (var_u *);
var_u fn_int (var_u *);
var_u fn_left (var_u *);
var_u fn_len (var_u *);
var_u fn_log (var_u *);
var_u fn_mid (var_u *);
var_u fn_rnd (var_u *);
var_u fn_seed (var_u *);
var_u fn_right (var_u *);
var_u fn_sgn (var_u *);
var_u fn_sin (var_u *);
var_u fn_sqr (var_u *);
var_u fn_str (var_u *);
var_u fn_tan (var_u *);
var_u fn_val (var_u *);

struct {
  const char *name;
  int num_args;
  var_u (*func) (var_u *);
  unsigned long argtypes;
} built_in_initializers[] = {
  { "ABS", 1, fn_abs, 0 },
  { "ASC", 1, fn_asc, 1 },
  { "ATN", 1, fn_atn, 0 },
  { "CHR$", 1, fn_chr, 0 },
  { "COS", 1, fn_cos, 0 },
  { "EXP", 1, fn_exp, 0 },
  { "INT", 1, fn_int, 0 },
  { "LEFT$", 2, fn_left, 1 },
  { "LEN", 1, fn_len, 1 },
  { "LOG", 1, fn_log, 0 },
  { "MID$", 3, fn_mid, 1 },
  { "RND", 1, fn_rnd, 0 },
  { "SEED", 1, fn_seed, 0 },
  { "RIGHT$", 2, fn_right, 1 },
  { "SGN", 1, fn_sgn, 0 },
  { "SIN", 1, fn_sin, 0 },
  { "SQR", 1, fn_sqr, 0 },
  { "STR$", 1, fn_str, 0 },
  { "TAN", 1, fn_tan, 0 },
  { "VAL", 1, fn_val, 1 },
};
#define NUM_INITIALIZERS \
  (sizeof (built_in_initializers) / sizeof (built_in_initializers[0]))

/* Initialize the function table with the built-in functions */
void
initialize_builtin_functions (void)
{
  int i;

  function_table_size = NUM_INITIALIZERS;
  function_table = (struct fndef *) realloc
    (function_table, sizeof (struct fndef) * NUM_INITIALIZERS);
  for (i = 0; (size_t) i < NUM_INITIALIZERS; i++)
    {
      function_table[i].name_index
	= find_var_name (built_in_initializers[i].name);
      function_table[i].type
	= (built_in_initializers[i].name
	   [strlen (built_in_initializers[i].name) - 1]
	   == '$') ? '$' : 0;
      function_table[i].num_args
	= built_in_initializers[i].num_args;
      function_table[i].argtypes
	= built_in_initializers[i].argtypes;
      function_table[i].built_in
	= built_in_initializers[i].func;
      function_table[i].expr = NULL;
      function_table[i].array_dimension = NULL;
      function_table[i].array_data = NULL;
    }
}


var_u fn_abs (var_u *args)
{
  return (var_u) fabs (args[0].num);
}

var_u fn_asc (var_u *args)
{
  if (args[0].str == NULL)
    return (var_u) 0.0;
  return (var_u) (double) args[0].str->contents[0];
}

var_u fn_atn (var_u *args)
{
  return (var_u) (atan (args[0].num) * DEG_OVER_RAD);
}

var_u fn_chr (var_u *args)
{
  struct string_value *str = (struct string_value *) malloc (4);
  str->length = 1;
  if ((args[0].num >= -4294967295.0) && (args[0].num <= 4294967295.0))
    {
      str->contents[0] = (char) args[0].num;
      str->contents[1] = '\0';
    } else {
      *((long *) str) = 0;
    }
  return (var_u) str;
}

var_u fn_cos (var_u *args)
{
  var_u result;
  result.num = cos (args[0].num / DEG_OVER_RAD);
  /* Because cos(pi/2) is not exactly 0 (due to rounding errors),
   * try to zero any result less than 2^-32
   * so that "COS(90)=0" comes out true. */
  if ((result.num < 2.3283064365387e-10)
      && (result.num > -2.3283064365387e-10))
    result.num = 0.0;
  return result;
}

var_u fn_exp (var_u *args)
{
  return (var_u) exp (args[0].num);
}

var_u fn_int (var_u *args)
{
  return (var_u) ((args[0].num > 0)
		  ? floor (args[0].num) : ceil (args[0].num));
}

var_u fn_left (var_u *args)
{
  int len, len2;
  struct string_value *new_str;

  if (args[1].num < 0.0)
    len = 0;
  else if (args[1].num > 2147483647.0)
    len = 2147483647;
  else
    len = (int) args[1].num;

  len2 = (args[0].str == NULL) ? 0 : args[0].str->length;
  if (len2 < len)
    len = len2;
  new_str = (struct string_value *) calloc
    (1, WALIGN (sizeof (struct string_value) + len + 1));
  if (len)
    {
      new_str->length = len;
      memcpy (new_str->contents, args[0].str->contents, len);
    }
  return (var_u) new_str;
}

var_u fn_len (var_u *args)
{
  if (args[0].str == NULL)
    return (var_u) 0.0;
  else
    return (var_u) (double) args[0].str->length;
}

var_u fn_log (var_u *args)
{
  return (var_u) log (args[0].num);
}

var_u fn_mid (var_u *args)
{
  int start, len, len2;
  struct string_value *new_str;

  /* This is the one function where the base index matters.
   * Most BASICs say MID$(X$,1,1) is the 1st character of X$. */
  if (args[1].num < 1.0)
    start = 0;
  else if (args[1].num > 2147483648.0)
    start = 2147483647;
  else
    start = (int) args[1].num - 1;

  if (args[2].num < 0.0)
    len = 0;
  else if (args[2].num > 2147483647.0)
    len = 2147483647;
  else
    len = (int) args[2].num;

  len2 = (args[0].str == NULL) ? 0 : args[0].str->length;
  if (start > len2)
    start = len2;
  if (start + len > len2)
    len = len2 - start;
  new_str = (struct string_value *) calloc
    (1, WALIGN (sizeof (struct string_value) + len + 1));
  if (len)
    {
      new_str->length = len;
      memcpy (new_str->contents, &args[0].str->contents[start], len);
    }
  return (var_u) new_str;
}

var_u fn_rnd (var_u *args)
{
  double r, x = args[0].num;
  struct timeval t;

  if (x == 0.0)
    {
      gettimeofday (&t, NULL);
      srand ((unsigned int) t.tv_sec + t.tv_usec);
      x = 1.0;
    }
  /* Because doubles have more precision than longs,
   * we call rand() twice and join the results. */
  r = (double) rand () + (double) rand () / 4294967296.0;
  r /= (double) RAND_MAX + 1.0;
  r *= x;
  return (var_u) r;
}

var_u fn_seed (var_u *args)
{
  srand (args[0].num);
  return args[0];
}

var_u fn_right (var_u *args)
{
  int len, len2;
  struct string_value *new_str;

  if (args[1].num < 0.0)
    len = 0;
  else if (args[1].num > 2147483647.0)
    len = 2147483647;
  else
    len = (int) args[1].num;

  len2 = (args[0].str == NULL) ? 0 : args[0].str->length;
  if (len2 < len)
    len = len2;
  new_str = (struct string_value *) calloc
    (1, WALIGN (sizeof (struct string_value) + len + 1));
  if (len)
    {
      new_str->length = len;
      memcpy (new_str->contents, &args[0].str->contents[len2 - len], len);
    }
  return (var_u) new_str;
}

var_u fn_sgn (var_u *args)
{
  if (args[0].num == 0.0)
    return (var_u) 0.0;
  else if (args[0].num < 0.0)
    return (var_u) -1.0;
  else
    return (var_u) 1.0;
}

var_u fn_sin (var_u *args)
{
  var_u result;
  result.num = sin (args[0].num / DEG_OVER_RAD);
  /* Because sin(pi) is not exactly 0 (due to rounding errors),
   * try to zero any result less than 2^-32
   * so that "SIN(180)=0" comes out true. */
  if ((result.num < 2.3283064365387e-10)
      && (result.num > -2.3283064365387e-10))
    result.num = 0.0;
  return result;
}

var_u fn_sqr (var_u *args)
{
  return (var_u) sqrt (args[0].num);
}

var_u fn_str (var_u *args)
{
  int oldlen = 30;
  struct string_value *str = (struct string_value *) calloc
    (1, sizeof (struct string_value) + oldlen);
  while (1) {
    str->length = snprintf (str->contents, oldlen, "%G", args[0].num);
    str = (struct string_value *) realloc
      (str, WALIGN (sizeof (struct string_value) + str->length + 1));
    if (str->length < oldlen)
      return (var_u) str;
    oldlen = WALIGN (str->length + 1);
    memset (str, 0, sizeof (struct string_value) + oldlen);
  }
}

var_u fn_tan (var_u *args)
{
  var_u result;
  result.num = tan (args[0].num / DEG_OVER_RAD);
  /* Because tan(pi) is not exactly 0 (due to rounding errors),
   * try to zero any result less than 2^-32
   * so that "TAN(180)=0" comes out true. */
  if ((result.num < 2.3283064365387e-10)
      && (result.num > -2.3283064365387e-10))
    result.num = 0.0;
  return result;
}

var_u fn_val (var_u *args)
{
  return (var_u) strtod (args[0].str->contents, NULL);
}
