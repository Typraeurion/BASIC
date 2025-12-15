/* Define a data structure for BASIC tokens,
 * which is how programs are stored in memory. */

#include <stdio.h>

/* All character strings must be word-aligned */
#define WALIGN(x) (((x) + sizeof (short) - 1) & ~(sizeof (short) - 1))

/* We'll support nesting LOADs up to 5 levels deep. */
#define MAX_LOAD_NESTING 5

struct string_value {
  unsigned short length;	/* Size in bytes of the string, *not*
				 * including the length or null padding. */
  char contents[0];		/* Variable-length, null-terminated,
				 * null padded to short boundary */
};

struct list_item {
  unsigned short length;	/* Size in bytes of the item,
				 * including this header.	*/
  unsigned short tokens[0];	/* The item			*/
};

struct list_header {
  unsigned short length;	/* Size in bytes of following data,
				 * including this header.	*/
  unsigned short num_items;	/* Number of elements in the list */
  struct list_item item[0];
};

struct statement_header {
  unsigned short length;	/* Size in bytes of the statement,
				 * including this header. */
  short command;		/* Token representing the command */
  unsigned short tokens[0];	/* Any additional statement parameters */
  /* (For `IF' statements, this goes up to, but not past, the `THEN'.
   * For other statements, this includes the ':' or '\n' at the end.)  */
};

struct if_header {
  unsigned short length;	/* Size in bytes of the if clause up to
				 * and including the `THEN' token. */
  short command;		/* This will always be the `IF' token. */
  unsigned short then_offset;	/* Offset in bytes to the `THEN' statement. */
  unsigned short else_offset;	/* Offset in bytes to the `ELSE' statement
				 * if there is one, or 0 if there is none. */
  unsigned short tokens[0];	/* Start of the numeric expression */
};

struct line_header {
  unsigned short length;	/* Size in bytes of the line,
				 * including this header.	*/
  unsigned long line_number;
  /* Variable number of variable-length statements */
  struct statement_header statement[0];
} __attribute__ ((packed));

/* Union of either numeric or string value */
typedef union {
  double num;
  struct string_value *str;
} var_u;

/* Structure containing a function definition */
struct fndef {
  unsigned short name_index;	/* Index of the name of the function
				 * in the variable name table	*/
  char in_use;			/* Set when evaluating a user-defined
				 * function to prevent recursion */
  char type;			/* Numeric or string function?	*/
  unsigned char num_args;	/* Number of arguments -- 1-32	*/
  unsigned long argtypes;	/* Bitmask of argument types; (1 << n)
				 * masks type of argument n+1,
				 * 1 for string, 0 for numeric	*/
  var_u (*built_in) (var_u *);	/* Pointer to function if it's built-in */
  unsigned short *arg_ids;	/* ID(s) of the arguments for user-defined */
  struct statement_header *expr; /* Function definition for user-defined */
  unsigned short *array_dimension; /* For arrays, the size of each dimension */
  void *array_data;		/* For arrays, the array data	*/
};

/* The variable name table. */
extern int name_table_size;	/* Number of names defined	*/
extern struct string_value **name_table;

/* There is one variable for each name defined. */
extern var_u *variable_values;

/* The function table. */
extern int function_table_size; /* Number of functions defined	*/
extern struct fndef *function_table;

/* Keep track of how far we've nested LOAD commands */
extern signed int current_load_nesting;

/* The program. */
extern int program_size;	/* Number of lines in the program */
extern struct line_header **program;

/* Program state. */
extern int executing;
extern unsigned long current_line;
extern unsigned long current_data_line;
extern unsigned short current_statement;
extern unsigned short current_data_statement;
extern unsigned short current_data_item;
extern struct line_header *immediate_line;
extern int tracing;
extern int current_column;

/* Trace flags (bits); the first three are for BASIC language level tracing */
#define TRACE_LINES 1
#define TRACE_STATEMENTS 2
#define TRACE_EXPRESSIONS 4
/* These next two flags are for interpreter (bison/flex) tracing */
#define TRACE_GRAMMAR 8
#define TRACE_PARSER 16


/* Functions used in implementing the BASIC interpreter: */
void initialize_tables (void);
void initialize_builtin_functions (void);
/* This function returns the index of a variable name in the name table.
 * If the name does not already exist, it is added. */
unsigned short find_var_name (const char *name);
/* This function returns the line with the given number.
 * If the flag is true and the given line does not exist,
 * the next existing line is returned. */
struct line_header *find_line (unsigned long number, int after);
/* Search a given line for the Nth statement */
struct statement_header *find_statement (struct line_header *line,
					 unsigned short statement_number);
/* This function adds a new line to the program.
 * If a line with the same number already exists, it is deleted first. */
void add_line (struct line_header *line);
/* List a line from a program */
void list_line (struct line_header *lp, FILE *to);
/* List a program statement */
void list_statement (struct statement_header *sp, FILE *to);
/* List a token from a program statement */
int list_token (unsigned short *tp, FILE *to);
void remove_line (unsigned long number);
void execute (struct line_header *);
double eval_number (unsigned short **);
double eval_numexpr (unsigned short **);
struct string_value *eval_string (unsigned short **);
double eval_strcond (unsigned short **);
var_u eval_fn_or_array (unsigned short id, struct list_header *arg_list);
double *num_array_lookup (unsigned short id, struct list_header *index_list);
struct string_value **str_array_lookup (unsigned short id,
					struct list_header *index_list);

/* BASIC commands */
void cmd_bye (struct statement_header *);
void cmd_continue (struct statement_header *);
void cmd_data (struct statement_header *);
void cmd_def (struct statement_header *);
void cmd_dim (struct statement_header *);
void cmd_end (struct statement_header *);
void cmd_for (struct statement_header *);
void cmd_gosub (struct statement_header *);
void cmd_goto (struct statement_header *);
void cmd_if (struct statement_header *);
void cmd_input (struct statement_header *);
void cmd_let (struct statement_header *);
void cmd_list (struct statement_header *);
void cmd_load (struct statement_header *);
void cmd_new (struct statement_header *);
void cmd_next (struct statement_header *);
void cmd_on (struct statement_header *);
void cmd_pause (struct statement_header *);
void cmd_print (struct statement_header *);
void cmd_read (struct statement_header *);
void cmd_rem (struct statement_header *);
void cmd_restore (struct statement_header *);
void cmd_return (struct statement_header *);
void cmd_run (struct statement_header *);
void cmd_save (struct statement_header *);
void cmd_stop (struct statement_header *);
void cmd_trace (struct statement_header *);


/* DEBUG */
#if 0
#include <stdio.h>
#define calloc(s1,s2) ({ register void *_rv; \
  register size_t _s1 = s1, _s2 = s2; \
  _rv = (calloc) (_s1, _s2); \
  fprintf (stderr, "Allocated and cleared %d bytes at %p\n", _s1*_s2, _rv); \
  _rv; })
#define malloc(s) ({ register void *_rv; \
  register size_t _s = s; \
  _rv = (malloc) (_s); \
  fprintf (stderr, "Allocated %d bytes at %p\n", _s, _rv); \
  _rv; })
#define realloc(p,s) ({ register void *_rv, *_p = p; \
  register size_t _s = s; \
  _rv = (realloc) (_p, _s); \
  fprintf (stderr, "Moved block %p to %d bytes at %p\n", _p, _s, _rv); \
  _rv; })
#define free(p) ({ register void *_p = p; \
  (free) (_p); \
  fprintf (stderr, "Freed block %p\n", _p); })
#endif /* 0 */
