#pragma once
#include "lpc/types.h"

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <io.h>
  /* define deprecated POSIX names to ISC C names */
  #define open _open
  #define close _close
  #define fdopen _fdopen
  #define fileno _fileno
  #define unlink _unlink
  #define read _read
  #define write _write
  #define mkdir _mkdir
  #define rmdir _rmdir
  #define getcwd _getcwd
#endif

/*
 * file.c
 */

int legal_path(char *);
char *check_valid_path(char *, object_t *, const char *, int);
void smart_log(char *, int, char *, int);
void dump_file_descriptors(outbuffer_t *);

char *read_file(const char *file, int start, int len);
char *read_bytes(char *, int, int, int *);
int write_file(char *, char *, int);
int write_bytes(char *, int, char *, int);
array_t *get_dir(char *, int);
int tail(char *);
int file_size(char *);
int copy_file(char *, char *);
int do_rename(char *, char *, int);
int remove_file(char *);
