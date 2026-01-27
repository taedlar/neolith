#pragma once
#include "hash.h"
#include "identifier.h"

#define DEFMAX 10000
#define MAXLINE 1024
#define MLEN 4096
#define NSIZE 256
#define MAX_INSTRS 512
#define EXPANDMAX 25000
#define NARGS 25
#define MARKS '@'

#define DEFAULT_NONE           0xff
#define DEFAULT_THIS_OBJECT    0xfe

#define PRAGMA_STRICT_TYPES    1
#define PRAGMA_WARNINGS        2
#define PRAGMA_SAVE_TYPES      4
#define PRAGMA_SAVE_BINARY     8
#define PRAGMA_OPTIMIZE       16
#define PRAGMA_ERROR_CONTEXT  32
#define PRAGMA_OPTIMIZE_HIGH  64

typedef struct ifstate_s {
    struct ifstate_s *next;
    int state;
} ifstate_t;

typedef struct defn_s {
    struct defn_s *next;
    char *name;
    char *exps;
    int flags;
    int nargs;
} defn_t;

/* must be a power of 4 */
#define DEFHASH 64
#define defhash(s) (whashstr((s), 10) & (DEFHASH - 1))

#define DEF_IS_UNDEFINED 1
#define DEF_IS_PREDEF    2
/* used only in edit_source */
#define DEF_IS_NOT_LOCAL 4

/**
 * @brief Linked list of predefined macros to be added at the start of compilation.
 * These can be specified at the command line using -D option.
 */
typedef struct lpc_predef_s lpc_predef_t;
struct lpc_predef_s {
    const char *expression; /* static pointer to command line arguments */
    lpc_predef_t *next;
};
extern lpc_predef_t *lpc_predefs;

#define EXPECT_ELSE 1
#define EXPECT_ENDIF 2

#define isalunum(c) (isalnum(c) || (c) == '_')

/*
 * Information about all instructions. This is not really needed as the
 * automatically generated efun_arg_types[] should be used.
 */

/* indicates that the instruction is only used at compile time */
#define F_ALIAS_FLAG 1024

typedef struct {
    short max_arg, min_arg;     /* Can't use char to represent -1 */
    short type[4];              /* need a short to hold the biggest type flag */
    short Default;
    short ret_type;
    char *name;
#ifdef LPC_TO_C
    char *routine;
#endif
    int arg_index;
} instr_t;

/*
 * lex.c
 */
extern instr_t instrs[MAX_INSTRS];
extern int current_line;
extern int current_line_base;
extern int current_line_saved;
extern int total_lines;
extern char *current_file;
extern int current_file_id;
extern int pragmas;
extern int num_parse_error;
extern int efun_arg_types[];
extern char yytext[MAXLINE];
extern keyword_t predefs[];

size_t init_keywords (void);
void init_predefines (void);

void push_function_context(void);
void pop_function_context(void);

int yylex(void);

void init_instrs(void);
void deinit_instrs(void);
const char *query_opcode_name(int);

void set_inc_list(const char *);
void reset_inc_list(void);

void start_new_file(int fd, const char* pre_text);
void end_new_file(void);
const char *main_file_name(void);

int lookup_predef(char *);
void add_predefines(void);
void free_defines (int include_predefs);

char *show_error_context(void);
