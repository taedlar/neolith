#pragma once

#include "interpret.h"
#include "async/async_runtime.h"
#include "async/async_queue.h"
#include "async/console_worker.h"

#ifdef __cplusplus
extern "C" {
#endif

extern time_t current_time;
extern bool heart_beat_flag;
extern object_t *current_heart_beat;
extern int64_t eval_cost;

extern async_runtime_t *g_runtime;
extern console_worker_context_t *g_console_worker;
extern async_queue_t *g_console_queue;

void init_backend();

/* User reception functions */
void init_console_user(bool reconnect);
object_t* mudlib_connect(int port, const char* address);
void mudlib_logon(object_t *);

/* Heart beat related functions */
int set_heart_beat(object_t *, int);
int query_heart_beat(object_t *);
int heart_beat_status(outbuffer_t *, bool);
array_t *get_heart_beats(void);
void call_heart_beat(void);

void init_precomputed_tables(void);
void update_load_av(void);
void update_compile_av(int);
char *query_load_av(void);

#ifdef __cplusplus
}
#endif
