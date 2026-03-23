/*
        [1992-05-??] by Dwayne Fontenot (Jacques@TMI), original coding.
        [1992-10-??] by Dave Richards (Cynosure), less original coding.

    MODIFIED BY
        [2001-06-27] by Annihilator <annihilator@muds.net>, see CVS log.
 */

#ifdef	HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <assert.h>
#include <time.h>
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#include <sys/types.h>
#include "port/socket_comm.h"

#include "src/std.h"
#include "src/comm.h"
#include "src/addr_resolver.h"
#include "async/async_runtime.h"
#include "lpc/array.h"
#include "lpc/functional.h"
#include "lpc/buffer.h"
#include "lpc/object.h"
#include "src/interpret.h"
#include "lpc/include/runtime_config.h"
#include "lpc/include/origin.h"
#include "rc.h"
#include "socket_efuns.h"
#include "socket_err.h"
#include "lpc/include/socket_err.h"
#include "lpc/operator.h"

#define socket_perror(x,y)	debug_perror(x,y)

#ifdef PACKAGE_SOCKETS

/* flags for socket_close */
#define SC_FORCE	1
#define SC_DO_CALLBACK	2
#define SC_FINAL_CLOSE  4

#define DNS_TIMEOUT_SECONDS 30  /* Max time for DNS resolution */

/* DNS telemetry counters for monitoring and debugging */
typedef struct {
  unsigned long admitted;         /* Total tasks successfully queued */
  unsigned long dedup_hit;        /* Duplicate hostname:port joined existing task */
  unsigned long rejected_global;  /* Rejected due to global cap */
  unsigned long rejected_owner;   /* Rejected due to per-owner cap */
  unsigned long rejected_queue;   /* Rejected due to queue full */
  unsigned long timed_out;        /* Resolutions that exceeded deadline */
  unsigned long completed;        /* Resolutions completed successfully */
  unsigned long failed;           /* Resolutions that failed (no timeout) */
} dns_telemetry_t;

static dns_telemetry_t dns_telemetry = {0};

lpc_socket_t *lpc_socks = 0;
int max_lpc_socks = 0;

typedef struct {
  int active;
  int terminal;
  int op_id;
  int socket_id;
  object_t *owner_ob;
  enum socket_operation_phase phase;
  time_t deadline;
} socket_operation_t;

static socket_operation_t *socket_ops = NULL;
typedef struct {
  int socket_id;
  int magic;
} socket_runtime_context_t;

typedef struct {
  int registered;
  int events;
  socket_fd_t fd;
} socket_runtime_state_t;

#define SOCKET_RUNTIME_CONTEXT_MAGIC 0x534f434b

/* DNS admission control and task queue (Stage 4A) */
#define DNS_GLOBAL_CAP 64          /* Max in-flight DNS resolutions globally */
#define DNS_PER_OWNER_CAP 8        /* Max per-owner pending DNS resolutions */
#define SOCKET_DNS_REQUEST_ID_BASE (RESOLVER_LOOKUP_REQUEST_CAPACITY + 1)

typedef struct {
  struct in_addr resolved_addr;
  uint16_t port;
  int success;
  int timed_out;
} socket_dns_apply_result_t;

typedef char dns_hostname_t[ADDR_BUF_SIZE];

static int dns_tasks_in_flight = 0;      /* Current global count */
static dns_hostname_t *dns_pending_hostnames = NULL;
static uint16_t *dns_pending_ports = NULL;
static int *dns_pending_leaders = NULL;
static int *dns_pending_leader_op_ids = NULL;
static int *dns_pending_request_ids = NULL;
static int next_socket_dns_request_id = SOCKET_DNS_REQUEST_ID_BASE;
static socket_dns_timeout_test_hook_t socket_dns_timeout_test_hook = NULL;

static socket_runtime_context_t **socket_contexts = NULL;
static socket_runtime_state_t *socket_runtime_state = NULL;
static int next_socket_op_id = 1;
static socket_release_test_hook_t socket_release_test_hook = NULL;

static int socket_name_to_sin (char *, struct sockaddr_in *);
static char *inet_address (struct sockaddr_in *);
static const char *lookup_socket_operation_phase_name (enum socket_operation_phase phase);
static int is_socket_operation_terminal_phase (enum socket_operation_phase phase);
static int can_transition_socket_operation_phase (enum socket_operation_phase from, enum socket_operation_phase to);
static void clear_socket_operation (int socket_id);
static int start_socket_operation (int socket_id, object_t *owner_ob, time_t deadline);
static int set_socket_operation_phase (int socket_id, enum socket_operation_phase next_phase);
static int complete_socket_operation (int socket_id, enum socket_operation_phase terminal_phase);
static int compute_socket_runtime_events (int socket_id);

static int init_dns_system(void);
static void clear_dns_pending_resolution(int socket_id);
static int queue_dns_resolution(int socket_id, const char *hostname, uint16_t port);
static void apply_dns_result_to_socket(int socket_id, const socket_dns_apply_result_t *result);
static int register_socket_runtime (int socket_id);
static int modify_socket_runtime (int socket_id);
static void remove_socket_runtime (int socket_id);

void
set_socket_release_test_hook (socket_release_test_hook_t hook)
{
  socket_release_test_hook = hook;
}

void
set_socket_dns_timeout_test_hook (socket_dns_timeout_test_hook_t hook)
{
  socket_dns_timeout_test_hook = hook;
}

static const char *
lookup_socket_operation_phase_name (enum socket_operation_phase phase)
{
  switch (phase)
    {
    case OP_INIT:
      return "INIT";
    case OP_DNS_RESOLVING:
      return "DNS_RESOLVING";
    case OP_CONNECTING:
      return "CONNECTING";
    case OP_TRANSFERRING:
      return "TRANSFERRING";
    case OP_COMPLETED:
      return "COMPLETED";
    case OP_FAILED:
      return "FAILED";
    case OP_TIMED_OUT:
      return "TIMED_OUT";
    case OP_CANCELED:
      return "CANCELED";
    default:
      return "UNKNOWN";
    }
}

static int
is_socket_operation_terminal_phase (enum socket_operation_phase phase)
{
  return phase == OP_COMPLETED || phase == OP_FAILED || phase == OP_TIMED_OUT || phase == OP_CANCELED;
}

static int
can_transition_socket_operation_phase (enum socket_operation_phase from, enum socket_operation_phase to)
{
  if (from == to)
    return 1;

  switch (from)
    {
    case OP_INIT:
      return to == OP_DNS_RESOLVING || to == OP_CONNECTING || is_socket_operation_terminal_phase (to);
    case OP_DNS_RESOLVING:
      return to == OP_CONNECTING || is_socket_operation_terminal_phase (to);
    case OP_CONNECTING:
      return to == OP_TRANSFERRING || is_socket_operation_terminal_phase (to);
    case OP_TRANSFERRING:
      return is_socket_operation_terminal_phase (to);
    case OP_COMPLETED:
    case OP_FAILED:
    case OP_TIMED_OUT:
    case OP_CANCELED:
      return 0;
    default:
      return 0;
    }
}

static void
clear_socket_operation (int socket_id)
{
  if (socket_ops == NULL || socket_id < 0 || socket_id >= max_lpc_socks)
    return;

  clear_dns_pending_resolution (socket_id);

  socket_ops[socket_id].active = 0;
  socket_ops[socket_id].terminal = 0;
  socket_ops[socket_id].op_id = 0;
  socket_ops[socket_id].socket_id = socket_id;
  socket_ops[socket_id].owner_ob = NULL;
  socket_ops[socket_id].phase = OP_INIT;
  socket_ops[socket_id].deadline = 0;
}

static int
start_socket_operation (int socket_id, object_t *owner_ob, time_t deadline)
{
  socket_operation_t *op;

  if (socket_ops == NULL || socket_id < 0 || socket_id >= max_lpc_socks)
    return 0;

  clear_socket_operation (socket_id);
  op = &socket_ops[socket_id];
  op->active = 1;
  op->terminal = 0;
  op->op_id = next_socket_op_id++;
  op->socket_id = socket_id;
  op->owner_ob = owner_ob;
  op->phase = OP_INIT;
  op->deadline = deadline;

  opt_trace (TT_COMM|2, "socket-op[%d] start: fd=%d socket=%d phase=%s deadline=%ld",
             op->op_id,
             (int) lpc_socks[socket_id].fd,
             socket_id,
             lookup_socket_operation_phase_name (op->phase),
             (long) op->deadline);

  return 1;
}

static int
set_socket_operation_phase (int socket_id, enum socket_operation_phase next_phase)
{
  socket_operation_t *op;
  enum socket_operation_phase prev_phase;

  if (socket_ops == NULL || socket_id < 0 || socket_id >= max_lpc_socks)
    return 0;

  op = &socket_ops[socket_id];
  if (!op->active)
    return 0;

  prev_phase = op->phase;
  if (!can_transition_socket_operation_phase (prev_phase, next_phase))
    {
      debug_error ("socket-op[%d] invalid transition: fd=%d socket=%d %s -> %s",
                   op->op_id,
                   (int) lpc_socks[socket_id].fd,
                   socket_id,
                   lookup_socket_operation_phase_name (prev_phase),
                   lookup_socket_operation_phase_name (next_phase));
      return 0;
    }

  op->phase = next_phase;
  opt_trace (TT_COMM|2, "socket-op[%d] phase: fd=%d socket=%d %s -> %s",
             op->op_id,
             (int) lpc_socks[socket_id].fd,
             socket_id,
             lookup_socket_operation_phase_name (prev_phase),
             lookup_socket_operation_phase_name (next_phase));

  return 1;
}

static int
complete_socket_operation (int socket_id, enum socket_operation_phase terminal_phase)
{
  socket_operation_t *op;

  if (!is_socket_operation_terminal_phase (terminal_phase))
    return 0;
  if (socket_ops == NULL || socket_id < 0 || socket_id >= max_lpc_socks)
    return 0;

  op = &socket_ops[socket_id];
  if (!op->active)
    return 0;

  if (op->terminal)
    {
      opt_trace (TT_COMM|1, "socket-op[%d] duplicate terminal completion ignored: fd=%d socket=%d current=%s requested=%s",
                 op->op_id,
                 (int) lpc_socks[socket_id].fd,
                 socket_id,
                 lookup_socket_operation_phase_name (op->phase),
                 lookup_socket_operation_phase_name (terminal_phase));
      return 0;
    }

  if (!set_socket_operation_phase (socket_id, terminal_phase))
    return 0;

  op->terminal = 1;
  opt_trace (TT_COMM|1, "socket-op[%d] terminal: fd=%d socket=%d phase=%s",
             op->op_id,
             (int) lpc_socks[socket_id].fd,
             socket_id,
             lookup_socket_operation_phase_name (op->phase));

  clear_socket_operation (socket_id);
  return 1;
}

/*
 * check permission
 */
int check_valid_socket (char *what, socket_fd_t fd, object_t * owner, char *addr, int port) {

  array_t *info;
  svalue_t *mret;

  info = allocate_empty_array (4);
  info->item[0].type = T_NUMBER;
  info->item[0].u.number = fd;
  assign_socket_owner (&info->item[1], owner);
  info->item[2].type = T_STRING;
  info->item[2].subtype = STRING_SHARED;
  info->item[2].u.string = make_shared_string (addr);
  info->item[3].type = T_NUMBER;
  info->item[3].u.number = port;

  push_object (current_object);
  push_constant_string (what);
  push_refed_array (info);

  mret = apply_master_ob (APPLY_VALID_SOCKET, 3);
  return MASTER_APPROVED (mret);
}

/*
 * Get more LPC sockets structures if we run out
 */
int more_lpc_sockets () {

  int i;

  max_lpc_socks += 10;

  if (!lpc_socks)
    lpc_socks = CALLOCATE (10, lpc_socket_t, TAG_SOCKETS, "more_lpc_sockets");
  else
    lpc_socks = RESIZE (lpc_socks, max_lpc_socks, lpc_socket_t, TAG_SOCKETS, "more_lpc_sockets");

  if (!socket_ops)
    socket_ops = CALLOCATE (10, socket_operation_t, TAG_SOCKETS, "more_lpc_sockets:socket_ops");
  else
    socket_ops = RESIZE (socket_ops, max_lpc_socks, socket_operation_t, TAG_SOCKETS, "more_lpc_sockets:socket_ops");

  if (!socket_contexts)
    socket_contexts = CALLOCATE (max_lpc_socks, socket_runtime_context_t *, TAG_SOCKETS, "more_lpc_sockets:socket_contexts");
  else
    socket_contexts = RESIZE (socket_contexts, max_lpc_socks, socket_runtime_context_t *, TAG_SOCKETS, "more_lpc_sockets:socket_contexts");

  if (!socket_runtime_state)
    socket_runtime_state = CALLOCATE (max_lpc_socks, socket_runtime_state_t, TAG_SOCKETS, "more_lpc_sockets:socket_runtime_state");
  else
    socket_runtime_state = RESIZE (socket_runtime_state, max_lpc_socks, socket_runtime_state_t, TAG_SOCKETS, "more_lpc_sockets:socket_runtime_state");

  if (!dns_pending_hostnames)
    dns_pending_hostnames = CALLOCATE (max_lpc_socks, dns_hostname_t, TAG_SOCKETS, "more_lpc_sockets:dns_pending_hostnames");
  else
    dns_pending_hostnames = RESIZE (dns_pending_hostnames, max_lpc_socks, dns_hostname_t, TAG_SOCKETS, "more_lpc_sockets:dns_pending_hostnames");

  if (!dns_pending_ports)
    dns_pending_ports = CALLOCATE (max_lpc_socks, uint16_t, TAG_SOCKETS, "more_lpc_sockets:dns_pending_ports");
  else
    dns_pending_ports = RESIZE (dns_pending_ports, max_lpc_socks, uint16_t, TAG_SOCKETS, "more_lpc_sockets:dns_pending_ports");

  if (!dns_pending_leaders)
    dns_pending_leaders = CALLOCATE (max_lpc_socks, int, TAG_SOCKETS, "more_lpc_sockets:dns_pending_leaders");
  else
    dns_pending_leaders = RESIZE (dns_pending_leaders, max_lpc_socks, int, TAG_SOCKETS, "more_lpc_sockets:dns_pending_leaders");

  if (!dns_pending_leader_op_ids)
    dns_pending_leader_op_ids = CALLOCATE (max_lpc_socks, int, TAG_SOCKETS, "more_lpc_sockets:dns_pending_leader_op_ids");
  else
    dns_pending_leader_op_ids = RESIZE (dns_pending_leader_op_ids, max_lpc_socks, int, TAG_SOCKETS, "more_lpc_sockets:dns_pending_leader_op_ids");

  if (!dns_pending_request_ids)
    dns_pending_request_ids = CALLOCATE (max_lpc_socks, int, TAG_SOCKETS, "more_lpc_sockets:dns_pending_request_ids");
  else
    dns_pending_request_ids = RESIZE (dns_pending_request_ids, max_lpc_socks, int, TAG_SOCKETS, "more_lpc_sockets:dns_pending_request_ids");

  if (max_lpc_socks == 10)
    {
      /* First time initialization - set up DNS system */
      if (!init_dns_system())
        {
          debug_error("Failed to initialize DNS system for sockets");
        }
    }

  i = max_lpc_socks;
  while (--i >= max_lpc_socks - 10)
    {
      lpc_socks[i].fd = INVALID_SOCKET_FD;
      lpc_socks[i].flags = 0;
      lpc_socks[i].mode = MUD;
      lpc_socks[i].state = CLOSED;
      memset ((char *) &lpc_socks[i].l_addr, 0, sizeof (lpc_socks[i].l_addr));
      memset ((char *) &lpc_socks[i].r_addr, 0, sizeof (lpc_socks[i].r_addr));
      lpc_socks[i].name[0] = '\0';
      lpc_socks[i].owner_ob = NULL;
      lpc_socks[i].release_ob = NULL;
      lpc_socks[i].read_callback.s = 0;
      lpc_socks[i].write_callback.s = 0;
      lpc_socks[i].close_callback.s = 0;
      lpc_socks[i].r_buf = NULL;
      lpc_socks[i].r_off = 0;
      lpc_socks[i].r_len = 0;
      lpc_socks[i].w_buf = NULL;
      lpc_socks[i].w_off = 0;
      lpc_socks[i].w_len = 0;

      socket_ops[i].active = 0;
      socket_ops[i].terminal = 0;
      socket_ops[i].op_id = 0;
      socket_ops[i].socket_id = i;
      socket_ops[i].owner_ob = NULL;
      socket_ops[i].phase = OP_INIT;
      socket_ops[i].deadline = 0;

      if (socket_contexts[i] == NULL)
        {
          socket_contexts[i] = CALLOCATE (1, socket_runtime_context_t, TAG_SOCKETS, "more_lpc_sockets:socket_context");
          socket_contexts[i]->socket_id = i;
          socket_contexts[i]->magic = SOCKET_RUNTIME_CONTEXT_MAGIC;
        }

      socket_runtime_state[i].registered = 0;
      socket_runtime_state[i].events = 0;
      socket_runtime_state[i].fd = INVALID_SOCKET_FD;

      if (dns_pending_hostnames != NULL)
        dns_pending_hostnames[i][0] = '\0';
      if (dns_pending_ports != NULL)
        dns_pending_ports[i] = 0;
      if (dns_pending_leaders != NULL)
        dns_pending_leaders[i] = -1;
      if (dns_pending_leader_op_ids != NULL)
        dns_pending_leader_op_ids[i] = -1;
      if (dns_pending_request_ids != NULL)
        dns_pending_request_ids[i] = 0;
    }
  return max_lpc_socks - 10;
}

static int
compute_socket_runtime_events (int socket_id)
{
  int events = EVENT_READ;

  if (socket_id < 0 || socket_id >= max_lpc_socks)
    return 0;

  if (lpc_socks[socket_id].state == CLOSED)
    return 0;

  if (lpc_socks[socket_id].flags & S_BLOCKED)
    events |= EVENT_WRITE;

  return events;
}

static int
register_socket_runtime (int socket_id)
{
  async_runtime_t *runtime;
  int events;

  if (socket_runtime_state == NULL || socket_contexts == NULL)
    return 0;
  if (socket_id < 0 || socket_id >= max_lpc_socks)
    return 0;

  runtime = get_async_runtime ();
  if (!runtime)
    return 1;

  events = compute_socket_runtime_events (socket_id);
  if (events == 0)
    return 0;

  if (socket_runtime_state[socket_id].registered)
    {
      if (socket_runtime_state[socket_id].fd != lpc_socks[socket_id].fd)
        {
          opt_trace (TT_COMM|1, "socket-runtime stale registration replaced: socket=%d old_fd=%d new_fd=%d",
                     socket_id,
                     (int) socket_runtime_state[socket_id].fd,
                     (int) lpc_socks[socket_id].fd);
          async_runtime_remove (runtime, socket_runtime_state[socket_id].fd);
          socket_runtime_state[socket_id].registered = 0;
          socket_runtime_state[socket_id].events = 0;
          socket_runtime_state[socket_id].fd = INVALID_SOCKET_FD;
        }
      else
        {
          return modify_socket_runtime (socket_id);
        }
    }

  if (async_runtime_add (runtime, lpc_socks[socket_id].fd, events, socket_contexts[socket_id]) != 0)
    {
      debug_error ("socket-runtime add failed: socket=%d fd=%d", socket_id, (int) lpc_socks[socket_id].fd);
      return 0;
    }

  socket_runtime_state[socket_id].registered = 1;
  socket_runtime_state[socket_id].events = events;
  socket_runtime_state[socket_id].fd = lpc_socks[socket_id].fd;
  opt_trace (TT_COMM|2, "socket-runtime add: socket=%d fd=%d events=%d", socket_id,
             (int) socket_runtime_state[socket_id].fd, socket_runtime_state[socket_id].events);

  return 1;
}

static int
modify_socket_runtime (int socket_id)
{
  async_runtime_t *runtime;
  int events;

  if (socket_runtime_state == NULL || socket_contexts == NULL)
    return 0;
  if (socket_id < 0 || socket_id >= max_lpc_socks)
    return 0;

  runtime = get_async_runtime ();
  if (!runtime)
    return 1;

  if (!socket_runtime_state[socket_id].registered)
    return register_socket_runtime (socket_id);

  if (socket_runtime_state[socket_id].fd != lpc_socks[socket_id].fd)
    return register_socket_runtime (socket_id);

  events = compute_socket_runtime_events (socket_id);
  if (events == 0)
    {
      remove_socket_runtime (socket_id);
      return 1;
    }

  if (events == socket_runtime_state[socket_id].events)
    return 1;

  if (async_runtime_modify (runtime, lpc_socks[socket_id].fd, events, socket_contexts[socket_id]) != 0)
    {
      debug_error ("socket-runtime modify failed: socket=%d fd=%d events=%d",
                   socket_id, (int) lpc_socks[socket_id].fd, events);
      return 0;
    }

  socket_runtime_state[socket_id].events = events;
  opt_trace (TT_COMM|2, "socket-runtime modify: socket=%d fd=%d events=%d", socket_id,
             (int) socket_runtime_state[socket_id].fd, socket_runtime_state[socket_id].events);
  return 1;
}

static void
remove_socket_runtime (int socket_id)
{
  async_runtime_t *runtime;

  if (socket_runtime_state == NULL)
    return;
  if (socket_id < 0 || socket_id >= max_lpc_socks)
    return;

  runtime = get_async_runtime ();
  if (runtime && socket_runtime_state[socket_id].registered && socket_runtime_state[socket_id].fd != INVALID_SOCKET_FD)
    {
      if (async_runtime_remove (runtime, socket_runtime_state[socket_id].fd) != 0)
        {
          opt_trace (TT_COMM|1, "socket-runtime remove failed: socket=%d fd=%d",
                     socket_id, (int) socket_runtime_state[socket_id].fd);
        }
      else
        {
          opt_trace (TT_COMM|2, "socket-runtime remove: socket=%d fd=%d",
                     socket_id, (int) socket_runtime_state[socket_id].fd);
        }
    }

  socket_runtime_state[socket_id].registered = 0;
  socket_runtime_state[socket_id].events = 0;
  socket_runtime_state[socket_id].fd = INVALID_SOCKET_FD;
}

/*
 * Set the callbacks for a socket
 */
void set_read_callback (int which, svalue_t * cb) {

  char *s;

  if (lpc_socks[which].flags & S_READ_FP)
    {
      free_funp (lpc_socks[which].read_callback.f);
      lpc_socks[which].flags &= ~S_READ_FP;
    }
  else if ((s = lpc_socks[which].read_callback.s))
    free_string (s);

  if (cb)
    {
      if (cb->type == T_FUNCTION)
        {
          lpc_socks[which].flags |= S_READ_FP;
          lpc_socks[which].read_callback.f = cb->u.fp;
          cb->u.fp->hdr.ref++;
        }
      else
        {
          lpc_socks[which].read_callback.s =
            make_shared_string (cb->u.string);
        }
    }
  else
    lpc_socks[which].read_callback.s = 0;
}

void set_write_callback (int which, svalue_t * cb) {

  char *s;

  if (lpc_socks[which].flags & S_WRITE_FP)
    {
      free_funp (lpc_socks[which].write_callback.f);
      lpc_socks[which].flags &= ~S_WRITE_FP;
    }
  else if ((s = lpc_socks[which].write_callback.s))
    free_string (s);

  if (cb)
    {
      if (cb->type == T_FUNCTION)
        {
          lpc_socks[which].flags |= S_WRITE_FP;
          lpc_socks[which].write_callback.f = cb->u.fp;
          cb->u.fp->hdr.ref++;
        }
      else
        {
          lpc_socks[which].write_callback.s =
            make_shared_string (cb->u.string);
        }
    }
  else
    lpc_socks[which].write_callback.s = 0;
}

void set_close_callback (int which, svalue_t * cb) {

  char *s;

  if (lpc_socks[which].flags & S_CLOSE_FP)
    {
      free_funp (lpc_socks[which].close_callback.f);
      lpc_socks[which].flags &= ~S_CLOSE_FP;
    }
  else if ((s = lpc_socks[which].close_callback.s))
    free_string (s);

  if (cb)
    {
      if (cb->type == T_FUNCTION)
        {
          lpc_socks[which].flags |= S_CLOSE_FP;
          lpc_socks[which].close_callback.f = cb->u.fp;
          cb->u.fp->hdr.ref++;
        }
      else
        {
          lpc_socks[which].close_callback.s =
            make_shared_string (cb->u.string);
        }
    }
  else
    lpc_socks[which].close_callback.s = 0;
}

static void copy_close_callback (int to, int from) {

  char *s;

  if (lpc_socks[to].flags & S_CLOSE_FP)
    {
      free_funp (lpc_socks[to].close_callback.f);
    }
  else if ((s = lpc_socks[to].close_callback.s))
    free_string (s);

  if (lpc_socks[from].flags & S_CLOSE_FP)
    {
      lpc_socks[to].flags |= S_CLOSE_FP;
      lpc_socks[to].close_callback.f = lpc_socks[from].close_callback.f;
      lpc_socks[to].close_callback.f->hdr.ref++;
    }
  else
    {
      lpc_socks[to].flags &= ~S_CLOSE_FP;
      s = lpc_socks[to].close_callback.s = lpc_socks[from].close_callback.s;
      if (s)
        ref_string (s);
    }
}

int find_new_socket (void) {

  int i;

  for (i = 0; i < max_lpc_socks; i++)
    {
      if (lpc_socks[i].state != CLOSED)
        continue;
      remove_socket_runtime (i);
      set_read_callback (i, 0);
      set_write_callback (i, 0);
      set_close_callback (i, 0);
      clear_socket_operation (i);
      return i;
    }
  return more_lpc_sockets ();
}

/*
 * Create an LPC efun socket
 */
int socket_create (enum socket_mode mode, svalue_t * read_callback, svalue_t * close_callback) {

  socket_fd_t fd;
  int type, i, optval;
  int binary = 0;

  if (mode == STREAM_BINARY)
    {
      binary = 1;
      mode = STREAM;
    }
  else if (mode == DATAGRAM_BINARY)
    {
      binary = 1;
      mode = DATAGRAM;
    }
  switch (mode)
    {
    case MUD:
    case STREAM:
      type = SOCK_STREAM;
      break;
    case DATAGRAM:
      type = SOCK_DGRAM;
      break;
    default:
      return EEMODENOTSUPP;
    }

  i = find_new_socket ();
  if (i >= 0)
    {
      fd = socket (AF_INET, type, 0);
      if (fd == INVALID_SOCKET_FD)
        {
          debug_error ("socket() failed: %d", SOCKET_ERRNO);
          return EESOCKET;
        }
      optval = 1;
      if (setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, (char *) &optval, sizeof (optval)) == SOCKET_ERROR)
        {
          debug_error ("setsockopt() failed: %d", SOCKET_ERRNO);
          SOCKET_CLOSE (fd);
          return EESETSOCKOPT;
        }
      if (set_socket_nonblocking (fd, 1) == SOCKET_ERROR)
        {
          debug_error ("set_socket_nonblocking() failed: %d", SOCKET_ERRNO);
          SOCKET_CLOSE (fd);
          return EENONBLOCK;
        }
      lpc_socks[i].fd = fd;
      lpc_socks[i].flags = S_HEADER;

      if (type == SOCK_DGRAM)
        close_callback = 0;
      set_read_callback (i, read_callback);
      set_write_callback (i, 0);
      set_close_callback (i, close_callback);

      if (binary)
        {
          lpc_socks[i].flags |= S_BINARY;
        }
      lpc_socks[i].mode = mode;
      lpc_socks[i].state = UNBOUND;
      memset ((char *) &lpc_socks[i].l_addr, 0, sizeof (lpc_socks[i].l_addr));
      memset ((char *) &lpc_socks[i].r_addr, 0, sizeof (lpc_socks[i].r_addr));
      lpc_socks[i].name[0] = '\0';
      lpc_socks[i].owner_ob = current_object;
      lpc_socks[i].release_ob = NULL;
      lpc_socks[i].r_buf = NULL;
      lpc_socks[i].r_off = 0;
      lpc_socks[i].r_len = 0;
      lpc_socks[i].w_buf = NULL;
      lpc_socks[i].w_off = 0;
      lpc_socks[i].w_len = 0;

      if (!register_socket_runtime (i))
        {
          set_read_callback (i, 0);
          set_write_callback (i, 0);
          set_close_callback (i, 0);
          SOCKET_CLOSE (fd);
          lpc_socks[i].fd = INVALID_SOCKET_FD;
          lpc_socks[i].state = CLOSED;
          lpc_socks[i].owner_ob = NULL;
          lpc_socks[i].release_ob = NULL;
          return EESOCKET;
        }

      current_object->flags |= O_EFUN_SOCKET;
    }

  return i;
}

/*
 * Bind an address to an LPC efun socket
 */
int socket_bind (int i, int port) {

  socklen_t len;
  struct sockaddr_in sin;

  if (i < 0 || i >= max_lpc_socks)
    return EEFDRANGE;
  if (lpc_socks[i].state == CLOSED || lpc_socks[i].state == FLUSHING)
    return EEBADF;
  if (lpc_socks[i].owner_ob != current_object)
    return EESECURITY;
  if (lpc_socks[i].state != UNBOUND)
    return EEISBOUND;

  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons ((u_short) port);

  if (bind (lpc_socks[i].fd, (struct sockaddr *) &sin, sizeof (sin)) == SOCKET_ERROR)
    {
      switch (SOCKET_ERRNO)
        {
#ifdef WINSOCK
        case WSAEADDRINUSE:
          return EEADDRINUSE;
#else
        case EADDRINUSE:
          return EEADDRINUSE;
#endif
        default:
          socket_perror ("socket_bind: bind", 0);
          return EEBIND;
        }
    }
  len = sizeof (sin);
  if (getsockname (lpc_socks[i].fd, (struct sockaddr *) &lpc_socks[i].l_addr, &len) == SOCKET_ERROR)
    {
      socket_perror ("socket_bind: getsockname", 0);
      return EEGETSOCKNAME;
    }
  lpc_socks[i].state = BOUND;

  return EESUCCESS;
}

/*
 * Listen for connections on an LPC efun socket
 */
int socket_listen (int i, svalue_t * callback) {

  if (i < 0 || i >= max_lpc_socks)
    return EEFDRANGE;
  if (lpc_socks[i].state == CLOSED || lpc_socks[i].state == FLUSHING)
    return EEBADF;
  if (lpc_socks[i].owner_ob != current_object)
    return EESECURITY;
  if (lpc_socks[i].mode == DATAGRAM)
    return EEMODENOTSUPP;
  if (lpc_socks[i].state == UNBOUND)
    return EENOADDR;
  if (lpc_socks[i].state != BOUND)
    return EEISCONN;

  if (listen (lpc_socks[i].fd, 5) == SOCKET_ERROR)
    {
      socket_perror ("socket_listen: listen", 0);
      return EELISTEN;
    }
  lpc_socks[i].state = LISTEN;
  set_read_callback (i, callback);

  current_object->flags |= O_EFUN_SOCKET;

  return EESUCCESS;
}

/**
 * Accept a connection on an LPC efun socket
 */
int socket_accept (int s, svalue_t * read_callback, svalue_t * write_callback) {

  socklen_t len;
  socket_fd_t accept_fd;
  int i;
  struct sockaddr_in sin;
#if 0
  struct hostent *hp;
#endif

  if (s < 0 || s >= max_lpc_socks)
    return EEFDRANGE;
  if (lpc_socks[s].state == CLOSED || lpc_socks[s].state == FLUSHING)
    return EEBADF;
  if (lpc_socks[s].owner_ob != current_object)
    return EESECURITY;
  if (lpc_socks[s].mode == DATAGRAM)
    return EEMODENOTSUPP;
  if (lpc_socks[s].state != LISTEN)
    return EENOTLISTN;

  lpc_socks[s].flags &= ~S_WACCEPT;

  len = sizeof (sin);
  accept_fd = accept (lpc_socks[s].fd, (struct sockaddr *) &sin, &len);
  if (accept_fd == INVALID_SOCKET_FD)
    {
      switch (SOCKET_ERRNO)
        {
        case EWOULDBLOCK:
          return EEWOULDBLOCK;
        case EINTR:
          return EEINTR;
        default:
          socket_perror ("socket_accept: accept", 0);
          return EEACCEPT;
        }
    }
  i = find_new_socket ();
  if (i >= 0)
    {
      fd_set wmask;
      struct timeval t;
      int nb;

      lpc_socks[i].fd = accept_fd;
      lpc_socks[i].flags = S_HEADER | (lpc_socks[s].flags & S_BINARY);

      FD_ZERO (&wmask);
      FD_SET (accept_fd, &wmask);
      t.tv_sec = 0;
      t.tv_usec = 0;
      nb = select (FD_SETSIZE, (fd_set *) 0, &wmask, (fd_set *) 0, &t); /* returns immediately */
      if ((nb < 0) || !(FD_ISSET (accept_fd, &wmask)))
        lpc_socks[i].flags |= S_BLOCKED;

      lpc_socks[i].mode = lpc_socks[s].mode;
      lpc_socks[i].state = DATA_XFER;
      lpc_socks[i].l_addr = lpc_socks[s].l_addr;
      lpc_socks[i].r_addr = sin;
      lpc_socks[i].owner_ob = NULL;
      lpc_socks[i].release_ob = NULL;
      lpc_socks[i].r_buf = NULL;
      lpc_socks[i].r_off = 0;
      lpc_socks[i].r_len = 0;
      lpc_socks[i].w_buf = NULL;
      lpc_socks[i].w_off = 0;
      lpc_socks[i].w_len = 0;

      /* FIXME: name resolution should be optional, to prevent DDoS attack */
#if 0
      hp = gethostbyaddr ((char *) &sin.sin_addr.s_addr, (int) sizeof (sin.sin_addr.s_addr), AF_INET);
      if (hp != NULL)
        {
          strncpy (lpc_socks[i].name, hp->h_name, ADDR_BUF_SIZE);
          lpc_socks[i].name[ADDR_BUF_SIZE - 1] = '\0';
        }
      else
        lpc_socks[i].name[0] = '\0';
#endif

      lpc_socks[i].owner_ob = current_object;
      set_read_callback (i, read_callback);
      set_write_callback (i, write_callback);
      copy_close_callback (i, s);

      if (!register_socket_runtime (i))
        {
          set_read_callback (i, 0);
          set_write_callback (i, 0);
          set_close_callback (i, 0);
          SOCKET_CLOSE (accept_fd);
          lpc_socks[i].fd = INVALID_SOCKET_FD;
          lpc_socks[i].state = CLOSED;
          lpc_socks[i].owner_ob = NULL;
          lpc_socks[i].release_ob = NULL;
          return EESOCKET;
        }

      current_object->flags |= O_EFUN_SOCKET;
    }
  else
    SOCKET_CLOSE (accept_fd);

  return i;
}

/*
 * Connect an LPC efun socket
 */
int socket_connect (int i, char *name, svalue_t * read_callback, svalue_t * write_callback)
{
  if (i < 0 || i >= max_lpc_socks)
    return EEFDRANGE;
  if (lpc_socks[i].state == CLOSED || lpc_socks[i].state == FLUSHING)
    return EEBADF;
  if (lpc_socks[i].owner_ob != current_object)
    return EESECURITY;
  if (lpc_socks[i].mode == DATAGRAM)
    return EEMODENOTSUPP;
  switch (lpc_socks[i].state)
    {
    case CLOSED:
    case FLUSHING:
    case UNBOUND:
    case BOUND:
      break;
    case LISTEN:
      return EEISLISTEN;
    case DATA_XFER:
      return EEISCONN;
    }

  {
    int is_hostname = 0;
    struct in_addr test_addr;
    long port_num = 0;
    char addr[ADDR_BUF_SIZE];
    char *cp, *port_end;

    /* Check if the address looks like a hostname (non-numeric IP) by attempting
     * to extract and parse the address component as IPv4. Also extract port. */
    if (name != NULL)
      {
        strncpy (addr, name, ADDR_BUF_SIZE);
        addr[ADDR_BUF_SIZE - 1] = '\0';

        cp = strchr (addr, ' ');
        if (cp != NULL)
          {
            *cp = '\0';
            /* Try to parse port */
            port_num = strtol (cp + 1, &port_end, 10);
            if (port_end != (cp + 1) && *port_end == '\0' && port_num >= 0 && port_num <= 65535)
              {
                /* Valid port - now check if address is hostname */
                if (inet_pton (AF_INET, addr, &test_addr) != 1)
                  is_hostname = 1;
              }
          }
      }

    /* If we detected a hostname and feature is enabled, queue DNS work asynchronously */
    if (is_hostname)
      {
        if (!start_socket_operation (i, current_object, 0))
          return EESOCKET;
        if (!set_socket_operation_phase (i, OP_DNS_RESOLVING))
          {
            complete_socket_operation (i, OP_FAILED);
            return EESOCKET;
          }

        /* Queue the DNS resolution to worker thread. This queues the work but
         * doesn't block. The socket stays in OP_DNS_RESOLVING until completion. */
        {
          int dns_queue_result = queue_dns_resolution (i, addr, (uint16_t)port_num);

          if (dns_queue_result != EESUCCESS)
          {
            /* Queue failed - admission control rejected or queue full */
            complete_socket_operation (i, OP_FAILED);
            return dns_queue_result;
          }
        }

        set_read_callback (i, read_callback);
        set_write_callback (i, write_callback);
        current_object->flags |= O_EFUN_SOCKET;

        /* Return EESUCCESS - work is queued, completion will transition to
         * OP_CONNECTING when DNS resolves */
        return EESUCCESS;
      }
  }

  /* Non-hostname path: resolve address synchronously (numeric IPv4 only) */
  if (!socket_name_to_sin (name, &lpc_socks[i].r_addr))
    return EEBADADDR;

  set_read_callback (i, read_callback);
  set_write_callback (i, write_callback);

  if (!start_socket_operation (i, current_object, 0))
    return EESOCKET;
  if (!set_socket_operation_phase (i, OP_CONNECTING))
    {
      complete_socket_operation (i, OP_FAILED);
      return EESOCKET;
    }

  current_object->flags |= O_EFUN_SOCKET;

  if (connect (lpc_socks[i].fd, (struct sockaddr *) &lpc_socks[i].r_addr, sizeof (struct sockaddr_in)) == SOCKET_ERROR)
    {
      switch (SOCKET_ERRNO)
        {
        case EINTR:
          complete_socket_operation (i, OP_FAILED);
          return EEINTR;
#ifdef WINSOCK
        case WSAEADDRINUSE:
          complete_socket_operation (i, OP_FAILED);
          return EEADDRINUSE;
        case WSAEALREADY:
          complete_socket_operation (i, OP_FAILED);
          return EEALREADY;
        case WSAECONNREFUSED:
          complete_socket_operation (i, OP_FAILED);
          return EECONNREFUSED;
        case WSAEINPROGRESS:
          break;
        case WSAEWOULDBLOCK:
          break;
#else
        case EADDRINUSE:
          complete_socket_operation (i, OP_FAILED);
          return EEADDRINUSE;
        case EALREADY:
          complete_socket_operation (i, OP_FAILED);
          return EEALREADY;
        case ECONNREFUSED:
          complete_socket_operation (i, OP_FAILED);
          return EECONNREFUSED;
        case EINPROGRESS:
          break;
#endif
        default:
          debug_error ("connect() failed: %d", SOCKET_ERRNO);
          complete_socket_operation (i, OP_FAILED);
          return EECONNECT;
        }
    }
  lpc_socks[i].state = DATA_XFER;
  lpc_socks[i].flags |= S_BLOCKED;
  set_socket_operation_phase (i, OP_TRANSFERRING);

  if (!modify_socket_runtime (i))
    {
      socket_close (i, SC_FORCE);
      complete_socket_operation (i, OP_FAILED);
      return EESOCKET;
    }

  return EESUCCESS;
}

/**
 * Write a message on an LPC efun socket
 * @param i the socket index
 * @param message the message to send
 * @param name the address to send to (for datagram sockets)
 * @return error code
 */
int socket_write (int i, svalue_t * message, char *name) {

  size_t len;
  int off;
  char *buf, *p;
  struct sockaddr_in sin;

  if (i < 0 || i >= max_lpc_socks)
    return EEFDRANGE;
  if (lpc_socks[i].state == CLOSED || lpc_socks[i].state == FLUSHING)
    return EEBADF;
  if (lpc_socks[i].owner_ob != current_object)
    return EESECURITY;
  if (lpc_socks[i].mode == DATAGRAM)
    {
      if (name == NULL)
        return EENOADDR;
      if (!socket_name_to_sin (name, &sin))
        return EEBADADDR;
    }
  else
    {
      if (lpc_socks[i].state != DATA_XFER)
        return EENOTCONN;
      if (name != NULL)
        return EEBADADDR;
      if (lpc_socks[i].flags & S_BLOCKED)
        return EEALREADY;
    }

  switch (lpc_socks[i].mode)
    {
    case MUD:
      switch (message->type)
        {
        case T_OBJECT:
          return EETYPENOTSUPP;

        default:
          save_svalue_depth = 0;
          len = svalue_save_size (message);
          if (save_svalue_depth > MAX_SAVE_SVALUE_DEPTH)
            {
              return EEBADDATA;
            }
          buf = (char *)
            DMALLOC (len + 5, TAG_TEMPORARY, "socket_write: default");
          if (buf == NULL)
            fatal ("Out of memory");
          *(INT_32 *) buf = htonl ((long) len);
          len += 4;
          buf[4] = '\0';
          p = buf + 4;
          save_svalue (message, &p);
          break;
        }
      break;

    case STREAM:
      switch (message->type)
        {
        case T_BUFFER:
          len = message->u.buf->size;
          buf = (char *) DMALLOC (len, TAG_TEMPORARY, "socket_write: T_BUFFER");
          if (buf == NULL)
            fatal ("Out of memory");
          memcpy (buf, message->u.buf->item, len);
          break;
        case T_STRING:
          len = SVALUE_STRLEN (message);
          buf = (char *) DMALLOC (len + 1, TAG_TEMPORARY, "socket_write: T_STRING");
          if (buf == NULL)
            fatal ("Out of memory");
          strcpy (buf, message->u.string);
          break;
        case T_ARRAY:
          {
            size_t n, limit;
            svalue_t *el;

            assert(sizeof(int64_t) == sizeof(double));
            len = message->u.arr->size * sizeof (int64_t);
            buf = (char *) DMALLOC (len + 1, TAG_TEMPORARY, "socket_write: T_ARRAY");
            if (buf == NULL)
              fatal ("Out of memory");
            el = message->u.arr->item;
            limit = len / sizeof (int64_t);
            for (n = 0; n < limit; n++)
              {
                switch (el[n].type)
                  {
                  case T_NUMBER:
                    memcpy ((char *) &buf[n * sizeof (int64_t)], (char *) &el[n].u.number, sizeof (int64_t));
                    break;
                  case T_REAL:
                    memcpy ((char *) &buf[n * sizeof (double)], (char *) &el[n].u.real, sizeof (double));
                    break;
                  default:
                    break;
                  }
              }
            break;
          }
        default:
          return EETYPENOTSUPP;
        }
      break;

    case DATAGRAM:
      switch (message->type)
        {
        case T_STRING:
          if (sendto (lpc_socks[i].fd, (char *) message->u.string,
                      (int)strlen (message->u.string) + 1, 0,
                      (struct sockaddr *) &sin, sizeof (sin)) == -1)
            {
              debug_error ("sendto() failed: %d", SOCKET_ERRNO);
              return EESENDTO;
            }
          return EESUCCESS;


        case T_BUFFER:
          if (sendto (lpc_socks[i].fd, (char *) message->u.buf->item,
                      message->u.buf->size, 0,
                      (struct sockaddr *) &sin, sizeof (sin)) == -1)
            {
              debug_error ("sendto() failed: %d", SOCKET_ERRNO);
              return EESENDTO;
            }
          return EESUCCESS;

        default:
          return EETYPENOTSUPP;
        }

    default:
      return EEMODENOTSUPP;
    }

  off = SOCKET_SEND (lpc_socks[i].fd, buf, len, 0);
  if (off == -1)
    {
      FREE (buf);
      switch (SOCKET_ERRNO)
        {
#ifdef WINSOCK
        case WSAEWOULDBLOCK:
          return EEWOULDBLOCK;
#else
        case EWOULDBLOCK:
          return EEWOULDBLOCK;
#endif

        default:
          debug_error ("send() failed: %d", SOCKET_ERRNO);
          return EESEND;
        }
    }
  if (off < (int)len)
    {
      lpc_socks[i].flags |= S_BLOCKED;
      lpc_socks[i].w_buf = buf;
      lpc_socks[i].w_off = off;
      lpc_socks[i].w_len = (int)(len - off);

      if (!modify_socket_runtime (i))
        {
          socket_close (i, SC_FORCE | SC_DO_CALLBACK);
          return EESOCKET;
        }

      return EECALLBACK;
    }
  FREE (buf);

  return EESUCCESS;
}

static void
call_callback (int i, int what, int num_arg)
{
  string_or_func_t callback;

  switch (what)
    {
    case S_READ_FP:
      callback = lpc_socks[i].read_callback;
      break;
    case S_WRITE_FP:
      callback = lpc_socks[i].write_callback;
      break;
    case S_CLOSE_FP:
      callback = lpc_socks[i].close_callback;
      break;
    default:
      return;
    }

  if (lpc_socks[i].flags & what)
    {
      safe_call_function_pointer (callback.f, num_arg);
    }
  else if (callback.s)
    {
      if (callback.s[0] == APPLY___INIT_SPECIAL_CHAR)
        error ("Illegal function name.\n");
      safe_apply (callback.s, lpc_socks[i].owner_ob, num_arg, ORIGIN_DRIVER);
    }
}

/*
 * Handle LPC efun socket read select events
 */
void
socket_read_select_handler (int i)
{
  socklen_t addrlen;
  int cc = 0;
  char buf[BUF_SIZE], addr[ADDR_BUF_SIZE];
  svalue_t value;
  struct sockaddr_in sin;

  switch (lpc_socks[i].state)
    {

    case CLOSED:
    case FLUSHING:
      return;

    case UNBOUND:
      debug_message ("socket_read_select_handler: read on unbound socket %i\n", i);
      break;

    case BOUND:
      switch (lpc_socks[i].mode)
        {

        case MUD:
        case STREAM:
          break;

        case DATAGRAM:
          addrlen = sizeof (sin);
          cc = recvfrom (lpc_socks[i].fd, buf, sizeof (buf) - 1, 0, (struct sockaddr *) &sin, &addrlen);
          if (cc <= 0)
            break;
          buf[cc] = '\0';
          inet_ntop (AF_INET, &sin.sin_addr, addr, ADDR_BUF_SIZE);
          snprintf (addr + strlen(addr), sizeof(addr) - strlen(addr), " %d", (int) ntohs (sin.sin_port));
          push_number (i);
          if (lpc_socks[i].flags & S_BINARY)
            {
              buffer_t *b;

              b = allocate_buffer (cc);
              if (b)
                {
                  memcpy (b->item, buf, cc);
                  push_refed_buffer (b);
                }
              else
                {
                  push_number (0);
                }
            }
          else
            {
              copy_and_push_string (buf);
            }
          copy_and_push_string (addr);
          call_callback (i, S_READ_FP, 3);
          return;
        case STREAM_BINARY:
        case DATAGRAM_BINARY:
          break;
        }
      break;

    case LISTEN:
      lpc_socks[i].flags |= S_WACCEPT;
      push_number (i);
      call_callback (i, S_READ_FP, 1);
      return;

    case DATA_XFER:
      switch (lpc_socks[i].mode)
        {

        case DATAGRAM:
          break;

        case MUD:
          if (lpc_socks[i].flags & S_HEADER)
            {
              cc = recv (lpc_socks[i].fd, (char *) &lpc_socks[i].r_len + lpc_socks[i].r_off, 4 - lpc_socks[i].r_off, 0);
              if (cc <= 0)
                break;
              lpc_socks[i].r_off += cc;
              if (lpc_socks[i].r_off != 4)
                return;
              lpc_socks[i].flags &= ~S_HEADER;
              lpc_socks[i].r_off = 0;
              lpc_socks[i].r_len = ntohl (lpc_socks[i].r_len);
              if (lpc_socks[i].r_len <= 0 || lpc_socks[i].r_len > CONFIG_INT (__MAX_BYTE_TRANSFER__))
                break;
              lpc_socks[i].r_buf = (char *)DMALLOC (lpc_socks[i].r_len + 1, TAG_TEMPORARY, "socket_read_select_handler");
              if (lpc_socks[i].r_buf == NULL)
                fatal ("Out of memory");
            }
          if (lpc_socks[i].r_off < lpc_socks[i].r_len)
            {
              cc = SOCKET_RECV (lpc_socks[i].fd, lpc_socks[i].r_buf + lpc_socks[i].r_off, lpc_socks[i].r_len - lpc_socks[i].r_off, 0);
              if (cc <= 0)
                break;
              lpc_socks[i].r_off += cc;
              if (lpc_socks[i].r_off != lpc_socks[i].r_len)
                return;
            }
          lpc_socks[i].r_buf[lpc_socks[i].r_len] = '\0';
          value = const0;
          push_number (i);
          if (restore_svalue (lpc_socks[i].r_buf, &value) == 0)
            *(++sp) = value;
          else
            push_undefined ();
          FREE (lpc_socks[i].r_buf);
          lpc_socks[i].flags |= S_HEADER;
          lpc_socks[i].r_buf = NULL;
          lpc_socks[i].r_off = 0;
          lpc_socks[i].r_len = 0;
          call_callback (i, S_READ_FP, 2);
          return;

        case STREAM:
          cc = SOCKET_RECV (lpc_socks[i].fd, buf, sizeof (buf) - 1, 0);
          if (cc <= 0)
            break;
          buf[cc] = '\0';
          push_number (i);
          if (lpc_socks[i].flags & S_BINARY)
            {
              buffer_t *b;

              b = allocate_buffer (cc);
              if (b)
                {
                  b->ref--;
                  memcpy (b->item, buf, cc);
                  push_buffer (b);
                }
              else
                {
                  push_number (0);
                }
            }
          else
            {
              copy_and_push_string (buf);
            }
          call_callback (i, S_READ_FP, 2);
          return;
        case STREAM_BINARY:
        case DATAGRAM_BINARY:
          break;
        }
      break;
    }
  if (cc == -1)
    {
      switch (SOCKET_ERRNO)
        {
#ifdef WINSOCK
        case WSAECONNREFUSED:
#else
        case ECONNREFUSED:
#endif
          /* Evidentally, on Linux 1.2.1, ECONNREFUSED gets returned
           * if an ICMP_PORT_UNREACHED error happens internally.  Why
           * they use this error message, I have no idea, but this seems
           * to work.
           */
          if (lpc_socks[i].state == BOUND && lpc_socks[i].mode == DATAGRAM)
            return;
          break;
        case EINTR:
#ifdef WINSOCK
        case WSAEWOULDBLOCK:
#else
        case EWOULDBLOCK:
#endif
          return;
        default:
          break;
        }
    }
  socket_close (i, SC_FORCE | SC_DO_CALLBACK);
}

/*
 * Handle LPC efun socket write select events
 */
void socket_write_select_handler (int i) {
  int cc;

  if ((lpc_socks[i].flags & S_BLOCKED) == 0)
    return;

  if (lpc_socks[i].w_buf != NULL)
    {
      cc = SOCKET_SEND (lpc_socks[i].fd, lpc_socks[i].w_buf + lpc_socks[i].w_off, lpc_socks[i].w_len, 0);
      if (cc == -1)
        {
          if (lpc_socks[i].state == FLUSHING && errno != EINTR)
            {
              /* give up on errors writing to closing sockets */
              lpc_socks[i].flags &= ~S_BLOCKED;
              socket_close (i, SC_FORCE | SC_FINAL_CLOSE);
            }
          return;
        }
      lpc_socks[i].w_off += cc;
      lpc_socks[i].w_len -= cc;
      if (lpc_socks[i].w_len != 0)
        return;
      FREE (lpc_socks[i].w_buf);
      lpc_socks[i].w_buf = NULL;
      lpc_socks[i].w_off = 0;
    }
  lpc_socks[i].flags &= ~S_BLOCKED;

  if (!modify_socket_runtime (i))
    {
      socket_close (i, SC_FORCE | SC_FINAL_CLOSE);
      return;
    }

  if (lpc_socks[i].state == FLUSHING)
    {
      socket_close (i, SC_FORCE | SC_FINAL_CLOSE);
      return;
    }

  if (lpc_socks[i].w_buf == NULL)
    complete_socket_operation (i, OP_COMPLETED);

  push_number (i);
  call_callback (i, S_WRITE_FP, 1);
}

/*
 * Close an LPC efun socket
 */
int socket_close (int i, int flags) {
  if (i < 0 || i >= max_lpc_socks)
    return EEFDRANGE;
  if (lpc_socks[i].state == CLOSED)
    return EEBADF;
  if (lpc_socks[i].state == FLUSHING && !(flags & SC_FINAL_CLOSE))
    return EEBADF;
  if (!(flags & SC_FORCE) && lpc_socks[i].owner_ob != current_object)
    return EESECURITY;

  if (lpc_socks[i].state != CLOSED)
    complete_socket_operation (i, OP_CANCELED);

  if (flags & SC_DO_CALLBACK)
    {
      push_number (i);
      call_callback (i, S_CLOSE_FP, 1);
    }

  set_read_callback (i, 0);
  set_write_callback (i, 0);
  set_close_callback (i, 0);

  if (lpc_socks[i].flags & S_BLOCKED)
    {
      /* Can't close now; we still have data to write.  Tell the mudlib
       * it is closed, but we really finish up later.
       */
      lpc_socks[i].state = FLUSHING;
      if (!modify_socket_runtime (i))
        {
          lpc_socks[i].flags &= ~S_BLOCKED;
        }
      return EESUCCESS;
    }

  remove_socket_runtime (i);

  while (SOCKET_CLOSE (lpc_socks[i].fd) == -1 && SOCKET_ERRNO == EINTR)
    ;				/* empty while */
  lpc_socks[i].fd = INVALID_SOCKET_FD;
  lpc_socks[i].state = CLOSED;
  if (lpc_socks[i].r_buf != NULL)
    FREE (lpc_socks[i].r_buf);
  if (lpc_socks[i].w_buf != NULL)
    FREE (lpc_socks[i].w_buf);

  return EESUCCESS;
}

/*
 * Release an LPC efun socket to another object
 */
int socket_release (int i, object_t * ob, svalue_t * callback) {
  if (i < 0 || i >= max_lpc_socks)
    return EEFDRANGE;
  if (lpc_socks[i].state == CLOSED || lpc_socks[i].state == FLUSHING)
    return EEBADF;
  if (lpc_socks[i].owner_ob != current_object)
    return EESECURITY;
  if (lpc_socks[i].flags & S_RELEASE)
    return EESOCKRLSD;

  lpc_socks[i].flags |= S_RELEASE;
  lpc_socks[i].release_ob = ob;

  push_number (i);
  push_object (ob);

  if (callback->type == T_FUNCTION)
    safe_call_function_pointer (callback->u.fp, 2);
  else
    safe_apply (callback->u.string, ob, 2, ORIGIN_DRIVER);

  if (socket_release_test_hook && (lpc_socks[i].flags & S_RELEASE))
    socket_release_test_hook (i, ob);

  if ((lpc_socks[i].flags & S_RELEASE) == 0)
    return EESUCCESS;

  lpc_socks[i].flags &= ~S_RELEASE;
  lpc_socks[i].release_ob = NULL;

  return EESOCKNOTRLSD;
}

/*
 * Aquire an LPC efun socket from another object
 */
int
socket_acquire (int i, svalue_t * read_callback, svalue_t * write_callback, svalue_t * close_callback)
{
  if (i < 0 || i >= max_lpc_socks)
    return EEFDRANGE;
  if (lpc_socks[i].state == CLOSED || lpc_socks[i].state == FLUSHING)
    return EEBADF;
  if ((lpc_socks[i].flags & S_RELEASE) == 0)
    return EESOCKNOTRLSD;
  if (lpc_socks[i].release_ob != current_object)
    return EESECURITY;

  lpc_socks[i].flags &= ~S_RELEASE;
  lpc_socks[i].owner_ob = current_object;
  lpc_socks[i].release_ob = NULL;

  set_read_callback (i, read_callback);
  set_write_callback (i, write_callback);
  set_close_callback (i, close_callback);

  return EESUCCESS;
}

/*
 * Return the string representation of a socket error
 */
char *socket_error (int error) {
  error = -(error + 1);
  if (error < 0 || error >= ERROR_STRINGS)
    return "socket_error: invalid error number";
  return error_strings[error];
}

int
get_socket_operation_info (int socket_id, int *active, int *terminal, int *op_id, int *phase)
{
  if (socket_id < 0 || socket_id >= max_lpc_socks)
    return EEFDRANGE;

  if (active)
    *active = 0;
  if (terminal)
    *terminal = 0;
  if (op_id)
    *op_id = 0;
  if (phase)
    *phase = OP_INIT;

  if (socket_ops == NULL)
    return EESUCCESS;

  if (active)
    *active = socket_ops[socket_id].active;
  if (terminal)
    *terminal = socket_ops[socket_id].terminal;
  if (op_id)
    *op_id = socket_ops[socket_id].op_id;
  if (phase)
    *phase = socket_ops[socket_id].phase;

  return EESUCCESS;
}

int
get_socket_runtime_info (int socket_id, int *registered, int *events, socket_fd_t *tracked_fd)
{
  if (socket_id < 0 || socket_id >= max_lpc_socks)
    return EEFDRANGE;

  if (registered)
    *registered = 0;
  if (events)
    *events = 0;
  if (tracked_fd)
    *tracked_fd = INVALID_SOCKET_FD;

  if (socket_runtime_state == NULL)
    return EESUCCESS;

  if (registered)
    *registered = socket_runtime_state[socket_id].registered;
  if (events)
    *events = socket_runtime_state[socket_id].events;
  if (tracked_fd)
    *tracked_fd = (int) socket_runtime_state[socket_id].fd;

  return EESUCCESS;
}

int
get_socket_runtime_registration_count (void)
{
  int i;
  int count = 0;

  if (socket_runtime_state == NULL)
    return 0;

  for (i = 0; i < max_lpc_socks; i++)
    if (socket_runtime_state[i].registered)
      count++;

  return count;
}

void *
get_socket_runtime_context (int socket_id)
{
  if (socket_id < 0 || socket_id >= max_lpc_socks)
    return NULL;
  if (socket_contexts == NULL)
    return NULL;
  return socket_contexts[socket_id];
}

int
resolve_lpc_socket_context (void *context, socket_fd_t event_fd, int *socket_index)
{
  int socket_id;
  int i;
  int found = 0;

  if (!context)
    return 0;
  if (socket_contexts == NULL || socket_runtime_state == NULL)
    return 0;

  socket_id = -1;
  for (i = 0; i < max_lpc_socks; i++)
    if (socket_contexts[i] == context)
      {
        socket_id = i;
        found = 1;
        break;
      }

  if (!found)
    return 0;
  if (!socket_runtime_state[socket_id].registered)
    return 0;
  if (socket_runtime_state[socket_id].fd != event_fd)
    {
      opt_trace (TT_COMM|2, "socket-runtime stale dispatch ignored: socket=%d event_fd=%d tracked_fd=%d",
                 socket_id,
                 (int) event_fd,
                 (int) socket_runtime_state[socket_id].fd);
      return 0;
    }
  if (lpc_socks[socket_id].fd != event_fd)
    return 0;
  if (lpc_socks[socket_id].state == CLOSED || lpc_socks[socket_id].state == FLUSHING)
    return 0;

  if (socket_index)
    *socket_index = socket_id;

  return 1;
}

/*
 * Return the remote address for an LPC efun socket
 */
int get_socket_address (int i, char *addr, int *port) {
  if (i < 0 || i >= max_lpc_socks)
    {
      addr[0] = '\0';
      *port = 0;
      return EEFDRANGE;
    }
  *port = (int) ntohs (lpc_socks[i].r_addr.sin_port);
  inet_ntop (AF_INET, &lpc_socks[i].r_addr.sin_addr, addr, ADDR_BUF_SIZE);
  return EESUCCESS;
}

/*
 * Return the current socket owner
 */
object_t *
get_socket_owner (int i)
{
  if (i < 0 || i >= max_lpc_socks)
    return (object_t *) NULL;
  if (lpc_socks[i].state == CLOSED || lpc_socks[i].state == FLUSHING)
    return (object_t *) NULL;
  return lpc_socks[i].owner_ob;
}

/*
 * Initialize a T_OBJECT svalue
 */
void
assign_socket_owner (svalue_t * sv, object_t * ob)
{
  if (ob != NULL)
    {
      sv->type = T_OBJECT;
      sv->u.ob = ob;
      add_ref (ob, "assign_socket_owner");
    }
  else
    assign_svalue_no_free (sv, &const0u);
}

/**
 * Convert a string representation of an address to a sockaddr_in
 */
static int socket_name_to_sin (char *name, struct sockaddr_in *sin) {
  long port;
  char *cp, *port_end, addr[ADDR_BUF_SIZE];

  if (name == NULL || sin == NULL)
    return 0;

  strncpy (addr, name, ADDR_BUF_SIZE);
  addr[ADDR_BUF_SIZE - 1] = '\0';

  cp = strchr (addr, ' ');
  if (cp == NULL || *(cp + 1) == '\0')
    return 0;

  *cp = '\0';
  port = strtol (cp + 1, &port_end, 10);
  if (port_end == (cp + 1) || *port_end != '\0' || port < 0 || port > 65535)
    return 0;

  sin->sin_family = AF_INET;
  sin->sin_port = htons ((u_short) port);
  if (inet_pton (AF_INET, addr, &sin->sin_addr) != 1)
    {
      /* Hostname resolution must be handled asynchronously via worker thread.
       * Returning 0 here means the address couldn't be parsed as numeric IPv4.
       * socket_connect() will detect this and queue DNS work. */
      return 0;
    }

  return 1;
}

static void
clear_dns_pending_resolution (int socket_id)
{
  if (socket_id < 0 || socket_id >= max_lpc_socks)
    return;

  if (dns_pending_hostnames != NULL)
    dns_pending_hostnames[socket_id][0] = '\0';
  if (dns_pending_ports != NULL)
    dns_pending_ports[socket_id] = 0;
  if (dns_pending_leaders != NULL)
    dns_pending_leaders[socket_id] = -1;
  if (dns_pending_leader_op_ids != NULL)
    dns_pending_leader_op_ids[socket_id] = -1;
  if (dns_pending_request_ids != NULL)
    dns_pending_request_ids[socket_id] = 0;
}

static void
apply_dns_result_to_socket (int socket_id, const socket_dns_apply_result_t *result)
{
  if (socket_id < 0 || socket_id >= max_lpc_socks)
    return;

  if (lpc_socks[socket_id].state == CLOSED ||
      lpc_socks[socket_id].state == FLUSHING)
    return;

  if (!socket_ops[socket_id].active ||
      socket_ops[socket_id].phase != OP_DNS_RESOLVING)
    return;

  if (result->timed_out)
    {
      complete_socket_operation (socket_id, OP_TIMED_OUT);
      return;
    }

  if (!result->success)
    {
      complete_socket_operation (socket_id, OP_FAILED);
      return;
    }

  lpc_socks[socket_id].r_addr.sin_family = AF_INET;
  lpc_socks[socket_id].r_addr.sin_addr = result->resolved_addr;
  lpc_socks[socket_id].r_addr.sin_port = htons (result->port);

  if (!set_socket_operation_phase (socket_id, OP_CONNECTING))
    {
      complete_socket_operation (socket_id, OP_FAILED);
      return;
    }

  if (connect (lpc_socks[socket_id].fd,
               (struct sockaddr *) &lpc_socks[socket_id].r_addr,
               sizeof (struct sockaddr_in)) == SOCKET_ERROR)
    {
      switch (SOCKET_ERRNO)
        {
#ifdef WINSOCK
        case WSAEWOULDBLOCK:
        case WSAEINPROGRESS:
          break;
#else
        case EINPROGRESS:
          break;
#endif
        default:
          complete_socket_operation (socket_id, OP_FAILED);
          return;
        }
    }

  lpc_socks[socket_id].state = DATA_XFER;
  lpc_socks[socket_id].flags |= S_BLOCKED;
  set_socket_operation_phase (socket_id, OP_TRANSFERRING);

  if (!modify_socket_runtime (socket_id))
    {
      socket_close (socket_id, SC_FORCE);
      complete_socket_operation (socket_id, OP_FAILED);
    }
}

static int
is_socket_dns_request_id (int request_id)
{
  return request_id >= SOCKET_DNS_REQUEST_ID_BASE;
}

/**
 * Queue a DNS resolution task (called from main thread only)
 * Returns EESUCCESS on success, deterministic EE* code on rejection/failure.
 */
static int queue_dns_resolution(int socket_id, const char *hostname, uint16_t port) {
  int i;
  int owner_count = 0;
  int leader_socket_id = -1;
  int leader_op_id = -1;
  int request_id = 0;
  time_t deadline;

  if (socket_id < 0 || socket_id >= max_lpc_socks)
    return EEFDRANGE;

  if (hostname == NULL || hostname[0] == '\0')
    return EEBADADDR;

  /* Optional duplicate lookup coalescing: attach to existing leader task. */
  if (hostname != NULL)
    {
      for (i = 0; i < max_lpc_socks; i++)
        {
          if (i == socket_id)
            continue;
          if (!socket_ops[i].active || socket_ops[i].phase != OP_DNS_RESOLVING)
            continue;
          if (dns_pending_leaders == NULL || dns_pending_leaders[i] != i)
            continue;
          if (dns_pending_ports == NULL || dns_pending_ports[i] != port)
            continue;
          if (dns_pending_hostnames == NULL)
            continue;
          if (strcmp (dns_pending_hostnames[i], hostname) != 0)
            continue;
          if (dns_pending_request_ids == NULL || dns_pending_request_ids[i] <= 0)
            continue;
          leader_socket_id = i;
          leader_op_id = socket_ops[i].op_id;
          request_id = dns_pending_request_ids[i];
          break;
        }
    }

  if (leader_socket_id >= 0)
    {
      if (dns_pending_hostnames != NULL && hostname != NULL)
        {
          strncpy (dns_pending_hostnames[socket_id], hostname, ADDR_BUF_SIZE);
          dns_pending_hostnames[socket_id][ADDR_BUF_SIZE - 1] = '\0';
        }
      if (dns_pending_ports != NULL)
        dns_pending_ports[socket_id] = port;
      if (dns_pending_leaders != NULL)
        dns_pending_leaders[socket_id] = leader_socket_id;
      if (dns_pending_leader_op_ids != NULL)
        dns_pending_leader_op_ids[socket_id] = leader_op_id;
      if (dns_pending_request_ids != NULL)
        dns_pending_request_ids[socket_id] = request_id;
      dns_telemetry.dedup_hit++;
      return EESUCCESS;
    }

  /* Check global in-flight cap */
  if (dns_tasks_in_flight >= DNS_GLOBAL_CAP) {
    dns_telemetry.rejected_global++;
    return EERESOLVERBUSY;
  }

  /* Check per-owner cap */
  if (socket_id >= 0 && socket_id < max_lpc_socks && 
      lpc_socks[socket_id].owner_ob != NULL) {
    for (i = 0; i < max_lpc_socks; i++) {
      if (lpc_socks[i].owner_ob == lpc_socks[socket_id].owner_ob &&
          socket_ops[i].active &&
          socket_ops[i].phase == OP_DNS_RESOLVING) {
        owner_count++;
      }
    }
    if (owner_count >= DNS_PER_OWNER_CAP) {
      dns_telemetry.rejected_owner++;
      return EERESOLVERBUSY;
    }
  }

  if (next_socket_dns_request_id <= SOCKET_DNS_REQUEST_ID_BASE)
    next_socket_dns_request_id = SOCKET_DNS_REQUEST_ID_BASE;
  request_id = next_socket_dns_request_id++;

  deadline = time(NULL) + DNS_TIMEOUT_SECONDS;
  if (strncmp (hostname, "force-timeout-", 14) == 0)
    {
      deadline = 0;
    }
  else if (socket_dns_timeout_test_hook != NULL &&
           socket_dns_timeout_test_hook (socket_id, hostname, port))
    {
      deadline = 0;
    }

  if (dns_pending_hostnames != NULL && hostname != NULL)
    {
      strncpy (dns_pending_hostnames[socket_id], hostname, ADDR_BUF_SIZE);
      dns_pending_hostnames[socket_id][ADDR_BUF_SIZE - 1] = '\0';
    }
  if (dns_pending_ports != NULL)
    dns_pending_ports[socket_id] = port;
  if (dns_pending_leaders != NULL)
    dns_pending_leaders[socket_id] = socket_id;
  if (dns_pending_leader_op_ids != NULL)
    dns_pending_leader_op_ids[socket_id] = socket_ops[socket_id].op_id;
  if (dns_pending_request_ids != NULL)
    dns_pending_request_ids[socket_id] = request_id;

  if (!addr_resolver_enqueue_lookup (request_id, hostname, deadline)) {
    clear_dns_pending_resolution (socket_id);
    dns_telemetry.rejected_queue++;
    return EERESOLVERBUSY;
  }

  dns_tasks_in_flight++;
  dns_telemetry.admitted++;
  return EESUCCESS;
}

int
handle_socket_dns_resolver_result (const resolver_result_t *result)
{
  int i;
  int leader_socket = -1;
  int leader_op_id = -1;
  socket_dns_apply_result_t socket_result;

  if (result == NULL || result->type != RESOLVER_REQ_LOOKUP)
    return 0;
  if (!is_socket_dns_request_id (result->request_id))
    return 0;

  if (dns_tasks_in_flight > 0)
    dns_tasks_in_flight--;

  for (i = 0; i < max_lpc_socks; i++)
    {
      if (dns_pending_leaders == NULL || dns_pending_leaders[i] != i)
        continue;
      if (dns_pending_request_ids == NULL || dns_pending_request_ids[i] != result->request_id)
        continue;
      if (!socket_ops[i].active || socket_ops[i].phase != OP_DNS_RESOLVING)
        continue;
      if (socket_ops[i].op_id != dns_pending_leader_op_ids[i])
        continue;
      leader_socket = i;
      leader_op_id = socket_ops[i].op_id;
      break;
    }

  if (leader_socket < 0)
    return 1;

  memset (&socket_result, 0, sizeof (socket_result));
  socket_result.timed_out = result->timed_out;
  socket_result.success = result->success;

  if (socket_result.success && !socket_result.timed_out)
    {
      if (inet_pton (AF_INET, result->result, &socket_result.resolved_addr) != 1)
        socket_result.success = 0;
    }

  if (socket_result.timed_out)
    dns_telemetry.timed_out++;
  else if (!socket_result.success)
    dns_telemetry.failed++;
  else
    dns_telemetry.completed++;

  for (i = 0; i < max_lpc_socks; i++)
    {
      if (dns_pending_request_ids == NULL || dns_pending_request_ids[i] != result->request_id)
        continue;
      if (dns_pending_leader_op_ids == NULL || dns_pending_leader_op_ids[i] != leader_op_id)
        continue;
      if (dns_pending_ports != NULL)
        socket_result.port = dns_pending_ports[i];
      apply_dns_result_to_socket (i, &socket_result);
    }

  return 1;
}

/**
 * Initialize DNS worker and queues (called once at driver startup)
 */
static int init_dns_system(void) {
  addr_resolver_config_t resolver_config;

  stem_get_addr_resolver_config (&resolver_config);
  return addr_resolver_init (get_async_runtime (), &resolver_config);
}

/**
 * Shutdown DNS worker and queues (called at driver shutdown)
 */
void deinit_dns_system(void) {
  if (dns_pending_hostnames != NULL) {
    FREE (dns_pending_hostnames);
    dns_pending_hostnames = NULL;
  }
  if (dns_pending_ports != NULL) {
    FREE (dns_pending_ports);
    dns_pending_ports = NULL;
  }
  if (dns_pending_leaders != NULL) {
    FREE (dns_pending_leaders);
    dns_pending_leaders = NULL;
  }
  if (dns_pending_leader_op_ids != NULL) {
    FREE (dns_pending_leader_op_ids);
    dns_pending_leader_op_ids = NULL;
  }
  if (dns_pending_request_ids != NULL) {
    FREE (dns_pending_request_ids);
    dns_pending_request_ids = NULL;
  }
  socket_dns_timeout_test_hook = NULL;
  dns_tasks_in_flight = 0;
  next_socket_dns_request_id = SOCKET_DNS_REQUEST_ID_BASE;
  memset(&dns_telemetry, 0, sizeof(dns_telemetry));
}

/**
 * Public wrapper for DNS completion handling (called from main loop)
 */
void handle_dns_completions(void) {
  resolver_result_t result;

  while (addr_resolver_dequeue_result (&result))
    {
      (void) handle_socket_dns_resolver_result (&result);
    }
}

int get_dns_telemetry_snapshot(int *in_flight, unsigned long *admitted, unsigned long *dedup_hit,
                               unsigned long *timed_out) {
  if (in_flight != NULL)
    *in_flight = dns_tasks_in_flight;
  if (admitted != NULL)
    *admitted = dns_telemetry.admitted;
  if (dedup_hit != NULL)
    *dedup_hit = dns_telemetry.dedup_hit;
  if (timed_out != NULL)
    *timed_out = dns_telemetry.timed_out;
  return EESUCCESS;
}

/**
 * Close any sockets owned by ob
 */
void close_referencing_sockets (object_t * ob) {
  int i;

  for (i = 0; i < max_lpc_socks; i++)
    if (lpc_socks[i].owner_ob == ob &&
        lpc_socks[i].state != CLOSED && lpc_socks[i].state != FLUSHING)
      socket_close (i, SC_FORCE);
}

/**
 * Return the string representation of a sockaddr_in
 */
static char* inet_address (struct sockaddr_in *sin) {
  static char addr[23], port[7];

  if (ntohl (sin->sin_addr.s_addr) == INADDR_ANY)
    strcpy (addr, "*");
  else
    inet_ntop (AF_INET, &sin->sin_addr.s_addr, addr, sizeof(addr));
  strcat (addr, ".");

  if (ntohs (sin->sin_port) == 0)
    strcpy (port, "*");
  else
    sprintf (port, "%d", (int) ntohs (sin->sin_port));
  strcat (addr, port);

  return (addr);
}

/**
 * Dump the LPC efun socket array
 */
void dump_socket_status (outbuffer_t * out) {
  int i;

  outbuf_add (out, "Fd    State      Mode       Local Address          Remote Address\n");
  outbuf_add (out, "--  ---------  --------  ---------------------  ---------------------\n");

  for (i = 0; i < max_lpc_socks; i++)
    {
      outbuf_addv (out, "%2d  ", lpc_socks[i].fd);

      switch (lpc_socks[i].state)
        {
        case FLUSHING:
          outbuf_add (out, " CLOSING ");
          break;
        case CLOSED:
          outbuf_add (out, "  CLOSED ");
          break;
        case UNBOUND:
          outbuf_add (out, " UNBOUND ");
          break;
        case BOUND:
          outbuf_add (out, "  BOUND  ");
          break;
        case LISTEN:
          outbuf_add (out, " LISTEN  ");
          break;
        case DATA_XFER:
          outbuf_add (out, "DATA_XFER");
          break;
        default:
          outbuf_add (out, "    ??    ");
          break;
        }
      outbuf_add (out, "  ");

      switch (lpc_socks[i].mode)
        {
        case MUD:
          outbuf_add (out, "   MUD  ");
          break;
        case STREAM:
          outbuf_add (out, " STREAM ");
          break;
        case DATAGRAM:
          outbuf_add (out, "DATAGRAM");
          break;
        default:
          outbuf_add (out, "   ??   ");
          break;
        }
      outbuf_add (out, "  ");

      outbuf_addv (out, "%-21s  ", inet_address (&lpc_socks[i].l_addr));
      outbuf_addv (out, "%-21s\n", inet_address (&lpc_socks[i].r_addr));
    }

  outbuf_add (out, "\nSocket Runtime Diagnostics\n");
  outbuf_add (out, "Idx  FD   TrackedFD  Reg  Events  Context   Mapping\n");
  outbuf_add (out, "---  ---  ---------  ---  ------  --------  -------\n");

  for (i = 0; i < max_lpc_socks; i++)
    {
      char event_mask[3];
      const char *context_state;
      const char *mapping_state;
      intptr_t tracked_fd_value;
      int tracked_fd_display;
      int events;
      int registered;

      tracked_fd_value = (intptr_t) INVALID_SOCKET_FD;
      events = 0;
      registered = 0;
      if (socket_runtime_state != NULL)
        {
          tracked_fd_value = (intptr_t) socket_runtime_state[i].fd;
          events = socket_runtime_state[i].events;
          registered = socket_runtime_state[i].registered;
        }

      tracked_fd_display = (tracked_fd_value == (intptr_t) INVALID_SOCKET_FD) ? -1 : (int) tracked_fd_value;
      event_mask[0] = (events & EVENT_READ) ? 'R' : '-';
      event_mask[1] = (events & EVENT_WRITE) ? 'W' : '-';
      event_mask[2] = '\0';

      context_state = (socket_contexts && socket_contexts[i]) ? "set" : "null";
      mapping_state = (registered && tracked_fd_value != (intptr_t) INVALID_SOCKET_FD &&
                       tracked_fd_value != (intptr_t) lpc_socks[i].fd)
        ? "stale"
        : "ok";

      outbuf_addv (out, "%3d  %3d  %9d  %3d  %-6s  %-8s  %s\n",
                   i,
                   (int) lpc_socks[i].fd,
                   tracked_fd_display,
                   registered,
                   event_mask,
                   context_state,
                   mapping_state);
    }

  outbuf_add (out, "\nDNS Telemetry:\n");
  outbuf_addv (out, "  Admitted: %lu\n", dns_telemetry.admitted);
  outbuf_addv (out, "  Dedup hit: %lu\n", dns_telemetry.dedup_hit);
  outbuf_addv (out, "  Rejected (global cap): %lu\n", dns_telemetry.rejected_global);
  outbuf_addv (out, "  Rejected (owner cap): %lu\n", dns_telemetry.rejected_owner);
  outbuf_addv (out, "  Rejected (queue full): %lu\n", dns_telemetry.rejected_queue);
  outbuf_addv (out, "  Completed successfully: %lu\n", dns_telemetry.completed);
  outbuf_addv (out, "  Failed (not timed out): %lu\n", dns_telemetry.failed);
  outbuf_addv (out, "  Timed out: %lu\n", dns_telemetry.timed_out);
  outbuf_addv (out, "  Currently in-flight: %d\n", dns_tasks_in_flight);
}

#endif /* SOCKET_EFUNS */
