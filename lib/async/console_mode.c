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

static DWORD console_input_mode_line(int echo) {
  DWORD mode = ENABLE_EXTENDED_FLAGS
               | ENABLE_QUICK_EDIT_MODE
               | ENABLE_PROCESSED_INPUT
               | ENABLE_LINE_INPUT;

  if (echo)
    mode |= ENABLE_ECHO_INPUT;

  return mode;
}

static DWORD console_input_mode_single_char(void) {
  return ENABLE_EXTENDED_FLAGS
         | ENABLE_QUICK_EDIT_MODE
         | ENABLE_PROCESSED_INPUT;
}

static int set_console_input_mode(console_worker_context_t *ctx, DWORD mode) {
  HANDLE handle;
  DWORD current_mode;

  if (ctx)
    {
      platform_mutex_lock(&ctx->state_mutex);
      ctx->desired_console_mode = mode;
      platform_mutex_unlock(&ctx->state_mutex);
    }

  if (!get_console_input_mode(&handle, &current_mode))
    return 0;

  if ((mode ^ current_mode) & ENABLE_LINE_INPUT)
    {
      /* Switching between line mode and single-char mode requires canceling any pending ReadConsole */
      debug_message("Switching console input mode, canceling pending input...\n");
#if _WIN32_WINNT > 0x0602
      CancelIoEx(handle, NULL);
#endif
    }

  if (!SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), mode))
    {
      debug_warn("SetConsoleMode failed for console stdin: %lu\n", GetLastError());
      return 0;
    }

  return 1;
}

/**
 * @brief Set the console input mode to the specified mode, preserving the original mode in the process.
 * FIXME: When switching from line mode (cooked mode) to single-character mode, we must *cancel* the
 * console line editing and discard any pending input. There is currently no official way to do this:
 * @see https://github.com/microsoft/terminal/issues/12143
 * @param mode The desired console input mode flags.
 */
int set_console_input_line_mode(console_worker_context_t *ctx, int echo) {
  return set_console_input_mode(ctx, console_input_mode_line(echo));
}

int set_console_input_echo(console_worker_context_t *ctx, int echo) {
  HANDLE handle;
  DWORD mode;

  if (!get_console_input_mode(&handle, &mode))
    return 0;

  if (echo)
    mode |= ENABLE_ECHO_INPUT;
  else
    mode &= ~ENABLE_ECHO_INPUT;

  return set_console_input_mode(ctx, mode);
}

int set_console_input_single_char(console_worker_context_t *ctx, int single) {
  if (!single)
    return set_console_input_line_mode(ctx, 1);

  return set_console_input_mode(ctx, console_input_mode_single_char());
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
      debug_warn("SetConsoleMode failed for console stdout: %lu\n", GetLastError());
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
