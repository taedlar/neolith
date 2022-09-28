/*  $Id: main.h,v 1.2 2002/11/25 11:11:05 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */

#ifndef MAIN_H
#define MAIN_H

#ifdef	STDC_HEADERS
#include <stdio.h>
#include <stdarg.h>
#endif	/* STDC_HEADERS */

#include "_lpc/types.h"

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

extern int log_message (const char* file, const char *fmt, ...);
extern int debug_message (char *, ...);
extern int debug_perror(char* what, char* file);

#endif	/* ! MAIN_H */
