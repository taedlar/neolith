#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <assert.h>
#include <utility>
#include <type_traits>

#include "src/std.h"
#include "buffer.h"
#include "svalue.h"

extern "C" {
extern void dealloc_object (object_t *, const char* caller);
extern void dealloc_array (array_t *);
extern void dealloc_mapping (mapping_t *);
extern void dealloc_class (array_t *);
extern void dealloc_funp (funptr_t *);
}

/* ========== C API IMPLEMENTATION ========== */

/**
 * Free the data that an svalue is pointing to. Not the svalue itself.
 * The caller argument is used for debugging purposes, to know which function is freeing the svalue.
 */
void free_svalue (svalue_t * v, const char* caller) {

  assert (v != NULL);

  if (v->type == T_STRING)
    {
      free_string_svalue (v);
    }
  else if (v->type & T_REFED)
    {
      if (!(--v->u.refed->ref))
        {
          switch (v->type)
            {
            case T_OBJECT:
              dealloc_object (v->u.ob, caller);
              break;
            case T_CLASS:
              dealloc_class (v->u.arr);
              break;
            case T_ARRAY:
              dealloc_array (v->u.arr);
              break;
            case T_BUFFER:
              if (v->u.buf != &null_buf)
                FREE ((char *) v->u.buf);
              break;
            case T_MAPPING:
              dealloc_mapping (v->u.map);
              break;
            case T_FUNCTION:
              dealloc_funp (v->u.fp);
              break;
            }
        }
    }
  else if (v->type == T_ERROR_HANDLER)
    {
      (*v->u.error_handler) ();
    }
}

/**
 * Free several svalues, and free up the space used by the svalues.
 * The svalues must be sequentially located.
 */
void free_some_svalues (svalue_t * v, int num) {
  while (num--)
    free_svalue (v + num, "free_some_svalues");
  FREE (v);
}

void assign_svalue (svalue_t * dest, svalue_t * v) {
  /* First deallocate the previous value. */
  free_svalue (dest, "assign_svalue");
  assign_svalue_no_free (dest, v);
}

/**
 * Assign to a svalue.
 * This is done either when element in array, or when to an identifier
 * (as all identifiers are kept in a array pointed to by the object).
 */
void assign_svalue_no_free (svalue_t * to, svalue_t * from) {
  DEBUG_CHECK (from == 0, "Attempt to assign_svalue() from a null ptr.\n");
  DEBUG_CHECK (to == 0, "Attempt to assign_svalue() to a null ptr.\n");
  *to = *from;

  if (from->type == T_STRING)
    {
      if (from->subtype == STRING_MALLOC)
        {
          INC_COUNTED_REF (to->u.malloc_string);
/*      ADD_STRING(MSTR_SIZE(to->u.malloc_string)); */
        }
      else if (from->subtype == STRING_SHARED)
        {
          INC_COUNTED_REF (to->u.shared_string);
/*      ADD_STRING(MSTR_SIZE(to->u.shared_string)); */
        }
    }
  else if (from->type & T_REFED)
    {
      from->u.refed->ref++;
    }
}

/*
 * Copies an array of svalues to another location, which should be
 * free space.
 */
void copy_some_svalues (svalue_t * dest, svalue_t * v, int num) {
  while (num--)
    assign_svalue_no_free (dest + num, v + num);
}

/* ========== C++ RAII WRAPPER IMPLEMENTATION ========== */

namespace lpc {

// ========== svalue_guard ==========

svalue_guard svalue_guard::allocate(const svalue_t *initial_value) noexcept {
    auto *sv = static_cast<svalue_t *>(DMALLOC(sizeof(svalue_t), 0, "svalue_guard::allocate"));
    if (!sv) return svalue_guard(nullptr);

    sv->type = T_NUMBER;
    sv->subtype = 0;
    sv->u.number = 0;

    if (initial_value) {
        assign_svalue_no_free(sv, const_cast<svalue_t *>(initial_value));
    }
    return svalue_guard(sv);
}

svalue_guard svalue_guard::copy(const svalue_t &value) noexcept {
    return allocate(&value);
}

svalue_guard::svalue_guard(svalue_guard &&other) noexcept : sv_(other.release()) {}

svalue_guard &svalue_guard::operator=(svalue_guard &&other) noexcept {
    reset(other.release());
    return *this;
}

svalue_guard::~svalue_guard() noexcept {
    reset();
}

svalue_t *svalue_guard::get() noexcept { return sv_; }
const svalue_t *svalue_guard::get() const noexcept { return sv_; }

svalue_t *svalue_guard::operator->() noexcept { return sv_; }
const svalue_t *svalue_guard::operator->() const noexcept { return sv_; }

svalue_t &svalue_guard::operator*() noexcept { return *sv_; }
const svalue_t &svalue_guard::operator*() const noexcept { return *sv_; }

svalue_guard::operator bool() const noexcept { return sv_ != nullptr; }

svalue_t *svalue_guard::release() noexcept {
    svalue_t *tmp = sv_;
    sv_ = nullptr;
    return tmp;
}

void svalue_guard::reset(svalue_t *new_sv) noexcept {
    if (sv_) {
        free_svalue(sv_, "svalue_guard::reset");
        FREE(sv_);
    }
    sv_ = new_sv;
}

void svalue_guard::assign(const svalue_t &value) noexcept {
    if (sv_) {
        assign_svalue(sv_, const_cast<svalue_t *>(&value));
    }
}

void svalue_guard::assign_no_free(const svalue_t &value) noexcept {
    if (sv_) {
        assign_svalue_no_free(sv_, const_cast<svalue_t *>(&value));
    }
}

svalue_guard::svalue_guard(svalue_t *sv) noexcept : sv_(sv) {}

// ========== svalue_array_guard ==========

svalue_array_guard svalue_array_guard::allocate(int size) noexcept {
    auto *arr = static_cast<svalue_t *>(DMALLOC(size * sizeof(svalue_t), 0, "svalue_array_guard::allocate"));
    if (!arr) return svalue_array_guard(nullptr, 0);

    // Initialize all to T_NUMBER 0
    for (int i = 0; i < size; ++i) {
        arr[i].type = T_NUMBER;
        arr[i].subtype = 0;
        arr[i].u.number = 0;
    }
    return svalue_array_guard(arr, size);
}

svalue_array_guard svalue_array_guard::copy(const svalue_t *src, int count) noexcept {
    auto guard = allocate(count);
    if (guard && src) {
        copy_some_svalues(guard.get(), const_cast<svalue_t *>(src), count);
    }
    return guard;
}

svalue_array_guard::svalue_array_guard(svalue_array_guard &&other) noexcept
    : arr_(other.release()), size_(other.size_) {
    other.size_ = 0;
}

svalue_array_guard &svalue_array_guard::operator=(svalue_array_guard &&other) noexcept {
    reset(other.release(), other.size_);
    other.size_ = 0;
    return *this;
}

svalue_array_guard::~svalue_array_guard() noexcept {
    reset();
}

svalue_t *svalue_array_guard::get() noexcept { return arr_; }
const svalue_t *svalue_array_guard::get() const noexcept { return arr_; }

svalue_t &svalue_array_guard::operator[](int index) noexcept { return arr_[index]; }
const svalue_t &svalue_array_guard::operator[](int index) const noexcept { return arr_[index]; }

int svalue_array_guard::size() const noexcept { return size_; }

svalue_array_guard::operator bool() const noexcept { return arr_ != nullptr; }

svalue_t *svalue_array_guard::release() noexcept {
    svalue_t *tmp = arr_;
    arr_ = nullptr;
    size_ = 0;
    return tmp;
}

void svalue_array_guard::reset(svalue_t *new_arr, int new_size) noexcept {
    if (arr_) {
        free_some_svalues(arr_, size_);
        // Note: free_some_svalues() calls FREE(v), so arr_ is deallocated
    }
    arr_ = new_arr;
    size_ = new_size;
}

svalue_array_guard::svalue_array_guard(svalue_t *arr, int size) noexcept
    : arr_(arr), size_(size) {}

// ========== svalue_ref ==========

svalue_ref::svalue_ref(svalue_t *sv) noexcept : sv_(sv) {
    if (sv_ && (sv_->type & T_REFED)) {
        sv_->u.refed->ref++;
    }
}

svalue_ref::svalue_ref(svalue_ref &&other) noexcept : sv_(other.release()) {}

svalue_ref &svalue_ref::operator=(svalue_ref &&other) noexcept {
    reset(other.release());
    return *this;
}

svalue_ref::svalue_ref(const svalue_ref &other) noexcept : sv_(other.sv_) {
    if (sv_ && (sv_->type & T_REFED)) {
        sv_->u.refed->ref++;
    }
}

svalue_ref &svalue_ref::operator=(const svalue_ref &other) noexcept {
    if (this != &other) {
        reset(other.sv_);
        if (sv_ && (sv_->type & T_REFED)) {
            sv_->u.refed->ref++;
        }
    }
    return *this;
}

svalue_ref::~svalue_ref() noexcept {
    reset();
}

svalue_t *svalue_ref::get() noexcept { return sv_; }
const svalue_t *svalue_ref::get() const noexcept { return sv_; }

svalue_t *svalue_ref::operator->() noexcept { return sv_; }
const svalue_t *svalue_ref::operator->() const noexcept { return sv_; }

svalue_t &svalue_ref::operator*() noexcept { return *sv_; }
const svalue_t &svalue_ref::operator*() const noexcept { return *sv_; }

svalue_ref::operator bool() const noexcept { return sv_ != nullptr; }

svalue_t *svalue_ref::release() noexcept {
    svalue_t *tmp = sv_;
    sv_ = nullptr;
    return tmp;
}

void svalue_ref::reset(svalue_t *new_sv) noexcept {
    if (sv_ && (sv_->type & T_REFED)) {
        if (!(--sv_->u.refed->ref)) {
            // Refcount hit zero; data will be freed by the LPC runtime
            // This guard just stops tracking it
        }
    }
    sv_ = new_sv;
}

} // namespace lpc

