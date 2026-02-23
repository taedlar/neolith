#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "src/command.h"
#include "lpc/array.h"
#include "lpc/object.h"

#ifdef F_EXEC
void f_exec (void) {
  int i;

  i = replace_interactive ((sp - 1)->u.ob, sp->u.ob);

  /* They might have been destructed */
  if (sp->type == T_OBJECT)
    free_object (sp->u.ob, "f_exec:1");
  if ((--sp)->type == T_OBJECT)
    free_object (sp->u.ob, "f_exec:2");
  put_number (i);
}
#endif


#ifdef F_INTERACTIVE
void
f_interactive (void)
{
  int i;

  i = (sp->u.ob->interactive != 0);
  free_object (sp->u.ob, "f_interactive");
  put_number (i);
}
#endif


#ifdef F_USERP
void f_userp (void) {
  int i;

  i = (0 != (sp->u.ob->flags & O_ONCE_INTERACTIVE));
  if (sp->u.ob->flags & O_CONSOLE_USER)
    i = 2;
  free_object (sp->u.ob, "f_userp");
  put_number (i);
}
#endif


#ifdef F_USERS
void
f_users (void)
{
  push_refed_array (users ());
}
#endif


#ifdef F_GET_CHAR
void
f_get_char (void)
{
  svalue_t *arg;
  int i, tmp;
  int flag;
  int num_args = st_num_arg;  /* Save original count for cleanup */

  arg = sp - st_num_arg + 1;	/* Points arg at first argument. */
  if (st_num_arg == 1 || !(arg[1].type == T_NUMBER))
    {
      tmp = 0;
      flag = 0;
    }
  else
    {
      tmp = 1;
      st_num_arg--;		/* Don't count the flag as an arg */
      flag = (int)arg[1].u.number;
    }
  st_num_arg--;
  i = get_char (arg, flag, st_num_arg, &arg[1 + tmp]);
  pop_n_elems (num_args);
  push_number (i);
}
#endif


#ifdef F_INPUT_TO
void
f_input_to (void)
{
  svalue_t *arg;
  int i, tmp;
  int flag;
  int num_args = st_num_arg;  /* Save original count for cleanup */

  arg = sp - st_num_arg + 1;	/* Points arg at first argument. */
  if ((st_num_arg == 1) || !(arg[1].type == T_NUMBER))
    {
      tmp = flag = 0;
    }
  else
    {
      tmp = 1;
      st_num_arg--;		/* Don't count the flag as an arg */
      flag = (int)arg[1].u.number;
    }
  st_num_arg--;			/* Don't count the name of the func either. */
  i = input_to (arg, flag, st_num_arg, &arg[1 + tmp]);
  pop_n_elems (num_args);
  push_number (i);
}
#endif


#ifdef F_IN_INPUT
void
f_in_input (void)
{
  int i;

  i = sp->u.ob->interactive && sp->u.ob->interactive->input_to;
  free_object (sp->u.ob, "f_in_input");
  put_number (i != 0);
}
#endif


#ifdef F_RECEIVE
void
f_receive (void)
{
  if (current_object->interactive)
    {
      check_legal_string (sp->u.string);
      add_message (current_object, sp->u.string);
    }
  free_string_svalue (sp--);
}
#endif


#ifdef F_REMOVE_INTERACTIVE
/* Magician - 08May95
 * int remove_interactive(object ob)
 * If the object isn't destructed and is interactive, then remove it's
 * interactivity and disconnect it.  (useful for exec()ing to an already
 * interactive object, ie, Linkdead reconnection)
 */
void f_remove_interactive (void) {
  if ((sp->u.ob->flags & O_DESTRUCTED) || !(sp->u.ob->interactive))
    {
      free_object (sp->u.ob, "f_remove_interactive");
      *sp = const0;
    }
  else
    {
      remove_interactive (sp->u.ob, 0);
      /* It may have been dested */
      if (sp->type == T_OBJECT)
        free_object (sp->u.ob, "f_remove_interactive");
      *sp = const1;
    }
}
#endif


#ifdef F_QUERY_IDLE
void
f_query_idle (void)
{
  int64_t i;

  i = query_idle (sp->u.ob);
  free_object (sp->u.ob, "f_query_idle");
  put_number (i);
}
#endif


/* Zakk - August 23 1995
 * return the port number the interactive object used to connect to the
 * mud.
 */
#ifdef F_QUERY_IP_PORT
int query_ip_port (object_t * ob) {
  if (!ob || ob->interactive == 0)
    return 0;
  return ob->interactive->local_port;
}

void f_query_ip_port (void) {
  int tmp;

  if (st_num_arg)
    {
      tmp = query_ip_port (sp->u.ob);
      free_object (sp->u.ob, "f_query_ip_port");
    }
  else
    {
      tmp = query_ip_port (command_giver);
      sp++;
    }
  put_number (tmp);
}
#endif


#ifdef F_QUERY_IP_NAME
void
f_query_ip_name (void)
{
  char *tmp;

  tmp = query_ip_name (st_num_arg ? sp->u.ob : 0);
  if (st_num_arg)
    free_object ((sp--)->u.ob, "f_query_ip_name");
  if (!tmp)
    *++sp = const0;
  else
    share_and_push_string (tmp);
}
#endif


#ifdef F_QUERY_IP_NUMBER
void
f_query_ip_number (void)
{
  char *tmp;

  tmp = query_ip_number (st_num_arg ? sp->u.ob : 0);
  if (st_num_arg)
    free_object ((sp--)->u.ob, "f_query_ip_number");
  if (!tmp)
    *++sp = const0;
  else
    share_and_push_string (tmp);
}
#endif


#ifdef F_QUERY_HOST_NAME
void
f_query_host_name (void)
{
  char *tmp;

  if ((tmp = query_host_name ()))
    push_constant_string (tmp);
  else
    push_number (0);
}
#endif


#ifdef F_RESOLVE
void
f_resolve (void)
{
  int i, query_addr_number (char *, char *);

  i = query_addr_number ((sp - 1)->u.string, sp->u.string);
  free_string_svalue (sp--);
  free_string_svalue (sp);
  put_number (i);
}				/* f_resolve() */
#endif


#ifdef F_SNOOP
void f_snoop (void) {
  /*
   * This one takes a variable number of arguments. It returns 0 or an
   * object.
   */
  if (st_num_arg == 1)
    {
      if (!new_set_snoop (sp->u.ob, 0) || (sp->u.ob->flags & O_DESTRUCTED))
        {
          free_object (sp->u.ob, "f_snoop:1");
          *sp = const0;
        }
    }
  else
    {
      if (!new_set_snoop ((sp - 1)->u.ob, sp->u.ob) ||
          (sp->u.ob->flags & O_DESTRUCTED))
        {
          free_object ((sp--)->u.ob, "f_snoop:2");
          free_object (sp->u.ob, "f_snoop:3");
          *sp = const0;
        }
      else
        {
          free_object ((--sp)->u.ob, "f_snoop:4");
          sp->u.ob = (sp + 1)->u.ob;
        }
    }
}
#endif

#ifdef F_QUERY_SNOOPING
void
f_query_snooping (void)
{
  object_t *ob;
  ob = query_snooping (sp->u.ob);
  free_object (sp->u.ob, "f_query_snooping");
  if (ob)
    {
      put_unrefed_undested_object (ob, "query_snooping");
    }
  else
    *sp = const0;
}
#endif


#ifdef F_QUERY_SNOOP
void
f_query_snoop (void)
{
  object_t *ob;
  ob = query_snoop (sp->u.ob);
  free_object (sp->u.ob, "f_query_snoop");
  if (ob)
    {
      put_unrefed_undested_object (ob, "query_snoop");
    }
  else
    *sp = const0;
}
#endif
