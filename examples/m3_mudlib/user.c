// vim: syntax=lpc
#include "m3_config.h"

private void logon()
{
  write("Welcome to the M3 Mud!\n");
  enable_commands();
  add_action("cmd_quit", "quit");
}

private int cmd_quit (string arg)
{
  write("Bye!\n");
  destruct(this_object());
}

