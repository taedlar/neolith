// vim: syntax=lpc
private void
logon()
{
  hello_world(); // a simul_efun call
  enable_commands();
  add_action("cmd_quit", "quit");
}

private int
cmd_quit (string arg)
{
  write("Bye!\n");
  destruct(this_object());
}

