#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*  dynamic LPC memory allocations:
 *
 *  DXALLOC - allocation that never fails. Exits on failure.
 *  DMALLOC - generic allocation. Returns NULL on failure.
 *  DREALLOC - generic re-allocation. Returns NULL on failure.
 *  DCALLOC - generic cleared-allocation. Returns NULL on failure.
 *  FREE - free memory allocated by any of the above.
 */
extern char *reserved_area;
extern char *xalloc(size_t);

#define DXALLOC(x,tag,desc)     xalloc(x)
#define DMALLOC(x,tag,desc)     malloc(x)
#define DREALLOC(x,y,tag,desc)  realloc(x,y)
#define DCALLOC(x,y,tag,desc)   calloc(x,y)

#define FREE(x)         free(x)

#ifdef __cplusplus
}
#endif
