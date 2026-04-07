#pragma once

#ifdef __cplusplus
extern "C" {
#endif
extern FILE *yyin;

#define MAX_FUNC    2048  /* If we need more than this we're in trouble! */

#define VOID        1
#define INT         2
#define STRING      3
#define OBJECT      4
#define MAPPING     5
#define MIXED       6
#define UNKNOWN     7
#define FLOAT       8
#define FUNCTION    9
#define BUFFER      10

extern int num_buff;
extern int op_code, efun_code, efun1_code;
extern char *oper_codes[MAX_FUNC];
extern char *efun_codes[MAX_FUNC], *efun1_codes[MAX_FUNC];
extern char *efun_names[MAX_FUNC], *efun1_names[MAX_FUNC];
extern char *key[MAX_FUNC], *buf[MAX_FUNC];

#define PRAGMA_NOTE_CASE_START      0x0001
#define PRAGMA_ALLOW_DOT_CALL       0x0002

extern int pragmas;

extern int arg_types[400], last_current_type;

char *ctype(int);
char *etype(int);
void handle_pragma(char *);

#ifdef __cplusplus
}
#endif
