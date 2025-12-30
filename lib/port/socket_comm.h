#pragma once

#ifdef _WIN32
/* Windows Sockets */
#include <winsock2.h>
#include <ws2tcpip.h>
#define WINSOCK
typedef SOCKET socket_fd_t;
#define INVALID_SOCKET_FD           INVALID_SOCKET
#define SOCKET_RECV(s, b, l, f)     recv(s, b, l, f)
#define SOCKET_SEND(s, b, l, f)     send(s, b, l, f)
#define SOCKET_CLOSE(s)             closesocket(s)
#define SOCKET_ERRNO                WSAGetLastError()
/* SOCKET_ERROR defined in winsock2.h */
#else
/* POSIX Sockets */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
typedef int socket_fd_t;
#define INVALID_SOCKET_FD           -1
#define SOCKET_RECV(s, b, l, f)     recv(s, b, l, f)
#define SOCKET_SEND(s, b, l, f)     send(s, b, l, f)
#define SOCKET_CLOSE(s)             close(s)
#define SOCKET_ERRNO                errno
#define SOCKET_ERROR               -1
#endif

#ifdef HAVE_POLL
#include <poll.h>
#endif

/**
 * @brief Create a connected socket pair for testing.
 * 
 * On POSIX systems, uses socketpair() with AF_UNIX.
 * On Windows, creates a TCP loopback connection pair.
 * Both sockets are set to non-blocking mode.
 * 
 * @param fds Output: array of 2 socket descriptors.
 * @return 0 on success, -1 on failure.
 */
int create_test_socket_pair(socket_fd_t fds[2]);

/**
 * @brief Set socket non-blocking mode.
 * @param fd Socket file descriptor.
 * @param which Non-zero to enable non-blocking, zero to disable.
 * @return 0 on success, -1 on failure.
 */
int set_socket_nonblocking(socket_fd_t fd, int which);

/**
 * @brief Set process receiving SIGIO/SIGURG signals.
 * @param fd Socket file descriptor.
 * @param which Process ID.
 * @return 0 on success, -1 on failure.
 */
int set_socket_owner(socket_fd_t fd, int which);

/**
 * @brief Allow receipt of asynchronous I/O signals.
 * @param fd Socket file descriptor.
 * @param which Non-zero to enable, zero to disable.
 * @return 0 on success, -1 on failure.
 */
int set_socket_async(socket_fd_t fd, int which);
