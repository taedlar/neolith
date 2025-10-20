#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"

server_options_t* g_svropts = NULL;

int slow_shut_down_to_do = 0;
int g_proceeding_shutdown = 0;

int t_flag = 0;			/* Disable heart beat and reset */
int comp_flag = 0;		/* Trace compilations */
int boot_time;
char *reserved_area;		/* reserved for MALLOC() */

svalue_t const0, const1, const0u;

/* -1 indicates that we have never had a master object.  This is so the
 * simul_efun object can load before the master. */
object_t *master_ob = 0;
