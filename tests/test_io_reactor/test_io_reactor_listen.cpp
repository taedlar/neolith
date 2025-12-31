/**
 * @file test_io_reactor_listen.cpp
 * @brief I/O reactor tests for listening socket use cases (external_port integration)
 */

#include "test_io_reactor_common.h"

using namespace testing;

/*
 * =============================================================================
 * Listening Socket Tests (for external_port use case)
 * =============================================================================
 */

TEST(IOReactorListenTest, BasicListenAccept) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    // Create listening socket on auto-assigned port
    int actual_port = 0;
    socket_fd_t listen_fd = create_listening_socket(0, &actual_port);
    ASSERT_NE(listen_fd, INVALID_SOCKET_FD);
    ASSERT_GT(actual_port, 0);
    
    mock_port_def_t test_port;
    test_port.kind = 1;  // PORT_TELNET
    test_port.port = actual_port;
    test_port.fd = listen_fd;
    
    // Register listening socket with reactor
    ASSERT_EQ(0, io_reactor_add(reactor, listen_fd, &test_port, EVENT_READ));
    
    // Initially no events (no incoming connections)
    io_event_t events[10];
    struct timeval short_timeout = {0, 100000};  // 100ms
    int n = io_reactor_wait(reactor, events, 10, &short_timeout);
    EXPECT_EQ(0, n) << "Should have no events before connection";
    
    // Connect from client
    socket_fd_t client_fd = connect_to_port(actual_port);
    ASSERT_NE(client_fd, INVALID_SOCKET_FD);
    
    // Wait for accept readiness event
    struct timeval timeout = {1, 0};
    n = io_reactor_wait(reactor, events, 10, &timeout);
    
    ASSERT_EQ(1, n) << "Should have exactly one event for listening socket";
    EXPECT_EQ(&test_port, events[0].context) << "Context should be port_def_t pointer";
    EXPECT_TRUE(events[0].event_type & EVENT_READ) << "Should be read event";
    
    // Accept should succeed without blocking
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    socket_fd_t accepted_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &addr_len);
    ASSERT_NE(accepted_fd, INVALID_SOCKET_FD) << "Accept should succeed";
    
    // Cleanup
    SOCKET_CLOSE(accepted_fd);
    SOCKET_CLOSE(client_fd);
    EXPECT_EQ(0, io_reactor_remove(reactor, listen_fd));
    SOCKET_CLOSE(listen_fd);
    io_reactor_destroy(reactor);
}

TEST(IOReactorListenTest, MultipleListeningPorts) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    const int NUM_PORTS = 5;
    mock_port_def_t ports[NUM_PORTS];
    
    // Setup multiple listening ports (simulating external_port[5])
    for (int i = 0; i < NUM_PORTS; i++) {
        int actual_port = 0;
        ports[i].fd = create_listening_socket(0, &actual_port);
        ASSERT_NE(ports[i].fd, INVALID_SOCKET_FD);
        ports[i].port = actual_port;
        ports[i].kind = 1 + (i % 3);  // Cycle through PORT_TELNET, PORT_BINARY, PORT_ASCII
        
        // Register with reactor
        ASSERT_EQ(0, io_reactor_add(reactor, ports[i].fd, &ports[i], EVENT_READ));
    }
    
    // Connect to port 2 (middle port)
    socket_fd_t client_fd = connect_to_port(ports[2].port);
    ASSERT_NE(client_fd, INVALID_SOCKET_FD);
    
    // Wait for event
    io_event_t events[10];
    struct timeval timeout = {1, 0};
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    
    ASSERT_EQ(1, n);
    mock_port_def_t* ready_port = (mock_port_def_t*)events[0].context;
    EXPECT_EQ(&ports[2], ready_port) << "Should be port 2";
    EXPECT_EQ(ports[2].port, ready_port->port);
    EXPECT_TRUE(events[0].event_type & EVENT_READ);
    
    // Accept connection
    socket_fd_t accepted_fd = accept(ports[2].fd, nullptr, nullptr);
    ASSERT_NE(accepted_fd, INVALID_SOCKET_FD);
    
    // Cleanup
    SOCKET_CLOSE(accepted_fd);
    SOCKET_CLOSE(client_fd);
    for (int i = 0; i < NUM_PORTS; i++) {
        EXPECT_EQ(0, io_reactor_remove(reactor, ports[i].fd));
        SOCKET_CLOSE(ports[i].fd);
    }
    io_reactor_destroy(reactor);
}

TEST(IOReactorListenTest, MultipleSimultaneousConnections) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    // Single listening port
    int actual_port = 0;
    socket_fd_t listen_fd = create_listening_socket(0, &actual_port);
    ASSERT_NE(listen_fd, INVALID_SOCKET_FD);
    
    mock_port_def_t test_port;
    test_port.kind = 1;
    test_port.port = actual_port;
    test_port.fd = listen_fd;
    ASSERT_EQ(0, io_reactor_add(reactor, listen_fd, &test_port, EVENT_READ));
    
    // Connect multiple clients rapidly
    const int NUM_CLIENTS = 5;
    socket_fd_t client_fds[NUM_CLIENTS];
    for (int i = 0; i < NUM_CLIENTS; i++) {
        client_fds[i] = connect_to_port(actual_port);
        ASSERT_NE(client_fds[i], INVALID_SOCKET_FD);
    }
    
    // Should get event for pending connections
    io_event_t events[10];
    struct timeval timeout = {1, 0};
    
    // Accept all connections (may need multiple event loop iterations)
    socket_fd_t accepted_fds[NUM_CLIENTS];
    int num_accepted = 0;
    
    for (int iter = 0; iter < 10 && num_accepted < NUM_CLIENTS; iter++) {
        int n = io_reactor_wait(reactor, events, 10, &timeout);
        
        if (n > 0) {
            EXPECT_EQ(&test_port, events[0].context);
            
            // Accept all pending connections
            while (num_accepted < NUM_CLIENTS) {
                socket_fd_t accepted = accept(listen_fd, nullptr, nullptr);
                if (accepted == INVALID_SOCKET_FD) {
                    break;  // No more pending
                }
                accepted_fds[num_accepted++] = accepted;
            }
        }
    }
    
    EXPECT_EQ(NUM_CLIENTS, num_accepted) << "Should accept all clients";
    
    // Cleanup
    for (int i = 0; i < num_accepted; i++) {
        SOCKET_CLOSE(accepted_fds[i]);
    }
    for (int i = 0; i < NUM_CLIENTS; i++) {
        SOCKET_CLOSE(client_fds[i]);
    }
    EXPECT_EQ(0, io_reactor_remove(reactor, listen_fd));
    SOCKET_CLOSE(listen_fd);
    io_reactor_destroy(reactor);
}

TEST(IOReactorListenTest, ContextPointerRangeCheck) {
    // Test the pattern used in process_io() to identify listening port events
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    mock_port_def_t external_port[5];
    socket_fd_t client_fds[5];
    
    // Setup 5 listening ports
    for (int i = 0; i < 5; i++) {
        int actual_port = 0;
        external_port[i].fd = create_listening_socket(0, &actual_port);
        ASSERT_NE(external_port[i].fd, INVALID_SOCKET_FD);
        external_port[i].port = actual_port;
        external_port[i].kind = 1;
        
        ASSERT_EQ(0, io_reactor_add(reactor, external_port[i].fd,
                                   &external_port[i], EVENT_READ));
    }
    
    // Connect to ports 0 and 4
    client_fds[0] = connect_to_port(external_port[0].port);
    client_fds[4] = connect_to_port(external_port[4].port);
    ASSERT_NE(client_fds[0], INVALID_SOCKET_FD);
    ASSERT_NE(client_fds[4], INVALID_SOCKET_FD);
    
    // Get events
    io_event_t events[10];
    struct timeval timeout = {1, 0};
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    
    ASSERT_EQ(2, n);
    
    // Verify context pointers are within external_port array range
    for (int i = 0; i < n; i++) {
        void* ctx = events[i].context;
        EXPECT_TRUE(ctx >= (void*)&external_port[0] &&
                   ctx <  (void*)&external_port[5])
            << "Context should be within external_port array";
        
        // Compute index via pointer arithmetic
        mock_port_def_t* port = (mock_port_def_t*)ctx;
        int which = port - external_port;
        EXPECT_TRUE(which >= 0 && which < 5);
        EXPECT_TRUE(which == 0 || which == 4);
    }
    
    // Cleanup
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(0, io_reactor_remove(reactor, external_port[i].fd));
        SOCKET_CLOSE(external_port[i].fd);
    }
    SOCKET_CLOSE(client_fds[0]);
    SOCKET_CLOSE(client_fds[4]);
    io_reactor_destroy(reactor);
}

TEST(IOReactorListenTest, ListenWithUserSockets) {
    // Test mixed scenario: listening socket + connected user sockets
    // (simulates real driver with external_port + interactive users)
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    // Create listening socket
    int listen_port = 0;
    socket_fd_t listen_fd = create_listening_socket(0, &listen_port);
    ASSERT_NE(listen_fd, INVALID_SOCKET_FD);
    
    mock_port_def_t test_port;
    test_port.kind = 1;
    test_port.port = listen_port;
    test_port.fd = listen_fd;
    ASSERT_EQ(0, io_reactor_add(reactor, listen_fd, &test_port, EVENT_READ));
    
    // Create some connected socket pairs (simulating existing users)
    socket_fd_t user_server_fds[3];
    socket_fd_t user_client_fds[3];
    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(0, create_socket_pair(&user_server_fds[i], &user_client_fds[i]));
        ASSERT_EQ(0, io_reactor_add(reactor, user_server_fds[i],
                                   (void*)(intptr_t)(100 + i), EVENT_READ));
    }
    
    // Trigger activity: new connection on listen socket + data on user 1
    socket_fd_t new_client = connect_to_port(listen_port);
    ASSERT_NE(new_client, INVALID_SOCKET_FD);
    ASSERT_EQ(1, SOCKET_SEND(user_client_fds[1], "x", 1, 0));
    
    // Should get 2 events: listen socket + user socket
    io_event_t events[10];
    struct timeval timeout = {1, 0};
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    
    ASSERT_EQ(2, n);
    
    // Verify we got both event types
    bool got_listen_event = false;
    bool got_user_event = false;
    
    for (int i = 0; i < n; i++) {
        if (events[i].context == &test_port) {
            got_listen_event = true;
            EXPECT_TRUE(events[i].event_type & EVENT_READ);
        } else if (events[i].context == (void*)(intptr_t)101) {
            got_user_event = true;
            EXPECT_TRUE(events[i].event_type & EVENT_READ);
        }
    }
    
    EXPECT_TRUE(got_listen_event) << "Should have listen socket event";
    EXPECT_TRUE(got_user_event) << "Should have user socket event";
    
    // Cleanup
    socket_fd_t accepted = accept(listen_fd, nullptr, nullptr);
    if (accepted != INVALID_SOCKET_FD) {
        SOCKET_CLOSE(accepted);
    }
    SOCKET_CLOSE(new_client);
    
    char buffer[10];
    SOCKET_RECV(user_server_fds[1], buffer, sizeof(buffer), 0);
    
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(0, io_reactor_remove(reactor, user_server_fds[i]));
        close_socket_pair(user_server_fds[i], user_client_fds[i]);
    }
    
    EXPECT_EQ(0, io_reactor_remove(reactor, listen_fd));
    SOCKET_CLOSE(listen_fd);
    io_reactor_destroy(reactor);
}

TEST(IOReactorListenTest, NoEventsWhenNoConnections) {
    io_reactor_t* reactor = io_reactor_create();
    ASSERT_NE(reactor, nullptr);
    
    // Create listening socket but don't connect
    int actual_port = 0;
    socket_fd_t listen_fd = create_listening_socket(0, &actual_port);
    ASSERT_NE(listen_fd, INVALID_SOCKET_FD);
    
    mock_port_def_t test_port;
    test_port.kind = 1;
    test_port.port = actual_port;
    test_port.fd = listen_fd;
    ASSERT_EQ(0, io_reactor_add(reactor, listen_fd, &test_port, EVENT_READ));
    
    // Wait - should timeout with no events
    io_event_t events[10];
    struct timeval timeout = {0, 500000};  // 500ms
    int n = io_reactor_wait(reactor, events, 10, &timeout);
    
    EXPECT_EQ(0, n) << "Should have no events when no connections pending";
    
    // Cleanup
    EXPECT_EQ(0, io_reactor_remove(reactor, listen_fd));
    SOCKET_CLOSE(listen_fd);
    io_reactor_destroy(reactor);
}
