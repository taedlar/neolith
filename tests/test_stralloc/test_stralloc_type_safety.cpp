#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"

#include <gtest/gtest.h>
#include <type_traits>

namespace {

using ref_string_sig_t = shared_str_t (*)(shared_str_handle_t);
using free_string_sig_t = void (*)(shared_str_handle_t);
using extend_string_sig_t = malloc_str_t (*)(malloc_str_handle_t, size_t);
using unlink_string_sig_t = malloc_str_t (*)(malloc_str_handle_t);

static_assert(std::is_same<decltype(&ref_string), ref_string_sig_t>::value,
              "ref_string signature must require shared_str_handle_t");
static_assert(std::is_same<decltype(&free_string), free_string_sig_t>::value,
              "free_string signature must require shared_str_handle_t");
static_assert(std::is_same<decltype(&int_extend_string), extend_string_sig_t>::value,
              "int_extend_string signature must require malloc_str_handle_t");
static_assert(std::is_same<decltype(&int_string_unlink), unlink_string_sig_t>::value,
              "int_string_unlink signature must require malloc_str_handle_t");

static_assert(std::is_same<decltype(to_shared_str((char *)nullptr)), shared_str_handle_t>::value,
              "to_shared_str must produce shared_str_handle_t");
static_assert(std::is_same<decltype(to_malloc_str((char *)nullptr)), malloc_str_handle_t>::value,
              "to_malloc_str must produce malloc_str_handle_t");

#ifdef STRING_TYPE_SAFETY
static_assert(!std::is_same<shared_str_handle_t, char *>::value,
              "shared_str_handle_t must be opaque under STRING_TYPE_SAFETY");
static_assert(!std::is_same<malloc_str_handle_t, char *>::value,
              "malloc_str_handle_t must be opaque under STRING_TYPE_SAFETY");
#else
static_assert(std::is_same<shared_str_handle_t, char *>::value,
              "shared_str_handle_t must be char* when STRING_TYPE_SAFETY is off");
static_assert(std::is_same<malloc_str_handle_t, char *>::value,
              "malloc_str_handle_t must be char* when STRING_TYPE_SAFETY is off");
#endif

}  // namespace

TEST(StrAllocTypeSafetyTest, BridgeRoundTripPreservesPointer) {
  char buffer[4] = {'a', 'b', 'c', '\0'};

  shared_str_handle_t sh = to_shared_str(buffer);
  malloc_str_handle_t mh = to_malloc_str(buffer);

  EXPECT_EQ(SHARED_STR_P(sh), buffer);
  EXPECT_EQ(MALLOC_STR_P(mh), buffer);
}
