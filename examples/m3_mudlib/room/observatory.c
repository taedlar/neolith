// example room for m3 mudlib

inherit "base/room.c";

void create() {
  set_short("Observatory");
  set_long("You are in the observatory. There is an exit to the south.");
  set_exit("south", "room/start_room.c");
}
