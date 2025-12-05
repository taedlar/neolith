#pragma once

#ifdef _WIN32
/* Windows Sockets */
#include <winsock2.h>
#include <ws2tcpip.h>
#define WINSOCK
typedef SOCKET socket_fd_t;
#define INVALID_SOCKET_FD           INVALID_SOCKET
#define SOCKET_READ(fd, buff, len)  read(fd, buff, len)
#define SOCKET_WRITE(fd, buff, len) write(fd, buff, len)
#define SOCKET_CLOSE(fd)            closesocket(fd)
#define SOCKET_ERRNO                WSAGetLastError()
/* SOCKET_ERROR defined in winsock2.h */
#else
/* POSIX Sockets */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
typedef int socket_fd_t;
#define INVALID_SOCKET_FD           -1
#define SOCKET_READ(fd, buff, len)  read(fd, buff, len)
#define SOCKET_WRITE(fd, buff, len) write(fd, buff, len)
#define SOCKET_CLOSE(fd)            close(fd)
#define SOCKET_ERRNO                errno
#define SOCKET_ERROR               -1
#endif
