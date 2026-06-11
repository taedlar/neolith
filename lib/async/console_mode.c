/**
 * @file console_mode.c
 * @brief Shared console mode helpers.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#define NO_STEM
#include "src/std.h"
#include "console_mode.h"
#include "console_worker.h"

#include "port/debug.h"

#ifdef _WIN32

static int get_console_input_mode(HANDLE *handle, DWORD *mode) {
  *handle = GetStdHandle(STD_INPUT_HANDLE);
  if (*handle == INVALID_HANDLE_VALUE || *handle == NULL)
    return 0;

  if (!GetConsoleMode(*handle, mode))
    return 0;

  return 1;
}

/**
 * @brief Apply a bit-delta to the current console input mode.
 *
 * Reads the current mode, sets @p set_bits and clears @p clear_bits, then
 * writes the result back.  Only the bits that actually need to change are
 * touched, so unrelated flags set by the terminal (e.g. ENABLE_QUICK_EDIT_MODE
 * under Windows Terminal / ConPTY) are preserved, avoiding ERROR_INVALID_PARAMETER.
 */
static int set_console_input_mode(console_worker_context_t *ctx,
                                  DWORD set_bits, DWORD clear_bits) {
  HANDLE handle;
  DWORD current_mode;
  DWORD new_mode;

  if (!get_console_input_mode(&handle, &current_mode))
    return 0;

  new_mode = (current_mode | set_bits) & ~clear_bits;

  if (ctx)
    {
      platform_mutex_lock(&ctx->state_mutex);
      ctx->desired_console_mode = new_mode;
      platform_mutex_unlock(&ctx->state_mutex);
    }

  if ((new_mode ^ current_mode) & ENABLE_LINE_INPUT)
    {
      /* Switching between line mode and single-char mode requires canceling any pending ReadConsole */
      LOG_TRACE ("Switching console input mode, canceling pending input...\n");
#if _WIN32_WINNT > 0x0602
      CancelIoEx(handle, NULL);
#endif
    }

  if (!SetConsoleMode(handle, new_mode))
    {
      LOG_WARN("SetConsoleMode failed for console stdin: %lu\n", GetLastError());
      return 0;
    }

  return 1;
}

/**
 * @brief Switch to line (cooked) input mode, optionally enabling echo.
 */
int set_console_input_line_mode(console_worker_context_t *ctx, int echo) {
  DWORD set_bits = ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT
                   | (echo ? ENABLE_ECHO_INPUT : 0);
  DWORD clear_bits = (echo ? 0 : ENABLE_ECHO_INPUT) | ENABLE_VIRTUAL_TERMINAL_INPUT;
  return set_console_input_mode(ctx, set_bits, clear_bits);
}

int set_console_input_echo(console_worker_context_t *ctx, int echo) {
  DWORD set_bits = echo ? ENABLE_ECHO_INPUT : 0;
  DWORD clear_bits = echo ? 0 : ENABLE_ECHO_INPUT;
  return set_console_input_mode(ctx, set_bits, clear_bits);
}

/**
 * @brief Switch to single-character (raw) input mode, disabling line input and echo.
 * 
 * Also enables Windows 10 ANSI processing to allow reading of virtual terminal sequences for special keys.
 * @see https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences
 */
int set_console_input_single_char(console_worker_context_t *ctx, int single) {
  if (!single)
    return set_console_input_line_mode(ctx, 1);

  return set_console_input_mode(ctx,
                                ENABLE_PROCESSED_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT,
                                ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
}

int enable_console_output_ansi(void) {
  HANDLE handle;
  DWORD mode;

  handle = GetStdHandle(STD_OUTPUT_HANDLE);
  if (handle == INVALID_HANDLE_VALUE || handle == NULL)
    return 0;

  if (!GetConsoleMode(handle, &mode))
    return 0;

  SetConsoleOutputCP(CP_UTF8);
  mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT;
  if (!SetConsoleMode(handle, mode))
    {
      LOG_WARN("SetConsoleMode failed for console stdout: %lu\n", GetLastError());
      return 0;
    }

  return 1;
}
#else /* Non-Windows (POSIX): console is a plain file, no input mode manipulation needed */
int set_console_input_line_mode(console_worker_context_t *ctx, int echo) {
  (void)ctx;
  (void)echo;
  return 0;
}

int set_console_input_echo(console_worker_context_t *ctx, int echo) {
  (void)ctx;
  (void)echo;
  return 0;
}

int set_console_input_single_char(console_worker_context_t *ctx, int single) {
  (void)ctx;
  (void)single;
  return 0;
}

int enable_console_output_ansi(void) {
  return 0;
}
#endif
