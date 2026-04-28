#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"
#include <gtest/gtest.h>

using neolith::heap::allocation_scope;

namespace neolith::heap {

bool fail_reserve_for_test() noexcept {
  return true;
}

class HeapAllocationTest : public ::testing::Test {
protected:
  void TearDown() override {
    allocation_scope::set_reserve_test_hook(nullptr);
    EXPECT_EQ(allocation_scope::current_scope(), nullptr);
  }
};

TEST_F(HeapAllocationTest, ScopeTracksAllocationAndReleaseRemovesOwnership) {
  allocation_scope scope;

  void *ptr = allocation_scope::allocate(32, false, false);
  ASSERT_NE(ptr, nullptr);
  ASSERT_EQ(allocation_scope::current_scope(), &scope);
  ASSERT_EQ(scope.tracked_.size(), 1u);
  EXPECT_EQ(scope.tracked_[0], ptr);
  EXPECT_EQ(allocation_scope::find_owner(ptr), &scope);

  EXPECT_EQ(allocation_scope::release(ptr), ptr);
  EXPECT_TRUE(scope.tracked_.empty());
  EXPECT_EQ(allocation_scope::find_owner(ptr), nullptr);

  allocation_scope::deallocate(ptr);
}

TEST_F(HeapAllocationTest, AllocateReturnsNullWhenReservePreflightFails) {
  allocation_scope scope;

  allocation_scope::set_reserve_test_hook(fail_reserve_for_test);
  EXPECT_EQ(allocation_scope::allocate(32, false, false), nullptr);

  EXPECT_TRUE(scope.tracked_.empty());
}

TEST_F(HeapAllocationTest, NestedScopeRestoresParentScope) {
  allocation_scope outer;
  void *outer_ptr = allocation_scope::allocate(16, false, false);
  ASSERT_NE(outer_ptr, nullptr);
  ASSERT_EQ(allocation_scope::current_scope(), &outer);
  ASSERT_EQ(outer.tracked_.size(), 1u);

  void *inner_ptr = nullptr;
  {
    allocation_scope inner;
    inner_ptr = allocation_scope::allocate(24, false, false);
    ASSERT_NE(inner_ptr, nullptr);
    EXPECT_EQ(allocation_scope::current_scope(), &inner);
    ASSERT_EQ(inner.tracked_.size(), 1u);
    EXPECT_EQ(inner.tracked_[0], inner_ptr);
    EXPECT_EQ(allocation_scope::find_owner(inner_ptr), &inner);
  }

  EXPECT_EQ(allocation_scope::current_scope(), &outer);
  ASSERT_EQ(outer.tracked_.size(), 1u);
  EXPECT_EQ(outer.tracked_[0], outer_ptr);
  EXPECT_EQ(allocation_scope::find_owner(inner_ptr), nullptr);

  allocation_scope::release(outer_ptr);
  allocation_scope::deallocate(outer_ptr);
}

TEST_F(HeapAllocationTest, ReallocateNullBehavesLikeAllocateAndTracksResult) {
  allocation_scope scope;

  void *ptr = allocation_scope::reallocate(nullptr, 40);
  ASSERT_NE(ptr, nullptr);
  ASSERT_EQ(scope.tracked_.size(), 1u);
  EXPECT_EQ(scope.tracked_[0], ptr);
  EXPECT_EQ(allocation_scope::find_owner(ptr), &scope);

  allocation_scope::release(ptr);
  allocation_scope::deallocate(ptr);
}

TEST_F(HeapAllocationTest, ReallocateTrackedPointerUpdatesTrackedSlot) {
  allocation_scope scope;

  void *ptr = allocation_scope::allocate(8, false, false);
  ASSERT_NE(ptr, nullptr);
  ASSERT_EQ(scope.tracked_.size(), 1u);
  EXPECT_EQ(scope.tracked_[0], ptr);

  void *new_ptr = allocation_scope::reallocate(ptr, 64);
  ASSERT_NE(new_ptr, nullptr);
  ASSERT_EQ(scope.tracked_.size(), 1u);
  EXPECT_EQ(scope.tracked_[0], new_ptr);
  EXPECT_EQ(allocation_scope::find_owner(new_ptr), &scope);
  if (new_ptr != ptr)
    {
      EXPECT_EQ(allocation_scope::find_owner(ptr), nullptr);
    }

  allocation_scope::release(new_ptr);
  allocation_scope::deallocate(new_ptr);
}

TEST_F(HeapAllocationTest, ReallocateZeroFreesTrackedPointerAndClearsOwnership) {
  allocation_scope scope;

  void *ptr = allocation_scope::allocate(48, false, false);
  ASSERT_NE(ptr, nullptr);
  ASSERT_EQ(scope.tracked_.size(), 1u);
  EXPECT_EQ(allocation_scope::find_owner(ptr), &scope);

  EXPECT_EQ(allocation_scope::reallocate(ptr, 0), nullptr);
  EXPECT_TRUE(scope.tracked_.empty());
  EXPECT_EQ(allocation_scope::find_owner(ptr), nullptr);
}

TEST_F(HeapAllocationTest, ReallocateUntrackedPointerAdoptsResultIntoCurrentScope) {
  allocation_scope scope;

  void *raw_ptr = std::malloc(20);
  ASSERT_NE(raw_ptr, nullptr);
  EXPECT_EQ(allocation_scope::find_owner(raw_ptr), nullptr);

  void *new_ptr = allocation_scope::reallocate(raw_ptr, 96);
  ASSERT_NE(new_ptr, nullptr);
  ASSERT_EQ(scope.tracked_.size(), 1u);
  EXPECT_EQ(scope.tracked_[0], new_ptr);
  EXPECT_EQ(allocation_scope::find_owner(new_ptr), &scope);

  allocation_scope::release(new_ptr);
  allocation_scope::deallocate(new_ptr);
}

TEST_F(HeapAllocationTest, ReallocateUntrackedPointerPreservesOriginalWhenReservePreflightFails) {
  allocation_scope scope;

  void *raw_ptr = std::malloc(20);
  ASSERT_NE(raw_ptr, nullptr);
  EXPECT_EQ(allocation_scope::find_owner(raw_ptr), nullptr);
  EXPECT_TRUE(scope.tracked_.empty());

  allocation_scope::set_reserve_test_hook(fail_reserve_for_test);
  EXPECT_EQ(allocation_scope::reallocate(raw_ptr, 96), nullptr);

  EXPECT_TRUE(scope.tracked_.empty());
  EXPECT_EQ(allocation_scope::find_owner(raw_ptr), nullptr);

  std::free(raw_ptr);
}

} // namespace neolith::heap
