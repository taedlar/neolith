#pragma once

#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#else
typedef int bool;
#define true 1
#define false 0
#endif /* !HAVE_STDBOOL_H */

#include "lpc/types.h"
#include "lpc/include/runtime_config.h"
#include "port/socket_comm.h"

/* port definitions as specified in runtime configuration */
typedef struct {
    int kind;
    int port;
    socket_fd_t fd;
} port_def_t;

#ifdef __cplusplus
extern "C" {
#endif
/* global trace flags as specified in runtime configuration */
extern int g_trace_flag;

/* runtime configurations */
extern int config_int[NUM_CONFIG_INTS];
extern char *config_str[NUM_CONFIG_STRS];
extern port_def_t external_port[5];
#ifdef __cplusplus
}
#endif

#define CONFIG_STR(x)           config_str[(x) - BASE_CONFIG_STR]
#define CONFIG_INT(x)           config_int[(x) - BASE_CONFIG_INT]

#define CLEAR_CONFIG_STR(x)    \
    do { if (CONFIG_STR(x)) { free(CONFIG_STR(x)); CONFIG_STR(x) = NULL; } } while(0)
#define SET_CONFIG_STR(x, val) \
    do { if (CONFIG_STR(x)) free(CONFIG_STR(x)); \
         CONFIG_STR(x) = xstrdup(val); } while(0)

#define DUMP_WITH_ARGS		0x0001
#define DUMP_WITH_LOCALVARS	0x0002

#define CONSOLE_USER        0
#define PORT_TELNET         1
#define PORT_BINARY         2
#define PORT_ASCII          3

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize runtime configurations from the specified configuration file.
 *
 * @param config_file The path to the configuration file. If NULL or empty, default
 *  configurations will be used.
 */
void init_config(const char* config_file);

/**
 * @brief Initialize the MUD application by loading from a mudlib archive.
 *
 * Files in the mudlib archive can be used in loading objects (read only).
 * TODO: Define the archive format and implement this function.
 *
 * @param archive_path The path to the mudlib archive.
 */
void init_mudlib_archive(const char* archive_path);

/**
 * @brief Initialize the MUD application with the specified master file.
 *
 * If configuration file is not specified, the parent directory of the master file will
 * be used as MudLibDir.
 *
 * If configuration file is specified, this function overrides the MasterFile setting in
 * the configuration file with the provided \p master_file argument, and loads it as the
 * master object. If \p master_file is not in the MudlibDir specified in the configuration
 * file, this function will fail and exit the driver with an error message.
 *
 * @param master_file The path to the master file.
 */
void init_application(const char* master_file);

void deinit_config();

/**
 * @brief Retrieve a runtime configuration item.
 * @param res Pointer to svalue_t where the result will be stored.
 * @param arg Pointer to svalue_t containing the configuration item number.
 * @return true if the configuration item was found and retrieved, false otherwise.
 */
bool get_config_item(svalue_t* res, svalue_t* arg);

#ifdef __cplusplus
}
#endif
