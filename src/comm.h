#pragma once

#ifdef	HAVE_ARPA_TELNET_H
#include <arpa/telnet.h>
#else
/* Minimal telnet definitions for systems that lack <arpa/telnet.h> */
#include "port/telnet.h"
#endif	/* HAVE_ARPA_TELNET_H */

#include "port/socket_comm.h"
#include "lpc/functional.h"

#define MAX_TEXT                   2048
#define MAX_SOCKET_PACKET_SIZE     1024
#define DESIRED_SOCKET_PACKET_SIZE 800
#define MESSAGE_BUF_SIZE           MESSAGE_BUFFER_SIZE	/* from options.h */
#define OUT_BUF_SIZE               2048
#define DFAULT_PROTO               0	/* use the appropriate protocol */
#define I_NOECHO                   0x1	/* input_to flag */
#define I_NOESC                    0x2	/* input_to flag */
#define I_SINGLE_CHAR              0x4  /* get_char */
#define I_WAS_SINGLE_CHAR          0x8  /* was get_char */
#define SB_SIZE			   100	/* More than enough */

enum msgtypes {
    NAMEBYIP = 0, IPBYNAME, DATALEN
};

/* The I_* flags are input_to flags */
#define NOECHO			I_NOECHO		/* don't echo lines */
#define NOESC			I_NOESC			/* don't allow shell out */
#define SINGLE_CHAR		I_SINGLE_CHAR		/* get_char */
#define WAS_SINGLE_CHAR		I_WAS_SINGLE_CHAR
#define HAS_PROCESS_INPUT	0x0010	/* interactive object has process_input()  */
#define HAS_WRITE_PROMPT	0x0020	/* interactive object has write_prompt()   */
#define CLOSING			0x0040	/* true when closing this file descriptor  */
#define CMD_IN_BUF		0x0080	/* there is a full command in input buffer */
#define NET_DEAD		0x0100
#define NOTIFY_FAIL_FUNC	0x0200	/* default_err_mesg is a function pointer  */
#define USING_TELNET		0x0400
#define	USING_LINEMODE		0x0800

typedef struct interactive_s {
    object_t *ob;               /* points to the associated object         */
    sentence_t *input_to;       /* to be called with next input line       */
    int connection_type;        /* the type of connection this is          */
    socket_fd_t fd;             /* file descriptor for interactive object  */
    struct sockaddr_in addr;    /* socket address of interactive object    */
#ifdef F_QUERY_IP_PORT
    int local_port;             /* which of our ports they connected to    */
#endif
    char *prompt;               /* prompt string for interactive object    */
    char text[MAX_TEXT];        /* input buffer for interactive object     */
    int text_end;               /* first free char in buffer               */
    int text_start;             /* where we are up to in user command buffer */
    struct interactive_s *snoop_on;
    struct interactive_s *snoop_by;
    int last_time;               /* time of last command executed           */
    string_or_func_t default_err_message;
#ifdef OLD_ED
    struct ed_buffer_s *ed_buffer;  /* local ed                        */
#endif
    int message_producer;       /* message buffer producer index */
    int message_consumer;       /* message buffer consumer index */
    int message_length;         /* message buffer length */
    char message_buf[MESSAGE_BUF_SIZE]; /* message buffer */
    int iflags;                 /* interactive flags */
    svalue_t *carryover;        /* points to args for input_to             */
    int num_carry;              /* number of args for input_to             */
    int out_of_band;            /* Send a telnet sync operation            */
    int state;                  /* Current telnet state.  Bingly wop       */
    int sb_pos;                 /* Telnet suboption negotiation stuff      */
    char sb_buf[SB_SIZE];
} interactive_t;


/*
 * comm.c
 */
extern int total_users;
extern int inet_packets;
extern int inet_volume;
extern int num_user;
extern int num_hidden;
extern int add_message_calls;

extern interactive_t **all_users;
extern int max_users;

void new_interactive(socket_fd_t socket_fd);
int do_comm_polling(struct timeval* timeout);

void add_vmessage(object_t *, char *, ...);
void add_message(object_t *, char *);

void make_selectmasks(void);
void init_user_conn(void);
void ipc_remove(void);
void set_prompt(char *);
void notify_no_command(void);
void set_notify_fail_message(char *);
void set_notify_fail_function(funptr_t *);
void process_io(void);
int process_user_command(void);
int replace_interactive(object_t *, object_t *);
int set_call(object_t *, sentence_t *, int);
void remove_interactive(object_t *, int);
int flush_message(interactive_t *);
int query_addr_number(char *, char *);
char *query_ip_name(object_t *);
char *query_ip_number(object_t *);
char *query_host_name(void);
int query_ip_port (object_t *);
int query_idle(object_t *);
int new_set_snoop(object_t *, object_t *);
object_t *query_snoop(object_t *);
object_t *query_snooping(object_t *);
