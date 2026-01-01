#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <sys/stat.h>
#include <sys/types.h>

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

#include "src/std.h"
#include "rc.h"
#include "lpc/array.h"
#include "lpc/object.h"
#include "lpc/include/runtime_config.h"
#include "src/interpret.h"
#include "lpc/lex.h"

#include "file_utils.h"

/* see binaries.c.  We don't want no $@$(*)@# system dependent mess of includes */

extern int sys_nerr;

static int match_string (char *, char *);
static int copy (char *from, char *to);
static int do_move (char *from, char *to, int flag);
static int pstrcmp (const void *, const void *);
static int parrcmp (const void *, const void *);
static void encode_stat (svalue_t *, int, char *, struct stat *);

#define MAX_LINES 50

/*
 * These are used by qsort in get_dir().
 */
static int
pstrcmp (const void *p1, const void *p2)
{
  svalue_t *x = (svalue_t *) p1;
  svalue_t *y = (svalue_t *) p2;

  return strcmp (x->u.string, y->u.string);
}

static int
parrcmp (const void *p1, const void *p2)
{
  svalue_t *x = (svalue_t *) p1;
  svalue_t *y = (svalue_t *) p2;

  return strcmp (x->u.arr->item[0].u.string, y->u.arr->item[0].u.string);
}

static void
encode_stat (svalue_t * vp, int flags, char *str, struct stat *st)
{
  if (flags == -1)
    {
      array_t *v = allocate_empty_array (3);

      v->item[0].type = T_STRING;
      v->item[0].subtype = STRING_MALLOC;
      v->item[0].u.string = string_copy (str, "encode_stat");
      v->item[1].type = T_NUMBER;
      v->item[1].u.number = ((st->st_mode & S_IFDIR) ? -2 : st->st_size);
      v->item[2].type = T_NUMBER;
      v->item[2].u.number = st->st_mtime;
      vp->type = T_ARRAY;
      vp->u.arr = v;
    }
  else
    {
      vp->type = T_STRING;
      vp->subtype = STRING_MALLOC;
      vp->u.string = string_copy (str, "encode_stat");
    }
}

/*
 * List files in directory. This function do same as standard list_files did,
 * but instead writing files right away to user this returns an array
 * containing those files. Actually most of code is copied from list_files()
 * function.
 * Differences with list_files:
 *
 *   - file_list("/w"); returns ({ "w" })
 *
 *   - file_list("/w/"); and file_list("/w/."); return contents of directory
 *     "/w"
 *
 *   - file_list("/");, file_list("."); and file_list("/."); return contents
 *     of directory "/"
 *
 * With second argument equal to non-zero, instead of returning an array
 * of strings, the function will return an array of arrays about files.
 * The information in each array is supplied in the order:
 *    name of file,
 *    size of file,
 *    last update of file.
 */
/* WIN32 should be fixed to do this correctly (i.e. no ifdefs for it) */
#define MAX_FNAME_SIZE 255
#define MAX_PATH_LEN   1024

array_t *
get_dir (char *path, int flags)
{
  array_t *v;
  int i, count = 0;
#ifdef HAVE_DIRENT_H
  DIR *dirp;
#elif _WIN32
  HANDLE dirp;
  WIN32_FIND_DATA findFileData;
  char searchPath[MAX_FNAME_SIZE + MAX_PATH_LEN + 2];
#endif
  int namelen, do_match = 0;

  struct dirent *de;
  struct stat st;
  char *endtemp;
  char temppath[MAX_FNAME_SIZE + MAX_PATH_LEN + 2];
  char regexppath[MAX_FNAME_SIZE + MAX_PATH_LEN + 2];
  char *p;

  if (!path)
    return 0;

  path = check_valid_path (path, current_object, "stat", 0);

  if (path == 0)
    return 0;

  if (strlen (path) < 2)
    {
      temppath[0] = path[0] ? path[0] : '.';
      temppath[1] = '\000';
      p = temppath;
    }
  else
    {
      strncpy (temppath, path, MAX_FNAME_SIZE + MAX_PATH_LEN + 1);
      temppath[MAX_FNAME_SIZE + MAX_PATH_LEN + 1] = '\0';

      /*
       * If path ends with '/' or "/." remove it
       */
      if ((p = strrchr (temppath, '/')) == 0)
        p = temppath;
      if (p[0] == '/' && ((p[1] == '.' && p[2] == '\0') || p[1] == '\0'))
        *p = '\0';
    }

  if (stat (temppath, &st) < 0)
    {
      if (*p == '\0')
        return 0;
      if (p != temppath)
        {
          strcpy (regexppath, p + 1);
          *p = '\0';
        }
      else
        {
          strcpy (regexppath, p);
          strcpy (temppath, ".");
        }
      do_match = 1;
    }
  else if (*p != '\0' && strcmp (temppath, "."))
    {
      if (*p == '/' && *(p + 1) != '\0')
        p++;
      v = allocate_empty_array (1);
      encode_stat (&v->item[0], flags, p, &st);
      return v;
    }
#ifdef HAVE_DIRENT_H
  if ((dirp = opendir (temppath)) == 0)
    return 0;
#elif _WIN32
  snprintf(searchPath, sizeof(searchPath), "%s\\*", temppath);
  dirp = FindFirstFile(searchPath, &findFileData);
  if (dirp == INVALID_HANDLE_VALUE)
    return 0;
#endif

  /*
   * Count files
   */
#ifdef HAVE_DIRENT_H
  for (de = readdir (dirp); de; de = readdir (dirp))
    {
      namelen = strlen (de->d_name);
      if (!do_match && (strcmp (de->d_name, ".") == 0 ||
                        strcmp (de->d_name, "..") == 0))
        continue;
      if (do_match && !match_string (regexppath, de->d_name))
        continue;
      count++;
      if (count >= CONFIG_INT (__MAX_ARRAY_SIZE__))
        break;
    }
#elif _WIN32
  do
    {
      namelen = strlen (findFileData.cFileName);
      if (!do_match && (strcmp (findFileData.cFileName, ".") == 0 ||
                        strcmp (findFileData.cFileName, "..") == 0))
        continue;
      if (do_match && !match_string (regexppath, findFileData.cFileName))
        continue;
      count++;
      if (count >= CONFIG_INT (__MAX_ARRAY_SIZE__))
        break;
    } while (FindNextFile(dirp, &findFileData) != 0);
#endif

  /*
   * Make array and put files on it.
   */
  v = allocate_empty_array (count);
  if (count == 0)
    {
      /* This is the easy case :-) */
#ifdef HAVE_DIRENT_H
      closedir (dirp);
#elif _WIN32
      FindClose(dirp);
#endif
      return v;
    }

#ifdef HAVE_DIRENT_H
  rewinddir (dirp);
#elif _WIN32
  FindClose(dirp);
  dirp = FindFirstFile(searchPath, &findFileData);
#endif
  endtemp = temppath + strlen (temppath);
  strcat (endtemp++, "/");

#ifdef HAVE_DIRENT_H
  for (i = 0, de = readdir (dirp); i < count; de = readdir (dirp))
    {
      namelen = strlen (de->d_name);
      if (!do_match && (strcmp (de->d_name, ".") == 0 ||
                        strcmp (de->d_name, "..") == 0))
        continue;
      if (do_match && !match_string (regexppath, de->d_name))
        continue;
      de->d_name[namelen] = '\0';
      if (flags == -1)
        {
          /*
           * We'll have to .... sigh.... stat() the file to get some add'tl
           * info.
           */
          strcpy (endtemp, de->d_name);
          stat (temppath, &st);	/* We assume it works. */
        }
      encode_stat (&v->item[i], flags, de->d_name, &st);
      i++;
    }
  closedir (dirp);
#elif _WIN32
  if (FindFirstFile(searchPath, &findFileData) != INVALID_HANDLE_VALUE)
    {
      for (i = 0; i < count; )
        {
          namelen = strlen (findFileData.cFileName);
          if (!do_match && (strcmp (findFileData.cFileName, ".") == 0 ||
                            strcmp (findFileData.cFileName, "..") == 0))
            {
              if (FindNextFile(dirp, &findFileData) == 0)
                break;
              continue;
            }
          if (do_match && !match_string (regexppath, findFileData.cFileName))
            {
              if (FindNextFile(dirp, &findFileData) == 0)
                break;
              continue;
            }
          findFileData.cFileName[namelen] = '\0';
          if (flags == -1)
            {
              /*
               * We'll have to .... sigh.... stat() the file to get some add'tl
               * info.
               */
              strcpy (endtemp, findFileData.cFileName);
              stat (temppath, &st);	/* We assume it works. */
            }
          encode_stat (&v->item[i], flags, findFileData.cFileName, &st);
          i++;
          if (FindNextFile(dirp, &findFileData) == 0)
            break;
        }
      FindClose(dirp);
    }
#endif

  /* Sort the names. */
  qsort ((void *) v->item, count, sizeof v->item[0], (flags == -1) ? parrcmp : pstrcmp);
  return v;
}

int
tail (char *path)
{
  char buff[1000];
  FILE *f;
  struct stat st;
  int offset;

  path = check_valid_path (path, current_object, "tail", 0);

  if (path == 0)
    return 0;
  f = fopen (path, "r");
  if (f == 0)
    return 0;
  if (fstat (fileno (f), &st) == -1)
    fatal ("Could not stat an open file.\n");
  offset = st.st_size - 54 * 20;
  if (offset < 0)
    offset = 0;
  if (fseek (f, offset, 0) == -1)
    fatal ("Could not seek.\n");
  if (offset > 0)
    {
      /* Throw away the first incomplete line. */
      if (NULL == fgets (buff, sizeof buff, f))
        {
          debug_perror ("fgets()", path);
          error ("Failed reading file.");
        }
    }
  while (fgets (buff, sizeof buff, f))
    {
      tell_object (command_giver, buff);
    }
  fclose (f);
  return 1;
}

int
remove_file (char *path)
{
  path = check_valid_path (path, current_object, "remove_file", 1);

  if (path == 0)
    return 0;
  if (unlink (path) == -1)
    return 0;
  return 1;
}

/*
 * Check that it is an legal path. No '..' are allowed.
 */
int legal_path (const char *path) {
  const char *p;

  if (path == NULL)
    return 0;
  if (path[0] == '/')
    return 0;
  /*
   * disallowing # seems the easiest way to solve a bug involving loading
   * files containing that character
   */
  if (strchr (path, '#'))
    {
      return 0;
    }
  p = path;
  while (p)
    {				/* Zak, 930530 - do better checking */
      if (p[0] == '.')
        {
          if (p[1] == '\0')	/* trailing `.' ok */
            break;
          if (p[1] == '.')	/* check for `..' or `../' */
            p++;
          if (p[1] == '/' || p[1] == '\0')
            return 0;		/* check for `./', `..', or `../' */
        }
      p = (char *) strstr (p, "/.");	/* search next component */
      if (p)
        p++;			/* step over `/' */
    }
#if defined(AMIGA) || defined(LATTICE) || defined(WIN32)
  /*
   * I don't know what the proper define should be, just leaving an
   * appropriate place for the right stuff to happen here - Wayfarer
   */
  /*
   * fail if there's a ':' since on AmigaDOS this means it's a logical
   * device!
   */
  /* Could be a drive thingy for os2. */
  if (strchr (path, ':'))
    return 0;
#endif
  return 1;
}				/* legal_path() */

/*
 * There is an error in a specific file. Ask the master object to log the
 * message somewhere.
 */
void
smart_log (char *error_file, int line, char *what, int flag)
{
  char *buff;
  svalue_t *mret;
  extern int pragmas;

  buff = (char *) DMALLOC (strlen (error_file) + strlen (what) +
             ((pragmas & PRAGMA_ERROR_CONTEXT) ? 100 : 40), TAG_TEMPORARY,
             "smart_log: 1");

  if (flag)
    sprintf (buff, "%s line %d: Warning: %s", error_file, line, what);
  else
    sprintf (buff, "%s line %d: %s", error_file, line, what);

  if (pragmas & PRAGMA_ERROR_CONTEXT)
    {
      char *ls = strrchr (buff, '\n');
      char *tmp;
      if (ls)
        {
          tmp = ls + 1;
          while (*tmp && isspace (*tmp))
            tmp++;
          if (!*tmp)
            *ls = 0;
        }
      strcat (buff, show_error_context ());
    }
  else
    strcat (buff, "\n");

  if (flag)
    sprintf (buff, "%s line %d: Warning: %s%s", error_file, line, what,
             (pragmas & PRAGMA_ERROR_CONTEXT) ? show_error_context () : "\n");
  else
    sprintf (buff, "%s line %d: %s%s", error_file, line, what,
             (pragmas & PRAGMA_ERROR_CONTEXT) ? show_error_context () : "\n");

  share_and_push_string (error_file);
  copy_and_push_string (buff);
  mret = safe_apply_master_ob (APPLY_LOG_ERROR, 2);
  if (!mret || mret == (svalue_t *) - 1)
    {
      log_message (NULL, "\t%s", buff);
    }
  FREE (buff);
}				/* smart_log() */

/**
 *  @brief Append string to file.
 *  @param file The file to write to.
 *  @param str The string to write.
 *  @param flags If 1, overwrite the file instead of appending.
 *               If 0, append to the file.
 *               Other bits are reserved for future use.
 *  @returns Return 0 for failure, otherwise 1.
 *  @see docs/efuns/write_file.md
 */
int write_file (char *file, char *str, int flags)
{
  FILE *f;
  size_t n_written;

  file = check_valid_path (file, current_object, "write_file", 1);
  if (!file)
    return 0;
  f = fopen (file, (flags & 1) ? "w" : "a");
  if (f == 0)
    {
      debug_perror ("fopen()", file);
      /* error ("Wrong permissions for opening file /%s for %s.\n\"%s\"\n", file, (flags & 1) ? "overwrite" : "append", strerror (errno)); */
      return 0;
    }
  n_written = fwrite (str, strlen (str), 1, f);
  fclose (f);
  return n_written == 1;
}				/* write_file() */

/**
 *  @brief Reads a text file into a string.
 *  @param file The file to read.
 *  @param start The line number to start reading from (1-based). If < 1, start from line 1.
 *  @param len The number of lines to read. If < 1, read the entire file.
 */
char *read_file (const char *path, int start, int len)
{
  char path_copy[PATH_MAX];
  char* file;
  struct stat st;
  int fd;
  FILE *f;
  char *str, *end;
  register char *p, *p2;
  int size;
  size_t n_read = 0;

  if (len < 0)
    return 0;
  strncpy (path_copy, path, PATH_MAX - 1);
  path_copy[PATH_MAX - 1] = '\0';
  file = check_valid_path (path_copy, current_object, "read_file", 0);

  if (!file)
    return 0;

#ifdef _WIN32
  fd = open (file, O_RDONLY | O_TEXT);
#else
  fd = open (file, O_RDONLY);
#endif
  if (-1 == fd)
    return 0;
  /*
   * file doesn't exist, or is really a directory
   */
  if (fstat (fd, &st) == -1 || (st.st_mode & S_IFDIR)) {
    close (fd);
    return 0;
  }

  f = fdopen (fd, "r");
  if (!f) {
    close (fd);
    return 0;
  }

  /* read entire file if actual file size <= MAX_READ_FILE_SIZE */
  size = st.st_size;
  if (size > CONFIG_INT (__MAX_READ_FILE_SIZE__))
    {
      if (start || len)
        size = CONFIG_INT (__MAX_READ_FILE_SIZE__); /* read fixed-size block for skip lines below */
      else
        {
          fclose (f);
          return 0; /* file too large */
        }
    }
  if (start < 1)
    start = 1;
  if (len < 1)
    len = CONFIG_INT (__MAX_READ_FILE_SIZE__); /* max number of lines possible in max read size */

  opt_trace (TT_EVAL|0, "actual size: %ld bytes, requested: (start line %d) %d lines or < %d bytes\n",
    st.st_size, start, (len ? len : CONFIG_INT (__MAX_READ_FILE_SIZE__)), size);

  if (!size)
    {
      /* zero length file */
      fclose (f);
      return 0;
    }

  /* allocate a buffer for file contents (we don't know the actual size of buffer since the read_file()
   * counts lines). we'll shrink it later if necessary.
   */
  str = new_string (size, "read_file: str");
  str[size] = '\0';
  do
    {
      /* 1. fill buffer
       *
       * [NEOLITH-EXTENSION] In Windows, text mode translates \r\n to \n on reading, so the number of bytes read
       * may be less than requested size. We cannot rely on st.st_size after fread(). The actual size of ingested
       * data is determined by the actual number of bytes returned by fread().
       */
      if (!size || (n_read = fread (str, 1, size, f)) == 0)
        {
          if (ferror (f))
            {
              debug_perror ("fread()", file);
              error ("Failed reading file.");
            }
          opt_trace (TT_EVAL|0, "fread(): EOF before start line");
          fclose (f);
          FREE_MSTR (str);
          return 0;
        }

      /* 2. try skip to the start line */
      end = str + n_read;
      *end = '\0';
      for (p = str; --start && (p2 = (char *) memchr (p, '\n', end - p));)
        {
          p = p2 + 1; /* next line */
        }
    }
  while (start > 1);

  /* now `p` points to the start of desired file contents, and `end` points to the
   * end of read buffer. Check if we need to read more for desired number of lines.
   */
  if (len < CONFIG_INT (__MAX_READ_FILE_SIZE__) && !feof (f))
    {
      /* move `len` lines of text starting from `p` to the beginning of the buffer */
      for (p2 = str; p != end;)
        {
          char c;

          c = *p++;
          *p2++ = c;
          if (c == '\n')
            {
              if (!--len)
                break;
            }
          else if (c == '\0') /* NUL character is not allowed in LPC string */
            {
              fclose (f);
              FREE_MSTR (str);
              error ("Attempted to read '\\0' into a string!\n");
            }
        }
      /* [NEOLITH-EXTENSION] Read more data if necessary and does not rely on st.st_size anymore.
       * This is important in Windows text mode that translates \r\n to \n on reading. The actual
       * size of ingested data is determined by the actual number of bytes returned by fread().
       */
      while (len && !feof (f)) /* `len` == remaining lines of text to read */
        {
          size -= (p2 - str); /* `size` == remaining buffer size, in bytes */
          if (!size)
            break;

          if (!size || (n_read = fread (p2, 1, size, f)) == 0)
            {
              fclose (f);
              FREE_MSTR (str);
              opt_trace (TT_EVAL|0, "fread() failed or EOF before reading all requested lines\n");
              return 0;
            }
          end = p2 + n_read; /* n_read could be different from requested size on Windows due to CR LF translation */
          *end = '\0';
          for (; p2 != end;)
            {
              if (*p2 == '\0')
                {
                  fclose (f);
                  FREE_MSTR (str);
                  error ("Attempted to read '\\0' into a string!\n");
                }
              if (*p2++ == '\n')
                if (!--len)
                  break; /* done reading requested lines */
            }
        }

      if (len && !feof (f))
        {
          /* size of requested lines exceeds __MAX_READ_FILE_SIZE__ */
          fclose (f);
          FREE_MSTR (str);
          opt_trace (TT_EVAL|0, "requested lines exceed max read file size\n");
          return 0;
        }
      *p2 = '\0';
      str = extend_string (str, p2 - str); /* shrink buffer to actual size */
    }

  fclose (f);
  return str;
}				/* read_file() */

char *
read_bytes (char *file, int start, int len, int *rlen)
{
  struct stat st;
  FILE *fp;
  char *str;
  int size;

  if (len < 0)
    return 0;
  file = check_valid_path (file, current_object, "read_bytes", 0);
  if (!file)
    return 0;
  fp = fopen (file, "rb");
  if (fp == NULL)
    return 0;
  if (fstat (fileno (fp), &st) == -1)
    fatal ("Could not stat an open file.\n");
  size = st.st_size;
  if (start < 0)
    start = size + start;

  if (len == 0)
    len = size;
  if (len > CONFIG_INT (__MAX_BYTE_TRANSFER__))
    {
      error ("Transfer exceeded maximum allowed number of bytes.\n");
      return 0;
    }
  if (start >= size)
    {
      fclose (fp);
      return 0;
    }
  if ((start + len) > size)
    len = (size - start);

  if ((size = fseek (fp, start, 0)) < 0)
    return 0;

  str = new_string (len, "read_bytes: str");

  size = fread (str, 1, len, fp);

  fclose (fp);

  if (size <= 0)
    {
      FREE_MSTR (str);
      return 0;
    }
  /*
   * The string has to end to '\0'!!!
   */
  str[size] = '\0';

  *rlen = size;
  return str;
}

int
write_bytes (char *file, int start, char *str, int theLength)
{
  struct stat st;
  size_t size;
  int fd;
  FILE *fp;

  file = check_valid_path (file, current_object, "write_bytes", 1);

  if (!file)
    return 0;
  if (theLength > CONFIG_INT (__MAX_BYTE_TRANSFER__))
    return 0;

  fd = open (file, O_CREAT | O_RDWR
#ifndef _WIN32
    , S_IRUSR | S_IWUSR
#endif
  ); /* create the file if it does not exist, do not truncate */
  if (-1 == fd)
    return 0;

  fp = fdopen (fd, "r+");
  if (!fp) {
    close (fd);
    return 0;
  }

  if (fstat (fd, &st) == -1)
    fatal ("Could not stat an open file.\n");
  size = st.st_size;
  if (start < 0) /* negative start position means offset from end-of-file */
    start = size + start;
  if (start < 0 || start > (int)size)
    {
      fclose (fp);
      return 0;
    }
  if ((size = fseek (fp, start, 0)) != 0)
    {
      fclose (fp);
      return 0;
    }
  size = fwrite (str, 1, theLength, fp);

  fclose (fp);

  if (size <= 0)
    {
      return 0;
    }
  return 1;
}

int
file_size (char *file)
{
  struct stat st;
  int ret;

  file = check_valid_path (file, current_object, "file_size", 0);
  if (!file)
    return -1;

  if (stat (file, &st) == -1)
    ret = -1;
  else if (S_IFDIR & st.st_mode)
    ret = -2;
  else
    ret = st.st_size;

  return ret;
}


/**
 *  @brief Check that a path to a file is valid for read or write.
 *  This is done by functions in the master object.
 *  The path is always treated as an absolute path, and is returned without a leading '/'.
 *  If the path was '/', then '.' is returned.
 *  Otherwise, the returned path is temporarily allocated by apply(), which means it will be deallocated at next apply().
 */
char *check_valid_path (char *path, object_t * call_object, const char *call_fun, int writeflg) {

  static char current_dir[] = ".";
  svalue_t *v;

  if (call_object == 0 || call_object->flags & O_DESTRUCTED)
    return 0;

  copy_and_push_string (path);
  push_object (call_object);
  push_constant_string (call_fun);
  if (writeflg)
    v = apply_master_ob (APPLY_VALID_WRITE, 3);
  else
    v = apply_master_ob (APPLY_VALID_READ, 3);

  if (v == (svalue_t *) - 1)
    {
      opt_trace(TT_EVAL|1, "master object not loaded yet");
      v = 0;
    }

  if (v)
    {
      if (v->type == T_NUMBER && v->u.number == 0)
        return 0;
      if (v->type == T_STRING)
        {
          path = v->u.string;
        }
      else
        {
          free_svalue (&apply_ret_value, "check_valid_path");
          apply_ret_value.type = T_STRING;
          apply_ret_value.subtype = STRING_MALLOC;
          path = apply_ret_value.u.string = string_copy (path, "check_valid_path");
        }
    }

  if (path[0] == '/')
    path++;
  if (path[0] == '\0')
    path = current_dir;
  if (legal_path (path))
    {
      opt_trace(TT_EVAL, "legal path: %s", path);
      return path;
    }

  opt_trace(TT_EVAL, "not legal path: %s", path);
  return 0;
}

static int
match_string (char *match, char *str)
{
  int i;

again:
  if (*str == '\0' && *match == '\0')
    return 1;
  switch (*match)
    {
    case '?':
      if (*str == '\0')
        return 0;
      str++;
      match++;
      goto again;
    case '*':
      match++;
      if (*match == '\0')
        return 1;
      for (i = 0; str[i] != '\0'; i++)
        if (match_string (match, str + i))
          return 1;
      return 0;
    case '\0':
      return 0;
    case '\\':
      match++;
      if (*match == '\0')
        return 0;
      /* Fall through ! */
    default:
      if (*match == *str)
        {
          match++;
          str++;
          goto again;
        }
      return 0;
    }
}

/*
 * Credits for some of the code below goes to Free Software Foundation
 * Copyright (C) 1990 Free Software Foundation, Inc.
 * See the GNU General Public License for more details.
 */
#ifndef S_ISDIR
#define	S_ISDIR(m)	(((m)&S_IFMT) == S_IFDIR)
#endif

#ifndef S_ISREG
#define	S_ISREG(m)	(((m)&S_IFMT) == S_IFREG)
#endif

#ifndef S_ISCHR
#define	S_ISCHR(m)	(((m)&S_IFMT) == S_IFCHR)
#endif

#ifndef S_ISBLK
#define	S_ISBLK(m)	(((m)&S_IFMT) == S_IFBLK)
#endif

/* Copy file <from> to <to>
 * Return 0 if success, or return non-zero if fails.
 * */
static int
copy (char *from, char *to)
{
  int ifd;
  int ofd;
  char buf[16384];
  int len;			/* Number of bytes read into `buf'. */
  struct stat from_stats;

  ifd = open (from, O_RDONLY);
  if (ifd == -1)
    return -1;

  if (-1 == fstat (ifd, &from_stats)) {
    close (ifd);
    return -1;
  }
  if (!S_ISREG (from_stats.st_mode)) /* is regular file ? */
    {
      error ("not a regular file: /%", from);
      return -1;
    }

  ofd = open (to, O_WRONLY | O_CREAT | O_TRUNC
#ifndef _WIN32
    , S_IRUSR | S_IWUSR
#endif
  );
  if (ofd < 0)
    {
      close (ifd);
      return -1;
    }

#ifdef HAS_FCHMOD
  /* set <to> file as the same mode as <from> file */
  if (fchmod (ofd, from_stats.st_mode & 0777))
    {
      close (ifd);
      close (ofd);
      unlink (to);
      return -1;
    }
#endif

  while ((len = read (ifd, buf, sizeof (buf))) > 0)
    {
      int wrote = 0;
      char *bp = buf;

      do
        {
          wrote = write (ofd, bp, len);
          if (wrote < 0)
            {
              close (ifd);
              close (ofd);
              unlink (to);
              return -1;
            }
          bp += wrote;
          len -= wrote;
        }
      while (len > 0);
    }
  if (len < 0)
    {
      close (ifd);
      close (ofd);
      unlink (to);
      return -1;
    }
  if (close (ifd) < 0)
    {
      close (ofd);
      return -1;
    }
  if (close (ofd) < 0)
    {
      return -1;
    }

  return 0;
}

/* Move FROM onto TO.  Handles cross-filesystem moves.
   If TO is a directory, FROM must be also.
   Return 0 if successful, 1 if an error occurred.  */

#ifdef F_RENAME
static int
do_move (char *from, char *to, int flag)
{
  if (flag == F_RENAME)
    {
      if (0 == rename (from, to))
        return 0;

      if (errno != EXDEV)
        {
          error ("cannot move `/%s' to `/%s'\n", from, to);
          return 1;
        }

      /* rename failed on cross-filesystem link.  Copy the file instead. */
      if ((0 == copy (from, to)) && (0 == unlink (from)))
        return 0;

      error ("cannot copy `/%s' to `/%s'", from, to);
      return 1;
    }
#ifdef F_LINK
  else if (flag == F_LINK)
    {
      if (symlink (from, to) == 0)	/* symbolic link */
        return 0;
      error ("cannot link `/%s' to `/%s'", from, to);
      return 1;
    }
#endif

  error ("invalid flag: %d", flag);
  return 1; /* invalid flag */
}
#endif /* F_RENAME */

/*
 * do_rename is used by the efun rename. It is basically a combination
 * of the unix system call rename and the unix command mv.
 */

#ifdef F_RENAME
int
do_rename (char *fr, char *t, int flag)
{
  char *from, *to, tbuf[3];
  char newfrom[MAX_FNAME_SIZE + MAX_PATH_LEN + 2];
  int flen;
  static svalue_t from_sv = { .type = T_NUMBER };
  static svalue_t to_sv = { .type = T_NUMBER };
  extern svalue_t apply_ret_value;

  /*
   * important that the same write access checks are done for link() as are
   * done for rename().  Otherwise all kinds of security problems would
   * arise (e.g. creating links to files in protected directories and then
   * modifying the protected file by modifying the linked file). The idea
   * is prevent linking to a file unless the person doing the linking has
   * permission to move the file.
   */
  from = check_valid_path (fr, current_object, "rename", 1);
  if (!from)
    return 1;

  assign_svalue (&from_sv, &apply_ret_value);

  to = check_valid_path (t, current_object, "rename", 1);
  if (!to)
    return 1;

  assign_svalue (&to_sv, &apply_ret_value);
  if (!strlen (to) && !strcmp (t, "/"))
    {
      to = tbuf;
      sprintf (to, "./");
    }

  /* Strip trailing slashes */
  flen = strlen (from);
  if (flen > 1 && from[flen - 1] == '/')
    {
      char *p = from + flen - 2;
      int n;

      while (*p == '/' && (p > from))
        p--;
      n = p - from + 1;
      memcpy (newfrom, from, n);
      newfrom[n] = 0;
      from = newfrom;
    }

  if (file_size (to) == -2)
    {
      /* Target is a directory; build full target filename. */
      char *cp;
      char newto[MAX_FNAME_SIZE + MAX_PATH_LEN + 2];

      cp = strrchr (from, '/');
      if (cp)
        cp++;
      else
        cp = from;

      if (snprintf (newto, sizeof(newto), "%s/%s", to, cp) >= (int)sizeof(newto))
        error("File path too long.");

      return do_move (from, newto, flag);
    }
  else
    return do_move (from, to, flag);
}
#endif /* F_RENAME */

int
copy_file (char *from, char *to)
{
  struct stat st;
  char buf[32768];
  int from_fd, to_fd;
  int num_read, num_written;
  char *write_ptr;
  static svalue_t from_sv = { .type = T_NUMBER };
  static svalue_t to_sv = { .type = T_NUMBER };
  extern svalue_t apply_ret_value;

  from = check_valid_path (from, current_object, "cp", 0);
  if (from == 0)
    return -1;
  assign_svalue (&from_sv, &apply_ret_value);

  to = check_valid_path (to, current_object, "cp", 1);
  if (to == 0)
    return -2;
  assign_svalue (&to_sv, &apply_ret_value);

  from_fd = open (from, O_RDONLY);
  if (from_fd < 0)
    {
      debug_perror ("copy_file: open", from);
      return (-1);
    }

  char newto[MAX_FNAME_SIZE + MAX_PATH_LEN + 2];
  if (0 == stat (to, &st) && (S_IFDIR & st.st_mode))
    {
      char *cp;

      cp = strrchr (from, '/');
      if (cp)
        cp++;
      else
        cp = from;

      sprintf (newto, "%s/%s", to, cp);
      to = newto;
    }

  to_fd = open (to, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (to_fd < 0)
    {
      debug_perror ("copy_file: open", to);
      close (from_fd);
      return (-2);
    }

  while ((num_read = read (from_fd, buf, sizeof (buf))) != 0)
    {
      if (num_read < 0)
        {
          debug_perror ("copy_file: read", from);
          close (from_fd);
          close (to_fd);
          return (-3);
        }

      write_ptr = buf;
      while (num_read)
        {
          num_written = write (to_fd, write_ptr, num_read);
          if (num_written < 0)
            {
              debug_perror ("copy_file: write", to);
              close (from_fd);
              close (to_fd);
              return (-3);
            }
          write_ptr += num_written;
          num_read -= num_written;
        }
    }

  close (from_fd);
  close (to_fd);

  return 1;			/* success */
}

void
dump_file_descriptors (outbuffer_t * out)
{
  int i;
  dev_t dev;
  struct stat stbuf;

  outbuf_add (out, "Fd  Device Number  Inode   Mode    Uid    Gid      Size\n");
  outbuf_add (out, "--  -------------  -----  ------  -----  -----  ----------\n");

#ifndef _WIN32
  for (i = 0; i < FD_SETSIZE; i++)
    {
      /* bug in NeXT OS 2.1, st_mode == 0 for sockets */
      if (fstat (i, &stbuf) == -1)
        continue;

      if (S_ISCHR (stbuf.st_mode) || S_ISBLK (stbuf.st_mode))
        dev = stbuf.st_rdev;
      else
        dev = stbuf.st_dev;

      outbuf_addv (out, "%2d", i);
      outbuf_addv (out, "%13lx", dev);
      outbuf_addv (out, "%9lu", stbuf.st_ino);
      outbuf_add (out, "  ");

      switch (stbuf.st_mode & S_IFMT)
        {

        case S_IFDIR:
          outbuf_add (out, "d");
          break;
        case S_IFCHR:
          outbuf_add (out, "c");
          break;
#ifdef S_IFBLK
        case S_IFBLK:
          outbuf_add (out, "b");
          break;
#endif
        case S_IFREG:
          outbuf_add (out, "f");
          break;
#ifdef S_IFIFO
        case S_IFIFO:
          outbuf_add (out, "p");
          break;
#endif
#ifdef S_IFLNK
        case S_IFLNK:
          outbuf_add (out, "l");
          break;
#endif
#ifdef S_IFSOCK
        case S_IFSOCK:
          outbuf_add (out, "s");
          break;
#endif
        default:
          outbuf_add (out, "?");
          break;
        }

      outbuf_addv (out, "%5o", stbuf.st_mode & ~S_IFMT);
      outbuf_addv (out, "%7d", stbuf.st_uid);
      outbuf_addv (out, "%7d", stbuf.st_gid);
      outbuf_addv (out, "%12lu", stbuf.st_size);
      outbuf_add (out, "\n");
    }
#endif
}
