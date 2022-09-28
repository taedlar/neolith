/*  $Id: socket_efuns.h,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith

    ORIGINAL AUTHOR
	[1992-05-??] by Dwayne Fontenot (Jacques@TMI), original coding.
	[1992-10-??] by Dave Richards (Cynosure), less original coding.

    MODIFIED BY
	[2001-06-27] by Annihilator <annihilator@muds.net>, see CVS log.
 */

#ifndef _SOCKET_EFUNS_H
#define _SOCKET_EFUNS_H

#include "lpc/types.h"
#include "lpc/function.h"

enum socket_mode {
    MUD, STREAM, DATAGRAM, STREAM_BINARY, DATAGRAM_BINARY
};
enum socket_state {
    CLOSED, FLUSHING, UNBOUND, BOUND, LISTEN, DATA_XFER
};

#define	BUF_SIZE	2048	/* max reliable packet size	   */
#define ADDR_BUF_SIZE	64	/* max length of address string    */

typedef struct {
    int fd;
    short flags;
    enum socket_mode mode;
    enum socket_state state;
    struct sockaddr_in l_addr;
    struct sockaddr_in r_addr;
    char name[ADDR_BUF_SIZE];
    object_t *owner_ob;
    object_t *release_ob;
    string_or_func_t	read_callback;
    string_or_func_t	write_callback;
    string_or_func_t	close_callback;
    char *r_buf;
    int r_off;
    long r_len;
    char *w_buf;
    int w_off;
    int w_len;
} lpc_socket_t;

extern lpc_socket_t *lpc_socks;
extern int max_lpc_socks;

#define	S_RELEASE	0x01
#define	S_BLOCKED	0x02
#define	S_HEADER	0x04
#define	S_WACCEPT	0x08
#define S_BINARY        0x10
#define S_READ_FP       0x20
#define S_WRITE_FP      0x40
#define S_CLOSE_FP      0x80
#define S_EXTERNAL	0x100

int check_valid_socket(char *, int, object_t *, char *, int);
void socket_read_select_handler(int);
void socket_write_select_handler(int);
void assign_socket_owner(svalue_t *, object_t *);
object_t *get_socket_owner(int);
void dump_socket_status(outbuffer_t *);
void close_referencing_sockets(object_t *);
int get_socket_address(int, char *, int *);
int socket_bind(int, int);
int socket_create(enum socket_mode, svalue_t *, svalue_t *);
int socket_listen(int, svalue_t *);
int socket_accept(int, svalue_t *, svalue_t *);
int socket_connect(int, char *, svalue_t *, svalue_t *);
int socket_write(int, svalue_t *, char *);
int socket_close(int, int);
int socket_release(int, object_t *, svalue_t *);
int socket_acquire(int, svalue_t *, svalue_t *, svalue_t *);
char *socket_error(int);

#endif	/* ! _SOCKET_EFUNS_H */
