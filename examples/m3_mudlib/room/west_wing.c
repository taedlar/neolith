// example room for m3 mudlib

inherit "base/room.c";

void create() {
  seteuid (getuid()); // enable loading of adjacent rooms or inventory objects
  set_short("West Wing");
  set_long(
    "You are in the west wing of the observatory. "
    "There are various scientific instruments here, but they all look old and dusty. "
    "The observatory hall is to the east."
  );
  set_room_desc("instruments",
    "The instruments are mostly broken and covered in dust."
  );
  set_exit("east", "room/observatory.c");
}
