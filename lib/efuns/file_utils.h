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
  #define mkdir(dir,mode) _mkdir(dir)
  #define rmdir _rmdir
  #define getcwd _getcwd
#endif

/*
 * file.c
 */

int legal_path(const char *);
char *check_valid_path(const char* path, object_t *, const char *, int);
void smart_log(char *, int, char *, int);
void dump_file_descriptors(outbuffer_t *);

char *read_file(const char *file, long start, size_t len);
char *read_bytes(const char *file, long start, size_t len, size_t *rlen);
int write_file(char* file, char* str, int flags);
int write_bytes(char* file, long start, char *, size_t len);
array_t *get_dir(char *, int);
int tail(char *);
int file_size(char *);
int copy_file(char *, char *);
int do_rename(char *, char *, int);
int remove_file(char *);
