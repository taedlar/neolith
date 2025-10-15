#pragma once

/* portable wrappers for bytecode manipulation */
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
