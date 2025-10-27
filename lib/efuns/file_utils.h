#pragma once
#include "lpc/types.h"

/*
 * file.c
 */

int legal_path(char *);
char *check_valid_path(char *, object_t *, const char *, int);
void smart_log(char *, int, char *, int);
void dump_file_descriptors(outbuffer_t *);

char *read_file(char *, int, int);
char *read_bytes(char *, int, int, int *);
int write_file(char *, char *, int);
int write_bytes(char *, int, char *, int);
array_t *get_dir(char *, int);
int tail(char *);
int file_size(char *);
int copy_file(char *, char *);
int do_rename(char *, char *, int);
int remove_file(char *);
