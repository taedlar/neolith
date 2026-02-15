// example room

string short_desc;
string long_desc;
mapping exits = ([]);

void set_short (string s) {
  short_desc = s;
}

void set_long (string s) {
  long_desc = s;
}

void set_exit (string dir, string dest) {
  exits[dir] = dest;
}

void init() {
  add_action("cmd_look", "look");
  add_action("cmd_go", "go");
}

int cmd_look (string arg) {
  if (!arg || arg == "") {
    write(short_desc + "\n");
    write("    " + long_desc + "\n");
    if (sizeof(exits) == 1) {
      write("There is only one obvious exit: " + implode(keys(exits), ", ") + ".\n");
    } else if (sizeof(exits) > 1) {
      write("Obvious exits are: " + implode(keys(exits), ", ") + ".\n");
    } else {
      write("There are no obvious exits.\n");
    }
    return 1;
  }
  notify_fail("You don't see that here.\n");
  return 0;
}

int cmd_go (string arg) {
  if (!arg || arg == "") {
    write("Go where?\n");
    return 1;
  }
  if (exits[arg]) {
    this_player()->move(exits[arg]);
    return 1;
  }
  notify_fail("You can't go that way.\n");
  return 0;
}
