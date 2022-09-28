#ifndef BACKEND_H
#define BACKEND_H

#include "interpret.h"

#define NULL_ERROR_CONTEXT       0
#define NORMAL_ERROR_CONTEXT     1
#define CATCH_ERROR_CONTEXT      2
#define SAFE_APPLY_ERROR_CONTEXT 4

/*
 * backend.c
 */
extern int current_time;
extern int heart_beat_flag;
extern object_t *current_heart_beat;
extern int eval_cost;
extern error_context_t *current_error_context;

extern void init_precomputed_tables(void);
extern void backend(void);
extern void clear_state(void);
extern void logon(object_t *);
extern int parse_command(char *, object_t *);
extern int set_heart_beat(object_t *, int);
extern int query_heart_beat(object_t *);
extern int heart_beat_status(outbuffer_t *, int);
extern void preload_objects(int);
extern void remove_destructed_objects(void);
extern void update_load_av(void);
extern void update_compile_av(int);
extern char *query_load_av(void);
extern array_t *get_heart_beats(void);

#endif
