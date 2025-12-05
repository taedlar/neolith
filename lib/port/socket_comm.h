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
