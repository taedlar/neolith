#pragma once

#include "lpc/types.h"
#include "misc/filepath.h"

#ifdef _WIN32
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

#ifdef __cplusplus
extern "C" {
#endif

/* legacy LPMud file path validation. Now refactored to use C++17 filesystem */
static inline int legal_path(const char *path) { return path_is_legal_relative(path); }

bool push_valid_path(const char* path, object_t *caller_ob, const char *call_fun, int writeflg);

bool push_resolved_valid_path(const char* path, object_t *caller_ob, const char *call_fun, int writeflg);

void dump_file_descriptors(outbuffer_t *);

char *read_file(const char *file, long start, size_t len);
char *read_bytes(const char *file, long start, size_t len, size_t *rlen);
int write_file(const char *file, const char *str, int flags);
int write_bytes(const char *file, long start, const char *buf, size_t len);
array_t *get_dir(const char *path, int flag);
int tail(const char *file);
int file_size(const char *file);
int copy_file(const char *from, const char *to);
int do_rename(const char *from, const char *to, int flag);
int remove_file(const char *file);

#ifdef __cplusplus
}
#endif
