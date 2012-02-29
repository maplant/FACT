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

/**
 * FACT_load_lib:
 * This function simply loads a library, searches for the "MOD_MAP" symbol
 * and loads all of the listed functions. It's very simple and probably
 * flawed. I'll fix that later.
 */

void FACT_load_lib (char *lib_name)
{
  void *lib;
  size_t i;
  FACT_scope_t temp;
  struct mapping {
    const char *func_name;
    void (*func)(void);
  } *map;

  lib = dlopen (lib_name, RTLD_LAZY);
  if (lib == NULL)
    FACT_throw_error ("could not load library \"%s\"", lib_name);

  map = (struct mapping *) dlsym (lib, "MOD_MAP");
  if (map == NULL)
    FACT_throw_error ("could not find MOD_MAP symbol in library \"%s\"", lib_name);

  for (i = 0; map[i].func_name != NULL && map[i].func != NULL; i++) {
    temp = FACT_add_scope (CURR_THIS, map[i].func_name);
    temp->extrn_func = map[i].func;
  }
}
  
  
