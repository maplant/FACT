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

#ifndef FACT_SHELL_H_
#define FACT_SHELL_H_

/* The initial shell prompt is two strings, with a line number in
 * between them. The incomplete prompt is just one part preceded by
 * the length of the initial prompt minus two in spaces. 
 */  
#define SHELL_START "["   /* The first part of the prompt. */
#define SHELL_END   "] " /* And the third part.           */
#define SHELL_CONT  "  "

#define REMEMBER_CMDS 100 

void FACT_shell ();

#endif /* FACT_SHELL_H_ */
