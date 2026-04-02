// example user object for m3 mudlib

#include "config.h"

static object my_env;

void write_prompt();

void create() {
  seteuid (getuid()); // enable loading inventory objects
}

#ifdef __PACKAGE_CURL__
void handle_curlget_result(int ok, mixed payload, string url) {
  string payload_str;

  /* perform_to() returns a buffer on success and a string on failure;
   * normalize to string for display. */
  payload_str = sprintf("%s", payload);

  if (ok) {
    write("CURL response from " + url + ":\n");
    write(payload_str);
    if (strlen(payload_str) == 0 || payload_str[<1] != '\n') {
      write("\n");
    }
  }
  else {
    write("CURL request failed for " + url + ": " + payload_str + "\n");
  }
  write_prompt();
}
#endif

void logon() {
  write("=========================================\n");
  write("Welcome to M3: the Minimal-Makeshift-MUD!\n");
  write("=========================================\n\n");

  // Set up basic commands for the user
  enable_commands();
  add_action("cmd_quit", "quit");
  add_action("cmd_say", "say");
  add_action("cmd_help", "help");
  add_action("cmd_shutdown", "shutdown");
#ifdef __PACKAGE_CURL__
  add_action("cmd_curlget", "curlget");
#endif

  // Move the player to the starting room
  move_object (find_object("room/start_room.c", 1));
  command ("look"); // Look around on login
  write_prompt();
}

void write_prompt() {
  write("> ");
}

void move(object dest) {
  if (!objectp(dest)) {
    error("Destination must be an object.\n");
    return;
  }
  move_object(dest);
  command ("look");
}

int cmd_quit (string arg) {
  write("Bye!\n");
  destruct(this_object());
  return 1;
}

int cmd_say (string arg) {
  if (!arg || arg == "")
  {
    write("Say what?\n");
    return 1;
  }
  write("You say: " + arg + "\n");
  return 1;
}

int cmd_help (string arg) {
  write("Available commands:\n");
  write("  say <message>  - Say something\n");
  write("  help           - Show this help\n");
  write("  quit           - Exit the MUD\n");
  write("  shutdown       - Shutdown the driver\n");
#ifdef __PACKAGE_CURL__
  write("  curlget <url>  - Fetch a URL with the PACKAGE_CURL demo\n");
#endif
  return 1;
}

int cmd_shutdown (string arg) {
  write("Shutting down...\n");
  shutdown();
  return 1;
}

#ifdef __PACKAGE_CURL__
int cmd_curlget (string arg) {
  if (!arg || arg == "") {
    write("Usage: curlget <url>\n");
    return 1;
  }

  if (in_perform()) {
    write("A CURL transfer is already in progress for this user.\n");
    return 1;
  }

  perform_using("url", arg);
  perform_using("follow_location", 1);
  perform_using("timeout_ms", 5000);
  perform_to("handle_curlget_result", 0, arg);

  write("Fetching " + arg + " ...\n");
  return 1;
}
#endif

// Catch-all for unknown commands
int catch_tell(string msg) {
  // Ignore unknown command messages for cleaner output
  return 0;
}

