#pragma once
#include "interpret.h"

extern time_t current_time;
extern int heart_beat_flag;
extern object_t *current_heart_beat;
extern size_t eval_cost;

void preload_objects(int);
void backend(void);

object_t* mudlib_connect(int, const char*);
void mudlib_logon(object_t *);

int set_heart_beat(object_t *, int);
int query_heart_beat(object_t *);
int heart_beat_status(outbuffer_t *, int);
array_t *get_heart_beats(void);

void init_precomputed_tables(void);
void update_load_av(void);
void update_compile_av(int);
char *query_load_av(void);

void init_console_user(int reconnect);
