Efuns library
=====
An **efun** (efunction) is a function available for LPC code to call in the mudlib. These functions are written in native C code instead of LPC, therefore you'll not find their LPC code in the mudlib.

The "e" stand for external (from the LPC perspective), in contrast with non-external functions written in LPC.
Some efuns like `shutdown()` that should be protected from unauthorized LPC code can be wrapped with **simulated efuns** written in LPC, allowing the mudlib author to fully control who can shutdown the MUD.

## Adding New Efuns
The LPMud Driver is designed to allow C programmers to add more efuns to extend LPC capabilities.

> [!IMPORTANT]
> Because Efuns are written in native C, a bug in the C code could result in serious software error such as **segmentation fault** and bring down the LPMud server process (losing unsaved data for all connecting users).
> If you are not an experienced C programmer, it is recommended to try simul_efun first.
