# Agent Harness Plan
An agent harness is a sandbox hosting one or more AI agents in a controlled environment.
LPMud was originally developed for human users, however, the idea can apply to AI agents as well.

The plan involves a collection of efuns that enables implementation of AI agents in LPC.

## Main Use Cases
- **Agent Users**: an LPC object can be enabled for AI agent with an efun `enable_agent()` to represent an agent user. Agent users can be interactive (also bound to a connection) or non-interactive (controlled by LPC code).
- **Agent Context Window**: Allows the master object to inject mandatory system instructions to agent users. Manage the LLM context with a special string buffer (not directly visible to LPC, and it can exceed maximum string length in LPC). Also provide a collection of efuns to manage the contents of context window.
- **Agent Execution**: AI agent execution workflow is implemented in LPC code by calling an efun `agent_model_interence()` that uses the context window as prompt and calls (async) model generate-content APIs in JSON mode. The response can contain function calling (using tool registry and LPC functions) or user commands.

## Scope

Phase 1 adds the full agentic feedback loop (tool results → context → re-inference → repeat until no tools called or `max_turns`) and before/after tool call LPC applies. Builds on the existing async worker infrastructure and `T_BUFFER` type — no new runtime types needed.

Deferred to Phase 2: sessions, context compaction, streaming deltas, lifecycle applies (`agent_start`/`agent_end`), timeouts.

## Phase 1A: Loop Model

### 1. Context Window Append API (efuns)

- `agent_context_reset(object agent)` — clears the context buffer
- `agent_context_add(object agent, int role, string content)` — appends one turn (role = `AGENT_ROLE_SYSTEM` / `USER` / `ASSISTANT` / `TOOL`, constants in a new `agent.h`)
- Context serialized as JSON in a `T_BUFFER` stored in C-side state (invisible to LPC; reuses existing buffer infrastructure from `lib/lpc/buffer.h`)

### 2. Tool Registry Efuns

- `agent_register_tool(object agent, string name, string json_schema, function callback)` — registers a named tool with its JSON schema and LPC callback
- `agent_unregister_tool(object agent, string name)` — remove a tool
- `agent_tools_json(object agent) : string` — returns JSON array of all schemas (for injecting into system prompt before inference)

### 3. `agent_model_inference(object agent, int max_turns, function done_cb)` (async efun)

Returns immediately; posts job to async worker.

Worker calls model API using context + tools JSON.

On completion, main thread (via `async_runtime_post_completion()` → `async_runtime_wait()` in `src/comm.c`):
- **No tool calls in response** → append ASSISTANT turn → call `done_cb(text, 0)`
- **Tool calls + turn < max_turns** → dispatch tool hooks + LPC functions, append results, post next worker job
- **max_turns exceeded** → call `done_cb(partial_text, ERR_MAX_TURNS)`

The loop is fully C-side; LPC sees only the final callback.

### 4. C-Side Agent State (new `lib/efuns/agent.c` + `agent.h`)

Attached to `object_t *` via an external hash table (avoids touching `object_t` struct and preserving binary compatibility).

State fields: `context_buf (T_BUFFER)`, `tool_registry[]`, `turn_count`, `max_turns`, `done_cb`, `in_flight`.

## Phase 1B: Tool Hooks (depends on Phase 1A step 3)

### 5. `before_tool_call` Apply

Before executing the LPC tool callback: `apply_low("before_tool_call", agent_ob, 2)` with `name (string)`, `args (mapping)`.

- Return `1` (non-zero) → veto: inject `{"error": "tool vetoed"}` into context, skip execution
- Return `0` or function not defined → proceed normally

### 6. `after_tool_call` Apply

After the LPC callback returns: `apply_low("after_tool_call", agent_ob, 3)` with `name`, `args`, `result (mixed)`.

Non-nil return value replaces the tool result before injecting into context. Allows agent object to sanitize, log, or transform output.

### 7. Tool Dispatch

- Match `tool_name` from model JSON against registry; unknown name → inject error, continue loop
- Catch LPC errors at the call boundary → inject error string as tool result (prevents loop crash)

## Relevant Files

| File | Role |
|---|---|
| `agent-harness.md` | This plan |
| `lib/lpc/func_spec.c` | Add 6 new efun declarations |
| `lib/efuns/agent.c` (new) | Loop, tool dispatch, async integration |
| `lib/efuns/agent.h` (new) | Agent state struct, role/error constants |
| `lib/lpc/buffer.h` | Reuse `T_BUFFER` as-is for context window |
| `src/apply.c` | `apply_low()` used as-is for tool hook dispatch |
| `docs/internals/async-library.md` | Async completion pattern to follow |

## Verification

1. LPC test object: registers 2 tools, calls `agent_model_inference()`, verifies `before_tool_call` / `after_tool_call` fire in order, verifies loop terminates on a text-only response
2. Unit tests in `tests/test_efuns/`: tool registry add/remove, context append JSON serialization, max_turns termination
3. Test that vetoing in `before_tool_call` (return `1`) skips execution and injects error into context
4. `ctest --preset ut-linux` passes with no regressions

## Decisions

- Tool hooks are applies on the agent object — consistent with existing apply pattern in `src/apply.c`
- Loop state in external hash table, not embedded in `object_t` — no binary compatibility risk
- Context stored as JSON in `T_BUFFER` — no new LPC type needed
- LPC sees only `done_cb` — loop machinery is opaque C
