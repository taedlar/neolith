// example room for m3 mudlib

inherit "base/room.c";

void create() {
  seteuid (getuid()); // enable loading of adjacent rooms or inventory objects
  set_short("East Wing");
  set_long(
    "You are in the east wing of the observatory. "
    "There are various scientific instruments here, but they all look old and dusty. "
    "The observatory hall is to the west."
  );
  set_room_desc("instruments",
    "The instruments are covered in dust and cobwebs. They look like they haven't been used in years."
  );
  set_exit("west", "room/observatory.c");
}
