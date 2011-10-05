/* This file is part of Furlow VM.
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

#include <FACT.h>

static void pretty_print_num (FACT_num_t, size_t);
static void pretty_print_scope (FACT_scope_t, size_t);
static inline void format_spcs (size_t);

void
Furlow_print_state () /* Print out debug information. */
{
  size_t i;

  printf ("Furlow VM current state:\n"
	  " * CURR_IP = %lu\n"
	  " * Thread # = %lu\n"
	  " * call stack size = %lu\n"
	  " * call stack contents:\n",
	  CURR_IP, curr_thread - threads, curr_thread->cstack_size);

  for (i = 0; i < curr_thread->cstack_size; i++)
    printf ("   * (%lu): Name = %s, ip = %lu\n", i, curr_thread->cstack[i].this->name, curr_thread->cstack[i].ip);

  if (curr_thread->vstack_size)
    {
      printf (" * var stack size = %lu\n", curr_thread->vstack_size);
      printf (" * var stack contents:\n");
  
      for (i = 0; i < curr_thread->vstack_size; i++)
	{
	  printf ("   * (%lu): ", i);
	  if (curr_thread->vstack[i].type == NUM_TYPE)
	    {
	      printf ("[number]:");
	      pretty_print_num (curr_thread->vstack[i].ap, 5);
	      printf ("\n");
	    }
	  else
	    {
	      printf ("[scope]:");
	      pretty_print_scope (curr_thread->vstack[i].ap, 5);
	      printf ("\n");
	    }
	}
    }
  else
    printf (" * var stack is empty.\n");
}

void
Furlow_print_registers () /* Print out the register values. */
{
  int i;

  for (i = 0; i < T_REGISTERS; i++)
    {
      printf ("$%d ", i);
      switch (curr_thread->registers[i].type)
	{
	case NUM_TYPE:
	  printf ("[number]:");
	  pretty_print_num (curr_thread->registers[i].ap, 0);
	  printf ("\n");
	  break;

	case SCOPE_TYPE:
	  printf ("[scope]:");
	  pretty_print_scope (curr_thread->registers[i].ap, 0);
	  printf ("\n");
	  break;

	default:
	  printf ("[unset]\n");
	  break;
	}
    }
}

static void
pretty_print_num (FACT_num_t val, size_t depth)
{
  size_t i;
  
  if (val->array_up != NULL)
    {
      format_spcs (depth);
      printf ("[");
      for (i = 0; i < val->array_size; i++)
	{
	  if (i)
	    printf (", ");
	  pretty_print_num (val->array_up[i], depth + 1);
	}
      format_spcs (depth);
      printf ("]");
    }
  else
    {
      format_spcs (depth);
      printf (" %s", mpc_get_str (val->value));
    }
}

static void
pretty_print_scope (FACT_scope_t val, size_t depth)
{
  size_t i;
  
  if (*val->array_up != NULL)
    {
      format_spcs (depth);
      printf ("[");
      for (i = 0; i < *val->array_size; i++)
	{
	  if (i)
	    printf (", ");
	  pretty_print_scope ((*val->array_up)[i], depth + 1);
	}
      format_spcs (depth);
      printf ("]");
    }
  else
    {
      format_spcs (depth);
      printf ("{");
      format_spcs (++depth);
      printf ("* name = '%s'", val->name);
      format_spcs (depth);
      printf ("* code = %lu", *val->code);
      /*
      if (*val->num_stack_size)
	{
	  format_spcs (depth);
	  printf ("* Numbers: ");
	  depth++;
	  for (i = 0; i < *val->num_stack_size; i++)
	    {
	      format_spcs (depth);
	      printf ("- %s:", (*val->num_stack)[i]->name);
	      pretty_print_num ((*val->num_stack)[i], depth + 1);
	    }
	}
      */
      depth--;
      format_spcs (depth);
      printf ("}");
    }
}

static inline void 
format_spcs (size_t n)
{
  size_t i;

  printf ("\n");
  for (i = 0; i <= n; i++)
    putchar (' ');
}
