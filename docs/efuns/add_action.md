# add_action()
## NAME
**add_action** - bind a command verb to a local function

## SYNOPSIS
~~~cxx
varargs void add_action( string | function fun, string | string * cmd, int flag, ... );
~~~

## DESCRIPTION
Set up a local function **fun** to be called when user input
matches the command **cmd**. Functions called by a player
command will receive the command arguments as the first parameter (a string),
followed by any additional arguments passed to `add_action`.
The function must return 0 if it was the wrong command, otherwise 1.

If the second argument is an array, then all the commands in
the array will call the second function.  It is possible to
find out which command called the function with
query_verb().

If it was the wrong command, the parser will continue
searching for another command, until one returns true or
give error message to player.

Usually add_action() is called only from an init() routine.
The object that defines commands must be present to the
player, either being the player, being carried by the
player, being the room around the player, or being an object
in the same room as the player.

### Flag Semantics

If argument **flag** is 1, then only the leading characters of the command has to match the verb **cmd**.

- **Non-empty verb** (e.g., `add_action(func, "'", 1)`): When a command matches the first characters of **cmd**, the handler receives the portion of the user input following the matched verb. For example, with `add_action(func, "'", 1)` and user input `'hello world`, the handler receives `hello world` as the argument. The `query_verb()` function returns the entire matched verb (`'hello`).

- **Empty verb** (e.g., `add_action(func, "", 1)`): An empty verb acts as a wildcard that matches any first token (word) of the command. The matched token becomes the verb, and the remainder becomes the argument. For example, with `add_action(func, "", 1)` and user input `dance quickly now`, the handler receives `quickly now` as the argument. The `query_verb()` function returns the matched verb (`dance`). This empty verb behavior is compatible with MudOS behavior.

If argument **flag** is 2, then only the leading characters must match, but `query_verb()` will only return the characters following **cmd**.

## IMPLEMENTATION NOTES
In Neolith, `add_action` supports carryover arguments which are passed to the command handler
along with the command arguments. This allows context to be captured at `init()` time and
available when the command executes, eliminating the need for global state or unreliable
`this_player()` calls.

## EXAMPLES
~~~cxx
// Basic usage (backward compatible)
void init() {
    add_action("do_climb", "climb");
}

int do_climb(string args) {
    // args = whatever user typed after "climb"
    write("You climb " + args + "\n");
    return 1;
}

// With carryover arguments to capture context
void init() {
    object player = this_player();
    mapping zone_info = query_zone_info();
    
    add_action("cmd_attack", "attack", 0, player, zone_info);
    add_action("cmd_take", ({"take", "get"}), 0, player);
}

int cmd_attack(string args, object who, mapping zone) {
    // args = command arguments (e.g., "orc")
    // who = player captured at init() time
    // zone = context data
    
    if (zone["safe_zone"]) {
        write("No combat in safe zones!\n");
        return 1;
    }
    
    // Attack logic with reliable context
    return 1;
}

int cmd_take(string args, object who) {
    // args = item name
    // who = player context
    write(who->query_name() + " takes " + args + "\n");
    return 1;
}

// Using function pointers with V_NOSPACE flag
void init() {
    add_action((: cmd_emote :), "!", 1, this_player());
}

int cmd_emote(string args, object who) {
    // With V_NOSPACE (flag=1), args includes everything after "!"
    write(who->query_name() + " " + args + "\n");
    return 1;
}
~~~

## SEE ALSO
[query_verb()](query_verb.md), [remove_action()](remove_action.md), [init()](init.md)
