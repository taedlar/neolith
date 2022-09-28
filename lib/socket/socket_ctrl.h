#ifndef SOCKET_CTRL_H
#define SOCKET_CTRL_H

/*
 * socket_ctrl.c
 */
int set_socket_nonblocking(int, int);
int set_socket_owner(int, int);
int set_socket_async(int, int);

#endif
