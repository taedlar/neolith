#pragma once

#ifdef HAVE_STDBOOL_H
  #include <stdbool.h>
#elif !defined(__cplusplus)
  typedef int bool;
  #define true 1
  #define false 0
#endif
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Expand environment variable references in a string.
 *
 * Supports three forms:
 * - `$VAR`         — expands to the value of VAR; empty string if unset.
 * - `${VAR}`       — same as `$VAR`.
 * - `${VAR:-default}` — expands to the value of VAR if set and non-empty,
 *                    otherwise expands to the literal text `default`.
 *
 * Variable names must match `[A-Za-z_][A-Za-z0-9_]*`.  A `$` that is not
 * followed by a valid variable name start character or `{` is emitted as a
 * literal `$`.  An unterminated `${` is emitted as a literal `$` and parsing
 * continues from the `{`.
 *
 * @param src      Input string containing variable references.
 * @param out      Caller-provided output buffer.
 * @param out_size Size of @p out in bytes.
 * @return true on success, false if any argument is NULL / zero, or if the
 *         expanded result does not fit in @p out.
 */
bool envsubst(const char *src, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif
