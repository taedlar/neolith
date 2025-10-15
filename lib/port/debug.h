#pragma once

#include "logger.h"

#if __STDC_VERSION__ >= 199901L
/* generic loggers */
#define debug_fatal(...)		debug_message_with_src("FATAL", __func__, __FILE__, __LINE__, __VA_ARGS__)
#define debug_error(...)		debug_message_with_src("ERROR", __func__, __FILE__, __LINE__, __VA_ARGS__)
#define debug_warn(...)			debug_message_with_src("WARN", __func__, __FILE__, __LINE__, __VA_ARGS__)
#define debug_info(...)			debug_message_with_src("INFO", __func__, __FILE__, __LINE__, __VA_ARGS__)
#define debug_trace(...)		debug_message_with_src("TRACE", __func__, __FILE__, __LINE__, __VA_ARGS__)

#define opt_error(level, ...)		do{if(SERVER_OPTION(debug_level)>=(level)) \
                                        debug_message_with_src("ERROR", __func__, __FILE__, __LINE__, ## __VA_ARGS__);}while(0)
#define opt_warn(level, ...)		do{if(SERVER_OPTION(debug_level)>=(level)) \
                                        debug_message_with_src("WARN", __func__, __FILE__, __LINE__, ## __VA_ARGS__);}while(0)
#define opt_info(level, ...)		do{if(SERVER_OPTION(debug_level)>=(level)) \
                                        debug_message_with_src("INFO", __func__, __FILE__, __LINE__, ## __VA_ARGS__);}while(0)
/* trace loggers */
#define opt_trace(tier, ...)		do{if(((SERVER_OPTION(trace_flags)&(tier)&~0170)==((tier)&~0170)) && ((SERVER_OPTION(trace_flags)&0170) >= ((tier)&0170))) \
                                        debug_message_with_src("TRACE", __func__, __FILE__, __LINE__, ## __VA_ARGS__);}while(0)
#define TT_EVAL		0010
#define TT_COMPILE	0020
#define TT_SIMUL_EFUN	0040
#define TT_BACKEND	0100
#endif /* using C99 */

#define debug_perror(what,file)		debug_perror_with_src(__func__, __FILE__, __LINE__, (what), (file))

#define IF_DEBUG(x) 			x
#define DEBUG_CHECK(x, y)		if(x) opt_error(1,"%s",(y))
#define DEBUG_CHECK1(x, y, a)		if(x) opt_error(1,(y),(a))
#define DEBUG_CHECK2(x, y, a, b)	if(x) opt_error(1,(y),(a),(b))
