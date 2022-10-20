// vim: syntax=lpc
private void
logon()
{
  write("Welcome to M3!\nUmm ... but the only thing you can do is 'quit'.\n");
  enable_commands();
  add_action("cmd_quit", "quit");
}

private int
cmd_quit (string arg)
{
  write("Bye!\n");
  destruct(this_object());
}

