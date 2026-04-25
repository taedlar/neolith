#pragma once

#include "lpc/svalue.h"
#include "apply.h"
#include "simulate.h"

#define PUSH_STRING    (0 << 6)
#define PUSH_NUMBER    (1 << 6)
#define PUSH_GLOBAL    (2 << 6)
#define PUSH_LOCAL     (3 << 6)

#define PUSH_WHAT      (3 << 6)
#define PUSH_MASK      (0xff ^ (PUSH_WHAT))

#define SWITCH_CASE_SIZE ((int)(2 + sizeof(char *)))

#define EXTRACT_UCHAR(p) (*(unsigned char *)(p))

/*
 * Control stack element.
 * 'prog' is usually same as 'ob->prog' (current_object), except when
 * when the current function is defined by inheritance.
 * The pointer, csp, will point to the values that will be used at return.
 */
#define FRAME_FUNCTION     0
#define FRAME_FUNP         1
#define FRAME_CATCH        2
#define FRAME_FAKE         3
#define FRAME_MASK         3

#define FRAME_OB_CHANGE    4
#define FRAME_EXTERNAL     8

typedef struct {
    int framekind;            /* see above FRAME_**/
    union {
        int table_index;
        funptr_t *funp;
    } fr;
    object_t *ob;               /* Current object */
    object_t *prev_ob;          /* Save previous object */
    program_t *prog;            /* Current program */
    int num_local_variables;    /* Local + arguments */
    const char *pc;             /* Program counter for LPC opcodes */
    svalue_t *fp;               /* Frame pointer */
    int function_index_offset;  /* Used when executing functions in inherited programs */
    int variable_index_offset;  /* Same */
    int caller_type;          /* was this a locally called function? */
} control_stack_t;

typedef struct {
    object_t *ob;
    union {
        funptr_t *fp;
        const char *str;
    } f;
    int narg;
    svalue_t *args;
} function_to_call_t;

#ifdef __cplusplus
extern "C" {
#endif

extern svalue_t *start_of_stack;
extern svalue_t *end_of_stack;
extern control_stack_t* control_stack;

extern program_t *current_prog;
extern int caller_type;
extern const char *pc;
extern svalue_t *sp;
extern svalue_t *fp;
extern control_stack_t *csp;
extern int function_index_offset;
extern int variable_index_offset;
extern int st_num_arg;

extern svalue_t const0;
extern svalue_t const1;
extern svalue_t const0u;
extern int num_varargs;

/* LPC interpreter */
void eval_instruction(const char *p);

void call_function (program_t *progp, int runtime_index, int num_args, svalue_t *ret_value);

void call_efun(int);
void process_efun_callback(int, function_to_call_t *, int);
svalue_t *call_efun_callback(function_to_call_t *, int);
void call_efun_callback_finish(function_to_call_t *);
#ifndef NO_SHADOWS
int is_static(const char *, object_t *);
#endif

#define	ES_STACK_FULL		(1 << 0)	/* svalue stack or control stack is full */
#define ES_MAX_EVAL_COST	(1 << 1)	/* eval cost exceeded */
int get_error_state (int mask);
void set_error_state (int flag);
void clear_error_state ();

void reset_interpreter (void);

/* stack manipulation */
void transfer_push_some_svalues(svalue_t *, int);
void push_some_svalues(svalue_t *, int);
void push_object(object_t *);
void push_number(int64_t);
void push_real(double);
void push_undefined(void);
void push_undefineds (int num);
void copy_and_push_string(const char *);
void share_and_push_string(const char *);
void push_array(array_t *);
void push_refed_array(array_t *);
void push_buffer(buffer_t *);
void push_refed_buffer(buffer_t *);
void push_mapping(mapping_t *);
void push_refed_mapping(mapping_t *);
void push_class(array_t *);
void push_refed_class(array_t *);
void push_malloced_string(malloc_str_t);
void push_shared_string(shared_str_t);
void push_constant_string(const char *);
void pop_stack(void);
void pop_n_elems(size_t);
void pop_2_elems(void);
void pop_3_elems(void);

void remove_object_from_stack(object_t *);

void free_string_svalue(svalue_t *);
void unlink_string_svalue(svalue_t *);

#ifdef __cplusplus
}
#endif

#define IS_ZERO(x) (!(x) || (((x)->type == T_NUMBER) && ((x)->u.number == 0)))
#define IS_UNDEFINED(x) (!(x) || (((x)->type == T_NUMBER) && \
        ((x)->subtype == T_UNDEFINED) && ((x)->u.number == 0)))

#define CHECK_TYPES(val, t, arg, inst) \
  if (!((val)->type & (t))) bad_argument(val, t, arg, inst);

/* Append a byte span to an svalue string using explicit source length. */
static inline void extend_svalue_string_len_impl(svalue_t *target_sv,
                                                 const char *src_bytes,
                                                 size_t src_len,
                                                 const char *alloc_tag) {
        malloc_str_t ess_res;
        size_t ess_len;
        size_t ess_r;

        (void)alloc_tag; /* Allocation tag kept for API compatibility. */
        ess_len = (ess_r = SVALUE_STRLEN(target_sv)) + src_len;

        if (target_sv->subtype == STRING_MALLOC && MSTR_REF(target_sv->u.malloc_string) == 1) {
                ess_res = int_extend_string(to_malloc_str(target_sv->u.malloc_string), ess_len);
                if (!ess_res) {
                        fatal("Out of memory!\n");
                }
                memcpy(ess_res + ess_r, src_bytes, src_len);
                ess_res[ess_len] = '\0';
        } else {
                ess_res = int_new_string(ess_len);
                memcpy(ess_res, SVALUE_STRPTR(target_sv), ess_r);
                memcpy(ess_res + ess_r, src_bytes, src_len);
                ess_res[ess_len] = '\0';
                free_string_svalue(target_sv);
                target_sv->subtype = STRING_MALLOC;
        }
        target_sv->u.malloc_string = ess_res;
}

#define EXTEND_SVALUE_STRING_LEN(target_sv, src_bytes, src_len, alloc_tag) \
        extend_svalue_string_len_impl((target_sv), (src_bytes), (src_len), (alloc_tag))

/* Compatibility wrapper for NUL-terminated callers. */
#define EXTEND_SVALUE_STRING(target_sv, cstr_src, alloc_tag) \
        EXTEND_SVALUE_STRING_LEN((target_sv), (cstr_src), strlen(cstr_src), (alloc_tag))

/* Prepend a byte span to the stack-top string value using explicit length. */
static inline void svalue_string_add_left_len_impl(svalue_t **sp_ref,
                                                   const char *prefix_bytes,
                                                   size_t prefix_len,
                                                   const char *alloc_tag) {
        malloc_str_t pss_res;
        size_t pss_len;

        (void)alloc_tag; /* Allocation tag kept for API compatibility. */
        pss_len = SVALUE_STRLEN(*sp_ref) + prefix_len;
        pss_res = int_new_string(pss_len);
        memcpy(pss_res, prefix_bytes, prefix_len);
        memcpy(pss_res + prefix_len, SVALUE_STRPTR(*sp_ref), SVALUE_STRLEN(*sp_ref));
        pss_res[pss_len] = '\0';

        free_string_svalue((*sp_ref)--);
        (*sp_ref)->type = T_STRING;
        (*sp_ref)->subtype = STRING_MALLOC;
        (*sp_ref)->u.malloc_string = pss_res;
}

#define SVALUE_STRING_ADD_LEFT_LEN(prefix_bytes, prefix_len, alloc_tag) \
        svalue_string_add_left_len_impl((&sp), (prefix_bytes), (prefix_len), (alloc_tag))

/* Compatibility wrapper for NUL-terminated callers. */
#define SVALUE_STRING_ADD_LEFT(prefix_cstr, alloc_tag) \
        SVALUE_STRING_ADD_LEFT_LEN((prefix_cstr), strlen(prefix_cstr), (alloc_tag))

/* Join two svalue strings via counted lengths instead of C-string scans. */
static inline void svalue_string_join_impl(svalue_t *left_sv,
                                           svalue_t *right_sv,
                                           const char *alloc_tag) {
        malloc_str_t ssj_res;
        size_t ssj_r;
        size_t ssj_len;

        (void)alloc_tag; /* Allocation tag kept for API compatibility. */
        ssj_r = SVALUE_STRLEN(left_sv);
        ssj_len = ssj_r + SVALUE_STRLEN(right_sv);

        if (left_sv->subtype == STRING_MALLOC && MSTR_REF(left_sv->u.malloc_string) == 1) {
                ssj_res = int_extend_string(to_malloc_str(left_sv->u.malloc_string), ssj_len);
                if (!ssj_res) {
                        fatal("Out of memory!\n");
                }
                memcpy(ssj_res + ssj_r, SVALUE_STRPTR(right_sv), SVALUE_STRLEN(right_sv));
                ssj_res[ssj_len] = '\0';
                free_string_svalue(right_sv);
        } else {
                ssj_res = int_new_string(ssj_len);
                memcpy(ssj_res, SVALUE_STRPTR(left_sv), ssj_r);
                memcpy(ssj_res + ssj_r, SVALUE_STRPTR(right_sv), SVALUE_STRLEN(right_sv));
                ssj_res[ssj_len] = '\0';
                free_string_svalue(right_sv);
                free_string_svalue(left_sv);
                left_sv->subtype = STRING_MALLOC;
        }
        left_sv->u.malloc_string = ssj_res;
}

#define SVALUE_STRING_JOIN(left_sv, right_sv, alloc_tag) \
        svalue_string_join_impl((left_sv), (right_sv), (alloc_tag))

#define STACK_CHECK(n)		do {\
        if (sp + n >= end_of_stack) \
          { set_error_state(ES_STACK_FULL); error("***Stack overflow!"); } \
        } while (0)

/* macro calls */
#define call_program(prog, offset) eval_instruction ((prog)->program + (offset))

static inline void push_svalue_impl(svalue_t **sp_ref, const svalue_t *value) {
        ++(*sp_ref);
        assign_svalue_no_free(*sp_ref, value);
}

#define push_svalue(x) \
        push_svalue_impl((&sp), (x))

static inline void put_number_impl(svalue_t *target_sp, int64_t value) {
        target_sp->type = T_NUMBER;
        target_sp->subtype = 0;
        target_sp->u.number = value;
}

#define put_number(x) \
        put_number_impl(sp, (x))

static inline void put_buffer_impl(svalue_t *target_sp, buffer_t *value) {
        target_sp->type = T_BUFFER;
        target_sp->u.buf = value;
}

#define put_buffer(x) \
        put_buffer_impl(sp, (x))

static inline void put_undested_object_impl(svalue_t *target_sp, object_t *ob) {
        target_sp->type = T_OBJECT;
        target_sp->u.ob = ob;
}

#define put_undested_object(x) \
        put_undested_object_impl(sp, (x))

#define put_object(x) do {\
        if ((x)->flags & O_DESTRUCTED) put_number(0); \
        else put_undested_object(x);\
        } while(0)

#define put_unrefed_undested_object(x, y) {\
        sp->type = T_OBJECT;\
        sp->u.ob = (x);\
        add_ref((x), y);\
        } while(0)

#define put_unrefed_object(x,y) do {\
        if ((x)->flags & O_DESTRUCTED)\
        put_number(0);\
        else put_unrefed_undested_object(x,y);\
        } while(0)

/* see comments on push_constant_string */
static inline void put_constant_string_impl(svalue_t *target_sp, const char *value) {
        target_sp->type = T_STRING;
        target_sp->subtype = STRING_SHARED;
        target_sp->u.shared_string = make_shared_string(value, NULL);
}

#define put_constant_string(x) \
        put_constant_string_impl(sp, (x))

#ifdef STRING_TYPE_SAFETY
static inline void check_put_malloced_string_value_impl(malloc_str_t value) {
        if (value && !is_malloc_string_payload(value)) {
                fatal("put_malloced_string: contract violation: shared string passed to malloc-string boundary\n");
        }
}

static inline void check_put_shared_string_value_impl(shared_str_t value) {
        if (value && !is_shared_string_payload(value)) {
                fatal("put_shared_string: contract violation: non-shared string passed to shared-string boundary\n");
        }
}

#define CHECK_PUT_MALLOCED_STRING_VALUE(v) \
        check_put_malloced_string_value_impl((v))
#define CHECK_PUT_SHARED_STRING_VALUE(v) \
        check_put_shared_string_value_impl((v))
#else
#define CHECK_PUT_MALLOCED_STRING_VALUE(v) do { (void)(v); } while(0)
#define CHECK_PUT_SHARED_STRING_VALUE(v) do { (void)(v); } while(0)
#endif

static inline void put_malloced_string_impl(svalue_t *target_sp, malloc_str_t value) {
        CHECK_PUT_MALLOCED_STRING_VALUE(value);
        target_sp->type = T_STRING;
        target_sp->subtype = STRING_MALLOC;
        target_sp->u.malloc_string = value;
}

#define put_malloced_string(x) \
        put_malloced_string_impl(sp, (x))

static inline void put_array_impl(svalue_t *target_sp, array_t *value) {
        target_sp->type = T_ARRAY;
        target_sp->u.arr = value;
}

#define put_array(x) \
        put_array_impl(sp, (x))

static inline void put_shared_string_impl(svalue_t *target_sp, shared_str_t value) {
        CHECK_PUT_SHARED_STRING_VALUE(value);
        target_sp->type = T_STRING;
        target_sp->subtype = STRING_SHARED;
        target_sp->u.shared_string = value;
}

#define put_shared_string(x) \
        put_shared_string_impl(sp, (x))
