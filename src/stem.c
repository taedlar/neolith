#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"

server_options_t* g_svropts = NULL;

int g_proceeding_shutdown = 0;

int t_flag = 0;			/* Disable heart beat and reset */
int comp_flag = 0;		/* Trace compilations */
time_t boot_time = 0L;

int slow_shut_down_to_do = 0;
