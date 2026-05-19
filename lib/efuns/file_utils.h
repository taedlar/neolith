#pragma once

#include "lpc/types.h"

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

void dump_file_descriptors(outbuffer_t *);

malloc_str_t do_read_file(const char *file, long start, size_t len);
malloc_str_t do_read_bytes(const char *file, long start, size_t len, size_t *rlen);
int do_write_file(const char *file, const char *str, size_t len, int flags);
int do_write_bytes(const char *file, long start, const char *buf, size_t len);
array_t *get_dir(const char *path, int flag);
int do_tail_file(const char *file);
int get_file_size(const char *file);
int do_copy_file(const char *from, const char *to);
int do_rename(const char *from, const char *to, int flag);
int do_remove_file(const char *file);

#ifdef __cplusplus
}
#endif
