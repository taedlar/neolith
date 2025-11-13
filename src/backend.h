#pragma once
#include "interpret.h"

extern time_t current_time;
extern int heart_beat_flag;
extern object_t *current_heart_beat;
extern size_t eval_cost;

extern void preload_objects(int);
extern void backend(void);

extern void logon(object_t *);

extern int set_heart_beat(object_t *, int);
extern int query_heart_beat(object_t *);
extern int heart_beat_status(outbuffer_t *, int);
extern array_t *get_heart_beats(void);

extern void init_precomputed_tables(void);
extern void update_load_av(void);
extern void update_compile_av(int);
extern char *query_load_av(void);
