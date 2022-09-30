#ifndef MAIN_H
#define MAIN_H

#include "lpc/types.h"
#include "logger.h"

#define PORT_TELNET      1
#define PORT_BINARY      2
#define PORT_ASCII       3

typedef struct {
    int kind;
    int port;
    int fd;
} port_def_t;

extern int g_proceeding_shutdown;
extern port_def_t external_port[5];
extern int t_flag;
extern int comp_flag;
extern int boot_time;
extern char *reserved_area;
extern svalue_t const0;
extern svalue_t const1;
extern svalue_t const0u;
extern int st_num_arg;
extern int slow_shut_down_to_do;
extern object_t *master_ob;

#endif	/* ! MAIN_H */
