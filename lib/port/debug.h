#pragma once

#include <assert.h>
#include "logger/logger.h"

#ifdef __cplusplus
extern "C" {
#endif

enum debug_severity {
    DEBUG_SEVERITY_FATAL = 4,
    DEBUG_SEVERITY_ERROR = 3,
    DEBUG_SEVERITY_WARN = 2,
    DEBUG_SEVERITY_NOTICE = 1,
    DEBUG_SEVERITY_TRACE = 0
};

/* generic loggers */
#define LOG_FATAL(fmt, ...)    do{if(DEBUG_SEVERITY_FATAL >= current_log_severity) \
   debug_message(fmt, ##__VA_ARGS__);}while(0)
#define debug_fatal(fmt, ...)    do{if(DEBUG_SEVERITY_FATAL >= current_log_severity) \
   debug_log_with_src("FATAL", __func__, __FILE__, __LINE__, fmt, ##__VA_ARGS__);}while(0)

#define LOG_ERROR(fmt, ...)    do{if(DEBUG_SEVERITY_ERROR >= current_log_severity) \
   debug_message(fmt, ##__VA_ARGS__);}while(0)
#define debug_error(fmt, ...)    do{if(DEBUG_SEVERITY_ERROR >= current_log_severity) \
   debug_log_with_src("ERROR", __func__, __FILE__, __LINE__, fmt, ##__VA_ARGS__);}while(0)

#define LOG_WARN(fmt, ...)     do{if(DEBUG_SEVERITY_WARN >= current_log_severity) \
   debug_message(fmt, ##__VA_ARGS__);}while(0)
#define debug_warn(fmt, ...)     do{if(DEBUG_SEVERITY_WARN >= current_log_severity) \
   debug_log_with_src("WARN", __func__, __FILE__, __LINE__, fmt, ##__VA_ARGS__);}while(0)

#define LOG_NOTICE(fmt, ...)     do{if(DEBUG_SEVERITY_NOTICE >= current_log_severity) \
   debug_message(fmt, ##__VA_ARGS__);}while(0)
#define debug_notice(fmt, ...)     do{if(DEBUG_SEVERITY_NOTICE >= current_log_severity) \
   debug_log_with_src("NOTICE", __func__, __FILE__, __LINE__, fmt, ##__VA_ARGS__);}while(0)

#define LOG_TRACE(fmt, ...)	   do{if(DEBUG_SEVERITY_TRACE >= current_log_severity) \
   debug_message(fmt, ##__VA_ARGS__);}while(0)
#define debug_trace(fmt, ...)	   do{if(DEBUG_SEVERITY_TRACE >= current_log_severity) \
   debug_log_with_src("TRACE", __func__, __FILE__, __LINE__, fmt, ##__VA_ARGS__);}while(0)

#define debug_perror(what,file)  debug_perror_with_src(__func__, __FILE__, __LINE__, (what), (file))

#define IF_DEBUG(x) x
#define DEBUG_CHECK(x, y)		if(x) opt_error(1,"%s",(y))
#define DEBUG_CHECK1(x, y, a)		if(x) opt_error(1,(y),(a))
#define DEBUG_CHECK2(x, y, a, b)	if(x) opt_error(1,(y),(a),(b))

#ifdef __cplusplus
}
#endif
