# Declares options for the project
#
# To make option settings visible to LPC code, add corresponding #cmakedefine
# directives to lib/lpc/options.h.in which is processed by configure_file() to
# generate options.h. The `edit_source` utility reads options.h to generate
# the LPC predefines table.

include(CMakeDependentOption)

# -------------------------------------------------------------------------
# Native C/C++ build options
#
# For FETCH_*_FROM_SOURCE options, see cmake/setup.cmake which configures.
# -------------------------------------------------------------------------
cmake_dependent_option(USE_STATIC_MSVC_RUNTIME "Enable static MSVC Runtime library (-MT/-MD)" ON "MSVC" OFF)

# -------------------------------------------------------------------------
# Neolith feature options
# -------------------------------------------------------------------------
cmake_dependent_option(PACKAGE_JSON "Enable JSON efuns to_json() and from_json() (requires Boost.JSON)" ON "HAVE_BOOST_JSON" OFF)
cmake_dependent_option(PACKAGE_CURL "Enable CURL efuns for REST API calls (perform_to, perform_using, in_perform; requires libcurl)" ON "HAVE_LIBCURL" OFF)

# -------------------------------------------------------------------------
# MudOS feature options
# -------------------------------------------------------------------------
option(PACKAGE_SOCKETS "Enable support for socket efunctions" ON)
option(BINARIES "Enable support for saving compiled LPC programs as binary files (e.g., .o files)" ON)

# -------------------------------------------------------------------------
# Compatibility options
# -------------------------------------------------------------------------
set(DEFAULT_PRAGMAS "PRAGMA_WARNINGS + PRAGMA_STRICT_TYPES" CACHE STRING "Default pragma flags for all LPC files; sum of PRAGMA_* constants from lex.h (0 for none)")

option(SANE_EXPLODE_STRING "Strip at most one leading delimiter in explode()" ON)
option(REVERSIBLE_EXPLODE_STRING "Make explode() fully reversible so implode(explode(x,y),y)==x; overrides SANE_EXPLODE_STRING" ON)

option(INTERACTIVE_CATCH_TELL "Call catch_tell() on interactive objects as well as NPCs" OFF)
option(THIS_PLAYER_IN_CALL_OUT "Make this_player() available in call_out() callbacks" ON)

option(OLD_TYPE_BEHAVIOR "Reintroduce type-checking bug for backward compatibility (renders compile-time type checking useless)" OFF)
option(OLD_RANGE_BEHAVIOR "Allow negative indexes in string/buffer ranges to count from the end" ON)
option(OLD_ED "Use old ed() efun behavior, backward compatible with pre-mudlib front end" ON)
option(RESTRICTED_ED "Enable restricted ed mode in the built-in ed editor" ON)

option(COMPAT_32 "Enable LPmud 3.2/3.2.1 compatibility aliases (m_indices, map_delete, inherit_list, heart_beat_info)" OFF)

# -------------------------------------------------------------------------
# Miscellaneous options
# -------------------------------------------------------------------------
set(SAVE_EXTENSION ".o" CACHE STRING "File extension used by save_object() and restore_object()")
option(AUTO_TRUST_BACKBONE "Automatically trust objects with the backbone uid" ON)
option(NO_ANSI "Strip ANSI escape sequences (ESC chars) from user input commands" ON)
cmake_dependent_option(STRIP_BEFORE_PROCESS_INPUT "Strip ANSI escapes before passing input to process_input() as well (requires NO_ANSI)" ON "NO_ANSI" OFF)
option(CALLOUT_HANDLES "Return integer handles from call_out() for targeted removal" ON)
option(PRIVS "Enable the object privileges system (requires mudlib support via privs_file apply)" OFF)
option(NO_SHADOWS "Disable shadow support in the driver" ON)
option(PROFILE_FUNCTIONS "Measure CPU time used by user-defined LPC functions (adds overhead to function headers)" OFF)

option(STRING_TYPE_SAFETY "Enable always-on runtime contract checks for STRING_MALLOC/STRING_SHARED boundary functions, including in release builds; typed aliases (shared_str_t/malloc_str_t) are always defined" ON)
option(STRING_STATS "Track statistics about allocated strings for mud_status()" ON)
option(ARRAY_STATS "Track statistics about allocated arrays" ON)
option(CACHE_STATS "Collect call_other cache statistics; adds HAS_CACHE_STATS LPC predefined macro" ON)
option(LOG_CATCHES "Send errors caught by catch() to the debug log" ON)

# -------------------------------------------------------------------------
# Performance tuning options
# -------------------------------------------------------------------------
set(HEARTBEAT_INTERVAL 2000000 CACHE STRING "Interval in microseconds between heartbeat() calls for individual LPC objects")
set(HEART_BEAT_CHUNK 32 CACHE STRING "Number of heart_beat list entries allocated at a time")

option(FLUSH_OUTPUT_IMMEDIATELY "Write output to sockets immediately as generated (debug)" OFF)

option(LAZY_RESETS "Only call reset() when an object is touched via call_other() or move_object()" OFF)
set(CALLOUT_CYCLE_SIZE 32 CACHE STRING "Number of slots in the call_out list (power of 2 recommended)")

set(SMALL_STRING_SIZE 100 CACHE STRING "Size threshold for small strings")
set(MESSAGE_BUFFER_SIZE 4096 CACHE STRING "Size of the output buffer for messages sent to users")
set(LARGEST_PRINTABLE_STRING 8192 CACHE STRING "Size of the vsprintf() buffer in add_message()")

set(MAX_SAVE_SVALUE_DEPTH 25 CACHE STRING "Maximum nesting depth when saving LPC data structures (prevents infinite recursion)")

set(APPLY_CACHE_BITS 11 CACHE STRING "Number of bits used in the call_other cache (6-11 typical)")
option(COMPRESS_FUNCTION_TABLES "Reduce function table memory at a slight CPU cost" ON)
