#pragma once
#include "lpc/types.h"
#include "lpc/include/runtime_config.h"

#define CONFIG_STR(x)           config_str[(x) - BASE_CONFIG_STR]
#define CONFIG_INT(x)           config_int[(x) - BASE_CONFIG_INT]

extern int config_int[NUM_CONFIG_INTS];
extern char *config_str[NUM_CONFIG_STRS];

extern void init_config(char *);
extern int get_config_item(svalue_t *, svalue_t *);
