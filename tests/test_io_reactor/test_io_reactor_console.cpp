/**
 * @file test_io_reactor_console.cpp
 * @brief I/O reactor tests for console mode use cases (POSIX only)
 */

#include "test_io_reactor_common.h"

using namespace testing;

#ifndef WINSOCK
/*
 * =============================================================================
 * Console Mode Tests (POSIX only - uses pipes to simulate stdin)
 * =============================================================================
 */

TEST(IOReactorConsoleTest, BasicConsoleInput) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    // Create pipe to simulate console input
    int console_read_fd = -1, console_write_fd = -1;
    ASSERT_EQ(0, create_pipe_pair(&console_read_fd, &console_write_fd));
    
    // Register console (read end) with reactor
    void* console_context = (void*)0xC0501E;  // Mock console user context
    ASSERT_EQ(0, io_reactor_add(reactor, console_read_fd, console_context, EVENT_READ));
    
    // Initially no input available
    io_event_t events[10];
    struct timeval short_timeout = {0, 100000};  // 100ms
    int n = io_reactor_wait(reactor, events, 10, &short_timeout);
    EXPECT_EQ(0, n) << "Should timeout with no console input";
    
    // Write to console (simulate user typing)
    const char* input = "test command\n";
    ASSERT_EQ((ssize_t)strlen(input), write(console_write_fd, input, strlen(input)));
    
    // Should get read event
    struct timeval timeout = {1, 0};
    n = io_reactor_wait(reactor, events, 10, &timeout);
    
    ASSERT_EQ(1, n);
    EXPECT_EQ(console_context, events[0].context);
    EXPECT_TRUE(events[0].event_type & EVENT_READ);
    
    // Read should succeed
    char buffer[100];
    ssize_t nread = read(console_read_fd, buffer, sizeof(buffer));
    ASSERT_EQ((ssize_t)strlen(input), nread);
    EXPECT_EQ(0, strncmp(buffer, input, strlen(input)));
    
    // Cleanup
    EXPECT_EQ(0, io_reactor_remove(reactor, console_read_fd));
    close(console_read_fd);
    close(console_write_fd);
    io_reactor_destroy(reactor);
}

TEST(IOReactorConsoleTest, ConsoleWithNetworkSockets) {
    // Simulate real scenario: console user + network connections
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    // Setup console
    int console_read_fd = -1, console_write_fd = -1;
    ASSERT_EQ(0, create_pipe_pair(&console_read_fd, &console_write_fd));
    ASSERT_EQ(0, io_reactor_add(reactor, console_read_fd,
                                (void*)0xC0501E, EVENT_READ));
    
    // Setup network connections
    socket_fd_t server_fds[3];
    socket_fd_t client_fds[3];
    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(0, create_socket_pair(&server_fds[i], &client_fds[i]));
        ASSERT_EQ(0, io_reactor_add(reactor, server_fds[i],
                                   (void*)(intptr_t)(100 + i), EVENT_READ));
    }
    
    // Trigger activity: console input + network data on socket 1
    const char* console_cmd = "look\n";
    ASSERT_EQ((ssize_t)strlen(console_cmd),
              write(console_write_fd, console_cmd, strlen(console_cmd)));
    ASSERT_EQ(1, SOCKET_SEND(client_fds[1], "x", 1, 0));
    
    // Should get 2 events
    io_event_t events[10];
    struct timeval timeout = {1, 0};
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    
    ASSERT_EQ(2, n);
    
    // Verify both event types received
    bool got_console = false;
    bool got_network = false;
    
    for (int i = 0; i < n; i++) {
        if (events[i].context == (void*)0xC0501E) {
            got_console = true;
            EXPECT_TRUE(events[i].event_type & EVENT_READ);
        } else if (events[i].context == (void*)(intptr_t)101) {
            got_network = true;
            EXPECT_TRUE(events[i].event_type & EVENT_READ);
        }
    }
    
    EXPECT_TRUE(got_console) << "Should receive console event";
    EXPECT_TRUE(got_network) << "Should receive network event";
    
    // Cleanup
    char buffer[100];
    ssize_t nread = read(console_read_fd, buffer, sizeof(buffer));
    (void)nread;  // Suppress unused warning
    SOCKET_RECV(server_fds[1], buffer, sizeof(buffer), 0);
    
    EXPECT_EQ(0, io_reactor_remove(reactor, console_read_fd));
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(0, io_reactor_remove(reactor, server_fds[i]));
        close_socket_pair(server_fds[i], client_fds[i]);
    }
    
    close(console_read_fd);
    close(console_write_fd);
    io_reactor_destroy(reactor);
}

TEST(IOReactorConsoleTest, ConsoleEOF) {
    // Test EOF on console (Ctrl+D behavior)
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    int console_read_fd = -1, console_write_fd = -1;
    ASSERT_EQ(0, create_pipe_pair(&console_read_fd, &console_write_fd));
    ASSERT_EQ(0, io_reactor_add(reactor, console_read_fd,
                                (void*)0xC0501E, EVENT_READ));
    
    // Close write end to simulate EOF
    close(console_write_fd);
    console_write_fd = -1;
    
    // Should get read event (EOF is readable)
    io_event_t events[10];
    struct timeval timeout = {1, 0};
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    
    ASSERT_GE(n, 1);
    EXPECT_TRUE(events[0].event_type & (EVENT_READ | EVENT_CLOSE));
    
    // Read should return 0 (EOF)
    char buffer[100];
    ssize_t nread = read(console_read_fd, buffer, sizeof(buffer));
    EXPECT_EQ(0, nread) << "Read should return 0 on EOF";
    
    // Cleanup
    EXPECT_EQ(0, io_reactor_remove(reactor, console_read_fd));
    close(console_read_fd);
    io_reactor_destroy(reactor);
}

TEST(IOReactorConsoleTest, MultipleConsoleCommands) {
    // Test processing multiple console commands
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    int console_read_fd = -1, console_write_fd = -1;
    ASSERT_EQ(0, create_pipe_pair(&console_read_fd, &console_write_fd));
    ASSERT_EQ(0, io_reactor_add(reactor, console_read_fd,
                                (void*)0xC0501E, EVENT_READ));
    
    const char* commands[] = {"north\n", "look\n", "inventory\n"};
    const int num_commands = 3;
    
    for (int i = 0; i < num_commands; i++) {
        // Write command
        ASSERT_EQ((ssize_t)strlen(commands[i]),
                  write(console_write_fd, commands[i], strlen(commands[i])));
        
        // Wait for event
        io_event_t events[10];
        struct timeval timeout = {1, 0};
        int n = io_reactor_wait(reactor, events, 10, &timeout);
        
        ASSERT_EQ(1, n) << "Should get event for command " << i;
        EXPECT_EQ((void*)0xC0501E, events[0].context);
        EXPECT_TRUE(events[0].event_type & EVENT_READ);
        
        // Read command
        char buffer[100];
        ssize_t nread = read(console_read_fd, buffer, sizeof(buffer));
        EXPECT_EQ((ssize_t)strlen(commands[i]), nread);
        EXPECT_EQ(0, strncmp(buffer, commands[i], strlen(commands[i])));
    }
    
    // Cleanup
    EXPECT_EQ(0, io_reactor_remove(reactor, console_read_fd));
    close(console_read_fd);
    close(console_write_fd);
    io_reactor_destroy(reactor);
}

TEST(IOReactorConsoleTest, ConsoleNoEventsWhenNoInput) {
    // Verify console doesn't generate spurious events
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    int console_read_fd = -1, console_write_fd = -1;
    ASSERT_EQ(0, create_pipe_pair(&console_read_fd, &console_write_fd));
    ASSERT_EQ(0, io_reactor_add(reactor, console_read_fd,
                                (void*)0xC0501E, EVENT_READ));
    
    // Wait without writing anything
    io_event_t events[10];
    struct timeval timeout = {0, 500000};  // 500ms
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    
    EXPECT_EQ(0, n) << "Should timeout with no console activity";
    
    // Cleanup
    EXPECT_EQ(0, io_reactor_remove(reactor, console_read_fd));
    close(console_read_fd);
    close(console_write_fd);
    io_reactor_destroy(reactor);
}

TEST(IOReactorConsoleTest, ConsoleWithListeningSocket) {
    // Test console + listening port + user connections (full driver scenario)
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    // Setup console
    int console_read_fd = -1, console_write_fd = -1;
    ASSERT_EQ(0, create_pipe_pair(&console_read_fd, &console_write_fd));
    ASSERT_EQ(0, io_reactor_add(reactor, console_read_fd,
                                (void*)0xC0501E, EVENT_READ));
    
    // Setup listening port
    int listen_port = 0;
    socket_fd_t listen_fd = create_listening_socket(0, &listen_port);
    ASSERT_NE(listen_fd, INVALID_SOCKET_FD);
    
    mock_port_def_t test_port;
    test_port.kind = 1;
    test_port.port = listen_port;
    test_port.fd = listen_fd;
    ASSERT_EQ(0, io_reactor_add(reactor, listen_fd, &test_port, EVENT_READ));
    
    // Setup existing user connection
    socket_fd_t user_server_fd = INVALID_SOCKET_FD;
    socket_fd_t user_client_fd = INVALID_SOCKET_FD;
    ASSERT_EQ(0, create_socket_pair(&user_server_fd, &user_client_fd));
    ASSERT_EQ(0, io_reactor_add(reactor, user_server_fd,
                               (void*)(intptr_t)200, EVENT_READ));
    
    // Trigger all three event types
    const char* console_input = "shutdown\n";
    ASSERT_EQ((ssize_t)strlen(console_input),
              write(console_write_fd, console_input, strlen(console_input)));
    
    socket_fd_t new_connection = connect_to_port(listen_port);
    ASSERT_NE(new_connection, INVALID_SOCKET_FD);
    
    ASSERT_EQ(1, SOCKET_SEND(user_client_fd, "!", 1, 0));
    
    // Should get 3 events
    io_event_t events[10];
    struct timeval timeout = {1, 0};
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    
    ASSERT_EQ(3, n);
    
    // Verify all event types
    bool got_console = false;
    bool got_listen = false;
    bool got_user = false;
    
    for (int i = 0; i < n; i++) {
        if (events[i].context == (void*)0xC0501E) {
            got_console = true;
        } else if (events[i].context == &test_port) {
            got_listen = true;
        } else if (events[i].context == (void*)(intptr_t)200) {
            got_user = true;
        }
    }
    
    EXPECT_TRUE(got_console) << "Should get console event";
    EXPECT_TRUE(got_listen) << "Should get listening socket event";
    EXPECT_TRUE(got_user) << "Should get user socket event";
    
    // Cleanup
    char buffer[100];
    ssize_t nread = read(console_read_fd, buffer, sizeof(buffer));
    (void)nread;  // Suppress unused warning
    SOCKET_RECV(user_server_fd, buffer, sizeof(buffer), 0);
    socket_fd_t accepted = accept(listen_fd, nullptr, nullptr);
    if (accepted != INVALID_SOCKET_FD) {
        SOCKET_CLOSE(accepted);
    }
    
    EXPECT_EQ(0, io_reactor_remove(reactor, console_read_fd));
    EXPECT_EQ(0, io_reactor_remove(reactor, listen_fd));
    EXPECT_EQ(0, io_reactor_remove(reactor, user_server_fd));
    
    close(console_read_fd);
    close(console_write_fd);
    SOCKET_CLOSE(listen_fd);
    SOCKET_CLOSE(new_connection);
    close_socket_pair(user_server_fd, user_client_fd);
    
    io_reactor_destroy(reactor);
}

TEST(IOReactorConsoleTest, ConsoleLargeInput) {
    // Test handling of large console input (paste operation)
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    int console_read_fd = -1, console_write_fd = -1;
    ASSERT_EQ(0, create_pipe_pair(&console_read_fd, &console_write_fd));
    ASSERT_EQ(0, io_reactor_add(reactor, console_read_fd,
                                (void*)0xC0501E, EVENT_READ));
    
    // Write large input (1KB)
    char large_input[1024];
    for (int i = 0; i < 1023; i++) {
        large_input[i] = 'a' + (i % 26);
    }
    large_input[1023] = '\n';
    
    ASSERT_EQ(1024, write(console_write_fd, large_input, 1024));
    
    // Should get read event
    io_event_t events[10];
    struct timeval timeout = {1, 0};
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    
    ASSERT_EQ(1, n);
    EXPECT_TRUE(events[0].event_type & EVENT_READ);
    
    // Read in chunks
    char buffer[1024];
    ssize_t total_read = 0;
    ssize_t nread;
    while ((nread = read(console_read_fd, buffer + total_read,
                        sizeof(buffer) - total_read)) > 0) {
        total_read += nread;
        if (total_read >= 1024) break;
    }
    
    EXPECT_EQ(1024, total_read);
    EXPECT_EQ(0, memcmp(buffer, large_input, 1024));
    
    // Cleanup
    EXPECT_EQ(0, io_reactor_remove(reactor, console_read_fd));
    close(console_read_fd);
    close(console_write_fd);
    io_reactor_destroy(reactor);
}

#else  // WINSOCK

/*
 * =============================================================================
 * Windows Console Tests
 * =============================================================================
 */

/**
 * Test that console registration succeeds when running in a real console.
 * Note: This test may fail in CI environments without a real console.
 */
TEST(IOReactorConsoleTest, AddConsoleBasic) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(nullptr, reactor);
    
    void* console_marker = (void*)0x1;
    
    // This may fail if stdin is redirected or not a console
    int result = io_reactor_add_console(reactor, console_marker);
    
    if (result == 0) {
        // Console was registered successfully
        SUCCEED();
    } else {
        // Not a console (redirected I/O) - skip test
        GTEST_SKIP() << "Console not available (likely redirected I/O in CI)";
    }
    
    io_reactor_destroy(reactor);
}

/**
 * Test that console registration fails gracefully with null reactor.
 */
TEST(IOReactorConsoleTest, AddConsoleNullReactor) {
    void* console_marker = (void*)0x1;
    EXPECT_EQ(-1, io_reactor_add_console(nullptr, console_marker));
}

/**
 * Test console with listening sockets to ensure no interference.
 */
TEST(IOReactorConsoleTest, ConsoleWithListeningSockets) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(nullptr, reactor);
    
    void* console_marker = (void*)0x1;
    
    // Try to register console (may skip if not available)
    int console_result = io_reactor_add_console(reactor, console_marker);
    if (console_result != 0) {
        io_reactor_destroy(reactor);
        GTEST_SKIP() << "Console not available";
    }
    
    // Create listening socket
    socket_fd_t listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(INVALID_SOCKET_FD, listen_fd);
    
    // Bind to any port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;  // Let OS assign port
    
    ASSERT_EQ(0, bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)));
    
    // Get assigned port
    socklen_t addr_len = sizeof(addr);
    ASSERT_EQ(0, getsockname(listen_fd, (struct sockaddr*)&addr, &addr_len));
    
    ASSERT_EQ(0, listen(listen_fd, SOMAXCONN));
    
    // Determine if socket is listening
    int listening = 0;
    int opt_len = sizeof(listening);
    ASSERT_EQ(0, getsockopt(listen_fd, SOL_SOCKET, SO_ACCEPTCONN, 
                           (char*)&listening, (socklen_t*)&opt_len));
    ASSERT_NE(0, listening);
    
    // Register listening socket
    void* port_context = (void*)0x100;
    ASSERT_EQ(0, io_reactor_add(reactor, listen_fd, port_context, EVENT_READ));
    
    // Create client connection
    socket_fd_t client_fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(INVALID_SOCKET_FD, client_fd);
    
    // Connect to listening socket
    ASSERT_EQ(0, connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)));
    
    // Wait for events (short timeout)
    io_event_t events[10];
    struct timeval timeout = {0, 100000};  // 100ms
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    
    // Should get event for listening socket (console may or may not have events)
    ASSERT_GT(n, 0);
    
    // Find the listening socket event
    bool found_listen_event = false;
    for (int i = 0; i < n; i++) {
        if (events[i].context == port_context) {
            EXPECT_TRUE(events[i].event_type & EVENT_READ);
            found_listen_event = true;
        }
    }
    
    EXPECT_TRUE(found_listen_event) << "Should receive event for listening socket";
    
    // Cleanup
    SOCKET_CLOSE(client_fd);
    SOCKET_CLOSE(listen_fd);
    io_reactor_destroy(reactor);
}

/**
 * Test console with network connections to ensure full integration.
 */
TEST(IOReactorConsoleTest, ConsoleWithNetworkConnections) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(nullptr, reactor);
    
    void* console_marker = (void*)0x1;
    
    // Try to register console
    int console_result = io_reactor_add_console(reactor, console_marker);
    if (console_result != 0) {
        io_reactor_destroy(reactor);
        GTEST_SKIP() << "Console not available";
    }
    
    // Create socket pair for testing
    socket_fd_t server_fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(INVALID_SOCKET_FD, server_fd);
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    
    ASSERT_EQ(0, bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)));
    
    socklen_t addr_len = sizeof(addr);
    ASSERT_EQ(0, getsockname(server_fd, (struct sockaddr*)&addr, &addr_len));
    
    ASSERT_EQ(0, listen(server_fd, SOMAXCONN));
    
    // Register listening socket
    void* server_context = (void*)0x100;
    ASSERT_EQ(0, io_reactor_add(reactor, server_fd, server_context, EVENT_READ));
    
    // Create client and connect
    socket_fd_t client_fd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(INVALID_SOCKET_FD, client_fd);
    ASSERT_EQ(0, connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)));
    
    // Accept connection
    socket_fd_t accepted_fd = accept(server_fd, NULL, NULL);
    ASSERT_NE(INVALID_SOCKET_FD, accepted_fd);
    
    // Register accepted socket with IOCP
    void* accepted_context = (void*)0x200;
    ASSERT_EQ(0, io_reactor_add(reactor, accepted_fd, accepted_context, EVENT_READ));
    
    // Post async read on accepted socket
    ASSERT_EQ(0, io_reactor_post_read(reactor, accepted_fd, NULL, 0));
    
    // Send data from client
    const char* msg = "Test message";
    int sent = send(client_fd, msg, (int)strlen(msg), 0);
    ASSERT_EQ((int)strlen(msg), sent);
    
    // Wait for read completion
    io_event_t events[10];
    struct timeval timeout = {1, 0};  // 1 second
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    
    ASSERT_GT(n, 0);
    
    // Verify we got the read event (not console event)
    bool found_read_event = false;
    for (int i = 0; i < n; i++) {
        if (events[i].context == accepted_context) {
            EXPECT_TRUE(events[i].event_type & EVENT_READ);
            EXPECT_EQ((int)strlen(msg), events[i].bytes_transferred);
            found_read_event = true;
        }
    }
    
    EXPECT_TRUE(found_read_event) << "Should receive read event on accepted socket";
    
    // Cleanup
    SOCKET_CLOSE(client_fd);
    SOCKET_CLOSE(accepted_fd);
    SOCKET_CLOSE(server_fd);
    io_reactor_destroy(reactor);
}

/**
 * Test that console events don't block when no input is available.
 */
TEST(IOReactorConsoleTest, ConsoleNoInputDoesNotBlock) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(nullptr, reactor);
    
    void* console_marker = (void*)0x1;
    
    int console_result = io_reactor_add_console(reactor, console_marker);
    if (console_result != 0) {
        io_reactor_destroy(reactor);
        GTEST_SKIP() << "Console not available";
    }
    
    // Wait with short timeout - should return quickly even without input
    io_event_t events[10];
    struct timeval timeout = {0, 100000};  // 100ms
    
    auto start = std::chrono::steady_clock::now();
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete within reasonable time (timeout + overhead)
    EXPECT_LT(elapsed.count(), 500) << "Wait should not block excessively";
    
    // Number of events depends on whether console has buffered input
    EXPECT_GE(n, 0);
    
    io_reactor_destroy(reactor);
}

#endif  // !WINSOCK
