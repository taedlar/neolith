/* user.c */

#include "config.h"

inherit "trait/command_giver.c";

void write_prompt();

void create() {
  create_command_giver_trait();
  seteuid (getuid()); // enable loading inventory objects
  set_alias ("l", "look");
  set_alias ("n", "go north");
  set_alias ("s", "go south");
  set_alias ("e", "go east");
  set_alias ("w", "go west");
  set_alias ("ne", "go northeast");
  set_alias ("nw", "go northwest");
  set_alias ("se", "go southeast");
  set_alias ("sw", "go southwest");
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
  add_action("cmd_alias", "alias");
  add_action("cmd_unalias", "unalias");
  add_action("cmd_aliases", "aliases");
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
  if (!arg || arg == "") {
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
  write("  shutdown [now] - Shutdown the driver\n");
  write("  alias <n> <c>  - Set alias name <n> to command <c>\n");
  write("  unalias <n>    - Remove alias <n>\n");
  write("  aliases        - List all aliases\n");
#ifdef __PACKAGE_CURL__
  write("  curlget <url>  - Fetch a URL with the PACKAGE_CURL demo\n");
#endif
  return 1;
}

int cmd_alias (string arg) {

  if (!arg || arg == "")
    {
      write("Usage: alias <name> <command>\n");
      return 1;
    }

  // [Neolith Extension] supports C99-style mixed local declarations within normal { ... } blocks
  string name;
  string command;
  if (sscanf(arg, "%s %s", name, command) != 2)
    {
      write("Usage: alias <name> <command>\n");
      return 1;
    }

  set_alias(name, command);
  write("Alias set: " + name + " -> " + command + "\n");
  return 1;
}

int cmd_unalias (string arg) {
  if (!arg || arg == "")
    {
      write("Usage: unalias <name>\n");
      return 1;
    }

  remove_alias(arg);
  write("Alias removed: " + arg + "\n");
  return 1;
}

int cmd_aliases (string arg) {
  mapping current_aliases;
  string *names;
  int i;

  current_aliases = query_aliases();
  names = keys(current_aliases);

  if (!sizeof(names))
    {
      write("No aliases defined.\n");
      return 1;
    }

  write("Aliases:\n");
  for (i = 0; i < sizeof(names); i++)
    {
      write("  " + names[i] + " -> " + current_aliases[names[i]] + "\n");
    }

  return 1;
}

/* [NEOLITH-EXTENSION] */
static void input_prompt (string f, mixed args) {
#define CSI "\x1b["
#define CUU CSI "A" /* move cursor up one line */
#define CUD CSI "B" /* move cursor down one line */
#define CLR CSI "J" /* clear from cursor to end of screen */

  if (f == "confirm_shutdown") {
    if (mapp(args)) {
      int pos = 0;
      write("\n"); // placeholder for prompt
      foreach (string opt in args["options"]) {
        if (pos == args["cursor"])
          write("-> ");
        else
          write("   ");
        write ("  [" + opt + "]\n");
        pos++;
      }
      write (repeat_string (CUU, args["options"].len() + 1) + "\r" + args["prompt"]);
    }
  }
}

static void confirm_shutdown (string answer, mixed args) {
  int cur = args["cursor"];
  if (answer == " " || answer == "\n")
    answer = args["options"][cur];
  switch (answer)
    {
    case "Y":
    case "y":
      write(CLR "\nShutting down...\n");
      shutdown();
      return;
    case "N":
    case "n":
      write(CLR "\nShutdown cancelled.\n");
      return;
    case CUU:
      args["cursor"] = (cur - 1) % args["options"].len();
      break;
    case CUD:
      args["cursor"] = (cur + 1) % args["options"].len();
      break;
    }
  get_char ("confirm_shutdown", args);
}

int cmd_shutdown (string arg) {
  if (arg != "now")
    {
      get_char("confirm_shutdown", ([
        "prompt": "Are you sure to shutdown the MUD? ",
        "options": ({ "Y", "N" }),
        "cursor": 0
      ]));
      return 1;
    }
  confirm_shutdown("Y", 0);
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

