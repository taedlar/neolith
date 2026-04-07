#pragma once
/**
 * @file lpc/types.hpp
 * @brief C++ companion for lpc/types.h.
 *
 * Keep types.h as the pure C ABI surface. C++ helpers such as lpc::svalue_view
 * live here so C++-only facilities (<string>, templates, inline methods) stay
 * out of headers that are commonly included inside extern "C" blocks.
 *
 * Include this header only from C++ code, and include it outside any extern
 * "C" block.
 */

#include <cstddef>
#include <climits>
#include <string>
#include <type_traits>

extern "C" {
#include "types.h"
#include "stralloc.h"
}

namespace lpc {

/**
 * @brief Non-owning C++ view over svalue_t.
 *
 * The view wraps an existing svalue_t pointer. It does not allocate, free, or
 * transfer ownership of the underlying value.
 *
 * String interface design:
 *   - Named write methods (set_shared_string / set_malloc_string /
 *     set_constant_string) prevent accidental cross-subtype assignments: there is
 *     no generic set_string(char*). Each setter atomically stamps type, subtype,
 *     and the correct union member.
 *   - Typed read accessors (shared_string() / malloc_string() / const_string())
 *     communicate ownership intent to the caller. When shared_str_t / malloc_str_t
 *     are made into distinct types, these signatures become compile-time-enforced
 *     overloads with no call-site changes required.
 *   - c_str() follows std::string::c_str() semantics: returns a null-terminated
 *     const char* safe for any string subtype. Null termination is guaranteed
 *     for all subtypes (enforced by new_string() / extend_string() and literal
 *     constants), so c_str() is semantically correct here.
 *   - str<StringT>() returns an owned copy of the full byte-span using the
 *     subtype-safe length; safe for embedded NUL bytes. StringT defaults to
 *     std::string and must be constructible from (const char *, std::size_t).
 *   - Small non-string accessors (number/object) support common test assertions
 *     without requiring direct union-member access for simple scalar/reference
 *     types.
 *   - length() is O(1) for STRING_MALLOC / STRING_SHARED (reads the block header)
 *     and O(n) fallback for STRING_CONSTANT.
 */
class svalue_view {
public:
    using raw_type = ::svalue_t;

    explicit constexpr svalue_view(raw_type *sv) noexcept : sv_(sv) {}

    static constexpr svalue_view from(raw_type *sv) noexcept {
        return svalue_view(sv);
    }

    static constexpr svalue_view from(const raw_type *sv) noexcept {
        return svalue_view(const_cast<raw_type *>(sv));
    }

    [[nodiscard]] constexpr raw_type *raw() noexcept { return sv_; }
    [[nodiscard]] constexpr const raw_type *raw() const noexcept { return sv_; }

    [[nodiscard]] constexpr bool is_valid() const noexcept { return sv_ != nullptr; }

    [[nodiscard]] bool is_string() const noexcept {
        return sv_ != nullptr && sv_->type == T_STRING;
    }

    [[nodiscard]] bool is_counted() const noexcept {
        return sv_ != nullptr && (sv_->subtype & STRING_COUNTED) != 0;
    }

    [[nodiscard]] bool is_shared() const noexcept {
        return sv_ != nullptr && sv_->subtype == STRING_SHARED;
    }

    [[nodiscard]] bool is_malloc() const noexcept {
        return sv_ != nullptr && sv_->subtype == STRING_MALLOC;
    }

    [[nodiscard]] bool is_constant() const noexcept {
        return sv_ != nullptr && sv_->subtype == STRING_CONSTANT;
    }

    [[nodiscard]] bool is_number() const noexcept {
        return sv_ != nullptr && sv_->type == T_NUMBER;
    }

    [[nodiscard]] bool is_object() const noexcept {
        return sv_ != nullptr && sv_->type == T_OBJECT;
    }

    /**
     * c_str(): null-terminated const char*, safe for all string subtypes.
     * Null termination is a driver invariant for all subtypes; semantics match
     * std::string::c_str(). Lifetime is tied to the underlying svalue_t.
     */
    [[nodiscard]] const char *c_str() const noexcept {
        return sv_ ? sv_->u.string : nullptr;
    }

    /** Precondition: is_shared() — use to pass to ref_string() / free_string(). */
    [[nodiscard]] shared_str_t shared_string() const noexcept {
        return sv_ ? sv_->u.shared_string : nullptr;
    }

    /** Precondition: is_malloc() — use to pass to extend_string() / FREE_MSTR(). */
    [[nodiscard]] malloc_str_t malloc_string() const noexcept {
        return sv_ ? sv_->u.malloc_string : nullptr;
    }

    /** Precondition: is_constant() — literal/constant string; not ref-counted. */
    [[nodiscard]] const char *const_string() const noexcept {
        return sv_ ? sv_->u.const_string : nullptr;
    }

    /** Precondition: is_number(). */
    [[nodiscard]] int64_t number() const noexcept {
        return sv_ ? sv_->u.number : 0;
    }

    /** Precondition: is_object(). */
    [[nodiscard]] object_t *object() const noexcept {
        return sv_ ? sv_->u.ob : nullptr;
    }

    /** Assign a STRING_SHARED payload; stamps type=T_STRING, subtype=STRING_SHARED. */
    void set_shared_string(shared_str_t s) noexcept {
        if (sv_ == nullptr) {
            return;
        }
        sv_->type = T_STRING;
        sv_->subtype = STRING_SHARED;
        sv_->u.shared_string = s;
    }

    /** Assign a STRING_MALLOC payload; stamps type=T_STRING, subtype=STRING_MALLOC. */
    void set_malloc_string(malloc_str_t s) noexcept {
        if (sv_ == nullptr) {
            return;
        }
        sv_->type = T_STRING;
        sv_->subtype = STRING_MALLOC;
        sv_->u.malloc_string = s;
    }

    /** Assign a STRING_CONSTANT; stamps type=T_STRING, subtype=STRING_CONSTANT. */
    void set_constant_string(const char *s) noexcept {
        if (sv_ == nullptr) {
            return;
        }
        sv_->type = T_STRING;
        sv_->subtype = STRING_CONSTANT;
        sv_->u.const_string = s;
    }

    /** Assign a number payload; stamps type=T_NUMBER. */
    void set_number(int64_t value) noexcept {
        if (sv_ == nullptr) {
            return;
        }
        sv_->type = T_NUMBER;
        sv_->subtype = 0;
        sv_->u.number = value;
    }

    /** Assign an object payload; stamps type=T_OBJECT. */
    void set_object(object_t *ob) noexcept {
        if (sv_ == nullptr) {
            return;
        }
        sv_->type = T_OBJECT;
        sv_->subtype = 0;
        sv_->u.ob = ob;
    }

    /** O(1) for counted strings, O(n) fallback for STRING_CONSTANT. */
    [[nodiscard]] std::size_t length() const noexcept {
        return sv_ ? SVALUE_STRLEN(sv_) : 0;
    }

    /** Returns an owned copy of the full byte-span. */
    template<typename StringT = std::string>
    [[nodiscard]] StringT str() const {
        if (!is_string()) {
            return StringT{};
        }
        return StringT(c_str(), length());
    }

private:
    raw_type *sv_;
};

template<typename StringT = std::string>
[[nodiscard]] inline StringT str(svalue_view sv) {
    return sv.template str<StringT>();
}

} // namespace lpc

static_assert(sizeof(lpc::svalue_view) == sizeof(::svalue_t *),
              "lpc::svalue_view must remain pointer-sized");
static_assert(alignof(lpc::svalue_view) == alignof(::svalue_t *),
              "lpc::svalue_view must remain pointer-aligned");
static_assert(std::is_standard_layout<lpc::svalue_view>::value,
              "lpc::svalue_view must remain standard-layout");
static_assert(std::is_trivially_copyable<lpc::svalue_view>::value,
              "lpc::svalue_view must remain trivially copyable");
