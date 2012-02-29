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

#ifndef FACT_VM_H_
#define FACT_VM_H_

extern __thread FACT_thread_t curr_thread; /* Data specific to a thread. */
#define FACT_CURR_THIS  curr_thread->cstackp->this

FACT_t pop_v (void);                /* Pop the var stack.                      */
struct cstack_t pop_c (void);       /* Pop the call stack.                     */
void push_v (FACT_t);               /* Push to the var stack.                  */
void push_c (size_t, FACT_scope_t); /* Push to the call stack.                 */
/* Push a constant value to the var stack: */
void push_constant_str (char *);
void push_constant_ui  (unsigned long int);
void push_constant_si  (  signed long int);

#endif /* FACT_VM_H_ */
