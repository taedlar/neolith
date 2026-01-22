#pragma once
#include "lpc/compiler.h"

/**
 * Information about a simul_efun function.
 */
typedef struct simul_info_s simul_info_t;

struct simul_info_s {
    compiler_function_t *func;
    function_index_t index; /* index into the program's runtime function table */
};

/**
 * The simul_efun function table.
 * 
 * Each simul_efun is referred to by its index (simul_num) in this table.
 * The index is stored in the F_SIMUL_EFUN opcode as a 16-bit unsigned integer.
 */
extern simul_info_t *simuls;

extern object_t *simul_efun_ob;
extern int simul_efun_is_loading;

/**
 * Initialize the simul_efun subsystem by loading the specified simul_efun object.
 * @param file The path to the simul_efun object file.
 */
extern void init_simul_efun(const char *file);

/**
 * Set the current simul_efun object.
 * @param ob The simul_efun object to set.
 * @return Upon return, the simul_efun object is set. Adds a reference to ob.
 */
extern void set_simul_efun (object_t *ob);

/**
 * Unset the current simul_efun object.
 * @return Upon return, the simul_efun object is unset and its reference is released.
 */
extern void unset_simul_efun();

/**
 * Find the index (simul_num) of a simul_efun function by its name string pointer.
 * The search uses binary search on the address of the name string (not string comparison).
 * 
 * The LPC compiler does not rely on this function at compile time; instead, it uses the
 * simul_num in defined_name_t (global identifier table, marked as IHE_SIMUL in set_simul_efun)
 * to accelerate the parser. This is how simul_efun integrates with the LPC compiler.
 * 
 * This function is currently used only by unit-tests.
 * 
 * @param func_name The name of the simul_efun function. It must be a shared string.
 * @return The index (simul_num) of the simul_efun function in the simul_efun table, or -1 if not found.
 */
extern int find_simul_efun(const char *func_name);

/**
 * Call a simul_efun by its index in the simul_efun table.
 * The LPC opcode F_SIMUL_EFUN uses this function.
 * @param simul_num The index in the simul_efun table. It is stored in LPC opcode as a 16-bit unsigned integer.
 * @param num_args The number of arguments on the stack.
 * @return Upon return, the return value is on the stack.
 */
extern void call_simul_efun (int simul_num, int num_args);
