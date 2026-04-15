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
using push_shared_string_sig_t = void (*)(shared_str_t);
using push_malloced_string_sig_t = void (*)(malloc_str_t);
using findstring_sig_t = shared_str_t (*)(const char *, const char *);
using make_shared_string_sig_t = shared_str_t (*)(const char *, const char *);
using int_new_string_sig_t = malloc_str_t (*)(size_t);
using int_string_copy_sig_t = malloc_str_t (*)(const char *, const char *);
using shared_payload_t = shared_str_t;
using malloc_payload_t = malloc_str_t;

static_assert(std::is_same<decltype(&ref_string), ref_string_sig_t>::value,
              "ref_string signature must require shared_str_handle_t");
static_assert(std::is_same<decltype(&free_string), free_string_sig_t>::value,
              "free_string signature must require shared_str_handle_t");
static_assert(std::is_same<decltype(&findstring), findstring_sig_t>::value,
              "findstring must return shared_str_t");
static_assert(std::is_same<decltype(&make_shared_string), make_shared_string_sig_t>::value,
              "make_shared_string must return shared_str_t");
static_assert(std::is_same<decltype(&int_new_string), int_new_string_sig_t>::value,
              "int_new_string must return malloc_str_t");
static_assert(std::is_same<decltype(&int_string_copy), int_string_copy_sig_t>::value,
              "int_string_copy must return malloc_str_t");
static_assert(std::is_same<decltype(&int_extend_string), extend_string_sig_t>::value,
              "int_extend_string signature must require malloc_str_handle_t");
static_assert(std::is_same<decltype(&int_string_unlink), unlink_string_sig_t>::value,
              "int_string_unlink signature must require malloc_str_handle_t");
static_assert(std::is_same<decltype(&push_shared_string), push_shared_string_sig_t>::value,
              "push_shared_string must require shared_str_t");
static_assert(std::is_same<decltype(&push_malloced_string), push_malloced_string_sig_t>::value,
              "push_malloced_string must require malloc_str_t");

static_assert(std::is_same<decltype(to_shared_str(static_cast<shared_payload_t>(nullptr))),
                           shared_str_handle_t>::value,
              "to_shared_str must produce shared_str_handle_t");
static_assert(std::is_same<decltype(to_malloc_str(static_cast<malloc_payload_t>(nullptr))),
                           malloc_str_handle_t>::value,
              "to_malloc_str must produce malloc_str_handle_t");
static_assert(std::is_same<decltype(extend_string(static_cast<malloc_payload_t>(nullptr), 0)),
                           malloc_str_t>::value,
              "extend_string must preserve malloc_str_t result type");
static_assert(std::is_same<decltype(string_unlink(static_cast<malloc_payload_t>(nullptr), 0)),
                           malloc_str_t>::value,
              "string_unlink must preserve malloc_str_t result type");
static_assert(std::is_same<decltype(new_string(0, "test")), malloc_str_t>::value,
              "new_string must preserve malloc_str_t result type");
static_assert(std::is_same<decltype(string_copy("test", "test")), malloc_str_t>::value,
              "string_copy must preserve malloc_str_t result type");

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
  shared_str_t shared_raw = buffer;
  malloc_str_t malloc_raw = buffer;

  shared_str_handle_t sh = to_shared_str(shared_raw);
  malloc_str_handle_t mh = to_malloc_str(malloc_raw);

  EXPECT_EQ(SHARED_STR_P(sh), buffer);
  EXPECT_EQ(MALLOC_STR_P(mh), buffer);
}
