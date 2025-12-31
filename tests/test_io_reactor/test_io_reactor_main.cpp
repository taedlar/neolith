/**
 * @file test_io_reactor.cpp
 * @brief Main test file for I/O reactor - includes Windows initialization
 * 
 * Test suite files:
 * - test_io_reactor_basic.cpp: Lifecycle, registration, event delivery tests
 * - test_io_reactor_listen.cpp: Listening socket tests (external_port pattern)
 * - test_io_reactor_console.cpp: Console mode tests (POSIX only)
 * - test_io_reactor_iocp.cpp: Windows IOCP-specific tests
 */

#include "test_io_reactor_common.h"

using namespace testing;

// Test suite name and test name should not contain underscores
// See: https://google.github.io/googletest/faq.html#why-should-test-suite-names-and-test-names-not-contain-underscore

#ifdef WINSOCK
/**
 * @brief Global test environment for Windows Sockets initialization.
 */
class WinsockEnvironment : public ::testing::Environment {
public:
    virtual void SetUp() override {
        WSADATA wsa_data;
        int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
        if (result != 0) {
            std::cerr << "WSAStartup failed with error: " << result << std::endl;
            exit(1);
        }
    }
    
    virtual void TearDown() override {
        WSACleanup();
    }
};

// Register the global environment
static ::testing::Environment* const winsock_env =
    ::testing::AddGlobalTestEnvironment(new WinsockEnvironment);
#endif
