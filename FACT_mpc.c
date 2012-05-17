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

#include <FACT.h>

unsigned long default_prec = 6;

static void pow_of_ten (mpz_t rop, unsigned long op)
{
  unsigned long i;

  for (i = 0; i < op; i++)
    mpz_mul_ui (rop, rop, 10);
}

void mpc_set_default_prec (unsigned long prec)
{
  default_prec = prec;
}

unsigned long mpc_get_default_prec ()
{
  return default_prec;
}

void mpc_init (mpc_t new)
{
  new->precision = 0;
  mpz_init (new->value);
}

void mpc_clear (mpc_t dead)
{
  mpz_clear (dead->value);
}

void mpc_set (mpc_t rop, mpc_t op)
{
  rop->precision = op->precision;
  mpz_set (rop->value, op->value);
}

void mpc_set_ui (mpc_t rop, unsigned long op)
{
  rop->precision = 0;
  mpz_set_ui (rop->value, op);
}

void mpc_set_si (mpc_t rop, signed long op)
{
  rop->precision = 0;
  mpz_set_si (rop->value, op);
}

static unsigned int get_prec (char *val)
{
  unsigned int i;
  
  for (i = 0; val[i] != '\0'; i++) {
    if (val[i] == '.') {
      val[i] = ' ';
      return strlen (val + i + 1);
    }
  }
  return 0;
}

void mpc_set_str (mpc_t rop, char *str, int base) /* Convert a string to an mpc type. */
{
  char *cpy;
  cpy = FACT_malloc_atomic (strlen (str) + 1);
  strcpy (cpy, str);
  rop->precision = get_prec (cpy);
  mpz_set_str (rop->value, cpy, base);
  FACT_free (cpy);
}

void mpc_add (mpc_t rop, mpc_t op1, mpc_t op2)
{
  mpz_t temp; 
  
  if (op1->precision == op2->precision) {
    mpz_add (rop->value, op1->value, op2->value);
    rop->precision = op1->precision;
  } else if (op1->precision > op2->precision) {
    mpz_init_set (temp, op2->value);
    pow_of_ten (temp, op1->precision - op2->precision);
    mpz_add (rop->value, temp, op1->value);
    mpz_clear (temp);
    rop->precision = op1->precision;
  } else {
    mpz_init_set (temp, op1->value);
    pow_of_ten (temp, op2->precision - op1->precision);
    mpz_add (rop->value, temp, op2->value);
    mpz_clear (temp);
    rop->precision = op2->precision;
  }
}

void mpc_sub (mpc_t rop, mpc_t op1, mpc_t op2)
{
  mpz_t temp; 
  
  if (op1->precision == op2->precision) {
    mpz_sub (rop->value, op1->value, op2->value);
    rop->precision = op1->precision;
  } else if (op1->precision > op2->precision) {
    mpz_init_set (temp, op2->value);
    pow_of_ten (temp, op1->precision - op2->precision);
    mpz_sub (rop->value, op1->value, temp);
    mpz_clear (temp);
    rop->precision = op1->precision;
  } else {
    mpz_init_set (temp, op1->value);
    pow_of_ten (temp, op2->precision - op1->precision);
    mpz_sub (rop->value, temp, op2->value);
    mpz_clear (temp);
    rop->precision = op2->precision;
  }
}

void mpc_neg (mpc_t rop, mpc_t op)
{
  rop->precision = op->precision;
  mpz_neg (rop->value, op->value);
}

void mpc_mul (mpc_t rop, mpc_t op1, mpc_t op2)
{
  mpz_mul (rop->value, op1->value, op2->value);
  rop->precision = op1->precision + op2->precision;
  /*
  mpz_t temp;
  mpz_init_set (temp, op2->value);
  mpz_mul (temp, op1->value, temp);
  rop->precision = op1->precision + op2->precision;
  mpz_swap (rop->value, temp);
  mpz_clear (temp);
  */
}

void mpc_div (mpc_t rop, mpc_t op1, mpc_t op2)
{
  size_t i;
  unsigned long prec;
  mpf_t hold_op1, hold_op2, hold_res;

  mpf_init (hold_op1);
  mpf_init (hold_op2);
  mpf_init (hold_res);
  mpf_set_z (hold_op1, op1->value);
  mpf_set_z (hold_op2, op2->value);

  /* Get the largest precision. */
  prec = (op1->precision > op2->precision) ? op1->precision : op2->precision;
  if (prec == 0)
    prec = default_prec;

  /* Set the scalar values. */
  for (i = op1->precision; i > 0; i--)
    mpf_div_ui (hold_op1, hold_op1, 10);
  for (i = op2->precision; i > 0; i--)
    mpf_div_ui (hold_op2, hold_op2, 10);

  /* Get the resulting value. */
  mpf_div (hold_res, hold_op1, hold_op2);

  for (rop->precision = prec; prec > 0; prec--)
    mpf_mul_ui (hold_res, hold_res, 10);
  
  mpz_set_f (rop->value, hold_res);
  mpf_clear (hold_op1);
  mpf_clear (hold_op2);
  mpf_clear (hold_res);
}

void mpc_mod (mpc_t rop, mpc_t op1, mpc_t op2)
{
  size_t i;
  unsigned long prec;
  mpf_t hold_op1, hold_op2, hold_res;

  mpf_init (hold_op1);
  mpf_init (hold_op2);
  mpf_init (hold_res);
  mpf_set_z (hold_op1, op1->value);
  mpf_set_z (hold_op2, op2->value);

  /* Get the largest precision. */
  prec = (op1->precision > op2->precision) ? op1->precision : op2->precision;
  if (prec == 0)
    prec = default_prec;

  /* Set the scalar values. */
  for (i = op1->precision; i > 0; i--)
    mpf_div_ui (hold_op1, hold_op1, 10);
  for (i = op2->precision; i > 0; i--)
    mpf_div_ui (hold_op2, hold_op2, 10);

  /* Get the resulting value. */
  mpf_div (hold_res, hold_op1, hold_op2);
  mpf_floor (hold_res, hold_res);
  mpf_mul (hold_res, hold_res, hold_op2);
  mpf_sub (hold_res, hold_op1, hold_res);

  for (rop->precision = prec; prec > 0; prec--)
    mpf_mul_ui (hold_res, hold_res, 10);
  
  mpz_set_f (rop->value, hold_res);
  mpf_clear (hold_op1);
  mpf_clear (hold_op2);
  mpf_clear (hold_res);
}

/* Bitwise functions ignore precision. */

void mpc_and (mpc_t rop, mpc_t op1, mpc_t op2)
{
  rop->precision = 0;
  mpz_and (rop->value, op1->value, op2->value);
}

void mpc_ior (mpc_t rop, mpc_t op1, mpc_t op2)
{
  rop->precision = 0;
  mpz_and (rop->value, op1->value, op2->value);
}

void mpc_xor (mpc_t rop, mpc_t op1, mpc_t op2)
{
  rop->precision = 0;
  mpz_and (rop->value, op1->value, op2->value);
}

int mpc_cmp (mpc_t op1, mpc_t op2)
{
  int ret;
  mpz_t temp;

  if (op1->precision == op2->precision)
    return mpz_cmp (op1->value, op2->value);
  else if (op1->precision > op2->precision) {
    mpz_init_set (temp, op2->value);
    pow_of_ten (temp, op1->precision - op2->precision);
    ret = mpz_cmp (op1->value, temp);
  } else {
    mpz_init_set (temp, op1->value);
    pow_of_ten (temp, op2->precision - op1->precision);
    ret = mpz_cmp (temp, op2->value);
  }
  
  mpz_clear (temp);
  return ret;
}

int mpc_cmp_ui (mpc_t op1, unsigned long int op2)
{
  unsigned int i;

  for (i = 0; i < op1->precision; i++)
    op2 *= 10;

  return mpz_cmp_ui (op1->value, op2);
}

int mpc_cmp_si (mpc_t op1, signed long int op2)
{
  unsigned int i;
  
  for (i = 0; i < op1->precision; i++)
    op2 *= 10;

  return mpz_cmp_si (op1->value, op2);
}

/* TODO: make these next few functions better. */

unsigned long int mpc_get_ui (mpc_t rop)
{
  unsigned long int ret, i;
  mpz_t temp1;
  //  mpz_t temp2;
 
  mpz_init_set (temp1, rop->value);
  for (i = 0; i < rop->precision; i++)
    mpz_tdiv_q_ui (temp1, temp1, 10);
  //  mpz_init_set_ui (temp2, 1);
  // pow_of_ten (temp2, rop->precision);
  // mpz_tdiv_q (temp1, temp1, temp2);
  ret = mpz_get_ui (temp1);

  mpz_clear (temp1);
  // mpz_clear (temp2);
  return ret;
}

signed long int mpc_get_si (mpc_t rop)
{
  signed long int ret;
  mpz_t temp1;
  mpz_t temp2;

  mpz_init_set (temp1, rop->value);
  mpz_init_set_ui (temp2, 1);
  pow_of_ten (temp2, rop->precision);
  mpz_tdiv_q (temp1, temp1, temp2);

  ret = mpz_get_si (temp1);
  mpz_clear (temp1);
  mpz_clear (temp2);
  return ret;
}

char *mpc_get_str (mpc_t rop)
{
  char *res;
  _Bool sign;
  size_t len;

  res = FACT_malloc_atomic (mpz_sizeinbase (rop->value, 10) + 2);
  mpz_get_str (res, 10, rop->value);

  if (rop->precision && mpz_cmp_ui (rop->value, 0)) {
    len = strlen (res);
    if (sign = (*res == '-'))
      memmove (res, res + 1, len--);
    if (rop->precision >= len) {
      res = FACT_realloc (res, rop->precision + 1);
      memmove (res + rop->precision - len, res, len + 1);
      memset (res, '0', rop->precision - len);
      len = rop->precision;
    }
    res = FACT_realloc (res, (len + (sign ? 3 : 2)));
    memmove (res + len - rop->precision + 1, res + len - rop->precision, rop->precision + 1);
    res[len - rop->precision] = '.';
    if (sign) {
      memmove (res + 1, res, strlen (res) + 1);
      *res = '-';
    }
  }
  return res;
}
