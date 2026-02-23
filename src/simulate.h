#pragma once
#include "lpc/types.h"

#define V_SHORT         1
#define V_NOSPACE       2
#define V_FUNCTION      4

/* The end of a static buffer */
#define EndOf(x) (x + sizeof(x)/sizeof(x[0]))

extern int illegal_sentence_action;
extern object_t *master_ob;
extern object_t *obj_list;
extern object_t *obj_list_destruct;
extern object_t *current_object;
extern object_t *previous_ob;
extern object_t *command_giver;
extern object_t *current_interactive;
extern char *last_verb;
extern int tot_alloc_sentence;

char *strput(char *dest, char *end, const char *src);
char *strput_int(char *, char *, int);

void check_legal_string(const char *);

int user_parser(char *);
int64_t command_for_object(char *);
void enable_commands(int);
void add_action(svalue_t *, char *, int, int, svalue_t *);
int remove_action(char *, char *);
int process_command(char *, object_t *);

int input_to(svalue_t *, int, int, svalue_t *);
int get_char(svalue_t *, int, int, svalue_t *);

void save_command_giver (object_t*);
void restore_command_giver (void);

object_t *load_object(const char *mudlib_filename, const char *pre_text);
void reset_load_object_limits();
object_t *clone_object(const char *, int);
object_t *environment(svalue_t *);
object_t *first_inventory(svalue_t *);
object_t *object_present(svalue_t *, object_t *);
object_t *find_or_load_object(const char *);
object_t *find_object_by_name(const char *);
void move_object(object_t *, object_t *);
void destruct_object(object_t *);
void reset_destruct_object_limits();
void destruct2(object_t *);
void remove_destructed_objects(void);

void print_svalue(svalue_t *);
void do_write(svalue_t *);
void do_message(svalue_t *, svalue_t *, array_t *, array_t *, int);
void say(svalue_t *, array_t *);
void tell_room(object_t *, svalue_t *, array_t *);
void shout_string(char *);

#ifdef LAZY_RESETS
void try_reset(object_t *);
#endif

void init_master(const char *);
void setup_simulate(void);
void tear_down_simulate(void);

/**
 * @brief Get the current machine state.
 * @returns Returns one of the MS_* values defined below, or -1 before the machine is initialized.
 */
extern int get_machine_state();
#define MS_PRE_MUDLIB           0     /* The LPMUD driver has started successfully and ready to compile/run LPC code. */
#define MS_MUDLIB_LIMBO         1     /* The mudlib is in limbo, vital objects (master_ob and simul_efun_on) were loaded successfully. */
#define MS_MUDLIB_INTERACTIVE   2     /* The mudlib is ready for human interactions, master_ob has finished epilog() successfully. */

void fatal(char *, ...) NO_RETURN;
int in_fatal_error(void);

void do_shutdown (void) NO_RETURN;
void do_slow_shutdown (int);
