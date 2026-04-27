#pragma once

#include <stddef.h>
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

	void dismiss() noexcept;

	/**
	 * @brief Release a pointer from tracking in the active scope.
	 *
	 * Untracks the pointer so the scope destructor will not free it.
	 * Useful when ownership is transferred back to caller.
	 * @param ptr Pointer to release from tracking (or nullptr).
	 * @return The same pointer for chaining in return statements.
	 */
	static void *release(void *ptr) noexcept;

	static void *allocate(size_t size, bool zeroed, bool no_fail) noexcept;

	static void *reallocate(void *ptr, size_t size) noexcept;

	static void deallocate(void *ptr) noexcept;

private:
	static allocation_scope *&current_scope() noexcept;

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

inline void *dxalloc(size_t size) {
	return allocation_scope::allocate(size, false, true);
}

inline void *dmalloc(size_t size) {
	return allocation_scope::allocate(size, false, false);
}

inline void *dcalloc(size_t count, size_t size) {
    if (size && count > SIZE_MAX / size) {
        return nullptr; // Prevent overflow
    }
	return allocation_scope::allocate(count * size, true, false);
}

inline void *drealloc(void *ptr, size_t size) {
	return allocation_scope::reallocate(ptr, size);
}

inline void dfree(void *ptr) {
	allocation_scope::deallocate(ptr);
}

} // namespace neolith::heap

#define NEOLITH_HEAP_SCOPE(name) ::neolith::heap::allocation_scope name

/**
 * @brief Release a pointer from scope tracking when ownership is transferred.
 *
 * Use this when returning/assigning memory allocated via ALLOC macros
 * to ensure the scope will not free it upon unwind.
 * Example: return NEOLITH_HEAP_RELEASE(obtab);
 */
#define NEOLITH_HEAP_RELEASE(ptr) ::neolith::heap::allocation_scope::release(ptr)

#define DXALLOC(x,tag,desc)     ::neolith::heap::dxalloc((x))
#define DMALLOC(x,tag,desc)     ::neolith::heap::dmalloc((x))
#define DREALLOC(x,y,tag,desc)  ::neolith::heap::drealloc((x), (y))
#define DCALLOC(x,y,tag,desc)   ::neolith::heap::dcalloc((x), (y))

#define FREE(x)                 ::neolith::heap::dfree((x))

#else
#define DXALLOC(x,tag,desc)     xalloc(x)
#define DMALLOC(x,tag,desc)     malloc(x)
#define DREALLOC(x,y,tag,desc)  realloc(x,y)
#define DCALLOC(x,y,tag,desc)   calloc(x,y)

#define FREE(x)         free(x)
#endif
