#ifndef MACROS_H
#define MACROS_H

#include "malloc.h"

#define ALLOCATE(type, tag, desc) ((type *)DXALLOC(sizeof(type), tag, desc))
#define CALLOCATE(num, type, tag, desc) ((type *)DXALLOC(sizeof(type[1]) * (num), tag, desc))
#define RESIZE(ptr, num, type, tag, desc) ((type *)DREALLOC((void *)ptr, sizeof(type) * (num), tag, desc))

#define IF_DEBUG(x) 
#define DEBUG_CHECK(x, y)
#define DEBUG_CHECK1(x, y, a)
#define DEBUG_CHECK2(x, y, a, b)

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
#define INT_32			int

#define COPY_FLOAT(x, y)	COPY4(x,y)
#define LOAD_FLOAT(x, y)	LOAD4(x,y)
#define STORE_FLOAT(x, y)	STORE4(x,y)

#define COPY_PTR(x, y)	COPY4(x,y)
#define LOAD_PTR(x, y)	LOAD4(x,y)
#define STORE_PTR(x, y)	STORE4(x,y)

#define POINTER_INT		int
#define INS_POINTER		ins_int

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
