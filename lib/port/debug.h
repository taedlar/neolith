#pragma once

#include "logger/logger.h"

/* generic loggers */
#define debug_fatal(...)		debug_message_with_src("FATAL", __func__, __FILE__, __LINE__, __VA_ARGS__)
#define debug_error(...)		debug_message_with_src("ERROR", __func__, __FILE__, __LINE__, __VA_ARGS__)
#define debug_warn(...)			debug_message_with_src("WARN", __func__, __FILE__, __LINE__, __VA_ARGS__)
#define debug_info(...)			debug_message_with_src("INFO", __func__, __FILE__, __LINE__, __VA_ARGS__)
#define debug_trace(...)		debug_message_with_src("TRACE", __func__, __FILE__, __LINE__, __VA_ARGS__)

#define opt_error(level, ...)		do{if(MAIN_OPTION(debug_level)>=(level)) \
                                        debug_message_with_src("ERROR", __func__, __FILE__, __LINE__, ## __VA_ARGS__);}while(0)
#define opt_warn(level, ...)		do{if(MAIN_OPTION(debug_level)>=(level)) \
                                        debug_message_with_src("WARN", __func__, __FILE__, __LINE__, ## __VA_ARGS__);}while(0)
#define opt_info(level, ...)		do{if(MAIN_OPTION(debug_level)>=(level)) \
                                        debug_message_with_src("INFO", __func__, __FILE__, __LINE__, ## __VA_ARGS__);}while(0)

/*
 * Trace levels and tiers are represented as octal values (starting with '0').
 * The lowest 3 bits (0-7) represent the required trace level minus one.
 * The higher bits represent the trace tiers (EVAL, COMPILE, SIMUL_EFUN, BACKEND).
 * 
 * For example:
 * -t 020 enables COMPILE traces at level 0.
 * -t 0107 enables all BACKEND traces (at level 7 and below).
 * -t 052 enables EVAL and SIMUL_EFUN traces at level 2 and below. 
 */
#define TT_LEVEL_MASK   0007U    /* trace level mask (lowest 3 bits) */
#define TT_EVAL         0010U
#define TT_COMPILE      0020U
#define TT_SIMUL_EFUN   0040U
#define TT_BACKEND      0100U

/**
 * @brief The trace logger macro generates trace log messages based on the tier specification
 *        and the current trace flags set in the server options (specified with -t when starting
 *        the server).
 * @param tier The trace \p tier is a bit-OR'ed value for this trace log:
 *             TT_EVAL = Evaluation related traces.
 *             TT_COMPILE = Compilation related traces.
 *             TT_SIMUL_EFUN = Simul_efun related traces.
 *             TT_BACKEND = Backend related traces.
 *             The lowest 3 bits specify the minimum level of tracing required
 *             to log the message.
 * @param ... The format string and additional arguments for the log message.
 */
#define opt_trace(tier, ...) do{if((MAIN_OPTION(trace_flags)&(tier)&~TT_LEVEL_MASK) \
   && ((MAIN_OPTION(trace_flags)&TT_LEVEL_MASK) + 1) > ((tier)&TT_LEVEL_MASK)) \
   debug_message_with_src("TRACE", __func__, __FILE__, __LINE__, ## __VA_ARGS__);}while(0)

#define debug_perror(what,file) debug_perror_with_src(__func__, __FILE__, __LINE__, (what), (file))

#define IF_DEBUG(x) 			x
#define DEBUG_CHECK(x, y)		if(x) opt_error(1,"%s",(y))
#define DEBUG_CHECK1(x, y, a)		if(x) opt_error(1,(y),(a))
#define DEBUG_CHECK2(x, y, a, b)	if(x) opt_error(1,(y),(a),(b))
