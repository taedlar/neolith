#pragma once
#include "types.h"
#include "functional.h"

/*
 * Definition of an object.
 * If the object is inherited, then it must not be destructed !
 *
 * The reset is used as follows:
 * 0: There is an error in the reset() in this object. Never call it again.
 * 1: Normal state.
 * 2 or higher: This is an interactive user, that has not given any commands
 *		for a number of reset periods.
 */

#define MAX_OBJECT_NAME_SIZE    2048

#define O_HEART_BEAT            0x0001	/* Does it have an heart beat ?      */
#define O_IS_WIZARD             0x0002	/* used to be O_IS_WIZARD            */
#define O_LISTENER              0x0004	/* can hear say(), etc */
#define O_ENABLE_COMMANDS       0x0004	/* Can it execute commands ?         */
#define O_CLONE                 0x0008	/* Is it cloned from a master copy ? */
#define O_DESTRUCTED            0x0010	/* Is it destructed ?                */
#define O_CONSOLE_USER          0x0020	/* Has it ever beena a console user ?*/
#define O_ONCE_INTERACTIVE      0x0040	/* Has it ever been interactive ?    */
#define O_RESET_STATE           0x0080	/* Object in a 'reset':ed state ?    */
#define O_WILL_CLEAN_UP         0x0100	/* clean_up will be called next time */
#define O_VIRTUAL               0x0200	/* We're a virtual object            */
#define O_HIDDEN                0x0400	/* We're hidden from nonprived objs  */
#define O_EFUN_SOCKET           0x0800	/* efun socket references object     */
#define O_WILL_RESET            0x1000	/* reset will be called next time    */
#define O_UNUSED                0x8000

/*
 * Note: use of more than 16 bits means extending flags to an unsigned long
 */

struct sentence_s {
    char *verb;
    struct sentence_s *next;
    object_t *ob;
    string_or_func_t function;
    int flags;
};

struct object_s {
    unsigned short ref;		/* Reference count. */
    unsigned short flags;	/* Bits or'ed together from above */
    char *name;
    struct object_s *next_hash;
    /* the fields above must match lpc_object_t */
    time_t load_time;		/* time when this object was created */
    time_t next_reset;		/* time of next reset of this object */
    time_t time_of_ref;		/* time when last referenced. Used by swap */
    program_t *prog;
    struct object_s *next_all;
    struct object_s *next_inv;
    struct object_s *contains;
    struct object_s *super;	/* Which object surround us ? */
    struct interactive_s *interactive;	/* Data about an interactive user */
    sentence_t *sent;
    struct object_s *next_hashed_living;
    char *living_name;		/* Name of living object if in hash */
    userid_t *uid;		/* the "owner" of this object */
    userid_t *euid;		/* the effective "owner" */
    svalue_t variables[1];	/* All variables to this program */
    /* The variables MUST come last in the struct */
};

#define add_ref(ob, str) ob->ref++

#define ROB_STRING_ERROR 1
#define ROB_ARRAY_ERROR 2
#define ROB_MAPPING_ERROR 4
#define ROB_NUMERAL_ERROR 8
#define ROB_GENERAL_ERROR 16
#define ROB_CLASS_ERROR 32
#define ROB_ERROR 63

extern object_t **hashed_living;
extern object_t *previous_ob;
extern int tot_alloc_object;
extern int tot_alloc_object_size;
extern int save_svalue_depth;

void init_objects();
void deinit_objects();
void bufcat(char **, char *);
int svalue_save_size(svalue_t *);
void save_svalue(svalue_t *, char **);
int restore_svalue(char *, svalue_t *);
int save_object(object_t *, const char *, int);
char *save_variable(svalue_t *);
int restore_object(object_t *, const char *, int);
void restore_variable(svalue_t *, char *);
object_t *get_empty_object(int);
void reset_object(object_t *);
void call_create(object_t *, int);
void reload_object(object_t *);
void free_object(object_t *, const char *);
object_t *find_living_object(char *, int);
int valid_hide(object_t *);
int object_visible(object_t *);
void set_living_name(object_t *, char *);
void remove_living_name(object_t *);
void stat_living_objects(outbuffer_t *);
void tell_npc(object_t *, char *);
void tell_object(object_t *, char *);
int find_global_variable(program_t *, char *, unsigned short *);
void dealloc_object(object_t *, const char *);
