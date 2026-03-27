/**
 * options.h
 *
 * Defines for the legacy compile-time configurations of the MudOS driver.
 * Several options have been removed either because they were no longer
 * supported in Neolith, or they become mandatory not cannot be turned off.
 * 
 * NOTE: This file is also processed by the edit_source tool to generate
 * LPC preprocessor predefined macros. The following special rules apply to
 * this file:
 * - Only #define macros are processed; all other code is ignored.
 * - Macro names starting with '_' are filtered out and not generated as
 *   predefined macros. 
 */

#ifndef _OPTIONS_H
#define _OPTIONS_H

/****************************************************************************
 *                          COMPATIBILITY                                   *
 *                         ---------------                                  *
 * The MudOS driver has evolved quite a bit over the years.  These defines  *
 * are mainly to preserve old behavior in case people didn't want to        *
 * rewrite the relevant portions of their code.                             *
 *                                                                          *
 * WARNING: If you are using software designed to run with the MudOS driver *
 *          it may assume certain settings of these options.  Check the     *
 *          instructions for details.                                       *
 ****************************************************************************/

/* explode():
 *
 * The old behavior (#undef both of the below) strips any number of
 * delimiters at the start of the string, and one at the end.  So
 * explode("..x.y..z..", ".") gives ({ "x", "y", "", "z", "" })
 *
 * SANE_EXPLODE_STRING strips off at most one leading delimiter, and
 * still strips off one at the end, so the example above gives
 * ({ "", "x", "y", "", "z", "" }).
 *
 * REVERSIBLE_EXPLODE_STRING overrides SANE_EXPLODE_STRING, and makes
 * it so that implode(explode(x, y), y) is always x; i.e. no delimiters
 * are every stripped.  So the example above gives
 * ({ "", "", "x", "y", "", "z", "", "" }).
 */
#define SANE_EXPLODE_STRING
#define REVERSIBLE_EXPLODE_STRING

/* CAST_CALL_OTHERS: define this if you want to require casting of call_other's;
 *   this was the default behavior of the driver prior to this addition.
 */
#undef CAST_CALL_OTHERS

/* OLD_TYPE_BEHAVIOR: reintroduces a bug in type-checking that effectively
 * renders compile time type checking useless.  For backwards compatibility.
 */
#undef OLD_TYPE_BEHAVIOR

/* OLD_RANGE_BEHAVIOR: define this if you want negative indexes in string
 * or buffer range values (not lvalue, i.e. x[-2..-1]; for e.g. not 
 * x[-2..-1] = foo, the latter is always illegal) to mean counting from the 
 * end 
 */
#define OLD_RANGE_BEHAVIOR

/* OLD_ED: ed() efun backwards compatible with the old version.  The new
 * version requires/allows a mudlib front end.
 */
#define OLD_ED

/* RESTRICTED_ED: define this if you want restricted ed mode enabled.
 */
#define RESTRICTED_ED

/****************************************************************************
 *                           MISCELLANEOUS                                  *
 *                          ---------------                                 *
 * Various options that affect the way the driver behaves.                  *
 *                                                                          *
 * WARNING: If you are using software designed to run with the MudOS driver *
 *          it may assume certain settings of these options.  Check the     *
 *          instructions for details.                                       *
 ****************************************************************************/

/*
 * Some minor tweaks that make it a bit easier to run code designed to run
 * on LPmud 3.2/3.2.1.  Currently has the following effects:
 * 
 * . m_indices() and m_values() are synonyms for keys() and values(),
 *   respectively
 * . map_delete() returns it's first argument
 * . inherit_list() means deep_inherit_list(), not shallow_inherit_list()
 * . heart_beat_info() is a synonym for heart_beats()
 */
#undef COMPAT_32

/*
 * Keep statistics about allocated strings, etc.  Which can be veiwed with
 * the mud_status() efun.  If this is off, mud_status() and memory_info()
 * ignore allocated strings, but string operations run faster.
 */
#define STRING_STATS

/*
 * Similarly for arrays ...
 */
#define ARRAY_STATS

/* LOG_CATCHES: define this to cause errors that are catch()'d to be
 *   sent to the debug log anyway.
 *
 * On by default, because newer libs use catch() alot, and it's confusing
 * if the errors don't show up in the logs.
 */
#define LOG_CATCHES

/* DEFAULT_PRAGMAS:  This should be a sum of pragmas you want to always
 * be on, i.e.
 *
 * #define DEFAULT_PRAGMAS PRAGMA_STRICT_TYPES + PRAGMA_SAVE_TYPES
 *
 * will make every LPC file behave as if it had the lines:
 * #pragma strict_types
 * #pragma save_types
 *
 * for no default pragmas:
 * #define DEFAULT_PRAGMAS 0
 *
 * If you don't know what these are, 0 is a good choice.
 *
 * Supported pragmas:
 * PRAGMA_STRICT_TYPES: enforces strict type checking
 * PRAGMA_WARNINGS:     issues warnings about various dangerous things in
 *                      your code
 * PRAGMA_SAVE_TYPES:   save the types of function arguments for checking
 *                      calls to functions in this object by objects that
 *                      inherit it.
 * PRAGMA_SAVE_BINARY:  save a compiled binary version of this file for
 *                      faster loading next time it is needed.
 * PRAGMA_OPTIMIZE:     make a second pass over the generated code to
 *                      optimize it further.  currently does jump threading.
 * PRAGMA_ERROR_CONTEXT:include some text telling where on the line a
 *                      compilation error occured.
 * (note: definitions of PRAGMA_* are in lex.h)
 */
#define DEFAULT_PRAGMAS PRAGMA_WARNINGS + PRAGMA_STRICT_TYPES

/* AUTO_TRUST_BACKBONE: define this if you want objects with the backbone
 *   uid to automatically be trusted and to have their euid set to the uid of
 *   the object that forced the object's creation.
 */
#define AUTO_TRUST_BACKBONE

/* LAZY_RESETS: if this is defined, an object will only have reset()
 *   called in it when it is touched via call_other() or move_object()
 *   (assuming enough time has passed since the last reset).  If LAZY_RESETS
 *   is #undef'd, then reset() will be called as always (which guaranteed that
 *   reset would always be called at least once).  The advantage of lazy
 *   resets is that reset doesn't get called in an object that is touched
 *   once and never again (which can save memory since some objects won't get
 *   reloaded that otherwise would).
 */
#undef LAZY_RESETS

/* COMPRESS_FUNCTION_TABLES: Causes function tables to take up significantly
 * less memory, at the cost of a slight increase in function call overhead
 * (speed).
 */
#define COMPRESS_FUNCTION_TABLES

/* SAVE_EXTENSION: defines the file extension used by save_object().
 *   and restore_object().  Some sysadmins run scripts that periodically
 *   scan for and remove files ending in .o (but many mudlibs are already
 *   set up to use .o thus we leave .o as the default).
 */
#define SAVE_EXTENSION ".o"

/* NO_ANSI: define if you wish to disallow users from typing in commands that
 *   contain ANSI escape sequences.  Defining NO_ANSI causes all escapes
 *   (ASCII 27) to be replaced with a space ' ' before the string is passed
 *   to the action routines added with add_action.
 *
 * STRIP_BEFORE_PROCESS_INPUT allows the location where the stripping is 
 * done to be controlled.  If it is defined, then process_input() doesn't
 * see ANSI characters either; if it is undefined ESC chars can be processed
 * by process_input(), but are stripped before add_actions are called.
 * Note that if NO_ADD_ACTION is defined, then #define NO_ANSI without
 * #define STRIP_BEFORE_PROCESS_INPUT is the same as #undef NO_ANSI.
 *
 * If you anticipate problems with users intentionally typing in ANSI codes
 * to make your terminal flash, etc define this.
 */
#define NO_ANSI
#define STRIP_BEFORE_PROCESS_INPUT

/* THIS_PLAYER_IN_CALL_OUT: define this if you wish this_player() to be
 *   usable from within call_out() callbacks.
 */
#define THIS_PLAYER_IN_CALL_OUT

/* CALLOUT_HANDLES: If this is defined, call_out() returns an integer, which
 * can be passed to remove_call_out() or find_call_out().  Removing call_outs
 * by name is still allowed, but is significantly less efficient, and also
 * doesn't work for function pointers.  This option adds 4 bytes overhead
 * per callout to keep track of the handle.
 */
#define CALLOUT_HANDLES

/* FLUSH_OUTPUT_IMMEDIATELY: Causes output to be written to sockets
 * immediately after being generated.  Useful for debugging.  
 */
#undef FLUSH_OUTPUT_IMMEDIATELY

/* PRIVS: define this if you want object privledges.  Your mudlib must
 *   explicitly make use of this functionality to be useful.  Defining this
 *   this will increase the size of the object structure by 4 bytes (8 bytes
 *   on the DEC Alpha) and will add a new master apply during object creation
 *   to "privs_file".  In general, priveleges can be used to increase the
 *   granularity of security beyond the current root uid mechanism.
 *
 * [NOTE: for those who'd rather do such things at the mudlib level, look at
 *  the inherits() efun and the 'valid_object' apply to master.]
 */
#undef PRIVS

/* INTERACTIVE_CATCH_TELL: define this if you want catch_tell called on
 *   interactives as well as NPCs.  If this is defined, user.c will need a
 *   catch_tell(msg) method that calls receive(msg);
*/
#undef INTERACTIVE_CATCH_TELL

/* NO_SHADOWS: define this if you want to disable shadows in your driver.
 */
#define NO_SHADOWS

/* PROFILE_FUNCTIONS: define this to be able to measure the CPU time used by
 *   all of the user-defined functions in each LPC object.  Note: defining
 *   this adds three long ints (12 bytes on 32-bit machines) to the function
 *   header structs.  Also note that the resolution of the getrusage() timer
 *   may not be high enough on some machines to give non-zero execution
 *   times to very small (fast) functions.  In particular if the clock
 *   resolution is 1/60 of a second, then any time less than approxmately 15k
 *   microseconds will resolve to zero (0).
 */
#undef PROFILE_FUNCTIONS

/* BINARIES: define this to enable the 'save_binary' pragma.
 *   This pragma, when set in a program, will cause it to save a
 *   binary image when loaded, so that subsequent loadings will
 *   be much faster.  The binaries are saved in the directory
 *   specified in the configuration file.  The binaries will not
 *   load if the LPC source or any of the inherited or included
 *   files are out of date, in which case the file is compiled
 *   normally (and may save a new binary).
 *
 *   In order to save the binary, valid_save_binary() is called
 *   in master.c, and is passed the name of the source file.  If
 *   this returns a non-zero value, the binary is allowed to be
 *   saved.  Allowing any file by any wizard to be saved as a
 *   binary is convenient, but may take up a lot of disk space.
 */
#define BINARIES

/****************************************************************************
 *                              PACKAGES                                    *
 *                              --------                                    *
 * Defining some/all of the following add certain efuns, and sometimes      *
 * add/remove code from the driver.                                         *
 *                                                                          *
 * In Neolith, the preferred way to enable/disable features is via CMake    *
 * options, which will set the appropriate #defines in config.h. Defining   *
 * these macros manually overrides the config.h settings, which may cause   *
 * unexpected behavior.                                                     *
 *                                                                          *
 * Check CMakeLists.txt for default enabled packages, and the CMake options *
 * to control them.                                                         *
 ****************************************************************************/

/* PACKAGE_SOCKETS: define this to enable the socket efunctions.
 */
#define PACKAGE_SOCKETS

/*************************************************************************
 *                       FOR EXPERIENCED USERS                           *
 *                      -----------------------                          *
 * Most of these options will probably be of no interest to many users.  *
 *************************************************************************/

/* HEARTBEAT_INTERVAL: define heartbeat interval in microseconds (us).
 *   1,000,000 us = 1 second.  The value of this macro specifies
 *   the frequency with which the heart_beat method will be called in
 *   those LPC objects which have called set_heart_beat(1).
 *
 * [NOTE: if ualarm() isn't available, alarm() is used instead.  Since
 *  alarm() requires its argument in units of a second, we map 1 - 1,000,000 us
 *  to an actual interval of one (1) second and 1,000,001 - 2,000,000 maps to
 *  an actual interval of two (2) seconds, etc.]
 */
#define HEARTBEAT_INTERVAL 2000000

/* 
 * CALLOUT_CYCLE_SIZE: This is the number of slots in the call_out list.
 * It should be approximately the average number of active call_outs, or
 * a few times smaller.  It should also be a power of 2, and also be relatively
 * prime to any common call_out lengths.  If all this is too confusing, 32
 * isn't a bad number :-)
 */
#define CALLOUT_CYCLE_SIZE 32

/* LARGEST_PRINTABLE_STRING: defines the size of the vsprintf() buffer in
 *   comm.c's add_message(). Instead of blindly making this value larger,
 *   mudlib should be coded to not send huge strings to users.
 */
#define LARGEST_PRINTABLE_STRING 8192

/* MESSAGE_BUFFER_SIZE: determines the size of the buffer for output that
 *   is sent to users.
 */
#define MESSAGE_BUFFER_SIZE 4096

/* APPLY_CACHE_BITS: defines the number of bits to use in the call_other cache
 *   (in interpret.c).  Somewhere between six (6) and ten (10) is probably
 *   sufficient for small muds.
 */
#define APPLY_CACHE_BITS 11

/* CACHE_STATS: define this if you want call_other (apply_low) cache 
 * statistics.  Causes HAS_CACHE_STATS to be defined in all LPC objects.
 */
#define CACHE_STATS

/* HEART_BEAT_CHUNK: The number of heart_beat chunks allocated at a time.
 * A large number wastes memory as some will be sitting around unused, while
 * a small one wastes more CPU reallocating when it needs to grow.  Default
 * to a middlish value.
 */
#define HEART_BEAT_CHUNK      32

/* Some maximum string sizes
 */
#define SMALL_STRING_SIZE     100
#define LARGE_STRING_SIZE     1000
#define COMMAND_BUF_SIZE      2000

/* Number of levels of nested datastructures allowed -- this limit prevents
 * crashes from occuring when saving objects containing variables containing
 * recursive datastructures (with circular references).
 */
#define MAX_SAVE_SVALUE_DEPTH 25

#endif /* _OPTIONS_H */
