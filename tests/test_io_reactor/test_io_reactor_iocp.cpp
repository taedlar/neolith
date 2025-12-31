/**
 * @file test_io_reactor_iocp.cpp
 * @brief Windows IOCP-specific reactor tests
 */

#include "test_io_reactor_common.h"

using namespace testing;

#ifdef WINSOCK
/*
 * =============================================================================
 * Windows IOCP-Specific Tests
 * =============================================================================
 */

TEST(IOReactorIOCPTest, CompletionWithDataInBuffer) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    socket_fd_t server_fd = INVALID_SOCKET_FD, client_fd = INVALID_SOCKET_FD;
    ASSERT_EQ(0, create_socket_pair(&server_fd, &client_fd));
    
    // Register and post async read
    ASSERT_EQ(0, io_reactor_add(reactor, server_fd, (void*)0xBEEF, EVENT_READ));
    
    // Write data
    const char* test_data = "IOCP test data";
    ASSERT_EQ((int)strlen(test_data), SOCKET_SEND(client_fd, test_data, strlen(test_data), 0));
    
    // Wait for completion
    io_event_t events[10];
    struct timeval timeout = {2, 0};
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    
    ASSERT_EQ(1, n) << "Expected 1 completion event";
    EXPECT_TRUE(events[0].event_type & EVENT_READ);
    EXPECT_EQ((int)strlen(test_data), events[0].bytes_transferred);
    EXPECT_NE(events[0].buffer, nullptr) << "Buffer should contain data";
    EXPECT_EQ(0, strncmp((char*)events[0].buffer, test_data, strlen(test_data)));
    
    // Cleanup
    EXPECT_EQ(0, io_reactor_remove(reactor, server_fd));
    close_socket_pair(server_fd, client_fd);
    io_reactor_destroy(reactor);
}

TEST(IOReactorIOCPTest, GracefulClose) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    socket_fd_t server_fd = INVALID_SOCKET_FD, client_fd = INVALID_SOCKET_FD;
    ASSERT_EQ(0, create_socket_pair(&server_fd, &client_fd));
    
    // Register and post async read
    ASSERT_EQ(0, io_reactor_add(reactor, server_fd, nullptr, EVENT_READ));
    
    // Close client side (graceful shutdown)
    SOCKET_CLOSE(client_fd);
    client_fd = INVALID_SOCKET_FD;
    
    // Should get close event
    io_event_t events[10];
    struct timeval timeout = {2, 0};
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    
    ASSERT_EQ(1, n);
    EXPECT_TRUE(events[0].event_type & EVENT_CLOSE);
    EXPECT_EQ(0, events[0].bytes_transferred);
    
    // Cleanup
    EXPECT_EQ(0, io_reactor_remove(reactor, server_fd));
    SOCKET_CLOSE(server_fd);
    io_reactor_destroy(reactor);
}

TEST(IOReactorIOCPTest, CancelledOperations) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    socket_fd_t server_fd = INVALID_SOCKET_FD, client_fd = INVALID_SOCKET_FD;
    ASSERT_EQ(0, create_socket_pair(&server_fd, &client_fd));
    
    // Register and post async read
    ASSERT_EQ(0, io_reactor_add(reactor, server_fd, nullptr, EVENT_READ));
    
    // Remove immediately (should cancel pending I/O)
    EXPECT_EQ(0, io_reactor_remove(reactor, server_fd));
    
    // Wait to see if we get cancelled completion
    io_event_t events[10];
    struct timeval timeout = {0, 500000};  // 500ms
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    
    // May or may not get a completion event (depends on timing)
    // If we do, it should be a CLOSE event due to cancellation
    if (n > 0) {
        EXPECT_TRUE(events[0].event_type & (EVENT_CLOSE | EVENT_ERROR));
    }
    
    // Cleanup
    close_socket_pair(server_fd, client_fd);
    io_reactor_destroy(reactor);
}

TEST(IOReactorIOCPTest, MultipleReadsOnSameSocket) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    socket_fd_t server_fd = INVALID_SOCKET_FD, client_fd = INVALID_SOCKET_FD;
    ASSERT_EQ(0, create_socket_pair(&server_fd, &client_fd));
    
    // Register socket
    ASSERT_EQ(0, io_reactor_add(reactor, server_fd, nullptr, EVENT_READ));
    
    // Send first message
    const char* msg1 = "first";
    ASSERT_EQ((int)strlen(msg1), SOCKET_SEND(client_fd, msg1, strlen(msg1), 0));
    
    // Get first completion
    io_event_t events[10];
    struct timeval timeout = {1, 0};
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    ASSERT_EQ(1, n);
    EXPECT_EQ((int)strlen(msg1), events[0].bytes_transferred);
    
    // Post another read
    ASSERT_EQ(0, io_reactor_post_read(reactor, server_fd, nullptr, 0));
    
    // Send second message
    const char* msg2 = "second";
    ASSERT_EQ((int)strlen(msg2), SOCKET_SEND(client_fd, msg2, strlen(msg2), 0));
    
    // Get second completion
    n = io_reactor_wait(reactor, events, 10, &timeout);
    ASSERT_EQ(1, n);
    EXPECT_EQ((int)strlen(msg2), events[0].bytes_transferred);
    EXPECT_EQ(0, strncmp((char*)events[0].buffer, msg2, strlen(msg2)));
    
    // Cleanup
    EXPECT_EQ(0, io_reactor_remove(reactor, server_fd));
    close_socket_pair(server_fd, client_fd);
    io_reactor_destroy(reactor);
}

#endif  // WINSOCK
