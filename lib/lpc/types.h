#pragma once

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#include <stdlib.h>

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
    /* Explicit string-subtype semantics */
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

#ifdef STRING_TYPE_SAFETY
#define SET_SVALUE_SHARED_STRING(sv, value) do { \
    shared_str_t set_svalue_shared_string_value = (value); \
    if (set_svalue_shared_string_value != NULL && !is_shared_string_payload(set_svalue_shared_string_value)) \
        abort(); \
    (sv)->type = T_STRING; \
    (sv)->subtype = STRING_SHARED; \
    (sv)->u.shared_string = set_svalue_shared_string_value; \
} while (0)

#define SET_SVALUE_MALLOC_STRING(sv, value) do { \
    malloc_str_t set_svalue_malloc_string_value = (value); \
    if (set_svalue_malloc_string_value != NULL && !is_malloc_string_payload(set_svalue_malloc_string_value)) \
        abort(); \
    (sv)->type = T_STRING; \
    (sv)->subtype = STRING_MALLOC; \
    (sv)->u.malloc_string = set_svalue_malloc_string_value; \
} while (0)
#else
#define SET_SVALUE_SHARED_STRING(sv, value) do { \
    shared_str_t set_svalue_shared_string_value = (value); \
    (sv)->type = T_STRING; \
    (sv)->subtype = STRING_SHARED; \
    (sv)->u.shared_string = set_svalue_shared_string_value; \
} while (0)

#define SET_SVALUE_MALLOC_STRING(sv, value) do { \
    malloc_str_t set_svalue_malloc_string_value = (value); \
    (sv)->type = T_STRING; \
    (sv)->subtype = STRING_MALLOC; \
    (sv)->u.malloc_string = set_svalue_malloc_string_value; \
} while (0)
#endif

#define SET_SVALUE_CONSTANT_STRING(sv, value) do { \
    const char *set_svalue_constant_string_value = (value); \
    (sv)->type = T_STRING; \
    (sv)->subtype = STRING_CONSTANT; \
    (sv)->u.const_string = set_svalue_constant_string_value; \
} while (0)

#ifdef __cplusplus
} // extern "C"

#include <cstddef>
#include <cstdlib>
#include <climits>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

extern "C" {
void free_svalue(svalue_t *v, const char *caller);
void assign_svalue_no_free(svalue_t *to, const svalue_t *from);
}

namespace lpc {

class const_svalue_view;

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
 *     communicate ownership intent to the caller.
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

    static constexpr svalue_view from(raw_type &sv) noexcept {
        return svalue_view(&sv);
    }

    static const_svalue_view from(const raw_type *sv) noexcept;
    static const_svalue_view from(const raw_type &sv) noexcept;

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
        return is_string() ? SVALUE_STRPTR(sv_) : nullptr;
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
        SET_SVALUE_SHARED_STRING(sv_, s);
    }

    /** Assign a STRING_SHARED payload from a byte-span. */
    void set_shared_string(std::string_view data) noexcept {
        if (sv_ == nullptr) {
            return;
        }
        const char *begin = data.data();
        const char *end = begin ? begin + data.size() : nullptr;
        set_shared_string(make_shared_string(begin ? begin : "", end));
    }

    /** Assign a STRING_MALLOC payload; stamps type=T_STRING, subtype=STRING_MALLOC. */
    void set_malloc_string(malloc_str_t s) noexcept {
        if (sv_ == nullptr) {
            return;
        }
        SET_SVALUE_MALLOC_STRING(sv_, s);
    }

    /** Assign a STRING_MALLOC payload from a byte-span. */
    void set_malloc_string(std::string_view data) noexcept {
        if (sv_ == nullptr) {
            return;
        }
        malloc_str_t str = int_new_string(data.size());
        if (data.size() > 0) {
            memcpy(str, data.data(), data.size());
        }
        str[data.size()] = '\0';
        set_malloc_string(str);
    }

    /** Assign a STRING_MALLOC payload from a NUL-terminated C string. */
    void set_malloc_string(const char *s) noexcept {
        set_malloc_string(std::string_view(s ? s : ""));
    }

    /** Assign a STRING_CONSTANT; stamps type=T_STRING, subtype=STRING_CONSTANT. */
    void set_constant_string(const char *s) noexcept {
        if (sv_ == nullptr) {
            return;
        }
        SET_SVALUE_CONSTANT_STRING(sv_, s);
    }

    /** Assign a STRING_CONSTANT payload from a byte-span.
     *  Precondition: data points to NUL-terminated constant storage.
     */
    void set_constant_string(std::string_view data) noexcept {
        set_constant_string(data.data());
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

/**
 * @brief Immutable non-owning C++ view over svalue_t.
 *
 * This view provides read-only typed access and cannot mutate the wrapped
 * value. It is used to make const intent explicit at call-sites.
 */
class const_svalue_view {
public:
    using raw_type = const ::svalue_t;

    explicit constexpr const_svalue_view(raw_type *sv) noexcept : sv_(sv) {}

    static constexpr const_svalue_view from(const raw_type *sv) noexcept {
        return const_svalue_view(sv);
    }

    static constexpr const_svalue_view from(const raw_type &sv) noexcept {
        return const_svalue_view(&sv);
    }

    [[nodiscard]] constexpr raw_type *raw() const noexcept { return sv_; }

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

    [[nodiscard]] const char *c_str() const noexcept {
        return is_string() ? SVALUE_STRPTR(sv_) : nullptr;
    }

    [[nodiscard]] shared_str_t shared_string() const noexcept {
        return sv_ ? sv_->u.shared_string : nullptr;
    }

    [[nodiscard]] malloc_str_t malloc_string() const noexcept {
        return sv_ ? sv_->u.malloc_string : nullptr;
    }

    [[nodiscard]] const char *const_string() const noexcept {
        return sv_ ? sv_->u.const_string : nullptr;
    }

    [[nodiscard]] int64_t number() const noexcept {
        return sv_ ? sv_->u.number : 0;
    }

    [[nodiscard]] double real() const noexcept {
        return sv_ ? sv_->u.real : 0.0;
    }

    [[nodiscard]] const object_t *object() const noexcept {
        return sv_ ? sv_->u.ob : nullptr;
    }

    [[nodiscard]] std::size_t length() const noexcept {
        return sv_ ? SVALUE_STRLEN(sv_) : 0;
    }

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

inline const_svalue_view svalue_view::from(const raw_type *sv) noexcept {
    return const_svalue_view::from(sv);
}

inline const_svalue_view svalue_view::from(const raw_type &sv) noexcept {
    return const_svalue_view::from(sv);
}

/**
 * @brief Owning RAII wrapper over svalue_t.
 *
 * This type owns one svalue_t instance and releases counted/referenced payloads
 * in its destructor via free_svalue().
 */
class svalue {
public:
    using raw_type = ::svalue_t;

    svalue() noexcept {
        value_.type = T_NUMBER;
        value_.subtype = 0;
        value_.u.number = 0;
    }

    svalue(const svalue &other) noexcept : svalue() {
        assign_svalue_no_free(&value_, &other.value_);
    }

    svalue(svalue &&other) noexcept {
        value_ = other.value_;
        other.value_.type = T_NUMBER;
        other.value_.subtype = 0;
        other.value_.u.number = 0;
    }

    /** Constructor from string literal; creates STRING_CONSTANT svalue.
     *  Only accepts compile-time string literals (const char (&)[N]), not pointers.
     *  This enforces explicit intent and prevents accidental construction from char* variables.
     */
    template<size_t N>
    svalue(const char (&literal)[N]) noexcept {
        value_.type = T_NUMBER;
        value_.subtype = 0;
        value_.u.number = 0;
        set_constant_string(std::string_view(literal, N - 1));  // N-1 excludes null terminator
    }

    ~svalue() noexcept {
        free_svalue(&value_, "lpc::svalue::~svalue");
    }

    svalue &operator=(const svalue &other) noexcept {
        if (this != &other) {
            free_svalue(&value_, "lpc::svalue::operator=(copy)");
            assign_svalue_no_free(&value_, &other.value_);
        }
        return *this;
    }

    svalue &operator=(svalue &&other) noexcept {
        if (this != &other) {
            free_svalue(&value_, "lpc::svalue::operator=(move)");
            value_ = other.value_;
            other.value_.type = T_NUMBER;
            other.value_.subtype = 0;
            other.value_.u.number = 0;
        }
        return *this;
    }

    [[nodiscard]] raw_type *raw() noexcept { return &value_; }
    [[nodiscard]] const raw_type *raw() const noexcept { return &value_; }

    [[nodiscard]] svalue_view view() noexcept { return svalue_view::from(&value_); }
    [[nodiscard]] const_svalue_view view() const noexcept { return const_svalue_view::from(&value_); }

    // Owning setters: free old value, then set new value
    // These are preferred for test code and cases where ownership is clear.

    /** Assign a STRING_SHARED; frees old value first. */
    void set_shared_string(shared_str_t s) noexcept {
        free_svalue(&value_, "lpc::svalue::set_shared_string");
        view().set_shared_string(s);
    }

    /** Assign a STRING_SHARED from byte-span; frees old value first. */
    void set_shared_string(std::string_view data) noexcept {
        free_svalue(&value_, "lpc::svalue::set_shared_string(span)");
        view().set_shared_string(data);
    }

    /** Assign a STRING_MALLOC; frees old value first. */
    void set_malloc_string(malloc_str_t s) noexcept {
        free_svalue(&value_, "lpc::svalue::set_malloc_string");
        view().set_malloc_string(s);
    }

    /** Assign a STRING_MALLOC from a NUL-terminated C string; frees old value first. */
    void set_malloc_string(const char *s) noexcept {
        set_malloc_string(std::string_view(s ? s : ""));
    }

    /** Assign a STRING_MALLOC from byte-span; allocates, copies data, frees old first. */
    void set_malloc_string(std::string_view data) noexcept {
        malloc_str_t str = int_new_string(data.size());
        if (data.size() > 0) {
            memcpy(str, data.data(), data.size());
        }
        str[data.size()] = '\0';
        set_malloc_string(str);  // delegate to existing setter
    }

    /** Assign a STRING_CONSTANT; frees old value first. */
    void set_constant_string(const char *s) noexcept {
        free_svalue(&value_, "lpc::svalue::set_constant_string");
        view().set_constant_string(s);
    }

    /** Assign a STRING_CONSTANT from byte-span; frees old first.
     *  Precondition: data must point to constant memory (string literal or equivalent).
     *  For stack/heap data with embedded NULs, use set_malloc_string(std::string_view) instead.
     */
    void set_constant_string(std::string_view data) noexcept {
        set_constant_string(data.data());
    }

    /** Assign a number; frees old value first. */
    void set_number(int64_t value) noexcept {
        free_svalue(&value_, "lpc::svalue::set_number");
        view().set_number(value);
    }

    /** Assign a real; frees old value first. */
    void set_real(double value) noexcept {
        free_svalue(&value_, "lpc::svalue::set_real");
        view().set_real(value);
    }

    /** Assign an object; frees old value first. */
    void set_object(object_t *ob) noexcept {
        free_svalue(&value_, "lpc::svalue::set_object");
        view().set_object(ob);
    }

    /** Assign an array; frees old value first. */
    void set_array(array_t *arr) noexcept {
        free_svalue(&value_, "lpc::svalue::set_array");
        view().set_array(arr);
    }

private:
    raw_type value_;
};

template<typename StringT = std::string>
[[nodiscard]] inline StringT str(svalue_view sv) {
    return sv.template str<StringT>();
}

template<typename StringT = std::string>
[[nodiscard]] inline StringT str(const_svalue_view sv) {
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
static_assert(sizeof(lpc::const_svalue_view) == sizeof(const ::svalue_t *),
              "lpc::const_svalue_view must remain pointer-sized");
static_assert(alignof(lpc::const_svalue_view) == alignof(const ::svalue_t *),
              "lpc::const_svalue_view must remain pointer-aligned");
static_assert(std::is_standard_layout<lpc::const_svalue_view>::value,
              "lpc::const_svalue_view must remain standard-layout");
static_assert(std::is_trivially_copyable<lpc::const_svalue_view>::value,
              "lpc::const_svalue_view must remain trivially copyable");
static_assert(sizeof(lpc::svalue) == sizeof(::svalue_t),
              "lpc::svalue must remain svalue_t-sized");
static_assert(alignof(lpc::svalue) == alignof(::svalue_t),
              "lpc::svalue must remain svalue_t-aligned");

#endif /* __cplusplus */