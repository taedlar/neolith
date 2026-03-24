#pragma once

/*#define MALLOC(x)       malloc(x)*/
#define FREE(x)         free(x)
/*#define REALLOC(x,y)    realloc(x,y)*/
/*#define CALLOC(x,y)     calloc(x,y)*/

#define DXALLOC(x,tag,desc)     xalloc(x)
#define DMALLOC(x,tag,desc)     malloc(x)
#define DREALLOC(x,y,tag,desc)  realloc(x,y)
#define DCALLOC(x,y,tag,desc)   calloc(x,y)
