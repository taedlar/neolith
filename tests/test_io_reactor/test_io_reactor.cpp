#include <gtest/gtest.h>
extern "C" {
#include "port/io_reactor.h"
#include "port/socket_comm.h"
}
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

using namespace testing;

// Test suite name and test name should not contain underscores
// See: https://google.github.io/googletest/faq.html#why-should-test-suite-names-and-test-names-not-contain-underscore

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
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        return -1;
    }
    *server_fd = fds[0];
    *client_fd = fds[1];
    
    // Set both to non-blocking for realistic testing
    fcntl(*server_fd, F_SETFL, O_NONBLOCK);
    fcntl(*client_fd, F_SETFL, O_NONBLOCK);
    
    return 0;
}

/**
 * @brief Close a socket pair.
 * @param server_fd Server side socket.
 * @param client_fd Client side socket.
 */
static void close_socket_pair(socket_fd_t server_fd, socket_fd_t client_fd) {
    if (server_fd >= 0) close(server_fd);
    if (client_fd >= 0) close(client_fd);
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
    
    // Second add of same fd should fail
    EXPECT_EQ(-1, io_reactor_add(reactor, server_fd, nullptr, EVENT_READ));
    
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
    
    // Modifying non-existent fd should fail
    EXPECT_EQ(-1, io_reactor_modify(reactor, 9999, EVENT_READ));
    
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
    ssize_t written = write(client_fd, test_data, 4);
    ASSERT_EQ(4, written);
    
    // Should get read event on server side
    io_event_t events[10];
    struct timeval timeout = {1, 0};  // 1 second timeout
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    
    ASSERT_EQ(1, n) << "Expected 1 event";
    EXPECT_TRUE(events[0].event_type & EVENT_READ) << "Expected read event";
    EXPECT_EQ(test_context, events[0].context) << "Context mismatch";
    
    // Read the data to clear the event
    char buffer[10];
    ssize_t read_bytes = read(server_fd, buffer, sizeof(buffer));
    ASSERT_EQ(4, read_bytes);
    
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
    ASSERT_EQ(4, write(client1_fd, "aaa", 4));
    ASSERT_EQ(4, write(client2_fd, "bbb", 4));
    
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
    ssize_t n_read;
    n_read = read(server1_fd, buffer, sizeof(buffer));
    n_read = read(server2_fd, buffer, sizeof(buffer));
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
    ASSERT_EQ(1, write(client1_fd, "a", 1));
    ASSERT_EQ(1, write(client2_fd, "b", 1));
    ASSERT_EQ(1, write(client3_fd, "c", 1));
    
    // Request only 2 events max
    io_event_t events[2];
    struct timeval timeout = {1, 0};
    int n = io_reactor_wait(reactor, events, 2, &timeout);
    
    // Should return at most 2 events
    EXPECT_LE(n, 2);
    EXPECT_GT(n, 0);
    
    // Cleanup
    char buffer[10];
    ssize_t n_read;
    n_read = read(server1_fd, buffer, sizeof(buffer));
    n_read = read(server2_fd, buffer, sizeof(buffer));
    n_read = read(server3_fd, buffer, sizeof(buffer));
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
    
    // Register for write events (socket should be immediately writable)
    ASSERT_EQ(0, io_reactor_add(reactor, server_fd, (void*)0x5678, EVENT_WRITE));
    
    io_event_t events[10];
    struct timeval timeout = {1, 0};
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    
    ASSERT_GE(n, 1);
    EXPECT_TRUE(events[0].event_type & EVENT_WRITE);
    EXPECT_EQ((void*)0x5678, events[0].context);
    
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
        ASSERT_EQ(1, write(client_fds[i], "x", 1));
    }
    
    // Should get events for the half we wrote to
    io_event_t events[NUM_PAIRS];
    struct timeval timeout = {1, 0};
    int n = io_reactor_wait(reactor, events, NUM_PAIRS, &timeout);
    
    EXPECT_EQ(NUM_PAIRS / 2, n) << "Expected events for half the connections";
    
    // Cleanup
    char buffer[10];
    ssize_t n_read;
    for (int i = 0; i < NUM_PAIRS; i++) {
        if (i < NUM_PAIRS / 2) {
            n_read = read(server_fds[i], buffer, sizeof(buffer));
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
    char buffer[100] = "test data";
    EXPECT_EQ(0, io_reactor_post_write(reactor, server_fd, buffer, strlen(buffer)));
    
    EXPECT_EQ(0, io_reactor_remove(reactor, server_fd));
    close_socket_pair(server_fd, client_fd);
    io_reactor_destroy(reactor);
}
