// example master object for m3 mudlib

private object connect (int port) {
  return new ("user.c");
}

string creator_file (string file) {
  return "Root";
}

string get_root_uid() {
  return "Root";
}

string get_bb_uid() {
  return "Root";
}

int valid_seteuid (object ob, string newuid) {
  return 1; // allow all seteuid calls for simplicity
}
