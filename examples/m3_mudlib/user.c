// vim: syntax=lpc
private void
logon()
{
  write("Welcome to the M3 Mud!\n");
  enable_commands();
  add_action("cmd_quit", "quit");
}

private int
cmd_quit (string arg)
{
  write("Bye!\n");
  destruct(this_object());
}

