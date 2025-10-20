## Program Structure

~~~mermaid
mindmap
  main
    comm
      process_io
      process_user_command
      add_message
    backend
      logon
      call_heart_beat
        reset
        call_out
    interpret
      apply
      eval_instruction
        efuns: f_*
    simulate
      load_object
        lpc: compile_file
      find_object
      move_object
      destruct_object
      add_action
~~~

### `comm` - Network communications
- Listens and accepts network connections.
- Process non-blocking and buffered socket I/O.
  - Dispatches user commands to the interactive objects.

### `backend` - Main loop for I/O and timer processing
- Runs the server (infinite) loop that waits for connections and incoming commands.
- Performs garbage collections.
- Provides `heart_beat()` apply, [`reset()`](/docs/applies/all/reset.md) apply, and [`call_out()`](/docs/efuns/call_out.md) function to enable animated objects.

### `interpret` - Stack machine opcode interpreter
- Provides the value stack and control stack.
  - Allows calling object methods and apply functions via `eval_instruction()`.
  - Supports **master object** for mudlib.
  - Interprets compiled opcode in LPC programs.
- Invokes efuns.

### `simulate` - Mudlib virtual world simulations
- Maps mudlib filesystem to LPC objects.
  - Load objects by calling **LPC compiler** to compile mudlib files on-demand.
  - Locate object instances via mudlib file name.
- Simulates a world object hierarchy by using [`move_object()`](/docs/efuns/move_object.md) to establish object positioning.
- Simulates interaction with objects by calling command-offering apply [`init`](/docs/applies/all/init.md) when moving objects.
