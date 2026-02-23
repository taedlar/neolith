#pragma once

#include "comm.h"
#include "lpc/functional.h"

/* prompts & input redirection */
void set_prompt (char *);
int set_call (object_t *, sentence_t *, int);
int call_function_interactive (interactive_t *, char *);

/* command fail handling */
void notify_no_command (void);
void set_notify_fail_message (char *);
void set_notify_fail_function (funptr_t *);
void clear_notify (interactive_t *);

/* snooping */
int new_set_snoop (object_t *, object_t *);
object_t *query_snoop (object_t *);
object_t *query_snooping (object_t *);

/* command buffer handling */
int cmd_in_buf (interactive_t *);
int process_user_command (void);
