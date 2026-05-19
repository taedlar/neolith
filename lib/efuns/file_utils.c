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
#include "misc/filepath.h"
#include "src/main.h"

#include "file_utils.h"

/* see binaries.c.  We don't want no $@$(*)@# system dependent mess of includes */

extern int sys_nerr;

static int match_string (const char *, const char *);
static int copy (const char *from, const char *to);
static int do_move (const char *from, const char *to, int flag);
static int pstrcmp (const void *, const void *);
static int parrcmp (const void *, const void *);
static void encode_stat (svalue_t *, int, const char *, struct stat *);

#define MAX_LINES 50

/*
 * These are used by qsort in get_dir().
 */
static int
pstrcmp (const void *p1, const void *p2)
{
  svalue_t *x = (svalue_t *) p1;
  svalue_t *y = (svalue_t *) p2;

  return strcmp (SVALUE_STRPTR(x), SVALUE_STRPTR(y));
}

static int
parrcmp (const void *p1, const void *p2)
{
  svalue_t *x = (svalue_t *) p1;
  svalue_t *y = (svalue_t *) p2;

  return strcmp (SVALUE_STRPTR(&x->u.arr->item[0]), SVALUE_STRPTR(&y->u.arr->item[0]));
}

static void
encode_stat (svalue_t * vp, int flags, const char *str, struct stat *st)
{
  if (flags == -1)
    {
      array_t *v = allocate_empty_array (3);

      SET_SVALUE_MALLOC_STRING(&v->item[0], string_copy (str, "encode_stat"));
      v->item[1].type = T_NUMBER;
      v->item[1].u.number = ((st->st_mode & S_IFDIR) ? -2 : st->st_size);
      v->item[2].type = T_NUMBER;
      v->item[2].u.number = st->st_mtime;
      vp->type = T_ARRAY;
      vp->u.arr = v;
    }
  else
    {
      SET_SVALUE_MALLOC_STRING(vp, string_copy (str, "encode_stat"));
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

array_t* get_dir (const char *path, int flags) {
  array_t *v;
  int i, count = 0;
#ifdef HAVE_DIRENT_H
  DIR *dirp;
  struct dirent *de;
#elif _WIN32
  HANDLE dirp;
  WIN32_FIND_DATA findFileData;
  char searchPath[PATH_MAX + 2];
#endif
  size_t namelen;
  int do_match = 0;

  struct stat st;
  char *endtemp;
  char temppath[PATH_MAX + 2];
  char fs_path[PATH_MAX + 2];
  char regexppath[PATH_MAX + 2];
  char *p;
  const char *mudlib_dir;
  char *resolved_path;

  if (!path)
    return 0;

  /* MudOS calls master apply with "stat" as caller here. We'll retain compatibility here */
  if (!push_valid_path (path, current_object, "stat", 0))
    return 0;

  path = SVALUE_STRPTR (sp);
  strncpy (temppath, path, sizeof (temppath) - 1);
  temppath[sizeof (temppath) - 1] = '\0';
  pop_stack ();

  mudlib_dir = MAIN_OPTION (mudlib_dir_absolute);
  if (!mudlib_dir || !*mudlib_dir)
    return 0;

  resolved_path = resolve_path_in_mudlib (temppath, mudlib_dir);
  if (!resolved_path)
    return 0;

  strncpy (fs_path, resolved_path, sizeof (fs_path) - 1);
  fs_path[sizeof (fs_path) - 1] = '\0';
  FREE_MSTR (resolved_path);

  if (strlen (path) < 2)
    {
      temppath[0] = path[0] ? path[0] : '.';
      temppath[1] = '\000';
      p = temppath;
    }
  else
    {
      /*
       * If path ends with '/' or "/." remove it
       */
      if ((p = strrchr (temppath, '/')) == 0)
        p = temppath;
      if (p[0] == '/' && ((p[1] == '.' && p[2] == '\0') || p[1] == '\0'))
        *p = '\0';
    }

  if (stat (fs_path, &st) < 0)
    {
      if (*p == '\0')
        return 0;
      if (p != temppath)
        {
          strcpy (regexppath, p + 1);
        }
      else
        {
          strcpy (regexppath, p);
        }
      {
        char *fs_parent = strrchr (fs_path, '/');

        if (fs_parent)
          {
            if (fs_parent == fs_path)
              fs_path[1] = '\0';
            else
              *fs_parent = '\0';
          }
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
  if ((dirp = opendir (fs_path)) == 0)
    return 0;
#elif _WIN32
  snprintf(searchPath, sizeof(searchPath), "%s\\*", fs_path);
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
  endtemp = fs_path + strlen (fs_path);
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
              stat (fs_path, &st); /* We assume it works. */
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
              stat (fs_path, &st); /* We assume it works. */
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
tail (const char *path)
{
  char buff[1000];
  FILE *f;
  struct stat st;
  int offset;

  if (!push_resolved_valid_path (path, current_object, "tail", 0))
    return 0;
  path = SVALUE_STRPTR (sp);
  f = fopen (path, "r");
  pop_stack (); /* done with path */
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
          debug_perror ("fgets()", "(tail file)");
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
remove_file (const char *path)
{
  if (!push_resolved_valid_path (path, current_object, "remove_file", 1))
    return 0;
  int rc = (unlink (SVALUE_STRPTR (sp)) != -1);
  pop_stack ();
  return rc;
}

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
int write_file (const char *file, const char *str, int flags)
{
  FILE *f;
  size_t n_written;

  if (!push_resolved_valid_path (file, current_object, "write_file", 1))
    return 0;
  f = fopen (SVALUE_STRPTR (sp), (flags & 1) ? "w" : "a");
  pop_stack ();
  if (f == 0)
    {
      debug_perror ("fopen()", file);
      /* error ("Wrong permissions for opening file /%s for %s.\n\"%s\"\n", file, (flags & 1) ? "overwrite" : "append", strerror (errno)); */
      return 0;
    }
  n_written = fwrite (str, strlen (str), 1, f);
  fclose (f);
  return (n_written == 1);
}				/* write_file() */

/**
 *  @brief Reads a text file into a string.
 *  @param file The file to read.
 *  @param start The line number to start reading from (1-based). If < 1, start from line 1.
 *  @param len The number of lines to read. If < 1, read the entire file.
 */
char *read_file (const char *path, long start, size_t len) {
  char path_copy[PATH_MAX];
  struct stat st;
  int fd;
  FILE *f;
  char *str, *end;
  register char *p, *p2;
  size_t size;
  size_t n_read = 0;

  strncpy (path_copy, path, PATH_MAX - 1);
  path_copy[PATH_MAX - 1] = '\0';
  if (!push_resolved_valid_path (path_copy, current_object, "read_file", 0))
    return 0;

#ifdef _WIN32
  fd = open (SVALUE_STRPTR (sp), O_RDONLY | O_TEXT);
#else
  fd = open (SVALUE_STRPTR (sp), O_RDONLY);
#endif
  strncpy (path_copy, SVALUE_STRPTR (sp), PATH_MAX - 1);
  path_copy[PATH_MAX - 1] = '\0';
  pop_stack ();
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
  if (size > (size_t)CONFIG_INT (__MAX_READ_FILE_SIZE__))
    {
      if (start || len)
        size = (size_t)CONFIG_INT (__MAX_READ_FILE_SIZE__); /* read fixed-size block for skip lines below */
      else
        {
          fclose (f);
          return 0; /* file too large */
        }
    }
  if (start < 1)
    start = 1;
  if (len < 1)
    len = (size_t)CONFIG_INT (__MAX_READ_FILE_SIZE__); /* max number of lines possible in max read size */

  opt_trace (TT_EVAL|0, "actual size: %ld bytes, requested: (start line %d) %d lines or < %d bytes\n",
    st.st_size, start, (len ? len : (size_t)CONFIG_INT (__MAX_READ_FILE_SIZE__)), size);

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
              debug_perror ("fread()", path_copy);
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
  if (len < (size_t)CONFIG_INT (__MAX_READ_FILE_SIZE__) && !feof (f))
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

char* read_bytes (const char *file, long start, size_t len, size_t *rlen) {
  struct stat st;
  FILE *f;
  char *str;
  size_t size;

  if (!push_resolved_valid_path (file, current_object, "read_bytes", 0))
    return 0;
  f = fopen (SVALUE_STRPTR (sp), "rb");
  pop_stack ();
  if (f == NULL)
    return 0;
  if (fstat (fileno (f), &st) == -1)
    fatal ("Could not stat an open file.\n");
  size = st.st_size;
  if (start < 0)
    start = (long)size + start;

  if (len == 0)
    len = size;
  if (len > (size_t)CONFIG_INT (__MAX_BYTE_TRANSFER__))
    {
      error ("Transfer exceeded maximum allowed number of bytes.\n");
      return 0;
    }
  if ((size_t)start >= size)
    {
      fclose (f);
      return 0;
    }
  if ((size_t)start + len > size)
    len = size - start;

  if (fseek (f, start, 0) < 0)
    return 0;

  str = new_string (len, "read_bytes: str");

  size = fread (str, 1, len, f);

  fclose (f);

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
write_bytes (const char *file, long start, const char *str, size_t theLength)
{
  struct stat st;
  size_t size;
  int fd;
  FILE *f;

  if (!push_resolved_valid_path (file, current_object, "write_bytes", 1))
    return 0;
  if (theLength > (size_t)CONFIG_INT (__MAX_BYTE_TRANSFER__))
    {
      pop_stack ();
      return 0;
    }

  fd = open (SVALUE_STRPTR (sp), O_CREAT | O_RDWR
#ifndef _WIN32
    , S_IRUSR | S_IWUSR
#endif
  ); /* create the file if it does not exist, do not truncate */
  if (-1 == fd)
    {
      pop_stack ();
      return 0;
    }

  pop_stack (); /* done with path; fd is open */
  f = fdopen (fd, "r+");
  if (!f) {
    close (fd);
    return 0;
  }

  if (fstat (fd, &st) == -1)
    fatal ("Could not stat an open file.\n");
  size = st.st_size;
  if (start < 0) /* negative start position means offset from end-of-file */
    start = (long)size + start;
  if (start < 0 || start > (int)size)
    {
      fclose (f);
      return 0;
    }
  if ((size = fseek (f, start, 0)) != 0)
    {
      fclose (f);
      return 0;
    }
  size = fwrite (str, 1, theLength, f);

  fclose (f);

  if (size <= 0)
    {
      return 0;
    }
  return 1;
}

int
file_size (const char *file)
{
  struct stat st;
  int ret;

  if (!push_resolved_valid_path (file, current_object, "file_size", 0))
    return -1;

  if (stat (SVALUE_STRPTR (sp), &st) == -1)
    ret = -1;
  else if (S_IFDIR & st.st_mode)
    ret = -2;
  else
    ret = st.st_size;
  pop_stack ();

  return ret;
}

static int
match_string (const char *match, const char *str)
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
copy (const char *from, const char *to)
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
static int do_move (const char *from, const char *to, int flag) {
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
int do_rename (const char *fr, const char *t, int flag) {
  const char *from, *to;
  char tbuf[3];
  char newfrom[PATH_MAX + 2];
  size_t flen;
  struct stat st;
  /*
   * Both paths are pushed onto the eval stack before any OS call.
   * This is re-entrant safe: each resolve call owns its own stack slot,
   * and the stack is unwound automatically on LPC error.
   */
  if (!push_resolved_valid_path (fr, current_object, "rename", 1))
    return 1;
  /* sp -> from path */

  if (!push_resolved_valid_path (t, current_object, "rename", 1))
    {
      pop_stack (); /* from */
      return 1;
    }
  /* sp-1 -> from path, sp -> to path */

  from = SVALUE_STRPTR (sp - 1);
  to = SVALUE_STRPTR (sp);
  if (!strlen (to) && !strcmp (t, "/"))
    {
      sprintf (tbuf, "./");
      to = tbuf;
    }

  /* Strip trailing slashes */
  flen = strlen (from);
  if (flen > 1 && from[flen - 1] == '/')
    {
      const char *p = from + flen - 2;
      ptrdiff_t n;

      while (*p == '/' && (p > from))
        p--;
      n = p - from + 1;
      memcpy (newfrom, from, n);
      newfrom[n] = 0;
      from = newfrom;
    }

  int result;
  if (0 == stat (to, &st) && (S_IFDIR & st.st_mode))
    {
      /* Target is a directory; build full target filename. */
      const char *cp;
      char newto[PATH_MAX + 2];

      cp = strrchr (from, '/');
      if (cp)
        cp++;
      else
        cp = from;

      if (snprintf (newto, sizeof(newto), "%s/%s", to, cp) >= (int)sizeof(newto))
        {
          pop_n_elems (2);
          error("File path too long.");
        }

      result = do_move (from, newto, flag);
    }
  else
    result = do_move (from, to, flag);

  pop_n_elems (2); /* from and to */
  return result;
}
#endif /* F_RENAME */

/**
 * @brief Copy a file from source path to destination path.
 *
 * Implements the `cp` efun contract.
 *
 * Both input paths are validated through master object permission checks and
 * resolved to sandboxed absolute paths before any filesystem operation.
 *
 * Destination behavior matches classic cp semantics used by this driver:
 * if @p to refers to an existing directory, the basename of @p from is
 * appended and the file is copied into that directory.
 *
 * @param from Source file path as passed from LPC.
 * @param to Destination file path as passed from LPC.
 * @return 1 on success.
 * @return -1 if source validation/open/read fails.
 * @return -2 if destination validation/open/path construction fails.
 * @return -3 if read/write during copy fails.
 */
int copy_file (const char *from, const char *to) {
  struct stat st;
  char buf[32768];
  int from_fd, to_fd;
  int num_read, num_written;
  char *write_ptr;
  char from_buf[PATH_MAX + 2];
  char to_buf[PATH_MAX + 2];

  if (!push_resolved_valid_path (from, current_object, "cp", 0))
    return -1;
  /* sp -> from path */

  if (!push_resolved_valid_path (to, current_object, "cp", 1))
    {
      pop_stack (); /* from */
      return -2;
    }
  /* sp-1 -> from path, sp -> to path */

  /* Copy to local buffers before popping; both may be needed through the OS calls. */
  strncpy (from_buf, SVALUE_STRPTR (sp - 1), sizeof (from_buf) - 1);
  from_buf[sizeof (from_buf) - 1] = '\0';
  strncpy (to_buf, SVALUE_STRPTR (sp), sizeof (to_buf) - 1);
  to_buf[sizeof (to_buf) - 1] = '\0';
  pop_n_elems (2);
  from = from_buf;
  to = to_buf;

  from_fd = open (from, O_RDONLY);
  if (from_fd < 0)
    {
      debug_perror ("open()", from);
      return (-1);
    }

  char newto[PATH_MAX + 2];
  if (0 == stat (to, &st) && (S_IFDIR & st.st_mode))
    {
      const char *cp;
      size_t to_len;
      size_t cp_len;

      cp = strrchr (from, '/');
      if (cp)
        cp++;
      else
        cp = from;

      to_len = strlen (to);
      cp_len = strlen (cp);
      if (to_len + 1 + cp_len >= sizeof (newto))
        {
          close (from_fd);
          return (-2);
        }

      memcpy (newto, to, to_len);
      newto[to_len] = '/';
      memcpy (newto + to_len + 1, cp, cp_len);
      newto[to_len + 1 + cp_len] = '\0';
      to = newto;
    }

  to_fd = open (to, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (to_fd < 0)
    {
      debug_perror ("open()", to);
      close (from_fd);
      return (-2);
    }

  while ((num_read = read (from_fd, buf, sizeof (buf))) != 0)
    {
      if (num_read < 0)
        {
          debug_perror ("read()", from);
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
              debug_perror ("write()", to);
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

void dump_file_descriptors (outbuffer_t * out) {
  (void) out;
#ifndef _WIN32
  int i;
  dev_t dev;
  struct stat stbuf;

  outbuf_add (out, "Fd  Device Number  Inode   Mode    Uid    Gid      Size\n");
  outbuf_add (out, "--  -------------  -----  ------  -----  -----  ----------\n");

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
