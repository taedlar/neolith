# epilog
## NAME
epilog - returns an array of the filenames of the objects to be preloaded.

## SYNOPSIS
~~~cxx
string *epilog (int level);

void preload (string filename);
~~~

## DESCRIPTION
Since LPMud 2.4.5, the driver calls `epilog()` in master after the master object has been loaded.

Mudlibs typically use epilog to initialize data structures in master (such as security tables etc).
`epilog()` should returns an array of filenames which correspond to objects that the mudlib wants to have preloaded; that is, loaded before the first player logs in.
For each filename returned in the array, the driver will called `preload(filename)` in master.

The *level* variable (used to mean *load_empty*) is an integer specified in the `-e` option when starting up the driver.
In LPMud 2.4.5, it can be used as a signal to the mudlib to not load "castles", etc.
(**Castles** are rooms meant to be used by wizards only. Sometimes these rooms may contain experimental stuff that may take up system resources.)

In later development of LPMud, the concept of castle is removed from the driver.
This is kept in Neolith as a tribute to the old days, and it is up to MUD designer for interpretion of this integer.

## SEE ALSO
[preload()](preload.md)
