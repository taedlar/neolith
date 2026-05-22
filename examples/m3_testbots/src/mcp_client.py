#!/usr/bin/env python3
"""
mcp_client.py - MCP client testbot for the neolith-m3-mcp server.

Launches the Neolith driver in console mode running apps/mcp_server.c and
exercises basic MCP lifecycle operations over stdio transport (newline-
delimited JSON-RPC 2.0).

No pexpect is used.  All I/O is raw subprocess pipes plus stdlib json.

Usage:
    hatch run mcp_client              # Basic lifecycle test with defaults
    hatch run mcp_client --debug      # Extra driver args forwarded verbatim

Exit codes:
    0   All assertions passed
    1   A test assertion failed
    2   Setup error (driver not found, config not found, etc.)
"""

import collections
import json
import os
import subprocess
import sys
from pathlib import Path


# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

def _find_driver() -> Path:
    """Return the path to the neolith driver binary, or raise if missing."""
    # __file__ is  examples/m3_testbots/src/mcp_client.py
    # four parents: src/ -> m3_testbots/ -> examples/ -> neolith/ (repo root)
    root = Path(__file__).resolve().parent.parent.parent.parent
    candidates = [
        root / "out/build/linux/src/RelWithDebInfo/neolith",
        root / "out/build/clang-x64/src/RelWithDebInfo/neolith",
        root / "out/build/vs16-x64/src/RelWithDebInfo/neolith.exe",
    ]
    # Also honour an explicit override from the environment.
    env_override = os.environ.get("NEOLITH_DRIVER")
    if env_override:
        candidates.insert(0, Path(env_override))

    for p in candidates:
        if p.exists():
            return p
    raise FileNotFoundError(
        "neolith driver not found. Build it first or set NEOLITH_DRIVER."
    )


def _find_config() -> Path:
    # examples/ is two parents above this file: src/ -> m3_testbots/ -> examples/
    examples = Path(__file__).resolve().parent.parent.parent
    p = examples / "m3.conf"
    if p.exists():
        return p
    raise FileNotFoundError(f"No config found in {examples}")


# ---------------------------------------------------------------------------
# McpClient: thin JSON-RPC 2.0 wrapper over a subprocess
# ---------------------------------------------------------------------------

class McpClient:
    """Manages a single MCP server subprocess and speaks JSON-RPC 2.0."""

    def __init__(self, proc: subprocess.Popen) -> None:
        self._proc = proc
        self._next_id = 1
        # Incoming notifications and server-initiated requests.
        self._recv_queue: collections.deque[dict] = collections.deque()
        # Responses keyed by request id (JSON-RPC: has "id", no "method").
        self._pending_responses: dict[int, dict] = {}

    # ------------------------------------------------------------------
    # Low-level send / receive
    # ------------------------------------------------------------------

    def _alloc_id(self) -> int:
        request_id = self._next_id
        self._next_id += 1
        return request_id

    def send_request(self, method: str, params: dict | None = None) -> int:
        """Send a JSON-RPC request and return its id."""
        request_id = self._alloc_id()
        msg: dict = {"jsonrpc": "2.0", "id": request_id, "method": method}
        if params is not None:
            msg["params"] = params
        self._write(msg)
        return request_id

    def send_notification(self, method: str, params: dict | None = None) -> None:
        """Send a JSON-RPC notification (no id, no response expected)."""
        msg: dict = {"jsonrpc": "2.0", "method": method}
        if params is not None:
            msg["params"] = params
        self._write(msg)

    def _pump(self) -> None:
        """Read one raw message from the pipe and route it to the right buffer.

        JSON-RPC responses (have ``id``, no ``method``) go into
        ``_pending_responses`` keyed by id.  Everything else (notifications,
        server-initiated requests) goes into ``_recv_queue``.
        """
        while True:
            raw = self._proc.stdout.readline()
            if not raw:
                raise EOFError("Server closed stdout unexpectedly")
            line = raw.decode("utf-8", errors="replace").strip()
            print(f"<<< {line}")
            if not line or not line.startswith("{"):
                continue  # skip blank separator lines
            msg = json.loads(line)
            if "id" in msg and "method" not in msg:
                self._pending_responses[msg["id"]] = msg
            else:
                self._recv_queue.append(msg)
            return

    def recv(self) -> dict:
        """Return the next notification or server-initiated request.

        Drains ``_recv_queue`` first; pumps the pipe when the queue is empty.
        """
        while not self._recv_queue:
            self._pump()
        return self._recv_queue.popleft()

    def _write(self, msg: dict) -> None:
        line = (json.dumps(msg, separators=(",", ":")) + "\r\n").encode("utf-8")
        self._proc.stdin.write(line)
        self._proc.stdin.flush()
        print(f">>> {line.decode('utf-8', errors='replace').strip()}")

    # ------------------------------------------------------------------
    # High-level helpers
    # ------------------------------------------------------------------

    def recv_response(self, request_id: int) -> dict:
        """Block until the response for request_id arrives.

        Responses for other in-flight requests accumulate in
        ``_pending_responses`` and are returned by their own
        ``recv_response()`` calls.  Notifications accumulate in
        ``_recv_queue`` and are returned by ``recv()``.
        """
        while request_id not in self._pending_responses:
            self._pump()
        return self._pending_responses.pop(request_id)

    def request(self, method: str, params: dict | None = None) -> dict:
        """Send a request and block until its matching response arrives."""
        return self.recv_response(self.send_request(method, params))

    def close(self) -> None:
        """Close the server's stdin (signals EOF) and wait for it to exit."""
        try:
            self._proc.stdin.close()
        except OSError:
            pass
        try:
            self._proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self._proc.kill()
            self._proc.wait()

    @property
    def exit_code(self) -> int | None:
        return self._proc.returncode


# ---------------------------------------------------------------------------
# Test helpers
# ---------------------------------------------------------------------------

_failures: list[str] = []
_passed = 0


def check(label: str, condition: bool, detail: str = "") -> None:
    global _passed
    if condition:
        _passed += 1
        print(f"  PASS  {label}")
    else:
        msg = f"  FAIL  {label}" + (f": {detail}" if detail else "")
        _failures.append(msg)
        print(msg)


def assert_ok_response(resp: dict, label: str) -> None:
    """Assert that a JSON-RPC response has no 'error' field."""
    check(f"{label}: no error", "error" not in resp,
          str(resp.get("error", "")))


# ---------------------------------------------------------------------------
# Test cases
# ---------------------------------------------------------------------------

def test_initialize(client: McpClient) -> None:
    print("\n[initialize]")
    params = {
        "protocolVersion": "2024-11-05",
        "capabilities": {},
        "clientInfo": {"name": "mcp-testbot", "version": "0.1.0"},
    }
    resp = client.request("initialize", params)
    assert_ok_response(resp, "initialize")

    result = resp.get("result", {})
    check("result.protocolVersion present",
          "protocolVersion" in result,
          str(result))
    check("result.protocolVersion value",
          result.get("protocolVersion") == "2024-11-05",
          repr(result.get("protocolVersion")))
    check("result.serverInfo present", "serverInfo" in result)
    check("result.capabilities present", "capabilities" in result)

    server_info = result.get("serverInfo", {})
    check("serverInfo.name is string", isinstance(server_info.get("name"), str))
    check("serverInfo.version is string",
          isinstance(server_info.get("version"), str))


def test_initialized_notification(client: McpClient) -> None:
    print("\n[initialized notification]")
    # Notifications produce no response; just verify no exception is raised.
    client.send_notification("initialized")
    check("initialized notification sent without error", True)


def test_ping(client: McpClient) -> None:
    print("\n[ping]")
    resp = client.request("ping")
    assert_ok_response(resp, "ping")
    check("ping result is object", isinstance(resp.get("result"), dict),
          repr(resp.get("result")))


def test_unknown_method(client: McpClient) -> None:
    print("\n[unknown method]")
    resp = client.request("tools/__nonexistent__")
    check("unknown method returns error", "error" in resp,
          str(resp))
    err = resp.get("error", {})
    check("error code is -32601 (method not found)",
          err.get("code") == -32601,
          repr(err.get("code")))


def test_shutdown_and_exit(client: McpClient) -> None:
    print("\n[shutdown + exit]")
    resp = client.request("shutdown")
    assert_ok_response(resp, "shutdown")
    check("shutdown result is null", resp.get("result") is None,
          repr(resp.get("result")))

    # After shutdown the server stays up until "exit".
    client.send_notification("exit")
    check("exit notification sent", True)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def run_tests(extra_args: list[str]) -> int:
    try:
        driver = _find_driver()
        config = _find_config()
    except FileNotFoundError as exc:
        print(f"Setup error: {exc}")
        return 2

    print(f"Driver : {driver}")
    print(f"Config : {config}")
    if extra_args:
        print(f"Extra  : {' '.join(extra_args)}")

    cmd = [
        str(driver),
        "-f", str(config),
        "-c", "m3_mudlib/apps/mcp_server.c",
        *extra_args,
    ]
    print(f"Command: {' '.join(cmd)}\n")
    print("=" * 60)
    print("MCP LIFECYCLE TEST")
    print("=" * 60)

    proc = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,   # capture MCP server errors too
        cwd=Path(config).parent,    # run from the examples/ directory
    )
    client = McpClient(proc)

    try:
        test_initialize(client)
        test_initialized_notification(client)
        test_ping(client)
        test_unknown_method(client)
        test_shutdown_and_exit(client)
    except EOFError as exc:
        print(f"\nConnection error: {exc}")
        _failures.append(str(exc))
    except json.JSONDecodeError as exc:
        print(f"\nJSON decode error: {exc}")
        _failures.append(str(exc))
    finally:
        client.close()

    print("\n" + "=" * 60)
    if _failures:
        print(f"RESULT: {len(_failures)} failure(s), {_passed} passed")
        for f in _failures:
            print(f)
        return 1
    else:
        print(f"RESULT: all {_passed} checks passed")
        return 0


if __name__ == "__main__":
    sys.exit(run_tests(sys.argv[1:]))
