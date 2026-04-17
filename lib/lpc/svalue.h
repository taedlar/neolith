#pragma once

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif
void free_svalue (svalue_t * v, const char* caller);
void free_some_svalues(svalue_t *, int);
void assign_svalue(svalue_t *, svalue_t *);
void assign_svalue_no_free(svalue_t *, svalue_t *);
void copy_some_svalues(svalue_t *, svalue_t *, int);

#ifdef __cplusplus
} // extern "C"

/*
 * C++ RAII wrappers for svalue management.
 * These provide safe ownership semantics without modifying the C API.
 */

namespace lpc {

/**
 * @brief RAII guard for a heap-allocated svalue_t.
 *
 * Acquires a heap-allocated svalue_t on construction.
 * Automatically calls free_svalue() on destruction.
 * Supports move semantics for safe ownership transfer.
 *
 * Not copyable (ownership is exclusive).
 *
 * Example:
 *   svalue_guard sv = svalue_guard::allocate();
 *   sv->type = T_STRING;
 *   // Automatically freed at scope exit
 */
class svalue_guard {
public:
    /**
     * Allocate a new svalue_t on the heap.
     * @param initial_value Optional initial svalue to assign; if null, initializes to T_NUMBER 0.
     * @return svalue_guard owning the allocated svalue.
     */
    static svalue_guard allocate(const svalue_t *initial_value = nullptr) noexcept;

    /**
     * Allocate and assign from an existing svalue.
     * @param value The svalue to copy into the new allocation.
     * @return svalue_guard owning the allocated and assigned svalue.
     */
    static svalue_guard copy(const svalue_t &value) noexcept;

    // Move semantics
    svalue_guard(svalue_guard &&other) noexcept;
    svalue_guard &operator=(svalue_guard &&other) noexcept;

    // Delete copy operations (ownership is exclusive)
    svalue_guard(const svalue_guard &) = delete;
    svalue_guard &operator=(const svalue_guard &) = delete;

    // Destructor
    ~svalue_guard() noexcept;

    // Accessors
    svalue_t *get() noexcept;
    const svalue_t *get() const noexcept;

    svalue_t *operator->() noexcept;
    const svalue_t *operator->() const noexcept;

    svalue_t &operator*() noexcept;
    const svalue_t &operator*() const noexcept;

    explicit operator bool() const noexcept;

    /**
     * Release ownership and return the raw pointer.
     * Caller becomes responsible for calling free_svalue().
     */
    svalue_t *release() noexcept;

    /**
     * Free the current svalue and optionally take ownership of a new one.
     */
    void reset(svalue_t *new_sv = nullptr) noexcept;

    /**
     * Assign a new value to this svalue.
     * Frees the old value before assigning.
     */
    void assign(const svalue_t &value) noexcept;

    /**
     * Assign without freeing the old value.
     * Use only when you're sure the current svalue is initialized to T_NUMBER 0.
     */
    void assign_no_free(const svalue_t &value) noexcept;

private:
    friend class svalue_array_guard;
    explicit svalue_guard(svalue_t *sv) noexcept;

    svalue_t *sv_;
};

/**
 * @brief RAII guard for a heap-allocated array of svalues.
 *
 * Acquires a heap-allocated array of svalue_t on construction.
 * Automatically calls free_some_svalues() on destruction (which frees both
 * the svalues AND the array allocation).
 *
 * Supports move semantics for safe ownership transfer.
 * Not copyable (ownership is exclusive).
 *
 * Example:
 *   svalue_array_guard arr = svalue_array_guard::allocate(10);
 *   arr[0].type = T_STRING;
 *   // Automatically freed (including the array) at scope exit
 */
class svalue_array_guard {
public:
    /**
     * Allocate a new array of svalues.
     * @param size Number of svalues to allocate.
     * @return svalue_array_guard owning the allocated array.
     */
    static svalue_array_guard allocate(int size) noexcept;

    /**
     * Allocate and copy from an existing array.
     * @param src Source array of svalues.
     * @param count Number of svalues to copy.
     * @return svalue_array_guard owning the allocated and filled array.
     */
    static svalue_array_guard copy(const svalue_t *src, int count) noexcept;

    // Move semantics
    svalue_array_guard(svalue_array_guard &&other) noexcept;
    svalue_array_guard &operator=(svalue_array_guard &&other) noexcept;

    // Delete copy operations (ownership is exclusive)
    svalue_array_guard(const svalue_array_guard &) = delete;
    svalue_array_guard &operator=(const svalue_array_guard &) = delete;

    // Destructor
    ~svalue_array_guard() noexcept;

    // Accessors
    svalue_t *get() noexcept;
    const svalue_t *get() const noexcept;

    svalue_t &operator[](int index) noexcept;
    const svalue_t &operator[](int index) const noexcept;

    int size() const noexcept;

    explicit operator bool() const noexcept;

    /**
     * Release ownership and return the raw pointer.
     * Caller becomes responsible for calling free_some_svalues().
     * Note: size is reset to 0, so save it beforehand if needed.
     */
    svalue_t *release() noexcept;

    /**
     * Free the current array and optionally take ownership of a new one.
     * Uses free_some_svalues() which frees both the svalues and the array.
     */
    void reset(svalue_t *new_arr = nullptr, int new_size = 0) noexcept;

private:
    explicit svalue_array_guard(svalue_t *arr, int size) noexcept;

    svalue_t *arr_;
    int size_;
};

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
