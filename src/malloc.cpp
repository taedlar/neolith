#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"

#ifdef __cplusplus
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#endif

char *reserved_area = NULL;		/* reserved for MALLOC() */

extern "C" char* xalloc (size_t size) {
  char *p;
  static int going_to_exit = 0;

  if (going_to_exit)
    exit (3);
  p = static_cast<char*>(std::malloc(size));
  if (p == 0)
    {
      if (reserved_area)
        {
          std::free (reserved_area);
          /* after freeing reserved area, we are supposed to be able to write log messages */
          debug_message ("{}\t***** temporarily out of MEMORY. Freeing reserve.");
          reserved_area = 0;
          slow_shutdown_to_do = 6;
          return xalloc (size);	/* Try again */
        }
      going_to_exit = 1;
      fatal ("Totally out of MEMORY.\n");
    }
  return p;
}

/* C++ wrapper for tracked heap allocations in scopes. */

neolith::heap::allocation_scope::allocation_scope() noexcept :
  parent_(current_scope()), active_(true) {
  current_scope() = this;
}

neolith::heap::allocation_scope::~allocation_scope() noexcept {
  if (active_)
    release_all();
  current_scope() = parent_;
}

void neolith::heap::allocation_scope::dismiss() noexcept {
  active_ = false;
}

void *neolith::heap::allocation_scope::release(void *ptr) noexcept {
  if (!ptr)
    return ptr;

  allocation_scope *owner = find_owner(ptr);
  if (owner)
    owner->erase(ptr);
  return ptr; // untrack but do not free, ownership transferred to caller
}

/**
 * @brief Allocate memory and track it in the current scope for auto-release on unwind.
 *
 * If no current scope, behaves like standard calloc/malloc.
 * If allocation fails and no_fail is true, logs fatal error and exits; otherwise returns nullptr.
 * When called with no_fail=true, reserved_area is used as emergency fallback to free memory
 * and log before retrying allocation. This gives a chance for the MUD to save player data and
 * shutdown gracefully instead of crashing outright when memory is exhausted.
 *
 * @param size Size of memory to allocate.
 * @param zeroed If true, memory is zero-initialized.
 * @param no_fail If true, allocation failure is fatal; if false, returns nullptr on failure.
 * @return Pointer to allocated memory, or nullptr on failure if no_fail is false.
 */
void *neolith::heap::allocation_scope::allocate(size_t size, bool zeroed, bool no_fail) noexcept {
  void *ptr;
  if (zeroed)
    {
      ptr = std::calloc(1, size);
    }
  else
    {
      ptr = no_fail ? static_cast<void *>(xalloc(size)) : std::malloc(size);
    }

  if (ptr && !track_in_current_scope(ptr))
    {
      std::free(ptr);
      if (no_fail)
        {
          fatal("Out of memory while tracking allocation.\n");
        }
      return nullptr;
    }
  return ptr;
}

/**
 * @brief Deallocate memory and update tracking in the current scope.
 * If the pointer is tracked in any active scope, it is untracked before deallocation.
 * If the pointer is not tracked, it is simply freed.
 * @param ptr Pointer to previously allocated memory (or nullptr).
 */
void neolith::heap::allocation_scope::deallocate(void *ptr) noexcept {
  if (!ptr)
    {
      return;
    }

  allocation_scope *owner = find_owner(ptr);
  if (owner)
    {
      owner->erase(ptr);
    }
  std::free(ptr);
}

/** @brief Get the current allocation scope for the thread. */
neolith::heap::allocation_scope *&neolith::heap::allocation_scope::current_scope() noexcept {
  static thread_local allocation_scope *scope = nullptr;
  return scope;
}

bool neolith::heap::allocation_scope::track_in_current_scope(void *ptr) noexcept {
  if (!ptr)
    return true; // nothing to track, consider it success

  allocation_scope *scope = current_scope();
  if (scope)
    {
      try
        {
          scope->tracked_.push_back(ptr);
        }
      catch (...)
        {
          return false; // allocation failed while trying to track, caller should free ptr
        }
    }
  return true;
}

/**
 * @brief Find the allocation scope that owns the given pointer, if any.
 * Searches from the current scope up through parent scopes to find the one that tracks the pointer.
 * @param ptr Pointer to check for ownership.
 * @return Pointer to the owning allocation_scope, or nullptr if not found in any active scope
 */
neolith::heap::allocation_scope *neolith::heap::allocation_scope::find_owner(void *ptr) noexcept {
  for (allocation_scope *scope = current_scope(); scope; scope = scope->parent_)
    {
      if (scope->contains(ptr))
        {
          return scope;
        }
    }
  return nullptr;
}

bool neolith::heap::allocation_scope::contains(void *ptr) const noexcept {
  return std::find(tracked_.begin(), tracked_.end(), ptr) != tracked_.end();
}

size_t neolith::heap::allocation_scope::index_of(void *ptr) const noexcept {
  for (size_t i = 0; i < tracked_.size(); ++i)
    {
      if (tracked_[i] == ptr)
        {
          return i;
        }
    }
  return npos;
}

/**
 * @brief Erase a pointer from tracking in the current scope without deallocating it.
 * If the pointer is found in the tracked list, it is removed so that the scope destructor
 * will not free it.
 * Useful when ownership is transferred back to caller.
 * @param ptr Pointer to release from tracking (or nullptr).
 */
void neolith::heap::allocation_scope::erase(void *ptr) noexcept {
  auto it = std::find(tracked_.begin(), tracked_.end(), ptr);
  if (it != tracked_.end())
    {
      tracked_.erase(it);
    }
}

void neolith::heap::allocation_scope::replace_at(size_t index, void *new_ptr) noexcept {
  if (index < tracked_.size())
    {
      tracked_[index] = new_ptr;
    }
}

void neolith::heap::allocation_scope::release_all() noexcept {
  for (void *ptr : tracked_)
    {
      std::free(ptr);
    }
  tracked_.clear();
}

void *neolith::heap::allocation_scope::reallocate(void *ptr, size_t size) noexcept {
  if (!ptr)
    return allocate(size, false, false); // equivalent to malloc if ptr is null

  if (size == 0)
    {
      deallocate(ptr);
      return nullptr; // equivalent to free if size is zero
    }

  allocation_scope *owner = find_owner(ptr);
  size_t owner_index = owner ? owner->index_of(ptr) : npos;
  allocation_scope *scope = current_scope();

  if (owner && owner_index == npos)
    {
      debug_message("{}\t***** Untracked pointer passed to reallocate()");
      return nullptr; // pointer is in scope but not found in tracked list, should not happen, treat as error
    }

  if (!owner && scope)
    {
      try
        {
          scope->tracked_.reserve(scope->tracked_.size() + 1);
        }
      catch (...)
        {
          return nullptr;
        }
    }

  void *new_ptr = std::realloc(ptr, size);
  if (!new_ptr)
    return nullptr; // reallocation failed, original ptr is still valid and unchanged, caller should handle this case

  if (owner && owner_index != npos)
    {
      // update tracking with new pointer location
      owner->replace_at(owner_index, new_ptr);
    }
  else
    {
      if (!track_in_current_scope(new_ptr))
        {
          std::free(new_ptr);
          return nullptr;
        }
    }

  return new_ptr;
}
