/**
 * @file console_mode.h
 * @brief Shared console mode helpers.
 */

#ifndef CONSOLE_MODE_H
#define CONSOLE_MODE_H

typedef struct console_worker_context_s console_worker_context_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enable cooked line-input mode for the process console stdin.
 * @param ctx Optional console worker context for desired-mode tracking.
 * @param echo Non-zero to enable local echo, zero to disable it.
 * @returns 1 if stdin is a real console and the mode was updated, otherwise 0.
 */
int set_console_input_line_mode(console_worker_context_t* ctx, int echo);

/**
 * @brief Toggle local echo for the current console stdin mode.
 * @param ctx Optional console worker context for desired-mode tracking.
 * @param echo Non-zero to enable local echo, zero to disable it.
 * @returns 1 if stdin is a real console and the mode was updated, otherwise 0.
 */
int set_console_input_echo(console_worker_context_t* ctx, int echo);

/**
 * @brief Toggle single-character mode for the process console stdin.
 * @param ctx Optional console worker context for desired-mode tracking.
 * @param single Non-zero to enable single-character mode, zero to restore line mode.
 * @returns 1 if stdin is a real console and the mode was updated, otherwise 0.
 */
int set_console_input_single_char(console_worker_context_t* ctx, int single);

/**
 * @brief Enable ANSI virtual terminal processing for the process console stdout.
 * @returns 1 if stdout is a real console and the mode was updated, otherwise 0.
 */
int enable_console_output_ansi(void);

#ifdef __cplusplus
}
#endif

#endif /* CONSOLE_MODE_H */
