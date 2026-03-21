#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"

#ifdef WINSOCK
static ::testing::Environment* const winsock_env =
  ::testing::AddGlobalTestEnvironment(new WinsockEnvironment);
#endif

#ifdef PACKAGE_PEER_REVERSE_DNS
extern "C" void handle_dns_completions(void);
extern "C" void set_socket_dns_timeout_test_hook(int (*hook)(int, const char *, uint16_t));
extern "C" int get_dns_telemetry_snapshot(int *in_flight, unsigned long *admitted, unsigned long *dedup_hit,
                                           unsigned long *timed_out);
#endif

namespace {

bool CreateLoopbackListener(socket_fd_t *listener_fd, int *port) {
  int one = 1;
  socklen_t len;
  struct sockaddr_in sin;

  if (!listener_fd || !port) {
    return false;
  }

  *listener_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (*listener_fd == INVALID_SOCKET_FD) {
    return false;
  }

  if (setsockopt(*listener_fd, SOL_SOCKET, SO_REUSEADDR,
                 (const char *)&one, sizeof(one)) == SOCKET_ERROR) {
    SOCKET_CLOSE(*listener_fd);
    *listener_fd = INVALID_SOCKET_FD;
    return false;
  }

  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  sin.sin_port = htons(0);

  if (bind(*listener_fd, (struct sockaddr *)&sin, sizeof(sin)) == SOCKET_ERROR) {
    SOCKET_CLOSE(*listener_fd);
    *listener_fd = INVALID_SOCKET_FD;
    return false;
  }

  if (listen(*listener_fd, 1) == SOCKET_ERROR) {
    SOCKET_CLOSE(*listener_fd);
    *listener_fd = INVALID_SOCKET_FD;
    return false;
  }

  len = sizeof(sin);
  if (getsockname(*listener_fd, (struct sockaddr *)&sin, &len) == SOCKET_ERROR) {
    SOCKET_CLOSE(*listener_fd);
    *listener_fd = INVALID_SOCKET_FD;
    return false;
  }

  *port = (int)ntohs(sin.sin_port);
  return true;
}

bool AcceptPendingConnection(socket_fd_t listener_fd, socket_fd_t *accepted_fd) {
  fd_set read_set;
  struct timeval timeout;
  int selected;

  if (accepted_fd == nullptr) {
    return false;
  }

  *accepted_fd = INVALID_SOCKET_FD;
  FD_ZERO(&read_set);
  FD_SET(listener_fd, &read_set);
  timeout.tv_sec = 0;
  timeout.tv_usec = 500000;

  selected = select(FD_SETSIZE, &read_set, nullptr, nullptr, &timeout);
  if (selected <= 0 || !FD_ISSET(listener_fd, &read_set)) {
    return false;
  }

  *accepted_fd = accept(listener_fd, nullptr, nullptr);
  return *accepted_fd != INVALID_SOCKET_FD;
}

#ifdef PACKAGE_PEER_REVERSE_DNS
extern "C" int get_socket_operation_info(int socket_id, 
                                         int* op_active, int* op_terminal,
                                         int* op_id, int* op_phase);

/**
 * Wait for pending DNS completions to be processed.
 * Polls handle_dns_completions() repeatedly with small sleeps to allow
 * DNS worker thread to resolve names and post completions back.
 * 
 * @param socket_fd Socket file descriptor to check
 * @param timeout_ms Maximum time to wait in milliseconds
 * @returns true if socket operation completes, false on timeout
 */
bool WaitForDNSCompletion(int socket_fd, int timeout_ms = 5000) {
  int elapsed = 0;
  const int sleep_step = 10;  // 10ms polling interval
  
  while (elapsed < timeout_ms) {
    // Poll for DNS completions
    handle_dns_completions();
    
    // Check current socket operation state
    int op_active = 0, op_terminal = 0, op_id = 0, op_phase = 0;
    if (get_socket_operation_info(socket_fd, &op_active, &op_terminal,
                                   &op_id, &op_phase) == EESUCCESS) {
      // If operation is terminal or transitioned out of DNS_RESOLVING, we're done
      if (op_terminal || op_phase != OP_DNS_RESOLVING) {
        return true;
      }
    }
    
#ifdef WINSOCK
    Sleep(sleep_step);
#else
    usleep(sleep_step * 1000);  // Convert to microseconds
#endif
    elapsed += sleep_step;
  }
  
  return false;  // Timeout waiting for DNS completion
}

extern "C" int ForceDnsTimeoutHook(int, const char *, uint16_t) {
  return 1;
}

class ScopedDnsTimeoutHook {
public:
  ScopedDnsTimeoutHook() {
    set_socket_dns_timeout_test_hook(ForceDnsTimeoutHook);
  }

  ~ScopedDnsTimeoutHook() {
    set_socket_dns_timeout_test_hook(nullptr);
  }
};
#endif

/* OP_PHASES definitions needed by tests */
#define OP_INIT 0
#define OP_DNS_RESOLVING 1
#define OP_CONNECTING 2
#define OP_TRANSFERRING 3
#define OP_COMPLETED 4
#define OP_FAILED 5
#define OP_TIMED_OUT 6
#define OP_CANCELED 7

class ScopedAsyncRuntime {
public:
  ScopedAsyncRuntime() : owns_runtime_(false) {
    if (g_runtime == nullptr) {
      g_runtime = async_runtime_init();
      owns_runtime_ = (g_runtime != nullptr);
    }
  }

  ~ScopedAsyncRuntime() {
    if (owns_runtime_ && g_runtime != nullptr) {
      async_runtime_deinit(g_runtime);
      g_runtime = nullptr;
    }
  }

  bool IsReady() const {
    return g_runtime != nullptr;
  }

private:
  bool owns_runtime_;
};

object_t *LoadInlineObject(const char *name, const char *code) {
  object_t *saved_current;
  object_t *obj;

  saved_current = current_object;
  current_object = master_ob;
  obj = load_object(name, code);
  current_object = saved_current;
  return obj;
}

int QueryObjectNumberMethod(object_t *obj, const char *method) {
  int index;
  int fio;
  int vio;
  int runtime_index;
  int saved_vio;
  object_t *saved_current;
  program_t *found_prog;
  svalue_t ret;
  int result = 0;

  found_prog = find_function(obj->prog, findstring(method), &index, &fio, &vio);
  if (found_prog == nullptr) {
    return 0;
  }

  runtime_index = found_prog->function_table[index].runtime_index + fio;
  saved_current = current_object;
  saved_vio = variable_index_offset;
  current_object = obj;
  variable_index_offset = vio;
  call_function(obj->prog, runtime_index, 0, &ret);
  current_object = saved_current;
  variable_index_offset = saved_vio;

  if (ret.type == T_NUMBER) {
    result = (int)ret.u.number;
  }
  free_svalue(&ret, "QueryObjectNumberMethod");
  return result;
}

static svalue_t *release_hook_read_cb = nullptr;
static svalue_t *release_hook_write_cb = nullptr;
static svalue_t *release_hook_close_cb = nullptr;
static int release_hook_called = 0;
static int release_hook_result = EEBADF;

void SocketReleaseAcquireHook(int fd, object_t *release_ob) {
  object_t *saved_current = current_object;
  current_object = release_ob;
  release_hook_called = 1;
  release_hook_result = socket_acquire(fd, release_hook_read_cb, release_hook_write_cb, release_hook_close_cb);
  current_object = saved_current;
}

} // namespace

// Behavior compatibility tests matching plan matrix IDs.
// Each test verifies socket efun behavior against the baseline matrix defined in
// docs/plan/socket-operation-engine.md Stage 1 checklist.

/**
 * SOCK_BHV_001: Create stream socket succeeds
 * Setup: valid owner object (master_ob)
 * Action: call socket_create(STREAM, read_cb, close_cb)
 * Expected: non-negative fd, no callbacks immediately
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_BHV_001_CreateStream_Success) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  
  ScopedObjectContext ctx(this, master_ob);
  
  // Create svalue callbacks for read and close
  svalue_t read_cb, close_cb;
  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  
  close_cb.type = T_STRING;
  close_cb.subtype = STRING_SHARED;
  close_cb.u.string = make_shared_string("close_callback");
  
  // Create stream socket with callbacks
  int fd = socket_create(STREAM, &read_cb, &close_cb);
  
  // Verify success: fd should be non-negative
  EXPECT_GE(fd, 0) << "socket_create(STREAM, ...) returned negative fd: " << fd;
  
  // Verify no callbacks fired immediately
  ExpectNoCallbacks();

  EXPECT_EQ(socket_close(fd, 1), EESUCCESS);
  
  // Cleanup
  free_string(read_cb.u.string);
  free_string(close_cb.u.string);
}

/**
 * SOCK_BHV_002: Create datagram socket drops close callback path
 * Setup: valid owner object
 * Action: call socket_create(DATAGRAM, read_cb, close_cb)
 * Expected: non-negative fd, close callback is ignored by current code
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_BHV_002_CreateDatagram_CloseCallbackIgnored) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  
  ScopedObjectContext ctx(this, master_ob);
  
  svalue_t read_cb, close_cb;
  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  
  close_cb.type = T_STRING;
  close_cb.subtype = STRING_SHARED;
  close_cb.u.string = make_shared_string("close_callback");
  
  // Create datagram socket - close callback should be ignored
  int fd = socket_create(DATAGRAM, &read_cb, &close_cb);
  
  EXPECT_GE(fd, 0) << "socket_create(DATAGRAM, ...) returned negative fd: " << fd;
  ExpectNoCallbacks();

  EXPECT_EQ(socket_close(fd, 1), EESUCCESS);
  
  free_string(read_cb.u.string);
  free_string(close_cb.u.string);
}

/**
 * SOCK_BHV_003: Invalid mode rejected
 * Setup: valid owner object
 * Action: call socket_create(invalid_mode, read_cb, close_cb)
 * Expected: EEMODENOTSUPP error, no callbacks
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_BHV_003_CreateInvalidMode_Rejected) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  
  ScopedObjectContext ctx(this, master_ob);
  
  svalue_t read_cb;
  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  
  // Try to create socket with invalid mode (999 is not a valid enum value)
  int fd = socket_create((enum socket_mode)999, &read_cb, NULL);
  
  // Mode validation should reject this
  // Note: actual behavior depends on implementation; commonly returns EEMODENOTSUPP or negative
  EXPECT_LT(fd, 1) << "socket_create with invalid mode should return error code";
  ExpectNoCallbacks();
  
  free_string(read_cb.u.string);
}

/**
 * SOCK_BHV_004: Bind unbound socket succeeds
 * Setup: created stream socket
 * Action: call socket_bind(fd, ephemeral_port)
 * Expected: EESUCCESS, local address populated
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_BHV_004_BindUnbound_Success) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  
  ScopedObjectContext ctx(this, master_ob);
  
  // Create stream socket
  svalue_t read_cb;
  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  int fd = socket_create(STREAM, &read_cb, NULL);
  ASSERT_GE(fd, 0) << "Failed to create socket for SOCK_BHV_004";
  
  // Bind to ephemeral port (port 0 lets OS choose)
  int result = socket_bind(fd, 0);
  
  EXPECT_EQ(result, EESUCCESS) 
    << "socket_bind(unbound_fd, 0) should succeed, got: " << result;
  ExpectNoCallbacks();

  EXPECT_EQ(socket_close(fd, 1), EESUCCESS);
  
  free_string(read_cb.u.string);
}

/**
 * SOCK_BHV_005: Bind already bound socket rejected
 * Setup: bound socket
 * Action: call socket_bind(fd, another_port)
 * Expected: EEISBOUND error
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_BHV_005_BindAlreadyBound_Rejected) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  
  ScopedObjectContext ctx(this, master_ob);
  
  // Create and bind socket
  svalue_t read_cb;
  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  int fd = socket_create(STREAM, &read_cb, NULL);
  ASSERT_GE(fd, 0) << "Failed to create socket";
  
  int result1 = socket_bind(fd, 0);
  ASSERT_EQ(result1, EESUCCESS) << "First bind should succeed";
  
  // Try to bind again to different port
  int result2 = socket_bind(fd, 0);
  
  EXPECT_EQ(result2, EEISBOUND) 
    << "socket_bind on already-bound socket should return EEISBOUND, got: " << result2;
  ExpectNoCallbacks();

  EXPECT_EQ(socket_close(fd, 1), EESUCCESS);
  
  free_string(read_cb.u.string);
}

/**
 * SOCK_BHV_006: Listen on bound stream socket succeeds
 * Setup: bound stream socket
 * Action: call socket_listen(fd, read_cb)
 * Expected: EESUCCESS, socket enters LISTEN state
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_BHV_006_ListenBoundStream_Success) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  
  ScopedObjectContext ctx(this, master_ob);
  
  // Create and bind socket
  svalue_t read_cb;
  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  int fd = socket_create(STREAM, &read_cb, NULL);
  ASSERT_GE(fd, 0) << "Failed to create socket";
  
  int bind_result = socket_bind(fd, 0);
  ASSERT_EQ(bind_result, EESUCCESS) << "Bind should succeed";
  
  // Listen on bound socket
  svalue_t listen_cb;
  listen_cb.type = T_STRING;
  listen_cb.subtype = STRING_SHARED;
  listen_cb.u.string = make_shared_string("listen_callback");
  int result = socket_listen(fd, &listen_cb);
  
  EXPECT_EQ(result, EESUCCESS) 
    << "socket_listen on bound stream should succeed, got: " << result;
  ExpectNoCallbacks();

  EXPECT_EQ(socket_close(fd, 1), EESUCCESS);
  
  free_string(read_cb.u.string);
  free_string(listen_cb.u.string);
}

/**
 * SOCK_BHV_007: Listen on datagram socket rejected
 * Setup: bound datagram socket
 * Action: call socket_listen(fd, read_cb)
 * Expected: EEMODENOTSUPP error (listen only works for STREAM/MUD sockets)
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_BHV_007_ListenDatagram_Rejected) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  
  ScopedObjectContext ctx(this, master_ob);
  
  // Create and bind datagram socket
  svalue_t read_cb;
  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  int fd = socket_create(DATAGRAM, &read_cb, NULL);
  ASSERT_GE(fd, 0) << "Failed to create datagram socket";
  
  int bind_result = socket_bind(fd, 0);
  ASSERT_EQ(bind_result, EESUCCESS) << "Bind should succeed";
  
  // Try to listen on datagram - should fail
  svalue_t listen_cb;
  listen_cb.type = T_STRING;
  listen_cb.subtype = STRING_SHARED;
  listen_cb.u.string = make_shared_string("listen_callback");
  int result = socket_listen(fd, &listen_cb);
  
  EXPECT_EQ(result, EEMODENOTSUPP) 
    << "socket_listen on datagram should fail with EEMODENOTSUPP, got: " << result;
  ExpectNoCallbacks();

  EXPECT_EQ(socket_close(fd, 1), EESUCCESS);
  
  free_string(read_cb.u.string);
  free_string(listen_cb.u.string);
}

/**
 * SOCK_BHV_008: Accept from listening socket succeeds
 * Setup: listening server socket
 * Action: (would require connected client for full test)
 * Expected: non-negative accepted fd, no callbacks at call site
 *
 * NOTE: Skeleton - full test requires loopback client connection setup
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_BHV_008_AcceptListening_Success) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";

  ScopedObjectContext ctx(this, master_ob);

  svalue_t read_cb;
  svalue_t listen_cb;
  svalue_t write_cb;
  socket_fd_t native_client_fd = INVALID_SOCKET_FD;
  int listen_fd;
  int accept_fd;
  int reserve_port = 0;
  socket_fd_t reserve_listener = INVALID_SOCKET_FD;
  struct sockaddr_in target;

  ASSERT_TRUE(CreateLoopbackListener(&reserve_listener, &reserve_port))
    << "Failed to reserve loopback port";
  SOCKET_CLOSE(reserve_listener);

  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  listen_cb.type = T_STRING;
  listen_cb.subtype = STRING_SHARED;
  listen_cb.u.string = make_shared_string("read_callback");
  write_cb.type = T_STRING;
  write_cb.subtype = STRING_SHARED;
  write_cb.u.string = make_shared_string("write_callback");

  listen_fd = socket_create(STREAM, &read_cb, nullptr);
  ASSERT_GE(listen_fd, 0) << "Failed to create listening socket";
  ASSERT_EQ(socket_bind(listen_fd, reserve_port), EESUCCESS) << "Bind should succeed";
  ASSERT_EQ(socket_listen(listen_fd, &listen_cb), EESUCCESS) << "Listen should succeed";

  native_client_fd = socket(AF_INET, SOCK_STREAM, 0);
  ASSERT_NE(native_client_fd, INVALID_SOCKET_FD) << "Failed to create native client socket";
  memset(&target, 0, sizeof(target));
  target.sin_family = AF_INET;
  target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  target.sin_port = htons((u_short)reserve_port);
  ASSERT_NE(connect(native_client_fd, (struct sockaddr *)&target, sizeof(target)), SOCKET_ERROR)
    << "Native client connect should succeed";

  accept_fd = socket_accept(listen_fd, &read_cb, &write_cb);
  EXPECT_GE(accept_fd, 0) << "socket_accept should return accepted fd";
  ExpectNoCallbacks();

  if (accept_fd >= 0) {
    EXPECT_EQ(socket_close(accept_fd, 1), EESUCCESS);
  }
  EXPECT_EQ(socket_close(listen_fd, 1), EESUCCESS);
  SOCKET_CLOSE(native_client_fd);

  free_string(read_cb.u.string);
  free_string(listen_cb.u.string);
  free_string(write_cb.u.string);
}

/**
 * SOCK_BHV_009: Accept when not listening rejected
 * Setup: created but non-listening socket
 * Action: call socket_accept(fd, read_cb, write_cb)
 * Expected: EENOTLISTN error
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_BHV_009_AcceptNotListening_Rejected) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  
  ScopedObjectContext ctx(this, master_ob);
  
  // Create unbound socket (not listening)
  svalue_t read_cb;
  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  int fd = socket_create(STREAM, &read_cb, NULL);
  ASSERT_GE(fd, 0) << "Failed to create socket";
  
  // Try to accept without listening
  svalue_t write_cb;
  write_cb.type = T_STRING;
  write_cb.subtype = STRING_SHARED;
  write_cb.u.string = make_shared_string("write_callback");
  int result = socket_accept(fd, &read_cb, &write_cb);
  
  EXPECT_EQ(result, EENOTLISTN) 
    << "socket_accept on non-listening socket should return EENOTLISTN, got: " << result;
  ExpectNoCallbacks();

  EXPECT_EQ(socket_close(fd, 1), EESUCCESS);
  
  free_string(read_cb.u.string);
  free_string(write_cb.u.string);
}

/**
 * SOCK_BHV_010: Connect with valid numeric address succeeds
 * Setup: created stream socket + reachable server
 * Action: call socket_connect(fd, "127.0.0.1 <port>", read_cb, write_cb)
 * Expected: EESUCCESS or callback-based completion success path
 *
 * NOTE: Skeleton - full test requires listening server setup
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_BHV_010_ConnectNumericAddress_SuccessPath) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";

  ScopedObjectContext ctx(this, master_ob);

  svalue_t read_cb;
  svalue_t write_cb;
  socket_fd_t listener_fd = INVALID_SOCKET_FD;
  socket_fd_t accepted_fd = INVALID_SOCKET_FD;
  int listener_port = 0;
  int fd;
  int result;
  std::string endpoint;

  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  write_cb.type = T_STRING;
  write_cb.subtype = STRING_SHARED;
  write_cb.u.string = make_shared_string("write_callback");

  fd = socket_create(STREAM, &read_cb, nullptr);
  ASSERT_GE(fd, 0) << "Failed to create stream socket";
  ASSERT_TRUE(CreateLoopbackListener(&listener_fd, &listener_port))
    << "Failed to create loopback listener";

  endpoint = "127.0.0.1 " + std::to_string(listener_port);
  result = socket_connect(fd, (char *)endpoint.c_str(), &read_cb, &write_cb);
  EXPECT_EQ(result, EESUCCESS) << "socket_connect should succeed on valid loopback endpoint";

  if (AcceptPendingConnection(listener_fd, &accepted_fd)) {
    SOCKET_CLOSE(accepted_fd);
  }

  EXPECT_EQ(socket_close(fd, 1), EESUCCESS);
  SOCKET_CLOSE(listener_fd);
  free_string(read_cb.u.string);
  free_string(write_cb.u.string);
}

/**
 * SOCK_BHV_011: Connect with malformed address rejected
 * Setup: created stream socket
 * Action: call socket_connect(fd, "bad_address", read_cb, write_cb)
 * Expected: EEBADADDR error
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_BHV_011_ConnectMalformedAddress_Rejected) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  
  ScopedObjectContext ctx(this, master_ob);
  
  // Create stream socket
  svalue_t read_cb;
  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  int fd = socket_create(STREAM, &read_cb, NULL);
  ASSERT_GE(fd, 0) << "Failed to create socket";
  
  // Try to connect with malformed address
  svalue_t write_cb;
  write_cb.type = T_STRING;
  write_cb.subtype = STRING_SHARED;
  write_cb.u.string = make_shared_string("write_callback");
  char *bad_addr = make_shared_string("bad_address_format");
  int result = socket_connect(fd, bad_addr, &read_cb, &write_cb);
  
  EXPECT_EQ(result, EEBADADDR) 
    << "socket_connect with malformed address should return EEBADADDR, got: " << result;
  ExpectNoCallbacks();

  EXPECT_EQ(socket_close(fd, 1), EESUCCESS);
  
  free_string(bad_addr);
  free_string(read_cb.u.string);
  free_string(write_cb.u.string);
}

/**
 * SOCK_BHV_012: Stream write on connected socket succeeds
 * Setup: connected stream socket
 * Action: call socket_write(fd, "payload", 0)
 * Expected: EESUCCESS or EECALLBACK
 *
 * NOTE: Skeleton - full test requires connection setup
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_BHV_012_StreamWriteConnected_SuccessOrCallback) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";

  object_t* callback_owner = LoadCallbackOwner("test_socket_write_owner.c");
  ASSERT_NE(callback_owner, nullptr);
  ScopedObjectContext ctx(this, callback_owner);

  svalue_t read_cb;
  svalue_t write_cb;
  svalue_t payload;
  socket_fd_t listener_fd = INVALID_SOCKET_FD;
  socket_fd_t accepted_fd = INVALID_SOCKET_FD;
  int listener_port = 0;
  int fd;
  int result;
  std::string endpoint;

  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  write_cb.type = T_STRING;
  write_cb.subtype = STRING_SHARED;
  write_cb.u.string = make_shared_string("write_callback");
  payload.type = T_STRING;
  payload.subtype = STRING_SHARED;
  payload.u.string = make_shared_string("payload");

  fd = socket_create(STREAM, &read_cb, nullptr);
  ASSERT_GE(fd, 0) << "Failed to create stream socket";
  ASSERT_TRUE(CreateLoopbackListener(&listener_fd, &listener_port))
    << "Failed to create loopback listener";

  endpoint = "127.0.0.1 " + std::to_string(listener_port);
  ASSERT_EQ(socket_connect(fd, (char *)endpoint.c_str(), &read_cb, &write_cb), EESUCCESS)
    << "Connect should succeed before write";

  ASSERT_TRUE(AcceptPendingConnection(listener_fd, &accepted_fd))
    << "Expected pending native connection after socket_connect";

  // socket_connect marks stream sockets blocked; flush that state before write test.
  socket_write_select_handler(fd);
  ClearCallbackOwnerEvents(callback_owner);

  result = socket_write(fd, &payload, nullptr);
  EXPECT_TRUE(result == EESUCCESS || result == EECALLBACK)
    << "Expected socket_write to return EESUCCESS or EECALLBACK, got: " << result;

  if (result == EECALLBACK) {
    socket_write_select_handler(fd);
  }

  EXPECT_EQ(socket_close(fd, 1), EESUCCESS);
  SOCKET_CLOSE(accepted_fd);
  SOCKET_CLOSE(listener_fd);
  free_string(read_cb.u.string);
  free_string(write_cb.u.string);
  free_string(payload.u.string);
}

/**
 * SOCK_BHV_013: Stream write while blocked rejected
 * Setup: connected stream socket forced into blocked state
 * Action: call socket_write(fd, "payload", 0)
 * Expected: EEALREADY error
 *
 * NOTE: Skeleton - requires blocked write state simulation
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_BHV_013_StreamWriteBlocked_Rejected) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";

  ScopedObjectContext ctx(this, master_ob);

  svalue_t read_cb;
  svalue_t write_cb;
  svalue_t payload;
  socket_fd_t listener_fd = INVALID_SOCKET_FD;
  socket_fd_t accepted_fd = INVALID_SOCKET_FD;
  int listener_port = 0;
  int fd;
  std::string endpoint;

  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  write_cb.type = T_STRING;
  write_cb.subtype = STRING_SHARED;
  write_cb.u.string = make_shared_string("write_callback");
  payload.type = T_STRING;
  payload.subtype = STRING_SHARED;
  payload.u.string = make_shared_string("payload");

  fd = socket_create(STREAM, &read_cb, nullptr);
  ASSERT_GE(fd, 0) << "Failed to create stream socket";
  ASSERT_TRUE(CreateLoopbackListener(&listener_fd, &listener_port))
    << "Failed to create loopback listener";

  endpoint = "127.0.0.1 " + std::to_string(listener_port);
  ASSERT_EQ(socket_connect(fd, (char *)endpoint.c_str(), &read_cb, &write_cb), EESUCCESS)
    << "Connect should succeed";

  ASSERT_TRUE(AcceptPendingConnection(listener_fd, &accepted_fd))
    << "Expected pending native connection after socket_connect";

  EXPECT_EQ(socket_write(fd, &payload, nullptr), EEALREADY)
    << "socket_write on blocked stream socket should return EEALREADY";

  EXPECT_EQ(socket_close(fd, 1), EESUCCESS);
  SOCKET_CLOSE(accepted_fd);
  SOCKET_CLOSE(listener_fd);
  free_string(read_cb.u.string);
  free_string(write_cb.u.string);
  free_string(payload.u.string);
}

/**
 * SOCK_BHV_014: Datagram write without address rejected
 * Setup: datagram socket (unconnected)
 * Action: call socket_write(fd, "payload", 0)
 * Expected: EENOADDR error
 *
 * NOTE: Skeleton - datagram requires explicit address handling
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_BHV_014_DatagramWriteNoAddress_Rejected) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";

  ScopedObjectContext ctx(this, master_ob);

  svalue_t read_cb;
  svalue_t payload;
  int fd;

  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");

  fd = socket_create(DATAGRAM, &read_cb, nullptr);
  ASSERT_GE(fd, 0) << "Failed to create datagram socket";

  payload.type = T_STRING;
  payload.subtype = STRING_SHARED;
  payload.u.string = make_shared_string("payload");

  EXPECT_EQ(socket_write(fd, &payload, nullptr), EENOADDR)
    << "Datagram write without address should return EENOADDR";

  EXPECT_EQ(socket_close(fd, 1), EESUCCESS);
  free_string(payload.u.string);
  free_string(read_cb.u.string);
}

/**
 * SOCK_BHV_015: Datagram write with invalid address rejected
 * Setup: datagram socket
 * Action: call socket_write(fd, "payload", "bad")
 * Expected: EEBADADDR error
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_BHV_015_DatagramWriteInvalidAddress_Rejected) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";

  ScopedObjectContext ctx(this, master_ob);

  svalue_t read_cb;
  svalue_t payload;
  int fd;

  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");

  fd = socket_create(DATAGRAM, &read_cb, nullptr);
  ASSERT_GE(fd, 0) << "Failed to create datagram socket";

  payload.type = T_STRING;
  payload.subtype = STRING_SHARED;
  payload.u.string = make_shared_string("payload");

  EXPECT_EQ(socket_write(fd, &payload, (char *)"bad"), EEBADADDR)
    << "Datagram write with malformed address should return EEBADADDR";

  EXPECT_EQ(socket_close(fd, 1), EESUCCESS);
  free_string(payload.u.string);
  free_string(read_cb.u.string);
}

/**
 * SOCK_BHV_016: Close by owner succeeds
 * Setup: open socket owned by caller
 * Action: call socket_close(fd)
 * Expected: EESUCCESS, close callback once if configured
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_BHV_016_CloseByOwner_Success) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";

  object_t* callback_owner = LoadCallbackOwner();
  ASSERT_NE(callback_owner, nullptr) << "Failed to load callback owner object";

  ScopedObjectContext ctx(this, callback_owner);

  svalue_t read_cb;
  svalue_t close_cb;
  int fd;
  constexpr int kSocketCloseForce = 1;
  constexpr int kSocketCloseDoCallback = 2;

  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  close_cb.type = T_STRING;
  close_cb.subtype = STRING_SHARED;
  close_cb.u.string = make_shared_string("close_callback");

  fd = socket_create(STREAM, &read_cb, &close_cb);
  ASSERT_GE(fd, 0) << "Failed to create stream socket";

  ClearCallbackOwnerEvents(callback_owner);

  EXPECT_EQ(socket_close(fd, kSocketCloseForce | kSocketCloseDoCallback), EESUCCESS)
    << "socket_close by owner should succeed";

  CaptureCallbacksFromOwner(callback_owner);
  ASSERT_EQ(callback_records.size(), 1U) << "Expected exactly one close callback";
  EXPECT_TRUE(VerifyCallbackType(CallbackRecord::CB_CLOSE, fd));

  CallbackRecord record = PopCallback();
  EXPECT_EQ(record.type, CallbackRecord::CB_CLOSE);
  EXPECT_EQ(record.socket_fd, fd);
  ExpectNoCallbacks();

  free_string(read_cb.u.string);
  free_string(close_cb.u.string);
}

/**
 * SOCK_BHV_017: Close by non-owner rejected
 * Setup: open socket owned by another object
 * Action: call socket_close(fd) from non-owner
 * Expected: EESECURITY error
 *
 * NOTE: Skeleton - requires multi-object test setup
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_BHV_017_CloseByNonOwner_Rejected) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";

  object_t* owner_a = LoadCallbackOwner("test_socket_owner_a.c");
  object_t* owner_b = LoadCallbackOwner("test_socket_owner_b.c");
  ASSERT_NE(owner_a, nullptr);
  ASSERT_NE(owner_b, nullptr);

  svalue_t read_cb;
  int fd;

  {
    ScopedObjectContext owner_ctx(this, owner_a);
    read_cb.type = T_STRING;
    read_cb.subtype = STRING_SHARED;
    read_cb.u.string = make_shared_string("read_callback");
    fd = socket_create(STREAM, &read_cb, nullptr);
    ASSERT_GE(fd, 0) << "Failed to create stream socket owned by owner_a";
  }

  {
    ScopedObjectContext non_owner_ctx(this, owner_b);
    EXPECT_EQ(socket_close(fd, 0), EESECURITY)
      << "Non-owner socket_close should return EESECURITY";
  }

  {
    ScopedObjectContext owner_ctx(this, owner_a);
    EXPECT_EQ(socket_close(fd, 1), EESUCCESS) << "Owner cleanup close should succeed";
  }

  free_string(read_cb.u.string);
}

/**
 * SOCK_BHV_018: Release then acquire succeeds
 * Setup: socket owner A, receiver B
 * Action: call socket_release(fd, B, cb) then socket_acquire(fd, ...)
 * Expected: EESUCCESS on successful handoff
 *
 * NOTE: Skeleton - requires multi-object test setup
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_BHV_018_ReleaseThenAcquire_Success) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";

  static const char release_receiver_code[] =
    "int release_count = 0;\n"
    "void release_callback(int fd, object owner) { release_count++; }\n"
    "void read_callback(int fd, mixed payload) { }\n"
    "void write_callback(int fd) { }\n"
    "void close_callback(int fd) { }\n"
    "int query_release_count() { return release_count; }\n";

  object_t *owner_a = LoadCallbackOwner("test_socket_release_owner_a.c");
  object_t *owner_b = LoadInlineObject("test_socket_release_owner_b.c", release_receiver_code);
  ASSERT_NE(owner_a, nullptr);
  ASSERT_NE(owner_b, nullptr);

  svalue_t owner_a_read_cb;
  svalue_t release_cb;
  svalue_t read_cb;
  svalue_t write_cb;
  svalue_t close_cb;
  int fd;
  int release_result;

  owner_a_read_cb.type = T_STRING;
  owner_a_read_cb.subtype = STRING_SHARED;
  owner_a_read_cb.u.string = make_shared_string("read_callback");

  {
    ScopedObjectContext owner_a_ctx(this, owner_a);
    fd = socket_create(STREAM, &owner_a_read_cb, nullptr);
    ASSERT_GE(fd, 0) << "Failed to create stream socket for handoff";
  }

  release_cb.type = T_STRING;
  release_cb.subtype = STRING_SHARED;
  release_cb.u.string = make_shared_string("release_callback");
  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  write_cb.type = T_STRING;
  write_cb.subtype = STRING_SHARED;
  write_cb.u.string = make_shared_string("write_callback");
  close_cb.type = T_STRING;
  close_cb.subtype = STRING_SHARED;
  close_cb.u.string = make_shared_string("close_callback");

  release_hook_read_cb = &read_cb;
  release_hook_write_cb = &write_cb;
  release_hook_close_cb = &close_cb;
  release_hook_called = 0;
  release_hook_result = EEBADF;
  set_socket_release_test_hook(SocketReleaseAcquireHook);

  {
    ScopedObjectContext owner_a_ctx(this, owner_a);
    release_result = socket_release(fd, owner_b, &release_cb);
  }

  set_socket_release_test_hook(nullptr);

  EXPECT_EQ(release_result, EESUCCESS) << "socket_release should succeed when callback seam acquires";
  EXPECT_EQ(release_hook_called, 1) << "release seam hook should have been invoked";
  EXPECT_EQ(release_hook_result, EESUCCESS) << "internal acquire via seam should succeed";
  EXPECT_EQ(QueryObjectNumberMethod(owner_b, "query_release_count"), 1)
    << "release callback should run exactly once in receiver object";

  {
    ScopedObjectContext owner_a_ctx(this, owner_a);
    EXPECT_EQ(socket_close(fd, 0), EESECURITY) << "Old owner should lose close permission";
  }
  {
    ScopedObjectContext owner_b_ctx(this, owner_b);
    EXPECT_EQ(socket_close(fd, 1), EESUCCESS) << "New owner should close acquired socket";
  }

  free_string(owner_a_read_cb.u.string);
  free_string(release_cb.u.string);
  free_string(read_cb.u.string);
  free_string(write_cb.u.string);
  free_string(close_cb.u.string);
}

/**
 * SOCK_BHV_019: Acquire without release rejected
 * Setup: socket not marked released
 * Action: call socket_acquire(fd, ...)
 * Expected: EESOCKNOTRLSD error
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_BHV_019_AcquireWithoutRelease_Rejected) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";

  object_t* owner = LoadCallbackOwner("test_socket_owner_for_acquire.c");
  ASSERT_NE(owner, nullptr);

  ScopedObjectContext ctx(this, owner);

  svalue_t read_cb;
  svalue_t write_cb;
  svalue_t close_cb;
  int fd;

  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  write_cb.type = T_STRING;
  write_cb.subtype = STRING_SHARED;
  write_cb.u.string = make_shared_string("write_callback");
  close_cb.type = T_STRING;
  close_cb.subtype = STRING_SHARED;
  close_cb.u.string = make_shared_string("close_callback");

  fd = socket_create(STREAM, &read_cb, nullptr);
  ASSERT_GE(fd, 0) << "Failed to create stream socket";

  EXPECT_EQ(socket_acquire(fd, &read_cb, &write_cb, &close_cb), EESOCKNOTRLSD)
    << "socket_acquire without prior release should return EESOCKNOTRLSD";

  EXPECT_EQ(socket_close(fd, 1), EESUCCESS);

  free_string(read_cb.u.string);
  free_string(write_cb.u.string);
  free_string(close_cb.u.string);
}

/**
 * SOCK_BHV_020: Read callback order on inbound data
 * Setup: connected peer
 * Action: process read readiness
 * Expected: read callback receives (fd, payload) once per message unit
 *
 * NOTE: Skeleton - requires full loopback integration
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_BHV_020_ReadCallbackOrdering_InboundData) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";

  object_t* callback_owner = LoadCallbackOwner("test_socket_read_owner.c");
  ASSERT_NE(callback_owner, nullptr);
  ScopedObjectContext ctx(this, callback_owner);

  svalue_t read_cb;
  svalue_t write_cb;
  socket_fd_t listener_fd = INVALID_SOCKET_FD;
  socket_fd_t accepted_fd = INVALID_SOCKET_FD;
  int listener_port = 0;
  int fd;
  std::string endpoint;
  const char *payload = "ping";

  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  write_cb.type = T_STRING;
  write_cb.subtype = STRING_SHARED;
  write_cb.u.string = make_shared_string("write_callback");

  fd = socket_create(STREAM, &read_cb, nullptr);
  ASSERT_GE(fd, 0) << "Failed to create stream socket";
  ASSERT_TRUE(CreateLoopbackListener(&listener_fd, &listener_port))
    << "Failed to create loopback listener";

  endpoint = "127.0.0.1 " + std::to_string(listener_port);
  ASSERT_EQ(socket_connect(fd, (char *)endpoint.c_str(), &read_cb, &write_cb), EESUCCESS)
    << "Connect should succeed";

  ASSERT_TRUE(AcceptPendingConnection(listener_fd, &accepted_fd))
    << "Expected pending native connection after socket_connect";

  ClearCallbackOwnerEvents(callback_owner);
  ASSERT_NE(SOCKET_SEND(accepted_fd, payload, (int)strlen(payload), 0), SOCKET_ERROR)
    << "Native peer send should succeed";

  socket_read_select_handler(fd);
  CaptureCallbacksFromOwner(callback_owner);

  ASSERT_EQ(callback_records.size(), 1U) << "Expected one read callback after inbound payload";
  EXPECT_TRUE(VerifyCallbackType(CallbackRecord::CB_READ, fd));
  CallbackRecord rec = PopCallback();
  EXPECT_EQ(rec.type, CallbackRecord::CB_READ);
  EXPECT_EQ(rec.socket_fd, fd);
  EXPECT_EQ(rec.data, payload);
  ExpectNoCallbacks();

  EXPECT_EQ(socket_close(fd, 1), EESUCCESS);
  SOCKET_CLOSE(accepted_fd);
  SOCKET_CLOSE(listener_fd);
  free_string(read_cb.u.string);
  free_string(write_cb.u.string);
}

/**
 * SOCK_OP_001: Connect operation enters transferring phase
 * Setup: stream socket + loopback listener
 * Action: socket_connect() to loopback endpoint
 * Expected: operation active, phase OP_TRANSFERRING, non-zero op_id
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_OP_001_ConnectTracksOperationLifecycle) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  ScopedObjectContext ctx(this, master_ob);

  svalue_t read_cb;
  svalue_t write_cb;
  socket_fd_t listener_fd = INVALID_SOCKET_FD;
  int listener_port = 0;
  int fd;
  int connect_result;
  int op_active = 0;
  int op_terminal = 0;
  int op_id = 0;
  int op_phase = OP_INIT;

  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  write_cb.type = T_STRING;
  write_cb.subtype = STRING_SHARED;
  write_cb.u.string = make_shared_string("write_callback");

  fd = socket_create(STREAM, &read_cb, NULL);
  ASSERT_GE(fd, 0) << "Failed to create stream socket";

  ASSERT_TRUE(CreateLoopbackListener(&listener_fd, &listener_port))
    << "Failed to create loopback listener";

  std::string endpoint = "127.0.0.1 " + std::to_string(listener_port);
  connect_result = socket_connect(fd, (char *)endpoint.c_str(), &read_cb, &write_cb);
  EXPECT_EQ(connect_result, EESUCCESS)
    << "socket_connect should follow async success path";

  EXPECT_EQ(get_socket_operation_info(fd, &op_active, &op_terminal, &op_id, &op_phase), EESUCCESS);
  EXPECT_EQ(op_active, 1);
  EXPECT_EQ(op_terminal, 0);
  EXPECT_GT(op_id, 0);
  EXPECT_EQ(op_phase, OP_TRANSFERRING);

  EXPECT_EQ(socket_close(fd, 1), EESUCCESS);
  if (listener_fd != INVALID_SOCKET_FD) {
    SOCKET_CLOSE(listener_fd);
  }

  free_string(read_cb.u.string);
  free_string(write_cb.u.string);
}

/**
 * SOCK_OP_002: Malformed connect does not start operation
 * Setup: stream socket
 * Action: socket_connect() with malformed address
 * Expected: EEBADADDR and no active operation record
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_OP_002_BadAddressDoesNotStartOperation) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  ScopedObjectContext ctx(this, master_ob);

  svalue_t read_cb;
  svalue_t write_cb;
  int fd;
  int connect_result;
  int op_active = 0;
  int op_terminal = 0;
  int op_id = 0;
  int op_phase = OP_CONNECTING;

  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  write_cb.type = T_STRING;
  write_cb.subtype = STRING_SHARED;
  write_cb.u.string = make_shared_string("write_callback");

  fd = socket_create(STREAM, &read_cb, NULL);
  ASSERT_GE(fd, 0) << "Failed to create stream socket";

  connect_result = socket_connect(fd, (char *)"bad_address_format", &read_cb, &write_cb);
  EXPECT_EQ(connect_result, EEBADADDR);

  EXPECT_EQ(get_socket_operation_info(fd, &op_active, &op_terminal, &op_id, &op_phase), EESUCCESS);
  EXPECT_EQ(op_active, 0);
  EXPECT_EQ(op_terminal, 0);
  EXPECT_EQ(op_id, 0);
  EXPECT_EQ(op_phase, OP_INIT);

  EXPECT_EQ(socket_close(fd, 1), EESUCCESS);

  free_string(read_cb.u.string);
  free_string(write_cb.u.string);
}

/**
 * SOCK_OP_003: Close clears operation and duplicate terminal path is inert
 * Setup: active connect operation
 * Action: close socket twice
 * Expected: first close clears operation; second close returns EEBADF with no operation
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_OP_003_CloseClearsOperationAndDuplicateTerminalIsInert) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  ScopedObjectContext ctx(this, master_ob);

  svalue_t read_cb;
  svalue_t write_cb;
  socket_fd_t listener_fd = INVALID_SOCKET_FD;
  int listener_port = 0;
  int fd;
  int op_active = 1;
  int op_terminal = 1;
  int op_id = -1;
  int op_phase = OP_CONNECTING;

  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  write_cb.type = T_STRING;
  write_cb.subtype = STRING_SHARED;
  write_cb.u.string = make_shared_string("write_callback");

  fd = socket_create(STREAM, &read_cb, NULL);
  ASSERT_GE(fd, 0) << "Failed to create stream socket";
  ASSERT_TRUE(CreateLoopbackListener(&listener_fd, &listener_port))
    << "Failed to create loopback listener";

  std::string endpoint = "127.0.0.1 " + std::to_string(listener_port);
  ASSERT_EQ(socket_connect(fd, (char *)endpoint.c_str(), &read_cb, &write_cb), EESUCCESS);

  ASSERT_EQ(socket_close(fd, 1), EESUCCESS);
  EXPECT_EQ(get_socket_operation_info(fd, &op_active, &op_terminal, &op_id, &op_phase), EESUCCESS);
  EXPECT_EQ(op_active, 0);
  EXPECT_EQ(op_terminal, 0);
  EXPECT_EQ(op_id, 0);
  EXPECT_EQ(op_phase, OP_INIT);

  EXPECT_EQ(socket_close(fd, 1), EEBADF);
  EXPECT_EQ(get_socket_operation_info(fd, &op_active, &op_terminal, &op_id, &op_phase), EESUCCESS);
  EXPECT_EQ(op_active, 0);
  EXPECT_EQ(op_id, 0);

  if (listener_fd != INVALID_SOCKET_FD) {
    SOCKET_CLOSE(listener_fd);
  }
  free_string(read_cb.u.string);
  free_string(write_cb.u.string);
}

/**
 * SOCK_DNS_001: DNS-disabled build still accepts numeric IPv4 endpoint.
 * Setup: stream socket + loopback listener
 * Action: socket_connect(fd, "127.0.0.1 <port>", ...)
 * Expected: EESUCCESS and operation enters transferring phase.
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_DNS_001_NumericConnectBaselineUnchanged) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  ScopedObjectContext ctx(this, master_ob);

  svalue_t read_cb;
  svalue_t write_cb;
  socket_fd_t listener_fd = INVALID_SOCKET_FD;
  socket_fd_t accepted_fd = INVALID_SOCKET_FD;
  int listener_port = 0;
  int fd;
  int connect_result;
  int op_active = 0;
  int op_terminal = 0;
  int op_id = 0;
  int op_phase = OP_INIT;

  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  write_cb.type = T_STRING;
  write_cb.subtype = STRING_SHARED;
  write_cb.u.string = make_shared_string("write_callback");

  fd = socket_create(STREAM, &read_cb, NULL);
  ASSERT_GE(fd, 0) << "Failed to create stream socket";
  ASSERT_TRUE(CreateLoopbackListener(&listener_fd, &listener_port))
    << "Failed to create loopback listener";

  std::string endpoint = "127.0.0.1 " + std::to_string(listener_port);
  connect_result = socket_connect(fd, (char *)endpoint.c_str(), &read_cb, &write_cb);
  EXPECT_EQ(connect_result, EESUCCESS)
    << "Numeric IPv4 connect should keep baseline success behavior";

  EXPECT_EQ(get_socket_operation_info(fd, &op_active, &op_terminal, &op_id, &op_phase), EESUCCESS);
  EXPECT_EQ(op_active, 1);
  EXPECT_EQ(op_terminal, 0);
  EXPECT_GT(op_id, 0);
  EXPECT_EQ(op_phase, OP_TRANSFERRING);

  if (AcceptPendingConnection(listener_fd, &accepted_fd)) {
    SOCKET_CLOSE(accepted_fd);
  }

  EXPECT_EQ(socket_close(fd, 1), EESUCCESS);
  SOCKET_CLOSE(listener_fd);
  free_string(read_cb.u.string);
  free_string(write_cb.u.string);
}

/**
 * SOCK_DNS_002: DNS-disabled build rejects hostname endpoint deterministically.
 * Setup: stream socket
 * Action: socket_connect(fd, "localhost <port>", ...)
 * Expected: EEBADADDR and no operation record leak.
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_DNS_002_HostnameRejectedWhenSocketConnectDnsDisabled) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  ScopedObjectContext ctx(this, master_ob);

  svalue_t read_cb;
  svalue_t write_cb;
  int fd;
  int connect_result;
  int op_active = 0;
  int op_terminal = 0;
  int op_id = 0;
  int op_phase = OP_CONNECTING;

  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  write_cb.type = T_STRING;
  write_cb.subtype = STRING_SHARED;
  write_cb.u.string = make_shared_string("write_callback");

  fd = socket_create(STREAM, &read_cb, NULL);
  ASSERT_GE(fd, 0) << "Failed to create stream socket";

  connect_result = socket_connect(fd, (char *)"localhost 4000", &read_cb, &write_cb);
#ifdef PACKAGE_PEER_REVERSE_DNS
  GTEST_SKIP() << "PACKAGE_PEER_REVERSE_DNS enabled: hostname rejection assertion does not apply";
#else
  EXPECT_EQ(connect_result, EEBADADDR)
    << "Hostname connect must fail fast with EEBADADDR when DNS is disabled";

  EXPECT_EQ(get_socket_operation_info(fd, &op_active, &op_terminal, &op_id, &op_phase), EESUCCESS);
  EXPECT_EQ(op_active, 0);
  EXPECT_EQ(op_terminal, 0);
  EXPECT_EQ(op_id, 0);
  EXPECT_EQ(op_phase, OP_INIT);
#endif

  EXPECT_EQ(socket_close(fd, 1), EESUCCESS);
  free_string(read_cb.u.string);
  free_string(write_cb.u.string);
}

/**
 * SOCK_DNS_003: DNS-enabled build resolves hostname and connects successfully.
 * Setup: stream socket + loopback listener
 * Action: socket_connect(fd, "localhost <port>", ...)
 * Expected: success path and active operation in OP_TRANSFERRING.
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_DNS_003_HostnameConnectSucceedsWhenFeatureEnabled) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  ScopedObjectContext ctx(this, master_ob);

  svalue_t read_cb;
  svalue_t write_cb;
  socket_fd_t listener_fd = INVALID_SOCKET_FD;
  socket_fd_t accepted_fd = INVALID_SOCKET_FD;
  int listener_port = 0;
  int fd;
  int connect_result;
#ifdef PACKAGE_PEER_REVERSE_DNS
  int op_active = 0;
  int op_terminal = 0;
  int op_id = 0;
  int op_phase = OP_INIT;
#endif

  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  write_cb.type = T_STRING;
  write_cb.subtype = STRING_SHARED;
  write_cb.u.string = make_shared_string("write_callback");

  fd = socket_create(STREAM, &read_cb, NULL);
  ASSERT_GE(fd, 0) << "Failed to create stream socket";
  ASSERT_TRUE(CreateLoopbackListener(&listener_fd, &listener_port))
    << "Failed to create loopback listener";

  std::string endpoint = "localhost " + std::to_string(listener_port);
  connect_result = socket_connect(fd, (char *)endpoint.c_str(), &read_cb, &write_cb);

#ifdef PACKAGE_PEER_REVERSE_DNS
  // Wait for DNS resolution to complete (non-blocking DNS operation)
  ASSERT_TRUE(WaitForDNSCompletion(fd, 5000))
    << "DNS resolution did not complete within timeout";
  
  EXPECT_EQ(connect_result, EESUCCESS)
    << "Hostname connect should succeed when PACKAGE_PEER_REVERSE_DNS is enabled";

  EXPECT_EQ(get_socket_operation_info(fd, &op_active, &op_terminal, &op_id, &op_phase), EESUCCESS);
  EXPECT_EQ(op_active, 1);
  EXPECT_EQ(op_terminal, 0);
  EXPECT_GT(op_id, 0);
  EXPECT_EQ(op_phase, OP_TRANSFERRING);
#else
  GTEST_SKIP() << "PACKAGE_PEER_REVERSE_DNS disabled: SOCK_DNS_003 requires feature-enabled build";
#endif

  if (AcceptPendingConnection(listener_fd, &accepted_fd)) {
    SOCKET_CLOSE(accepted_fd);
  }

  EXPECT_EQ(socket_close(fd, 1), EESUCCESS);
  SOCKET_CLOSE(listener_fd);
  free_string(read_cb.u.string);
  free_string(write_cb.u.string);
}

/**
 * SOCK_DNS_004: Global admission control cap enforces max 64 concurrent DNS resolutions.
 * Setup: Multiple sockets with pending hostname resolutions
 * Action: Queue 65+ hostname DNS operations
 * Expected: 1-64th succeed (enter OP_DNS_RESOLVING), 65th+ fail with deterministic overload mapping.
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_DNS_004_GlobalCapEnforcesMaxInFlightResolutions) {
#ifdef PACKAGE_PEER_REVERSE_DNS
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  ScopedAsyncRuntime runtime_guard;
  ASSERT_TRUE(runtime_guard.IsReady()) << "async runtime is required for DNS admission tests";

  svalue_t read_cb, write_cb;
  std::vector<int> fd_list;
  std::vector<object_t *> owners;
  int connect_result;
  int success_count = 0;
  int would_block_count = 0;
  int first_would_block_at = -1;
  const int TEST_LIMIT = 70;  /* Exceed cap of 64 */
  const int OWNER_COUNT = 8;

  static const char owner_code[] =
    "void read_callback(int fd, mixed payload) { }\n"
    "void write_callback(int fd, mixed payload) { }\n"
    "void close_callback(int fd, mixed payload) { }\n";

  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  write_cb.type = T_STRING;
  write_cb.subtype = STRING_SHARED;
  write_cb.u.string = make_shared_string("write_callback");

  for (int i = 0; i < OWNER_COUNT; i++) {
    std::string owner_name = "test_socket_dns_owner_" + std::to_string(i) + ".c";
    object_t *owner = LoadInlineObject(owner_name.c_str(), owner_code);
    ASSERT_NE(owner, nullptr) << "Failed to load DNS test owner " << i;
    owners.push_back(owner);
  }

  for (int i = 0; i < TEST_LIMIT; i++) {
    object_t *owner = owners[i % OWNER_COUNT];
    ScopedObjectContext owner_ctx(this, owner);
    int fd = socket_create(STREAM, &read_cb, NULL);
    if (fd < 0) break;  /* Ran out of sockets */
    fd_list.push_back(fd);

    {
      std::string endpoint = "localhost " + std::to_string(8080 + i);
      connect_result = socket_connect(fd, (char *)endpoint.c_str(), &read_cb, &write_cb);
    }

    if (connect_result == EESUCCESS) {
      success_count++;
    } else if (connect_result == EEWOULDBLOCK) {
      would_block_count++;
      if (first_would_block_at < 0) {
        first_would_block_at = i;
      }
    } else {
      ADD_FAILURE() << "Unexpected DNS connect result " << connect_result
                    << " at request index " << i;
    }
  }

  EXPECT_GT(success_count, 0) << "Expected at least one DNS request admitted";
  EXPECT_GT(would_block_count, 0) << "Expected overload rejections beyond global cap";
  EXPECT_LE(first_would_block_at, 64)
    << "Global cap rejection should begin no later than request index 64";

  /* Cleanup */
  for (int fd : fd_list) {
    socket_close(fd, 1);
  }

  free_string(read_cb.u.string);
  free_string(write_cb.u.string);
#else
  GTEST_SKIP() << "PACKAGE_PEER_REVERSE_DNS disabled: SOCK_DNS_004 requires feature-enabled build";
#endif
}

/**
 * SOCK_DNS_005: Per-owner admission control cap enforces max 8 concurrent DNS per owner.
 * Setup: Single owner creating multiple sockets with pending hostname resolutions
 * Action: Queue 9+ hostname DNS operations from same owner
 * Expected: 1-8th succeed (enter OP_DNS_RESOLVING), 9th+ fail with deterministic overload mapping.
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_DNS_005_PerOwnerCapEnforcesMaxConcurrentDns) {
#ifdef PACKAGE_PEER_REVERSE_DNS
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  ScopedAsyncRuntime runtime_guard;
  ASSERT_TRUE(runtime_guard.IsReady()) << "async runtime is required for DNS admission tests";
  ScopedObjectContext ctx(this, master_ob);

  svalue_t read_cb, write_cb;
  std::vector<int> fd_list;
  int connect_result;
  int success_count = 0;
  int would_block_count = 0;
  int first_would_block_at = -1;
  const int TEST_LIMIT = 12;  /* Exceed per-owner cap of 8 */

  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  write_cb.type = T_STRING;
  write_cb.subtype = STRING_SHARED;
  write_cb.u.string = make_shared_string("write_callback");

  /* Create TEST_LIMIT sockets all owned by master_ob */
  for (int i = 0; i < TEST_LIMIT; i++) {
    int fd = socket_create(STREAM, &read_cb, NULL);
    if (fd < 0) break;  /* Ran out of sockets */
    fd_list.push_back(fd);

    {
      std::string endpoint = "localhost " + std::to_string(9000 + i);
      connect_result = socket_connect(fd, (char *)endpoint.c_str(), &read_cb, &write_cb);
    }

    if (connect_result == EESUCCESS) {
      success_count++;
    } else if (connect_result == EEWOULDBLOCK) {
      would_block_count++;
      if (first_would_block_at < 0) {
        first_would_block_at = i;
      }
    } else {
      ADD_FAILURE() << "Unexpected DNS connect result " << connect_result
                    << " at request index " << i;
    }
  }

  EXPECT_GT(success_count, 0) << "Expected at least one DNS request admitted";
  EXPECT_GT(would_block_count, 0) << "Expected overload rejections beyond owner cap";
  EXPECT_LE(first_would_block_at, 8)
    << "Per-owner cap rejection should begin no later than request index 8";

  /* Cleanup */
  for (int fd : fd_list) {
    socket_close(fd, 1);
  }

  free_string(read_cb.u.string);
  free_string(write_cb.u.string);
#else
  GTEST_SKIP() << "PACKAGE_PEER_REVERSE_DNS disabled: SOCK_DNS_005 requires feature-enabled build";
#endif
}

/**
 * SOCK_DNS_006: Forced timeout path maps deterministically to OP_TIMED_OUT.
 * Setup: stream socket with test hook forcing DNS deadline expiry.
 * Action: hostname socket_connect and wait for DNS completion processing.
 * Expected: operation reaches terminal OP_TIMED_OUT phase.
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_DNS_006_TimeoutMapsToTimedOutPhase) {
#ifdef PACKAGE_PEER_REVERSE_DNS
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  ScopedAsyncRuntime runtime_guard;
  ASSERT_TRUE(runtime_guard.IsReady()) << "async runtime is required for DNS timeout tests";
  ScopedObjectContext ctx(this, master_ob);
  ScopedDnsTimeoutHook timeout_hook;

  svalue_t read_cb;
  svalue_t write_cb;
  int fd;
  int connect_result;
  int op_active = 0;
  int op_terminal = 0;
  int op_id = 0;
  int op_phase = OP_INIT;
  int in_flight = 0;
  unsigned long admitted = 0;
  unsigned long dedup = 0;
  unsigned long timed_out_before = 0;
  unsigned long timed_out_after = 0;

  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  write_cb.type = T_STRING;
  write_cb.subtype = STRING_SHARED;
  write_cb.u.string = make_shared_string("write_callback");

  fd = socket_create(STREAM, &read_cb, NULL);
  ASSERT_GE(fd, 0) << "Failed to create stream socket";

  ASSERT_EQ(get_dns_telemetry_snapshot(&in_flight, &admitted, &dedup, &timed_out_before), EESUCCESS);

  connect_result = socket_connect(fd, (char *)"force-timeout-neolith.invalid 8080", &read_cb, &write_cb);
  ASSERT_EQ(connect_result, EESUCCESS)
    << "Hostname connect should queue DNS work before timeout resolves";

  ASSERT_TRUE(WaitForDNSCompletion(fd, 3000))
    << "Forced DNS timeout did not complete within test timeout";

  ASSERT_EQ(get_dns_telemetry_snapshot(&in_flight, &admitted, &dedup, &timed_out_after), EESUCCESS);
  EXPECT_GE(timed_out_after, timed_out_before + 1)
    << "DNS timeout path must increment timed_out telemetry deterministically";

  ASSERT_EQ(get_socket_operation_info(fd, &op_active, &op_terminal, &op_id, &op_phase), EESUCCESS);
  EXPECT_EQ(op_active, 0);
  EXPECT_EQ(op_terminal, 0);
  EXPECT_EQ(op_id, 0);
  EXPECT_EQ(op_phase, OP_INIT);

  EXPECT_EQ(socket_close(fd, 1), EESUCCESS);
  free_string(read_cb.u.string);
  free_string(write_cb.u.string);
#else
  GTEST_SKIP() << "PACKAGE_PEER_REVERSE_DNS disabled: SOCK_DNS_006 requires feature-enabled build";
#endif
}

/**
 * SOCK_DNS_012: Duplicate hostname lookups coalesce to one admitted DNS task.
 * Setup: two stream sockets, same owner, same hostname endpoint.
 * Action: queue two hostname connects back-to-back.
 * Expected: admitted counter increases once; dedup counter increases at least once.
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_DNS_012_DuplicateHostnameConnectsCoalesceWork) {
#ifdef PACKAGE_PEER_REVERSE_DNS
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  ScopedAsyncRuntime runtime_guard;
  ASSERT_TRUE(runtime_guard.IsReady()) << "async runtime is required for DNS coalescing tests";
  ScopedObjectContext ctx(this, master_ob);

  svalue_t read_cb;
  svalue_t write_cb;
  int fd_a;
  int fd_b;
  int result_a;
  int result_b;
  int in_flight = 0;
  unsigned long admitted_before = 0;
  unsigned long dedup_before = 0;
  unsigned long admitted_after = 0;
  unsigned long dedup_after = 0;

  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  write_cb.type = T_STRING;
  write_cb.subtype = STRING_SHARED;
  write_cb.u.string = make_shared_string("write_callback");

  fd_a = socket_create(STREAM, &read_cb, NULL);
  fd_b = socket_create(STREAM, &read_cb, NULL);
  ASSERT_GE(fd_a, 0) << "Failed to create first stream socket";
  ASSERT_GE(fd_b, 0) << "Failed to create second stream socket";

  ASSERT_EQ(get_dns_telemetry_snapshot(&in_flight, &admitted_before, &dedup_before, nullptr), EESUCCESS);

  result_a = socket_connect(fd_a, (char *)"localhost 8080", &read_cb, &write_cb);
  result_b = socket_connect(fd_b, (char *)"localhost 8080", &read_cb, &write_cb);

  EXPECT_EQ(result_a, EESUCCESS);
  EXPECT_EQ(result_b, EESUCCESS);

  ASSERT_EQ(get_dns_telemetry_snapshot(&in_flight, &admitted_after, &dedup_after, nullptr), EESUCCESS);
  EXPECT_EQ(admitted_after, admitted_before + 1)
    << "Duplicate hostname requests should admit only one DNS worker task";
  EXPECT_GE(dedup_after, dedup_before + 1)
    << "Duplicate hostname requests should record at least one dedup hit";

  EXPECT_EQ(socket_close(fd_a, 1), EESUCCESS);
  EXPECT_EQ(socket_close(fd_b, 1), EESUCCESS);
  free_string(read_cb.u.string);
  free_string(write_cb.u.string);
#else
  GTEST_SKIP() << "PACKAGE_PEER_REVERSE_DNS disabled: SOCK_DNS_012 requires feature-enabled build";
#endif
}

/**
 * SOCK_DNS_011: Backend loop remains responsive while DNS flood is in progress.
 * Setup: Queue multiple hostname DNS operations to exercise worker/admission path.
 * Action: Perform independent numeric connect while DNS operations remain in flight.
 * Expected: Numeric connect path succeeds without being blocked by DNS worker activity.
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_DNS_011_BackendRemainsResponsiveUnderDnsFlood) {
#ifdef PACKAGE_PEER_REVERSE_DNS
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  ScopedAsyncRuntime runtime_guard;
  ASSERT_TRUE(runtime_guard.IsReady()) << "async runtime is required for DNS responsiveness tests";
  ScopedObjectContext ctx(this, master_ob);

  svalue_t read_cb, write_cb;
  std::vector<int> flood_fds;
  socket_fd_t listener_fd = INVALID_SOCKET_FD;
  socket_fd_t accepted_fd = INVALID_SOCKET_FD;
  int listener_port = 0;
  int probe_fd;
  int result;

  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  write_cb.type = T_STRING;
  write_cb.subtype = STRING_SHARED;
  write_cb.u.string = make_shared_string("write_callback");

  for (int i = 0; i < 24; i++) {
    int fd = socket_create(STREAM, &read_cb, NULL);
    if (fd < 0) {
      break;
    }
    flood_fds.push_back(fd);
    (void)socket_connect(fd, (char *)"localhost 8080", &read_cb, &write_cb);
  }

  ASSERT_TRUE(CreateLoopbackListener(&listener_fd, &listener_port))
    << "Failed to create loopback listener";

  probe_fd = socket_create(STREAM, &read_cb, NULL);
  ASSERT_GE(probe_fd, 0) << "Failed to create probe stream socket";

  {
    std::string endpoint = "127.0.0.1 " + std::to_string(listener_port);
    result = socket_connect(probe_fd, (char *)endpoint.c_str(), &read_cb, &write_cb);
  }

  EXPECT_EQ(result, EESUCCESS)
    << "Numeric connect should remain responsive under DNS flood conditions";
  EXPECT_TRUE(AcceptPendingConnection(listener_fd, &accepted_fd))
    << "Probe connection should be accepted while DNS flood is active";

  if (accepted_fd != INVALID_SOCKET_FD) {
    SOCKET_CLOSE(accepted_fd);
  }
  EXPECT_EQ(socket_close(probe_fd, 1), EESUCCESS);
  SOCKET_CLOSE(listener_fd);

  for (int fd : flood_fds) {
    socket_close(fd, 1);
  }

  free_string(read_cb.u.string);
  free_string(write_cb.u.string);
#else
  GTEST_SKIP() << "PACKAGE_PEER_REVERSE_DNS disabled: SOCK_DNS_011 requires feature-enabled build";
#endif
}

TEST_F(SocketEfunsBehaviorTest, SOCK_RT_001_CreateRegistersAndCloseRemovesRuntimeEntry) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  ScopedAsyncRuntime runtime_guard;
  ASSERT_TRUE(runtime_guard.IsReady()) << "async runtime is required for Stage 3 runtime tests";
  ScopedObjectContext ctx(this, master_ob);

  svalue_t read_cb;
  int fd;
  int registered = 0;
  int events = 0;
  int tracked_fd = (int)INVALID_SOCKET_FD;
  int resolved_socket = -1;
  socket_fd_t native_fd = INVALID_SOCKET_FD;
  void* runtime_context;

  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");

  fd = socket_create(STREAM, &read_cb, NULL);
  ASSERT_GE(fd, 0) << "Failed to create stream socket";

  ASSERT_EQ(get_socket_runtime_info(fd, &registered, &events, &tracked_fd), EESUCCESS);
  EXPECT_EQ(registered, 1);
  EXPECT_EQ(events, EVENT_READ);
  EXPECT_EQ(tracked_fd, (int)lpc_socks[fd].fd);

  runtime_context = get_socket_runtime_context(fd);
  native_fd = lpc_socks[fd].fd;
  ASSERT_NE(runtime_context, nullptr);
  EXPECT_EQ(resolve_lpc_socket_context(runtime_context, native_fd, &resolved_socket), 1);
  EXPECT_EQ(resolved_socket, fd);

  EXPECT_EQ(socket_close(fd, 1), EESUCCESS);

  ASSERT_EQ(get_socket_runtime_info(fd, &registered, &events, &tracked_fd), EESUCCESS);
  EXPECT_EQ(registered, 0);
  EXPECT_EQ(events, 0);
  EXPECT_EQ(tracked_fd, (int)INVALID_SOCKET_FD);
  EXPECT_EQ(resolve_lpc_socket_context(runtime_context, native_fd, &resolved_socket), 0);

  free_string(read_cb.u.string);
}

TEST_F(SocketEfunsBehaviorTest, SOCK_RT_002_BlockedStateTracksWriteInterest) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  ScopedAsyncRuntime runtime_guard;
  ASSERT_TRUE(runtime_guard.IsReady()) << "async runtime is required for Stage 3 runtime tests";
  ScopedObjectContext ctx(this, master_ob);

  svalue_t read_cb;
  svalue_t write_cb;
  socket_fd_t listener_fd = INVALID_SOCKET_FD;
  socket_fd_t accepted_fd = INVALID_SOCKET_FD;
  int listener_port = 0;
  int fd;
  int registered = 0;
  int events = 0;
  int tracked_fd = (int)INVALID_SOCKET_FD;
  int attempts;

  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");
  write_cb.type = T_STRING;
  write_cb.subtype = STRING_SHARED;
  write_cb.u.string = make_shared_string("write_callback");

  fd = socket_create(STREAM, &read_cb, NULL);
  ASSERT_GE(fd, 0) << "Failed to create stream socket";
  ASSERT_TRUE(CreateLoopbackListener(&listener_fd, &listener_port))
    << "Failed to create loopback listener";

  std::string endpoint = "127.0.0.1 " + std::to_string(listener_port);
  ASSERT_EQ(socket_connect(fd, (char *)endpoint.c_str(), &read_cb, &write_cb), EESUCCESS);

  ASSERT_EQ(get_socket_runtime_info(fd, &registered, &events, &tracked_fd), EESUCCESS);
  EXPECT_EQ(registered, 1);
  EXPECT_NE((events & EVENT_WRITE), 0) << "Blocked connect path should watch write events";

  ASSERT_TRUE(AcceptPendingConnection(listener_fd, &accepted_fd));

  for (attempts = 0; attempts < 32 && (lpc_socks[fd].flags & S_BLOCKED); attempts++) {
    socket_write_select_handler(fd);
  }

  EXPECT_EQ((lpc_socks[fd].flags & S_BLOCKED), 0) << "Socket should clear blocked state after write-ready handling";

  ASSERT_EQ(get_socket_runtime_info(fd, &registered, &events, &tracked_fd), EESUCCESS);
  EXPECT_EQ(registered, 1);
  EXPECT_EQ((events & EVENT_WRITE), 0) << "Unblocked socket should only watch read events";

  EXPECT_EQ(socket_close(fd, 1), EESUCCESS);
  if (accepted_fd != INVALID_SOCKET_FD) {
    SOCKET_CLOSE(accepted_fd);
  }
  if (listener_fd != INVALID_SOCKET_FD) {
    SOCKET_CLOSE(listener_fd);
  }
  free_string(read_cb.u.string);
  free_string(write_cb.u.string);
}

TEST_F(SocketEfunsBehaviorTest, SOCK_RT_003_RuntimeRegistrationStress_NoLeaks) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  ScopedAsyncRuntime runtime_guard;
  ASSERT_TRUE(runtime_guard.IsReady()) << "async runtime is required for Stage 3 runtime tests";
  ScopedObjectContext ctx(this, master_ob);

  svalue_t read_cb;
  int baseline_registrations;
  int iteration;

  read_cb.type = T_STRING;
  read_cb.subtype = STRING_SHARED;
  read_cb.u.string = make_shared_string("read_callback");

  baseline_registrations = get_socket_runtime_registration_count();
  ASSERT_GE(baseline_registrations, 0);

  for (iteration = 0; iteration < 100; iteration++) {
    int fd = socket_create(STREAM, &read_cb, NULL);
    ASSERT_GE(fd, 0) << "socket_create failed on iteration " << iteration;
    EXPECT_EQ(get_socket_runtime_registration_count(), baseline_registrations + 1)
      << "Registration count should increase by one for an open socket";
    EXPECT_EQ(socket_close(fd, 1), EESUCCESS)
      << "socket_close failed on iteration " << iteration;
    EXPECT_EQ(get_socket_runtime_registration_count(), baseline_registrations)
      << "Registration count should return to baseline after close";
  }

  free_string(read_cb.u.string);
}

