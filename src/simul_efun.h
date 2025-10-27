#pragma once
#include "lpc/compiler.h"

typedef struct {
    compiler_function_t *func;
    int index;
} simul_info_t;

extern object_t *simul_efun_ob;
extern simul_info_t *simuls;
extern int simul_efun_is_loading;

extern void init_simul_efun(const char *);
extern void set_simul_efun(object_t *);
extern void unset_simul_efun();
extern int find_simul_efun(char *);

extern void call_simul_efun(unsigned short, int);
