Neolith World Creation Guide
============================

# Chapter 1: Tutorial of M3 Mudlib
Did you ever seen a LPMud mudlib in *less than 20* lines of LPC code?

Meet the [M3 Mudlib](/examples/m3_mudlib) (**M**inimum **M**akeshift **M**udlib)

- M3 Mudlib uses only the most basic LPC functions.
- Yes, you may start a MUD with M3 Mudlib.
- Yes, more than one users can connect to it. It is a MUD after all.
- Yes, you can quit from the MUD. It is not Sword Art Online.

The purpose of M3 Mudlib is to show you how a MUD world is created from scratch, and provide you a thinnest testsuite to try out things.

## Setting up
Before we can start the tutorial, please check the requirements:
- An installed `neolith` executable, either compiled by yourself or downloaded from someone else's distrubution. If you don't, please read [INSTALL](/INSTALL.md)
- A copy of the directory [m3_mudlib/](/examples/m3_mudlib).
- A copy of the file [m3.conf](/examples/m3.conf).
- Open two shell windows: One for the LPMud Driver, and the other for the client.

### Step 1
Open `m3.conf` with a text editor, find the line with `MudlibDir` followed by a path. Modify the path to fit your actual path of `m3_mudlib`.

### Step 2
Run the command:
```
$ neolith -f m3.conf
```
You are supposed to see some messages like below:
```
2022-10-06 02:03:02     {}      ===== neolith version 0.1.3 starting up =====
2022-10-06 02:03:02     {}      using locale "C.UTF-8"
2022-10-06 02:03:02     {}      using MudLibDir "/home/taedlar/m3_mudlib"
2022-10-06 02:03:02     {}      ----- loading simul efuns -----
2022-10-06 02:03:02     warnning: no simul_efun file
2022-10-06 02:03:02     {}      ----- loading master -----
2022-10-06 02:03:02     {}      ----- epilogue -----
2022-10-06 02:03:02     {}      ----- entering MUD -----
```

## Connecting to M3 MUD
Now the M3 MUD is up and running. Switch to the other shell window and type:
```
$ telnet localhost 4000
```

You can see it connects to M3 MUD and show you the welcome messages:
```
Trying 127.0.0.1...
Connected to localhost.
Escape character is '^]'.
Welcome to M3!
Umm ... but the only thing you can do is 'quit'.
```
As it has confessed, the M3 MUD does not provide you the commands like "look" or moving around. You can type any commands anyway, but the only thing you can do is "quit".

> NOTE: You can conenect as many users as you want.

If you type `quit` command, you'll disconnect from the MUD immediately.

## Terminating the M3 MUD
Let's go back to the shell window running `neolith`.

Please press Ctrl-C, and you shall see messages like below:
```
2022-10-06 02:03:02     {}      ===== neolith version 0.1.3 starting up =====
2022-10-06 02:03:02     {}      using locale "C.UTF-8"
2022-10-06 02:03:02     {}      using MudLibDir "/home/taedlar/m3_mudlib"
2022-10-06 02:03:02     {}      ----- loading simul efuns -----
2022-10-06 02:03:02     warnning: no simul_efun file
2022-10-06 02:03:02     {}      ----- loading master -----
2022-10-06 02:03:02     {}      ----- epilogue -----
2022-10-06 02:03:02     {}      ----- entering MUD -----
^C2022-10-06 02:19:31   {}      ***** process interrupted
Aborted
```
When the `neolith` process is interrupted, it disconnects all connected users, write a log message and exits.

## Meet M3 Mudlib Again (in a new way)

Start your M3 MUD again, with the following command:
```
$ neolith -f m3.conf -t 030
```

This time, you shall see some extra trace messages like below:
```
2022-10-06 12:20:22     {}      ===== neolith version 0.1.3 starting up =====
2022-10-06 12:20:22     {}      using locale "C.UTF-8"
2022-10-06 12:20:22     {}      using MudLibDir "/home/taedlar/m3_mudlib"
2022-10-06 12:20:22     {}      ----- loading simul efuns -----
2022-10-06 12:20:22     warnning: no simul_efun file
2022-10-06 12:20:22     {}      ----- loading master -----
2022-10-06 12:20:22     ["TRACE","simulate.c",407,"load_object"]        file: /master.c
2022-10-06 12:20:22     ["TRACE","interpret.c",4877,"apply_master_ob"]  no master object: "valid_object"
2022-10-06 12:20:22     ["TRACE","interpret.c",3979,"apply_low"]        not defined: "create"
2022-10-06 12:20:22     ["TRACE","interpret.c",3979,"apply_low"]        not defined: "get_root_uid"
2022-10-06 12:20:22     ["TRACE","interpret.c",3979,"apply_low"]        not defined: "get_bb_uid"
2022-10-06 12:20:22     {}      ----- epilogue -----
2022-10-06 12:20:22     ["TRACE","interpret.c",3979,"apply_low"]        not defined: "epilog"
2022-10-06 12:20:22     {}      ----- entering MUD -----
```

### What happens when M3 MUD starts?
As you can see, `neolith` prints several messages when it starts. These messages includes version numbers, locales, mudlib
directory and perhaps more informations if you are using a newer version of Neolith. Those lines with a JSON string containing
`TRACE` things are enabled by the `-t 030` argument, which tells you when and what function that Neolith calls in the M3 Mudlib.

```
2022-10-06 12:20:22     {}      ----- loading master -----
2022-10-06 12:20:22     ["TRACE","simulate.c",407,"load_object"]        file: /master.c
```
M3 Mudlib does not use Simul Efuns, as the log message has indicated. The first trace message says Neolith has loaded the
[master object](master_ob.md). The log messages are in [NDLF](https://github.com/taedlar/neolith/blob/main/docs/manual/dev.md#neolith-debug-log-format)
format that shows which Neolith function prints the message.

```
2022-10-06 12:20:22     ["TRACE","interpret.c",4877,"apply_master_ob"]  no master object: "valid_object"
```
Follwing the `load_object` function, you can see the `apply_master_ob` function says *"no master object"* when it tries to call
the `valid_object` in the master object. This is okay because master object is not acting yet before it finiishs initialization
in `create`.

> The term "apply" is LPMud's version of [delegate](https://en.wikipedia.org/wiki/Delegation_pattern) technique that calls a
> LPC function from the LPMud Driver. Don't complain for misuses of term, LPMud was developed in 1989 while the definition of
>  delegation pattern is proposed in 1994

```
2022-10-06 12:20:22     ["TRACE","interpret.c",3979,"apply_low"]        not defined: "create"
```
Neolith then calls `create` in the master object. The `apply_low` is a generic function to call apply functions in any LPC object,
including the master object. You can expect `apply_master_ob` to invoke `apply_low` after the master object has finished loading.

M3 Mudlib's master object LPC code looks like below:
```C
static object connect (int port)
{
        return new ("/user.c");
}
```

It only defines a `connect` function, which we will discuss later. M3's master object does not define the `create` function,
as the trace message has indicated. This is okay too.

The `get_root_uid` apply and `get_bb_uid` apply are not used in M3 either. Neolith then initiates the mudlib-defined `epilog`
stage that allows pre-loading important objects. Again, M3 Mudlib does not need pre-loading.

When you see the line saying **----- entering MUD -----**, it means the MUD has finished epilog stage and is ready for
accepting user connections.

### What happens when a user connects to M3 MUD?
Now you have a M3 MUD server running with `-t 030` trace flag enabled. Open another shell window and connect to your M3 MUD
with telnet:
```
$ telnet localhost 4000
```

Leave alone the telnet client and switch back to your M3 MUD server. You shall see trace messages like below:
```
2022-10-06 14:36:47     {}      ----- entering MUD -----
2022-10-06 14:37:08     ["TRACE","interpret.c",3953,"apply_low"]        call_program "connect": offset +0
2022-10-06 14:37:08     ["TRACE","simulate.c",407,"load_object"]        file: /user.c
2022-10-06 14:37:08     ["TRACE","interpret.c",3979,"apply_low"]        not defined: "valid_object"
2022-10-06 14:37:08     ["TRACE","interpret.c",3979,"apply_low"]        not defined: "creator_file"
2022-10-06 14:37:08     ["TRACE","interpret.c",3979,"apply_low"]        not defined: "create"
2022-10-06 14:37:08     ["TRACE","interpret.c",3979,"apply_low"]        not defined: "creator_file"
2022-10-06 14:37:08     ["TRACE","interpret.c",3979,"apply_low"]        not defined: "create"
2022-10-06 14:37:08     ["TRACE","interpret.c",3953,"apply_low"]        call_program "logon": offset +0
2022-10-06 14:37:08     ["TRACE","interpret.c",3979,"apply_low"]        not defined: "set_window_size"
2022-10-06 14:37:08     ["TRACE","interpret.c",3979,"apply_low"]        not defined: "set_terminal_type"
```

You now witness the first successful apply call from Neolith into M3 Mudlib. The `connect` function is defined in `/master.c`
and it only contains one line of LPC code:
```C
        return new ("/user.c");
```

The trace messages indicates that Neolith run the LPC code and call `load_object` to compile the file `/user.c`.
The `valid_object` apply is attempted by `apply_low` since `apply_master_ob` is able to find the master object.
M3 Mudlib does not define `valid_object` and it was fine. Then follows a new apply call on `creator_file`, which
is not defined by M3 Mudlib either. The `creator_file` is called on all LPC objects except the simul_efun and master
object. In a typical LPMud, the `creator_file` gives the object uid to represent the creator of the LPC object. In M3, the uid
system is not used.

Then Neolith calls `create` in the newly created object. It is not defined in `/user.c' either, and its okay as usual.

You may wonder why there are *two sequences* of `creator_file` and `create` apply calls? The answer is in a LPMud, when the
`new` function (originally called `clone_object`) creates an object, it checks if an **prototype** of the object (as
indexed by the filename) has been loaded. If the object prototype has *not* been loaded, it calls `load_object` to compile
the LPC code and creates the prototype object. Once the prototype object has been loaded, it then creates a **clone** of
the object that shares the same copy of compiled LPC code (binaries) with the prototype object, and return the cloned
object to the caller in LPC code.

> You can except a LPMud can create clones of object much faster (by sharing the compiled LPC code) once the prototype
> object has been loaded. This is one of the most basic concept in LPMud.

After the master object's `connect` function has returned a object, Neolith uses the object as **user object** that represents
a connected user, for mudlib to communicate with.

> If the `connect` function returns something other than an object, the user is disconnected immediately.

Neolith then calls the `logon` function in the user object, in order to let the object know it had *just become an user object*.

Just right before calling `logon`, Neolith also sends several **TELNET negotiation sequences** to request information of window
size and terminal type, and propose TELNET line mode.

> You won't see TELNET negotiation messages in a typical telnet client. They are a few bytes exchanged between Neolith and
> the telnet client when the connection is established. TELNET line mode lets the telnet client send a whole line to Neolith
> until ENTER is pressed. Neolith is able to do other TELNET negotiation with the telnet client by using efuns.

Let's take a look at the `logon` in `/user.c`, it only has three lines of LPC code:
```C
        write ("Welcome to M3!\nUmm ... but the only thing you can do is 'quit'.\n");
        enable_commands();
        add_action ("cmd_quit", "quit");
```
It writes a welcome message, marks the user object as able to receive user commands, and installs the `quit` command handler.
 
If your telnet client supports TELNET protocol, you may expect Neolith to receive responses shortly after the `logon` has returnedand.
and then offer these information to the user object by calling the `set_window_size` apply and `set_terminal_type` apply respectively.
The M3 Mudlib simply ignore these information.

### What happens when user types a command?
Switch to your telnet client window and --- just not quit yet --- type something else and press ENTER.

Check your M3 MUD server and you shall see these:
```
2022-10-06 18:12:50     ["TRACE","interpret.c",3979,"apply_low"]        not defined: "process_input"
2022-10-06 18:12:50     ["TRACE","interpret.c",3979,"apply_low"]        not defined: "write_prompt"
```

Neolith calls the `process_input` apply in the user object to "preprocess" the user's command it has received. In a typical LPMud,
this is where the shortcut of commands are handled. For example, the mudlib code can convert a command "l" (lower case L) into "look".
If the user object does not define `process_input`, Neolith will not call it again in order to speed up command processing.

Since you are not typeing the `'quit'` command, the M3 Mudlib will not recognize you command and print the default fail message `"What?"`.

After processing the command, Neolith calls the `write_prompt` apply in the user object to give user the hint that it is ready to
accept next command. Similarly, if the `write_prompt` apply is not defined in the user object, Neolith will not call it again and fall
back to the default prompt string `"> "`.

Now, try yo enter the `'quit'` command in the telnet client. Your connection shall be closed and below trace message is printed:
```
2022-10-06 18:59:00     ["TRACE","interpret.c",3953,"apply_low"]        call_program "cmd_quit": offset +12
```

The `add_action` in the `logon` registers the `cmd_quit` as command handler of `'quit'` (a **verb**, in LPMud's terminology) for
this user object. When Neolith receives a line of command from the client, it parses the command and take the first word as verb,
the look up the list of registered command handlers and call matching verb handlers in sequence until one of them returns a
non-zero value. The way commands are parsed allows the handler of `'quit'` to match any command string start with `'quit'` and
optionally followed by a space and arguments, for example: `'quit now'`.

In the `cmd_quit` function, there are two lines of code:
```C
        write ("Bye!\n");
        destruct (this_object());
```

Calling `destruct` for `this_object()` destroys the object where the function is defined, which is the user object. In LPMud, destroy
an user object effectively disconnects the user.

### What happens when LPMud Driver process is terminated?
The M3 Mudlib does not provide wizard command to shutdown the MUD. Instead, you can shutdown it directly by press Ctrl-C to interrupt
the process. The operating system sends SIGINT to the process and Neolith can catch it, then terminate.

```
^C2022-10-06 19:32:49   {}      ***** process interrupted
2022-10-06 19:32:49     ["TRACE","interpret.c",3979,"apply_low"]        not defined: "crash"
Aborted
```
The last line shows Neolith attempts to call the `crash` apply in master to let M3 do the some proper handlings. Besides SIGINT, SIGTERM
SIGSEGV also lead to the crash handler and terminate the MUD process.

## Tutorial Take-Aways
The tutorials of M3 Mudlib ends here.

By finish reading this far, you are able to set up a minimum mudlib and add verb handlers. Below table is a cheatsheet for you to recall
the take-aways:

Concept | Summary
--- | ---
`connect` in master object | Creates the user object when a new connection is accepted.
`logon` in user object | Initiates the other things after `connect` creates the user object.
`enable_commands` | Eables an object to take commands.
verb | The first word of a command string.
`add_action` efun | Installs a handler function to handle a verb.

# Chapter 2: Physics of LPC Objects
Before we proceed to explain about essential concepts of object physics, here is a table of more terminologies
(they are common to all LPMud variants) that will be used in the following chapters:

Terminology | Definition
--- | ---
*user object* | An object can become an user obejct by being returned from the master object's `connect` function when a new connection is accepted. You can also turn an object into user object by calling `exec` efun to transfer the user from an existing user object.
*living object* | An object becomes living object after the `enable_commands` is called. Only a living object can interact with its environment, inventory, and sibling objects in the same environment via commands. The state of living object can be removed by calling `disable_commands`.
*environment* | An object A can be moved "into" another object B, so that object B becomes the environment of object A.
*inventory* | If object B is environment of object A, then object A is one of the objects in object B's inventory.

## Creating Rooms
> Explains `say`, `shout`, `tell_object`, `tell_room`

## Creating Items
> Explains `move_object`, `environment`, `inventory`

## Creating Non-Player Characters
> Explains `heart_beat`

## Resource Management
> Explains `reset`, `clean_up`

# Chapter 3: Players

Terminology | Definition
--- | ---
*uid* | A identifier assigned by the master object's `creator_file` function when an object was created.
*euid* | A secondary uid (effective uid) for mudlib designer to implement temporary uid.

## Creating Logon Flow
> Explains `input_to`

## Creating Command Routes
> Explains `call_other`
> Implement command handlers as individual object to help resource management.

## Creating Save Files
> Explains `save_object`, `restore_object`

# Chapter 4: Security

Terminology | Definition
--- | ---
*simul efun* | A LPC function defined in the simul efun file that is made directly available to all LPC programs by the LPMud Driver.

## Creating Simul Efun
> Explains `valid_*`

## Creating Wizard Class
> Explains `enable_wizard`, `disable_wizard`
