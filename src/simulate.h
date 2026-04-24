#pragma once

#include "lpc/types.h"

#ifdef __cplusplus
extern "C" {
#endif

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
extern int tot_alloc_sentence;

char *strput(char *dest, char *end, const char *src);
char *strput_int(char *, char *, int);

/* command handling */
void enable_commands (int enable);
int process_command(char *buff, object_t *);
int64_t command_for_object(const char *cmd);
void add_action(svalue_t *, const char *, int, int, svalue_t *);
int remove_action(const char *, const char *);

int input_to(svalue_t *, int, int, svalue_t *);
int get_char(svalue_t *, int, int, svalue_t *);

void save_command_giver (object_t*);
void restore_command_giver (void);

/* object physics */
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
void remove_destructed_objects(void);

/* message handling */
void do_write(svalue_t *);
void do_message(svalue_t *, svalue_t *, array_t *, array_t *, int);
void say(svalue_t *, array_t *);
void tell_room(object_t *, svalue_t *, array_t *);
void shout_string(const char *);

#ifdef LAZY_RESETS
void try_reset(object_t *);
#endif

void init_master(const char *, const char *);
void setup_simulate(void);
void tear_down_simulate(void);

/**
 * @brief Get the current MUD state.
 *
 * The \c MS_MUDLIB_LIMBO state is used to indicate that the mudlib is ready for master
 * applies, meaning the main "policy" verdicts are in place.
 * Rule of thumb is:
 * - If mud_state() < MS_MUDLIB_LIMBO, the master object should not be
 *   trusted to make security-sensitive decisions.
 * - If mud_state() >= MS_MUDLIB_LIMBO, master applies should be consulted
 *   to make security-sensitive decisions.
 * 
 * The \c MS_MUDLIB_INTERACTIVE state indicates the mudlib has finished epilog() and the
 * game is ready for user interactions. This is the state where "time" in the game
 * starts ticking and players can start playing. Heart beats, call_outs, reset and cleanups
 * will start working in this state.
 * 
 * Note that the machine state is not necessarily monotonic; for example, if the master
 * object fails to load, the machine state may transition from MS_MUDLIB_LIMBO back to
 * MS_PRE_MUDLIB or even MS_FATAL_ERROR.
 * 
 * @returns Returns one of the MS_* values defined below.
 */
int mud_state();
#define MS_FATAL_ERROR          -2    /* The machine is in a fatal error state, where the driver loop is still running but the mudlib is not functional. */
#define MS_PRE_INIT             -1    /* The machine is in a pre-initialization state, where the driver loop has not started yet. */
#define MS_PRE_MUDLIB           0     /* The LPMUD driver has started successfully and ready to compile/run LPC code. */
#define MS_MUDLIB_LIMBO         1     /* The mudlib is in limbo, vital objects (master_ob and simul_efun_on) were loaded successfully. */
#define MS_MUDLIB_INTERACTIVE   2     /* The mudlib is ready for human interactions, master_ob has finished epilog() successfully. */

void fatal(const char* fmt, ...) NO_RETURN;  /* Fatal termination path; never returns to caller. */

void do_shutdown (void) NO_RETURN;            /* Shutdown path terminates process/driver loop. */
void do_slow_shutdown (int);

#ifdef __cplusplus
}
#endif
