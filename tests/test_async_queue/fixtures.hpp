#pragma once
#include "src/std.h"
#include "port/socket_comm.h"
#include "async/async_queue.h"
#include "async/async_worker.h"

#include <gtest/gtest.h>

class AsyncQueueTest : public ::testing::Test {
protected:
    async_queue_t* queue = nullptr;
    
    void TearDown() override {
        if (queue) {
            async_queue_destroy(queue);
            queue = nullptr;
        }
    }
};
