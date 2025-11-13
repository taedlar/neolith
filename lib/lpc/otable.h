#pragma once

int strip_name(const char* src, char* dest, size_t dest_size);
char *add_slash(const char *);

/*
 * otable.c
 */
void init_otable(size_t sz);
void deinit_otable();
void enter_object_hash(object_t *);
void enter_object_hash_at_end(object_t *);
void remove_object_hash(object_t *);
void remove_precompiled_hashes(char *);
object_t *lookup_object_hash(char *);
int show_otable_status(outbuffer_t *, int);
