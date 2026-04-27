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
  p = (char *) DMALLOC (size, TAG_MISC, "main.c: xalloc");
  if (p == 0)
    {
      if (reserved_area)
        {
          FREE (reserved_area);
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

neolith::heap::allocation_scope::allocation_scope() noexcept :
  parent_(current_scope()), active_(true) {
  current_scope() = this;
}

neolith::heap::allocation_scope::~allocation_scope() {
  if (active_)
    {
      release_all();
    }
  current_scope() = parent_;
}

void neolith::heap::allocation_scope::dismiss() noexcept {
  active_ = false;
}

void *neolith::heap::allocation_scope::release(void *ptr) noexcept {
  if (!ptr)
    {
      return ptr;
    }

  allocation_scope *owner = find_owner(ptr);
  if (owner)
    {
      owner->erase(ptr);
    }
  return ptr;
}

void *neolith::heap::allocation_scope::allocate(size_t size, bool zeroed, bool no_fail) {
  void *ptr;
  if (zeroed)
    {
      ptr = std::calloc(1, size);
    }
  else
    {
      ptr = no_fail ? static_cast<void *>(xalloc(size)) : std::malloc(size);
    }

  track_in_current_scope(ptr);
  return ptr;
}

void neolith::heap::allocation_scope::deallocate(void *ptr) {
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

neolith::heap::allocation_scope *&neolith::heap::allocation_scope::current_scope() {
  static thread_local allocation_scope *scope = nullptr;
  return scope;
}

void neolith::heap::allocation_scope::track_in_current_scope(void *ptr) {
  if (!ptr)
    {
      return;
    }

  allocation_scope *scope = current_scope();
  if (scope)
    {
      scope->tracked_.push_back(ptr);
    }
}

neolith::heap::allocation_scope *neolith::heap::allocation_scope::find_owner(void *ptr) {
  for (allocation_scope *scope = current_scope(); scope; scope = scope->parent_)
    {
      if (scope->contains(ptr))
        {
          return scope;
        }
    }
  return nullptr;
}

int neolith::heap::allocation_scope::find_owner_depth_addr(uintptr_t ptr_addr) {
  int depth = 0;
  for (allocation_scope *scope = current_scope(); scope; scope = scope->parent_)
    {
      if (scope->contains_addr(ptr_addr))
        {
          return depth;
        }
      depth++;
    }
  return -1;
}

neolith::heap::allocation_scope *neolith::heap::allocation_scope::scope_at_depth(int depth) {
  allocation_scope *scope = current_scope();
  while (scope && depth > 0)
    {
      scope = scope->parent_;
      depth--;
    }
  return scope;
}

bool neolith::heap::allocation_scope::contains(void *ptr) const {
  return std::find(tracked_.begin(), tracked_.end(), ptr) != tracked_.end();
}

bool neolith::heap::allocation_scope::contains_addr(uintptr_t ptr_addr) const {
  for (void *tracked_ptr : tracked_)
    {
      if (reinterpret_cast<uintptr_t>(tracked_ptr) == ptr_addr)
        {
          return true;
        }
    }
  return false;
}

size_t neolith::heap::allocation_scope::index_of_addr(uintptr_t ptr_addr) const {
  for (size_t i = 0; i < tracked_.size(); ++i)
    {
      if (reinterpret_cast<uintptr_t>(tracked_[i]) == ptr_addr)
        {
          return i;
        }
    }
  return npos;
}

void neolith::heap::allocation_scope::erase(void *ptr) {
  auto it = std::find(tracked_.begin(), tracked_.end(), ptr);
  if (it != tracked_.end())
    {
      tracked_.erase(it);
    }
}

void neolith::heap::allocation_scope::erase_at(size_t index) {
  if (index < tracked_.size())
    {
      tracked_.erase(tracked_.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

void neolith::heap::allocation_scope::replace_at(size_t index, void *new_ptr) {
  if (index < tracked_.size())
    {
      tracked_[index] = new_ptr;
    }
  else
    {
      tracked_.push_back(new_ptr);
    }
}

void neolith::heap::allocation_scope::insert_at_or_push(size_t index, void *ptr) {
  if (index <= tracked_.size())
    {
      tracked_.insert(tracked_.begin() + static_cast<std::ptrdiff_t>(index), ptr);
    }
  else
    {
      tracked_.push_back(ptr);
    }
}

void neolith::heap::allocation_scope::release_all() noexcept {
  for (void *ptr : tracked_)
    {
      std::free(ptr);
    }
  tracked_.clear();
}

void *neolith::heap::allocation_scope::reallocate(void *ptr, size_t size) {
  uintptr_t ptr_addr = reinterpret_cast<uintptr_t>(ptr);
  int owner_depth = find_owner_depth_addr(ptr_addr);
  size_t owner_index = npos;

  if (owner_depth >= 0)
    {
      allocation_scope *owner = scope_at_depth(owner_depth);
      if (owner)
        {
          owner_index = owner->index_of_addr(ptr_addr);
          if (owner_index != npos)
            {
              owner->erase_at(owner_index);
            }
        }
    }

  void *new_ptr = std::realloc(reinterpret_cast<void *>(ptr_addr), size);
  if (!new_ptr)
    {
      if (owner_depth >= 0)
        {
          allocation_scope *owner = scope_at_depth(owner_depth);
          if (owner)
            {
              owner->insert_at_or_push(owner_index, reinterpret_cast<void *>(ptr_addr));
            }
          else
            {
              track_in_current_scope(reinterpret_cast<void *>(ptr_addr));
            }
        }
      return nullptr;
    }

  if (owner_depth >= 0 && owner_index != npos)
    {
      allocation_scope *owner = scope_at_depth(owner_depth);
      if (owner)
        {
          owner->replace_at(owner_index, new_ptr);
        }
      else
        {
          track_in_current_scope(new_ptr);
        }
    }
  else
    {
      track_in_current_scope(new_ptr);
    }

  return new_ptr;
}
