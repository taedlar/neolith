#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

malloc_str_t add_slash (const char *);

/*
 * otable.c
 */
void init_otable(size_t sz);
void deinit_otable();
void enter_object_hash(object_t *);
void enter_object_hash_at_end(object_t *);
void remove_object_hash(object_t *);
object_t *lookup_object_hash(const char *);
int show_otable_status(outbuffer_t *, int);
bool make_otable_name (const char* path, char* out, size_t out_size);

/* legacy LPMud strip_name() converts mudlib file names to otable names */
static inline bool strip_name (const char *src, char *dest, size_t size) {
  return make_otable_name(src, dest, size);
}

#ifdef __cplusplus
}
#endif
