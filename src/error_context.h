#pragma once

#include "interpret.h"
#include <setjmp.h>

// Phase 2: C++ exception classes for error handling (outside extern "C")
#ifdef __cplusplus
#include "exceptions.hpp"
#include "error_guards.hpp"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct error_context_s {
    jmp_buf context;
    control_stack_t *save_csp;
    object_t *save_command_giver; 
    svalue_t *save_sp;
    struct error_context_s *save_context;
} error_context_t;

#define NULL_ERROR_CONTEXT       0
#define NORMAL_ERROR_CONTEXT     1
#define CATCH_ERROR_CONTEXT      2
#define SAFE_APPLY_ERROR_CONTEXT 4

/* longjmp() contexts for native C error handling */
void pop_context(error_context_t *);
void restore_context(error_context_t *);
int save_context(error_context_t *);

/* LPC error handling */
extern svalue_t catch_value;

void error_handler(const char *) NO_RETURN;
void error(const char *, ...) NO_RETURN;
void throw_error(void) NO_RETURN;

/* stock error throwing function */
const char *type_name(int c);
void bad_arg(int, int) NO_RETURN;
void bad_argument(svalue_t *, int, int, int) NO_RETURN;

/* Phase 2: C++ exception-based error dispatcher (defined in error_context.cpp)
 * This is called from error_handler() C wrapper to dispatch typed exceptions.
 * It throws one of: catchable_runtime_error, fatal_runtime_error */
void error_handler_cpp(const char *err);

#ifdef __cplusplus
}
#endif
