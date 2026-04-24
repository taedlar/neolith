#pragma once

#include "comm.h"
#include "lpc/functional.h"

#ifdef __cplusplus
extern "C" {
#endif

int process_command(char *buff, object_t *);

/* prompts & input redirection */
void set_prompt (char *);
int set_call (object_t *, sentence_t *, int);
int call_function_interactive (interactive_t *, char *);

/* sentence handling */
#define V_SHORT         1   /* Short form of the command */
#define V_NOSPACE       2   /* No space allowed in the command */
#define V_FUNCTION      4   /* Command is a function */
void add_action(svalue_t *action, const char *cmd, int flags, int num_carry, svalue_t *carry_args);
int remove_action(const char *cmd, const char *pattern);
void remove_sent (object_t *, object_t *);

/* command fail handling */
void notify_no_command (void);
void set_notify_fail_message (char *);
void set_notify_fail_function (funptr_t *);
void clear_notify (interactive_t *);

/* snooping */
int new_set_snoop (object_t *, object_t *);
object_t *query_snoop (object_t *);
object_t *query_snooping (object_t *);

/* command turn handling */
int cmd_in_buf (interactive_t *);
int process_user_command (void);

/* NPC command handling */
int command_for_object (const char *cmd, object_t *ob);

#ifdef __cplusplus
}
#endif
