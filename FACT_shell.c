/* This file is part of FACT.
 *
 * FACT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FACT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FACT. If not, see <http://www.gnu.org/licenses/>.
 */

#include "FACT.h"
#include "FACT_shell.h"
#include "FACT_types.h"
#include "FACT_lexer.h"
#include "FACT_parser.h"
#include "FACT_comp.h"
#include "FACT_vm.h"
#include "FACT_error.h"
#include "FACT_mpc.h"
#include "FACT_alloc.h"
#include "FACT_hash.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <histedit.h>

static void print_num (FACT_num_t);
static void print_scope (FACT_scope_t);

static int is_blank (const char *str) /* Returns 1 if the rest of a string is junk (comment or whitespace). */ 
{
  for (; *str != '\0' && *str != '#'; str++) {
    if (!isspace (*str))
      return 0;
  }
  return 1;
}

static int is_complete (const char *line) /* Check to see if a line forms a complete statement. */
{
  /* This is not reentrant. */
  static int p_count, b_count, c_count;
  static enum {
    NO_QUOTE = 0,
    IN_DQUOTE,
    IN_SQUOTE,
  } quote_stat;
  bool bslash;
  size_t i;

  /* If line is NULL, take that as a signal to reset all the counts. */
  if (line == NULL) {
    p_count = b_count = c_count = 0;
    return 1;
  }
  
  bslash = false;

  for (i = 0; line[i] != '\0'; i++) {
    switch (line[i]) {
    case '(':
      if (quote_stat == NO_QUOTE)
	p_count++;
      break;

    case ')':
      if (quote_stat == NO_QUOTE
	  && p_count > 0)
	p_count--;
      break;

    case '[':
      if (quote_stat == NO_QUOTE)
	b_count++;
      break;

    case ']':
      if (quote_stat == NO_QUOTE
	  && b_count > 0)
	b_count--;
      break;
	  
    case '{':
      if (quote_stat == NO_QUOTE)
	c_count++;
      break;

    case '}':
      if (quote_stat == NO_QUOTE) {
	if (c_count > 0)
	  c_count--;
	if (p_count == 0
	    && b_count == 0
	    && c_count == 0
	    && is_blank (line + i + 1))
	  return 1;
      }		
      break;

    case ';':
      if (p_count == 0
	  && b_count == 0
	  && c_count == 0
	  && is_blank (line + i + 1))
	return 1;
      break;

    case '\'':
      if (quote_stat == NO_QUOTE)
	quote_stat = IN_SQUOTE;
      else if (quote_stat == IN_SQUOTE) {
	if (!bslash)
	  quote_stat = NO_QUOTE;
      }
      break;

    case '"':
      if (quote_stat == NO_QUOTE)
	quote_stat = IN_DQUOTE;
      else if (quote_stat == IN_DQUOTE) {
	if (!bslash)
	  quote_stat = NO_QUOTE;
      }
      break;

    case '\\':
      bslash = (bslash ? false : true);
      continue; /* Oh hohoho! I'm tricky aren't I! */

    case '#':
      if (quote_stat == NO_QUOTE)
	/* The very fact that we reached this point means that
	 * the statement is incomplete.
	 */
	return 0;
      break;

    default:
      break;
    }

    /* This is skipped over when bslash is set to true, thus
     * it will reset bslash after one iteration.
     */
    bslash = false;
  }

  return 0; /* If we got here, it's an incomplete statement. */
}

static bool new_stmt = true;
static size_t curr_line = 1;

char *shell_prompt (EditLine *e) /* Return the shell prompt. */
{
  static char *prev = NULL;
  static size_t prev_len;
  size_t i, j, k, line;
  
  /* Free the previous prompt or reuse it. */
  if (prev != NULL) {
    if (prev[0] == ' ' && !new_stmt)
      return prev;
    
    FACT_free (prev);
  }

  /* Reallocate and set the prompt. */
  if (new_stmt) {
    i = sizeof (SHELL_START) - 1;
    prev = FACT_malloc_atomic (i + 1);
    strcpy (prev, SHELL_START);
    
    /* Write the line number to the string. */
    line = curr_line;
    do {
      prev[i++] = (line % 10) + '0';
      /* We aren't trying to be very speedy here. */
      prev = FACT_realloc (prev, i + sizeof (SHELL_END));
    } while ((line /= 10) > 0);
    
    for (j = i - 1, k = sizeof (SHELL_START) - 1; k < j; j--, k++) {
      char hold;
      hold = prev[j];
      prev[j] = prev[k];
      prev[k] = hold;
    } 

    /* Set the end prompt. */
    strcpy (prev + i, SHELL_END);
    prev_len = i + sizeof (SHELL_END) - 1;
  } else {
    prev = FACT_malloc_atomic (prev_len - 1 + sizeof (SHELL_CONT));
    memset (prev, ' ', prev_len - 2);
    strcpy (prev + prev_len - 2, SHELL_CONT);
  }

  return prev;
}
      
void FACT_shell (void)
{
  char *input;
  const char *line;
  size_t i;
  volatile size_t last_ip;
  FACT_t *ret_val;
  FACT_tree_t parsed;
  FACT_lexed_t tokenized;
  struct cstack_t frame;

  /* Editline variables. */
  int ignore;
  EditLine *el;
  History *hist;
  HistEvent hev;

  input = NULL;
  last_ip = 0;

  /* Print out some info and then set up the prompt. */
  printf ("FACT version %s\n", FACT_VERSION);

  el = el_init ("/usr/bin/FACT", stdin, stdout, stderr);
  el_set (el, EL_PROMPT, &shell_prompt);
  el_set (el, EL_EDITOR, "emacs");
  hist = history_init ();
  history (hist, &hev, H_SETSIZE, REMEMBER_CMDS);
  el_set (el, EL_HIST, history, hist);

  /* Set error recovery. */
 reset_error:
  if (setjmp (recover)) {
    /* Print out the error and a stack trace. */
    fprintf (stderr, "Caught unhandled error: %s\n", curr_thread->curr_err.what);
    while (curr_thread->cstackp - curr_thread->cstack >= 0) {
      frame = pop_c ();
      if (FACT_is_BIF (frame.this->extrn_func))
	fprintf (stderr, "\tat built-in function %s\n", frame.this->name);
      else
	fprintf (stderr, "\tat scope %s (%s:%zu)\n", frame.this->name, FACT_get_file (frame.ip), FACT_get_line (frame.ip));
    }
    
    push_c (Furlow_offset(), frame.this);
    goto reset_error;
  }
   
  for (;;) { /* This can be simplified a lot I think. */
    new_stmt = true;
    input = NULL;
    i = 1;
    
    do {
      line = el_gets (el, &ignore);
      new_stmt = false;
      if (line != NULL && ignore > 0) {
	i += strlen (line);
	history (hist, &hev, H_ENTER, line);
	if (input == NULL) {
	  input = FACT_malloc_atomic (i);
	  strcpy (input, line);
	} else {
	  input = FACT_realloc (input, i);
	  strcat (input, line);
	}
      } else
	break;
    } while (!is_complete (line));
    
    if (input == NULL)
      break;
      
    /* Tokenize and parse the code. */
    tokenized = FACT_lex_string (input);
    tokenized.line = curr_line;
    
    /* Go through every token and get to the correct line. */
    for (i = 0; tokenized.tokens[i].id != E_END; i++)
      curr_line += tokenized.tokens[i].lines;
    curr_line += tokenized.tokens[i].lines;
      
    if (setjmp (tokenized.handle_err)) {
      /* There was a parsing error, print it and skip. */
      printf ("Parsing error (%s:%zu): %s.\n", "<stdin>", tokenized.line, tokenized.err);
      continue;
    }
	  
    parsed = FACT_parse (&tokenized);
    /* There was no error, continue to compilation. */
    FACT_compile (parsed, "<stdin>", true);
     
    /* Run the code. */
    Furlow_run ();
    
    /* The X register contains the return value of the last expression. */
    ret_val = Furlow_register (R_X);
    printf ("%%");
    if (ret_val->type == UNSET_TYPE) {
      FACT_t *t;
      if (ret_val->home != NULL &&
	  (t = FACT_find_in_table_nohash (ret_val->home, ret_val->ap)) != NULL)
	ret_val = t;
    } 

    if (ret_val->type == UNSET_TYPE)
      printf (" Undef");
    else if (ret_val->type == NUM_TYPE)
      print_num ((FACT_num_t) ret_val->ap);
    else if (FACT_is_BIF (((FACT_scope_t) ret_val->ap)->extrn_func))
      printf (" Built-in function %s", ((FACT_scope_t) ret_val->ap)->name);
    else /* Scope. */
      print_scope ((FACT_scope_t) ret_val->ap);
    printf ("\n");
    Furlow_register (R_X)->type = UNSET_TYPE;
    Furlow_register (R_X)->home = NULL;
  }

  history_end (hist);
  el_end (el);
}

static void print_num (FACT_num_t val)
{
  size_t i;
  
  if (val->array_up != NULL) {
    printf (" [");
    for (i = 0; i < val->array_size; i++) {
      if (i)
	printf (", ");
      print_num (val->array_up[i]);
    }
    printf (" ]");
  } else 
    printf (" %s", mpc_get_str (val->value));
}

static void print_scope (FACT_scope_t val)
{
  size_t i;

  if (*val->marked) 
    return;
  
  if (*val->array_up != NULL) {
    printf (" [");
    for (i = 0; i < *val->array_size; i++) {
      if (i)
	printf (", ");
      print_scope ((*val->array_up)[i]);
    }
    printf (" ]");
  } else {
    printf ("{ %s%s ", val->name, ((val->vars->num_entries == 0) ? "\0" : ":"));
    FACT_table_digest (val->vars);
    printf ("}");
  }
}
