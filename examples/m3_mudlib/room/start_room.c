// stating room when user logins

inherit "base/room.c";

void create() {
  set_short("Starting Room");
  set_long("You are in the starting room. This room is very large but empty.");
  set_exit("north", "room/observatory.c");
}
