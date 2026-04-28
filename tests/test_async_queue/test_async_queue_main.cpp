/**
 * @file test_async_queue_main.cpp
 * @brief Test runner for async_queue tests
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */
#include "src/std.h"
#include "port/socket_comm.h"

#include <gtest/gtest.h>

#ifdef _WIN32
class WinsockEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        WSADATA wsa_data;
        int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
        if (result != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
    }
    
    void TearDown() override {
        WSACleanup();
    }
};

static ::testing::Environment* const winsock_env =
    ::testing::AddGlobalTestEnvironment(new WinsockEnvironment);
#endif

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
