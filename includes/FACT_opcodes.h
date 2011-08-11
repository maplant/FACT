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

#ifndef FACT_OPCODES_H_
#define FACT_OPCODES_H_

/* Furlow VM bytecode instructions. */
typedef enum Furlow_opcode 
  {
    /* Memory management:                                       */
    REF = 0,  /* Create a reference.                            */
    STO,      /* Copy one var to the other.                     */
    VAR,      /* Retrieve and push a variable to the stack.     */
    CONST,    /* Retrieve a constant value and push it.         */
    NEW_V,    /* Allocate a var and push it to the var stack.   */
    NEW_S,    /* Allocate a scope and push it to the var stack. */
    ELEM,     /* Get the element of an array.                   */

    /* Register manipulation.                                   */
    INC,      /* Increment a register by 1.                     */
    DEC,      /* Decrement a register by 1.                     */
    
    /* Arithmetic:                                              */
    ADD,      /* Addition.                                      */
    SUB,      /* Subraction.                                    */
    MUL,      /* Multiplication.                                */
    DIV,      /* Division.                                      */
    MOD,      /* Modulo.                                        */
    XOR,      /* Bitwise exclusive OR.                          */
    IOR,      /* Bitwise inclusive OR.                          */
    AND,      /* Bitwise AND.                                   */
    NEG,      /* Negative.                                      */

    /* Boolean operations:                                      */
    CMT,      /* More than.                                     */
    CME,      /* More than, equal.                              */
    CLT,      /* Less than.                                     */
    CLE,      /* Less than, equal.                              */
    CEQ,      /* Equal.                                         */
    CNE,      /* Not equal.                                     */

    /* Branching and jumps:                                     */
    JMP,      /* Unconditional jump.                            */
    JIT,      /* Jump on true.                                  */
    JIF,      /* Jump on false.                                 */
    SPRT,     /* Create a new thread and unconditionally jump.  */

    /* Function and scope handling:                             */
    SET_C,    /* Set the jump address of a function.            */
    SET_F,    /* Set the function data of a scope.              */
    CALL,     /* Push to the call stack and jump to a function. */
    RET,      /* Pop the call stack.                            */
    USE,      /* Push to the call stack.                        */
    EXIT,     /* Like ret, except the ip is left unchanged.     */
    THIS,     /* Push the this scope to the variable stack.     */

    /* Variable handling:                                       */
    DEF_V,    /* Define a new variable in the this scope.       */
    DEF_S,    /* Define a new scope in the this scope.          */

    /* Error handling:                                          */
    TRAP,     /* Push to the trap stack.                        */
    END_TRAP, /* Pop the trap stack.                            */
    ERR_D,    /* Push the description of curr_err to the stack. */
    ERR_S,    /* Push the scope of curr_err to the stack.       */

    /* Dynamic library loading:                                 */
    LIB,      /* Load an external library.                      */
    EXTRN,    /* Run a function from a loaded library.          */
    
    /* General:                                                 */
    HALT,     /* Halt execution.                                */
    NEW_L     /* Newline; used for error messages.              */
  } Furlow_opc_t;

#define MAX_INSTRUCTION_LEN 10

#endif /* FACT_OPCODES_H_ */
