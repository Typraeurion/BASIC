#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tables.h"
#include "basic.tab.h"


/* Evaluate a numeric operand.  Return the result.
 * The token pointer is advanced to the next token after the operand. */
double
eval_number (unsigned short **tpp)
{
  double result;
  unsigned short *tp = *tpp;
  int i;

  /* Ignore NUMEXPR prefixes, but bail out on any others */
  switch ((int) *tp)
    {
    case NUMEXPR: ++tp; break;
    case STREXPR:
      fputs ("Error: eval_number() called with a string expression\n", stderr);
      return 0.0;
    case NUMLVAL:
      fputs ("Error: eval_number() called with an lvalue\n", stderr);
      return 0.0;
    case STRLVAL:
      fputs ("Error: eval_number() called with a string lvalue\n", stderr);
      return 0.0;
    }

  switch ((int) *tp)
    {
    case INTEGER:
      ++tp;
      *tpp = (unsigned short *) &((unsigned long *) tp)[1];
      return (double) *((unsigned long *) tp);

    case FLOATINGPOINT:
      ++tp;
      *tpp = (unsigned short *) &((double *) tp)[1];
      return *((double *) tp);

    case IDENTIFIER:
      ++tp;
      i = *tp++;
      /* If this is a simple variable, we can get its value directly */
      if (*tp != '(')
	{
	  *tpp = tp;
	  return variable_values[i].num;
	}
      /* Otherwise, call the function or array lookup routine */
      if (*(++tp) != ITEMLIST)
	{
	  fputs ("eval_number(): Unexpected token ", stderr);
	  list_token (tp, stderr);
	  fprintf (stderr, " after identifier %s(\n",
		   name_table[i]->contents);
	  return 0.0;
	}
      ++tp;
      result = eval_fn_or_array (i, (struct list_header *) tp).num;
      /* Skip past the argument list and closing parenthesis */
      tp = (unsigned short *) &((char *) tp)
	[((struct list_header *) tp)->length];
      *tpp = ++tp;
      return result;

    case '(':	/* This starts a parenthesized expr; ignore it,
		 * but dig in for the `real' numeric value. */
      ++tp;
      result = eval_number (&tp);
      /* Make sure we ended on a close parenthesis */
      if (*tp != ')')
	{
	  fputs ("eval_number(): token ", stderr);
	  list_token (tp, stderr);
	  fputs (" found at end of parenthesized expression\n", stderr);
	} else {
	  tp++;
	}
      *tpp = tp;
      return result;

    case '{':	/* This is the start of an expression */
      ++tp;
      if (*tp == STREXPR)
	{
	  /* This precedes a string comparison. */
	  ++tp;
	  result = eval_strcond (&tp);
	} else {
	  if (*tp == NUMEXPR)
	    /* eval_numexpr also handles numeric comparisons. */
	    ++tp;
	  result = eval_numexpr (&tp);
	}
      /* Make sure we ended on a close parenthesis */
      if (*tp != '}')
	{
	  fputs ("eval_number(): token ", stderr);
	  list_token (tp, stderr);
	  fputs (" found at end of arithmetic expression\n", stderr);
	} else {
	  tp++;
	}
      *tpp = tp;
      return result;

    default:
      fputs ("eval_number(): called with unknown token ", stderr);
      list_token (tp, stderr);
      fputc ('\n', stderr);
      *tpp = &tp[1];
      return 0.0;
    }
}


/* Concatenate two strings.  Returns the new string.
 * The first string will have been reallocated; the second left alone. */
static struct string_value *
concat_strings (struct string_value *str1, const struct string_value *str2)
{
  int len1, len2, newlen;

  /* Do nothing if the second string is null. */
  if (str2 == NULL)
    return str1;

  /* Make the first string large enough to contain the concatenation */
  len1 = str1->length;
  len2 = str2->length;
  newlen = len1 + len2;
  str1 = (struct string_value *) realloc
    (str1, WALIGN (sizeof (struct string_value) + newlen + 1));
  /* Make sure the string gets terminated and padded */
  *((short *) &str1->contents[WALIGN (newlen - 1)]) = 0;
  /* Copy the second string onto the end of the first */
  memcpy (&str1->contents[len1], str2->contents, len2);
  str1->length = newlen;
  return str1;
}

/* Evaluate a string operand.  Return the result.
 * The token pointer is advanced to the next token after the operand. */
struct string_value *
eval_string (unsigned short **tpp)
{
  struct string_value *string, *string2;
  unsigned short *tp = *tpp;
  int i;

  /* Ignore STREXPR prefixes, but bail out on any others */
  switch ((int) *tp)
    {
    case STREXPR: ++tp; break;
    case NUMEXPR:
      fputs ("Error: eval_string() called with a numeric expression\n",
	     stderr);
      return NULL;
    case NUMLVAL:
      fputs ("Error: eval_string() called with an lvalue\n", stderr);
      return NULL;
    case STRLVAL:
      fputs ("Error: eval_string() called with a string lvalue\n", stderr);
      return NULL;
    }

  /* Start with an empty string */
  string = (struct string_value *) calloc
    (1, WALIGN (sizeof (struct string_value) + 1));

  while (1)
    {
      switch ((int) *tp)
	{
	case STRING:
	  ++tp;
	  /* Append the string directly */
	  string = concat_strings (string, (const struct string_value *) tp);
	  tp = (unsigned short *) &((char *) tp)
	    [WALIGN (sizeof (struct string_value)
		     + ((struct string_value *) tp)->length + 1)];
	  break;

	case STRINGIDENTIFIER:
	  ++tp;
	  i = *tp++;
	  /* If this is a simple variable, we can get its value directly */
	  if (*tp != '(')
	    {
	      string = concat_strings (string, variable_values[i].str);
	    } else {
	      /* Otherwise, call the function or array lookup routine */
	      if (*(++tp) != ITEMLIST)
		{
		  fputs ("eval_string(): Unexpected token ", stderr);
		  list_token (tp, stderr);
		  fprintf (stderr, " after identifier %s(\n",
			   name_table[i]->contents);
		  break;
		}
	      ++tp;
	      string2 = eval_fn_or_array (i, (struct list_header *) tp).str;
	      if (string2 != NULL)
		{
		  string = concat_strings
		    (string, (const struct string_value *) string2);
		  free (string2);
		}
	      /* Skip past the argument list and closing parenthesis */
	      tp = (unsigned short *) &((char *) tp)
		[((struct list_header *) tp)->length];
	      ++tp;
	    }
	  break;

	default:
	  fputs ("eval_string(): called with unknown token ", stderr);
	  list_token (tp, stderr);
	  fputc ('\n', stderr);
	  *tpp = &tp[1];
	  return string;
	}

      /* Check for concatenation */
      if (*tp != '+')
	break;
      ++tp;
      continue;
    }

  *tpp = tp;
  return string;
}


/* Evaluate a numeric expression.  Return the result.
 * The token pointer is advanced to the next token after the evaluation.
 * Assumes a simple unary or binary expression. */
double
eval_numexpr (unsigned short **tpp)
{
  unsigned short *tp = *tpp;
  double op1, op2;	/* Operands */
  unsigned short op;	/* Operator */
  double result;

  /* Check for a unary operation -- negation */
  if (*tp == NEG)
    {
      *tpp = ++tp;
      op1 = eval_number (tpp);
      result = -op1;
      if (tracing & TRACE_EXPRESSIONS)
	fprintf(stderr, "- %g = %g\n", op1, result);
      return result;
    }

  /* All other operations are binary */
  op1 = eval_number (&tp);
  op = *tp++;
  op2 = eval_number (&tp);
  *tpp = tp;

  switch (op)
    {
    case '+':
      result = (op1 + op2);
      if (tracing & TRACE_EXPRESSIONS)
	fprintf (stderr, "%g + %g = %g\n", op1, op2, result);
      return result;
    case '-':
      result = (op1 - op2);
      if (tracing & TRACE_EXPRESSIONS)
	fprintf (stderr, "%g - %g = %g\n", op1, op2, result);
      return result;
    case '*':
      result = (op1 * op2);
      if (tracing & TRACE_EXPRESSIONS)
	fprintf (stderr, "%g * %g = %g\n", op1, op2, result);
      return result;
    case '/':
      result = (op1 / op2);
      if (tracing & TRACE_EXPRESSIONS)
	fprintf (stderr, "%g / %g = %g\n", op1, op2, result);
      return result;
    case '^':
      result = pow (op1, op2);
      if (tracing & TRACE_EXPRESSIONS)
	fprintf (stderr, "%g ^ %g = %g\n", op1, op2, result);
      return result;
    }

  /* For comparisons of equality, we'll use a lower precision
   * if the number will fit, so that rounding errors don't
   * throw is off.  BASIC isn't the best language for high
   * precision mathematical calculations anyway. */
  if ((fabs(op1) > FLT_MIN) && (fabs(op1) < FLT_MAX))
    {
      volatile float reduce = op1;
      op1 = reduce;
    }
  if ((fabs(op2) > FLT_MIN) && (fabs(op2) < FLT_MAX))
    {
      volatile float reduce = op2;
      op2 = reduce;
    }

  switch (op)
    {
    case '=':
      result = ((op1 == op2) ? 1.0 : 0.0);
      if (tracing & TRACE_EXPRESSIONS)
	fprintf (stderr, "%g = %g = %g\n", op1, op2, result);
      return result;
    case NOTEQ:
      result = ((op1 != op2) ? 1.0 : 0.0);
      if (tracing & TRACE_EXPRESSIONS)
	fprintf (stderr, "%g <> %g = %g\n", op1, op2, result);
      return result;
    case '<':
      result = ((op1 < op2) ? 1.0 : 0.0);
      if (tracing & TRACE_EXPRESSIONS)
	fprintf (stderr, "%g < %g = %g\n", op1, op2, result);
      return result;
    case '>':
      result = ((op1 > op2) ? 1.0 : 0.0);
      if (tracing & TRACE_EXPRESSIONS)
	fprintf (stderr, "%g > %g = %g\n", op1, op2, result);
      return result;
    case LESSEQ:
      result = ((op1 <= op2) ? 1.0 : 0.0);
      if (tracing & TRACE_EXPRESSIONS)
	fprintf (stderr, "%g <= %g = %g\n", op1, op2, result);
      return result;
    case GRTREQ:
      result = ((op1 >= op2) ? 1.0 : 0.0);
      if (tracing & TRACE_EXPRESSIONS)
	fprintf (stderr, "%g >= %g = %g\n", op1, op2, result);
      return result;
    case AND:
      result = ((op1 != 0.0) && (op2 != 0.0));
      if (tracing & TRACE_EXPRESSIONS)
	fprintf (stderr, "%g AND %g = %g\n", op1, op2, result);
      return result;
    case OR:
      result = ((op1 != 0.0) || (op2 != 0.0));
      if (tracing & TRACE_EXPRESSIONS)
	fprintf (stderr, "%g OR %g = %g\n", op1, op2, result);
      return result;
    }

  /* Anything past this point is a bad operator. */
  fputs ("eval_numexpr(): Bad operator token ", stderr);
  list_token (&op, stderr);
  fputc ('\n', stderr);
  return op1;
}


/* Evaluate a string comparison.  Return 1.0 if true, 0.0 if false.
 * The token pointer is advanced to the next token after the operand. */
double
eval_strcond (unsigned short **tpp)
{
  struct string_value *string1, *string2;
  unsigned short op;
  int result;

  string1 = eval_string (tpp);
  op = *((*tpp)++);
  string2 = eval_string (tpp);
  result = strcmp (string1->contents, string2->contents);
  if (tracing & TRACE_EXPRESSIONS) {
    switch (op) {
    case '=':
      fprintf (stderr, "\"%s\" = \"%s\" = %g\n",
	       string1->contents, string2->contents,
	       (result == 0) ? 1.0 : 0.0);
      break;
    case NOTEQ:
      fprintf (stderr, "\"%s\" <> \"%s\" = %g\n",
	       string1->contents, string2->contents,
	       (result != 0) ? 1.0 : 0.0);
      break;
    case '<':
      fprintf (stderr, "\"%s\" < \"%s\" = %g\n",
	       string1->contents, string2->contents,
	       (result < 0) ? 1.0 : 0.0);
      break;
    case '>':
      fprintf (stderr, "\"%s\" > \"%s\" = %g\n",
	       string1->contents, string2->contents,
	       (result > 0) ? 1.0 : 0.0);
      break;
    case LESSEQ:
      fprintf (stderr, "\"%s\" <= \"%s\" = %g\n",
	       string1->contents, string2->contents,
	       (result <= 0) ? 1.0 : 0.0);
      break;
    case GRTREQ:
      fprintf (stderr, "\"%s\" >= \"%s\" = %g\n",
	       string1->contents, string2->contents,
	       (result >= 0) ? 1.0 : 0.0);
      break;
    }
  }
  free (string1);
  free (string2);

  switch (op)
    {
    case '=': return ((result == 0) ? 1.0 : 0.0);
    case NOTEQ: return ((result != 0) ? 1.0 : 0.0);
    case '<': return ((result < 0) ? 1.0 : 0.0);
    case '>': return ((result > 0) ? 1.0 : 0.0);
    case LESSEQ: return ((result <= 0) ? 1.0 : 0.0);
    case GRTREQ: return ((result >= 0) ? 1.0 : 0.0);
    }

  /* Anything past this point is a bad operator. */
  fputs ("eval_strcond(): Bad operator token ", stderr);
  list_token (&op, stderr);
  fputc ('\n', stderr);
  return 0.0;
}


/* Evaluate an expression and assign it to a variable. */
void
cmd_let (struct statement_header *stmt)
{
  unsigned short *tp;
  double *numptr;
  struct string_value **strptr, *newstr;
  int i;

  tp = (unsigned short *) &stmt->tokens[0];
  /* The next token should tell is whether this is a string
   * or numeric assignment */
  switch ((int) *tp++)
    {
    case NUMLVAL:
      /* Get the address of the variable */
      ++tp;
      i = *tp++;
      numptr = &variable_values[i].num;
      // FIXME: Should be printed to a string for later output,
      // since we may have to print the results of
      // additional evaluations along the way.
      if (tracing & TRACE_EXPRESSIONS)
	fprintf (stderr, "%s", name_table[i]->contents);
      /* If this is an array, we need to do more work */
      if (*tp == '(')
	{
	  if (tracing & TRACE_EXPRESSIONS)
	    {
	      fputc ('(', stderr);
	      // FIXME: Why did the code temporarily set this trace bit?
	      // (It tells get_indices to display the evaluated indices)
	      // tracing |= 4;
	    }
	  if (*(++tp) != ITEMLIST)
	    {
	      fputs ("cmd_let(): Unexpected token ", stderr);
	      list_token (tp, stderr);
	      fprintf (stderr, " after identifier %s(\n",
		       name_table[i]->contents);
	      break;
	    }
	  ++tp;
	  numptr = num_array_lookup (i, (struct list_header *) tp);
	  if (numptr == NULL)
	    return;
	  /* Skip past the argument list and closing parenthesis */
	  tp = (unsigned short *) &((char *) tp)
	    [((struct list_header *) tp)->length];
	  ++tp;
	  if (tracing & TRACE_EXPRESSIONS)
	    {
	      fputc (')', stderr);
	      // FIXME
	      // tracing &= ~4;
	    }
	}

      /* Skip past the '=' and NUMEXPR tokens */
      tp += 2;

      /* Assign the result of the next expression */
      *numptr = eval_number (&tp);
      if (tracing & TRACE_EXPRESSIONS)
	fprintf (stderr, "=%g\n", *numptr);
      return;

    case STRLVAL:
      /* Get the address of the variable */
      ++tp;
      i = *tp++;
      strptr = &variable_values[i].str;
      if (tracing & TRACE_EXPRESSIONS)
	fprintf (stderr, "%s", name_table[i]->contents);
      /* If this is an array, we need to do more work */
      if (*tp == '(')
	{
	  if (tracing & TRACE_EXPRESSIONS)
	    {
	      fputc ('(', stderr);
	      // FIXME: Why did the code temporarily set this trace bit?
	      // (It tells get_indices to display the evaluated indices)
	      // tracing |= 4;
	    }
	  if (*(++tp) != ITEMLIST)
	    {
	      fputs ("cmd_let(): Unexpected token ", stderr);
	      list_token (tp, stderr);
	      fprintf (stderr, " after identifier %s(\n",
		       name_table[i]->contents);
	      break;
	    }
	  ++tp;
	  strptr = str_array_lookup (i, (struct list_header *) tp);
	  if (strptr == NULL)
	    return;
	  /* Skip past the argument list and closing parenthesis */
	  tp = (unsigned short *) &((char *) tp)
	    [((struct list_header *) tp)->length];
	  ++tp;
	  if (tracing & TRACE_EXPRESSIONS)
	    {
	      fputc (')', stderr);
	      // FIXME
	      // tracing &= ~4;
	    }
	}

      /* Skip past the '=' and STREXPR tokens */
      tp += 2;

      /* Get the result of the next expression.  Note that we don't
       * discard the original string yet, since it may be needed
       * in the expression. */
      newstr = eval_string (&tp);
      /* Now we can free the original string, and save the new. */
      free (*strptr);
      *strptr = newstr;
      if (tracing & TRACE_EXPRESSIONS)
	fprintf (stderr, "=\"%s\"\n", newstr->contents);
      return;

    default:
      fputs ("cmd_let(): Bad token in assignment: ", stderr);
      list_token (tp, stderr);
      fputc ('\n', stderr);
    }
}
