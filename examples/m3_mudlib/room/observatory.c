// example room for m3 mudlib

inherit "base/room.c";

void create() {
  set_short("Observatory");
  set_long("You are in the observatory. The start room is to the south.");
  set_exit("south", "room/start_room.c");
}
