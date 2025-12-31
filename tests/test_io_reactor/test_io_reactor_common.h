#pragma once

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <gtest/gtest.h>
extern "C" {
#include "port/io_reactor.h"
#include "port/socket_comm.h"
}
#include "std.h"

/*
 * =============================================================================
 * Common Helper Functions and Structures
 * =============================================================================
 */

/**
 * @brief Create a socket pair for testing.
 * @param server_fd Output: server side of the socket pair.
 * @param client_fd Output: client side of the socket pair.
 * @return 0 on success, -1 on failure.
 */
static inline int create_socket_pair(socket_fd_t *server_fd, socket_fd_t *client_fd) {
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
static inline void close_socket_pair(socket_fd_t server_fd, socket_fd_t client_fd) {
    if (server_fd != INVALID_SOCKET_FD) SOCKET_CLOSE(server_fd);
    if (client_fd != INVALID_SOCKET_FD) SOCKET_CLOSE(client_fd);
}

/**
 * @brief Create a listening socket on a specified port.
 * @param port Port number to listen on (0 for auto-assigned).
 * @param actual_port Output: the actual port being listened on.
 * @return Socket fd on success, INVALID_SOCKET_FD on failure.
 */
static inline socket_fd_t create_listening_socket(int port, int *actual_port) {
    socket_fd_t listen_fd;
    struct sockaddr_in addr;
    socklen_t addr_len;
    int optval = 1;
    
    // Create socket
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == INVALID_SOCKET_FD) {
        return INVALID_SOCKET_FD;
    }
    
    // Set SO_REUSEADDR
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
                   (char*)&optval, sizeof(optval)) == SOCKET_ERROR) {
        SOCKET_CLOSE(listen_fd);
        return INVALID_SOCKET_FD;
    }
    
    // Bind to localhost
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((unsigned short)port);
    
    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        SOCKET_CLOSE(listen_fd);
        return INVALID_SOCKET_FD;
    }
    
    // Get actual bound port
    addr_len = sizeof(addr);
    if (getsockname(listen_fd, (struct sockaddr*)&addr, &addr_len) == SOCKET_ERROR) {
        SOCKET_CLOSE(listen_fd);
        return INVALID_SOCKET_FD;
    }
    if (actual_port) {
        *actual_port = ntohs(addr.sin_port);
    }
    
    // Set non-blocking
    if (set_socket_nonblocking(listen_fd, 1) == SOCKET_ERROR) {
        SOCKET_CLOSE(listen_fd);
        return INVALID_SOCKET_FD;
    }
    
    // Listen
    if (listen(listen_fd, SOMAXCONN) == SOCKET_ERROR) {
        SOCKET_CLOSE(listen_fd);
        return INVALID_SOCKET_FD;
    }
    
    return listen_fd;
}

/**
 * @brief Connect to a listening port on localhost.
 * @param port Port number to connect to.
 * @return Connected socket fd on success, INVALID_SOCKET_FD on failure.
 */
static inline socket_fd_t connect_to_port(int port) {
    socket_fd_t client_fd;
    struct sockaddr_in addr;
    
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd == INVALID_SOCKET_FD) {
        return INVALID_SOCKET_FD;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((unsigned short)port);
    
    if (connect(client_fd, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        SOCKET_CLOSE(client_fd);
        return INVALID_SOCKET_FD;
    }
    
    return client_fd;
}

/**
 * @brief Mock port_def_t structure for testing.
 * Mimics the structure used in lib/rc/rc.h for external_port.
 */
typedef struct {
    int kind;           // PORT_TELNET, PORT_BINARY, PORT_ASCII
    int port;           // Port number
    socket_fd_t fd;     // Listening socket file descriptor
} mock_port_def_t;

#ifndef WINSOCK
/**
 * @brief Create a pipe pair for console simulation.
 * @param read_fd Output: read end of pipe.
 * @param write_fd Output: write end of pipe.
 * @return 0 on success, -1 on failure.
 */
static inline int create_pipe_pair(int *read_fd, int *write_fd) {
    int fds[2];
    if (pipe(fds) != 0) {
        return -1;
    }
    *read_fd = fds[0];
    *write_fd = fds[1];
    
    // Set read end non-blocking
    int flags = fcntl(*read_fd, F_GETFL, 0);
    if (flags == -1 || fcntl(*read_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    
    return 0;
}
#endif
