#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"

extern "C" {
    #include "lpc/object.h"
    #include "lpc/otable.h"
}

using namespace testing;

TEST_F(StackMachineTest, objectHash) {
    EXPECT_EQ(lookup_object_hash("non_existent_object"), nullptr);

    object_t test_ob; // object hash does not presume dynamic allocation of objects.
    memset(&test_ob, 0, sizeof(object_t));
    test_ob.ref = 1; // prevent accidental deallocation
    test_ob.name = make_shared_string("/test_object"); // object name must be a shared string

    ASSERT_NO_THROW(enter_object_hash(&test_ob));
    EXPECT_EQ(lookup_object_hash("/test_object"), &test_ob);

    // Attempting to re-enter the same object should not throw an error
    ASSERT_NO_THROW(enter_object_hash(&test_ob));

    remove_object_hash(&test_ob);
    EXPECT_EQ(lookup_object_hash("/test_object"), nullptr);
    EXPECT_EQ(test_ob.next_hash, nullptr);

    free_string(test_ob.name);

    // object hash has nothing to do with object lifecycle, an object can exist after removal
    // from the hash table.
}
