#pragma once
#include "interpret.h"

#define NULL_ERROR_CONTEXT       0
#define NORMAL_ERROR_CONTEXT     1
#define CATCH_ERROR_CONTEXT      2
#define SAFE_APPLY_ERROR_CONTEXT 4

void pop_context(error_context_t *);
void restore_context(error_context_t *);
int save_context(error_context_t *);

void throw_error(void) NO_RETURN;
void error_handler(const char *) NO_RETURN;
void error(const char *, ...) NO_RETURN;
