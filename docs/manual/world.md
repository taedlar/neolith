Neolith World Creation Guide
============================

# Tutorial: M3 Mudlib
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

# Neolith and Mudlib Interaction

If you wonder how your favorite MUD is created, and why it is so difficult to find good documentations about creating a MUD,
it is because most LPMud Driver variants hide their interactions with the Mudlib deep inside the LPC code.
It makes the knowledge about the system of a MUD almost a "secret" owned by the high level wizards of individual MUD, and
those wizards love it. This article will try to *make you one of them*. It is your decision whether you'd share the
secrets with your players, because knowing too much is not always a good thing for ordinary players.

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
### What happens when user types a command?
### What happens when LPMud Driver process is terminated?
## User Object
### Command routing
### The `heart_beat` apply
## Physics of LPC Objects
### The `reset` apply
### The `clean_up` apply
## Magics behind Wizard
### File System
### Socket


