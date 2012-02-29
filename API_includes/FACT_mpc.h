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

#ifndef FACT_MPC_H_
#define FACT_MPC_H_

#include <gmp.h>

/* Because mpc is designed as a small extension upon GMP (one that will
 * never be developed as its own independent project), GMP/MPC naming
 * conventions are used.
 */

/* The mpc_t is used for arbitrary precision arithmetic. */
typedef struct 
{
  mpz_t value;      /* Arbitray-length integer.                  */
  size_t precision; /* Number of digits after the decimal point. */
} __mpc_struct;

typedef __mpc_struct mpc_t[1];

void mpc_set_default_prec (unsigned long);
unsigned long mpc_get_default_prec (void);

void mpc_init (mpc_t);
void mpc_clear (mpc_t);

void mpc_set (mpc_t, mpc_t);
void mpc_set_ui (mpc_t, unsigned long);
void mpc_set_si (mpc_t, signed long);
void mpc_set_str (mpc_t, char *, int);

/* Arithmetic functions. */
void mpc_add (mpc_t, mpc_t, mpc_t);
void mpc_sub (mpc_t, mpc_t, mpc_t);
void mpc_mul (mpc_t, mpc_t, mpc_t);
void mpc_div (mpc_t, mpc_t, mpc_t);
void mpc_mod (mpc_t, mpc_t, mpc_t);

/* Bitwise functions. */
void mpc_and (mpc_t, mpc_t, mpc_t);
void mpc_ior (mpc_t, mpc_t, mpc_t);
void mpc_xor (mpc_t, mpc_t, mpc_t);

void mpc_neg (mpc_t, mpc_t);

int mpc_cmp (mpc_t, mpc_t);
int mpc_cmp_ui (mpc_t, unsigned long int);
int mpc_cmp_si (mpc_t, signed long int);

unsigned long int mpc_get_ui (mpc_t);
signed long int mpc_get_si (mpc_t);
char *mpc_get_str (mpc_t);

static inline void
mpc_add_ui (mpc_t rop, mpc_t op1, unsigned int op2)
{
  op2 *= pow_10 (op1->precision);
  mpz_add_ui (rop->value, op1->value, op2);
  rop->precision = op1->precision;
}

static inline void
mpc_sub_ui (mpc_t rop, mpc_t op1, unsigned int op2)
{
  op2 *= pow_10 (op1->precision);
  mpz_sub_ui (rop->value, op1->value, op2);
  rop->precision = op1->precision;
}

#endif /* FACT_MPC_H_ */
