// stating room when user logins

inherit "base/room.c";

void create() {
  seteuid (getuid()); // enable loading room objects
  set_short ("Starting Room");
  set_long (
    "You are in the starting room.\nThis room is quite spacious but empty.\n"
    "You can see a large telescope to the north.\n"
  );
  set_exit ("north", "room/observatory.c");
}
