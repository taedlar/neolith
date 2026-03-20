/**
 * @file console_mode.h
 * @brief Shared console mode helpers.
 */

#ifndef CONSOLE_MODE_H
#define CONSOLE_MODE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enable cooked line-input mode for the process console stdin.
 * @param echo Non-zero to enable local echo, zero to disable it.
 * @returns 1 if stdin is a real console and the mode was updated, otherwise 0.
 */
int set_console_input_line_mode(int echo);

/**
 * @brief Toggle local echo for the current console stdin mode.
 * @param echo Non-zero to enable local echo, zero to disable it.
 * @returns 1 if stdin is a real console and the mode was updated, otherwise 0.
 */
int set_console_input_echo(int echo);

/**
 * @brief Toggle single-character mode for the process console stdin.
 * @param single Non-zero to enable single-character mode, zero to restore line mode.
 * @returns 1 if stdin is a real console and the mode was updated, otherwise 0.
 */
int set_console_input_single_char(int single);

/**
 * @brief Enable ANSI virtual terminal processing for the process console stdout.
 * @returns 1 if stdout is a real console and the mode was updated, otherwise 0.
 */
int enable_console_output_ansi(void);

#ifdef __cplusplus
}
#endif

#endif /* CONSOLE_MODE_H */