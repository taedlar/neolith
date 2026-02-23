#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"
#include "lpc/array.h"
#include "lpc/object.h"
#include "lpc/include/origin.h"
#include "comm.h"
#include "command.h"
#include "rc/rc.h"
#include "efuns/ed.h"

 /*
  * This macro is for testing whether ip is still valid, since many
  * functions call LPC code, which could otherwise use
  * enable_commands(), set_this_player(), or destruct() to cause
  * all hell to break loose by changing or dangling command_giver
  * or command_giver->interactive.  It also saves us a few dereferences
  * since we know we can trust ip, and also increases code readability.
  *
  * Basically, this should be used as follows:
  *
  * (1) when using command_giver:
  *     set a variable named ip to command_giver->interactive at a point
  *     when you know it is valid.  Then, after a call that might have
  *     called LPC code, check IP_VALID(command_giver), or use
  *     VALIDATE_IP.
  * (2) some other object:
  *     set a variable named ip to ob->interactive, and save ob somewhere;
  *     or if you are just dealing with an ip as input, save ip->ob somewhere.
  *     After calling LPC code, check IP_VALID(ob), or use VALIDATE_IP.
  * 
  * Yes, I know VALIDATE_IP uses a goto.  It's due to C's lack of proper
  * exception handling.  Only use it in subroutines that are set up
  * for it (i.e. define a failure label, and are set up to deal with
  * branching to it from arbitrary points).
  */
#define IP_VALID(ip, ob) (ob && ob->interactive == ip)
#define VALIDATE_IP(ip, ob) if (!IP_VALID(ip, ob)) goto failure


static char *get_user_command (void);
static char *first_cmd_in_buf (interactive_t *);
static void next_cmd_in_buf (interactive_t *);
static void print_prompt (interactive_t *);


void set_prompt (char *str) {
  if (command_giver && command_giver->interactive)
    command_giver->interactive->prompt = str;
}


/**
 *  @brief Print the prompt, but only if input_to is disabled.
 */
static void print_prompt (interactive_t * ip) {
  object_t *ob = ip->ob;

  if (ip->input_to == 0)
    {
      /* give user object a chance to write its own prompt */
      if (!(ip->iflags & HAS_WRITE_PROMPT))
        tell_object (ip->ob, ip->prompt);
#ifdef OLD_ED
      else if (ip->ed_buffer)
        tell_object (ip->ob, ip->prompt);
#endif
      else if (!apply (APPLY_WRITE_PROMPT, ip->ob, 0, ORIGIN_DRIVER))
        {
          if (!IP_VALID (ip, ob))
            return;
          ip->iflags &= ~HAS_WRITE_PROMPT;
          tell_object (ip->ob, ip->prompt);
        }
    }

#if 0
  /*
   * Put the IAC GA thing in here... Moved from before writing the prompt;
   * vt src says it's a terminator. Should it be inside the no-input_to
   * case? We'll see, I guess.
   * 
   * TODO: we have negotiated suppress GA, so we should only send this if
   * the client did not negotiate that option.
   */
  if (ip->iflags & USING_TELNET)
    add_message (command_giver, telnet_ga);
#endif

  flush_message (ip);
}


void notify_no_command () {
  string_or_func_t p;
  svalue_t *v;

  if (!command_giver || !command_giver->interactive)
    return;
  p = command_giver->interactive->default_err_message;
  if (command_giver->interactive->iflags & NOTIFY_FAIL_FUNC)
    {
      save_command_giver (command_giver);
      v = call_function_pointer (p.f, 0);
      restore_command_giver ();
      free_funp (p.f);
      if (command_giver && command_giver->interactive)
        {
          if (v && v->type == T_STRING)
            tell_object (command_giver, v->u.string);
          command_giver->interactive->iflags &= ~NOTIFY_FAIL_FUNC;
          command_giver->interactive->default_err_message.s = 0;
        }
    }
  else
    {
      if (p.s)
        {
          tell_object (command_giver, p.s);
          free_string (p.s);
          command_giver->interactive->default_err_message.s = 0;
        }
      else if (CONFIG_STR (__DEFAULT_FAIL_MESSAGE__))
        {
          add_vmessage (command_giver, "%s\n",
                        CONFIG_STR (__DEFAULT_FAIL_MESSAGE__));
        }
      else
        {
          tell_object (command_giver, "What?\n");
        }
    }
}				/* notify_no_command() */

void clear_notify (interactive_t * ip) {
  string_or_func_t dem;

  dem = ip->default_err_message;
  if (ip->iflags & NOTIFY_FAIL_FUNC)
    {
      free_funp (dem.f);
      ip->iflags &= ~NOTIFY_FAIL_FUNC;
    }
  else if (dem.s)
    free_string (dem.s);
  ip->default_err_message.s = 0;
}				/* clear_notify() */

void set_notify_fail_message (char *str) {
  if (!command_giver || !command_giver->interactive)
    return;
  clear_notify (command_giver->interactive);
  command_giver->interactive->default_err_message.s = make_shared_string (str);
}				/* set_notify_fail_message() */

void set_notify_fail_function (funptr_t * funp) {
  if (!command_giver || !command_giver->interactive)
    return;
  clear_notify (command_giver->interactive);
  command_giver->interactive->iflags |= NOTIFY_FAIL_FUNC;
  command_giver->interactive->default_err_message.f = funp;
  funp->hdr.ref++;
}				/* set_notify_fail_function() */



/*
 * Let object 'me' snoop object 'you'. If 'you' is 0, then turn off
 * snooping.
 *
 * This routine is almost identical to the old set_snoop. The main
 * difference is that the routine writes nothing to user directly,
 * all such communication is taken care of by the mudlib. It communicates
 * with master.c in order to find out if the operation is permissble or
 * not. The old routine let everyone snoop anyone. This routine also returns
 * 0 or 1 depending on success.
 */
int new_set_snoop (object_t * me, object_t * you) {
  interactive_t *on, *by, *tmp;

  /*
   * Stop if people managed to quit before we got this far.
   */
  if (me->flags & O_DESTRUCTED)
    return (0);
  if (you && (you->flags & O_DESTRUCTED))
    return (0);
  /*
   * Find the snooper && snoopee.
   */
  if (!me->interactive)
    error ("First argument of snoop() is not interactive!\n");

  by = me->interactive;

  if (you)
    {
      if (!you->interactive)
        error ("Second argument of snoop() is not interactive!\n");
      on = you->interactive;
    }
  else
    {
      /*
       * Stop snoop.
       */
      if (by->snoop_on)
        {
          by->snoop_on->snoop_by = 0;
          by->snoop_on = 0;
        }
      return 1;
    }

  /*
   * Protect against snooping loops.
   */
  for (tmp = on; tmp; tmp = tmp->snoop_on)
    {
      if (tmp == by)
        return (0);
    }

  /*
   * Terminate previous snoop, if any.
   */
  if (by->snoop_on)
    {
      by->snoop_on->snoop_by = 0;
      by->snoop_on = 0;
    }
  if (on->snoop_by)
    {
      on->snoop_by->snoop_on = 0;
      on->snoop_by = 0;
    }
  on->snoop_by = by;
  by->snoop_on = on;
  return (1);
}				/* set_new_snoop() */

object_t *query_snoop (object_t * ob) {
  if (!ob->interactive || (ob->interactive->snoop_by == 0))
    return (0);
  return (ob->interactive->snoop_by->ob);
}				/* query_snoop() */

object_t *query_snooping (object_t * ob) {
  if (!ob->interactive || (ob->interactive->snoop_on == 0))
    return (0);
  return (ob->interactive->snoop_on->ob);
}				/* query_snooping() */


/**
 *  @brief Set up an input_to call for an interactive object.
 *  @param ob The interactive object.
 *  @param sent The sentence (function and object) to call.
 *  @param flags Flags for the input_to call (I_NOECHO = 1, I_NOESC = 2, I_SINGLE_CHAR = 4).
 *  @return 1 on success, 0 on failure.
 */
int set_call (object_t * ob, sentence_t * sent, int flags) {
  if (ob == 0 || sent == 0 || ob->interactive == 0 || ob->interactive->input_to)
    return 0;

  ob->interactive->input_to = sent;
  ob->interactive->iflags |= (flags & (I_NOECHO | I_NOESC | I_SINGLE_CHAR));

  if (ob->interactive == all_users[0])
    {
      /* don't try to set telnet options on console */
      if (flags & I_NOECHO)
        set_console_echo (0);
    }
  else
    {
      /* This is a TELNET trick to hide input by telling the client that we'll be doing echo,
       * but we won't actually do it.
       */
      if (flags & I_NOECHO)
        set_telnet_echo (ob, 1);
    }

  if (flags & I_SINGLE_CHAR)
    set_telnet_single_char (ob->interactive, 1);
  return 1;
}				/* set_call() */

/**
 * Call a function on an interactive object set up by input_to() efun or
 * get_char() efun.
 *
 * @param i The interactive structure for the user.
 * @param str The input string to pass to the function.
 * @return 1 if a function was called, otherwise returns 0.
 */
int call_function_interactive (interactive_t * i, char *str) {

  funptr_t *funp = NULL;
  array_t *args;
  sentence_t *sent;
  int num_arg;

  i->iflags &= ~NOESC; /* remove disable shell escape flag */

  if (!(sent = i->input_to))
    return 0; /* no input_to() was set up on this interactive */

  /* [NEOLITH-EXTENSION] The sentence is always V_FUNCTION now. And carryover arguments
   * are passed via sent->args array. This is more efficient and flexible than the old
   * code that stores a carryover svalue_t array in the interactive_t struct.
   */
  DEBUG_CHECK (!(sent->flags & V_FUNCTION), "input_to must be function pointer");
  funp = sent->function.f;
  funp->hdr.ref++; /* by local variable funp */

  args = sent->args;
  if (args)
    args->ref++; /* by local variable args */
  num_arg = args ? args->size : 0;

  /* Free sentence before calling the function pointer.
   * This is necessary since the input_to/get_char callback (LPC code) may call
   * set_call() again to set up a new input_to before the current callback returns,
   * and we need to free the old sentence or the set_call() will fail due to the
   * existing sentence.
   */
  free_sentence (sent);
  i->input_to = 0;

  /* Disable single char mode if needed */
  if (i->iflags & SINGLE_CHAR)
    {
      i->iflags &= ~SINGLE_CHAR;
      set_telnet_single_char (i, 0);
    }

  /* Push input FIRST.
   * The LPC efun input_to/get_char expect the input string to be the
   * first argument, followed by any carryover args from the original call
   * to input_to/get_char.
   */
  copy_and_push_string (str);

  if (args)
    {
      /* Push carryover args AFTER input */
      for (int j = 0; j < args->size; j++)
        {
          push_svalue (&args->item[j]);
        }
      free_array (args); /* by local variable args */
      args = 0; /* this is always the last reference to carryover args array */
    }

  /* Call function pointer.
   * The function pointer can be a closure with arguments already bound.
   * In the case, they will be combined via merge_arg_lists() to form the
   * actual argument list. For example:
   *     input_to(bind((: foo :), arg1, arg2), I_NOECHO, arg3, arg4);
   * will result in a call to:
   *     foo(arg1, arg2, str, arg3, arg4) where str is the user input.
   */
  call_function_pointer (funp, num_arg + 1);
  free_funp (funp); /* by local variable funp */
  funp = 0;
  return 1;
}				/* call_function_interactive() */


/** @brief Return the next user command to be processed in sequence.
 * The order of user command being processed is "rotated" so that no one
 * user can monopolize the command processing. The \c s_next_user
 * static variable keeps track of which user should be checked next.
 * This function scans through all connected users starting from
 * s_next_user and looks for a user with a complete command
 * in his input buffer. It also calls \c flush_message() to ensure
 * that any outgoing messages are sent to the user before processing
 * his input.
 * 
 * This should also return a value if there is something in the
 * buffer and we are supposed to be in single character mode.
 * 
 * @returns Pointer to a static buffer containing the next user command to be processed
 * and updates \c command_giver if a command is found, or 0 if no commands are available.
 */
static char* get_user_command () {

  /* A static counter that iterates between all users in sequence.
   * This ensures fair processing of user commands.
   */
  static int s_next_user = 0;

  int i;
  interactive_t *ip = NULL;
  char *user_command = NULL;
  static char buf[MAX_TEXT];

  /*
   * find and return a user command.
   */
  for (i = 0; i < max_users; i++)
    {
      ip = all_users[s_next_user];
      if (ip && ip->message_length)
        {
          object_t *ob = ip->ob;
          flush_message (ip);
          if (!IP_VALID (ip, ob))
            ip = 0;
        }

      if (ip && ip->iflags & CMD_IN_BUF)
        {
          user_command = first_cmd_in_buf (ip);
          if (user_command)
            {
              /* Check if user has their turn */
              if (ip->iflags & HAS_CMD_TURN)
                {
                  ip->iflags &= ~HAS_CMD_TURN;  /* Consume turn */
                  break;  /* Process this command */
                }
              else
                {
                  /* User has command but no turn - skip and continue searching */
                  user_command = NULL;
                }
            }
          else
            ip->iflags &= ~CMD_IN_BUF;
        }

      if (s_next_user-- == 0)
        s_next_user = max_users - 1; /* wrap around */
    }

  /*
   * no cmds found; return 0.
   */
  if (!ip || !user_command)
    return 0;

  /*
   * we have a user cmd -- return it. If user has only one partially
   * completed cmd left after this, move it to the start of his buffer; new
   * stuff will be appended.
   */
  command_giver = ip->ob;

  /*
   * telnet option parsing and negotiation.
   */
  telnet_neg (buf, user_command);

  /*
   * move input buffer pointers to next command.
   */
  next_cmd_in_buf (ip);
  if (!cmd_in_buf (ip))
    ip->iflags &= ~CMD_IN_BUF;

  if (s_next_user-- == 0)
    s_next_user = max_users - 1; /* wrap around */

  if (ip->iflags & NOECHO)
    {
      /*
       * Must not enable echo before the user input is received.
       */
      if (ip->connection_type == CONSOLE_USER)
        {
          set_console_echo (1);
        }
      else if (ip->connection_type == PORT_TELNET)
        {
          set_telnet_echo (command_giver, 0);
        }
      ip->iflags &= ~NOECHO;
    }

  ip->last_time = current_time;
  return buf;
}


/*
 * find the first character of the next complete cmd in a buffer, 0 if no
 * completed cmd.  There is a completed cmd if there is a null between
 * text_start and text_end.  Zero length commands are discarded (as occur
 * between <cr> and <lf>).  Update text_start if we have to skip leading
 * nulls.
 * This should return true when in single char mode and there is
 * Anything at all in the buffer.
 */
static char* first_cmd_in_buf (interactive_t * ip) {
  char *p, *q;

  p = ip->text + ip->text_start;

  /*
   * skip null input.
   */
  while ((p < (ip->text + ip->text_end)) && !*p)
    p++;

  ip->text_start = p - ip->text;

  if (ip->text_start >= ip->text_end)
    {
      ip->text_start = ip->text_end = 0;
      ip->text[0] = '\0';
      return 0;
    }
  /* If we got here, must have something in the array */
  if (ip->iflags & SINGLE_CHAR)
    {
      /* We need to return true here... */
      return (ip->text + ip->text_start);
    }
  /*
   * find end of cmd.
   */
  while ((p < (ip->text + ip->text_end)) && *p)
    p++;
  /*
   * null terminated; was command.
   */
  if (p < ip->text + ip->text_end)
    return (ip->text + ip->text_start);
  /*
   * have a partial command at end of buffer; move it to start, return
   * null. if it can't move down, truncate it and return it as cmd.
   */
  p = ip->text + ip->text_start;
  q = ip->text;
  while (p < (ip->text + ip->text_end))
    *(q++) = *(p++);

  ip->text_end -= ip->text_start;
  ip->text_start = 0;
  if (ip->text_end > MAX_TEXT - 2)
    {
      ip->text[ip->text_end - 2] = '\0';	/* nulls to truncate */
      ip->text[ip->text_end - 1] = '\0';	/* nulls to truncate */
      ip->text_end--;
      return (ip->text);
    }
  /*
   * buffer not full and no newline - no cmd.
   */
  return 0;
}				/* first_command_in_buf() */

/**
 *  @brief Check if there is a complete, non-empty line in the buffer.
 *  Looks for a null character between text_start and text_end.
 *  If in SINGLE_CHAR mode, any input is a complete command.
 *  @param ip The interactive structure for the user.
 *  @return 1 if there is a complete command, otherwise returns zero.
 */
int cmd_in_buf (interactive_t * ip) {

  const char *p;

  p = ip->text + ip->text_start;

  /* skip empty lines */
  while ((p < (ip->text + ip->text_end)) && !*p)
    p++;

   /* end of user command buffer? */
  if ((p - ip->text) >= ip->text_end)
    return 0;

  /* expecting single character input? */
  if (ip->iflags & SINGLE_CHAR)
    return 1;

  /* find end of command */
  while ((p < (ip->text + ip->text_end)) && *p)
    p++;
  if (p < ip->text + ip->text_end)
    return 1;

  /* user command buffer is empty or only partial command received. */
  return 0;
}

/*
 * move pointers to next cmd, or clear buf.
 */
static void next_cmd_in_buf (interactive_t * ip) {
  char *p = ip->text + ip->text_start;

  while (*p && p < ip->text + ip->text_end)
    p++;
  /*
   * skip past any nulls at the end.
   */
  while (!*p && p < ip->text + ip->text_end)
    p++;
  if (p < ip->text + ip->text_end)
    ip->text_start = p - ip->text;
  else
    {
      ip->text_start = ip->text_end = 0;
      ip->text[0] = '\0';
    }
}				/* next_cmd_in_buf() */


/**
 *  User command turn handler.
 *
 *  This function is called by the backend after unblocked from a communication polling.
 *  Network traffics from all connected users are buffered in each user's command buffer and
 *  marked with CMD_IN_BUF flag if a complete command is available.
 * 
 *  This function calls \c get_user_command() to iterate over all connected users,
 *  assigining \c command_giver to each user in turn, and checking for pending commands.
 *  If a command is pending, it is processed by \c process_command() or \c apply() to the user
 *  object as appropriate.
 *  
 *  User commands are processed in sequence (round-robin) that one user command is processed
 *  per execution of this function.
 * 
 *  @return Returns 1 if a user command was processed, 0 if no more user commands are pending.
 */
int process_user_command () {

  char *user_command;
  static char buf[MAX_TEXT], *tbuf;
  object_t *save_current_object = current_object;
  object_t *save_command_giver = command_giver;
  interactive_t *ip;
  svalue_t *ret;

  buf[MAX_TEXT - 1] = '\0';

  /* WARNING: get_user_command() sets command_giver */
  if ((user_command = get_user_command ()))
    {
#if defined(NO_ANSI) && defined(STRIP_BEFORE_PROCESS_INPUT)
      char *p;
      for (p = user_command; *p; p++)
        {
          if (*p == 27)
            {
              char *q = buf;
              for (p = user_command; *p && p - user_command < MAX_TEXT - 1; p++)
                *q++ = ((*p == 27) ? ' ' : *p);
              *q = 0;
              user_command = buf;
              break;
            }
        }
#endif

      if (command_giver->flags & O_DESTRUCTED)
        {
          command_giver = save_command_giver;
          current_object = save_current_object;
          return 1;
        }
      ip = command_giver->interactive;
      if (!ip)
        return 1;
      current_interactive = command_giver;
      current_object = 0;
      clear_notify (ip);
      update_load_av ();
      tbuf = user_command;

      /*
       * Check for special command prefixes.
       * '!' indicates a command to be processed by process_input() in the user object.
       * If ed_buffer is set, the command is for the line editor
       */
      if ((user_command[0] == '!') && (
#ifdef OLD_ED
          ip->ed_buffer ||
#endif
          (ip->input_to && !(ip->iflags & NOESC))))
        {
          if (ip->iflags & SINGLE_CHAR)
            {
              /* only 1 char ... switch to line buffer mode */
              ip->iflags |= WAS_SINGLE_CHAR;
              ip->iflags &= ~SINGLE_CHAR;
              set_telnet_single_char (ip, 0);
              /* come back later */
            }
          else
            {
              if (ip->iflags & WAS_SINGLE_CHAR)
                {
                  /* we now have a string ... switch back to char mode */
                  ip->iflags &= ~WAS_SINGLE_CHAR;
                  ip->iflags |= SINGLE_CHAR;
                  set_telnet_single_char (ip, 1);
                  VALIDATE_IP (ip, command_giver);
                }

              if (ip->iflags & HAS_PROCESS_INPUT)
                {
                  copy_and_push_string (user_command + 1);
                  ret = apply (APPLY_PROCESS_INPUT, command_giver, 1, ORIGIN_DRIVER);
                  VALIDATE_IP (ip, command_giver);
                  if (!ret)
                    ip->iflags &= ~HAS_PROCESS_INPUT;
                  if (ret && ret->type == T_STRING)
                    {
                      strncpy (buf, ret->u.string, MAX_TEXT - 1);
                      process_command (buf, command_giver);
                    }
                  else if (!ret || ret->type != T_NUMBER || !ret->u.number)
                    {
                      process_command (tbuf + 1, command_giver);
                    }
                }
              else
                process_command (tbuf + 1, command_giver);
            }
#ifdef OLD_ED
        }
      else if (ip->ed_buffer)
        {
          ed_cmd (user_command);
#endif /* OLD_ED */
        }
      else if (call_function_interactive (ip, user_command))
        {
          /* input_to or get_char handled by call_function_interactive() */
        }
      else
        {
          /*
           * send a copy of user input back to user object to provide
           * support for things like command history and mud shell
           * programming languages.
           */
          if (ip->iflags & HAS_PROCESS_INPUT)
            {
              copy_and_push_string (user_command);
              ret = apply (APPLY_PROCESS_INPUT, command_giver, 1, ORIGIN_DRIVER);
              VALIDATE_IP (ip, command_giver);
              if (!ret)
                ip->iflags &= ~HAS_PROCESS_INPUT;
              if (ret && ret->type == T_STRING)
                {
                  strncpy (buf, ret->u.string, MAX_TEXT - 1);
                  process_command (buf, command_giver);
                }
              else if (!ret || ret->type != T_NUMBER || !ret->u.number)
                {
                  process_command (tbuf, command_giver);
                }
            }
          else
            process_command (tbuf, command_giver);
        }
      VALIDATE_IP (ip, command_giver);
      /*
       * Print a prompt if user is still here.
       */
      print_prompt (ip);
    failure:
      current_object = save_current_object;
      command_giver = save_command_giver;
      current_interactive = 0;
      return (1);
    }
  /* no more commands */
  current_object = save_current_object;
  command_giver = save_command_giver;
  current_interactive = 0;
  return 0;
}				/* process_user_command() */
