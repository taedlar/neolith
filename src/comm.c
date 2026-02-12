#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"
#include "lpc/object.h"
#include "lpc/array.h"
#include "lpc/buffer.h"
#include "comm.h"
#include "rc.h"
#include "simul_efun.h"
#include "interpret.h"
#include "socket/socket_efuns.h"
#include "port/socket_comm.h"
#include "efuns/ed.h"

#include "lpc/include/origin.h"

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#ifdef WINSOCK
#pragma comment(lib, "ws2_32.lib")
#endif

#ifdef HAVE_TERMIOS_H
/**
 * @brief Apply terminal settings without data loss for pipes
 * 
 * Real TTY: Use TCSAFLUSH to discard stale input (security)
 * Pipe:     Use TCSANOW to preserve all data (testbot)
 */
static inline void safe_tcsetattr(int fd, struct termios *tio) {
    int action = isatty(fd) ? TCSAFLUSH : TCSANOW;
    tcsetattr(fd, action, tio);
}
#endif

 /*
  * This macro is for testing whether ip is still valid, since many
  * functions call LPC code, which could otherwise use
  * enable_commands(), set_this_player(), or destruct() to cause
  * all hell to break loose by changing or dangling command_giver
  * or command_giver->interactive.  It also saves us a few dereferences
  * since we know we can trust ip, and also increases code readability.
  *
  * Basically, this should be used as follows:
  *
  * (1) when using command_giver:
  *     set a variable named ip to command_giver->interactive at a point
  *     when you know it is valid.  Then, after a call that might have
  *     called LPC code, check IP_VALID(command_giver), or use
  *     VALIDATE_IP.
  * (2) some other object:
  *     set a variable named ip to ob->interactive, and save ob somewhere;
  *     or if you are just dealing with an ip as input, save ip->ob somewhere.
  *     After calling LPC code, check IP_VALID(ob), or use VALIDATE_IP.
  * 
  * Yes, I know VALIDATE_IP uses a goto.  It's due to C's lack of proper
  * exception handling.  Only use it in subroutines that are set up
  * for it (i.e. define a failure label, and are set up to deal with
  * branching to it from arbitrary points).
  */
#define IP_VALID(ip, ob) (ob && ob->interactive == ip)
#define VALIDATE_IP(ip, ob) if (!IP_VALID(ip, ob)) goto failure

#define UCHAR	unsigned char
#ifdef __GNUC__
/* casting integer to char: standard conforming */
#define INT_CHAR(x)	((char)x)
#else
/* casting integer to char: MSVC requires straight casting */
#define INT_CHAR(x)	(x)
#endif

int total_users = 0;

/*
 * local function prototypes.
 */
#ifndef WINSOCK
static void sigpipe_handler (int);
#endif
static void hname_handler (void);
static void get_user_data (interactive_t *, io_event_t *);
static char *get_user_command (void);
static char *first_cmd_in_buf (interactive_t *);
static int cmd_in_buf (interactive_t *);
static void next_cmd_in_buf (interactive_t *);
static void print_prompt (interactive_t *);
static void telnet_neg (char *, char *);
static void query_addr_name (object_t *);
static void got_addr_number (char *, char *);
static void add_ip_entry (unsigned long, const char *);
static void reset_ip_names (void);
static void clear_notify (interactive_t *);
static void setup_accepted_connection (port_def_t *, socket_fd_t, struct sockaddr_in *);
static void new_user_handler (port_def_t *);
static void receive_snoop (char *, object_t * ob);
static void set_telnet_single_char (interactive_t *, int);

/* global variables */

int num_user;
int num_hidden;			/* for the O_HIDDEN flag.  This counter must
                                 * be kept up to date at all times!  If you
                                 * modify the O_HIDDEN flag in an object,
                                 * make sure that you update this counter if
                                 * the object is interactive. */
int add_message_calls = 0;
int inet_packets = 0;
int inet_volume = 0;
interactive_t **all_users = 0;
int max_users = 0;

/* static declarations */

static io_event_t g_io_events[512];  /* Event buffer for async_runtime_wait() */
static int g_num_io_events = 0;

static socket_fd_t addr_server_fd = INVALID_SOCKET_FD;

/* implementations */

/* Context identification helpers for event dispatch */
static inline int is_listening_port (void *context) {
  return (context >= (void*)&external_port[0] &&
          context <  (void*)&external_port[5]);
}

static inline int is_interactive_user (void *context) {
  if (!all_users || !context)
    return 0;
  
  /* Check if pointer is in all_users array range */
  for (int i = 0; i < max_users; i++) {
    if (all_users[i] == context)
      return 1;
  }
  return 0;
}

int is_console_user (void *context) {
  return context && all_users && ((object_t*)context)->interactive == all_users[0];
}

#ifdef PACKAGE_SOCKETS
static inline int is_lpc_socket (void *context) {
  return (context >= (void*)&lpc_socks[0] &&
          context <  (void*)&lpc_socks[max_lpc_socks]);
}
#endif

static void receive_snoop (char *buf, object_t * snooper) {

  /* command giver no longer set to snooper */
  copy_and_push_string (buf);
  apply (APPLY_RECEIVE_SNOOP, snooper, 1, ORIGIN_DRIVER);
}

/**
 *  @brief Initialize new user connection socket.
 */
void init_user_conn () {

  struct sockaddr_in sin;
  socklen_t sin_len;
  int optval;
  int i;

  for (i = 0; i < 5; i++)
    {
      if (!external_port[i].port)
        continue;

      /* create socket of proper type. */
      if ((external_port[i].fd = socket (AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET_FD)
        {
          debug_error ("socket() failed: %d", SOCKET_ERRNO);
          debug_fatal ("Failed to create socket for port %d\n", external_port[i].port);
          exit (EXIT_FAILURE);
        }

      /* enable local address reuse. */
      optval = 1;
      if (setsockopt (external_port[i].fd, SOL_SOCKET, SO_REUSEADDR, (char *) &optval, sizeof (optval)) == SOCKET_ERROR)
        {
          debug_error ("setsockopt() failed: %d", SOCKET_ERRNO);
          debug_fatal ("Failed to set SO_REUSEADDR on port %d\n", external_port[i].port);
          exit (2);
        }
      /* fill in socket address information. */
      sin.sin_family = AF_INET;
      sin.sin_addr.s_addr = INADDR_ANY;
      sin.sin_port = htons ((u_short) external_port[i].port);

      /* bind name to socket. */
      if (bind (external_port[i].fd, (struct sockaddr *) &sin, sizeof (sin)) == SOCKET_ERROR)
        {
          debug_error ("bind() failed: %d", SOCKET_ERRNO);
          debug_fatal ("Failed to bind to port %d\n", external_port[i].port);
          exit (3);
        }

      /* get socket name. */
      sin_len = sizeof (sin);
      if (getsockname (external_port[i].fd, (struct sockaddr *) &sin, &sin_len) == SOCKET_ERROR)
        {
          debug_error ("getsockname() failed: %d", SOCKET_ERRNO);
          debug_fatal ("Failed to get socket name for port %d\n", external_port[i].port);
          exit (4);
        }
      /* set socket non-blocking, */
      if (set_socket_nonblocking (external_port[i].fd, 1) == SOCKET_ERROR)
        {
          debug_error ("set_socket_nonblocking() failed: %d", SOCKET_ERRNO);
          debug_fatal ("Failed to set socket non-blocking on port %d\n", external_port[i].port);
          exit (8);
        }
      /* listen on socket for connections. */
      if (listen (external_port[i].fd, SOMAXCONN) == SOCKET_ERROR)
        {
          debug_error ("listen() failed: %d", SOCKET_ERRNO);
          debug_fatal ("Failed to listen on port %d\n", external_port[i].port);
          exit (10);
        }
    }
  opt_trace (TT_BACKEND|1, "finished initializing user connection sockets.\n");

  /* Create async runtime */
  g_runtime = async_runtime_init();
  if (!g_runtime)
    {
      debug_fatal ("Failed to create async runtime\n");
      exit (11);
    }
  
  /* Register listening sockets to async runtime. When new connections arrive, the listening sockets
   * will generate read events.
   */
  for (i = 0; i < 5; i++)
    {
      if (!external_port[i].port)
        continue;
      
      if (async_runtime_add (g_runtime, external_port[i].fd,
                            EVENT_READ, &external_port[i]) != 0)
        {
          debug_fatal ("Failed to register listening socket for port %d with async runtime\n",
                       external_port[i].port);
          exit (12);
        }
      opt_trace (TT_BACKEND|1, "registered listening socket for port %d with async runtime\n",
                 external_port[i].port);
    }

  /* Register console if enabled */
  if (MAIN_OPTION(console_mode))
    {
      /* Create console line queue (capacity: 256 lines, max line size: CONSOLE_MAX_LINE) */
      g_console_queue = async_queue_create(256, CONSOLE_MAX_LINE, ASYNC_QUEUE_DROP_OLDEST);
      if (!g_console_queue)
        {
          debug_message("Warning: Failed to create console queue\n");
        }
      else
        {
          /* Initialize console worker */
          g_console_worker = console_worker_init(g_runtime, g_console_queue, CONSOLE_COMPLETION_KEY);
          if (!g_console_worker)
            {
              debug_message("Warning: Failed to initialize console worker\n");
              async_queue_destroy(g_console_queue);
              g_console_queue = NULL;
            }
          else
            {
              opt_trace(TT_BACKEND|1, "Console worker initialized (type: %s)\n",
                       console_type_str(g_console_worker->console_type));
            }
        }
    }

#ifndef _WIN32
  /* register signal handler for SIGPIPE. */
  if (signal (SIGPIPE, sigpipe_handler) == SIG_ERR)
    {
      debug_perror ("signal()", 0);
      debug_fatal ("Failed to set signal handler for SIGPIPE\n");
      exit (5);
    }
#endif
  
  add_ip_entry (INADDR_LOOPBACK, "localhost");
}

/**
 *  @brief Shut down new user accept file descriptor.
 *  Also stop the console worker and destroy the async runtime.
 */
void ipc_remove () {

  int i;

  /* close all external ports */
  for (i = 0; i < 5; i++)
    {
      if (!external_port[i].port)
        continue;
      debug_message ("{}\tclosing service on TCP port %d\n", external_port[i].port);
      if (SOCKET_CLOSE (external_port[i].fd) == SOCKET_ERROR)
        debug_error ("SOCKET_CLOSE() failed: %d", SOCKET_ERRNO);
    }

  reset_ip_names ();
}

int do_comm_polling (struct timeval *timeout) {
  opt_trace (TT_COMM|3, "calling async_runtime_wait(): timeout %ld sec, %ld usec",
             timeout->tv_sec, timeout->tv_usec);
  
  /* Use async runtime for event demultiplexing */
  g_num_io_events = async_runtime_wait (g_runtime, g_io_events,
                                        sizeof(g_io_events) / sizeof(g_io_events[0]),
                                        timeout);
  
  opt_trace (TT_COMM|3, "finished waiting: got %d events", g_num_io_events);
  
  return g_num_io_events;
}

/*
 * Send a message to an interactive object.
 */
void add_message (object_t * who, char *data) {

  interactive_t *ip;
  char *cp;

  /* check destination of message */
  if (!who || (who->flags & O_DESTRUCTED) || !who->interactive ||
      (who->interactive->iflags & (NET_DEAD | CLOSING)))
    {
      if (who == master_ob || who == simul_efun_ob)
        debug_message ("%s", data);
      return;
    }

  ip = who->interactive;

  /* write message into ip->message_buf. */
  for (cp = data; *cp; cp++)
    {
      if (ip->message_length == MESSAGE_BUF_SIZE)
        {
          if (!flush_message (ip))
            {
              debug_message ("Broken connection during add_message.\n");
              return;
            }
          if (ip->message_length == MESSAGE_BUF_SIZE)
            break;
        }
      if (*cp == '\n')
        {
          if (ip->message_length == (MESSAGE_BUF_SIZE - 1))
            {
              if (!flush_message (ip))
                {
                  debug_message ("Broken connection during add_message.\n");
                  return;
                }
              if (ip->message_length == (MESSAGE_BUF_SIZE - 1))
                break;
            }
          ip->message_buf[ip->message_producer] = '\r';
          ip->message_producer = (ip->message_producer + 1) % MESSAGE_BUF_SIZE;
          ip->message_length++;
        }
      ip->message_buf[ip->message_producer] = *cp;
      ip->message_producer = (ip->message_producer + 1) % MESSAGE_BUF_SIZE;
      ip->message_length++;
    }

  /* snoop handling. */
  if (ip->snoop_by)
    receive_snoop (data, ip->snoop_by->ob);

#ifdef FLUSH_OUTPUT_IMMEDIATELY
  flush_message (ip);
#else
  if (ip == all_users[0]) /* console user */
    {
      flush_message (ip);
    }
  else
    {
      /* Request write notification from async runtime */
      async_runtime_modify (g_runtime, ip->fd, EVENT_READ | EVENT_WRITE, ip);
    }
#endif

  add_message_calls++;
}				/* add_message() */


/* add_vmessage() is mainly used by the efun ed().
 */
void
add_vmessage (object_t * who, char *format, ...)
{
  int ret = -1;
  interactive_t *ip;
  char *cp, *str = NULL;
  va_list args;

  va_start (args, format);
#ifdef _GNU_SOURCE
  ret = vasprintf (&str, format, args);
#else
  ret = _vscprintf (format, args) + 1;
  if (ret > 0)
    {
      str = DXALLOC (ret, TAG_TEMPORARY, "add_vmessage: str");
      ret = vsprintf (str, format, args);
    }
#endif
  va_end (args);
  if (ret == -1)
    {
      debug_perror ("vsaprintf()", NULL);
      return;
    }

  /*
   * if who->interactive is not valid, write message on stderr.
   * (maybe)
   */
  if (!who || (who->flags & O_DESTRUCTED) || !who->interactive ||
      (who->interactive->iflags & (NET_DEAD | CLOSING)))
    {
      if (who == master_ob || who == simul_efun_ob)
        debug_message ("%s", str);
      free (str);
      return;
    }

  /* write message into ip->message_buf. */
  ip = who->interactive;
  for (cp = str; *cp; cp++)
    {
      if (ip->message_length == MESSAGE_BUF_SIZE)
        {
          if (!flush_message (ip))
            {
              debug_message ("Broken connection during add_message.\n");
              break;
            }
          if (ip->message_length == MESSAGE_BUF_SIZE)
            break;
        }
      if (*cp == '\n')
        {
          if (ip->message_length == (MESSAGE_BUF_SIZE - 1))
            {
              if (!flush_message (ip))
                {
                  debug_message ("Broken connection during add_message.\n");
                  break;
                }
              if (ip->message_length == (MESSAGE_BUF_SIZE - 1))
                break;
            }
          /* write CR LF for every newline, to make some crappy terminal happy */
          ip->message_buf[ip->message_producer] = '\r';
          ip->message_producer = (ip->message_producer + 1) % MESSAGE_BUF_SIZE;
          ip->message_length++;
        }
      ip->message_buf[ip->message_producer] = *cp;
      ip->message_producer = (ip->message_producer + 1) % MESSAGE_BUF_SIZE;
      ip->message_length++;
    }

  if ((ip->message_length != 0) && !flush_message (ip))
    debug_message ("Broken connection during add_message.\n");

  /* snoop handling. */
  if (ip->snoop_by)
    receive_snoop (str, ip->snoop_by->ob);

  free (str);

#ifdef FLUSH_OUTPUT_IMMEDIATELY
  flush_message (ip);
#endif

  add_message_calls++;
}				/* add_message() */


/*
 * Flush outgoing message buffer of current interactive object.
 */
int flush_message (interactive_t * ip) {
  int length, num_bytes;

  /* if ip is not valid, do nothing. */
  if (!ip || (ip->iflags & (CLOSING | NET_DEAD)))
    return 0;

  /*
   * write ip->message_buf[] to socket.
   */
  while (ip->message_length != 0)
    {
      if (ip->message_consumer < ip->message_producer)
        {
          length = ip->message_producer - ip->message_consumer;
        }
      else
        {
          length = MESSAGE_BUF_SIZE - ip->message_consumer;
        }
      /* Need to use send to get Out-Of-Band data
       * num_bytes = write(ip->fd,ip->message_buf + ip->message_consumer,length);
       * [NEOLITH-EXTENSION] if ip is the console user, use write to STDOUT_FILENO
       */
      num_bytes = (ip == all_users[0]) ?
        FILE_WRITE (STDOUT_FILENO, ip->message_buf + ip->message_consumer, length) :
        SOCKET_SEND (ip->fd, ip->message_buf + ip->message_consumer, length, ip->out_of_band);
      if (num_bytes == -1)
        {
          if (SOCKET_ERRNO == EWOULDBLOCK || SOCKET_ERRNO == EINTR)
            {
              /* Socket would block - request write notification from async runtime */
              if (ip != all_users[0])
                {
                  async_runtime_modify (g_runtime, ip->fd, EVENT_READ | EVENT_WRITE, ip);
                }
              return 1;
            }

          if (SOCKET_ERRNO != EPIPE)
            debug_error ("send() failed: %d", SOCKET_ERRNO);
          ip->iflags |= NET_DEAD;
          return 0;
        }
      ip->message_consumer = (ip->message_consumer + num_bytes) % MESSAGE_BUF_SIZE;
      ip->message_length -= num_bytes;
      ip->out_of_band = 0;
      inet_packets++;
      inet_volume += num_bytes;
    }
  
  /* All data sent - remove write notification if it was set */
  if (ip != all_users[0])
    {
      async_runtime_modify (g_runtime, ip->fd, EVENT_READ, ip);
    }
  
  return 1;
}				/* flush_message() */


#define TS_DATA         0
#define TS_IAC          1
#define TS_WILL         2
#define TS_WONT         3
#define TS_DO           4
#define TS_DONT         5
#define TS_SB           6
#define TS_SB_IAC       7

static char telnet_break_response[] = { 28, INT_CHAR(IAC), INT_CHAR(WILL), TELOPT_TM, 0 };
static char telnet_interrupt_response[] = { 127, INT_CHAR(IAC), INT_CHAR(WILL), TELOPT_TM, 0 };
static char telnet_abort_response[] = { INT_CHAR(IAC), INT_CHAR(DM), 0 };
static char telnet_do_tm_response[] = { INT_CHAR(IAC), INT_CHAR(WILL), TELOPT_TM, 0 };
static char telnet_do_sga[] = { INT_CHAR(IAC), INT_CHAR(DO), TELOPT_SGA, 0 };
static char telnet_will_sga[] = { INT_CHAR(IAC), INT_CHAR(WILL), TELOPT_SGA, 0 };
static char telnet_wont_sga[] = { INT_CHAR(IAC), INT_CHAR(WONT), TELOPT_SGA, 0 };
static char telnet_do_naws[] = { INT_CHAR(IAC), INT_CHAR(DO), TELOPT_NAWS, 0 };
static char telnet_do_ttype[] = { INT_CHAR(IAC), INT_CHAR(DO), TELOPT_TTYPE, 0 };
static char telnet_do_linemode[] = { INT_CHAR(IAC), INT_CHAR(DO), TELOPT_LINEMODE, 0 };
static char telnet_term_query[] = { INT_CHAR(IAC), INT_CHAR(SB), TELOPT_TTYPE, TELQUAL_SEND, INT_CHAR(IAC), INT_CHAR(SE), 0 };
static char telnet_no_echo[] = { INT_CHAR(IAC), INT_CHAR(WONT), TELOPT_ECHO, 0 };
static char telnet_yes_echo[] = { INT_CHAR(IAC), INT_CHAR(WILL), TELOPT_ECHO, 0 };
static char telnet_sb_lm_mode[] = { INT_CHAR(IAC), INT_CHAR(SB), TELOPT_LINEMODE, LM_MODE, MODE_ACK, INT_CHAR(IAC), INT_CHAR(SE), 0 };
static char telnet_sb_lm_slc[] = { INT_CHAR(IAC), INT_CHAR(SB), TELOPT_LINEMODE, LM_SLC, 0 };
static char telnet_se[] = { INT_CHAR(IAC), INT_CHAR(SE), 0 };
static char telnet_ga[] = { INT_CHAR(IAC), INT_CHAR(GA), 0 };

/**
 * @brief Copy input characters from socket read buffer to the interactive command buffer.
 * Replace newlines with '\0'. Also add an extra space and back space for every newline.
 * This trick will allow otherwise empty lines, as multiple newlines would be replaced by
 * multiple zeroes only.
 *
 * Also handles TELNET negotiations and remove them from the input stream.
 * (Original by Pinkfish@MudOS)
 *
 * @param from Source buffer.
 * @param to Destination buffer.
 * @param count Number of characters to copy.
 * @param ip Pointer to interactive structure.
 * @return Number of characters copied.
 */
static size_t copy_chars (UCHAR* from, UCHAR* to, size_t count, interactive_t* ip) {

  size_t i;
  UCHAR *start = to;

  /* a simple state-machine that processes TELNET commands */
  for (i = 0; i < count; i++)
    {
      switch (ip->state)
        {
        case TS_DATA:		/* data transmission */
          switch (from[i])
            {
            case IAC:
              ip->state = TS_IAC;
              break;
            case '\r':
              if (ip->iflags & SINGLE_CHAR)
                *to++ = from[i];
              break;
            case '\n':
              if (ip->iflags & SINGLE_CHAR)
                *to++ = from[i];
              else
                {
                  *to++ = ' ';
                  *to++ = '\b';
                  *to++ = '\0';
                }
              break;
            default:
              *to++ = from[i];
              break;
            }
          break;

        case TS_SB_IAC:
          if (from[i] == IAC)
            {
              if (ip->sb_pos >= SB_SIZE)
                break;
              /* IAC IAC is a quoted IAC char */
              ip->sb_buf[ip->sb_pos++] = INT_CHAR(IAC);
              ip->state = TS_SB;
              break;
            }
          /* SE counts as going back into data mode */
          if (from[i] == SE)
            {
              /*
               * Ok...  need to call a function on the interactive object,
               * passing the buffer as a paramater.
               */
              ip->sb_buf[ip->sb_pos] = 0;	/* may need setup as a buffer */
              switch (ip->sb_buf[0])
                {
                case TELOPT_TTYPE:
                  {
                    if (ip->sb_buf[1] != TELQUAL_IS)
                      break;
                    copy_and_push_string ((char*)ip->sb_buf + 2);
                    apply (APPLY_TERMINAL_TYPE, ip->ob, 1, ORIGIN_DRIVER);
                    break;
                  }
                case TELOPT_NAWS:
                  {
                    int w, h;

                    w =
                      ((UCHAR) ip->sb_buf[1]) * 256 + ((UCHAR) ip->sb_buf[2]);
                    h =
                      ((UCHAR) ip->sb_buf[3]) * 256 + ((UCHAR) ip->sb_buf[4]);
                    push_number (w);
                    push_number (h);
                    apply (APPLY_WINDOW_SIZE, ip->ob, 2, ORIGIN_DRIVER);
                    break;
                  }
                case TELOPT_LINEMODE:
                  {
                    switch (ip->sb_buf[1])
                      {
                      case LM_MODE:
                        if (ip->sb_buf[2] & MODE_ACK)
                          {
                            /* LM_MODE confirmed */
                            opt_trace (TT_COMM|2, "Telnet LINEMODE mode acknowledged by client.\n");
                            break;
                          }
                        else
                          {
                            /* if no MODE_ACK bit set, client is trying to set our
                             * LM_MODE (which violates RFC-1091), we just ignore
                             * them. --- Annihilator@ES2 [2002-05-07] */
                            /* set our preferred mode */
                            add_message (ip->ob, telnet_sb_lm_mode);
                            break;
                          }
                        break;
                      case LM_SLC:
                        {
                          int j;
                          char slc[4] = { 0, 0, 0, 0 };

                          /* We does very little on SLC for now, just ack
                           * anything client tells us. --- Annihilator@ES2 [2002-05-07] */
                          add_message (ip->ob, telnet_sb_lm_slc);
                          for (j = 2; j < ip->sb_pos - 3; j += 3)
                            {
                              if (ip->sb_buf[j] == 0
                                  && ip->sb_buf[j + 2] == 0)
                                {
                                  /* client requests default/current SLCs, give them an empty set */
                                  break;	/* ignore all other stuff in the suboption */
                                }

                              /* copy the slc triplet */
                              slc[SLC_FUNC] = ip->sb_buf[j];
                              slc[SLC_FLAGS] = ip->sb_buf[j + 1];
                              slc[SLC_VALUE] = ip->sb_buf[j + 2];

                              if (slc[SLC_FLAGS] & SLC_ACK)
                                continue;	/* ignore if SLC_ACK bit is set */

                              if (slc[SLC_FUNC] > NSLC)
                                {
                                  /* we don't know this local character */
                                  slc[SLC_FLAGS] = SLC_NOSUPPORT;
                                }
                              else
                                {
                                  switch (slc[SLC_FLAGS] & SLC_LEVELBITS)
                                    {
                                    case SLC_DEFAULT:
                                      slc[SLC_VALUE] = 0;
                                      break;	/* reply XXX SLC_DEFAULT 0 */

                                    case SLC_VARIABLE:
                                    case SLC_CANTCHANGE:
                                      if ((unsigned) slc[SLC_VALUE] >= ' ' &&
                                          (unsigned) slc[SLC_VALUE] != '\x7f')
                                        {
                                          slc[SLC_FLAGS] = SLC_NOSUPPORT;
                                          break;
                                        }
                                      slc[SLC_FLAGS] |= SLC_ACK;
                                      break;

                                    case SLC_NOSUPPORT:
                                      /* ignore not supported local chars */
                                      continue;
                                    }
                                }
                              add_message (ip->ob, slc);
                            }
                          add_message (ip->ob, telnet_se);
                          break;
                        }
                      }
                    break;
                  }
                default:
                  {
                    /* FIXME: Telnet subnegotiation data may contain '\0'
                     * characters, passing as string implicitly truncated
                     * anything beyond '\0'. Maybe need change to buffer
                     * or something. --- Annihilator@ES2 [2002-05-07]
                     */
                    copy_and_push_string ((char*)ip->sb_buf);
                    apply (APPLY_TELNET_SUBOPTION, ip->ob, 1, ORIGIN_DRIVER);
                    break;
                  }
                }
              ip->state = TS_DATA;
              break;
            }
          /* unrecognized IAC ??? between IAC SB and IAC SE, discard */
          break;

        case TS_IAC:
          switch (from[i])
            {
            case IAC:		/* escape sequence for the very IAC char */
              *to++ = IAC;
              ip->state = TS_DATA;
              break;
            case DO:
              ip->state = TS_DO;
              break;
            case DONT:
              ip->state = TS_DONT;
              break;
            case WILL:
              ip->state = TS_WILL;
              break;
            case WONT:
              ip->state = TS_WONT;
              break;
            case BREAK:	/* Send back a break character. */
              add_message (ip->ob, telnet_break_response);
              flush_message (ip);
              break;
            case IP:		/* Send back an interupt process character. */
              add_message (ip->ob, telnet_interrupt_response);
              break;
            case AYT:		/* Are you there signal.  Yep we are. */
              add_vmessage (ip->ob, "\n[%s-%s] \n", PACKAGE, VERSION);
              break;
            case AO:		/* Abort output. Do a telnet sync operation. */
              ip->out_of_band = MSG_OOB;
              add_message (ip->ob, telnet_abort_response);
              flush_message (ip);
              break;
            case SB:		/* start subnegotiation */
              ip->state = TS_SB;
              ip->sb_pos = 0;
              break;
            default:		/* IAC ???, treat as IAC NOP */
              ip->state = TS_DATA;
              break;
            }
          break;

        case TS_DO:
          switch (from[i])
            {
            case TELOPT_SGA:
              add_message (ip->ob, telnet_will_sga);
              flush_message (ip);
              break;
            case TELOPT_TM:
              add_message (ip->ob, telnet_do_tm_response);
              flush_message (ip);
              break;
            }
          ip->state = TS_DATA;
          break;

        case TS_WILL:
          /* if we get any IAC WILL or IAC WONTs back, we assume they
           * understand the telnet protocol.  Typically this will become
           * set at the first IAC WILL/WONT TTYPE/NAWS response to the
           * initial queries.
           */
          if (!(ip->iflags & USING_TELNET))
            {
              opt_trace (TT_COMM|2, "Got IAC WILL from client, assuming telnet support.\n");
              ip->iflags |= USING_TELNET;
            }

          switch (from[i])
            {
            case TELOPT_TTYPE:
              add_message (ip->ob, telnet_term_query);
              flush_message (ip);
              break;
            case TELOPT_NAWS:
              break;
            case TELOPT_LINEMODE:
              /* client understands RFC-1091, now set LM_MODE */
              ip->iflags |= USING_LINEMODE;
              if (!(ip->iflags & SINGLE_CHAR))
                {
                  telnet_sb_lm_mode[4] = MODE_EDIT | MODE_TRAPSIG;
                  add_message (ip->ob, telnet_sb_lm_mode);
                  flush_message (ip);
                }
              break;
            case TELOPT_SGA:
              add_message (ip->ob, telnet_do_sga);
              flush_message (ip);
              break;
            }
          ip->state = TS_DATA;
          break;

        case TS_DONT:
          /* if we get any IAC WILL or IAC WONTs back, we assume they
           * understand the telnet protocol.  Typically this will become
           * set at the first IAC WILL/WONT TTYPE/NAWS response to the
           * initial queries.
           */
          if (!(ip->iflags & USING_TELNET))
            {
              opt_trace (TT_COMM|2, "Got IAC WONT/DONT from client, assuming telnet support.\n");
              ip->iflags |= USING_TELNET;
            }
          switch (from[i])
            {
            case TELOPT_SGA:
              add_message (ip->ob, telnet_wont_sga); /* acknowledged, won't send go ahead */
              flush_message (ip);
              break;
            }
          ip->state = TS_DATA;
          break;

        case TS_WONT:
          /* if we get any IAC WILL or IAC WONTs back, we assume they
           * understand the telnet protocol.  Typically this will become
           * set at the first IAC WILL/WONT TTYPE/NAWS response to the
           * initial queries.
           */
          if (!(ip->iflags & USING_TELNET))
            {
              opt_trace (TT_COMM|2, "Got IAC WONT/DONT from client, assuming telnet support.\n");
              ip->iflags |= USING_TELNET;
            }
          switch (from[i])
            {
            case TELOPT_SGA:
              opt_trace (TT_COMM|2, "(TELNET) client won't send go ahead.\n");
              break;
            case TELOPT_LINEMODE:
              opt_trace (TT_COMM|2, "(TELNET) client disabled LINEMODE.\n");
              ip->iflags &= ~USING_LINEMODE;
              break;
            }
          ip->state = TS_DATA;
          break;

        case TS_SB:
          if ((UCHAR) from[i] == IAC)
            {
              ip->state = TS_SB_IAC;
              break;
            }
          if (ip->sb_pos < SB_SIZE)
            ip->sb_buf[ip->sb_pos++] = from[i];
          break;
        }
    }

  return (to - start);
}


/**
 *  @brief set_telnet_single_char () - set single-char mode on/off
 */
static void set_telnet_single_char (interactive_t * ip, int single)
{
  if (ip == all_users[0]) /* console user */
    {
#ifdef HAVE_TERMIOS_H
      /* console user, try termios */
      struct termios tio;      
      tcgetattr (ip->fd, &tio);
      if (single)
        {
          /* disable canonical mode and echo: input character is immediately available for read() */
          tio.c_lflag &= ~(ICANON | ECHO);
          tio.c_cc[VMIN] = 0; /* use polling as like O_NONBLOCK was set */
          tio.c_cc[VTIME] = 0; /* no timeout */
        }
      else
        tio.c_lflag |= ICANON|ECHO; /* enable canonical mode and echo: use line editing */
      safe_tcsetattr (ip->fd, &tio); /* TTY: discard pending input, Pipe: preserve data */
#endif
      return;
    }

  /* if the client side doesnot understand telnet, do nothing */
  if (!(ip->iflags & USING_TELNET))
    return;

  /* if client is using linemode, set LM_MODE */
  if (ip->iflags & USING_LINEMODE)
    {
      if (single)
        telnet_sb_lm_mode[4] = MODE_TRAPSIG;
      else
        telnet_sb_lm_mode[4] = MODE_TRAPSIG | MODE_EDIT;
      add_message (ip->ob, telnet_sb_lm_mode);
      flush_message (ip);
      return;
    }

  if (single)
    add_message (ip->ob, telnet_will_sga);
  else
    add_message (ip->ob, telnet_wont_sga);
  flush_message (ip);
}

#ifndef WINSOCK
/*
 * SIGPIPE handler -- does very little for now.
 */
static void sigpipe_handler (int sig) {
  (void) sig;
  debug_message ("SIGPIPE received.\n");
  signal (SIGPIPE, sigpipe_handler);
}
#endif

/**
 * @brief Add console input line to interactive buffer.
 * 
 * Console input uses virtual terminal (VT) mode, not TELNET protocol.
 * Lines are already read by the console worker thread. This function
 * converts CR/LF to null terminators and appends to the text buffer.
 * 
 * @param ip Interactive object (must be console user)
 * @param line_buffer Line data from console worker (null-terminated)
 * @param line_length Length including null terminator
 */
static void add_console_line (interactive_t *ip, const char *line_buffer, size_t line_length) {
  if (!ip || (ip->iflags & (NET_DEAD | CLOSING)))
    return;

  int len = (int)(line_length > 0 ? line_length - 1 : 0); /* Exclude null terminator */
  if (len <= 0 || ip->text_end + len >= MAX_TEXT)
    return;

  /* Convert newlines to null terminators for command parsing */
  const char* from = line_buffer;
  char* to = ip->text + ip->text_end;
  int bytes = len;
  
  while (bytes-- > 0)
    {
      if (*from == '\n' || *from == '\r')
        *to++ = '\0';
      else
        *to++ = *from;
      from++;
    }
  
  ip->text_end = to - ip->text;
  ip->text[ip->text_end] = '\0';
  
  /* Set flag if new data completes command */
  if (cmd_in_buf(ip))
    {
      opt_trace(TT_COMM|1, "Console command available in buffer\n");
      ip->iflags |= CMD_IN_BUF;
    }
}

/**
 *  @brief Process I/O for sockets or console (if enabled).
 *
 *  This function is called after async_runtime_wait() returns events.
 *  Events are dispatched to appropriate handlers based on context.
 */
void process_io () {

  int i;

  /* Dispatch all events returned by reactor */
  for (i = 0; i < g_num_io_events; i++)
    {
      io_event_t *evt = &g_io_events[i];
      
      /* Identify event source by context pointer */
      if (is_listening_port (evt->context))
        {
          /* Listening socket event */
          port_def_t *port = (port_def_t*)evt->context;
          
          if (evt->event_type & EVENT_READ)
            {
#ifdef _WIN32
              /* On Windows, accept worker has already called accept() and posted
               * the accepted socket FD. The FD is in evt->fd. */
              if (evt->fd != INVALID_SOCKET)
                {
                  opt_trace (TT_COMM|1, "Accept worker accepted connection on port %d (fd=%d)\n", 
                            port->port, (int)evt->fd);
                  
                  /* Get peer address for the already-accepted socket */
                  struct sockaddr_in addr;
                  socklen_t addr_len = sizeof(addr);
                  if (getpeername(evt->fd, (struct sockaddr*)&addr, &addr_len) == 0)
                    {
                      setup_accepted_connection(port, evt->fd, &addr);
                    }
                  else
                    {
                      debug_message("getpeername() failed for accepted socket: %d\n", SOCKET_ERRNO);
                      SOCKET_CLOSE(evt->fd);
                    }
                }
              else
                {
                  /* Fallback: call accept() synchronously (shouldn't happen with accept worker) */
                  opt_trace (TT_COMM|1, "Fallback: synchronous accept on port %d\n", port->port);
                  new_user_handler (port);
                }
#else
              /* On POSIX, listening socket is ready - call accept() */
              opt_trace (TT_COMM|1, "New connection on port %d\n", port->port);
              new_user_handler (port);
#endif
            }
          
          if (evt->event_type & EVENT_ERROR)
            {
              debug_message ("Error on listening port %d\n", port->port);
            }
        }
      else if (evt->completion_key == CONSOLE_COMPLETION_KEY)
        {
          /* Console input - completion posted by console worker thread.
           * Console uses virtual terminal mode (VT), not TELNET protocol.
           * Worker thread has already read the data; we just process it. */
          if (g_console_queue)
            {
              /* all_users slot #0 is reserved for the console user */
              interactive_t *console_ip = all_users[0];
              if (!console_ip)
                {
                  /* Console user disconnected - reconnect first */
                  opt_trace(TT_COMM|1, "Console user re-connecting\n");
                  init_console_user(1);
                  console_ip = all_users[0];
                }
              
              /* Drain all pending lines from queue (always null-terminated) */
              if (console_ip)
                {
                  char line_buffer[CONSOLE_MAX_LINE];
                  size_t line_length;
                  
                  while (async_queue_dequeue(g_console_queue, line_buffer, sizeof(line_buffer), &line_length))
                    {
                      add_console_line(console_ip, line_buffer, line_length);
                    }
                }
            }
        }
      else if (is_interactive_user (evt->context))
        {
          /* Interactive user socket */
          interactive_t *ip = (interactive_t*)evt->context;
          
          /* Validate interactive is still valid */
          if (!ip->ob || (ip->ob->flags & O_DESTRUCTED) ||
              ip->ob->interactive != ip)
            {
              continue;
            }
          
          if (evt->event_type & (EVENT_ERROR | EVENT_CLOSE))
            {
              /* Network error or connection closed */
              opt_trace (TT_COMM|1, "Connection closed on fd %d\n", ip->fd);
              remove_interactive (ip->ob, 0);
              continue;
            }
          
          if (evt->event_type & EVENT_READ)
            {
              get_user_data (ip, evt);
              /* ip->ob may be invalid after get_user_data if object was destructed */
              if (!ip->ob || (ip->ob->flags & O_DESTRUCTED) || ip->ob->interactive != ip)
                {
                  continue;
                }
            }
          
          if (evt->event_type & EVENT_WRITE)
            {
              flush_message (ip);
            }
        }
#ifdef PACKAGE_SOCKETS
      else if (is_lpc_socket (evt->context))
        {
          /* LPC socket efun */
          lpc_socket_t *sock = (lpc_socket_t*)evt->context;
          ptrdiff_t sock_index = sock - lpc_socks;
          
          if (sock->state == CLOSED)
            continue;
          
          if (evt->event_type & EVENT_READ)
            {
              socket_read_select_handler ((int)sock_index);
            }
          
          if (sock->state != CLOSED && (evt->event_type & EVENT_WRITE))
            {
              socket_write_select_handler ((int)sock_index);
            }
        }
#endif
      else if (evt->context == &addr_server_fd)
        {
          /* Address server pipe */
          if (evt->event_type & EVENT_READ)
            {
              hname_handler ();
            }
        }
    }
  
  /* Flush console user output if connected (console is always writable) */
  if (all_users[0])
    flush_message (all_users[0]);
  /*
  for (i = 1; i < max_users; i++) {
    if (all_users[i])
      flush_message (all_users[i]);
  }*/
}

/**
 *  @brief Creates a new interactive structure for a given socket file descriptor.
 *  The new interactive_t structure is allocated and added to the \c all_users array (dynamically resized if needed).
 *  The master object is set as the command giver object for this new interactive.
 *  @param socket_fd The file descriptor of the socket to associate with the new interactive.
 */
void new_interactive (socket_fd_t socket_fd) {

  int i;
  if (socket_fd == INVALID_SOCKET) {
    debug_message ("Invalid socket file descriptor: %d\n", (int)socket_fd);
    return;
  }
  if (socket_fd == (socket_fd_t)STDIN_FILENO) {
    /* Console user is always at slot #0 in all_users */
    if (all_users && all_users[0]) {
      debug_message ("Console user already exists, cannot create another.\n");
      return;
    }
    i = 0; /* reserve slot #0 for console user */
  }
  else {
    /* find a free slot in all_users (slot #0 reserved for console user) */
    for (i = 1; i < max_users; i++)
      if (!all_users[i])
        break;
  }

  if (i >= max_users) {
    if (all_users) {
      /* allocate 50 more user slots */
      all_users = RESIZE (all_users, max_users + 50, interactive_t *, TAG_USERS, "new_user_handler");
    }
    else {
      /* first time allocation */
      all_users = CALLOCATE (50, interactive_t *, TAG_USERS, "new_user_handler");
    }
    while (max_users < i + 50)
      all_users[max_users++] = 0;
  }

  command_giver = master_ob;
  master_ob->interactive = (interactive_t *)DXALLOC (sizeof (interactive_t), TAG_INTERACTIVE, "new_user_handler");
  total_users++;
  master_ob->interactive->default_err_message.s = 0;
  master_ob->flags |= O_ONCE_INTERACTIVE;
  if (i == 0) /* console user */
    master_ob->flags |= O_CONSOLE_USER;
  /*
   * initialize new user interactive data structure.
   */
  master_ob->interactive->ob = master_ob;
  master_ob->interactive->input_to = 0;
  master_ob->interactive->iflags = 0;
  master_ob->interactive->text[0] = '\0';
  master_ob->interactive->text_end = 0;
  master_ob->interactive->text_start = 0;
  master_ob->interactive->snoop_on = 0;
  master_ob->interactive->snoop_by = 0;
  master_ob->interactive->last_time = current_time;
#ifdef TRACE
  master_ob->interactive->trace_level = 0;
  master_ob->interactive->trace_prefix = 0;
#endif
#ifdef OLD_ED
  master_ob->interactive->ed_buffer = 0;
#endif
  master_ob->interactive->message_producer = 0;
  master_ob->interactive->message_consumer = 0;
  master_ob->interactive->message_length = 0;
  master_ob->interactive->state = TS_DATA; /* initial telnet state when connection is established */
  master_ob->interactive->out_of_band = 0;
  all_users[i] = master_ob->interactive;
  all_users[i]->fd = socket_fd;
  set_prompt ("> ");

  /* Register interactive socket with async runtime.
   * Console user (slot 0) is handled by console worker via completion posting,
   * so stdin is NOT registered with async_runtime_add(). Only network users (slot > 0)
   * need to be registered for I/O event notification.
   * 
   * Note: async_runtime_add() automatically posts initial async read on Windows IOCP
   * when EVENT_READ is requested, so no explicit post_read() call is needed.
   */
  if (i > 0)
    {
      if (async_runtime_add (g_runtime, socket_fd, EVENT_READ, master_ob->interactive) != 0)
        {
          debug_message ("Failed to register user socket with async runtime\n");
          SOCKET_CLOSE (socket_fd);
          FREE (master_ob->interactive);
          master_ob->interactive = 0;
          all_users[i] = 0;
          return;
        }
    }

  num_user++;
}

/**
 *  @brief Setup a newly accepted connection.
 *  This helper function is called after accept() has been performed (either by new_user_handler
 *  on POSIX, or by the accept worker thread on Windows). It performs initial socket configuration
 *  and delegates to backend's mudlib_connect/mudlib_logon pattern.
 *  @param port The port definition for the listening socket
 *  @param new_socket_fd The accepted socket file descriptor
 *  @param addr The peer address structure from accept() or getpeername()
 */
static void setup_accepted_connection (port_def_t *port, socket_fd_t new_socket_fd, struct sockaddr_in *addr) {
  object_t *user_ob;
  char addr_str[50];

  inet_ntop (AF_INET, &addr->sin_addr.s_addr, addr_str, sizeof(addr_str));
  opt_trace (TT_COMM|1, "Connection from %s:%d on port %d (fd=%d)\n",
            addr_str, ntohs (addr->sin_port), port->port, (int)new_socket_fd);

  /* Set non-blocking mode on accepted socket */
  if (set_socket_nonblocking (new_socket_fd, 1) == SOCKET_ERROR)
    {
      debug_message ("Failed to set non-blocking mode on socket: %d\n", SOCKET_ERRNO);
      SOCKET_CLOSE (new_socket_fd);
      return;
    }

  /* Create interactive structure attached to master_ob */
  new_interactive (new_socket_fd);
  
  /* master_ob->interactive can be NULL if new_interactive failed */
  if (!master_ob->interactive)
    {
      SOCKET_CLOSE (new_socket_fd);
      return;
    }

  /* Store connection info in the interactive structure */
  memcpy (&master_ob->interactive->addr, addr, sizeof (struct sockaddr_in));
  master_ob->interactive->connection_type = port->kind;
#ifdef F_QUERY_IP_PORT
  master_ob->interactive->local_port = port->port;
#endif

  /* Call master->connect() to get user object and transfer interactive */
  user_ob = mudlib_connect (port->port, addr_str);
  
  if (!user_ob)
    {
      /* Connection rejected by mudlib */
      if (master_ob->interactive)
        remove_interactive (master_ob, 0);
      return;
    }

  /* Send initial TELNET negotiation if using telnet protocol */
  if (port->kind == PORT_TELNET)
    {
      query_addr_name (user_ob);
      add_message (user_ob, telnet_do_ttype);
      add_message (user_ob, telnet_do_naws);
      add_message (user_ob, telnet_do_linemode);
      flush_message (user_ob->interactive);
    }

  /* Call logon() apply to start the logon process on the user object. */
  mudlib_logon (user_ob);  
  if (user_ob->flags & O_DESTRUCTED)
    return; /* logon() destructed the user object */

  opt_info (1, "connection established for %s (fd=%d, ob=%s)\n",
            addr_str, (int)new_socket_fd, user_ob->name);

#ifdef _WIN32
  /* On Windows IOCP, the initial async read posted by async_runtime_add() may complete
   * before mudlib_connect() transfers the interactive. Post the first read here instead
   * after user object is fully set up. */
  if (user_ob->interactive && async_runtime_post_read(g_runtime, user_ob->interactive->fd, NULL, 0) != 0)
    {
      debug_message("Failed to post initial read for user socket (fd=%d)\n", user_ob->interactive->fd);
      remove_interactive(user_ob, 0);
    }
#endif
}

/**
 *  @brief This is the new user connection handler.
 *  This function is called by the event handler when data is pending on a listening port.
 *  A new connection is established by using \c accept() on the listening socket.
 *  An interactive data structure is allocated and initialized to represent the user just connected.
 *  The master object is set as the command giver object for this new interactive in \c new_interactive().
 *  The \c mudlib_connect() function is called to allow the mudlib to create a user object for the new connection.
 *  If the mudlib returns an object, the connection is successful and \c mudlib_logon() is called to start
 *  the logon process (after initial TELNET negotiation).
 *  If the mudlib returns NULL, the connection is closed and the interactive structure is removed.
 *  @param port The port definition structure representing the listening port.
 */
static void new_user_handler (port_def_t *port) {

  socket_fd_t new_socket_fd;
  struct sockaddr_in addr;
  socklen_t length;

  if (!port || !port->port)
    {
      debug_message ("new_user_handler: invalid port\n");
      return;
    }

  length = sizeof (addr);
  new_socket_fd = accept (port->fd, (struct sockaddr *) &addr, &length);
  if (INVALID_SOCKET_FD == new_socket_fd)
    {
      if (SOCKET_ERRNO != EWOULDBLOCK)
        {
          debug_error ("accept() failed: %d", SOCKET_ERRNO);
        }
      return;
    }

  /* Note: according to Amylaar, 'accepted' sockets in Linux 0.99p6 don't
   * properly inherit the nonblocking property from the listening socket. */
  setup_accepted_connection(port, new_socket_fd, &addr);
}

/**
 *  User command turn handler.
 *
 *  This function is called by the backend after unblocked from a communication polling.
 *  Network traffics from all connected users are buffered in each user's command buffer and
 *  marked with CMD_IN_BUF flag if a complete command is available.
 * 
 *  This function calls \c get_user_command() to iterate over all connected users,
 *  assigining \c command_giver to each user in turn, and checking for pending commands.
 *  If a command is pending, it is processed by \c process_command() or \c apply() to the user
 *  object as appropriate.
 *  
 *  User commands are processed in sequence (round-robin) that one user command is processed
 *  per execution of this function.
 * 
 *  @return Returns 1 if a user command was processed, 0 if no more user commands are pending.
 */
int process_user_command () {

  char *user_command;
  static char buf[MAX_TEXT], *tbuf;
  object_t *save_current_object = current_object;
  object_t *save_command_giver = command_giver;
  interactive_t *ip;
  svalue_t *ret;

  buf[MAX_TEXT - 1] = '\0';

  /* WARNING: get_user_command() sets command_giver */
  if ((user_command = get_user_command ()))
    {
#if defined(NO_ANSI) && defined(STRIP_BEFORE_PROCESS_INPUT)
      char *p;
      for (p = user_command; *p; p++)
        {
          if (*p == 27)
            {
              char *q = buf;
              for (p = user_command; *p && p - user_command < MAX_TEXT - 1; p++)
                *q++ = ((*p == 27) ? ' ' : *p);
              *q = 0;
              user_command = buf;
              break;
            }
        }
#endif

      if (command_giver->flags & O_DESTRUCTED)
        {
          command_giver = save_command_giver;
          current_object = save_current_object;
          return 1;
        }
      ip = command_giver->interactive;
      if (!ip)
        return 1;
      current_interactive = command_giver;
      current_object = 0;
      clear_notify (ip);
      update_load_av ();
      tbuf = user_command;

      /*
       * Check for special command prefixes.
       * '!' indicates a command to be processed by process_input() in the user object.
       * If ed_buffer is set, the command is for the line editor
       */
      if ((user_command[0] == '!') && (
#ifdef OLD_ED
          ip->ed_buffer ||
#endif
          (ip->input_to && !(ip->iflags & NOESC))))
        {
          if (ip->iflags & SINGLE_CHAR)
            {
              /* only 1 char ... switch to line buffer mode */
              ip->iflags |= WAS_SINGLE_CHAR;
              ip->iflags &= ~SINGLE_CHAR;
              set_telnet_single_char (ip, 0);
              /* come back later */
            }
          else
            {
              if (ip->iflags & WAS_SINGLE_CHAR)
                {
                  /* we now have a string ... switch back to char mode */
                  ip->iflags &= ~WAS_SINGLE_CHAR;
                  ip->iflags |= SINGLE_CHAR;
                  set_telnet_single_char (ip, 1);
                  VALIDATE_IP (ip, command_giver);
                }

              if (ip->iflags & HAS_PROCESS_INPUT)
                {
                  copy_and_push_string (user_command + 1);
                  ret = apply (APPLY_PROCESS_INPUT, command_giver, 1, ORIGIN_DRIVER);
                  VALIDATE_IP (ip, command_giver);
                  if (!ret)
                    ip->iflags &= ~HAS_PROCESS_INPUT;
                  if (ret && ret->type == T_STRING)
                    {
                      strncpy (buf, ret->u.string, MAX_TEXT - 1);
                      process_command (buf, command_giver);
                    }
                  else if (!ret || ret->type != T_NUMBER || !ret->u.number)
                    {
                      process_command (tbuf + 1, command_giver);
                    }
                }
              else
                process_command (tbuf + 1, command_giver);
            }
#ifdef OLD_ED
        }
      else if (ip->ed_buffer)
        {
          ed_cmd (user_command);
#endif /* OLD_ED */
        }
      else if (call_function_interactive (ip, user_command))
        {
          /* input_to or get_char handled by call_function_interactive() */
        }
      else
        {
          /*
           * send a copy of user input back to user object to provide
           * support for things like command history and mud shell
           * programming languages.
           */
          if (ip->iflags & HAS_PROCESS_INPUT)
            {
              copy_and_push_string (user_command);
              ret = apply (APPLY_PROCESS_INPUT, command_giver, 1, ORIGIN_DRIVER);
              VALIDATE_IP (ip, command_giver);
              if (!ret)
                ip->iflags &= ~HAS_PROCESS_INPUT;
              if (ret && ret->type == T_STRING)
                {
                  strncpy (buf, ret->u.string, MAX_TEXT - 1);
                  process_command (buf, command_giver);
                }
              else if (!ret || ret->type != T_NUMBER || !ret->u.number)
                {
                  process_command (tbuf, command_giver);
                }
            }
          else
            process_command (tbuf, command_giver);
        }
      VALIDATE_IP (ip, command_giver);
      /*
       * Print a prompt if user is still here.
       */
      print_prompt (ip);
    failure:
      current_object = save_current_object;
      command_giver = save_command_giver;
      current_interactive = 0;
      return (1);
    }
  /* no more commands */
  current_object = save_current_object;
  command_giver = save_command_giver;
  current_interactive = 0;
  return 0;
}				/* process_user_command() */

#define HNAME_BUF_SIZE 200
/**
 * This is the hname input data handler. This function is called by the
 * master handler when data is pending on the hname socket (addr_server_fd).
 */
static void hname_handler () {
  static char hname_buf[HNAME_BUF_SIZE];
  int num_bytes;
  int tmp;
  char *pp, *q;

  if (addr_server_fd < 0)
    return;

  num_bytes = SOCKET_RECV (addr_server_fd, hname_buf, HNAME_BUF_SIZE, 0);
  switch (num_bytes)
    {
    case -1:
      switch (errno)
        {
#ifdef EWOULDBLOCK
        case EWOULDBLOCK:
          break;
#endif
        default:
          debug_message ("hname_handler: read on fd %d\n", addr_server_fd);
          debug_perror ("hname_handler: read", 0);
          tmp = addr_server_fd;
          addr_server_fd = -1;
          SOCKET_CLOSE (tmp);
          return;
        }
      break;
    case 0:
      debug_message ("hname_handler: closing address server connection.\n");
      tmp = addr_server_fd;
      addr_server_fd = -1;
      SOCKET_CLOSE (tmp);
      return;
    default:
      hname_buf[num_bytes] = '\0';
      if (hname_buf[0] >= '0' && hname_buf[0] <= '9')
        {
          struct in_addr addr;
          if (inet_pton (AF_INET, hname_buf, &addr))
            {
              pp = strchr (hname_buf, ' ');
              if (pp)
                {
                  *pp++ = 0;
                  q = strchr (pp, '\n');
                  if (q)
                    {
                      *q = 0;
                      if (strcmp (pp, "0"))
                        add_ip_entry (addr.s_addr, pp);
                      got_addr_number (pp, hname_buf);	/* Recognises this as failure. */
                    }
                }
            }
        }
      else
        {
          char *r;

          /* This means it was a name lookup... */
          pp = strchr (hname_buf, ' ');
          if (pp)
            {
              *pp = 0;
              pp++;
              r = strchr (pp, '\n');
              if (r)
                *r = 0;
              got_addr_number (pp, hname_buf);
            }
        }
      break;
    }
}				/* hname_handler() */



/**
 * @brief This is the user data handler. This function is called from
 * the backend when a user has transmitted data to us.
 *
 * Supports both I/O notification models:
 * - Readiness notification (POSIX): evt is NULL, perform synchronous read
 * - Completion notification (Windows IOCP): evt contains data already read,
 *   post next async read operation
 *
 * @param ip The interactive data structure for the user.
 * @param evt Event structure from async_runtime_wait (NULL for POSIX).
 */
static void get_user_data (interactive_t* ip, io_event_t* evt) {

  char buf[MAX_TEXT];
  size_t text_space, num_bytes;
  int err = 0;

  /* Console users should never reach this function - they use completion queue.
   * This assertion validates the architecture invariant. */
  if (ip->connection_type == CONSOLE_USER)
    {
      debug_message("get_user_data: console user unexpectedly in network I/O path (fd %d)\n", ip->fd);
      return;
    }

  switch (ip->connection_type)
    {
    case PORT_TELNET:
      /* NOTE: The /3 size calculation is because copy_chars() expands TELNET
       * escape sequences. Worst case: every byte could become IAC IAC (2 bytes)
       * plus the null terminator expansion for newlines adds another byte.
       */
      text_space = (MAX_TEXT - (int)ip->text_end - 1) / 3;

      /* shift out processed text from the buffer */
      if (text_space < MAX_TEXT / 16)
        {
          size_t len = ip->text_end - ip->text_start;

          memmove (ip->text, ip->text + ip->text_start, len + 1);
          ip->text_start = 0;
          ip->text_end = len;
          text_space = (MAX_TEXT - ip->text_end - 1) / 3;
          if (text_space < MAX_TEXT / 16)
            {
              /* We've got almost 2k of data without a newline.
               * Discard buffer to prevent DoS from extremely long lines.
               */
              ip->text_start = 0;
              ip->text_end = 0;
              text_space = MAX_TEXT / 3;
            }
        }
      break;

    case PORT_ASCII:
    case PORT_BINARY:
    default:
      /* No protocol overhead - use full buffer */
      text_space = MAX_TEXT - ip->text_end - 1;
      break;
    }

  /*
   * Read user data using appropriate I/O model:
   *
   * Readiness notification (POSIX poll/epoll):
   *   - evt is NULL or evt->buffer is NULL
   *   - Socket is ready to read, perform synchronous recv()/read()
   *   - Used on Linux, BSD, macOS
   *
   * Completion notification (Windows IOCP):
   *   - evt->buffer contains data already read asynchronously
   *   - evt->bytes_transferred indicates how many bytes were read
   *   - Must post next async read operation to continue receiving data
   *   - Used on Windows
   */
  if (evt && evt->buffer && evt->bytes_transferred > 0)
    {
      /* Completion notification: use data already in event buffer (Windows IOCP) */
      num_bytes = evt->bytes_transferred;
      if (num_bytes > text_space)
        {
          num_bytes = text_space;  /* Truncate if buffer overflow */
        }
      memcpy(buf, evt->buffer, num_bytes);

      /* Post next async read to continue receiving data */
      opt_trace (TT_COMM|3, "Number of bytes received: %d. Posting next async read for fd %d\n", num_bytes, ip->fd);
      
      if (async_runtime_post_read(g_runtime, ip->fd, NULL, 0) != 0)
        {
          debug_message("get_user_data: failed to post next read for fd %d\n", ip->fd);
          /* Treat as connection error */
          ip->iflags |= NET_DEAD;
          remove_interactive(ip->ob, 0);
          return;
        }
    }
  else
    {
      /* Readiness notification: perform synchronous read (POSIX poll/epoll)
       * 
       * NOTE: Console input NEVER reaches this path. Console uses a dedicated
       * worker thread that posts completions to async_queue, processed in
       * process_io() under CONSOLE_COMPLETION_KEY. This function handles only
       * network socket I/O (TELNET, ASCII, BINARY protocols).
       */
      num_bytes = SOCKET_RECV(ip->fd, buf, text_space, 0);
      err = SOCKET_ERRNO;
    }

  switch (num_bytes)
    {
    case 0:
      if (ip->iflags & CLOSING)
        debug_message ("get_user_data: tried to read from closing fd.\n");
      ip->iflags |= NET_DEAD;
      remove_interactive (ip->ob, 0);
      return;

    case SOCKET_ERROR:
      if ((err != EWOULDBLOCK) && (err != EILSEQ))
        {
          switch (err)
            {
            case EPIPE:
            case ECONNRESET:
            case ETIMEDOUT:
              break;
            default:
              debug_message ("get_user_data: (fd %d): %s\n", ip->fd, strerror (err));
              break;
            }
          ip->iflags |= NET_DEAD;
          remove_interactive (ip->ob, 0);
          return;
        }
      break;

    default:
      buf[num_bytes] = '\0';
      switch (ip->connection_type)
        {
        case PORT_TELNET:
          /*
           * Process TELNET protocol: replace newlines with nulls, handle IAC sequences,
           * process suboption negotiations (TTYPE, NAWS, LINEMODE), etc.
           * copy_chars() implements the TELNET state machine.
           */
          ip->text_end += copy_chars ((UCHAR *) buf, (UCHAR *) ip->text + ip->text_end, num_bytes, ip);
          opt_trace (TT_COMM|3, "Command buffer contains %d characters\n", ip->text_end - ip->text_start);
          /*
           * now, ip->text_end is just after the last character read. If the last character
           * is a newline, the character before ip->text_end will be null.
           */
          ip->text[ip->text_end] = '\0';
          /*
           * handle snooping - snooper does not see type-ahead. seems like
           * that would be very inefficient, for little functional gain.
           */
          if (ip->snoop_by && !(ip->iflags & NOECHO))
            receive_snoop (buf, ip->snoop_by->ob);

          /*
           * set flag if new data completes command.
           */
          if (cmd_in_buf (ip))
            {
              opt_trace (TT_COMM|3, "Command available in buffer for fd %d\n", ip->fd);
              ip->iflags |= CMD_IN_BUF;
            }
          break;

        case PORT_ASCII:
          {
            char *nl, *str;
            char *p = ip->text + ip->text_start;

            memcpy (p, buf, num_bytes);
            ip->text_end = ip->text_start + num_bytes;
            while ((nl = memchr (p, '\n', ip->text_end - ip->text_start)))
              {
                ip->text_start = (nl + 1) - ip->text;

                *nl = 0;
                str = new_string (nl - p, "PORT_ASCII");
                memcpy (str, p, nl - p + 1);
                if (!(ip->ob->flags & O_DESTRUCTED))
                  {
                    push_malloced_string (str);
                    apply (APPLY_PROCESS_INPUT, ip->ob, 1, ORIGIN_DRIVER);
                  }
                if (ip->text_start == ip->text_end)
                  {
                    ip->text_start = 0;
                    ip->text_end = 0;
                    break;
                  }
                else
                  {
                    p = nl + 1;
                  }
              }
            break;
          }

        case PORT_BINARY:
          {
            buffer_t *buffer;
            buffer = allocate_buffer (num_bytes);
            memcpy (buffer->item, buf, num_bytes);
            push_refed_buffer (buffer);
            apply (APPLY_PROCESS_INPUT, ip->ob, 1, ORIGIN_DRIVER);
            break;
          }
        }
    }
}


/** @brief Return the next user command to be processed in sequence.
 * The order of user command being processed is "rotated" so that no one
 * user can monopolize the command processing. The \c s_next_user
 * static variable keeps track of which user should be checked next.
 * This function scans through all connected users starting from
 * s_next_user and looks for a user with a complete command
 * in his input buffer. It also calls \c flush_message() to ensure
 * that any outgoing messages are sent to the user before processing
 * his input.
 * 
 * This should also return a value if there is something in the
 * buffer and we are supposed to be in single character mode.
 * 
 * @returns Pointer to a static buffer containing the next user command to be processed
 * and updates \c command_giver if a command is found, or 0 if no commands are available.
 */
static char* get_user_command () {

  /* A static counter that iterates between all users in sequence.
   * This ensures fair processing of user commands.
   */
  static int s_next_user = 0;

  int i;
  interactive_t *ip = NULL;
  char *user_command = NULL;
  static char buf[MAX_TEXT];

  /*
   * find and return a user command.
   */
  for (i = 0; i < max_users; i++)
    {
      ip = all_users[s_next_user];
      if (ip && ip->message_length)
        {
          object_t *ob = ip->ob;
          flush_message (ip);
          if (!IP_VALID (ip, ob))
            ip = 0;
        }

      if (ip && ip->iflags & CMD_IN_BUF)
        {
          user_command = first_cmd_in_buf (ip);
          if (user_command)
            {
              /* Check if user has their turn */
              if (ip->iflags & HAS_CMD_TURN)
                {
                  ip->iflags &= ~HAS_CMD_TURN;  /* Consume turn */
                  break;  /* Process this command */
                }
              else
                {
                  /* User has command but no turn - skip and continue searching */
                  user_command = NULL;
                }
            }
          else
            ip->iflags &= ~CMD_IN_BUF;
        }

      if (s_next_user-- == 0)
        s_next_user = max_users - 1; /* wrap around */
    }

  /*
   * no cmds found; return 0.
   */
  if (!ip || !user_command)
    return 0;

  /*
   * we have a user cmd -- return it. If user has only one partially
   * completed cmd left after this, move it to the start of his buffer; new
   * stuff will be appended.
   */
  command_giver = ip->ob;

  /*
   * telnet option parsing and negotiation.
   */
  telnet_neg (buf, user_command);

  /*
   * move input buffer pointers to next command.
   */
  next_cmd_in_buf (ip);
  if (!cmd_in_buf (ip))
    ip->iflags &= ~CMD_IN_BUF;

  if (s_next_user-- == 0)
    s_next_user = max_users - 1; /* wrap around */

  if (ip->iflags & NOECHO)
    {
      /*
       * Must not enable echo before the user input is received.
       */
      if (ip->connection_type == PORT_TELNET)
        {
#ifdef HAVE_TERMIOS_H
          struct termios tty;

          tcgetattr (ip->fd, &tty);
          tty.c_lflag |= ECHO;
          safe_tcsetattr (ip->fd, &tty); /* TTY: discard pending input, Pipe: preserve data */
#endif
        }
      else
        add_message (command_giver, telnet_no_echo);
      ip->iflags &= ~NOECHO;
    }

  ip->last_time = current_time;
  return buf;
}


/*
 * find the first character of the next complete cmd in a buffer, 0 if no
 * completed cmd.  There is a completed cmd if there is a null between
 * text_start and text_end.  Zero length commands are discarded (as occur
 * between <cr> and <lf>).  Update text_start if we have to skip leading
 * nulls.
 * This should return true when in single char mode and there is
 * Anything at all in the buffer.
 */
static char *
first_cmd_in_buf (interactive_t * ip)
{
  char *p, *q;

  p = ip->text + ip->text_start;

  /*
   * skip null input.
   */
  while ((p < (ip->text + ip->text_end)) && !*p)
    p++;

  ip->text_start = p - ip->text;

  if (ip->text_start >= ip->text_end)
    {
      ip->text_start = ip->text_end = 0;
      ip->text[0] = '\0';
      return 0;
    }
  /* If we got here, must have something in the array */
  if (ip->iflags & SINGLE_CHAR)
    {
      /* We need to return true here... */
      return (ip->text + ip->text_start);
    }
  /*
   * find end of cmd.
   */
  while ((p < (ip->text + ip->text_end)) && *p)
    p++;
  /*
   * null terminated; was command.
   */
  if (p < ip->text + ip->text_end)
    return (ip->text + ip->text_start);
  /*
   * have a partial command at end of buffer; move it to start, return
   * null. if it can't move down, truncate it and return it as cmd.
   */
  p = ip->text + ip->text_start;
  q = ip->text;
  while (p < (ip->text + ip->text_end))
    *(q++) = *(p++);

  ip->text_end -= ip->text_start;
  ip->text_start = 0;
  if (ip->text_end > MAX_TEXT - 2)
    {
      ip->text[ip->text_end - 2] = '\0';	/* nulls to truncate */
      ip->text[ip->text_end - 1] = '\0';	/* nulls to truncate */
      ip->text_end--;
      return (ip->text);
    }
  /*
   * buffer not full and no newline - no cmd.
   */
  return 0;
}				/* first_command_in_buf() */

/**
 *  @brief Check if there is a complete, non-empty line in the buffer.
 *  Looks for a null character between text_start and text_end.
 *  If in SINGLE_CHAR mode, any input is a complete command.
 *  @param ip The interactive structure for the user.
 *  @return 1 if there is a complete command, otherwise returns zero.
 */
static int cmd_in_buf (interactive_t * ip) {

  const char *p;

  p = ip->text + ip->text_start;

  /* skip empty lines */
  while ((p < (ip->text + ip->text_end)) && !*p)
    p++;

   /* end of user command buffer? */
  if ((p - ip->text) >= ip->text_end)
    return 0;

  /* expecting single character input? */
  if (ip->iflags & SINGLE_CHAR)
    return 1;

  /* find end of command */
  while ((p < (ip->text + ip->text_end)) && *p)
    p++;
  if (p < ip->text + ip->text_end)
    return 1;

  /* user command buffer is empty or only partial command received. */
  return 0;
}

/*
 * move pointers to next cmd, or clear buf.
 */
static void
next_cmd_in_buf (interactive_t * ip)
{
  char *p = ip->text + ip->text_start;

  while (*p && p < ip->text + ip->text_end)
    p++;
  /*
   * skip past any nulls at the end.
   */
  while (!*p && p < ip->text + ip->text_end)
    p++;
  if (p < ip->text + ip->text_end)
    ip->text_start = p - ip->text;
  else
    {
      ip->text_start = ip->text_end = 0;
      ip->text[0] = '\0';
    }
}				/* next_cmd_in_buf() */

/**
 *  @brief Remove an interactive user immediately.
 */
void remove_interactive (object_t * ob, int dested) {

  int idx;
  /* don't have to worry about this dangling, since this is the routine
   * that causes this to dangle elsewhere, and we are protected from
   * getting called recursively by CLOSING.  safe_apply() should be
   * used here, since once we start this process we can't back out,
   * so jumping out with an error would be bad.
   */
  interactive_t *ip = ob->interactive;

  if (!ip)
    return;

  if (ip->iflags & CLOSING)
    {
      if (!dested)
        debug_message ("Double call to remove_interactive()\n");
      return;
    }

  flush_message (ip);
  ip->iflags |= CLOSING;

#ifdef F_ED
  if (ip->ed_buffer)
    {
      save_ed_buffer (ob);
    }
#endif

  if (!dested)
    {
      /*
       * auto-notification of net death
       */
      safe_apply (APPLY_NET_DEAD, ob, 0, ORIGIN_DRIVER);
    }

  if (ip->snoop_by)
    {
      ip->snoop_by->snoop_on = 0;
      ip->snoop_by = 0;
    }
  if (ip->snoop_on)
    {
      ip->snoop_on->snoop_by = 0;
      ip->snoop_on = 0;
    }

  /* Unregister from async runtime (except console on POSIX) */
  if (ip != all_users[0])
    {
      async_runtime_remove (g_runtime, ip->fd);
    }

  if (MAIN_OPTION(console_mode) && ip == all_users[0])
    {
      console_type_t console_type = async_runtime_get_console_type(g_runtime);
      /* Check if stdin is a pipe/file - if so, exit instead of trying to reconnect */
#ifdef _WIN32
      if (console_type == CONSOLE_TYPE_PIPE || console_type == CONSOLE_TYPE_FILE) {
        debug_message ("Console input closed (pipe/file) - shutting down\n");
        g_proceeding_shutdown++;
      }
#else
      if (!isatty(STDIN_FILENO)) {
        debug_message ("Console input closed (pipe/file) - shutting down\n");
        g_proceeding_shutdown++;
      }
#endif
      else if (console_type == CONSOLE_TYPE_REAL) {
        /* Real console - allow reconnection */
        add_message (ob, "===== PRESS ENTER TO RECONNECT CONSOLE =====\n");
        flush_message (ip);
      }
    }
  else
    {
      if (SOCKET_CLOSE (ip->fd) == SOCKET_ERROR)
        debug_perror ("remove_interactive: close", 0);
    }
  if (ob->flags & O_HIDDEN)
    num_hidden--;
  num_user--;
  clear_notify (ip);
  if (ip->input_to)
    {
      free_sentence (ip->input_to);
      ip->input_to = 0;
    }
  for (idx = 0; idx < max_users; idx++)
    if (all_users[idx] == ip)
      break;
  DEBUG_CHECK (idx == max_users, "remove_interactive: could not find and remove user!\n");
  FREE (ip);
  total_users--;
  ob->interactive = 0;
  all_users[idx] = 0;
  free_object (ob, "remove_interactive");
  return;
}				/* remove_interactive() */

/**
 * Call a function on an interactive object set up by input_to() efun or
 * get_char() efun.
 *
 * @param i The interactive structure for the user.
 * @param str The input string to pass to the function.
 * @return 1 if a function was called, otherwise returns 0.
 */
int call_function_interactive (interactive_t * i, char *str) {

  funptr_t *funp = NULL;
  array_t *args;
  sentence_t *sent;
  int num_arg;

  i->iflags &= ~NOESC; /* remove disable shell escape flag */

  if (!(sent = i->input_to))
    return 0; /* no input_to() was set up on this interactive */

  /* [NEOLITH-EXTENSION] The sentence is always V_FUNCTION now. And carryover arguments
   * are passed via sent->args array. This is more efficient and flexible than the old
   * code that stores a carryover svalue_t array in the interactive_t struct.
   */
  DEBUG_CHECK (!(sent->flags & V_FUNCTION), "input_to must be function pointer");
  funp = sent->function.f;
  funp->hdr.ref++; /* by local variable funp */

  args = sent->args;
  if (args)
    args->ref++; /* by local variable args */
  num_arg = args ? args->size : 0;

  /* Free sentence before calling the function pointer.
   * This is necessary since the input_to/get_char callback (LPC code) may call
   * set_call() again to set up a new input_to before the current callback returns,
   * and we need to free the old sentence or the set_call() will fail due to the
   * existing sentence.
   */
  free_sentence (sent);
  i->input_to = 0;

  /* Disable single char mode if needed */
  if (i->iflags & SINGLE_CHAR)
    {
      i->iflags &= ~SINGLE_CHAR;
      set_telnet_single_char (i, 0);
    }

  /* Push input FIRST.
   * The LPC efun input_to/get_char expect the input string to be the
   * first argument, followed by any carryover args from the original call
   * to input_to/get_char.
   */
  copy_and_push_string (str);

  if (args)
    {
      /* Push carryover args AFTER input */
      for (int j = 0; j < args->size; j++)
        {
          push_svalue (&args->item[j]);
        }
      free_array (args); /* by local variable args */
      args = 0; /* this is always the last reference to carryover args array */
    }

  /* Call function pointer.
   * The function pointer can be a closure with arguments already bound.
   * In the case, they will be combined via merge_arg_lists() to form the
   * actual argument list. For example:
   *     input_to(bind((: foo :), arg1, arg2), I_NOECHO, arg3, arg4);
   * will result in a call to:
   *     foo(arg1, arg2, str, arg3, arg4) where str is the user input.
   */
  call_function_pointer (funp, num_arg + 1);
  free_funp (funp); /* by local variable funp */
  funp = 0;
  return 1;
}				/* call_function_interactive() */

/**
 *  @brief Set up an input_to call for an interactive object.
 *  @param ob The interactive object.
 *  @param sent The sentence (function and object) to call.
 *  @param flags Flags for the input_to call (I_NOECHO, I_NOESC, I_SINGLE_CHAR).
 *  @return 1 on success, 0 on failure.
 */
int set_call (object_t * ob, sentence_t * sent, int flags) {
  if (ob == 0 || sent == 0 || ob->interactive == 0 || ob->interactive->input_to)
    return 0;

  ob->interactive->input_to = sent;
  ob->interactive->iflags |= (flags & (I_NOECHO | I_NOESC | I_SINGLE_CHAR));

  if (ob->interactive == all_users[0])
    {
      /* don't try to set telnet options on console */
#ifdef HAVE_TERMIOS_H
      if (flags & I_NOECHO)
        {
          struct termios tio;

          tcgetattr (ob->interactive->fd, &tio);
          tio.c_lflag &= ~ECHO;
          safe_tcsetattr (ob->interactive->fd, &tio); /* TTY: discard pending input, Pipe: preserve data */
        }
#endif
    }
  else
    {
      /* This is a TELNET trick to hide input by telling the client that we'll be doing echo,
       * but we won't actually do it.
       */
      if (flags & I_NOECHO)
        add_message (ob, telnet_yes_echo);
    }

  if (flags & I_SINGLE_CHAR)
    set_telnet_single_char (ob->interactive, 1);
  return 1;
}				/* set_call() */


void
set_prompt (char *str)
{
  if (command_giver && command_giver->interactive)
    command_giver->interactive->prompt = str;
}


/*
 * Print the prompt, but only if input_to not is disabled.
 */
static void
print_prompt (interactive_t * ip)
{
  object_t *ob = ip->ob;

  if (ip->input_to == 0)
    {
      /* give user object a chance to write its own prompt */
      if (!(ip->iflags & HAS_WRITE_PROMPT))
        tell_object (ip->ob, ip->prompt);
#ifdef OLD_ED
      else if (ip->ed_buffer)
        tell_object (ip->ob, ip->prompt);
#endif
      else if (!apply (APPLY_WRITE_PROMPT, ip->ob, 0, ORIGIN_DRIVER))
        {
          if (!IP_VALID (ip, ob))
            return;
          ip->iflags &= ~HAS_WRITE_PROMPT;
          tell_object (ip->ob, ip->prompt);
        }
    }

  if (!IP_VALID (ip, ob))
    return;

  /*
   * Put the IAC GA thing in here... Moved from before writing the prompt;
   * vt src says it's a terminator. Should it be inside the no-input_to
   * case? We'll see, I guess.
   */
  if (ip->iflags & USING_TELNET)
    add_message (command_giver, telnet_ga);

  if (!IP_VALID (ip, ob))
    return;

  flush_message (ip);
}


/*
 * Let object 'me' snoop object 'you'. If 'you' is 0, then turn off
 * snooping.
 *
 * This routine is almost identical to the old set_snoop. The main
 * difference is that the routine writes nothing to user directly,
 * all such communication is taken care of by the mudlib. It communicates
 * with master.c in order to find out if the operation is permissble or
 * not. The old routine let everyone snoop anyone. This routine also returns
 * 0 or 1 depending on success.
 */
int
new_set_snoop (object_t * me, object_t * you)
{
  interactive_t *on, *by, *tmp;

  /*
   * Stop if people managed to quit before we got this far.
   */
  if (me->flags & O_DESTRUCTED)
    return (0);
  if (you && (you->flags & O_DESTRUCTED))
    return (0);
  /*
   * Find the snooper && snoopee.
   */
  if (!me->interactive)
    error ("First argument of snoop() is not interactive!\n");

  by = me->interactive;

  if (you)
    {
      if (!you->interactive)
        error ("Second argument of snoop() is not interactive!\n");
      on = you->interactive;
    }
  else
    {
      /*
       * Stop snoop.
       */
      if (by->snoop_on)
        {
          by->snoop_on->snoop_by = 0;
          by->snoop_on = 0;
        }
      return 1;
    }

  /*
   * Protect against snooping loops.
   */
  for (tmp = on; tmp; tmp = tmp->snoop_on)
    {
      if (tmp == by)
        return (0);
    }

  /*
   * Terminate previous snoop, if any.
   */
  if (by->snoop_on)
    {
      by->snoop_on->snoop_by = 0;
      by->snoop_on = 0;
    }
  if (on->snoop_by)
    {
      on->snoop_by->snoop_on = 0;
      on->snoop_by = 0;
    }
  on->snoop_by = by;
  by->snoop_on = on;
  return (1);
}				/* set_new_snoop() */


/*
 * Bit of a misnomer now.  But I can't be bothered changeing the
 * name.  This will handle backspace resolution amongst other things,
 * (Pinkfish change)
 */
static void
telnet_neg (char *to, char *from)
{
  int ch;
  char *first;

  first = to;

  while (1)
    {
      ch = *from++;
      switch (ch)
        {
        case '\b':		/* Backspace */
        case 0x7f:		/* Delete */
          if (to <= first)
            continue;
          to -= 1;
          continue;
        default:
          *to++ = (char)ch;
          if (ch == 0)
            return;
        }
    }
}

static void query_addr_name (object_t * ob) {
  char buf[100];
  char *dbuf = &buf[sizeof (int) + sizeof (int) + sizeof (int)];
  size_t msglen;
  int msgtype;

  if (addr_server_fd == INVALID_SOCKET_FD)
    return;

  sprintf (dbuf, "%s", query_ip_number (ob));
  msglen = sizeof (int) + strlen (dbuf) + 1;

  msgtype = DATALEN;
  memcpy (buf, (char *) &msgtype, sizeof (msgtype));
  memcpy (&buf[sizeof (int)], (char *) &msglen, sizeof (msglen));

  msgtype = NAMEBYIP;
  memcpy (&buf[sizeof (int) + sizeof (int)], (char *) &msgtype, sizeof (msgtype));

  if (SOCKET_SEND (addr_server_fd, buf, msglen + sizeof (int) + sizeof (int), 0) == SOCKET_ERROR)
    {
      switch (SOCKET_ERRNO)
        {
        case EBADF:
          debug_message ("Address server has closed connection.\n");
          addr_server_fd = -1;
          break;
        default:
          debug_error ("send() failed: %d", SOCKET_ERRNO);
          break;
        }
    }
}				/* query_addr_name() */

#define IPSIZE 200
typedef struct
{
  char *name, *call_back;
  object_t *ob_to_call;
}
ipnumberentry_t;

static ipnumberentry_t ipnumbertable[IPSIZE];

/*
 * Does a call back on the current_object with the function call_back.
 */
int
query_addr_number (char *name, char *call_back)
{
  static char buf[100];
  static char *dbuf = &buf[sizeof (int) + sizeof (int) + sizeof (int)];
  size_t msglen;
  int msgtype;

  if ((addr_server_fd < 0) || (strlen (name) >= 100 - (sizeof (msgtype) + sizeof (msglen) + sizeof (int))))
    {
      share_and_push_string (name);
      push_undefined ();
      apply (call_back, current_object, 2, ORIGIN_DRIVER);
      return 0;
    }
  strcpy (dbuf, name);
  msglen = sizeof (int) + strlen (name) + 1;

  msgtype = DATALEN;
  memcpy (buf, (char *) &msgtype, sizeof (msgtype));
  memcpy (&buf[sizeof (int)], (char *) &msglen, sizeof (msglen));

  msgtype = (name[0] >= '0' && name[0] <= '9') ? NAMEBYIP : IPBYNAME;
  memcpy (&buf[sizeof (int) + sizeof (int)], (char *) &msgtype, sizeof (msgtype));

  if (SOCKET_SEND (addr_server_fd, buf, msglen + sizeof (int) + sizeof (int), 0) == SOCKET_ERROR)
    {
      switch (SOCKET_ERRNO)
        {
        case EBADF:
          debug_message ("Address server has closed connection.\n");
          addr_server_fd = -1;
          break;
        default:
          debug_error ("send() failed: %d", SOCKET_ERRNO);
          break;
        }
      share_and_push_string (name);
      push_undefined ();
      apply (call_back, current_object, 2, ORIGIN_DRIVER);
      return 0;
    }
  else
    {
      int i;

/* We put ourselves into the pending name lookup entry table */
/* Find the first free entry */
      for (i = 0; i < IPSIZE && ipnumbertable[i].name; i++)
        ;
      if (i == IPSIZE)
        {
/* We need to error...  */
          share_and_push_string (name);
          push_undefined ();
          apply (call_back, current_object, 2, ORIGIN_DRIVER);
          return 0;
        }
/* Create our entry... */
      ipnumbertable[i].name = make_shared_string (name);
      ipnumbertable[i].call_back = make_shared_string (call_back);
      ipnumbertable[i].ob_to_call = current_object;
      add_ref (current_object, "query_addr_number: ");
      return i + 1;
    }
}				/* query_addr_number() */

static void
got_addr_number (char *number, char *name)
{
  int i;
  char *theName, *theNumber;

  /* First remove all the dested ones... */
  for (i = 0; i < IPSIZE; i++)
    if (ipnumbertable[i].name && ipnumbertable[i].ob_to_call->flags & O_DESTRUCTED)
      {
        free_string (ipnumbertable[i].call_back);
        free_string (ipnumbertable[i].name);
        free_object (ipnumbertable[i].ob_to_call, "got_addr_number: ");
        ipnumbertable[i].name = NULL;
      }
  for (i = 0; i < IPSIZE; i++)
    {
      if (ipnumbertable[i].name && strcmp (name, ipnumbertable[i].name) == 0)
        {
          /* Found one, do the call back... */
          theName = ipnumbertable[i].name;
          theNumber = number;

          if (isdigit (theName[0]))
            {
              char *tmp;

              tmp = theName;
              theName = theNumber;
              theNumber = tmp;
            }
          if (strcmp (theName, "0"))
            {
              share_and_push_string (theName);
            }
          else
            {
              push_undefined ();
            }
          if (strcmp (number, "0"))
            {
              share_and_push_string (theNumber);
            }
          else
            {
              push_undefined ();
            }
          push_number (i + 1);
          safe_apply (ipnumbertable[i].call_back, ipnumbertable[i].ob_to_call, 3, ORIGIN_DRIVER);
          free_string (ipnumbertable[i].call_back);
          free_string (ipnumbertable[i].name);
          free_object (ipnumbertable[i].ob_to_call, "got_addr_number: ");
          ipnumbertable[i].name = NULL;
        }
    }
}				/* got_addr_number() */

#undef IPSIZE
#define IPSIZE 200
typedef struct ipentry_s {
  unsigned long addr;
  char *name;
} ipentry_t;

static ipentry_t iptable[IPSIZE];
static int ipcur;

/**
 * @brief Return the cached name for the IP address of an interactive object.
 * If no name is cached, return the IP address as a string.
 * @param ob The interactive object. If NULL, use command_giver.
 * @return The cached name or the IP address as a string.
 */
char *query_ip_name (object_t * ob) {
  int i;

  if (ob == 0)
    ob = command_giver;
  if (!ob || ob->interactive == 0)
    return NULL;
  for (i = 0; i < IPSIZE; i++)
    {
      if (iptable[i].addr == ob->interactive->addr.sin_addr.s_addr && iptable[i].name)
        return (iptable[i].name);
    }
  return query_ip_number (ob); /* fallback to return IP address as string */
}

static void add_ip_entry (unsigned long addr, const char *name) {
  int i;

  for (i = 0; i < IPSIZE; i++)
    {
      if (iptable[i].addr == addr)
        return;
    }
  iptable[ipcur].addr = addr;
  if (iptable[ipcur].name)
    free_string (iptable[ipcur].name);
  iptable[ipcur].name = make_shared_string (name);
  ipcur = (ipcur + 1) % IPSIZE;
}

static void reset_ip_names (void) {
  int i;
  for (i = 0; i < IPSIZE; i++)
    {
      if (iptable[i].name)
        {
          free_string (iptable[i].name);
          iptable[i].name = NULL;
        }
      iptable[i].addr = 0;
    }
}

/**
 * @brief Return the IP address of an interactive object as a string.
 * If the object is NULL, use command_giver.
 * @param ob The interactive object.
 * @return The IP address as a string, or "N/A" if not interactive.
 */
char *query_ip_number (object_t * ob) {
  static char ip_name[50];
  if (ob == 0)
    ob = command_giver;
  if (!ob || ob->interactive == 0)
    return "N/A";
  inet_ntop (AF_INET, &ob->interactive->addr.sin_addr, ip_name, sizeof (ip_name));
  return ip_name;
}

char *query_host_name () {
  static char name[128];

  gethostname (name, sizeof (name));
  name[sizeof (name) - 1] = '\0';
  return name;
}

object_t *query_snoop (object_t * ob) {
  if (!ob->interactive || (ob->interactive->snoop_by == 0))
    return (0);
  return (ob->interactive->snoop_by->ob);
}				/* query_snoop() */

object_t *query_snooping (object_t * ob) {
  if (!ob->interactive || (ob->interactive->snoop_on == 0))
    return (0);
  return (ob->interactive->snoop_on->ob);
}				/* query_snooping() */

time_t
query_idle (object_t * ob)
{
  if (!ob->interactive)
    error ("query_idle() of non-interactive object.\n");
  return (current_time - ob->interactive->last_time);
}				/* query_idle() */

void notify_no_command () {
  string_or_func_t p;
  svalue_t *v;

  if (!command_giver || !command_giver->interactive)
    return;
  p = command_giver->interactive->default_err_message;
  if (command_giver->interactive->iflags & NOTIFY_FAIL_FUNC)
    {
      save_command_giver (command_giver);
      v = call_function_pointer (p.f, 0);
      restore_command_giver ();
      free_funp (p.f);
      if (command_giver && command_giver->interactive)
        {
          if (v && v->type == T_STRING)
            tell_object (command_giver, v->u.string);
          command_giver->interactive->iflags &= ~NOTIFY_FAIL_FUNC;
          command_giver->interactive->default_err_message.s = 0;
        }
    }
  else
    {
      if (p.s)
        {
          tell_object (command_giver, p.s);
          free_string (p.s);
          command_giver->interactive->default_err_message.s = 0;
        }
      else if (CONFIG_STR (__DEFAULT_FAIL_MESSAGE__))
        {
          add_vmessage (command_giver, "%s\n",
                        CONFIG_STR (__DEFAULT_FAIL_MESSAGE__));
        }
      else
        {
          tell_object (command_giver, "What?\n");
        }
    }
}				/* notify_no_command() */

static void
clear_notify (interactive_t * ip)
{
  string_or_func_t dem;

  dem = ip->default_err_message;
  if (ip->iflags & NOTIFY_FAIL_FUNC)
    {
      free_funp (dem.f);
      ip->iflags &= ~NOTIFY_FAIL_FUNC;
    }
  else if (dem.s)
    free_string (dem.s);
  ip->default_err_message.s = 0;
}				/* clear_notify() */

void
set_notify_fail_message (char *str)
{
  if (!command_giver || !command_giver->interactive)
    return;
  clear_notify (command_giver->interactive);
  command_giver->interactive->default_err_message.s =
    make_shared_string (str);
}				/* set_notify_fail_message() */

void
set_notify_fail_function (funptr_t * funp)
{
  if (!command_giver || !command_giver->interactive)
    return;
  clear_notify (command_giver->interactive);
  command_giver->interactive->iflags |= NOTIFY_FAIL_FUNC;
  command_giver->interactive->default_err_message.f = funp;
  funp->hdr.ref++;
}				/* set_notify_fail_message() */

int
replace_interactive (object_t * ob, object_t * obfrom)
{
  if (ob->interactive)
    {
      error ("Bad argument 1 to exec()\n");
    }
  if (!obfrom->interactive)
    {
      error ("Bad argument 2 to exec()\n");
    }
  if ((ob->flags & O_HIDDEN) != (obfrom->flags & O_HIDDEN))
    {
      if (ob->flags & O_HIDDEN)
        {
          num_hidden++;
        }
      else
        {
          num_hidden--;
        }
    }
  ob->interactive = obfrom->interactive;
  /*
   * assume the existance of write_prompt and process_input in user.c until
   * proven wrong (after trying to call them).
   */
  ob->interactive->iflags |= (HAS_WRITE_PROMPT | HAS_PROCESS_INPUT);
  obfrom->interactive = 0;
  ob->interactive->ob = ob;
  ob->flags |= O_ONCE_INTERACTIVE;
  obfrom->flags &= ~O_ONCE_INTERACTIVE;
  add_ref (ob, "exec");
  free_object (obfrom, "exec");
  if (obfrom == command_giver)
    {
      command_giver = ob;
    }
  return (1);
}				/* replace_interactive() */

/*
 * Return the async runtime instance for integration with other subsystems
 * (e.g., timer callback wake-up on Windows).
 */
async_runtime_t *
get_async_runtime(void) {
    return g_runtime;
}
