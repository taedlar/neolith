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

