#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "src/std.h"
#include "src/command.h"
#include "rc.h"
#include "lpc/object.h"
#include "lpc/array.h"


#ifdef F_ENABLE_COMMANDS
void
f_enable_commands (void)
{
  enable_commands (1);
}
#endif


#ifdef F_DISABLE_COMMANDS
void
f_disable_commands (void)
{
  enable_commands (0);
}
#endif


#ifdef F_SET_LIVING_NAME
void
f_set_living_name (void)
{
  set_living_name (current_object, sp->u.string);
  free_string_svalue (sp--);
}
#endif


#ifdef F_LIVING
void
f_living (void)
{
  int living;

  living = sp->u.ob->flags & O_ENABLE_COMMANDS;
  free_object (sp->u.ob, "f_living");

  *sp = living ? const1 : const0;
}
#endif


#ifdef F_LIVINGS
void
f_livings (void)
{
  push_refed_array (livings ());
}
#endif


/*
 * This differs from the livings() efun in that this efun only returns
 * objects which have had set_living_name() called as well as 
 * enable_commands().  The other major difference is that this is
 * substantially faster.
 */
#ifdef F_NAMED_LIVINGS
void
f_named_livings ()
{
  int i;
  int nob, apply_valid_hide, hide_is_valid = 0;
  object_t *ob, **obtab;
  array_t *vec;

  nob = 0;
  apply_valid_hide = 1;

  obtab = CALLOCATE (CONFIG_INT (__MAX_ARRAY_SIZE__), object_t *, TAG_TEMPORARY, "named_livings");

  for (i = 0; i < CONFIG_INT (__LIVING_HASH_TABLE_SIZE__); i++)
    {
      for (ob = hashed_living[i]; ob; ob = ob->next_hashed_living)
        {
          if (!(ob->flags & O_ENABLE_COMMANDS))
            continue;
          if (ob->flags & O_HIDDEN)
            {
              if (apply_valid_hide)
                {
                  apply_valid_hide = 0;
                  hide_is_valid = valid_hide (current_object);
                }
              if (hide_is_valid)
                continue;
            }
          if (nob == CONFIG_INT (__MAX_ARRAY_SIZE__))
            break;
          obtab[nob++] = ob;
        }
    }

  vec = allocate_empty_array (nob);
  while (--nob >= 0)
    {
      vec->item[nob].type = T_OBJECT;
      vec->item[nob].u.ob = obtab[nob];
      add_ref (obtab[nob], "livings");
    }

  FREE (obtab);

  push_refed_array (vec);
}
#endif


#ifdef F_FIND_PLAYER
void
f_find_player (void)
{
  object_t *ob;
  ob = find_living_object (sp->u.string, 1);
  free_string_svalue (sp);
  if (ob)
    {
      put_unrefed_undested_object (ob, "find_player");
    }
  else
    *sp = const0;
}
#endif


#ifdef F_ENABLE_WIZARD
void
f_enable_wizard (void)
{
  if (current_object->interactive)
    current_object->flags |= O_IS_WIZARD;
}
#endif


#ifdef F_DISABLE_WIZARD
void
f_disable_wizard (void)
{
  if (current_object->interactive)
    current_object->flags &= ~O_IS_WIZARD;
}
#endif


#ifdef F_WIZARDP
void
f_wizardp (void)
{
  int i;

  i = (int) sp->u.ob->flags & O_IS_WIZARD;
  free_object (sp->u.ob, "f_wizardp");
  put_number (i != 0);
}
#endif


#ifdef F_FIND_LIVING
void
f_find_living (void)
{
  object_t *ob = find_living_object (sp->u.string, 0);
  free_string_svalue (sp);
  /* safe b/c destructed objects have had their living names removed */
  if (ob)
    {
      put_unrefed_undested_object (ob, "find_living");
    }
  else
    *sp = const0;
}
#endif


#ifdef F_ADD_ACTION
void
f_add_action (void)
{
  uint64_t flag = 0;
  int num_carry = 0;
  svalue_t *carry_args = NULL;

  /* Extract flag and carryover args */
  if (st_num_arg >= 3)
    {
      /* Check if 3rd arg is number (flag) or first carryover arg */
      if ((sp - (st_num_arg - 3))->type == T_NUMBER)
        {
          flag = (sp - (st_num_arg - 3))->u.number;
          num_carry = st_num_arg - 3;
          if (num_carry > 0)
            carry_args = sp - num_carry + 1;
        }
      else
        {
          /* 3rd arg is first carryover, no flag */
          flag = 0;
          num_carry = st_num_arg - 2;
          carry_args = sp - num_carry + 1;
        }
    }

  /* Handle array of verbs or single verb */
  if ((sp - (st_num_arg - 2))->type == T_ARRAY)
    {
      int i, n = (sp - (st_num_arg - 2))->u.arr->size;
      svalue_t *sv = (sp - (st_num_arg - 2))->u.arr->item;

      for (i = 0; i < n; i++)
        {
          if (sv[i].type == T_STRING)
            {
              add_action (sp - (st_num_arg - 1), sv[i].u.string,
                         flag & 3, num_carry, carry_args);
            }
        }
    }
  else
    {
      /* Single verb */
      add_action ((sp - (st_num_arg - 1)),
                 (sp - (st_num_arg - 2))->u.string,
                 flag & 3, num_carry, carry_args);
    }

  /* Pop all args */
  pop_n_elems (st_num_arg);
}
#endif


#ifdef F_REMOVE_ACTION
void
f_remove_action (void)
{
  int success;

  success = remove_action ((sp - 1)->u.string, sp->u.string);
  free_string_svalue (sp--);
  free_string_svalue (sp);
  put_number (success);
}
#endif


#ifdef F_QUERY_VERB
void
f_query_verb (void)
{
  if (!last_verb)
    {
      push_number (0);
      return;
    }
  share_and_push_string (last_verb);
}
#endif


#ifdef F_COMMAND
void
f_command (void)
{
  int64_t i;

  i = command_for_object (sp->u.string);
  free_string_svalue (sp);
  put_number (i);
}
#endif


#ifdef F_COMMANDS
void
f_commands (void)
{
  push_refed_array (commands (current_object));
}
#endif


#ifdef F_NOTIFY_FAIL
void
f_notify_fail (void)
{
  if (sp->type == T_STRING)
    {
      set_notify_fail_message (sp->u.string);
      free_string_svalue (sp--);
    }
  else
    {
      set_notify_fail_function (sp->u.fp);
      free_funp ((sp--)->u.fp);
    }
}
#endif


/* This was originally written my Malic for Demon.  I rewrote parts of it
   when I added it (added function support, etc) -Beek */
#ifdef F_QUERY_NOTIFY_FAIL
void
f_query_notify_fail (void)
{
  char *p;

  if (command_giver && command_giver->interactive)
    {
      if (command_giver->interactive->iflags & NOTIFY_FAIL_FUNC)
        {
          push_funp (command_giver->interactive->default_err_message.f);
          return;
        }
      else if ((p = command_giver->interactive->default_err_message.s))
        {
          sp++;
          sp->type = T_STRING;
          sp->subtype = STRING_SHARED;
          sp->u.string = p;
          ref_string (p);
          return;
        }
    }
  push_number (0);
}
#endif

