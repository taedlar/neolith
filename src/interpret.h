#pragma once

#include <setjmp.h>
#include "lpc/functional.h"

#define PUSH_STRING    (0 << 6)
#define PUSH_NUMBER    (1 << 6)
#define PUSH_GLOBAL    (2 << 6)
#define PUSH_LOCAL     (3 << 6)

#define PUSH_WHAT      (3 << 6)
#define PUSH_MASK      (0xff ^ (PUSH_WHAT))

#define SWITCH_CASE_SIZE ((int)(2 + sizeof(char *)))

#define APPLY_CACHE_SIZE (1 << APPLY_CACHE_BITS)

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
    short framekind;
    union {
	int table_index;
	funptr_t *funp;
    } fr;
    object_t *ob;		/* Current object */
    object_t *prev_ob;	/* Save previous object */
    program_t *prog;	/* Current program */
    int num_local_variables;	/* Local + arguments */
    char *pc;
    svalue_t *fp;
    int function_index_offset;	/* Used when executing functions in inherited
				 * programs */
    int variable_index_offset;	/* Same */
    short caller_type;		/* was this a locally called function? */
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

typedef struct error_context_s {
    jmp_buf context;
    control_stack_t *save_csp;
    object_t *save_command_giver; 
    svalue_t *save_sp;
    struct error_context_s *save_context;
} error_context_t;

/* for apply_master_ob */
#define MASTER_APPROVED(x) (((x)==(svalue_t *)-1) || ((x) && (((x)->type != T_NUMBER) || (x)->u.number))) 

#define IS_ZERO(x) (!(x) || (((x)->type == T_NUMBER) && ((x)->u.number == 0)))
#define IS_UNDEFINED(x) (!(x) || (((x)->type == T_NUMBER) && \
	((x)->subtype == T_UNDEFINED) && ((x)->u.number == 0)))

#define CHECK_TYPES(val, t, arg, inst) \
  if (!((val)->type & (t))) bad_argument(val, t, arg, inst);

/* Beek - add some sanity to joining strings */
/* add to an svalue */
#define EXTEND_SVALUE_STRING(x, y, z) do {\
	char *ess_res; \
	int ess_len; \
	int ess_r; \
	ess_len = (ess_r = SVALUE_STRLEN(x)) + strlen(y); \
	if ((x)->subtype == STRING_MALLOC && MSTR_REF((x)->u.string) == 1) { \
          ess_res = (char *) extend_string((x)->u.string, ess_len); \
          if (!ess_res) fatal("Out of memory!\n"); \
          strcpy(ess_res + ess_r, (y)); \
	} else { \
	  ess_res = new_string(ess_len, z); \
	  strcpy(ess_res, (x)->u.string); \
	  strcpy(ess_res + ess_r, (y)); \
	  free_string_svalue(x); \
	  (x)->subtype = STRING_MALLOC; \
	} \
	(x)->u.string = ess_res;\
	} while(0)

/* <something that needs no free> + string svalue */
#define SVALUE_STRING_ADD_LEFT(y, z) do {\
	char *pss_res; int pss_r; int pss_len; \
        pss_len = SVALUE_STRLEN(sp) + (pss_r = strlen(y)); \
        pss_res = new_string(pss_len, z); \
        strcpy(pss_res, y); \
        strcpy(pss_res + pss_r, sp->u.string); \
        free_string_svalue(sp--); \
	sp->type = T_STRING; \
        sp->u.string = pss_res; \
        sp->subtype = STRING_MALLOC; \
	} while(0)

/* basically, string + string; faster than using extend b/c of SVALUE_STRLEN */
#define SVALUE_STRING_JOIN(x, y, z) do {\
	char *ssj_res; int ssj_r; int ssj_len; \
        ssj_r = SVALUE_STRLEN(x); \
        ssj_len = ssj_r + SVALUE_STRLEN(y); \
        if ((x)->subtype == STRING_MALLOC && MSTR_REF((x)->u.string) == 1) { \
            ssj_res = (char *) extend_string((x)->u.string, ssj_len); \
            if (!ssj_res) fatal("Out of memory!\n"); \
            (void) strcpy(ssj_res + ssj_r, (y)->u.string); \
            free_string_svalue(y); \
        } else { \
            ssj_res = (char *) new_string(ssj_len, z); \
	    strcpy(ssj_res, (x)->u.string); \
	    strcpy(ssj_res + ssj_r, (y)->u.string); \
	    free_string_svalue(y); \
            free_string_svalue(x); \
            (x)->subtype = STRING_MALLOC; \
        } \
        (x)->u.string = ssj_res; \
	} while(0)

/* macro calls */
#define call_program(prog, offset) \
        eval_instruction ((prog)->program + (offset))

#define free_svalue(x,y) int_free_svalue(x)

#define push_svalue(x) assign_svalue_no_free(++sp, x)

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
	sp->u.string = make_shared_string(x);\
	} while(0)

#define put_malloced_string(x) do {\
	sp->type = T_STRING;\
	sp->subtype = STRING_MALLOC;\
	sp->u.string = (x);\
	} while(0)

#define put_array(x) do {\
	sp->type = T_ARRAY;\
	sp->u.arr = (x);\
	} while(0)

#define put_shared_string(x) do {\
	sp->type = T_STRING;\
	sp->subtype = STRING_SHARED;\
	sp->u.string = (x);\
	} while(0)

extern program_t *current_prog;
extern short caller_type;
extern char *pc;
extern svalue_t *sp;
extern svalue_t *fp;
extern svalue_t catch_value;
extern control_stack_t* control_stack;
extern control_stack_t *csp;
extern int function_index_offset;
extern int variable_index_offset;
extern unsigned int apply_low_call_others;
extern unsigned int apply_low_cache_hits;
extern unsigned int apply_low_slots_used;
extern unsigned int apply_low_collisions;
extern int function_index_offset;
extern int simul_efun_is_loading;
extern program_t fake_prog;
extern svalue_t global_lvalue_byte;
extern int num_varargs;

void eval_instruction(char *p);
void assign_svalue(svalue_t *, svalue_t *);
void assign_svalue_no_free(svalue_t *, svalue_t *);
void copy_some_svalues(svalue_t *, svalue_t *, int);
void transfer_push_some_svalues(svalue_t *, int);
void push_some_svalues(svalue_t *, int);
void int_free_svalue(svalue_t *);
void free_string_svalue(svalue_t *);
void free_some_svalues(svalue_t *, int);
void push_object(object_t *);
void push_number(int);
void push_real(double);
void push_undefined(void);
void copy_and_push_string(char *);
void share_and_push_string(char *);
void push_array(array_t *);
void push_refed_array(array_t *);
void push_buffer(buffer_t *);
void push_refed_buffer(buffer_t *);
void push_mapping(mapping_t *);
void push_refed_mapping(mapping_t *);
void push_class(array_t *);
void push_refed_class(array_t *);
void push_malloced_string(char *);
void push_shared_string(char *);
void push_constant_string(char *);
void pop_stack(void);
void pop_n_elems(int);
void pop_2_elems(void);
void pop_3_elems(void);
compiler_function_t *setup_inherited_frame(int);
char *function_name(program_t *, int);
void remove_object_from_stack(object_t *);
void setup_fake_frame(funptr_t *);
void remove_fake_frame(void);
int merge_arg_lists(int, array_t *, int);
void push_indexed_lvalue(int);

void process_efun_callback(int, function_to_call_t *, int);
svalue_t *call_efun_callback(function_to_call_t *, int);
char *type_name(int c);
void bad_arg(int, int) NO_RETURN;
void bad_argument(svalue_t *, int, int, int) NO_RETURN;
void check_for_destr(array_t *);
int is_static(char *, object_t *);
int apply_low(char *, object_t *, int);
svalue_t *apply(char *, object_t *, int, int);
svalue_t *call_function_pointer(funptr_t *, int);
svalue_t *safe_call_function_pointer(funptr_t *, int);
svalue_t *safe_apply(char *, object_t *, int, int);
void call___INIT(object_t *);
array_t *call_all_other(array_t *, char *, int);
char *function_exists(char *, object_t *, int);
void call_function(program_t *, int);
svalue_t *apply_master_ob(char *, int);
svalue_t *safe_apply_master_ob(char *, int);
void init_master(char *);
void mark_apply_low_cache(void);
int translate_absolute_line(int, unsigned short *, size_t, int *, int *);
char *add_slash(char *);
int strpref(char *, char *);
void do_trace(char *, char *, char *);

void opcdump(char *);
int inter_sscanf(svalue_t *, svalue_t *, svalue_t *, int);

extern void reset_machine (void);
void unlink_string_svalue(svalue_t *);
void copy_lvalue_range(svalue_t *);
void assign_lvalue_range(svalue_t *);

#ifdef LAZY_RESETS
void try_reset(object_t *);
#endif

void pop_context(error_context_t *);
void restore_context(error_context_t *);
int save_context(error_context_t *);

void pop_control_stack(void);
compiler_function_t *setup_new_frame(int);
void push_control_stack(int);

void break_point(void);


extern int get_error_state (int mask);
extern void set_error_state (int flag);

#define	ES_STACK_FULL		(1 << 0)	/* svalue stack or control stack is full */
#define ES_MAX_EVAL_COST	(1 << 1)	/* eval cost exceeded */
