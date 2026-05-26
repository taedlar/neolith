// example room for m3 mudlib

inherit "base/room.c";

void create() {
  seteuid (getuid()); // enable loading of adjacent rooms or inventory objects
  set_short("Observatory");
  set_long(
    "You are in the observatory hall. The start room is to the south. "
    "There are two wings to the east and west. "
    "The ceiling is domed and has a large telescope pointing up at the sky."
  );
  set_room_desc("telescope",
    "The telescope is large and looks like it has been here for a long time. "
    "It is currently pointed at the sky, but you can't see anything through it from here."
  );
  set_exit("east", "room/east_wing.c");
  set_exit("west", "room/west_wing.c");
  set_exit("south", "room/start_room.c");
}
