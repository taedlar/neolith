#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "efuns_opcode.h"
#include "src/std.h"
#include "src/applies.h"
#include "src/simulate.h"
#include "file_utils.h"
#include "src/interpret.h"
#include "src/stralloc.h"
#include "lpc/array.h"
#include "lpc/buffer.h"
#include "lpc/object.h"

#include <sys/stat.h>
#include <netinet/in.h>		/* for htonl() in write_bytes */

#include <fcntl.h>


#ifdef F_CP
void
f_cp (void)
{
  int i;

  i = copy_file (sp[-1].u.string, sp[0].u.string);
  free_string_svalue (sp--);
  free_string_svalue (sp);
  put_number (i);
}
#endif


#ifdef F_RENAME
void
f_rename (void)
{
  int i;

  i = do_rename ((sp - 1)->u.string, sp->u.string, F_RENAME);
  free_string_svalue (sp--);
  free_string_svalue (sp);
  put_number (i);
}
#endif /* F_RENAME */


#ifdef F_RM
void
f_rm (void)
{
  int i;

  i = remove_file (sp->u.string);
  free_string_svalue (sp);
  put_number (i);
}
#endif


#ifdef F_MKDIR
void
f_mkdir (void)
{
  char *path;

  path = check_valid_path (sp->u.string, current_object, "mkdir", 1);
  if (!path || mkdir (path, 0770) == -1)
    {
      free_string_svalue (sp);
      *sp = const0;
    }
  else
    {
      free_string_svalue (sp);
      *sp = const1;
    }
}
#endif


#ifdef F_RMDIR
void
f_rmdir (void)
{
  char *path;

  path = check_valid_path (sp->u.string, current_object, "rmdir", 1);
  if (!path || rmdir (path) == -1)
    {
      free_string_svalue (sp);
      *sp = const0;
    }
  else
    {
      free_string_svalue (sp);
      *sp = const1;
    }
}
#endif


#ifdef F_STAT
void
f_stat (void)
{
  struct stat buf;
  char *path;
  array_t *v;
  object_t *ob;

  path = check_valid_path ((--sp)->u.string, current_object, "stat", 0);
  if (!path)
    {
      free_string_svalue (sp);
      *sp = const0;
      return;
    }
  if (stat (path, &buf) != -1)
    {
      if (buf.st_mode & S_IFREG)
	{			/* if a regular file */
	  v = allocate_empty_array (3);
	  v->item[0].type = T_NUMBER;
	  v->item[0].u.number = buf.st_size;
	  v->item[1].type = T_NUMBER;
	  v->item[1].u.number = buf.st_mtime;
	  v->item[2].type = T_NUMBER;
	  ob = find_object2 (path);
	  if (ob && !object_visible (ob))
	    ob = 0;
	  if (ob)
	    v->item[2].u.number = ob->load_time;
	  else
	    v->item[2].u.number = 0;
	  free_string_svalue (sp);
	  put_array (v);
	  return;
	}
    }
  v = get_dir (sp->u.string, (sp + 1)->u.number);
  free_string_svalue (sp);
  if (v)
    {
      put_array (v);
    }
  else
    *sp = const0;
}
#endif


#ifdef F_READ_FILE
void
f_read_file (void)
{
  char *str;
  int start, len;

  if (st_num_arg == 3)
    {
      len = (sp--)->u.number;
    }
  else
    len = 0;
  if (st_num_arg > 1)
    start = (sp--)->u.number;
  else
    start = 0;

  str = read_file (sp->u.string, start, len);
  free_string_svalue (sp);
  if (!str)
    *sp = const0;
  else
    {
      sp->subtype = STRING_MALLOC;
      sp->u.string = str;
    }
}
#endif


#ifdef F_WRITE_FILE
void
f_write_file (void)
{
  int flags = 0;

  if (st_num_arg == 3)
    {
      flags = (sp--)->u.number;
    }
  flags = write_file ((sp - 1)->u.string, sp->u.string, flags);
  free_string_svalue (sp--);
  free_string_svalue (sp);
  put_number (flags);
}
#endif

#ifdef F_FILE_SIZE
void
f_file_size (void)
{
  int i = file_size (sp->u.string);
  free_string_svalue (sp);
  put_number (i);
}
#endif


#ifdef F_FILE_LENGTH
/*
 * file_length() efun, returns the number of lines in a file.
 * Returns -1 if no privs or file doesn't exist.
 */
int
file_length (char *file)
{
  struct stat st;
  int fd;
  FILE *f;
  int ret = 0;
  int num;
  char buf[2049];
  char *p, *newp;

  file = check_valid_path (file, current_object, "file_size", 0);

  if (!file)
    return -1;
  fd = open (file, O_RDONLY);
  if (fd == -1)
    return -1;

  if (fstat (fd, &st) == -1)
    return -1;
  if (st.st_mode & S_IFDIR) {
    close (fd);
    return -2;
  }

  f = fdopen (fd, "r");
  if (!f) {
    close (fd);
    return -1;
  }

  do
    {
      num = fread (buf, 1, 2048, f);
      p = buf - 1;
      while ((newp = memchr (p + 1, '\n', num)))
	{
	  num -= (newp - p);
	  p = newp;
	  ret++;
	}
    }
  while (!feof (f));

  fclose (f);
  return ret;
}				/* end of file_length() */

void
f_file_length (void)
{
  int l;

  l = file_length (sp->u.string);
  pop_stack ();
  push_number (l);
}
#endif


#ifdef F_GET_DIR
void
f_get_dir (void)
{
  array_t *vec;

  vec = get_dir ((sp - 1)->u.string, sp->u.number);
  free_string_svalue (--sp);
  if (vec)
    {
      put_array (vec);
    }
  else
    *sp = const0;
}
#endif


#ifdef F_LINK
void
f_link (void)
{
  svalue_t *ret;
  int i;

  push_svalue (sp - 1);
  push_svalue (sp);
  ret = apply_master_ob (APPLY_VALID_LINK, 2);
  if (MASTER_APPROVED (ret))
    i = do_rename ((sp - 1)->u.string, sp->u.string, F_LINK);
  else
    i = 0;
  (--sp)->type = T_NUMBER;
  sp->u.number = i;
  sp->subtype = 0;
}
#endif /* F_LINK */


#ifdef F_READ_BYTES
void
f_read_bytes (void)
{
  char *str;
  int start = 0, len = 0, rlen = 0, num_arg = st_num_arg;
  svalue_t *arg;

  arg = sp - num_arg + 1;
  if (num_arg > 1)
    start = arg[1].u.number;
  if (num_arg == 3)
    {
      len = arg[2].u.number;
    }
  str = read_bytes (arg[0].u.string, start, len, &rlen);
  pop_n_elems (num_arg);
  if (str == 0)
    push_number (0);
  else
    {
      push_malloced_string (str);
    }
}
#endif


#ifdef F_READ_BUFFER
void
f_read_buffer (void)
{
  char *str;
  int start = 0, len = 0, rlen = 0, num_arg = st_num_arg;
  int from_file = 0;		/* new line */
  svalue_t *arg = sp - num_arg + 1;

  if (num_arg > 1)
    {
      start = arg[1].u.number;
      if (num_arg == 3)
	{
	  len = arg[2].u.number;
	}
    }
  if (arg[0].type == T_STRING)
    {
      from_file = 1;		/* new line */
      str = read_bytes (arg[0].u.string, start, len, &rlen);
    }
  else
    {				/* T_BUFFER */
      str = read_buffer (arg[0].u.buf, start, len, &rlen);
    }
  pop_n_elems (num_arg);
  if (str == 0)
    {
      push_number (0);
    }
  else if (from_file)
    {				/* changed */
      buffer_t *buf;

      buf = allocate_buffer (rlen);
      memcpy (buf->item, str, rlen);
      (++sp)->u.buf = buf;
      sp->type = T_BUFFER;
      FREE_MSTR (str);
    }
  else
    {				/* T_BUFFER */
      push_malloced_string (str);
    }
}
#endif


#ifdef F_WRITE_BYTES
void
f_write_bytes (void)
{
  int i = 0;

  switch (sp->type)
    {
    case T_NUMBER:
      {
	int netint;
	char *netbuf;

	if (!sp->u.number)
	  bad_arg (3, F_WRITE_BYTES);
	netint = htonl (sp->u.number);	/* convert to network
					 * byte-order */
	netbuf = (char *) &netint;
	i = write_bytes ((sp - 2)->u.string, (sp - 1)->u.number, netbuf,
			 sizeof (int));
	break;
      }

    case T_BUFFER:
      i = write_bytes ((sp - 2)->u.string, (sp - 1)->u.number,
		       (char *) sp->u.buf->item, sp->u.buf->size);
      break;
    case T_STRING:
      i = write_bytes ((sp - 2)->u.string, (sp - 1)->u.number,
		       sp->u.string, SVALUE_STRLEN (sp));
      break;
    default:
      bad_argument (sp, T_BUFFER | T_STRING | T_NUMBER, 3, F_WRITE_BYTES);
    }
  free_svalue (sp--, "f_write_bytes");
  free_string_svalue (--sp);
  put_number (i);
}
#endif

#ifdef F_WRITE_BUFFER
void
f_write_buffer (void)
{
  int i = 0;

  if ((sp - 2)->type == T_STRING)
    {
      f_write_bytes ();
      return;
    }

  switch (sp->type)
    {
    case T_NUMBER:
      {
	int netint;
	char *netbuf;

	netint = htonl (sp->u.number);	/* convert to network
					 * byte-order */
	netbuf = (char *) &netint;
	i = write_buffer ((sp - 2)->u.buf, (sp - 1)->u.number, netbuf,
			  sizeof (int));
	break;
      }

    case T_BUFFER:
      i = write_buffer ((sp - 2)->u.buf, (sp - 1)->u.number,
			(char *) sp->u.buf->item, sp->u.buf->size);
      break;
    case T_STRING:
      i = write_buffer ((sp - 2)->u.buf, (sp - 1)->u.number,
			sp->u.string, SVALUE_STRLEN (sp));
      break;
    default:
      bad_argument (sp, T_BUFFER | T_STRING | T_NUMBER, 3, F_WRITE_BUFFER);
    }
  free_svalue (sp--, "f_write_buffer");
  free_string_svalue (--sp);
  put_number (i);
}
#endif


#ifdef F_RESTORE_OBJECT
void
f_restore_object (void)
{
  int flag;

  flag = (st_num_arg > 1) ? (sp--)->u.number : 0;
  flag = restore_object (current_object, sp->u.string, flag);
  free_string_svalue (sp);
  put_number (flag);
}
#endif


#ifdef F_SAVE_OBJECT
void
f_save_object (void)
{
  int flag;

  flag = (st_num_arg == 2) && (sp--)->u.number;
  flag = save_object (current_object, sp->u.string, flag);
  free_string_svalue (sp);
  put_number (flag);
}
#endif
