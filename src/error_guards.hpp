#pragma once

#include "src/exceptions.hpp"

// Forward declaration to avoid circular includes
// The actual definition is in error_context.h
struct error_context_s;
typedef struct error_context_s error_context_t;

// C declarations - implemented in error_context.cpp
extern "C" {
int save_context(error_context_t *);
void pop_context(error_context_t *);
void restore_context(error_context_t *);
}

namespace neolith {

/**
 * @brief RAII guard for error boundary context management.
 *
 * Replaces manual save_context() / pop_context() pairing with automatic
 * scope-based cleanup. Captures VM state on construction and guarantees
 * context restoration and error-state clearing on destruction (via pop_context()).
 *
 * This ensures that even if an exception is thrown, the context stack
 * and error state are properly unwound without manual intervention.
 *
 * Example usage:
 * @code
 * error_context_t econ;
 * {
 *     error_boundary_guard guard(&econ);  // calls save_context(&econ)
 *     // protected code here
 * }  // guard destructor calls pop_context(&econ)
 * @endcode
 *
 * Invariant: exactly one pop_context() call per successful save_context(),
 * even if an exception is thrown during the guarded scope.
 */
class error_boundary_guard {
private:
    error_context_t *m_context;
    bool m_active;

public:
    /**
     * @brief Construct the guard and save the current execution context.
     *
     * @param econ Pointer to an error_context_t structure to fill in.
     *
     * Calls save_context(econ). If save_context returns 0 (max recursion depth),
     * this constructor throws a catchable_runtime_error to signal the failure.
     * In this case, m_active remains false and the destructor does nothing.
     */
    explicit error_boundary_guard(error_context_t *econ)
        : m_context(econ), m_active(false) {
        if (!econ) {
            throw noncatchable_runtime_limit("error_boundary_guard: null context");
        }
        // save_context returns 0 on failure (max recursion depth)
        if (save_context(econ) == 0) {
            throw catchable_runtime_error("*Too deep recursion");
        }
        m_active = true;
    }

    /**
     * @brief Destructor: pop the context and clear error state.
     *
     * This is declared noexcept to ensure it does not throw. If pop_context
     * itself were to fail, we would have a critical bug and should abort.
     *
     * Calls pop_context(m_context) to restore the previous error context
     * and clear error state flags.
     */
    ~error_boundary_guard() noexcept {
        if (m_active && m_context) {
            pop_context(m_context);
        }
    }

    // Not copyable or movable
    error_boundary_guard(const error_boundary_guard &) = delete;
    error_boundary_guard &operator=(const error_boundary_guard &) = delete;
    error_boundary_guard(error_boundary_guard &&) = delete;
    error_boundary_guard &operator=(error_boundary_guard &&) = delete;

    /**
     * @brief Manually restore VM state from the saved context.
     *
     * Called by catch handlers to restore the VM to the saved state before
     * handling the caught exception. This is distinct from destructor cleanup
     * (pop_context), which manages the context stack.
     */
    void restore() noexcept {
        if (m_context) {
            restore_context(m_context);
        }
    }

    /**
     * @brief Return the wrapped error context for direct access if needed.
     */
    error_context_t *context() noexcept {
        return m_context;
    }
};

/**
 * @brief RAII guard for error reentry protection.
 *
 * Replaces manual in_error and in_mudlib_error_handler flag toggling with
 * scoped guard objects that guarantee state restoration even if exceptions
 * are thrown.
 *
 * Use to wrap error handling code paths where recursion must be detected
 * and prevented.
 *
 * Example:
 * @code
 * {
 *     error_reentry_guard guard(error_reentry_guard::flag::IN_ERROR);
 *     // code that may recursively error
 * }  // guard destructor clears the flag
 * @endcode
 */
class error_reentry_guard {
public:
    enum class flag {
        IN_ERROR,                    /**< in_error flag (main error handler reentry) */
        IN_MUDLIB_ERROR_HANDLER,     /**< in_mudlib_error_handler flag */
    };

private:
    volatile int *m_flag_ref;
    int m_old_value;

public:
    /**
     * @brief Construct guard for a specific reentry flag.
     *
     * @param which The flag to protect (IN_ERROR or IN_MUDLIB_ERROR_HANDLER).
     *
     * If the flag is already set (indicating recursion), throws noncatchable_runtime_limit
     * to prevent infinite recursion. Otherwise, sets the flag and saves the old value.
     */
    explicit error_reentry_guard(flag which);

    /**
     * @brief Destructor: restore the flag to its previous state.
     *
     * Declared noexcept to guarantee flag cleanup even on exception paths.
     */
    ~error_reentry_guard() noexcept;

    // Not copyable or movable
    error_reentry_guard(const error_reentry_guard &) = delete;
    error_reentry_guard &operator=(const error_reentry_guard &) = delete;
    error_reentry_guard(error_reentry_guard &&) = delete;
    error_reentry_guard &operator=(error_reentry_guard &&) = delete;
};

/**
 * @brief RAII guard for catch_value lifecycle management.
 *
 * Owns the lifecycle of the global catch_value between error throws and catches.
 * Ensures the previous value is freed before a new value is assigned, and that
 * values are properly cleaned up on scope exit.
 *
 * In a migrated exception-based error handler, this replaces the manual
 * free_svalue(&catch_value) + set new value pattern.
 *
 * Example:
 * @code
 * {
 *     catch_value_guard guard;
 *     // previously: free_svalue(&catch_value); catch_value = new_value;
 *     guard.set_caught_error("*Error message");  // sets catch_value atomically
 * }  // guard destructor frees catch_value if it was set
 * @endcode
 */
class catch_value_guard {
private:
    bool m_owned;

public:
    /**
     * @brief Construct the guard (does not modify catch_value initially).
     */
    catch_value_guard() noexcept : m_owned(false) {}

    /**
     * @brief Destructor: free catch_value if we set it.
     *
     * Declared noexcept to prevent exceptions during cleanup.
     */
    ~catch_value_guard() noexcept;

    /**
     * @brief Set a new caught error value, freeing the old one first.
     *
     * @param error_msg The error message string to assign to catch_value.
     *
     * Frees any existing catch_value, then sets it to a new STRING_MALLOC
     * copy of error_msg. Marks this guard as the owner.
     */
    void set_caught_error(const char *error_msg) noexcept;

    /**
     * @brief Commit the caught value as the result of a successful catch().
     *
     * Releases ownership so the value persists when the guard is destroyed.
     * This is called after F_END_CATCH context to hand ownership to the
     * VM state rather than the guard.
     */
    void commit_success() noexcept {
        m_owned = false;
    }

    // Not copyable or movable
    catch_value_guard(const catch_value_guard &) = delete;
    catch_value_guard &operator=(const catch_value_guard &) = delete;
    catch_value_guard(catch_value_guard &&) = delete;
    catch_value_guard &operator=(catch_value_guard &&) = delete;
};

} // namespace neolith

// C-compatible wrappers used by C translation units.
extern "C" {

/**
 * @brief C-callable wrapper to check for reentry.
 *
 * Returns nonzero if currently in the error handler (in_error is set),
 * indicating we should not recursively call error_handler().
 */
int in_error_handler_already(void) noexcept;

/**
 * @brief C-callable wrapper to check for mudlib error handler reentry.
 */
int in_mudlib_error_handler_already(void) noexcept;

} // extern "C"
