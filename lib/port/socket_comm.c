/**
 * @file socket_comm.c
 * @brief Portable socket communication utilities.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "socket_comm.h"
#include <string.h>

#ifdef _WIN32
/* Windows specific headers already in socket_comm.h */
#else
#include <unistd.h>
#include <fcntl.h>

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#endif

int create_test_socket_pair(socket_fd_t fds[2]) {
#ifdef _WIN32
    /* Windows doesn't have socketpair(), so create a TCP loopback connection */
    socket_fd_t listener = INVALID_SOCKET_FD;
    socket_fd_t connector = INVALID_SOCKET_FD;
    socket_fd_t acceptor = INVALID_SOCKET_FD;
    struct sockaddr_in addr;
    int addr_len = sizeof(addr);
    int reuse = 1;
    
    /* Create listener socket */
    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET_FD) {
        goto error;
    }
    
    /* Allow reuse of address */
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
                   (const char*)&reuse, sizeof(reuse)) == SOCKET_ERROR) {
        goto error;
    }
    
    /* Bind to loopback on ephemeral port */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;  /* Let OS choose port */
    
    if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        goto error;
    }
    
    /* Get the actual port assigned */
    if (getsockname(listener, (struct sockaddr*)&addr, &addr_len) == SOCKET_ERROR) {
        goto error;
    }
    
    /* Start listening */
    if (listen(listener, 1) == SOCKET_ERROR) {
        goto error;
    }
    
    /* Create connector socket */
    connector = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connector == INVALID_SOCKET_FD) {
        goto error;
    }
    
    /* Set connector to non-blocking for connect */
    if (set_socket_nonblocking(connector, 1) != 0) {
        goto error;
    }
    
    /* Initiate connection (will complete immediately on loopback) */
    connect(connector, (struct sockaddr*)&addr, sizeof(addr));
    /* Ignore WSAEWOULDBLOCK - expected for non-blocking connect on loopback */
    
    /* Accept the connection */
    acceptor = accept(listener, NULL, NULL);
    if (acceptor == INVALID_SOCKET_FD) {
        goto error;
    }
    
    /* Set both to non-blocking */
    if (set_socket_nonblocking(acceptor, 1) != 0 ||
        set_socket_nonblocking(connector, 1) != 0) {
        goto error;
    }
    
    /* Close listener - no longer needed */
    closesocket(listener);
    
    /* Return the connected pair */
    fds[0] = acceptor;
    fds[1] = connector;
    return 0;
    
error:
    if (listener != INVALID_SOCKET_FD) closesocket(listener);
    if (connector != INVALID_SOCKET_FD) closesocket(connector);
    if (acceptor != INVALID_SOCKET_FD) closesocket(acceptor);
    return -1;
    
#else
    /* POSIX: use socketpair() */
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    if (ret != 0) {
        return -1;
    }
    
    /* Set both to non-blocking */
    if (set_socket_nonblocking(fds[0], 1) != 0 ||
        set_socket_nonblocking(fds[1], 1) != 0) {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    
    return 0;
#endif
}

/**
 * @brief Set process receiving SIGIO/SIGURG signals to us.
 */
int set_socket_owner(socket_fd_t fd, int which) {
#ifdef OLD_ULTRIX
    return fcntl(fd, F_SETOWN, which);
#elif defined(WINSOCK)
    (void)fd;
    (void)which;
    return 1; /* No equivalent */
#else
    return ioctl(fd, SIOCSPGRP, &which);
#endif
}

/**
 * @brief Allow receipt of asynchronous I/O signals.
 */
int set_socket_async(socket_fd_t fd, int which) {
#ifdef OLD_ULTRIX
    return fcntl(fd, F_SETFL, FASYNC);
#elif defined(WINSOCK)
    (void)fd;
    (void)which;
    return 1; /* No equivalent */
#else
    return ioctl(fd, FIOASYNC, &which);
#endif
}

/**
 * @brief Set socket non-blocking
 */
int set_socket_nonblocking(socket_fd_t fd, int which) {
#ifdef OLD_ULTRIX
    if (which)
        return fcntl(fd, F_SETFL, FNDELAY);
    else
        return fcntl(fd, F_SETFL, FNBLOCK);
#elif defined(WINSOCK)
    u_long mode = which ? 1 : 0;
    return ioctlsocket(fd, FIONBIO, &mode);
#else
#ifdef _SEQUENT_
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1)
        return (-1);
    if (which)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags);
#else
    return ioctl(fd, FIONBIO, &which);
#endif
#endif
}
