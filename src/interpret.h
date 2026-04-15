#pragma once

#include "lpc/svalue.h"
#include "apply.h"

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
        char *str;
    } f;
    int narg;
    svalue_t *args;
} function_to_call_t;

#define IS_ZERO(x) (!(x) || (((x)->type == T_NUMBER) && ((x)->u.number == 0)))
#define IS_UNDEFINED(x) (!(x) || (((x)->type == T_NUMBER) && \
        ((x)->subtype == T_UNDEFINED) && ((x)->u.number == 0)))

#define CHECK_TYPES(val, t, arg, inst) \
  if (!((val)->type & (t))) bad_argument(val, t, arg, inst);

/* Append a byte span to an svalue string using explicit source length. */
#define EXTEND_SVALUE_STRING_LEN(target_sv, src_bytes, src_len, alloc_tag) do {\
                malloc_str_t ess_res; size_t ess_len; size_t ess_r; \
                const char *ess_src = (src_bytes); size_t ess_src_len = (src_len); \
                ess_len = (ess_r = SVALUE_STRLEN(target_sv)) + ess_src_len; \
                if ((target_sv)->subtype == STRING_MALLOC && MSTR_REF((target_sv)->u.malloc_string) == 1) { \
                        ess_res = extend_string((target_sv)->u.malloc_string, ess_len); \
                        if (!ess_res) fatal("Out of memory!\n"); \
                        memcpy(ess_res + ess_r, ess_src, ess_src_len); \
                        ess_res[ess_len] = '\0'; \
                } else { \
                        ess_res = new_string(ess_len, alloc_tag); \
                        memcpy(ess_res, (target_sv)->u.string, ess_r); \
                        memcpy(ess_res + ess_r, ess_src, ess_src_len); \
                        ess_res[ess_len] = '\0'; \
                        free_string_svalue(target_sv); \
                        (target_sv)->subtype = STRING_MALLOC; \
                } \
                (target_sv)->u.malloc_string = ess_res;\
        } while(0)

/* Compatibility wrapper for NUL-terminated callers. */
#define EXTEND_SVALUE_STRING(target_sv, cstr_src, alloc_tag) \
        EXTEND_SVALUE_STRING_LEN((target_sv), (cstr_src), strlen(cstr_src), (alloc_tag))

/* Prepend a byte span to the stack-top string value using explicit length. */
#define SVALUE_STRING_ADD_LEFT_LEN(prefix_bytes, prefix_len, alloc_tag) do {\
                malloc_str_t pss_res; size_t pss_r; size_t pss_len; \
                const char *pss_src = (prefix_bytes); \
                pss_len = SVALUE_STRLEN(sp) + (pss_r = (prefix_len)); \
                pss_res = new_string(pss_len, alloc_tag); \
                memcpy(pss_res, pss_src, pss_r); \
                memcpy(pss_res + pss_r, sp->u.string, SVALUE_STRLEN(sp)); \
                pss_res[pss_len] = '\0'; \
                free_string_svalue(sp--); \
                sp->type = T_STRING; \
                sp->subtype = STRING_MALLOC; \
                sp->u.malloc_string = pss_res; \
        } while(0)

/* Compatibility wrapper for NUL-terminated callers. */
#define SVALUE_STRING_ADD_LEFT(prefix_cstr, alloc_tag) \
        SVALUE_STRING_ADD_LEFT_LEN((prefix_cstr), strlen(prefix_cstr), (alloc_tag))

/* Join two svalue strings via counted lengths instead of C-string scans. */
#define SVALUE_STRING_JOIN(left_sv, right_sv, alloc_tag) do {\
                malloc_str_t ssj_res; size_t ssj_r; size_t ssj_len; \
                ssj_r = SVALUE_STRLEN(left_sv); \
                ssj_len = ssj_r + SVALUE_STRLEN(right_sv); \
                if ((left_sv)->subtype == STRING_MALLOC && MSTR_REF((left_sv)->u.malloc_string) == 1) { \
                        ssj_res = extend_string((left_sv)->u.malloc_string, ssj_len); \
                        if (!ssj_res) fatal("Out of memory!\n"); \
                        memcpy(ssj_res + ssj_r, (right_sv)->u.string, SVALUE_STRLEN(right_sv)); \
                        ssj_res[ssj_len] = '\0'; \
                        free_string_svalue(right_sv); \
                } else { \
                        ssj_res = new_string(ssj_len, alloc_tag); \
                        memcpy(ssj_res, (left_sv)->u.string, ssj_r); \
                        memcpy(ssj_res + ssj_r, (right_sv)->u.string, SVALUE_STRLEN(right_sv)); \
                        ssj_res[ssj_len] = '\0'; \
                        free_string_svalue(right_sv); \
                        free_string_svalue(left_sv); \
                        (left_sv)->subtype = STRING_MALLOC; \
                } \
                (left_sv)->u.malloc_string = ssj_res; \
        } while(0)

#define STACK_CHECK(n)		do {\
        if (sp + n >= end_of_stack) \
          { set_error_state(ES_STACK_FULL); error("***Stack overflow!"); } \
        } while (0)

/* macro calls */
#define call_program(prog, offset) eval_instruction ((prog)->program + (offset))

#define push_svalue(x) do{++sp;assign_svalue_no_free(sp, (x));}while(0)

#define put_number(x) do {\
        sp->type = T_NUMBER;\
        sp->subtype = 0;\
        sp->u.number = (x);\
        } while(0)

#define put_buffer(x) do {\
        sp->type = T_BUFFER;\
        sp->u.buf = (x);\
        } while(0)

#define put_undested_object(x) do {\
        sp->type = T_OBJECT;\
        sp->u.ob = (x);\
        } while(0)

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
#define put_constant_string(x) do {\
        sp->type = T_STRING;\
        sp->subtype = STRING_SHARED;\
        sp->u.shared_string = make_shared_string(x, NULL);\
        } while(0)

#define put_malloced_string(x) do {\
        sp->type = T_STRING;\
        sp->subtype = STRING_MALLOC;\
        sp->u.malloc_string = (x);\
        } while(0)

#define put_array(x) do {\
        sp->type = T_ARRAY;\
        sp->u.arr = (x);\
        } while(0)

#define put_shared_string(x) do {\
        sp->type = T_STRING;\
        sp->subtype = STRING_SHARED;\
        sp->u.shared_string = (x);\
        } while(0)

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
