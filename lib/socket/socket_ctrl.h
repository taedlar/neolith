#pragma once

/*
 * socket_ctrl.c
 */
int set_socket_nonblocking(socket_fd_t, int);
int set_socket_owner(socket_fd_t, int);
int set_socket_async(socket_fd_t, int);
