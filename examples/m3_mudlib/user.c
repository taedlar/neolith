// example user object for m3 mudlib

#include "config.h"

static object my_env;

void write_prompt();

void create() {
  seteuid (getuid()); // enable loading inventory objects
}

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
  return 1;
}

int cmd_shutdown (string arg) {
  write("Shutting down...\n");
  shutdown();
  return 1;
}

// Catch-all for unknown commands
int catch_tell(string msg) {
  // Ignore unknown command messages for cleaner output
  return 0;
}

