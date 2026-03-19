#pragma once

#include <gtest/gtest.h>

/**
 * Behavior compatibility skeleton fixture for socket efun tests.
 *
 * Each test maps directly to the SOCK_BHV_XXX IDs in
 * docs/plan/socket-operation-engine.md.
 */
class SocketEfunsBehaviorTest : public ::testing::Test {
protected:
  void BehaviorSkeletonOnly(const char* test_id) {
    GTEST_SKIP() << test_id << " is a behavior skeleton placeholder. "
                 << "Implement setup/action/assertions per matrix.";
  }
};
