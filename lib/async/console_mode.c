/**
 * @file console_mode.c
 * @brief Shared console mode helpers.
 */

#include "console_mode.h"

#include "port/debug.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static int get_console_input_mode(HANDLE *handle, DWORD *mode) {
  *handle = GetStdHandle(STD_INPUT_HANDLE);
  if (*handle == INVALID_HANDLE_VALUE || *handle == NULL)
    return 0;

  if (!GetConsoleMode(*handle, mode))
    return 0;

  return 1;
}

static int set_console_input_mode(DWORD mode) {
  HANDLE handle;
  DWORD current_mode;

  if (!get_console_input_mode(&handle, &current_mode))
    return 0;

  if (!SetConsoleMode(handle, mode))
    {
      debug_warn("SetConsoleMode failed for console stdin: %lu\n", GetLastError());
      return 0;
    }

  return 1;
}

int set_console_input_line_mode(int echo) {
  DWORD mode = ENABLE_EXTENDED_FLAGS
               | ENABLE_QUICK_EDIT_MODE
               | ENABLE_PROCESSED_INPUT
               | ENABLE_LINE_INPUT;

  if (echo)
    mode |= ENABLE_ECHO_INPUT;

  return set_console_input_mode(mode);
}

int set_console_input_echo(int echo) {
  HANDLE handle;
  DWORD mode;

  if (!get_console_input_mode(&handle, &mode))
    return 0;

  if (echo)
    mode |= ENABLE_ECHO_INPUT;
  else
    mode &= ~ENABLE_ECHO_INPUT;

  if (!SetConsoleMode(handle, mode))
    {
      debug_warn("SetConsoleMode failed for console stdin echo: %lu\n", GetLastError());
      return 0;
    }

  return 1;
}

int set_console_input_single_char(int single) {
  if (!single)
    return set_console_input_line_mode(1);

  return set_console_input_mode(ENABLE_EXTENDED_FLAGS
                                | ENABLE_QUICK_EDIT_MODE
                                | ENABLE_PROCESSED_INPUT);
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
#else
int set_console_input_line_mode(int echo) {
  (void)echo;
  return 0;
}

int set_console_input_echo(int echo) {
  (void)echo;
  return 0;
}

int set_console_input_single_char(int single) {
  (void)single;
  return 0;
}

int enable_console_output_ansi(void) {
  return 0;
}
#endif