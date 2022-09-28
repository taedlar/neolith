/*  $Id: array.h,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith

    ORIGINAL AUTHOR
	Unknown

    MODIFIED BY
	[2001-06-27] by Annihilator <annihilator@muds.net>, see CVS log.
 */

#ifndef _LPC_ARRAY_H
#define _LPC_ARRAY_H

#include "types.h"

struct array_s {
    unsigned short ref;
#ifdef DEBUG
    int extra_ref;
#endif
    unsigned short size;
    svalue_t item[1];
};

extern array_t the_null_array;

/*
 * array.c
 */
#ifdef ARRAY_STATS
extern int num_arrays;
extern int total_array_size;
#endif

int sameval(svalue_t *, svalue_t *);
array_t *allocate_array(int);
array_t *allocate_empty_array(int);
void free_array(array_t *);
void free_empty_array(array_t *);
array_t *add_array(array_t *, array_t *);
void implode_array(funptr_t *, array_t *, svalue_t *, int);
array_t *subtract_array(array_t *, array_t *);
array_t *slice_array(array_t *, int, int);
array_t *explode_string(char *, int, char *, int);
char *implode_string(array_t *, char *, int);
array_t *users(void);
array_t *commands(object_t *);
void filter_array(svalue_t *, int);
array_t *deep_inherit_list(object_t *);
array_t *inherit_list(object_t *);
array_t *children(char *);
array_t *livings(void);
array_t *objects(funptr_t *);
array_t *all_inventory(object_t *, int);
array_t *deep_inventory(object_t *, int);
array_t *filter(array_t *, funptr_t *, svalue_t *);
array_t *builtin_sort_array(array_t *, int);
array_t *fp_sort_array(array_t *, funptr_t *);
array_t *sort_array(array_t *, char *, object_t *);
array_t *make_unique(array_t *, char *, funptr_t *, svalue_t *);
void map_string(svalue_t *arg, int num_arg);
void map_array(svalue_t *arg, int num_arg);
array_t *intersect_array(array_t *, array_t *);
int match_single_regexp(char *, char *);
array_t *match_regexp(array_t *, char *, int);
array_t *reg_assoc(char *, array_t *, array_t *, svalue_t *);
void dealloc_array(array_t *);

#define ALLOC_ARRAY(nelem) \
    (array_t *)DXALLOC(sizeof (array_t) + \
	  sizeof(svalue_t) * (nelem - 1), TAG_ARRAY, "ALLOC_ARRAY")
#define RESIZE_ARRAY(vec, nelem) \
    (array_t *)DREALLOC(vec, sizeof (array_t) + \
	  sizeof(svalue_t) * (nelem - 1), TAG_ARRAY, "RESIZE_ARRAY")

#endif	/* ! _LPC_ARRAY_H */
