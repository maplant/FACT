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

#ifndef FACT_H_
#define FACT_H_

#define FACT_VERSION 0.0.1

#include <FACT/FACT_mpc.h>
#include <FACT/FACT_types.h>
#include <FACT/FACT_error.h>
#include <FACT/FACT_file.h>
#include <FACT/FACT_alloc.h>
#include <FACT/FACT_strs.h>
#include <FACT/FACT_scope.h>
#include <FACT/FACT_num.h>
#include <FACT/FACT_var.h>
#include <FACT/FACT_threads.h>
#include <FACT/FACT_vm.h>

#define FACT_MOD_MAP()				\
  struct {					\
    const char *func_name;			\
    void (*func)(void);				\
  } MOD_MAP[]

#endif /* FACT_H_ */
