#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

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
funptr_t *make_lfun_funp_by_name(const char *, svalue_t *);
funptr_t *make_simul_funp(int, svalue_t *);
funptr_t* make_functional_funp (int num_arg, int num_local, int len, svalue_t * args, int flag);

void dealloc_funp(funptr_t *);
void push_refed_funp(funptr_t *);

void push_undefined(void);
void pop_stack(void);

/*
 * Stack-slot wrappers.
 * Caller must pair *_SLOT_CALL() with CALL_FUNCTION_POINTER_SLOT_FINISH()
 * after consuming the return value.
 */
#define CALL_FUNCTION_POINTER_SLOT_CALL(funp, num_arg) \
    (push_undefined(), call_function_pointer_mode((funp), (num_arg), 1))
#define SAFE_CALL_FUNCTION_POINTER_SLOT_CALL(funp, num_arg) \
    (push_undefined(), safe_call_function_pointer_mode((funp), (num_arg), 1))
#define CALL_FUNCTION_POINTER_SLOT_FINISH() pop_stack()

/* Compatibility wrappers for existing non-slot call sites. */
#define CALL_FUNCTION_POINTER_CALL(funp, num_arg) \
    call_function_pointer_mode((funp), (num_arg), 0)
#define SAFE_CALL_FUNCTION_POINTER_CALL(funp, num_arg) \
    safe_call_function_pointer_mode((funp), (num_arg), 0)

/*
 * eoperators.c
 */
void push_funp(funptr_t *);
void free_funp(funptr_t *);

int merge_arg_lists(int, array_t *, int);
svalue_t *call_function_pointer_mode(funptr_t *, int, int);
svalue_t *call_function_pointer(funptr_t *, int);
svalue_t *call_function_pointer_with_slot(funptr_t *, int);
svalue_t *safe_call_function_pointer_mode(funptr_t *, int, int);
svalue_t *safe_call_function_pointer(funptr_t *, int);
svalue_t *safe_call_function_pointer_with_slot(funptr_t *, int);

#ifdef __cplusplus
}
#endif
