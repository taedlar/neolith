#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <gtest/gtest.h>
extern "C" {
#include "port/io_reactor.h"
#include "port/socket_comm.h"
}
#include "std.h"

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

/*
 * =============================================================================
 * Helper Functions for Tests
 * =============================================================================
 */

/**
 * @brief Create a socket pair for testing.
 * @param server_fd Output: server side of the socket pair.
 * @param client_fd Output: client side of the socket pair.
 * @return 0 on success, -1 on failure.
 */
static int create_socket_pair(socket_fd_t *server_fd, socket_fd_t *client_fd) {
    socket_fd_t fds[2];
    if (create_test_socket_pair(fds) != 0) {
        return -1;
    }
    *server_fd = fds[0];
    *client_fd = fds[1];
    return 0;
}

/**
 * @brief Close a socket pair.
 * @param server_fd Server side socket.
 * @param client_fd Client side socket.
 */
static void close_socket_pair(socket_fd_t server_fd, socket_fd_t client_fd) {
    if (server_fd != INVALID_SOCKET_FD) SOCKET_CLOSE(server_fd);
    if (client_fd != INVALID_SOCKET_FD) SOCKET_CLOSE(client_fd);
}

/*
 * =============================================================================
 * Lifecycle Tests
 * =============================================================================
 */

TEST(IOReactorTest, CreateDestroy) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr) << "Failed to create reactor";
    io_reactor_destroy(reactor);
}

TEST(IOReactorTest, CreateMultiple) {
    // Test that we can create multiple reactors independently
    io_reactor_t* reactor1 = io_reactor_create();
    io_reactor_t* reactor2 = io_reactor_create();
    
    ASSERT_NE(reactor1, nullptr);
    ASSERT_NE(reactor2, nullptr);
    ASSERT_NE(reactor1, reactor2);
    
    io_reactor_destroy(reactor1);
    io_reactor_destroy(reactor2);
}

TEST(IOReactorTest, DestroyNull) {
    // Should not crash on NULL pointer
    io_reactor_destroy(nullptr);
}

/*
 * =============================================================================
 * Registration Tests
 * =============================================================================
 */

TEST(IOReactorTest, AddRemoveSocket) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    socket_fd_t server_fd, client_fd;
    ASSERT_EQ(0, create_socket_pair(&server_fd, &client_fd));
    
    // Add socket to reactor
    EXPECT_EQ(0, io_reactor_add(reactor, server_fd, nullptr, EVENT_READ));
    
    // Remove socket from reactor
    EXPECT_EQ(0, io_reactor_remove(reactor, server_fd));
    
    close_socket_pair(server_fd, client_fd);
    io_reactor_destroy(reactor);
}

TEST(IOReactorTest, AddWithContext) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    socket_fd_t server_fd, client_fd;
    ASSERT_EQ(0, create_socket_pair(&server_fd, &client_fd));
    
    // Use a dummy pointer as context
    void* test_context = (void*)0x12345678;
    
    EXPECT_EQ(0, io_reactor_add(reactor, server_fd, test_context, EVENT_READ));
    
    // Verify context is returned with events (we'll do this in EventDelivery test)
    
    EXPECT_EQ(0, io_reactor_remove(reactor, server_fd));
    close_socket_pair(server_fd, client_fd);
    io_reactor_destroy(reactor);
}

TEST(IOReactorTest, AddDuplicateFails) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    socket_fd_t server_fd, client_fd;
    ASSERT_EQ(0, create_socket_pair(&server_fd, &client_fd));
    
    // First add should succeed
    EXPECT_EQ(0, io_reactor_add(reactor, server_fd, nullptr, EVENT_READ));
    
#ifndef WINSOCK
    // On POSIX, second add of same fd should fail
    // On Windows IOCP, duplicate adds may be allowed (different overlapped operations)
    EXPECT_EQ(-1, io_reactor_add(reactor, server_fd, nullptr, EVENT_READ));
#endif
    
    EXPECT_EQ(0, io_reactor_remove(reactor, server_fd));
    close_socket_pair(server_fd, client_fd);
    io_reactor_destroy(reactor);
}

TEST(IOReactorTest, RemoveNonExistent) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    // Removing a non-existent fd should be safe (no-op, returns 0)
    EXPECT_EQ(0, io_reactor_remove(reactor, 9999));
    
    io_reactor_destroy(reactor);
}

TEST(IOReactorTest, ModifyEvents) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    socket_fd_t server_fd, client_fd;
    ASSERT_EQ(0, create_socket_pair(&server_fd, &client_fd));
    
    // Add with read events
    EXPECT_EQ(0, io_reactor_add(reactor, server_fd, nullptr, EVENT_READ));
    
    // Modify to read+write events
    EXPECT_EQ(0, io_reactor_modify(reactor, server_fd, EVENT_READ | EVENT_WRITE));
    
    // Modify to write-only
    EXPECT_EQ(0, io_reactor_modify(reactor, server_fd, EVENT_WRITE));
    
    // Modify to no events
    EXPECT_EQ(0, io_reactor_modify(reactor, server_fd, 0));
    
    EXPECT_EQ(0, io_reactor_remove(reactor, server_fd));
    close_socket_pair(server_fd, client_fd);
    io_reactor_destroy(reactor);
}

TEST(IOReactorTest, ModifyNonExistentFails) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
#ifndef WINSOCK
    // On POSIX, modifying non-existent fd should fail
    // On Windows IOCP, modify is a no-op and always succeeds
    EXPECT_EQ(-1, io_reactor_modify(reactor, 9999, EVENT_READ));
#else
    // On Windows IOCP, modify is a no-op (event interest managed by post operations)
    EXPECT_EQ(0, io_reactor_modify(reactor, 9999, EVENT_READ));
#endif
    
    io_reactor_destroy(reactor);
}

/*
 * =============================================================================
 * Event Wait Tests
 * =============================================================================
 */

TEST(IOReactorTest, TimeoutNoEvents) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    io_event_t events[10];
    struct timeval timeout = {0, 100000};  // 100ms
    
    // Should timeout with no registered sockets
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    EXPECT_EQ(0, n);
    
    io_reactor_destroy(reactor);
}

TEST(IOReactorTest, EventDelivery) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    socket_fd_t server_fd, client_fd;
    ASSERT_EQ(0, create_socket_pair(&server_fd, &client_fd));
    
    void* test_context = (void*)0x1234;
    ASSERT_EQ(0, io_reactor_add(reactor, server_fd, test_context, EVENT_READ));
    
    // Write data to client side to trigger read event on server side
    const char* test_data = "test";
    int written = SOCKET_SEND(client_fd, test_data, 4, 0);
    ASSERT_EQ(4, written);
    
    // Should get read event on server side
    io_event_t events[10];
    struct timeval timeout = {1, 0};  // 1 second timeout
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    
    ASSERT_EQ(1, n) << "Expected 1 event";
    EXPECT_TRUE(events[0].event_type & EVENT_READ) << "Expected read event";
    
#ifdef WINSOCK
    // On Windows IOCP, context may be NULL because it's stored in the operation context
    // The application should track fd -> user context mapping separately if needed
    // For this test, we just verify the data was read correctly
    EXPECT_EQ(4, events[0].bytes_transferred) << "Expected 4 bytes transferred";
    EXPECT_EQ(0, strncmp((char*)events[0].buffer, test_data, 4)) << "Data mismatch";
    
    // On IOCP, we need to post another read for the next event
    io_reactor_post_read(reactor, server_fd, nullptr, 0);
#else
    EXPECT_EQ(test_context, events[0].context) << "Context mismatch";
    
    // Read the data to clear the event
    char buffer[10];
    int read_bytes = SOCKET_RECV(server_fd, buffer, sizeof(buffer), 0);
    ASSERT_EQ(4, read_bytes);
#endif
    
    // Cleanup
    EXPECT_EQ(0, io_reactor_remove(reactor, server_fd));
    close_socket_pair(server_fd, client_fd);
    io_reactor_destroy(reactor);
}

TEST(IOReactorTest, MultipleEvents) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    socket_fd_t server1_fd, client1_fd;
    socket_fd_t server2_fd, client2_fd;
    ASSERT_EQ(0, create_socket_pair(&server1_fd, &client1_fd));
    ASSERT_EQ(0, create_socket_pair(&server2_fd, &client2_fd));
    
    void* context1 = (void*)0x1111;
    void* context2 = (void*)0x2222;
    
    ASSERT_EQ(0, io_reactor_add(reactor, server1_fd, context1, EVENT_READ));
    ASSERT_EQ(0, io_reactor_add(reactor, server2_fd, context2, EVENT_READ));
    
    // Write to both client sockets
    ASSERT_EQ(4, SOCKET_SEND(client1_fd, "aaa", 4, 0));
    ASSERT_EQ(4, SOCKET_SEND(client2_fd, "bbb", 4, 0));
    
    // Should get events on both server sockets
    io_event_t events[10];
    struct timeval timeout = {1, 0};
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    
    ASSERT_EQ(2, n) << "Expected 2 events";
    
    // Events may arrive in any order, so check both
    bool found_context1 = false;
    bool found_context2 = false;
    for (int i = 0; i < n; i++) {
        EXPECT_TRUE(events[i].event_type & EVENT_READ);
        if (events[i].context == context1) found_context1 = true;
        if (events[i].context == context2) found_context2 = true;
    }
    EXPECT_TRUE(found_context1) << "Context1 event not found";
    EXPECT_TRUE(found_context2) << "Context2 event not found";
    
    // Cleanup
    char buffer[10];
    int n_read;
    n_read = SOCKET_RECV(server1_fd, buffer, sizeof(buffer), 0);
    n_read = SOCKET_RECV(server2_fd, buffer, sizeof(buffer), 0);
    (void) n_read;
    EXPECT_EQ(0, io_reactor_remove(reactor, server1_fd));
    EXPECT_EQ(0, io_reactor_remove(reactor, server2_fd));
    close_socket_pair(server1_fd, client1_fd);
    close_socket_pair(server2_fd, client2_fd);
    io_reactor_destroy(reactor);
}

TEST(IOReactorTest, MaxEventsLimitation) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    socket_fd_t server1_fd, client1_fd;
    socket_fd_t server2_fd, client2_fd;
    socket_fd_t server3_fd, client3_fd;
    ASSERT_EQ(0, create_socket_pair(&server1_fd, &client1_fd));
    ASSERT_EQ(0, create_socket_pair(&server2_fd, &client2_fd));
    ASSERT_EQ(0, create_socket_pair(&server3_fd, &client3_fd));
    
    ASSERT_EQ(0, io_reactor_add(reactor, server1_fd, (void*)1, EVENT_READ));
    ASSERT_EQ(0, io_reactor_add(reactor, server2_fd, (void*)2, EVENT_READ));
    ASSERT_EQ(0, io_reactor_add(reactor, server3_fd, (void*)3, EVENT_READ));
    
    // Trigger all three
    ASSERT_EQ(1, SOCKET_SEND(client1_fd, "a", 1, 0));
    ASSERT_EQ(1, SOCKET_SEND(client2_fd, "b", 1, 0));
    ASSERT_EQ(1, SOCKET_SEND(client3_fd, "c", 1, 0));
    
    // Request only 2 events max
    io_event_t events[2];
    struct timeval timeout = {1, 0};
    int n = io_reactor_wait(reactor, events, 2, &timeout);
    
    // Should return at most 2 events
    EXPECT_LE(n, 2);
    EXPECT_GT(n, 0);
    
    // Cleanup
    char buffer[10];
    int n_read;
    n_read = SOCKET_RECV(server1_fd, buffer, sizeof(buffer), 0);
    n_read = SOCKET_RECV(server2_fd, buffer, sizeof(buffer), 0);
    n_read = SOCKET_RECV(server3_fd, buffer, sizeof(buffer), 0);
    (void) n_read;
    EXPECT_EQ(0, io_reactor_remove(reactor, server1_fd));
    EXPECT_EQ(0, io_reactor_remove(reactor, server2_fd));
    EXPECT_EQ(0, io_reactor_remove(reactor, server3_fd));
    close_socket_pair(server1_fd, client1_fd);
    close_socket_pair(server2_fd, client2_fd);
    close_socket_pair(server3_fd, client3_fd);
    io_reactor_destroy(reactor);
}

TEST(IOReactorTest, WriteEvent) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    socket_fd_t server_fd, client_fd;
    ASSERT_EQ(0, create_socket_pair(&server_fd, &client_fd));
    
#ifdef WINSOCK
    // On Windows IOCP, write events are completion-based, not readiness-based
    // Register socket and post an async write
    ASSERT_EQ(0, io_reactor_add(reactor, server_fd, (void*)0x5678, 0));
    
    char test_data[] = "test write";
    ASSERT_EQ(0, io_reactor_post_write(reactor, server_fd, test_data, strlen(test_data)));
    
    io_event_t events[10];
    struct timeval timeout = {1, 0};
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    
    ASSERT_GE(n, 1);
    EXPECT_TRUE(events[0].event_type & EVENT_WRITE);
    EXPECT_GT(events[0].bytes_transferred, 0);
#else
    // On POSIX, register for write events (socket should be immediately writable)
    ASSERT_EQ(0, io_reactor_add(reactor, server_fd, (void*)0x5678, EVENT_WRITE));
    
    io_event_t events[10];
    struct timeval timeout = {1, 0};
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    
    ASSERT_GE(n, 1);
    EXPECT_TRUE(events[0].event_type & EVENT_WRITE);
    EXPECT_EQ((void*)0x5678, events[0].context);
#endif
    
    // Cleanup
    EXPECT_EQ(0, io_reactor_remove(reactor, server_fd));
    close_socket_pair(server_fd, client_fd);
    io_reactor_destroy(reactor);
}

/*
 * =============================================================================
 * Error Handling Tests
 * =============================================================================
 */

TEST(IOReactorTest, InvalidParameters) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    io_event_t events[10];
    struct timeval timeout = {0, 100000};
    
    // NULL reactor
    EXPECT_EQ(-1, io_reactor_wait(nullptr, events, 10, &timeout));
    
    // NULL events array
    EXPECT_EQ(-1, io_reactor_wait(reactor, nullptr, 10, &timeout));
    
    // Zero max_events
    EXPECT_EQ(-1, io_reactor_wait(reactor, events, 0, &timeout));
    
    // Negative max_events
    EXPECT_EQ(-1, io_reactor_wait(reactor, events, -1, &timeout));
    
    io_reactor_destroy(reactor);
}

TEST(IOReactorTest, AddInvalidFd) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    // Try to add invalid fd
    EXPECT_EQ(-1, io_reactor_add(reactor, -1, nullptr, EVENT_READ));
    
    io_reactor_destroy(reactor);
}

/*
 * =============================================================================
 * Scalability Tests
 * =============================================================================
 */

TEST(IOReactorTest, ManyConnections) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    const int NUM_PAIRS = 100;
    socket_fd_t server_fds[NUM_PAIRS];
    socket_fd_t client_fds[NUM_PAIRS];
    
    // Create and register many socket pairs
    for (int i = 0; i < NUM_PAIRS; i++) {
        ASSERT_EQ(0, create_socket_pair(&server_fds[i], &client_fds[i]));
        ASSERT_EQ(0, io_reactor_add(reactor, server_fds[i], (void*)(intptr_t)i, EVENT_READ));
    }
    
    // Write to half of them
    for (int i = 0; i < NUM_PAIRS / 2; i++) {
        ASSERT_EQ(1, SOCKET_SEND(client_fds[i], "x", 1, 0));
    }
    
    // Should get events for the half we wrote to
    io_event_t events[NUM_PAIRS];
    struct timeval timeout = {1, 0};
    int n = io_reactor_wait(reactor, events, NUM_PAIRS, &timeout);
    
    EXPECT_EQ(NUM_PAIRS / 2, n) << "Expected events for half the connections";
    
    // Cleanup
    char buffer[10];
    int n_read;
    for (int i = 0; i < NUM_PAIRS; i++) {
        if (i < NUM_PAIRS / 2) {
            n_read = SOCKET_RECV(server_fds[i], buffer, sizeof(buffer), 0);
            (void) n_read;
        }
        EXPECT_EQ(0, io_reactor_remove(reactor, server_fds[i]));
        close_socket_pair(server_fds[i], client_fds[i]);
    }
    
    io_reactor_destroy(reactor);
}

/*
 * =============================================================================
 * Platform-Specific Helper Tests (no-ops on POSIX)
 * =============================================================================
 */

TEST(IOReactorTest, PostReadNoOp) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    socket_fd_t server_fd, client_fd;
    ASSERT_EQ(0, create_socket_pair(&server_fd, &client_fd));
    
    ASSERT_EQ(0, io_reactor_add(reactor, server_fd, nullptr, EVENT_READ));
    
    // On POSIX, this should be a no-op and return 0
    // On Windows IOCP, this posts an actual async read operation
    char buffer[100];
    EXPECT_EQ(0, io_reactor_post_read(reactor, server_fd, buffer, sizeof(buffer)));
    
    EXPECT_EQ(0, io_reactor_remove(reactor, server_fd));
    close_socket_pair(server_fd, client_fd);
    io_reactor_destroy(reactor);
}

TEST(IOReactorTest, PostWriteNoOp) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    socket_fd_t server_fd, client_fd;
    ASSERT_EQ(0, create_socket_pair(&server_fd, &client_fd));
    
    ASSERT_EQ(0, io_reactor_add(reactor, server_fd, nullptr, EVENT_WRITE));
    
    // On POSIX, this should be a no-op and return 0
    // On Windows IOCP, this posts an actual async write operation
    char buffer[100] = "test data";
    EXPECT_EQ(0, io_reactor_post_write(reactor, server_fd, buffer, strlen(buffer)));
    
    EXPECT_EQ(0, io_reactor_remove(reactor, server_fd));
    close_socket_pair(server_fd, client_fd);
    io_reactor_destroy(reactor);
}

#ifdef WINSOCK
/*
 * =============================================================================
 * Windows IOCP-Specific Tests
 * =============================================================================
 */

TEST(IOReactorIOCPTest, CompletionWithDataInBuffer) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    socket_fd_t server_fd, client_fd;
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
    
    socket_fd_t server_fd, client_fd;
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
    
    socket_fd_t server_fd, client_fd;
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
    
    socket_fd_t server_fd, client_fd;
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
