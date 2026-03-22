# LPC Mudlib DNS Resolver Guide

**Status**: Reference guide for Stage 4B (Mudlib DNS Track)  
**Audience**: Mudlib developers implementing custom DNS resolution  
**Availability**: Always available (no build-time feature flag required)

## Overview

This guide describes implementing DNS hostname resolution at the mudlib layer using LPC socket efuns (DATAGRAM mode). This approach is:

- **Always available**: works regardless of whether the driver's optional DNS feature is enabled or disabled.
- **Flexible**: allows mudlib developers to implement custom resolution logic, caching, and retry policies.
- **Non-blocking**: uses async socket I/O to avoid stalling the LPC interpreter.
- **Deterministic**: provides explicit failure and timeout handling you control.

## When to Use This Approach

Use mudlib-side DNS resolution when:

1. **DNS feature disabled**: The driver was built without `PACKAGE_SOCKET_CONNECT_DNS`, so `socket_connect(fd, hostname:port, ...)` fails with `EEBADADDR`.
2. **Custom caching**: You need to cache resolved addresses across connections or implement negative caching.
3. **Retry policies**: You want fine-grained control over retry logic, backoff, or fallback strategies.
4. **Monitoring/logging**: You need to log or monitor all DNS queries for operational visibility.
5. **Rate limiting**: You want to apply custom rate limits or priority handling for DNS requests.

When the driver's built-in DNS feature **is** enabled, direct `socket_connect(fd, hostname:port, ...)` calls are simpler and preferred over this approach.

## Architecture: UDP-Based DNS Resolution

The DNS protocol uses UDP (DATAGRAM sockets). A typical mudlib resolver follows this flow:

```
LPC Code
  │
  ├─> Create DATAGRAM socket (owner=resolver_object)
  │        │
  │        ├─> Bind to local ephemeral port
  │        │
  │        ├─> Send DNS query packet to nameserver (usually 127.0.0.1:53)
  │        │        │
  │        │        ├─> Query waits in DNS server
  │        │        │
  │        │        └─> Response enqueued in socket receive buffer
  │        │
  │        ├─> Read callback fires (data available)
  │        │        │
  │        │        └─> Parse response packet
  │        │
  │        └─> Close socket when done or timeout triggered
  │
  └─> Invoke callback with (resolved_address, error_info)
           │
           └─> LPC code calls socket_connect(new_fd, resolved_address:port, ...)
```

## DNS Packet Format (Simplified)

DNS queries and responses are binary packets. Here's a minimal reference:

**Query Packet Structure** (simplified big-endian):
- **Transaction ID** (2 bytes): random identifier (echoed in response)
- **Flags** (2 bytes): `0x0100` for standard query
- **Questions** (2 bytes): number of queries (usually 1)
- **Answer RRs** (2 bytes): 0 for query
- **Authority RRs** (2 bytes): 0 for query
- **Additional RRs** (2 bytes): 0 for query
- **Question Section**:
  - **Name** (variable): labels separated by length bytes, ends with 0x00
    - Example: `localhost` → `0x09 l o c a l h o s t 0x00`
  - **Type** (2 bytes): `0x0001` for A record (IPv4 address)
  - **Class** (2 bytes): `0x0001` for IN (Internet)

**Response Packet Structure** (includes query + answer):
- Same header as query, but **Flags** = `0x8400` (response + standard + authoritative)
- **Answer RRs** (2 bytes): number of answers (e.g., 1)
- **Answer Section** (if flags indicate success):
  - **Name** (variable, often compressed): hostname being resolved
  - **Type** (2 bytes): `0x0001` for A record
  - **Class** (2 bytes): `0x0001` for IN
  - **TTL** (4 bytes): time-to-live in seconds
  - **Data Length** (2 bytes): 4 for A record (IPv4 is 4 bytes)
  - **Address** (4 bytes): IPv4 address in network byte order (e.g., `127.0.0.1` as `0x7F 0x00 0x00 0x01`)

**Response Flags Interpretation**:
- `0x8000` bit set: response (vs query)
- `0x0400` bit set: authority section (nameserver is authoritative)
- `0x0200` bit set: truncated (answer was truncated; shouldn't happen on LAN)
- `0x000F` bits (low 4 bits): response code
  - `0x0000`: success (NOERROR)
  - `0x0001`: format error (FORMERR) — query malformed
  - `0x0002`: server failure (SERVFAIL) — DNS server failed
  - `0x0003`: name error (NXDOMAIN) — domain does not exist
  - `0x0004`: not implemented (NOTIMP) — server doesn't support query type
  - `0x0005`: refused (REFUSED) — server refused to process

For full DNS spec, see RFC 1035. Most mudlib resolvers use simple parsing: skip the query section, then extract the first A record from the answer section (or handle NXDOMAIN as "not found").

## Example: Simple DNS Resolver

Here's a minimal LPC DNS resolver implementation:

```lpc
#include <std.h>
#include <daemon.h>

/* Resolver state */
static mapping pending_queries = ([]);  /* transaction_id -> callback_info */
static int resolver_socket = -1;
static int query_counter = 0;

/**
 * @brief Initialize resolver (call once at boot, from master object).
 */
void resolver_init() {
  if (resolver_socket >= 0)
    return;

  /* Create UDP socket */
  resolver_socket = socket_create(DATAGRAM, "resolver_read_cb", 0);
  if (resolver_socket < 0) {
    write_file("/log/resolver.log", sprintf("Failed to create resolver socket: %O\n", resolver_socket));
    return;
  }

  /* Bind to ephemeral port (OS assigns) */
  int result = socket_bind(resolver_socket, 0);
  if (result != EESUCCESS) {
    write_file("/log/resolver.log", sprintf("Failed to bind resolver socket: %O\n", result));
    socket_close(resolver_socket);
    resolver_socket = -1;
    return;
  }

  write_file("/log/resolver.log", "DNS resolver initialized.\n");
}

/**
 * @brief Shut down resolver (call at shutdown).
 */
void resolver_shutdown() {
  if (resolver_socket >= 0) {
    socket_close(resolver_socket);
    resolver_socket = -1;
  }
  pending_queries = ([]);
}

/**
 * @brief Query DNS for hostname. Async result delivered to callback_obj->callback_func(address, error).
 * @param hostname string hostname to resolve (e.g., "localhost", "example.com")
 * @param port int port number (for error messages; not sent to DNS)
 * @param callback_obj object to receive result
 * @param callback_func name of callback function (signature: void callback_func(string address, int error))
 * @param timeout_seconds int seconds to wait before timing out (default 5)
 * @returns int 0 on success, -1 if resolver not ready, -2 if query encoding failed
 */
int dns_query(string hostname, int port, object callback_obj, string callback_func, int timeout_seconds) {
  int trans_id, result;

  if (!callback_obj || !callback_func)
    return -1;

  if (resolver_socket < 0)
    return -1;

  if (timeout_seconds <= 0)
    timeout_seconds = 5;

  /* Generate unique transaction ID */
  trans_id = ++query_counter & 0xFFFF;  /* 16-bit ID */

  /* Encode DNS query packet */
  string query_packet = encode_dns_query(hostname, trans_id);
  if (!query_packet)
    return -2;

  /* Send query to nameserver (127.0.0.1:53 for loopback resolver) */
  result = socket_write(resolver_socket, query_packet, "127.0.0.1 53");
  if (result != EESUCCESS && result != EECALLBACK) {
    write_file("/log/resolver.log", sprintf("DNS query failed for %O: %O\n", hostname, result));
    return -1;
  }

  /* Record pending query with timeout */
  pending_queries[trans_id] = ([
    "hostname" : hostname,
    "port" : port,
    "callback_obj" : callback_obj,
    "callback_func" : callback_func,
    "timeout" : time() + timeout_seconds,
  ]);

  return 0;
}

/**
 * @brief Encode a DNS A-record query packet for a hostname.
 * @param hostname string hostname to encode
 * @param trans_id int transaction ID (0-65535)
 * @returns string binary query packet, or 0 if encoding failed
 */
string encode_dns_query(string hostname, int trans_id) {
  string *labels, result;
  int i;

  if (!hostname || strlen(hostname) == 0)
    return 0;

  /* Split hostname into labels and encode */
  labels = explode(hostname, ".");
  result = sprintf("%c%c", (trans_id >> 8) & 0xFF, trans_id & 0xFF);  /* Transaction ID */
  result += "\x01\x00";  /* Flags: standard query */
  result += "\x00\x01";  /* Questions: 1 */
  result += "\x00\x00";  /* Answer RRs: 0 */
  result += "\x00\x00";  /* Authority RRs: 0 */
  result += "\x00\x00";  /* Additional RRs: 0 */

  /* Encode question section (hostname labels) */
  for (i = 0; i < sizeof(labels); i++) {
    if (strlen(labels[i]) > 63)
      return 0;  /* Label too long (max 63 chars) */
    result += sprintf("%c%s", strlen(labels[i]), labels[i]);
  }
  result += "\x00";  /* End of hostname */

  /* Query type (A record) and class (IN) */
  result += "\x00\x01";  /* Type: A (host address) */
  result += "\x00\x01";  /* Class: IN (Internet) */

  return result;
}

/**
 * @brief Parse DNS response packet and extract IPv4 address.
 * @param packet string response packet from DNS server
 * @returns mapping with keys: success (int 1/0), address (string IPv4 or 0), error_code (int)
 */
mapping parse_dns_response(string packet) {
  int flags, rcode, offset, qd_count, an_count, i, rdlength;
  string address;

  if (!packet || strlen(packet) < 12)
    return ([ "success" : 0, "address" : 0, "error_code" : -1 ]);

  /* Parse header */
  flags = ((packet[2] & 0xFF) << 8) | (packet[3] & 0xFF);
  rcode = flags & 0x000F;  /* Response code (low 4 bits) */
  qd_count = ((packet[4] & 0xFF) << 8) | (packet[5] & 0xFF);
  an_count = ((packet[6] & 0xFF) << 8) | (packet[7] & 0xFF);

  if (rcode != 0) {
    /* DNS error */
    return ([ "success" : 0, "address" : 0, "error_code" : rcode ]);
  }

  if (an_count == 0) {
    /* No answers (NXDOMAIN treated as success with no address) */
    return ([ "success" : 0, "address" : 0, "error_code" : 0 ]);
  }

  /* Skip question section */
  offset = 12;
  for (i = 0; i < qd_count; i++) {
    offset = skip_dns_name(packet, offset);
    if (offset < 0)
      return ([ "success" : 0, "address" : 0, "error_code" : -2 ]);
    offset += 4;  /* Type (2) + Class (2) */
  }

  /* Parse first answer record (should be A record with IPv4) */
  offset = skip_dns_name(packet, offset);
  if (offset < 0)
    return ([ "success" : 0, "address" : 0, "error_code" : -3 ]);

  /* Check type (should be 0x0001 for A record) */
  if ((packet[offset] & 0xFF) != 0 || (packet[offset + 1] & 0xFF) != 1) {
    return ([ "success" : 0, "address" : 0, "error_code" : -4 ]);  /* Not an A record */
  }

  offset += 2;  /* Type (2) */
  offset += 2;  /* Class (2) */
  offset += 4;  /* TTL (4) */

  /* Data length */
  rdlength = ((packet[offset] & 0xFF) << 8) | (packet[offset + 1] & 0xFF);
  offset += 2;

  if (rdlength != 4) {
    return ([ "success" : 0, "address" : 0, "error_code" : -5 ]);  /* Not IPv4 (wrong size) */
  }

  /* Extract IPv4 address */
  address = sprintf("%d.%d.%d.%d",
                    packet[offset] & 0xFF,
                    packet[offset + 1] & 0xFF,
                    packet[offset + 2] & 0xFF,
                    packet[offset + 3] & 0xFF);

  return ([ "success" : 1, "address" : address, "error_code" : 0 ]);
}

/**
 * @brief Skip a DNS name in packet (handles both uncompressed names and compressed pointers).
 * @param packet string DNS packet
 * @param offset int current offset in packet
 * @returns int new offset after name, or -1 if malformed
 */
int skip_dns_name(string packet, int offset) {
  while (offset < strlen(packet)) {
    int len = packet[offset] & 0xFF;

    if (len == 0) {
      return offset + 1;  /* End of name */
    }

    if ((len & 0xC0) == 0xC0) {
      /* Pointer (compressed name) */
      return offset + 2;
    }

    if ((len & 0xC0) != 0) {
      return -1;  /* Invalid label type */
    }

    /* Regular label */
    offset += 1 + len;
  }

  return -1;  /* Reached end without 0 terminator */
}

/**
 * @brief Read callback: invoked when DNS response arrives.
 * @param fd int socket descriptor
 * @param data string received data (response packet)
 */
void resolver_read_cb(int fd, string data) {
  mapping response, query_info;
  int trans_id;

  if (!data || strlen(data) < 2)
    return;

  /* Extract transaction ID from response header */
  trans_id = ((data[0] & 0xFF) << 8) | (data[1] & 0xFF);

  /* Look up pending query */
  query_info = pending_queries[trans_id];
  if (!query_info)
    return;  /* Stale or unsolicited response */

  /* Parse response */
  response = parse_dns_response(data);

  /* Invoke callback */
  if (query_info["callback_obj"] && objectp(query_info["callback_obj"])) {
    call_other(query_info["callback_obj"], query_info["callback_func"],
               response["address"],  /* IPv4 address or 0 if not found */
               response["error_code"]);
  }

  /* Clean up */
  map_delete(pending_queries, trans_id);
}

/**
 * @brief Check for query timeouts and invoke callbacks with error.
 * Call this periodically (e.g., every heart_beat).
 */
void resolver_check_timeouts() {
  int *trans_ids, i;
  int now = time();

  if (!pending_queries || sizeof(pending_queries) == 0)
    return;

  trans_ids = keys(pending_queries);

  for (i = 0; i < sizeof(trans_ids); i++) {
    mapping query_info = pending_queries[trans_ids[i]];

    if (now >= query_info["timeout"]) {
      /* Timeout */
      if (query_info["callback_obj"] && objectp(query_info["callback_obj"])) {
        call_other(query_info["callback_obj"], query_info["callback_func"],
                   0,  /* No address */
                   -1);  /* Timeout error code */
      }

      map_delete(pending_queries, trans_ids[i]);
    }
  }
}
```

## Integration with socket_connect

Once DNS resolution completes, use the resolved numeric address with `socket_connect`:

```lpc
void on_dns_result(string address, int error) {
  int fd, result;
  object server;

  if (error || !address) {
    write_file("/log/connect.log", sprintf("DNS resolution failed: error_code=%d\n", error));
    return;
  }

  /* Create new socket for actual connection */
  fd = socket_create(STREAM, "read_callback", "close_callback");
  if (fd < 0) {
    write_file("/log/connect.log", "Failed to create connect socket\n");
    return;
  }

  /* Connect using resolved numeric address */
  result = socket_connect(fd, address + " 80", "write_callback", "read_callback");
  if (result != EESUCCESS && result != EECALLBACK) {
    write_file("/log/connect.log", sprintf("Connect failed: %O\n", result));
    socket_close(fd);
  }
}
```

## Timeout and Retry Strategies

### Strategy 1: Simple Timeout

Request with `N`-second timeout; if no response, invoke callback with error:

```lpc
dns_query("example.com", 80, this_object(), "on_dns_result", 5);  /* 5-second timeout */
```

The resolver's `resolver_check_timeouts()` detects expiry and invokes callback with `error = -1`.

### Strategy 2: Exponential Backoff Retry

Retry with increasing delays if DNS fails:

```lpc
void on_dns_result_with_retry(string address, int error) {
  if (error && this_player()->get("dns_retry_count") < 3) {
    int retry_delay = 1 << this_player()->get("dns_retry_count");  /* 2^N seconds */
    this_player()->incr("dns_retry_count");
    call_out("retry_dns_query", retry_delay, this_player()->query("target_host"));
  }
  else if (address) {
    /* Success */
    connect_to_remote(address, 80);
  }
  else {
    /* Failure after retries */
    write_file("/log/connection.log", "DNS resolution failed after retries\n");
  }
}

void retry_dns_query(string hostname) {
  dns_query(hostname, 80, this_object(), "on_dns_result_with_retry", 5);
}
```

### Strategy 3: Cached Resolver

Cache resolved addresses to avoid repeated queries:

```lpc
static mapping dns_cache = ([]);  /* hostname -> ([ "address" : str, "expires" : int ]) */
static int cache_ttl = 3600;  /* 1 hour */

int dns_query_cached(string hostname, int port, object cb_obj, string cb_func) {
  mapping cached;
  int now = time();

  /* Check cache */
  cached = dns_cache[hostname];
  if (cached && cached["expires"] > now) {
    /* Cache hit */
    call_other(cb_obj, cb_func, cached["address"], 0);
    return 0;
  }

  /* Cache miss or expired; query DNS */
  return dns_query(hostname, port, cb_obj, cb_func, 5);
}

void on_cached_dns_result(string hostname, string address, int error) {
  if (address) {
    /* Cache success */
    dns_cache[hostname] = ([ "address" : address, "expires" : time() + cache_ttl ]);
  }
  else {
    /* Cache failure for shorter period (negative caching) */
    dns_cache[hostname] = ([ "address" : 0, "expires" : time() + 60 ]);
  }

  /* Invoke original callback */
  invoke_pending_callback(hostname, address, error);
}
```

## Error Codes and Failure Mapping

DNS errors are:

| Error Code | Meaning | Action |
|---|---|---|
| 0 | NOERROR: successful response but no address (NXDOMAIN) | Hostname does not exist; fail connection |
| 1 | FORMERR: query malformed | Bug in query encoder; log and fail |
| 2 | SERVFAIL: DNS server failure | Temporary failure; retry with backoff |
| 3 | NXDOMAIN: domain does not exist | Permanent failure; do not retry |
| 4 | NOTIMP: server does not support query | Server configuration issue; retry different server |
| 5 | REFUSED: server refused query | Server policy; do not retry immediately |
| -1 | Timeout: no response within timeout window | Transient; retry with backoff |
| -2 | Malformed query encoding | Bug in query encoder; cannot recover |
| -3 | Malformed response parsing | Corrupted or non-DNS response; retry |
| -4 | Response is not A record (IPv4) | Non-IPv4 response (e.g., IPv6); handle or skip |
| -5 | Invalid response size | Protocol error; skip and retry |

### Suggested Retry Policy

```lpc
void on_dns_result(string address, int error) {
  switch (error) {
    case 0:
      /* NXDOMAIN: hostname doesn't exist, don't retry */
      callback_fail("Host not found");
      break;

    case 3:
      /* NXDOMAIN: domain does not exist, don't retry */
      callback_fail("Domain not found");
      break;

    case 2:
    case -1:
      /* SERVFAIL or timeout: transient failure, retry with backoff */
      schedule_retry(1 << retry_count);
      break;

    case 1:
    case -2:
      /* FORMERR or encoding error: likely a bug, don't retry repeatedly */
      callback_fail("Query encoding error");
      break;

    default:
      /* Other errors: log and fail */
      callback_fail(sprintf("DNS error: %d", error));
      break;
  }
}
```

## Compatibility: DNS-Disabled Deployments

When the driver is built without `PACKAGE_SOCKET_CONNECT_DNS`, direct `socket_connect(fd, hostname:port, ...)` calls fail with `EEBADADDR`. This guide's mudlib resolver approach works unchanged:

1. **Mudlib resolver always available**: No build-time dependency.
2. **Numeric connect still works**: `socket_connect(fd, "192.0.2.1 80", ...)` succeeds (this guide's output).
3. **Explicit resolution contract**: Mudlib code is responsible for DNS flow and timeout.

For deployments with DNS enabled, mudlib can optionally skip this resolver and use direct `socket_connect(fd, hostname:port, ...)` calls, which delegate DNS to the driver.

## Operational Considerations

### Nameserver Configuration

By default, most systems have a local nameserver (often `systemd-resolved` on Linux or localhost resolver) listening on `127.0.0.1:53`. If your environment differs:

1. Check `/etc/resolv.conf` (Linux) for the `nameserver` line.
2. On Windows, use `ipconfig /all` to find DNS server addresses.
3. Update the resolver to query the appropriate server:

```lpc
result = socket_write(resolver_socket, query_packet, "8.8.8.8 53");  /* Google Public DNS */
```

### Query Logging

Add logging to track DNS queries and performance:

```lpc
void dns_query(string hostname, ...) {
  write_file("/log/dns_queries.log",
             sprintf("[%s] Query: %O\n", ctime(), hostname));

  int start_time = time();

  /* ... send query ... */

  call_out("log_query_result", timeout_seconds + 1, hostname, start_time);
}

void log_query_result(string hostname, int start_time) {
  int elapsed = time() - start_time;
  write_file("/log/dns_queries.log",
             sprintf("[%s] Result: %O (elapsed %ds)\n", ctime(), hostname, elapsed));
}
```

### Resource Limits

Running many concurrent DNS queries consumes socket descriptors:

- Each query uses one DATAGRAM socket (or one resolver socket with many pending queries).
- Set a practical limit on `max(sizeof(pending_queries))` to 64-256 queries.
- Use admission control to reject new queries if pending count exceeds threshold.

```lpc
int dns_query(string hostname, ...) {
  if (sizeof(pending_queries) >= 64) {
    return -1;  /* Overloaded */
  }
  /* ... continue ... */
}
```

## Related Efun Documentation

- [socket_create](../efuns/socket_create.md): create DATAGRAM socket
- [socket_bind](../efuns/socket_bind.md): bind to local port
- [socket_write](../efuns/socket_write.md): send DNS query to nameserver
- [socket_connect](../efuns/socket_connect.md): connect using resolved numeric address
- [socket_close](../efuns/socket_close.md): close resolver socket

## References

- **RFC 1035**: Domain Names - Implementation and Specification (DNS protocol specification)
- **RFC 1123**: Requirements for Internet Hosts (DNS implementation guidelines)
- **systemd-resolved(8)**: System daemon for DNS resolution (Linux)

## See Also

- [Socket Operation Engine Plan](../plan/socket-operation-engine.md) — Stage 4B (Mudlib DNS Track)
- [Async Library User Guide](./async.md) — overview of async I/O patterns
- [Socket Efuns](../efuns/socket_create.md) — comprehensive socket efun reference
