Neolith LPMud Driver Internals
==============================

> :pushpin: The contents of this article only applies to Neolith, which is different from original LPMud and MudOS.

# Starting Up

When the LPMud Driver process is started, it goes through the follow steps in sequence before begining accept connections:

1. Process command line arguments
2. Process configuration file
3. Initialize LPC virtual machine
4. Initialize LPC compiler
5. Load simul efun object (optional)
6. Load master object
7. Do epilogue

# Backend Mode

After done start up, the LPMud Driver goes into backend mode that **accepts user connections** while doing necessary house keep tasks in the background.

In the backend mode, the LPMud driver provides:
- Accept connections on the TCP port specified in configuration file.
- Compile and load extra objects when your LPC program requires.
- Animate objects by calling `heart_beat()` for objects that has enabled heart beat.
- Call scheduled functions that has been installed by `call_out()`.
- Renew the world by calling `reset()` periodically for all objects.
- Free up unused objects by calling `clean_up()` for objects that has been idle for a while.

The LPMud Driver stays in backend mode forever, until `shutdown()` is called or otherwise terminated.

# LPC Objects

> Anything in the world of a LPMud is a **LPC object**, which is created from a LPC program file. In fact, a LPC object is named using
> the filename (local to Mudlib directory) internally in the LPMud driver.

Once the LPMud Driver has entered backend mode, you have at least one LPC object created: the master object. Depend on the mudlib's design,
it may load more objects in `epilog` before it is ready to accept user connections.
