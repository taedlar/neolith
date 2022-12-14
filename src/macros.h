#ifndef MACROS_H
#define MACROS_H

#ifdef STDC_HEADERS
#include <stdint.h>
#endif /* STDC_HEADERS */

#include "main.h"
#include "malloc.h"
#include "logger.h"

#define ALLOCATE(type, tag, desc) ((type *)DXALLOC(sizeof(type), tag, desc))
#define CALLOCATE(num, type, tag, desc) ((type *)DXALLOC(sizeof(type[1]) * (num), tag, desc))
#define RESIZE(ptr, num, type, tag, desc) ((type *)DREALLOC((void *)ptr, sizeof(type) * (num), tag, desc))

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
#define opt_trace(tier, ...)		do{if(((SERVER_OPTION(trace_flags)&(tier)&~07)==((tier)&~07)) && ((SERVER_OPTION(trace_flags)&07) >= ((tier)&07))) \
					debug_message_with_src("TRACE", __func__, __FILE__, __LINE__, ## __VA_ARGS__);}while(0)
#define TT_EVAL		010
#define TT_COMPILE	020
#define TT_SIMUL_EFUN	040
#define TT_BACKEND	0100
#endif /* using C99 */

#define debug_perror(what,file)		debug_perror_with_src(__func__, __FILE__, __LINE__, (what), (file))

#define IF_DEBUG(x) 			x
#define DEBUG_CHECK(x, y)		if(x) opt_error(1,"%s",(y))
#define DEBUG_CHECK1(x, y, a)		if(x) opt_error(1,(y),(a))
#define DEBUG_CHECK2(x, y, a, b)	if(x) opt_error(1,(y),(a),(b))

#define COPY2(x, y)      ((char *)(x))[0] = ((char *)(y))[0]; \
                         ((char *)(x))[1] = ((char *)(y))[1]
#define LOAD2(x, y)      ((char *)&(x))[0] = *y++; \
                         ((char *)&(x))[1] = *y++
#define STORE2(x, y)     *x++ = ((char *)(&(y)))[0]; \
                         *x++ = ((char *)(&(y)))[1]

#define COPY4(x, y)      ((char *)(x))[0] = ((char *)(y))[0]; \
                         ((char *)(x))[1] = ((char *)(y))[1]; \
                         ((char *)(x))[2] = ((char *)(y))[2]; \
                         ((char *)(x))[3] = ((char *)(y))[3]
#define LOAD4(x, y)      ((char *)&(x))[0] = *y++; \
                         ((char *)&(x))[1] = *y++; \
                         ((char *)&(x))[2] = *y++; \
                         ((char *)&(x))[3] = *y++
#define STORE4(x, y)     *x++ = ((char *)(&(y)))[0]; \
                         *x++ = ((char *)(&(y)))[1]; \
                         *x++ = ((char *)(&(y)))[2]; \
                         *x++ = ((char *)(&(y)))[3]

#define COPY8(x, y)      ((char *)(x))[0] = ((char *)(y))[0]; \
                         ((char *)(x))[1] = ((char *)(y))[1]; \
                         ((char *)(x))[2] = ((char *)(y))[2]; \
                         ((char *)(x))[3] = ((char *)(y))[3]; \
                         ((char *)(x))[4] = ((char *)(y))[4]; \
                         ((char *)(x))[5] = ((char *)(y))[5]; \
                         ((char *)(x))[6] = ((char *)(y))[6]; \
                         ((char *)(x))[7] = ((char *)(y))[7]
#define LOAD8(x, y)      ((char *)&(x))[0] = *y++; \
                         ((char *)&(x))[1] = *y++; \
                         ((char *)&(x))[2] = *y++; \
                         ((char *)&(x))[3] = *y++; \
                         ((char *)&(x))[4] = *y++; \
                         ((char *)&(x))[5] = *y++; \
                         ((char *)&(x))[6] = *y++; \
                         ((char *)&(x))[7] = *y++;
#define STORE8(x, y)     *x++ = ((char *)(&(y)))[0]; \
                         *x++ = ((char *)(&(y)))[1]; \
                         *x++ = ((char *)(&(y)))[2]; \
                         *x++ = ((char *)(&(y)))[3]; \
                         *x++ = ((char *)(&(y)))[4]; \
                         *x++ = ((char *)(&(y)))[5]; \
                         *x++ = ((char *)(&(y)))[6]; \
                         *x++ = ((char *)(&(y)))[7]

#define COPY_SHORT(x, y)	COPY2(x,y)
#define LOAD_SHORT(x, y)	LOAD2(x,y)
#define STORE_SHORT(x, y)	STORE2(x,y)

#define COPY_INT(x, y)		COPY4(x,y)
#define LOAD_INT(x, y)		LOAD4(x,y)
#define STORE_INT(x, y)		STORE4(x,y)
#define INT_32			int32_t

#define COPY_FLOAT(x, y)	COPY4(x,y)
#define LOAD_FLOAT(x, y)	LOAD4(x,y)
#define STORE_FLOAT(x, y)	STORE4(x,y)

#if UINTPTR_MAX == UINT32_MAX
#define COPY_PTR(x, y)		COPY4(x,y)
#define LOAD_PTR(x, y)		LOAD4(x,y)
#define STORE_PTR(x, y)		STORE4(x,y)
#elif UINTPTR_MAX == UINT64_MAX
#define COPY_PTR(x, y)		COPY8(x,y)
#define LOAD_PTR(x, y)		LOAD8(x,y)
#define STORE_PTR(x, y)		STORE8(x,y)
#else
#error only supports pointer size of 4 or 8 bytes
#endif

#define POINTER_INT		intptr_t
#define INS_POINTER		ins_intptr

#ifndef _FUNC_SPEC_
extern char *xalloc(int);
extern char *int_string_copy(char *);
extern char *int_string_unlink(char *);
extern char *int_new_string(int);
extern char *int_alloc_cstring(char *);
#endif	/* _FUNC_SPEC_ */

#define string_copy(x,y) int_string_copy(x)
#define string_unlink(x,y) int_string_unlink(x)
#define new_string(x,y) int_new_string(x)
#define alloc_cstring(x,y) int_alloc_cstring(x)

#endif
