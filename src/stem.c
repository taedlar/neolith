#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"

main_options_t* g_main_options = NULL;

int g_proceeding_shutdown = 0;

int t_flag = 0;			/* Disable heart beat and reset */
int comp_flag = 0;		/* Trace compilations */
time_t boot_time = 0L;

int slow_shut_down_to_do = 0;

int init_stem (int debug_level, unsigned long trace_flags, const char* config_file)
{
    static main_options_t stem_opts;

    tzset ();
    current_time = boot_time = time(NULL);
    srand (boot_time);

    stem_opts.debug_level = debug_level;
    stem_opts.trace_flags = trace_flags;
    memset(stem_opts.config_file, 0, PATH_MAX);
    if (config_file)
        strncpy(stem_opts.config_file, config_file, PATH_MAX - 1);

    g_main_options = &stem_opts; /* this is required throughout the code*/
    return 0;
}
