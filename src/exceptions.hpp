#pragma once

#include <exception>
#include <stdexcept>
#include <string>

namespace neolith {

/**
 * @brief Base class for all LPC runtime errors that can be thrown.
 *
 * Derived classes categorize the error by catchability and routing:
 * - catchable_runtime_error: trapped by LPC catch() and routed to master error_handler()
 * - noncatchable_runtime_limit: escapes catch boundaries (eval cost, stack depth)
 * - fatal_runtime_error: driver-level fatal failures, routed to master crash()
 */
class driver_runtime_error : public std::exception {
protected:
    std::string m_message;

public:
    explicit driver_runtime_error(const std::string &msg) : m_message(msg) {}
    explicit driver_runtime_error(const char *msg) : m_message(msg ? msg : "") {}

    virtual ~driver_runtime_error() noexcept = default;

    const char *what() const noexcept override {
        return m_message.c_str();
    }

    const std::string &message() const noexcept {
        return m_message;
    }
};

/**
 * @brief Catchable runtime error.
 *
 * Errors of this type are trapped by LPC catch() blocks and routed through
 * the master error_handler() apply with the error message and context mapping.
 * If no catch block exists, the error propagates to the backend error handler.
 *
 * LPC callers see these as error strings caught by catch().
 */
class catchable_runtime_error : public driver_runtime_error {
public:
    explicit catchable_runtime_error(const std::string &msg) : driver_runtime_error(msg) {}
    explicit catchable_runtime_error(const char *msg) : driver_runtime_error(msg) {}
    virtual ~catchable_runtime_error() noexcept = default;
};

/**
 * @brief Non-catchable resource limit error.
 *
 * Errors of this type escape catch() boundaries and propagate directly to
 * the backend error handler. These represent resource exhaustion (eval cost,
 * stack depth) where continuing execution would be unsafe.
 *
 * Master error_handler() is NOT called for these errors; they bypass catch entirely.
 */
class noncatchable_runtime_limit : public driver_runtime_error {
public:
    explicit noncatchable_runtime_limit(const std::string &msg) : driver_runtime_error(msg) {}
    explicit noncatchable_runtime_limit(const char *msg) : driver_runtime_error(msg) {}
    virtual ~noncatchable_runtime_limit() noexcept = default;
};

/**
 * @brief Fatal driver-level error.
 *
 * Errors of this type represent unrecoverable driver failures (startup,
 * signal crashes, integrity violations). They are routed to master crash()
 * apply and then terminate the process.
 *
 * These errors must never escape a driver boundary without terminating.
 */
class fatal_runtime_error : public driver_runtime_error {
public:
    explicit fatal_runtime_error(const std::string &msg) : driver_runtime_error(msg) {}
    explicit fatal_runtime_error(const char *msg) : driver_runtime_error(msg) {}
    virtual ~fatal_runtime_error() noexcept = default;
};

} // namespace neolith

/**
 * @brief C-compatible exception throwable check function.
 *
 * Used in extern "C" contexts to determine if a caught exception is a
 * driver runtime error (C++ exception) that needs special handling.
 *
 * Use this in C ABI wrappers that translate C++ exceptions to driver
 * error states before returning to C callers.
 */
extern "C" {
bool is_runtime_error(const std::exception *ex) noexcept;
} // extern "C"
