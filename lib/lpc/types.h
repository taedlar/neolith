#pragma once

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "src/stralloc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned short lpc_type_t; /* entry type in A_ARGUMENT_TYPES area */
typedef unsigned short function_index_t; /* an integer type for LPC function index (runtime_function_u) */

/* forward declarations */
typedef struct array_s			array_t;
typedef struct buffer_s			buffer_t;
typedef struct compiler_function_s	compiler_function_t;
typedef struct funptr_s			funptr_t;
typedef struct mapping_s		mapping_t;
typedef struct object_s			object_t;
typedef struct program_s		program_t;
typedef struct sentence_s		sentence_t;
typedef struct svalue_s			svalue_t;
typedef struct userid_s			userid_t;

typedef struct {
    unsigned short ref;
} refed_t;

union svalue_u {
    /* C-string semantics*/
    char *string;
    const char *const_string;
    shared_str_t shared_string;
    malloc_str_t malloc_string;

    int64_t number;    /* Neolith extension: fixed 64-bit integer for consistent cross-platform semantics */
    double real;    /* Neolith extension: both float and double are in native double precision */

    refed_t *refed; /* any of the block below */

    struct buffer_s *buf;
    struct object_s *ob;
    struct array_s *arr;
    struct mapping_s *map;
    struct funptr_s *fp;

    struct svalue_s *lvalue;
    unsigned char *lvalue_byte;
    void (*error_handler) (void);
};

/** @brief The value stack element.
 *  If it is a string, then the way that the string has been allocated differently, which will affect how it should be freed.
 */
typedef short svalue_type_t;
struct svalue_s {
    svalue_type_t type; /* runtime type of svalue_t (bit flags, not to be confused with lpc_type_t) */
    short subtype;
    union svalue_u u;
};

/* values for type field of svalue struct */
#define T_INVALID       0x0
#define T_LVALUE        0x1

#define T_NUMBER        0x2
#define T_STRING        0x4
#define T_REAL          0x80

#define T_ARRAY         0x8
#define T_OBJECT        0x10
#define T_MAPPING       0x20
#define T_FUNCTION      0x40
#define T_BUFFER        0x100
#define T_CLASS         0x200

#define T_REFED (T_ARRAY|T_OBJECT|T_MAPPING|T_FUNCTION|T_BUFFER|T_CLASS)

#define T_LVALUE_BYTE   0x400   /* byte-sized lvalue */
#define T_LVALUE_RANGE  0x800
#define T_ERROR_HANDLER 0x1000
#define T_ANY T_STRING|T_NUMBER|T_ARRAY|T_OBJECT|T_MAPPING|T_FUNCTION| \
        T_REAL|T_BUFFER|T_CLASS

/* values for subtype field of svalue struct */
#define STRING_COUNTED  0x1     /* has a length an ref count */
#define STRING_HASHED   0x2     /* is in the shared string table */

#define STRING_MALLOC   STRING_COUNTED
#define STRING_SHARED   (STRING_COUNTED | STRING_HASHED)
#define STRING_CONSTANT 0       /* constant string, always in multi-byte encoding (UTF-8) */

#define T_UNDEFINED     0x4     /* undefinedp() returns true */

#ifdef __cplusplus
} // extern "C"

#include <cstddef>
#include <climits>
#include <string>
#include <type_traits>

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

    [[nodiscard]] bool is_real() const noexcept {
        return sv_ != nullptr && sv_->type == T_REAL;
    }

    [[nodiscard]] bool is_object() const noexcept {
        return sv_ != nullptr && sv_->type == T_OBJECT;
    }

    [[nodiscard]] bool is_array() const noexcept {
        return sv_ != nullptr && sv_->type == T_ARRAY;
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

    /** Precondition: is_real(). */
    [[nodiscard]] double real() const noexcept {
        return sv_ ? sv_->u.real : 0.0;
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

    /** Assign a real payload; stamps type=T_REAL. */
    void set_real(double value) noexcept {
        if (sv_ == nullptr) {
            return;
        }
        sv_->type = T_REAL;
        sv_->subtype = 0;
        sv_->u.real = value;
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

    /** Assign an array payload; stamps type=T_ARRAY. */
    void set_array(array_t *arr) noexcept {
        if (sv_ == nullptr) {
            return;
        }
        sv_->type = T_ARRAY;
        sv_->subtype = 0;
        sv_->u.arr = arr;
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

#endif /* __cplusplus */