// vim: syntax=lpc
#include "m3_config.h"

inherit "char.c";

private void logon()
{
  write("Welcome to the M3 Mud!\n");
  write("Type 'help' for available commands.\n");
  enable_commands();
  add_action("cmd_quit", "quit");
  add_action("cmd_say", "say");
  add_action("cmd_help", "help");
  add_action("cmd_shutdown", "shutdown");
}

private int cmd_quit (string arg)
{
  write("Bye!\n");
  destruct(this_object());
  return 1;
}

private int cmd_say (string arg)
{
  if (!arg || arg == "")
  {
    write("Say what?\n");
    return 1;
  }
  write("You say: " + arg + "\n");
  return 1;
}

private int cmd_help (string arg)
{
  write("Available commands:\n");
  write("  say <message>  - Say something\n");
  write("  help           - Show this help\n");
  write("  quit           - Exit the MUD\n");
  write("  shutdown       - Shutdown the driver\n");
  return 1;
}

private int cmd_shutdown (string arg)
{
  write("Shutting down...\n");
  shutdown();
  return 1;
}

// Catch-all for unknown commands
int catch_tell(string msg)
{
  // Ignore unknown command messages for cleaner output
  return 0;
}

