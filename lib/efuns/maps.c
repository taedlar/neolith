#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "lpc/array.h"
#include "lpc/mapping.h"


#ifdef F_ALLOCATE_MAPPING
void
f_allocate_mapping (void)
{
  sp->type = T_MAPPING;
  sp->u.map = allocate_mapping (sp->u.number);
}
#endif


#ifdef F_KEYS
void
f_keys (void)
{
  array_t *vec;

  vec = mapping_indices (sp->u.map);
  free_mapping (sp->u.map);
  put_array (vec);
}
#endif


#ifdef F_VALUES
void
f_values (void)
{
  array_t *vec;

  vec = mapping_values (sp->u.map);
  free_mapping (sp->u.map);
  put_array (vec);
}
#endif


#ifdef F_MAP_DELETE
void
f_map_delete (void)
{
  mapping_delete ((sp - 1)->u.map, sp);
  pop_stack ();
#ifndef COMPAT_32
  free_mapping ((sp--)->u.map);
#endif
}
#endif


#ifdef F_MAPP
void
f_mapp (void)
{
  if (sp->type == T_MAPPING)
    {
      free_mapping (sp->u.map);
      *sp = const1;
    }
  else
    {
      free_svalue (sp, "f_mapp");
      *sp = const0;
    }
}
#endif


#ifdef F_MAP
void
f_map (void)
{
  svalue_t *arg = sp - st_num_arg + 1;

  if (arg->type == T_MAPPING)
    map_mapping (arg, st_num_arg);
  else if (arg->type == T_ARRAY)
    map_array (arg, st_num_arg);
  else
    map_string (arg, st_num_arg);
}
#endif

/*
This efun searches a mapping for a path.  Each key is assumed to be a
string.  The value is completely arbitrary.  The efun finds the largest
matching path in the mapping.  Keys ended in '/' are assumed to match
paths with character that follow the '/', i.e. / is a wildcard for anything
below this directory.  DO NOT CHANGE THIS EFUN TIL YOU UNDERSTAND IT.  It
catches folks by suprise at first, but it is coded the way it is for a reason.
It effectively implements the search loop in TMI's access object as a single
efun.

Cygnus

* Changed to improve the way ES2 uses by Annihilator (05/13/2000)

*/
#ifdef F_MATCH_PATH
void
f_match_path (void)
{
  svalue_t *value;
  register char *src, *dst;
  svalue_t *nvalue;
  mapping_t *map;
  char *tmpstr;

  value = &const0u;

  tmpstr = DMALLOC (SVALUE_STRLEN (sp) + 1, TAG_STRING, "match_path");

  src = sp->u.string;
  dst = tmpstr;
  map = (sp - 1)->u.map;

  while (*src != '\0')
    {
      while (*src != '/' && *src != '\0')
        *dst++ = *src++;
      if (*src == '/')
        {
          while (*++src == '/');
          if (dst == tmpstr)
            continue;
        }
      *dst = '\0';
      nvalue = find_string_in_mapping (map, tmpstr);

      value = nvalue;

      if (value == &const0u)
        break;
      if (value->type != T_MAPPING)
        {
          if (*src != '\0')
            value = &const0u;
          break;
        }
      map = value->u.map;
      dst = tmpstr;
    }

  FREE (tmpstr);
  /* Don't free mapping first, in case sometimes one uses a ref 1 mapping */
  /* Randor - 5/29/94 */
  free_string_svalue (sp--);
  map = sp->u.map;
  assign_svalue_no_free (sp, value);
  free_mapping (map);
}
#endif /* F_MATCH_PATH */

