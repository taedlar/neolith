#pragma once
#include "lpc/types.h"

#define V_SHORT         1
#define V_NOSPACE       2
#define V_FUNCTION      4

/* The end of a static buffer */
#define EndOf(x) (x + sizeof(x)/sizeof(x[0]))

extern int illegal_sentence_action;
extern object_t *obj_list;
extern object_t *obj_list_destruct;
extern object_t *current_object;
extern object_t *command_giver;
extern object_t *current_interactive;
extern char *inherit_file;
extern char *last_verb;
extern int tot_alloc_sentence;

char *strput(char *, char *, char *);
char *strput_int(char *, char *, int);

void check_legal_string(char *);

int user_parser(char *);
int command_for_object(char *);
void enable_commands(int);
void add_action(svalue_t *, char *, int);
int remove_action(char *, char *);
void free_sentence(sentence_t *);

int input_to(svalue_t *, int, int, svalue_t *);
int get_char(svalue_t *, int, int, svalue_t *);

extern void save_command_giver (object_t*);
extern void restore_command_giver (void);

int strip_name(char *, char *, int);
char *check_name(char *);
extern object_t *load_object(char *);
object_t *clone_object(char *, int);
object_t *environment(svalue_t *);
object_t *first_inventory(svalue_t *);
object_t *object_present(svalue_t *, object_t *);
object_t *find_object(char *);
object_t *find_object2(char *);
void move_object(object_t *, object_t *);
void destruct_object(object_t *);
void destruct2(object_t *);

void print_svalue(svalue_t *);
void do_write(svalue_t *);
void do_message(svalue_t *, svalue_t *, array_t *, array_t *, int);
void say(svalue_t *, array_t *);
void tell_room(object_t *, svalue_t *, array_t *);
void shout_string(char *);

extern char *dump_trace (int);
extern array_t *get_svalue_trace (int);

extern void throw_error(void) NO_RETURN;
extern void error_handler(char *) NO_RETURN;
extern void fatal(char *, ...) NO_RETURN;
extern void error(char *, ...) NO_RETURN;

void do_shutdown (int);
void slow_shut_down (int);

char* origin_name (int);
