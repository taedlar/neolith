#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/*  dynamic LPC memory allocations:
 *
 *  DXALLOC - allocation that never fails. Exits on failure.
 *  DMALLOC - generic allocation. Returns NULL on failure.
 *  DREALLOC - generic re-allocation. Returns NULL on failure.
 *  DCALLOC - generic cleared-allocation. Returns NULL on failure.
 *  FREE - free memory allocated by any of the above.
 */
extern char *reserved_area;
char *xalloc(size_t);

#ifdef __cplusplus
}

#include <cstdint>
#include <vector>
#ifdef HAVE_GTEST
/* for FRIEND_TEST */
#include <gtest/gtest_prod.h>
#endif

namespace neolith::heap {

/**
 * @brief Tracks heap allocations created through *ALLOC macros in C++ scopes.
 *
 * Use NEOLITH_HEAP_SCOPE(name) at function/block entry where temporary
 * allocations should auto-release when the stack unwinds (including exceptions).
 */
class allocation_scope {
public:
  allocation_scope() noexcept;

  allocation_scope(const allocation_scope &) = delete;
  allocation_scope &operator=(const allocation_scope &) = delete;

  ~allocation_scope() noexcept;

  /**
   * @brief Release a pointer from tracking in the active scope.
   *
   * Untracks the pointer so the scope destructor will not free it.
   * Useful when ownership is transferred back to caller.
   * @param ptr Pointer to release from tracking (or nullptr).
   * @return The same pointer for chaining in return statements.
   */
  static void *release(void *ptr) noexcept;

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
  static void *allocate(size_t size, bool zeroed, bool no_fail) noexcept;

  /**
   * @brief Reallocate memory and update tracking in the current scope.
   * If ptr is null, behaves like allocate(). If size is zero, behaves like deallocate().
   * If ptr is tracked in the current scope, updates tracking to new pointer location on success.
   * If ptr is not tracked but current scope exists, attempts to track new pointer on success.
   * If reallocation fails, original ptr is still valid and unchanged, caller should handle this case.
   * @param ptr Pointer to previously allocated memory (or nullptr).
   * @param size New size of memory to allocate.
   * @return Pointer to reallocated memory, or nullptr on failure.
   *  Original ptr is unchanged on failure.
   */
  static void *reallocate(void *ptr, size_t size) noexcept;

  /**
   * @brief Deallocate memory and update tracking in the current scope.
   * If the pointer is tracked in any active scope, it is untracked before deallocation.
   * If the pointer is not tracked, it is simply freed.
   * @param ptr Pointer to previously allocated memory (or nullptr).
   */
  static void deallocate(void *ptr) noexcept;

private:
#ifdef HAVE_GTEST
  /* white-box testing */
  friend class HeapAllocationTest;
  FRIEND_TEST(HeapAllocationTest, AllocateReturnsNullWhenReservePreflightFails);
  FRIEND_TEST(HeapAllocationTest, ReallocateUntrackedPointerAdoptsResultIntoCurrentScope);
  FRIEND_TEST(HeapAllocationTest, ReallocateNullBehavesLikeAllocateAndTracksResult);
  FRIEND_TEST(HeapAllocationTest, ScopeTracksAllocationAndReleaseRemovesOwnership);
  FRIEND_TEST(HeapAllocationTest, NestedScopeRestoresParentScope);
  FRIEND_TEST(HeapAllocationTest, ReallocateZeroFreesTrackedPointerAndClearsOwnership);
  FRIEND_TEST(HeapAllocationTest, ReallocateUntrackedPointerPreservesOriginalWhenReservePreflightFails);
  FRIEND_TEST(HeapAllocationTest, ReallocateTrackedPointerUpdatesTrackedSlot);
  using reserve_test_hook_t = bool (*)() noexcept;
  static void set_reserve_test_hook(reserve_test_hook_t hook) noexcept;
  static reserve_test_hook_t reserve_test_hook_;
#endif

  /** @brief Get the current allocation scope for the thread. */
  static allocation_scope *&current_scope() noexcept;
  static bool reserve_tracking_slot(allocation_scope *scope) noexcept;
  static bool track_in_current_scope(void *ptr) noexcept;
  static allocation_scope *find_owner(void *ptr) noexcept;

  bool contains(void *ptr) const noexcept;
  size_t index_of(void *ptr) const noexcept;
  void erase(void *ptr) noexcept;
  void replace_at(size_t index, void *new_ptr) noexcept;
  void release_all() noexcept;

  allocation_scope *parent_;
  bool active_;
  std::vector<void *> tracked_;

  static constexpr size_t npos = static_cast<size_t>(-1);
};

inline void *dcalloc(size_t count, size_t size) {
  if (size && count > SIZE_MAX / size) {
      return nullptr; // Prevent overflow
  }
  return allocation_scope::allocate(count * size, true, false);
}

} // namespace neolith::heap

/**
 * @brief RAII scope guard for heap allocations in C++.
 * Use NEOLITH_HEAP_SCOPE(name) at the start of a function or block to automatically track
 * and free heap allocations on unwind.
 *
 * Example:
 * void my_function() {
 *   NEOLITH_HEAP_SCOPE(scope);
 *   char *temp = (char *) DMALLOC(100, TAG_TEMP, "my_function: temp");
 *   // temp will be automatically freed if the function exits before manual FREE()
 *   // call, including on exceptions.
 *   // If ownership of temp is transferred to caller, use NEOLITH_HEAP_RELEASE(temp) to
 *   // untrack it before returning.
 * }
 */
#define NEOLITH_HEAP_SCOPE(name) ::neolith::heap::allocation_scope name

/**
 * @brief Release a pointer from scope tracking when ownership is transferred.
 *
 * Use this when returning/assigning memory allocated via ALLOC macros
 * to ensure the scope will not free it upon unwind.
 * Example: return NEOLITH_HEAP_RELEASE(obtab);
 */
#define NEOLITH_HEAP_RELEASE(ptr) ::neolith::heap::allocation_scope::release(ptr)

#define DXALLOC(x,tag,desc)     ::neolith::heap::allocation_scope::allocate((x), false, true)
#define DMALLOC(x,tag,desc)     ::neolith::heap::allocation_scope::allocate((x), false, false)
#define DREALLOC(x,y,tag,desc)  ::neolith::heap::allocation_scope::reallocate((x), (y))
#define DCALLOC(x,y,tag,desc)   ::neolith::heap::allocation_scope::dcalloc((x), (y))

#define FREE(x)                 ::neolith::heap::allocation_scope::deallocate((x))

#else
#define DXALLOC(x,tag,desc)     xalloc(x)
#define DMALLOC(x,tag,desc)     malloc(x)
#define DREALLOC(x,y,tag,desc)  realloc(x,y)
#define DCALLOC(x,y,tag,desc)   calloc(x,y)

#define FREE(x)         free(x)
#endif
