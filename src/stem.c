#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"
#include "rc.h"
#include "addr_resolver.h"

main_options_t* g_main_options = NULL;

int g_proceeding_shutdown = 0;
int g_exit_code = EXIT_SUCCESS;

int comp_flag = 0;		/* Trace compilations */
time_t boot_time = 0L;

int slow_shutdown_to_do = 0;

int init_stem (int debug_level, unsigned long trace_flags, const char* config_file)
{
    static main_options_t stem_opts;

#ifdef _WIN32
    _tzset ();
#else
    tzset ();
#endif
    current_time = boot_time = time(NULL);
    srand ((unsigned int)boot_time);

    stem_opts.epilog_level = 0;
    stem_opts.debug_level = debug_level;
    stem_opts.trace_flags = trace_flags;
    stem_opts.console_mode = 0;
    stem_opts.pedantic = 0;
    stem_opts.timer_flags = TIMER_FLAG_HEARTBEAT | TIMER_FLAG_CALLOUT | TIMER_FLAG_RESET;
    memset(stem_opts.config_file, 0, PATH_MAX);
    if (config_file)
        strncpy(stem_opts.config_file, config_file, PATH_MAX - 1);

    g_main_options = &stem_opts; /* this is required throughout the code*/
    return 0;
}

static int normalize_runtime_setting(int value)
{
    return value >= 0 ? value : 0;
}

void stem_get_addr_resolver_config(addr_resolver_config_t *config)
{
    if (!config)
        return;

    addr_resolver_config_init_defaults(config);
    config->forward_cache_ttl = normalize_runtime_setting(CONFIG_INT(__RESOLVER_FORWARD_CACHE_TTL__));
    config->reverse_cache_ttl = normalize_runtime_setting(CONFIG_INT(__RESOLVER_REVERSE_CACHE_TTL__));
    config->negative_cache_ttl = normalize_runtime_setting(CONFIG_INT(__RESOLVER_NEGATIVE_CACHE_TTL__));
    config->stale_refresh_window = normalize_runtime_setting(CONFIG_INT(__RESOLVER_STALE_REFRESH_WINDOW__));
    config->forward_quota = normalize_runtime_setting(CONFIG_INT(__RESOLVER_FORWARD_QUOTA__));
    config->reverse_quota = normalize_runtime_setting(CONFIG_INT(__RESOLVER_REVERSE_QUOTA__));
    config->refresh_quota = normalize_runtime_setting(CONFIG_INT(__RESOLVER_REFRESH_QUOTA__));
}
