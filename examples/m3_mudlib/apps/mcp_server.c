// apps/mcp_server.c - MCP server over stdio transport
//
// Implements the Model Context Protocol (MCP) using JSON-RPC 2.0 over
// stdin/stdout.  Intended to be launched as a child process by an MCP
// client.
//
// Lifecycle operations implemented:
//   initialize      - negotiate protocol version and advertise capabilities
//   initialized     - client acknowledgement notification (no response)
//   ping            - keepalive round-trip
//   shutdown        - client requests graceful stop (server stays alive
//                     until "exit" notification)
//   exit            - client signals the process should terminate
//
// Usage:
//   neolith -f m3.conf -c m3_mudlib/apps/mcp_server.c

// -----------------------------------------------------------------------
// Constants
// -----------------------------------------------------------------------

#define MCP_VERSION    "2024-11-05"
#define SERVER_NAME    "neolith-m3-mcp"
#define SERVER_VERSION "0.1.0"

// JSON-RPC 2.0 reserved error codes
#define ERR_PARSE_ERROR      -32700
#define ERR_INVALID_REQUEST  -32600
#define ERR_METHOD_NOT_FOUND -32601
#define ERR_INVALID_PARAMS   -32602
#define ERR_INTERNAL_ERROR   -32603

// -----------------------------------------------------------------------
// Forward declarations
// -----------------------------------------------------------------------

private void handle_line(string line);
private void dispatch_method(mixed id, string method, mixed params);
private void handle_initialize(mixed id, mixed params);
private void handle_shutdown(mixed id);
private void send_error(mixed id, int code, string msg);

// -----------------------------------------------------------------------
// State
// -----------------------------------------------------------------------

private int server_initialized;  // set to 1 after "initialized" notification
private int current_request_id;  // for generating unique IDs for server-initiated requests
private int shutting_down;       // set to 1 after "shutdown" request handled

// -----------------------------------------------------------------------
// Console mode entry points
// -----------------------------------------------------------------------

object connect(int port) {
  return this_object();   // single-user console mode: be our own user object
}

void logon() {
  input_to("handle_line", 0);
}

void net_dead() {
  // EOF in console mode or client disconnect in socket mode.
  shutdown();
}

void error_handler(mixed err, int caught) {
  if (!undefinedp(current_request_id))
    send_error(current_request_id, ERR_INTERNAL_ERROR, "Internal error: " + err["error"]);
}

// -----------------------------------------------------------------------
// I/O helpers
// -----------------------------------------------------------------------

private void send_raw(string json_line) {
  write(json_line + "\n");
}

private void send_response(mixed id, mixed result) {
  send_raw(to_json(([ "jsonrpc": "2.0", "id": id, "result": result ])));
}

private void send_error(mixed id, int code, string msg) {
  send_raw(to_json(([
    "jsonrpc": "2.0",
    "id":      id,
    "error":   ([ "code": code, "message": msg ]),
  ])));
}

// -----------------------------------------------------------------------
// Top-level dispatch
// -----------------------------------------------------------------------

private void dispatch_method(mixed id, string method, mixed params) {
  switch (method) {
    case "initialize":
      handle_initialize(id, params);
      break;
    case "initialized":
      // Notification: client acknowledges; no response.
      server_initialized = 1;
      break;
    case "ping":
      // Both requests (with id) and notifications (without id) are valid.
      if (!undefinedp(id))
        send_response(id, ([]));
      break;
    case "shutdown":
      handle_shutdown(id);
      break;
    case "exit":
      // Process exit — only safe after shutdown, but honour it either way.
      shutdown();
      break;
    default:
      if (!undefinedp(id))
        send_error(id, ERR_METHOD_NOT_FOUND, "Method not found: " + method);
      break;
  }
}

private void handle_line(string line) {
  mixed err, msg, id, method, params;

  // Re-arm input_to BEFORE processing.  If any error below escapes catch,
  // the server remains responsive to the next message.
  input_to("handle_line", 0);

  // EOF/disconnect is handled by net_dead(). Ignore non-string/blank lines.
  if (!stringp(line) || line == "")
    return;

  // Parse.
  err = catch(msg = from_json(line));
  if (err || !mapp(msg)) {
    send_error(id, ERR_PARSE_ERROR, "Parse error");
    return;
  }

  id     = msg["id"];
  current_request_id = id;  // for error handler context
  method = msg["method"];
  params = msg["params"];

  // "method" must be a string (validates both requests and notifications).
  if (!stringp(method)) {
    if (!undefinedp(id))  // per JSON-RPC 2.0: no response to notifications
      send_error(id, ERR_INVALID_REQUEST, "Invalid Request: missing method");
    return;
  }

  // Dispatch; catch any LPC error so requests always get a valid response.
  err = catch(dispatch_method(id, method, params));
  if (err) {
    if (!undefinedp(id))  // per JSON-RPC 2.0: no response to notifications
      send_error(id, ERR_INTERNAL_ERROR, "Internal error");
  }
}

// -----------------------------------------------------------------------
// Method handlers
// -----------------------------------------------------------------------

private void handle_initialize(mixed id, mixed params) {
  string proto;

  // "initialize" must be a request (id is required).
  if (undefinedp(id)) {
    return;
  }

  // Echo back the client's requested version if it matches; otherwise use
  // our supported version.
  proto = (mapp(params) && stringp(params["protocolVersion"]))
          ? params["protocolVersion"]
          : MCP_VERSION;

  if (proto != MCP_VERSION)
    proto = MCP_VERSION;

  send_response(id, ([
    "protocolVersion": proto,
    "capabilities":   ([
      // No optional capability extensions in this baseline implementation.
    ]),
    "serverInfo":     ([
      "name":    SERVER_NAME,
      "version": SERVER_VERSION,
    ]),
  ]));
}

private void handle_shutdown(mixed id) {
  mixed json_null;   // uninitialized LPC mixed == undefined == JSON null

  shutting_down = 1;
  if (!undefinedp(id))
    send_response(id, json_null);
}
