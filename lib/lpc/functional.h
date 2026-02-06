#pragma once
#include "types.h"

/* FP_LOCAL */
typedef struct {
    function_index_t index;
} local_ptr_t;

/* FP_SIMUL */
typedef local_ptr_t simul_ptr_t;

/* FP_EFUN */
typedef local_ptr_t efun_ptr_t;

/* FP_FUNCTIONAL */
typedef struct {
    /* these two must come first */
    unsigned char num_arg;
    unsigned char num_local;
    short offset;
    program_t *prog;
    short fio, vio;
} functional_t;

/* common header */
typedef struct {
    unsigned short ref;
    short type;                 /* FP_* is used */
    struct object_s *owner;
    struct array_s *args;
} funptr_hdr_t;

struct funptr_s {
    funptr_hdr_t hdr;
    union {
        efun_ptr_t efun;
        local_ptr_t local;
        simul_ptr_t simul;
        functional_t functional;
    } f;
};

typedef union {
    funptr_t *f;
    char *s;
} string_or_func_t;

extern program_t fake_prog;

funptr_t *make_efun_funp(int, svalue_t *);
funptr_t *make_lfun_funp(int, svalue_t *);
funptr_t *make_simul_funp(int, svalue_t *);
funptr_t* make_functional_funp (int num_arg, int num_local, int len, svalue_t * args, int flag);

void dealloc_funp(funptr_t *);
void push_refed_funp(funptr_t *);

/*
 * eoperators.c
 */
void push_funp(funptr_t *);
void free_funp(funptr_t *);

int merge_arg_lists(int, array_t *, int);
svalue_t *call_function_pointer(funptr_t *, int);
svalue_t *safe_call_function_pointer(funptr_t *, int);
