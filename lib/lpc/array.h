#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

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
extern size_t total_array_size;
#endif

int sameval(svalue_t *, svalue_t *);
array_t *allocate_array(size_t);
array_t *allocate_empty_array(size_t);
void free_array(array_t *);
void free_empty_array(array_t *);
void check_for_destr(array_t *);
array_t *add_array(array_t *, array_t *);
void implode_array(funptr_t *, array_t *, svalue_t *, int);
array_t *subtract_array(array_t *, array_t *);
array_t *slice_array(array_t* a, int from, int to);
array_t *explode_string(const char *, size_t, const char *, size_t);
malloc_str_t implode_string(array_t *, const char *, size_t);
void filter_array(svalue_t *, int);
array_t *deep_inventory(object_t *, bool take_top);
array_t *filter(array_t *, funptr_t *, svalue_t *);
array_t *fp_sort_array(array_t *, funptr_t *);
array_t *sort_array(array_t *, const char *, object_t *);
array_t *make_unique(array_t *, const char *, funptr_t *, svalue_t *);
void map_string(svalue_t *arg, int num_arg);
void map_array(svalue_t *arg, int num_arg);
array_t *intersect_array(array_t *, array_t *);
void dealloc_array(array_t *);

#ifdef __cplusplus
}
#endif
