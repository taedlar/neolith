#pragma once

#include <assert.h>
#include "logger/logger.h"

#ifdef __cplusplus
extern "C" {
#endif

enum debug_severity {
    DEBUG_SEVERITY_FATAL = 10,
    DEBUG_SEVERITY_ERROR = 9,
    DEBUG_SEVERITY_WARN = 8,
    DEBUG_SEVERITY_INFO = 7, /* -d1 */
    DEBUG_SEVERITY_NOTICE = 6, /* -d2 */
    DEBUG_SEVERITY_VERBOSE = 5, /* -d3 */
    DEBUG_SEVERITY_TRACE = 0 /* -t */
};

/* generic loggers */
#define LOG_FATAL(...)    do{if(DEBUG_SEVERITY_FATAL >= current_log_severity) \
   debug_message(__VA_ARGS__);}while(0)
#define debug_fatal(...)    do{if(DEBUG_SEVERITY_FATAL >= current_log_severity) \
   debug_log_with_src("FATAL", __func__, __FILE__, __LINE__, __VA_ARGS__);}while(0)

#define LOG_ERROR(...)    do{if(DEBUG_SEVERITY_ERROR >= current_log_severity) \
   debug_message(__VA_ARGS__);}while(0)
#define debug_error(...)    do{if(DEBUG_SEVERITY_ERROR >= current_log_severity) \
   debug_log_with_src("ERROR", __func__, __FILE__, __LINE__, __VA_ARGS__);}while(0)

#define LOG_WARN(...)     do{if(DEBUG_SEVERITY_WARN >= current_log_severity) \
   debug_message(__VA_ARGS__);}while(0)
#define debug_warn(...)     do{if(DEBUG_SEVERITY_WARN >= current_log_severity) \
   debug_log_with_src("WARN", __func__, __FILE__, __LINE__, __VA_ARGS__);}while(0)

#define LOG_INFO(...)     do{if(DEBUG_SEVERITY_INFO >= current_log_severity) \
   debug_message(__VA_ARGS__);}while(0)
#define debug_info(...)   do{if(DEBUG_SEVERITY_INFO >= current_log_severity) \
   debug_log_with_src("INFO", __func__, __FILE__, __LINE__, __VA_ARGS__);}while(0)

#define LOG_NOTICE(...)     do{if(DEBUG_SEVERITY_NOTICE >= current_log_severity) \
   debug_message(__VA_ARGS__);}while(0)
#define debug_notice(...)     do{if(DEBUG_SEVERITY_NOTICE >= current_log_severity) \
   debug_log_with_src("NOTICE", __func__, __FILE__, __LINE__, __VA_ARGS__);}while(0)

#define LOG_TRACE(...)       do{if(DEBUG_SEVERITY_TRACE >= current_log_severity) \
   debug_message(__VA_ARGS__);}while(0)
#define debug_trace(...)       do{if(DEBUG_SEVERITY_TRACE >= current_log_severity) \
   debug_log_with_src("TRACE", __func__, __FILE__, __LINE__, __VA_ARGS__);}while(0)

#define debug_perror(what,file)  debug_perror_with_src(__func__, __FILE__, __LINE__, (what), (file))

#define IF_DEBUG(x) x
#define DEBUG_CHECK(x, y)		if(x) opt_error(1,"%s",(y))
#define DEBUG_CHECK1(x, y, a)		if(x) opt_error(1,(y),(a))
#define DEBUG_CHECK2(x, y, a, b)	if(x) opt_error(1,(y),(a),(b))

#ifdef __cplusplus
}
#endif
