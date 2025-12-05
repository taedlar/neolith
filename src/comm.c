#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "std.h"
#include "lpc/object.h"
#include "lpc/buffer.h"
#include "comm.h"
#include "rc.h"
#include "simul_efun.h"
#include "interpret.h"
#include "socket/socket_efuns.h"
#include "socket/socket_ctrl.h"
#include "efuns/ed.h"

#include "lpc/include/origin.h"

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
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
#define SCHAR	char

int total_users = 0;

/*
 * local function prototypes.
 */
static int copy_chars (UCHAR *, UCHAR *, int, interactive_t *);
static void sigpipe_handler (int);
static void hname_handler (void);
static void get_user_data (interactive_t *);
static char *get_user_command (void);
static char *first_cmd_in_buf (interactive_t *);
static int cmd_in_buf (interactive_t *);
static void next_cmd_in_buf (interactive_t *);
static int call_function_interactive (interactive_t *, char *);
static void print_prompt (interactive_t *);
static void telnet_neg (char *, char *);
static void query_addr_name (object_t *);
static void got_addr_number (char *, char *);
static void add_ip_entry (long, const char *);
static void clear_notify (interactive_t *);
static void new_user_handler (int);
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

static fd_set readmask, writemask;
static int addr_server_fd = -1;

/* implementations */

static void
receive_snoop (char *buf, object_t * snooper)
{
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
      if ((external_port[i].fd = socket (AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
        {
          debug_perror ("socket()", 0);
          debug_fatal ("Failed to create socket for port %d\n", external_port[i].port);
          exit (EXIT_FAILURE);
        }

      /* enable local address reuse. */
      optval = 1;
      if (setsockopt (external_port[i].fd, SOL_SOCKET, SO_REUSEADDR, (char *) &optval, sizeof (optval)) == SOCKET_ERROR)
        {
          debug_perror ("setsockopt()", 0);
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
          debug_perror ("bind()", 0);
          debug_fatal ("Failed to bind to port %d\n", external_port[i].port);
          exit (3);
        }

      /* get socket name. */
      sin_len = sizeof (sin);
      if (getsockname (external_port[i].fd, (struct sockaddr *) &sin, &sin_len) == SOCKET_ERROR)
        {
          debug_perror ("getsockname()", 0);
          debug_fatal ("Failed to get socket name for port %d\n", external_port[i].port);
          exit (4);
        }
      /* set socket non-blocking, */
      if (set_socket_nonblocking (external_port[i].fd, 1) == SOCKET_ERROR)
        {
          debug_perror ("set_socket_nonblocking()", 0);
          debug_fatal ("Failed to set socket non-blocking on port %d\n", external_port[i].port);
          exit (8);
        }
      /* listen on socket for connections. */
      if (listen (external_port[i].fd, SOMAXCONN) == SOCKET_ERROR)
        {
          debug_perror ("listen()", 0);
          debug_fatal ("Failed to listen on port %d\n", external_port[i].port);
          exit (10);
        }
    }
  opt_trace (TT_BACKEND, "finished initializing user connection sockets.\n");

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

/*
 * Shut down new user accept file descriptor.
 */
void
ipc_remove ()
{
  int i;

  for (i = 0; i < 5; i++)
    {
      if (!external_port[i].port)
        continue;
      debug_message ("closing service on TCP port %d\n", external_port[i].port);
      if (close (external_port[i].fd) == -1)
        debug_perror ("ipc_remove: close", 0);
    }

}

int
do_comm_polling (struct timeval *timeout)
{
  return select (FD_SETSIZE, &readmask, &writemask, NULL, timeout);
}

/*
 * Send a message to an interactive object.
 */
void
add_message (object_t * who, char *data)
{
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
int
flush_message (interactive_t * ip)
{
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
       */
      num_bytes = (ip->fd == STDIN_FILENO) ?
        write (STDOUT_FILENO, ip->message_buf + ip->message_consumer, length) :
        send (ip->fd, ip->message_buf + ip->message_consumer, length, ip->out_of_band);
      if (num_bytes == -1)
        {
          if (errno == EWOULDBLOCK || errno == EINTR)
            return 1;

          if (errno != EPIPE)
            debug_perror ("flush_message: write", 0);
          ip->iflags |= NET_DEAD;
          return 0;
        }
      ip->message_consumer = (ip->message_consumer + num_bytes) % MESSAGE_BUF_SIZE;
      ip->message_length -= num_bytes;
      ip->out_of_band = 0;
      inet_packets++;
      inet_volume += num_bytes;
    }
  return 1;
}				/* flush_message() */


#define TS_DATA         0
#define TS_IAC          1
#define TS_WILL         2
#define TS_WONT         3
#define TS_DO           4
#define TS_DONT         5
#define TS_SB		6
#define TS_SB_IAC       7

static char telnet_break_response[] = { 28, (SCHAR) IAC, (SCHAR) WILL, TELOPT_TM, 0 };
static char telnet_interrupt_response[] = { 127, (SCHAR) IAC, (SCHAR) WILL, TELOPT_TM, 0 };
static char telnet_abort_response[] = { (SCHAR) IAC, (SCHAR) DM, 0 };
static char telnet_do_tm_response[] = { (SCHAR) IAC, (SCHAR) WILL, TELOPT_TM, 0 };
static char telnet_do_sga[] = { (SCHAR) IAC, (SCHAR) DO, TELOPT_SGA, 0 };
static char telnet_will_sga[] = { (SCHAR) IAC, (SCHAR) WILL, TELOPT_SGA, 0 };
static char telnet_wont_sga[] = { (SCHAR) IAC, (SCHAR) WONT, TELOPT_SGA, 0 };
static char telnet_do_naws[] = { (SCHAR) IAC, (SCHAR) DO, TELOPT_NAWS, 0 };
static char telnet_do_ttype[] = { (SCHAR) IAC, (SCHAR) DO, TELOPT_TTYPE, 0 };
static char telnet_do_linemode[] = { (SCHAR) IAC, (SCHAR) DO, TELOPT_LINEMODE, 0 };
static char telnet_term_query[] = { (SCHAR) IAC, (SCHAR) SB, TELOPT_TTYPE, TELQUAL_SEND, (SCHAR) IAC, (SCHAR) SE, 0 };
static char telnet_no_echo[] = { (SCHAR) IAC, (SCHAR) WONT, TELOPT_ECHO, 0 };
static char telnet_yes_echo[] = { (SCHAR) IAC, (SCHAR) WILL, TELOPT_ECHO, 0 };
static char telnet_sb_lm_mode[] = { (SCHAR) IAC, (SCHAR) SB, TELOPT_LINEMODE, LM_MODE, MODE_ACK, (SCHAR) IAC, (SCHAR) SE, 0 };
static char telnet_sb_lm_slc[] = { (SCHAR) IAC, (SCHAR) SB, TELOPT_LINEMODE, LM_SLC, 0 };
static char telnet_se[] = { (SCHAR) IAC, (SCHAR) SE, 0 };
static char telnet_ga[] = { (SCHAR) IAC, (SCHAR) GA, 0 };

/**
 * @brief Copy a string, replacing newlines with '\0'. Also add an extra
 * space and back space for every newline. This trick will allow
 * otherwise empty lines, as multiple newlines would be replaced by
 * multiple zeroes only.
 *
 * Also handle the telnet stuff.  So instead of this being a direct
 * copy it is a small state thingy.
 *
 * In fact, it is telnet_neg conglomerated into this.  This is mostly
 * done so we can sanely remove the telnet sub option negotation stuff
 * out of the input stream.  Need this for terminal types.
 * (Pinkfish change)
 */
static int copy_chars (UCHAR * from, UCHAR * to, int n, interactive_t * ip)
{
  int i;
  UCHAR *start = to;

  /*
   *    scan through the input buffer for TELNET commands and process
   *    if found.
   */
  for (i = 0; i < n; i++)
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
              ip->sb_buf[ip->sb_pos++] = (SCHAR) IAC;
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
                    copy_and_push_string (ip->sb_buf + 2);
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
                            break;
                          }
                        /* if no MODE_ACK bit set, client is trying to set our
                         * LM_MODE (which violate RFC-1091), we just ignore
                         * them. --- Annihilator@ES2 [2002-05-07] */
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
                    /* TODO: Telnet subnegotiation data may contain '\0'
                     * characters, passing as string implicitly truncated
                     * anything beyond '\0'. Maybe need change to buffer
                     * or something. --- Annihilator@ES2 [2002-05-07]
                     */
                    copy_and_push_string (ip->sb_buf);
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
          ip->iflags |= USING_TELNET;

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
        case TS_WONT:
          /* if we get any IAC WILL or IAC WONTs back, we assume they
           * understand the telnet protocol.  Typically this will become
           * set at the first IAC WILL/WONT TTYPE/NAWS response to the
           * initial queries.
           */
          ip->iflags |= USING_TELNET;
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
}				/* copy_chars() */


/**
 *  @brief set_telnet_single_char () - set single-char mode on/off
 */
static void set_telnet_single_char (interactive_t * ip, int single)
{
  if (ip->fd == STDIN_FILENO)
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
      tcsetattr (ip->fd, TCSAFLUSH, &tio); /* discard pending input */
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

#ifndef _WIN32
/*
 * SIGPIPE handler -- does very little for now.
 */
static void
sigpipe_handler (int sig)
{
  (void) sig;
  debug_message ("SIGPIPE received.\n");
  signal (SIGPIPE, sigpipe_handler);
}
#endif

void
make_selectmasks ()
{
  int i;

  /*
   * generate readmask and writemask for select() call.
   */
  FD_ZERO (&readmask);
  FD_ZERO (&writemask);
  /*
   * set new user accept fd in readmask.
   */
  for (i = 0; i < 5; i++)
    {
      if (!external_port[i].port)
        continue;
      FD_SET (external_port[i].fd, &readmask);
    }
  /*
   * set user fds in readmask.
   */
  for (i = 0; i < max_users; i++)
    {
      if (!all_users[i] || (all_users[i]->iflags & (CLOSING | CMD_IN_BUF)))
        continue;
      /*
       * if this user needs more input to make a complete command, set his
       * fd so we can get it.
       */
      FD_SET (all_users[i]->fd, &readmask);
      if (all_users[i]->message_length != 0)
        {
          if (all_users[i]->fd != STDIN_FILENO)
            FD_SET (all_users[i]->fd, &writemask);
        }
    }
  if (MAIN_OPTION(console_mode))
    FD_SET(STDIN_FILENO, &readmask); // for console re-connect

  /*
   * if addr_server_fd is set, set its fd in readmask.
   */
  if (addr_server_fd >= 0)
    {
      FD_SET (addr_server_fd, &readmask);
    }
#ifdef PACKAGE_SOCKETS
  /*
   * set fd's for efun sockets.
   */
  for (i = 0; i < max_lpc_socks; i++)
    {
      if (lpc_socks[i].state != CLOSED)
        {
          if (lpc_socks[i].state != FLUSHING &&
              (lpc_socks[i].flags & S_WACCEPT) == 0)
            FD_SET (lpc_socks[i].fd, &readmask);
          if (lpc_socks[i].flags & S_BLOCKED)
            FD_SET (lpc_socks[i].fd, &writemask);
        }
    }
#endif
}				/* make_selectmasks() */


/*
 * Process I/O.
 */
void process_io () {

  int console_user_connected = 0;
  int i;

  /*
   * check for new user connection.
   */
  for (i = 0; i < 5; i++)
    {
      if (!external_port[i].port)
        continue;
      if (FD_ISSET (external_port[i].fd, &readmask))
        {
          new_user_handler (i);
        }
    }

  /*
   * check for data pending on user connections.
   */
  for (i = 0; i < max_users; i++)
    {
      if (!all_users[i] || (all_users[i]->iflags & (CLOSING | CMD_IN_BUF)))
        continue;

      if (all_users[i]->iflags & NET_DEAD)
        {
          remove_interactive (all_users[i]->ob, 0);
          continue;
        }

      if (FD_ISSET (all_users[i]->fd, &readmask))
        {
          get_user_data (all_users[i]);
          if (!all_users[i])
            continue;
        }

      if (all_users[i]->fd == STDIN_FILENO)
        {
          console_user_connected = 1;
          flush_message (all_users[i]);
        }
      else if (FD_ISSET (all_users[i]->fd, &writemask))
        flush_message (all_users[i]);
    }
  if (!console_user_connected && FD_ISSET (STDIN_FILENO, &readmask))
    {
      /* [NEOLITH-EXTENSION] console user re-connect */
      init_console_user(1);
    }

#ifdef PACKAGE_SOCKETS
  /*
   * check for data pending on efun socket connections.
   */
  for (i = 0; i < max_lpc_socks; i++)
    {
      if (lpc_socks[i].state != CLOSED)
        if (FD_ISSET (lpc_socks[i].fd, &readmask))
          socket_read_select_handler (i);
      if (lpc_socks[i].state != CLOSED)
        if (FD_ISSET (lpc_socks[i].fd, &writemask))
          socket_write_select_handler (i);
    }
#endif

  /*
   * check for data pending from address server.
   */
  if (addr_server_fd >= 0)
    {
      if (FD_ISSET (addr_server_fd, &readmask))
        {
          hname_handler ();
        }
    }
}

/**
 *  @brief Creates a new interactive structure for a given socket file descriptor.
 *  The master object is set as the command giver object for this new interactive.
 *  @param socket_fd The file descriptor of the socket to associate with the new interactive.
 */
void new_interactive(socket_fd_t socket_fd) {

  int i;
  for (i = 0; i < max_users; i++)
    if (!all_users[i]) /* find free slot in all_users */
      break;

  if (i == max_users)
    {
      /* allocate 50 user slots */
      if (all_users)
        {
          all_users = RESIZE (all_users, max_users + 50, interactive_t *, TAG_USERS, "new_user_handler");
        }
      else
        {
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
  if (socket_fd == STDIN_FILENO)
    master_ob->interactive->iflags = O_CONSOLE_USER;
  /*
   * initialize new user interactive data structure.
   */
  master_ob->interactive->ob = master_ob;
  master_ob->interactive->input_to = 0;
  master_ob->interactive->iflags = 0;
  master_ob->interactive->text[0] = '\0';
  master_ob->interactive->text_end = 0;
  master_ob->interactive->text_start = 0;
  master_ob->interactive->carryover = NULL;
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
  master_ob->interactive->num_carry = 0;
  master_ob->interactive->state = TS_DATA;
  master_ob->interactive->out_of_band = 0;
  all_users[i] = master_ob->interactive;
  all_users[i]->fd = socket_fd;
  set_prompt ("> ");

/* debug_message ("new connection from %s\n", inet_ntoa (addr.sin_addr)); */
  num_user++;
}

/**
 *  @brief This is the new user connection handler. This function is called by the
 *  event handler when data is pending on the listening socket (new_user_fd).
 *  If space is available, an interactive data structure is initialized and
 *  the user is connected.
 *  @param which The index of the external_port array indicating which port
 *  the new connection is on.
 */
static void new_user_handler (int which) {

  socket_fd_t new_socket_fd;
  struct sockaddr_in addr;
  socklen_t length;
  object_t *ob;
  int num_external_ports = sizeof(external_port) / sizeof(external_port[0]);

  if (which >= num_external_ports || !external_port[which].port)
    {
      debug_message ("new_user_handler: invalid port index %d\n", which);
      return;
    }

  length = sizeof (addr);
  new_socket_fd = accept (external_port[which].fd, (struct sockaddr *) &addr, &length);
  if (new_socket_fd < 0)
    {
      if (errno != EWOULDBLOCK)
        {
          debug_perror ("accept()", 0);
        }
      return;
    }

  /*
   * according to Amylaar, 'accepted' sockets in Linux 0.99p6 don't
   * properly inherit the nonblocking property from the listening socket.
   */
  if (set_socket_nonblocking (new_socket_fd, 1) == -1)
    {
      debug_perror ("set_socket_nonblocking", 0);
      close (new_socket_fd);
      return;
    }

  /*
   * Make master object interactive to allow sending messages to the
   * new user during connection setup.
   */
  new_interactive(new_socket_fd);
  master_ob->interactive->connection_type = external_port[which].kind;
#ifdef F_QUERY_IP_PORT
  master_ob->interactive->local_port = external_port[which].port;
#endif
  memcpy ((char *) &master_ob->interactive->addr, (char *) &addr, length);

  ob = mudlib_connect(external_port[which].port, inet_ntoa (addr.sin_addr));
  if (!ob)
    {
      if (master_ob->interactive)
        remove_interactive (master_ob, 0);
      return;
    }
  if (addr_server_fd >= 0)
    query_addr_name (ob);
  if (external_port[which].kind == PORT_TELNET)
    {
      /* Ask permission to ask them for their terminal type */
      add_message (ob, telnet_do_ttype);
      /* Ask them for their window size */
      add_message (ob, telnet_do_naws);
      /* Ask them for linemode */
      add_message (ob, telnet_do_linemode);
    }

  mudlib_logon (ob);
  command_giver = 0;
}				/* new_user_handler() */

/*
 * This is the user command handler. This function is called when
 * a user command needs to be processed.
 * This function calls get_user_command() to get a user command.
 * One user command is processed per execution of this function.
 */
int
process_user_command ()
{
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
              for (p = user_command; *p && p - user_command < MAX_TEXT - 1;
                   p++)
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
      if ((user_command[0] == '!') && (
#ifdef OLD_ED
                                        ip->ed_buffer ||
#endif
                                        (ip->input_to
                                         && !(ip->iflags & NOESC))))
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
                  ret =
                    apply (APPLY_PROCESS_INPUT, command_giver, 1,
                           ORIGIN_DRIVER);
                  VALIDATE_IP (ip, command_giver);
                  if (!ret)
                    ip->iflags &= ~HAS_PROCESS_INPUT;
                  if (ret && ret->type == T_STRING)
                    {
                      strncpy (buf, ret->u.string, MAX_TEXT - 1);
                      process_comand (buf, command_giver);
                    }
                  else if (!ret || ret->type != T_NUMBER || !ret->u.number)
                    {
                      process_comand (tbuf + 1, command_giver);
                    }
                }
              else
                process_comand (tbuf + 1, command_giver);
            }
#ifdef OLD_ED
        }
      else if (ip->ed_buffer)
        {
          ed_cmd (user_command);
#endif /* ED */
        }
      else if (call_function_interactive (ip, user_command))
        {
          ;			/* do nothing */
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
              ret =
                apply (APPLY_PROCESS_INPUT, command_giver, 1, ORIGIN_DRIVER);
              VALIDATE_IP (ip, command_giver);
              if (!ret)
                ip->iflags &= ~HAS_PROCESS_INPUT;
              if (ret && ret->type == T_STRING)
                {
                  strncpy (buf, ret->u.string, MAX_TEXT - 1);
                  process_comand (buf, command_giver);
                }
              else if (!ret || ret->type != T_NUMBER || !ret->u.number)
                {
                  process_comand (tbuf, command_giver);
                }
            }
          else
            process_comand (tbuf, command_giver);
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
/*
 * This is the hname input data handler. This function is called by the
 * master handler when data is pending on the hname socket (addr_server_fd).
 */

static void
hname_handler ()
{
  static char hname_buf[HNAME_BUF_SIZE];
  int num_bytes;
  int tmp;
  char *pp, *q;
  long laddr;

  if (addr_server_fd < 0)
    return;

  num_bytes = SOCKET_READ (addr_server_fd, hname_buf, HNAME_BUF_SIZE);
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
          laddr = inet_addr (hname_buf);
          if (laddr != -1)
            {
              pp = strchr (hname_buf, ' ');
              if (pp)
                {
                  *pp = 0;
                  pp++;
                  q = strchr (pp, '\n');
                  if (q)
                    {
                      *q = 0;
                      if (strcmp (pp, "0"))
                        add_ip_entry (laddr, pp);
                      got_addr_number (pp, hname_buf);	/* Recognises this as
                                                         * failure. */
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
 *  @brief Read pending data for a user into user->interactive->text.
 *  This also does telnet negotiation if the connection type is PORT_TELNET.
 */
static void
get_user_data (interactive_t * ip)
{
  static char buf[MAX_TEXT];
  int text_space;
  int num_bytes;

  /*
   * this /3 is here because of the trick copy_chars() uses to allow empty
   * commands. it needs to be fixed right. later.
   */
  switch (ip->connection_type)
    {
    case PORT_TELNET:
      text_space = (MAX_TEXT - ip->text_end - 1) / 3;
      /*
         * Check if we need more space.
       */
      if (text_space < MAX_TEXT / 16)
        {
          int l = ip->text_end - ip->text_start;

          memmove (ip->text, ip->text + ip->text_start, l + 1);
          ip->text_start = 0;
          ip->text_end = l;
          text_space = (MAX_TEXT - ip->text_end - 1) / 3;
          if (text_space < MAX_TEXT / 16)
            {
              /* almost 2k data without a newline.  Flush it, otherwise
                 text_space will eventually go to zero and dest the user. */
              ip->text_start = 0;
              ip->text_end = 0;
              text_space = MAX_TEXT / 3;
            }
        }
      break;

    case PORT_CONSOLE:
    case PORT_ASCII:
      text_space = MAX_TEXT - ip->text_end - 1;
      break;

    case PORT_BINARY:
    default:
      text_space = MAX_TEXT - ip->text_end - 1;
      break;
    }

  /*
   * read user data.
   */
  num_bytes = SOCKET_READ (ip->fd, buf, text_space);
  switch (num_bytes)
    {
    case 0:
      if (ip->iflags & CLOSING)
        debug_message ("get_user_data: tried to read from closing fd.\n");
      ip->iflags |= NET_DEAD;
      remove_interactive (ip->ob, 0);
      return;

    case -1:
#ifdef EWOULDBLOCK
      if (errno == EWOULDBLOCK)
        {
        }
      else
#endif
        {
          switch (errno)
            {
            case EPIPE:
            case ECONNRESET:
            case ETIMEDOUT:
              break;
            default:
              debug_message ("get_user_data: (fd %d): %s\n", ip->fd, strerror (errno));
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
           * replace newlines with nulls and catenate to buffer. Also do all
           * the useful telnet negotation at this point too. Rip out the sub
           * option stuff and send back anything non useful we feel we have
           * to.
           */
          ip->text_end += copy_chars ((UCHAR *) buf, (UCHAR *) ip->text + ip->text_end, num_bytes, ip);
          /*
           * now, text->end is just after the last char read. If last char
           * was a nl, char *before* text_end will be null.
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
            ip->iflags |= CMD_IN_BUF;
          break;

        case PORT_CONSOLE:
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


/*
 * Return the first cmd of the next user in sequence that has a complete cmd
 * in their buffer.
 * CmdsGiven is used to allow users in ED to send more cmds (if they have
 * them queued up) than users not in ED.
 * This should also return a value if there is something in the
 * buffer and we are supposed to be in single character mode.
 */
#define IncCmdGiver     NextCmdGiver = (NextCmdGiver == 0 ? max_users - 1: \
                                        NextCmdGiver - 1)

static char *
get_user_command ()
{
  static int NextCmdGiver = 0;

  int i;
  interactive_t *ip = NULL;
  char *user_command = NULL;
  static char buf[MAX_TEXT];

  /*
   * find and return a user command.
   */
  for (i = 0; i < max_users; i++)
    {
      ip = all_users[NextCmdGiver];
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
            break;
          else
            ip->iflags &= ~CMD_IN_BUF;
        }

      if (NextCmdGiver-- == 0)
        NextCmdGiver = max_users - 1;
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

  if (NextCmdGiver-- == 0)
    NextCmdGiver = max_users - 1;

  if (ip->iflags & NOECHO)
    {
      /*
       * Must not enable echo before the user input is received.
       */
      if (ip->fd == STDIN_FILENO)
        {
#ifdef HAVE_TERMIOS_H
          struct termios tty;

          tcgetattr (ip->fd, &tty);
          tty.c_lflag |= ECHO;
          tcsetattr (ip->fd, TCSAFLUSH, &tty); /* discard pending input */
#endif
        }
      else
        add_message (command_giver, telnet_no_echo);
      ip->iflags &= ~NOECHO;
    }

  ip->last_time = current_time;
  return (buf);
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

/*
 * return(1) if there is a complete command in ip->text, otherwise return(0).
 */
static int
cmd_in_buf (interactive_t * ip)
{
  char *p;

  p = ip->text + ip->text_start;

  /*
   * skip null input.
   */
  while ((p < (ip->text + ip->text_end)) && !*p)
    p++;

  if ((p - ip->text) >= ip->text_end)
    {
      return (0);
    }
  /* If we get here, must have something in the buffer */
  if (ip->iflags & SINGLE_CHAR)
    {
      return (1);
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
    return (1);
  /*
   * no newline - no cmd.
   */
  return (0);
}				/* cmd_in_buf() */

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

  if (MAIN_OPTION(console_mode) && ip->fd == STDIN_FILENO)
    {
      /* don't close stdin */
      debug_message ("===== PRESS ENTER TO RECONNECT CONSOLE =====\n");
    }
  else
    {
      if (SOCKET_CLOSE (ip->fd) == -1)
        debug_perror ("remove_interactive: close", 0);
    }
  if (ob->flags & O_HIDDEN)
    num_hidden--;
  num_user--;
  clear_notify (ip);
  if (ip->input_to)
    {
      free_object (ip->input_to->ob, "remove_interactive");
      free_sentence (ip->input_to);
      if (ip->num_carry > 0)
        free_some_svalues (ip->carryover, ip->num_carry);
      ip->carryover = NULL;
      ip->num_carry = 0;
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

static int
call_function_interactive (interactive_t * i, char *str)
{
  object_t *ob;
  funptr_t *funp = NULL;
  char *function;
  svalue_t *args;
  sentence_t *sent;
  int num_arg;

  i->iflags &= ~NOESC;
  if (!(sent = i->input_to))
    return (0);

  /*
   * Special feature: input_to() has been called to setup a call to a
   * function.
   */
  if (sent->ob->flags & O_DESTRUCTED)
    {
      /* Sorry, the object has selfdestructed ! */
      free_object (sent->ob, "call_function_interactive");
      free_sentence (sent);
      i->input_to = 0;
      if (i->num_carry)
        free_some_svalues (i->carryover, i->num_carry);
      i->carryover = NULL;
      i->num_carry = 0;
      return (0);
    }
  /*
   * We must all references to input_to fields before the call to apply(),
   * because someone might want to set up a new input_to().
   */
  free_object (sent->ob, "call_function_interactive");
  /* we put the function on the stack in case of an error */
  sp++;
  if (sent->flags & V_FUNCTION)
    {
      function = 0;
      sp->type = T_FUNCTION;
      sp->u.fp = funp = sent->function.f;
      funp->hdr.ref++;
    }
  else
    {
      sp->type = T_STRING;
      sp->subtype = STRING_SHARED;
      sp->u.string = function = sent->function.s;
      ref_string (function);
    }
  ob = sent->ob;
  free_sentence (sent);

  /*
   * If we have args, we have to copy them, so the svalues on the
   * interactive struct can be FREEd
   */
  num_arg = i->num_carry;
  if (num_arg)
    {
      args = i->carryover;
      i->num_carry = 0;
      i->carryover = NULL;
    }
  else
    args = NULL;

  i->input_to = 0;
  if (i->iflags & SINGLE_CHAR)
    {
      /*
       * clear single character mode
       */
      i->iflags &= ~SINGLE_CHAR;
      set_telnet_single_char (i, 0);
    }

  copy_and_push_string (str);
  /*
   * If we have args, we have to push them onto the stack in the order they
   * were in when we got them.  They will be popped off by the called
   * function.
   */
  if (args)
    {
      transfer_push_some_svalues (args, num_arg);
      FREE (args);
    }
  /* current_object no longer set */
  if (function)
    {
      if (function[0] == APPLY___INIT_SPECIAL_CHAR)
        error ("Illegal function name.\n");
      (void) apply (function, ob, num_arg + 1, ORIGIN_DRIVER);
    }
  else
    call_function_pointer (funp, num_arg + 1);

  pop_stack ();			/* remove `function' from stack */

  return (1);
}				/* call_function_interactive() */

/**
 *  @brief Set up an input_to call for an interactive object.
 *  @param ob The interactive object.
 *  @param sent The sentence (function and object) to call.
 *  @param flags Flags for the input_to call (I_NOECHO, I_NOESC, I_SINGLE_CHAR).
 *  @return 1 on success, 0 on failure.
 */
int set_call (object_t * ob, sentence_t * sent, int flags) {
  if (ob == 0 || sent == 0 || ob->interactive == 0 ||
      ob->interactive->input_to)
    return 0;

  ob->interactive->input_to = sent;
  ob->interactive->iflags |= (flags & (I_NOECHO | I_NOESC | I_SINGLE_CHAR));

  if (ob->interactive->fd == STDIN_FILENO && MAIN_OPTION (console_mode))
    {
      /* don't try to set telnet options on console */
#ifdef HAVE_TERMIOS_H
      if (flags & I_NOECHO)
        {
          struct termios tio;

          tcgetattr (ob->interactive->fd, &tio);
          tio.c_lflag &= ~ECHO;
          tcsetattr (ob->interactive->fd, TCSAFLUSH, &tio); /* discard pending input */
        }
#endif /* HAVE_TERMIOS_H */
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
  return (1);
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
          *to++ = ch;
          if (ch == 0)
            return;
        }
    }
}

static void
query_addr_name (object_t * ob)
{
  static char buf[100];
  static char *dbuf = &buf[sizeof (int) + sizeof (int) + sizeof (int)];
  int msglen;
  int msgtype;

  sprintf (dbuf, "%s", query_ip_number (ob));
  msglen = sizeof (int) + strlen (dbuf) + 1;

  msgtype = DATALEN;
  memcpy (buf, (char *) &msgtype, sizeof (msgtype));
  memcpy (&buf[sizeof (int)], (char *) &msglen, sizeof (msglen));

  msgtype = NAMEBYIP;
  memcpy (&buf[sizeof (int) + sizeof (int)], (char *) &msgtype,
          sizeof (msgtype));

  if (SOCKET_WRITE (addr_server_fd, buf, msglen + sizeof (int) + sizeof (int)) == -1)
    {
      switch (errno)
        {
        case EBADF:
          debug_message ("Address server has closed connection.\n");
          addr_server_fd = -1;
          break;
        default:
          debug_perror ("query_addr_name: write", 0);
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
  int msglen;
  int msgtype;

  if ((addr_server_fd < 0) || (strlen (name) >=
                               100 - (sizeof (msgtype) + sizeof (msglen) +
                                      sizeof (int))))
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
  memcpy (&buf[sizeof (int) + sizeof (int)], (char *) &msgtype,
          sizeof (msgtype));

  if (SOCKET_WRITE (addr_server_fd, buf, msglen + sizeof (int) + sizeof (int)) == -1)
    {
      switch (errno)
        {
        case EBADF:
          debug_message ("Address server has closed connection.\n");
          addr_server_fd = -1;
          break;
        default:
          debug_perror ("query_addr_name: write", 0);
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
    if (ipnumbertable[i].name
        && ipnumbertable[i].ob_to_call->flags & O_DESTRUCTED)
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
          safe_apply (ipnumbertable[i].call_back, ipnumbertable[i].ob_to_call,
                      3, ORIGIN_DRIVER);
          free_string (ipnumbertable[i].call_back);
          free_string (ipnumbertable[i].name);
          free_object (ipnumbertable[i].ob_to_call, "got_addr_number: ");
          ipnumbertable[i].name = NULL;
        }
    }
}				/* got_addr_number() */

#undef IPSIZE
#define IPSIZE 200
typedef struct
{
  long addr;
  char *name;
}
ipentry_t;

static ipentry_t iptable[IPSIZE];
static int ipcur;

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
  return (inet_ntoa (ob->interactive->addr.sin_addr));
}

static void add_ip_entry (long addr, const char *name) {
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

char *
query_ip_number (object_t * ob)
{
  if (ob == 0)
    ob = command_giver;
  if (!ob || ob->interactive == 0)
    return "N/A";
  return (inet_ntoa (ob->interactive->addr.sin_addr));
}

char *
query_host_name ()
{
  static char name[128];

  gethostname (name, sizeof (name));
  name[sizeof (name) - 1] = '\0';
  return name;
}

object_t *
query_snoop (object_t * ob)
{
  if (!ob->interactive || (ob->interactive->snoop_by == 0))
    return (0);
  return (ob->interactive->snoop_by->ob);
}				/* query_snoop() */

object_t *
query_snooping (object_t * ob)
{
  if (!ob->interactive || (ob->interactive->snoop_on == 0))
    return (0);
  return (ob->interactive->snoop_on->ob);
}				/* query_snooping() */

int
query_idle (object_t * ob)
{
  if (!ob->interactive)
    error ("query_idle() of non-interactive object.\n");
  return (current_time - ob->interactive->last_time);
}				/* query_idle() */

void
notify_no_command ()
{
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
set_notify_fail_function (funptr_t * fp)
{
  if (!command_giver || !command_giver->interactive)
    return;
  clear_notify (command_giver->interactive);
  command_giver->interactive->iflags |= NOTIFY_FAIL_FUNC;
  command_giver->interactive->default_err_message.f = fp;
  fp->hdr.ref++;
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

void
outbuf_zero (outbuffer_t * outbuf)
{
  outbuf->real_size = 0;
  outbuf->buffer = 0;
}

int
outbuf_extend (outbuffer_t * outbuf, int l)
{
  int limit;

  DEBUG_CHECK (l < 0, "Negative length passed to outbuf_extend.\n");

  if (outbuf->buffer)
    {
      limit = MSTR_SIZE (outbuf->buffer);
      if (outbuf->real_size + l > limit)
        {
          if (outbuf->real_size == USHRT_MAX)
            return 0;		/* TRUNCATED */

          /* assume it's going to grow some more */
          limit = (outbuf->real_size + l) * 2;
          if (limit > USHRT_MAX)
            {
              limit = outbuf->real_size + l;
              if (limit > USHRT_MAX)
                {
                  outbuf->buffer = extend_string (outbuf->buffer, USHRT_MAX);
                  return USHRT_MAX - outbuf->real_size;
                }
            }
          outbuf->buffer = extend_string (outbuf->buffer, limit);
        }
    }
  else
    {
      outbuf->buffer = new_string (l, "outbuf_add");
      outbuf->real_size = 0;
    }
  return l;
}

void
outbuf_add (outbuffer_t * outbuf, char *str)
{
  int l, limit;

  if (!outbuf)
    return;
  l = strlen (str);
  if (outbuf->buffer)
    {
      limit = MSTR_SIZE (outbuf->buffer);
      if (outbuf->real_size + l > limit)
        {
          if (outbuf->real_size == USHRT_MAX)
            return;		/* TRUNCATED */

          /* assume it's going to grow some more */
          limit = (outbuf->real_size + l) * 2;
          if (limit > USHRT_MAX)
            {
              limit = outbuf->real_size + l;
              if (limit > USHRT_MAX)
                {
                  outbuf->buffer = extend_string (outbuf->buffer, USHRT_MAX);
                  strncpy (outbuf->buffer + outbuf->real_size, str,
                           USHRT_MAX - outbuf->real_size);
                  outbuf->buffer[USHRT_MAX] = 0;
                  outbuf->real_size = USHRT_MAX;
                  return;
                }
            }
          outbuf->buffer = extend_string (outbuf->buffer, limit);
        }
    }
  else
    {
      outbuf->buffer = new_string (l, "outbuf_add");
      outbuf->real_size = 0;
    }
  strcpy (outbuf->buffer + outbuf->real_size, str);
  outbuf->real_size += l;
}

void
outbuf_addchar (outbuffer_t * outbuf, char c)
{
  int limit;

  if (!outbuf)
    return;

  if (outbuf->buffer)
    {
      limit = MSTR_SIZE (outbuf->buffer);
      if (outbuf->real_size + 1 > limit)
        {
          if (outbuf->real_size == USHRT_MAX)
            return;		/* TRUNCATED */

          /* assume it's going to grow some more */
          limit = (outbuf->real_size + 1) * 2;
          if (limit > USHRT_MAX)
            {
              limit = outbuf->real_size + 1;
              if (limit > USHRT_MAX)
                {
                  outbuf->buffer = extend_string (outbuf->buffer, USHRT_MAX);
                  *(outbuf->buffer + outbuf->real_size) = c;
                  outbuf->buffer[USHRT_MAX] = 0;
                  outbuf->real_size = USHRT_MAX;
                  return;
                }
            }
          outbuf->buffer = extend_string (outbuf->buffer, limit);
        }
    }
  else
    {
      outbuf->buffer = new_string (80, "outbuf_add");
      outbuf->real_size = 0;
    }
  *(outbuf->buffer + outbuf->real_size++) = c;
  *(outbuf->buffer + outbuf->real_size) = 0;
}

void
outbuf_addv (outbuffer_t * outbuf, char *format, ...)
{
  char buf[LARGEST_PRINTABLE_STRING];
  va_list args;

  va_start (args, format);

  vsprintf (buf, format, args);
  va_end (args);

  if (!outbuf)
    return;

  outbuf_add (outbuf, buf);
}

void
outbuf_fix (outbuffer_t * outbuf)
{
  if (outbuf && outbuf->buffer)
    outbuf->buffer = extend_string (outbuf->buffer, outbuf->real_size);
}

void
outbuf_push (outbuffer_t * outbuf)
{
  (++sp)->type = T_STRING;
  if (outbuf && outbuf->buffer)
    {
      outbuf->buffer = extend_string (outbuf->buffer, outbuf->real_size);

      sp->subtype = STRING_MALLOC;
      sp->u.string = outbuf->buffer;
    }
  else
    {
      sp->subtype = STRING_CONSTANT;
      sp->u.string = "";
    }
}

