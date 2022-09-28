/*  $Id: edit_source.h,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */

#ifndef EDIT_SOURCE_H
#define EDIT_SOURCE_H

extern FILE *yyin;

#define MAX_FUNC        2048  /* If we need more than this we're in trouble! */

#define VOID 		1
#define INT		2
#define STRING		3
#define OBJECT		4
#define MAPPING		5
#define MIXED		6
#define UNKNOWN		7
#define FLOAT		8
#define FUNCTION	9
#define BUFFER         10

extern int num_buff;
extern int op_code, efun_code, efun1_code;
extern char *oper_codes[MAX_FUNC];
extern char *efun_codes[MAX_FUNC], *efun1_codes[MAX_FUNC];
extern char *efun_names[MAX_FUNC], *efun1_names[MAX_FUNC];
extern char *key[MAX_FUNC], *buf[MAX_FUNC];

extern int arg_types[400], last_current_type;

char *ctype(int);
char *etype(int);

#endif	/* ! EDIT_SOURCE_H */
