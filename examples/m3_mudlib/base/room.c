/* base/room.c */

string short_desc;
string long_desc;
mapping room_desc;
mapping exits;

void set_short (string s) {
  short_desc = s;
}

void set_long (string s) {
  long_desc = s;
}

void set_room_desc (string desc_name, string desc) {
  if (!room_desc) {
    room_desc = ([ desc_name: desc ]);
  } else {
    room_desc[desc_name] = desc;
  }
}

void set_exit (string dir, string dest) {
  if (!exits) {
    exits = ([ dir: dest ]);
  } else {
    exits[dir] = dest;
  }
}

string query_exit (string dir) {
  if (!exits)
    return 0;
  return exits[dir];
}

void init() {
  add_action ("cmd_look", "look");
  add_action ("cmd_go", "go");
}

int cmd_look (string arg) {
  if (!arg || arg == "") {
    write (short_desc + "\n");
    write ("    " + textwrap (long_desc, 70) + "\n");
    if (sizeof(exits) == 1) {
      write ("    There is only one obvious exit: " + keys (exits)[0] + ".\n");
    } else if (sizeof(exits) > 1) {
      write ("    Obvious exits are: " + implode (keys (exits), ", ") + ".\n");
    } else {
      write ("    There are no obvious exits.\n");
    }
    return 1;
  }
  if (room_desc && room_desc[arg]) {
    write (textwrap (room_desc[arg], 70) + "\n");
    return 1;
  }
  notify_fail ("You don't see that here.\n");
  return 0;
}

int cmd_go (string arg) {
  string dest;
  if (!arg || arg == "") {
    write ("Go where?\n");
    return 1;
  }
  dest = query_exit(arg);
  if (dest) {
    this_player()->move (find_object (dest, 1));
    return 1;
  }
  notify_fail ("You can't go that way.\n");
  return 0;
}
