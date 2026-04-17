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

namespace {

void retain_svalue_payload(svalue_t *sv) noexcept {
  if (sv == nullptr)
    {
      return;
    }

  auto view = lpc::svalue_view::from(sv);
  if (view.is_string())
    {
      if (view.is_malloc())
        {
          INC_COUNTED_REF(view.malloc_string());
        }
      else if (view.is_shared())
        {
          INC_COUNTED_REF(view.shared_string());
        }
      return;
    }

  if (sv->type & T_REFED)
    {
      sv->u.refed->ref++;
    }
}

void release_retained_svalue(svalue_t *sv, const char *caller) noexcept {
  if (sv == nullptr)
    {
      return;
    }

  auto view = lpc::svalue_view::from(sv);
  if (view.is_string() || (sv->type & T_REFED))
    {
      free_svalue(sv, caller);
    }
}

}  // namespace

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
void assign_svalue_no_free (svalue_t * to, const svalue_t * from) {
  DEBUG_CHECK (from == 0, "Attempt to assign_svalue() from a null ptr.\n");
  DEBUG_CHECK (to == 0, "Attempt to assign_svalue() to a null ptr.\n");
  *to = *from;

  retain_svalue_payload (to);
}

/*
 * Copies an array of svalues to another location, which should be
 * free space.
 */
void copy_some_svalues (svalue_t * dest, svalue_t * v, int num) {
  while (num--)
    assign_svalue_no_free (dest + num, v + num);
}

int svalue_string_lexcmp(const svalue_t *lhs, const svalue_t *rhs) {
  auto lhs_view = lpc::const_svalue_view::from(lhs);
  auto rhs_view = lpc::const_svalue_view::from(rhs);
  size_t lhs_len = lhs_view.length();
  size_t rhs_len = rhs_view.length();
  size_t prefix_len = lhs_len < rhs_len ? lhs_len : rhs_len;
  int cmp = 0;

  if (prefix_len != 0)
    {
      cmp = memcmp(lhs_view.c_str(), rhs_view.c_str(), prefix_len);
      if (cmp != 0)
        {
          return cmp;
        }
    }

  if (lhs_len < rhs_len)
    {
      return -1;
    }
  if (lhs_len > rhs_len)
    {
      return 1;
    }
  return 0;
}

/* ========== C++ RAII WRAPPER IMPLEMENTATION ========== */

namespace lpc {
// ========== svalue_ref ==========

svalue_ref::svalue_ref(svalue_t *sv) noexcept : sv_(sv) {
  retain_svalue_payload(sv_);
}

svalue_ref::svalue_ref(svalue_ref &&other) noexcept : sv_(other.release()) {}

svalue_ref &svalue_ref::operator=(svalue_ref &&other) noexcept {
    reset(other.release());
    return *this;
}

svalue_ref::svalue_ref(const svalue_ref &other) noexcept : sv_(other.sv_) {
  retain_svalue_payload(sv_);
}

svalue_ref &svalue_ref::operator=(const svalue_ref &other) noexcept {
    if (this != &other) {
        reset(other.sv_);
    retain_svalue_payload(sv_);
    }
    return *this;
}

svalue_ref::~svalue_ref() noexcept {
    reset();
}

svalue_view svalue_ref::view() noexcept { return svalue_view::from(sv_); }
const_svalue_view svalue_ref::view() const noexcept { return const_svalue_view::from(sv_); }

svalue_t *svalue_ref::get() noexcept { return view().raw(); }
const svalue_t *svalue_ref::get() const noexcept { return view().raw(); }

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
  release_retained_svalue(sv_, "svalue_ref::reset");
    sv_ = new_sv;
}

} // namespace lpc

