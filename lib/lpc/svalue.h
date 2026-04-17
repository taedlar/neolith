#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif
void free_svalue (svalue_t * v, const char* caller);
void free_some_svalues(svalue_t *, int);
void assign_svalue(svalue_t *, svalue_t *);
void assign_svalue_no_free(svalue_t *, const svalue_t *);
void copy_some_svalues(svalue_t *, svalue_t *, int);

int string_length_differs(const svalue_t *x, const svalue_t *y);

#ifdef __cplusplus
} // extern "C"

/*
 * C++ RAII wrappers for svalue management.
 * These provide safe ownership semantics without modifying the C API.
 */

namespace lpc {

class svalue_view;
class const_svalue_view;
class svalue_ref;

/**
 * @brief Non-owning reference to a reference-counted svalue.
 *
 * For svalues with reference-counted types (T_STRING, T_OBJECT, T_ARRAY, etc.),
 * this reference automatically increments the refcount on copy/move and decrements
 * on destruction.
 *
 * Use this for passing reference-counted svalues safely without full ownership.
 * Both copy and move semantics are supported.
 *
 * Example:
 *   svalue_t sv = ... // reference-counted svalue
 *   svalue_ref ref(&sv);
 *   // refcount is incremented
 *   // refcount is decremented when ref is destroyed
 */
class svalue_ref {
public:
    /**
     * Create a non-owning reference to an svalue.
     * If the svalue is reference-counted, increments the refcount.
     */
    explicit svalue_ref(svalue_t *sv) noexcept;

    // Move semantics
    svalue_ref(svalue_ref &&other) noexcept;
    svalue_ref &operator=(svalue_ref &&other) noexcept;

    // Copy semantics (both copies hold references)
    svalue_ref(const svalue_ref &other) noexcept;
    svalue_ref &operator=(const svalue_ref &other) noexcept;

    // Destructor
    ~svalue_ref() noexcept;

    // Accessors
    [[nodiscard]] svalue_view view() noexcept;
    [[nodiscard]] const_svalue_view view() const noexcept;

    svalue_t *get() noexcept;
    const svalue_t *get() const noexcept;

    svalue_t *operator->() noexcept;
    const svalue_t *operator->() const noexcept;

    svalue_t &operator*() noexcept;
    const svalue_t &operator*() const noexcept;

    explicit operator bool() const noexcept;

    /**
     * Release ownership of the reference.
     * The refcount is NOT decremented; caller becomes responsible.
     */
    svalue_t *release() noexcept;

    /**
     * Decrement refcount (if applicable) and clear this reference.
     */
    void reset(svalue_t *new_sv = nullptr) noexcept;

private:
    svalue_t *sv_;
};

} // namespace lpc

#endif // __cplusplus
