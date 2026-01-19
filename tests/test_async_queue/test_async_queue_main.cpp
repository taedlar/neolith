/**
 * @file test_async_queue_main.cpp
 * @brief Test runner for async_queue tests
 */

#include <gtest/gtest.h>

#ifdef _WIN32
#include <windows.h>

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
