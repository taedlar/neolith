#ifndef MAIN_H
#define MAIN_H

#ifdef STDC_HEADERS
#include <limits.h>
#endif /* STDC_HEADERS */

#include "lpc/types.h"

#define PORT_TELNET      1
#define PORT_BINARY      2
#define PORT_ASCII       3

typedef struct {
    int kind;
    int port;
    int fd;
} port_def_t;

typedef struct server_options
{
  char config_file[PATH_MAX];
  int debug_level;
  unsigned long trace_flags;
} server_options_t;

extern server_options_t* g_svropts;
#define SERVER_OPTION(x)	(g_svropts->x)

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
