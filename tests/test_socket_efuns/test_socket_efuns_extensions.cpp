#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "fixtures.hpp"

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

extern "C" void DelayLocalhostResolverHookExtensions(const char *query,
                                             unsigned int *delay_ms_out,
                                             const char **effective_query_out) {
  if (delay_ms_out != nullptr) {
    *delay_ms_out = 0;
  }
  if (effective_query_out != nullptr) {
    *effective_query_out = nullptr;
  }

  if (query == nullptr) {
    return;
  }

  if (strcmp(query, "delay-localhost") == 0) {
    if (delay_ms_out != nullptr) {
      *delay_ms_out = 1200;
    }
    if (effective_query_out != nullptr) {
      *effective_query_out = "localhost";
    }
  }
}

class ScopedResolverLookupHook {
public:
  explicit ScopedResolverLookupHook(void (*hook)(const char *, unsigned int *, const char **)) {
    addr_resolver_set_lookup_test_hook(hook);
  }

  ~ScopedResolverLookupHook() {
    addr_resolver_set_lookup_test_hook(nullptr);
  }
};

bool WaitForSocketPhase(int socket_id, int expected_phase, int timeout_ms) {
  int elapsed = 0;
  const int sleep_step = 10;

  while (elapsed < timeout_ms) {
    int op_active = 0;
    int op_terminal = 0;
    int op_id = 0;
    int op_phase = OP_INIT;

    handle_dns_completions();

    if (get_socket_operation_info(socket_id, &op_active, &op_terminal,
                                  &op_id, &op_phase) == EESUCCESS &&
        op_phase == expected_phase) {
      return true;
    }

#ifdef WINSOCK
    Sleep(sleep_step);
#else
    usleep(sleep_step * 1000);
#endif
    elapsed += sleep_step;
  }

  return false;
}

object_t *LoadInlineObject(const char *name, const char *code) {
  object_t *saved_current;
  object_t *obj;

  saved_current = current_object;
  current_object = master_ob;
  obj = load_object(name, code);
  current_object = saved_current;
  return obj;
}

} // namespace

// Extension tests for socket operation tracking, DNS behavior, and runtime hooks.
// Kept separate from SOCK_BHV_* behavior-lockdown tests by design.

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

  lpc::svalue_view::from(&read_cb).set_shared_string(make_shared_string("read_callback", NULL));
  lpc::svalue_view::from(&write_cb).set_shared_string(make_shared_string("write_callback", NULL));

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

  free_string_svalue(&read_cb);
  free_string_svalue(&write_cb);
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

  lpc::svalue_view::from(&read_cb).set_shared_string(make_shared_string("read_callback", NULL));
  lpc::svalue_view::from(&write_cb).set_shared_string(make_shared_string("write_callback", NULL));

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

  free_string_svalue(&read_cb);
  free_string_svalue(&write_cb);
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

  lpc::svalue_view::from(&read_cb).set_shared_string(make_shared_string("read_callback", NULL));
  lpc::svalue_view::from(&write_cb).set_shared_string(make_shared_string("write_callback", NULL));

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
  free_string_svalue(&read_cb);
  free_string_svalue(&write_cb);
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

  lpc::svalue_view::from(&read_cb).set_shared_string(make_shared_string("read_callback", NULL));
  lpc::svalue_view::from(&write_cb).set_shared_string(make_shared_string("write_callback", NULL));

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
  free_string_svalue(&read_cb);
  free_string_svalue(&write_cb);
}

/**
 * SOCK_DNS_002: malformed hostname endpoint is rejected deterministically.
 * Setup: stream socket
 * Action: socket_connect(fd, "localhost", ...)
 * Expected: EEBADADDR and no operation record leak.
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_DNS_002_MalformedHostnameEndpointRejected) {
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

  lpc::svalue_view::from(&read_cb).set_shared_string(make_shared_string("read_callback", NULL));
  lpc::svalue_view::from(&write_cb).set_shared_string(make_shared_string("write_callback", NULL));

  fd = socket_create(STREAM, &read_cb, NULL);
  ASSERT_GE(fd, 0) << "Failed to create stream socket";

  connect_result = socket_connect(fd, (char *)"localhost", &read_cb, &write_cb);
  EXPECT_EQ(connect_result, EEBADADDR)
    << "Malformed hostname endpoint without port must fail fast with EEBADADDR";

  EXPECT_EQ(get_socket_operation_info(fd, &op_active, &op_terminal, &op_id, &op_phase), EESUCCESS);
  EXPECT_EQ(op_active, 0);
  EXPECT_EQ(op_terminal, 0);
  EXPECT_EQ(op_id, 0);
  EXPECT_EQ(op_phase, OP_INIT);

  EXPECT_EQ(socket_close(fd, 1), EESUCCESS);
  free_string_svalue(&read_cb);
  free_string_svalue(&write_cb);
}

/**
 * SOCK_DNS_003: hostname resolves and connects successfully.
 * Setup: stream socket + loopback listener
 * Action: socket_connect(fd, "localhost <port>", ...)
 * Expected: success path and active operation in OP_TRANSFERRING.
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_DNS_003_HostnameConnectSucceeds) {
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

  lpc::svalue_view::from(&read_cb).set_shared_string(make_shared_string("read_callback", NULL));
  lpc::svalue_view::from(&write_cb).set_shared_string(make_shared_string("write_callback", NULL));

  fd = socket_create(STREAM, &read_cb, NULL);
  ASSERT_GE(fd, 0) << "Failed to create stream socket";
  ASSERT_TRUE(CreateLoopbackListener(&listener_fd, &listener_port))
    << "Failed to create loopback listener";

  std::string endpoint = "localhost " + std::to_string(listener_port);
  connect_result = socket_connect(fd, (char *)endpoint.c_str(), &read_cb, &write_cb);

  // Wait for DNS resolution to complete (non-blocking DNS operation)
  ASSERT_TRUE(WaitForDNSCompletion(fd, 5000))
    << "DNS resolution did not complete within timeout";
  
  EXPECT_EQ(connect_result, EESUCCESS)
    << "Hostname connect should succeed";

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
  free_string_svalue(&read_cb);
  free_string_svalue(&write_cb);
}

/**
 * SOCK_DNS_004: Global admission control cap enforces max 64 concurrent DNS resolutions.
 * Setup: Multiple sockets with pending hostname resolutions
 * Action: Queue 65+ hostname DNS operations
 * Expected: 1-64th succeed (enter OP_DNS_RESOLVING), 65th+ fail with deterministic overload mapping.
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_DNS_004_GlobalCapEnforcesMaxInFlightResolutions) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";

  svalue_t read_cb, write_cb;
  std::vector<int> fd_list;
  std::vector<object_t *> owners;
  int connect_result;
  int success_count = 0;
  int resolver_busy_count = 0;
  int first_resolver_busy_at = -1;
  const int TEST_LIMIT = 70;  /* Exceed cap of 64 */
  const int OWNER_COUNT = 8;

  static const char owner_code[] =
    "void read_callback(int fd, mixed payload) { }\n"
    "void write_callback(int fd, mixed payload) { }\n"
    "void close_callback(int fd, mixed payload) { }\n";

  lpc::svalue_view::from(&read_cb).set_shared_string(make_shared_string("read_callback", NULL));
  lpc::svalue_view::from(&write_cb).set_shared_string(make_shared_string("write_callback", NULL));

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
    } else if (connect_result == EERESOLVERBUSY) {
      resolver_busy_count++;
      if (first_resolver_busy_at < 0) {
        first_resolver_busy_at = i;
      }
    } else {
      ADD_FAILURE() << "Unexpected DNS connect result " << connect_result
                    << " at request index " << i;
    }
  }

  EXPECT_GT(success_count, 0) << "Expected at least one DNS request admitted";
  EXPECT_GT(resolver_busy_count, 0) << "Expected overload rejections beyond global cap";
  EXPECT_LE(first_resolver_busy_at, 64)
    << "Global cap rejection should begin no later than request index 64";

  /* Cleanup */
  for (int fd : fd_list) {
    socket_close(fd, 1);
  }

  free_string_svalue(&read_cb);
  free_string_svalue(&write_cb);
}

/**
 * SOCK_DNS_005: Driver socket_connect follows shared-resolver forward class quota.
 * Setup: Single owner creating multiple sockets with pending hostname resolutions.
 * Action: Queue multiple hostname DNS operations from same owner through socket_connect.
 * Expected: Requests are either admitted or rejected with EERESOLVERBUSY by centralized resolver policy.
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_DNS_005_PerOwnerCapEnforcesMaxConcurrentDns) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  ScopedObjectContext ctx(this, master_ob);

  svalue_t read_cb, write_cb;
  std::vector<int> fd_list;
  int connect_result;
  int success_count = 0;
  int resolver_busy_count = 0;
  const int TEST_LIMIT = 12;

  lpc::svalue_view::from(&read_cb).set_shared_string(make_shared_string("read_callback", NULL));
  lpc::svalue_view::from(&write_cb).set_shared_string(make_shared_string("write_callback", NULL));

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
    } else if (connect_result == EERESOLVERBUSY) {
      resolver_busy_count++;
    } else {
      ADD_FAILURE() << "Unexpected DNS connect result " << connect_result
                    << " at request index " << i;
    }
  }

  EXPECT_GT(success_count, 0) << "Expected at least one DNS request admitted";
  EXPECT_EQ(success_count + resolver_busy_count, static_cast<int>(fd_list.size()))
    << "Each queued connect should resolve to either admitted or resolver-busy result";

  /* Cleanup */
  for (int fd : fd_list) {
    socket_close(fd, 1);
  }

  free_string_svalue(&read_cb);
  free_string_svalue(&write_cb);
}

/**
 * SOCK_DNS_006: Forced timeout path maps deterministically to OP_TIMED_OUT.
 * Setup: stream socket with test hook forcing DNS deadline expiry.
 * Action: hostname socket_connect and wait for DNS completion processing.
 * Expected: operation reaches terminal OP_TIMED_OUT phase.
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_DNS_006_TimeoutMapsToTimedOutPhase) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
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

  lpc::svalue_view::from(&read_cb).set_shared_string(make_shared_string("read_callback", NULL));
  lpc::svalue_view::from(&write_cb).set_shared_string(make_shared_string("write_callback", NULL));

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
  free_string_svalue(&read_cb);
  free_string_svalue(&write_cb);
}

/**
 * SOCK_DNS_012: Duplicate hostname lookups coalesce to one admitted DNS task.
 * Setup: two stream sockets, same owner, same hostname endpoint.
 * Action: queue two hostname connects back-to-back.
 * Expected: admitted counter increases once; dedup counter increases at least once.
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_DNS_012_DuplicateHostnameConnectsCoalesceWork) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
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

  lpc::svalue_view::from(&read_cb).set_shared_string(make_shared_string("read_callback", NULL));
  lpc::svalue_view::from(&write_cb).set_shared_string(make_shared_string("write_callback", NULL));

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
  free_string_svalue(&read_cb);
  free_string_svalue(&write_cb);
}

/**
 * SOCK_DNS_013: no-cares fallback pool allows independent progress while one lookup is delayed.
 * Setup: one delayed hostname query and one normal hostname query to loopback listeners.
 * Action: queue delayed connect first, then normal connect immediately after.
 * Expected: normal connect reaches OP_TRANSFERRING while delayed lookup is still in OP_DNS_RESOLVING.
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_DNS_013_FallbackPoolAvoidsHeadOfLineBlocking) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  ScopedObjectContext ctx(this, master_ob);
  ScopedResolverLookupHook resolver_hook(DelayLocalhostResolverHookExtensions);

  svalue_t read_cb;
  svalue_t write_cb;
  socket_fd_t slow_listener_fd = INVALID_SOCKET_FD;
  socket_fd_t fast_listener_fd = INVALID_SOCKET_FD;
  socket_fd_t accepted_fd = INVALID_SOCKET_FD;
  int slow_listener_port = 0;
  int fast_listener_port = 0;
  int slow_fd;
  int fast_fd;
  int slow_result;
  int fast_result;
  int slow_active = 0;
  int slow_terminal = 0;
  int slow_op_id = 0;
  int slow_phase = OP_INIT;

  lpc::svalue_view::from(&read_cb).set_shared_string(make_shared_string("read_callback", NULL));
  lpc::svalue_view::from(&write_cb).set_shared_string(make_shared_string("write_callback", NULL));

  slow_fd = socket_create(STREAM, &read_cb, NULL);
  fast_fd = socket_create(STREAM, &read_cb, NULL);
  ASSERT_GE(slow_fd, 0) << "Failed to create slow stream socket";
  ASSERT_GE(fast_fd, 0) << "Failed to create fast stream socket";
  ASSERT_TRUE(CreateLoopbackListener(&slow_listener_fd, &slow_listener_port))
    << "Failed to create delayed loopback listener";
  ASSERT_TRUE(CreateLoopbackListener(&fast_listener_fd, &fast_listener_port))
    << "Failed to create fast loopback listener";

  {
    std::string slow_endpoint = "delay-localhost " + std::to_string(slow_listener_port);
    std::string fast_endpoint = "localhost " + std::to_string(fast_listener_port);

    slow_result = socket_connect(slow_fd, (char *)slow_endpoint.c_str(), &read_cb, &write_cb);
    fast_result = socket_connect(fast_fd, (char *)fast_endpoint.c_str(), &read_cb, &write_cb);
  }

  ASSERT_EQ(slow_result, EESUCCESS);
  ASSERT_EQ(fast_result, EESUCCESS);
  ASSERT_TRUE(WaitForSocketPhase(fast_fd, OP_TRANSFERRING, 600))
    << "Fast hostname lookup should complete while delayed lookup is still pending";

  ASSERT_EQ(get_socket_operation_info(slow_fd, &slow_active, &slow_terminal, &slow_op_id, &slow_phase), EESUCCESS);
  EXPECT_EQ(slow_active, 1);
  EXPECT_EQ(slow_terminal, 0);
  EXPECT_GT(slow_op_id, 0);
  EXPECT_EQ(slow_phase, OP_DNS_RESOLVING)
    << "Delayed lookup should still be resolving when independent fast lookup completes";

  EXPECT_TRUE(AcceptPendingConnection(fast_listener_fd, &accepted_fd))
    << "Fast listener should accept connection before delayed lookup completes";
  if (accepted_fd != INVALID_SOCKET_FD) {
    SOCKET_CLOSE(accepted_fd);
    accepted_fd = INVALID_SOCKET_FD;
  }

  ASSERT_TRUE(WaitForDNSCompletion(slow_fd, 3000))
    << "Delayed lookup should eventually complete";

  if (AcceptPendingConnection(slow_listener_fd, &accepted_fd)) {
    SOCKET_CLOSE(accepted_fd);
  }

  EXPECT_EQ(socket_close(slow_fd, 1), EESUCCESS);
  EXPECT_EQ(socket_close(fast_fd, 1), EESUCCESS);
  SOCKET_CLOSE(slow_listener_fd);
  SOCKET_CLOSE(fast_listener_fd);
  free_string_svalue(&read_cb);
  free_string_svalue(&write_cb);
}

/**
 * SOCK_DNS_011: Backend loop remains responsive while DNS flood is in progress.
 * Setup: Queue multiple hostname DNS operations to exercise worker/admission path.
 * Action: Perform independent numeric connect while DNS operations remain in flight.
 * Expected: Numeric connect path succeeds without being blocked by DNS worker activity.
 */
TEST_F(SocketEfunsBehaviorTest, SOCK_DNS_011_BackendRemainsResponsiveUnderDnsFlood) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  ScopedObjectContext ctx(this, master_ob);

  svalue_t read_cb, write_cb;
  std::vector<int> flood_fds;
  socket_fd_t listener_fd = INVALID_SOCKET_FD;
  socket_fd_t accepted_fd = INVALID_SOCKET_FD;
  int listener_port = 0;
  int probe_fd;
  int result;

  lpc::svalue_view::from(&read_cb).set_shared_string(make_shared_string("read_callback", NULL));
  lpc::svalue_view::from(&write_cb).set_shared_string(make_shared_string("write_callback", NULL));

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

  free_string_svalue(&read_cb);
  free_string_svalue(&write_cb);
}

TEST_F(SocketEfunsBehaviorTest, SOCK_RT_001_CreateRegistersAndCloseRemovesRuntimeEntry) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  ScopedObjectContext ctx(this, master_ob);

  svalue_t read_cb;
  int fd;
  int registered = 0;
  int events = 0;
  socket_fd_t tracked_fd = INVALID_SOCKET_FD;
  int resolved_socket = -1;
  socket_fd_t native_fd = INVALID_SOCKET_FD;
  void* runtime_context;

  lpc::svalue_view::from(&read_cb).set_shared_string(make_shared_string("read_callback", NULL));

  fd = socket_create(STREAM, &read_cb, NULL);
  ASSERT_GE(fd, 0) << "Failed to create stream socket";

  ASSERT_EQ(get_socket_runtime_info(fd, &registered, &events, &tracked_fd), EESUCCESS);
  EXPECT_EQ(registered, 1);
  EXPECT_EQ(events, EVENT_READ);
  EXPECT_EQ(tracked_fd, lpc_socks[fd].fd);

  runtime_context = get_socket_runtime_context(fd);
  native_fd = lpc_socks[fd].fd;
  ASSERT_NE(runtime_context, nullptr);
  EXPECT_EQ(resolve_lpc_socket_context(runtime_context, native_fd, &resolved_socket), 1);
  EXPECT_EQ(resolved_socket, fd);

  EXPECT_EQ(socket_close(fd, 1), EESUCCESS);

  ASSERT_EQ(get_socket_runtime_info(fd, &registered, &events, &tracked_fd), EESUCCESS);
  EXPECT_EQ(registered, 0);
  EXPECT_EQ(events, 0);
  EXPECT_EQ(tracked_fd, INVALID_SOCKET_FD);
  EXPECT_EQ(resolve_lpc_socket_context(runtime_context, native_fd, &resolved_socket), 0);

  free_string_svalue(&read_cb);
}

TEST_F(SocketEfunsBehaviorTest, SOCK_RT_002_BlockedStateTracksWriteInterest) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  ScopedObjectContext ctx(this, master_ob);

  svalue_t read_cb;
  svalue_t write_cb;
  socket_fd_t listener_fd = INVALID_SOCKET_FD;
  socket_fd_t accepted_fd = INVALID_SOCKET_FD;
  int listener_port = 0;
  int fd;
  int registered = 0;
  int events = 0;
  socket_fd_t tracked_fd = INVALID_SOCKET_FD;
  int attempts;

  lpc::svalue_view::from(&read_cb).set_shared_string(make_shared_string("read_callback", NULL));
  lpc::svalue_view::from(&write_cb).set_shared_string(make_shared_string("write_callback", NULL));

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
  free_string_svalue(&read_cb);
  free_string_svalue(&write_cb);
}

TEST_F(SocketEfunsBehaviorTest, SOCK_RT_003_RuntimeRegistrationStress_NoLeaks) {
  ASSERT_TRUE(master_ob) << "Master object not initialized";
  ScopedObjectContext ctx(this, master_ob);

  svalue_t read_cb;
  int baseline_registrations;
  int iteration;

  lpc::svalue_view::from(&read_cb).set_shared_string(make_shared_string("read_callback", NULL));

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

  free_string_svalue(&read_cb);
}

