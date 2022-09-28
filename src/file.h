/*  $Id: file.h,v 1.2 2002/11/25 11:11:05 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith

    ORIGINAL AUTHOR
	Unknown

    MODIFIED BY
	[2001-06-27] by Annihilator <annihilator@muds.net>, see CVS log.
 */

#ifndef FILE_H
#define FILE_H

#include "lpc/types.h"

/*
 * file.c
 */

int legal_path(char *);
char *check_valid_path(char *, object_t *, char *, int);
void smart_log(char *, int, char *, int);
void dump_file_descriptors(outbuffer_t *);

char *read_file(char *, int, int);
char *read_bytes(char *, int, int, int *);
int write_file(char *, char *, int);
int write_bytes(char *, int, char *, int);
array_t *get_dir(char *, int);
int tail(char *);
int file_size(char *);
int copy_file(char *, char *);
int do_rename(char *, char *, int);
int remove_file(char *);

#endif
