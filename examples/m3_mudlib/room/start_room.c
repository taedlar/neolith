// stating room when user logins

inherit "base/room.c";

void create() {
  seteuid (getuid()); // enable loading of adjacent rooms or inventory objects
  set_short ("Starting Room");
  set_long (
    "You are in the starting room. This room is quite spacious but empty. "
    "You can see a large telescope to the north."
  );
  set_exit ("north", "room/observatory.c");
}
