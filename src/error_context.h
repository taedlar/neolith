#pragma once

#include "interpret.h"

// Exception-based error handling context definitions
#ifdef __cplusplus
#include "exceptions.hpp"
#include "error_guards.hpp"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct error_context_s {
    control_stack_t *save_csp;
    object_t *save_command_giver; 
    svalue_t *save_sp;
    struct error_context_s *save_context;
} error_context_t;

#define NULL_ERROR_CONTEXT       0
#define NORMAL_ERROR_CONTEXT     1
#define CATCH_ERROR_CONTEXT      2
#define SAFE_APPLY_ERROR_CONTEXT 4

/* Exception-based error handling context management */
void pop_context(error_context_t *);
void restore_context(error_context_t *);
int save_context(error_context_t *);

/* LPC error handling (non-returning via exception propagation). */
extern svalue_t catch_value;

void error_handler(const char *) NO_RETURN;   /* Throws runtime/fatal exception based on context. */
void error(const char *, ...) NO_RETURN;      /* Formats then forwards to error_handler(). */
void throw_error(void) NO_RETURN;             /* Throws catchable runtime exception or calls error(). */

/* Stock argument/type error helpers (non-returning via error() exception path). */
const char *type_name(int c);
void bad_arg(int, int) NO_RETURN;
void bad_argument(svalue_t *, int, int, int) NO_RETURN;

#ifdef __cplusplus
}
#endif
