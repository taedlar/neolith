/*  $Id: avltree.h,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */
/*
 * avltree.h
 *
 * Written by Paul Vixie
 */

#ifndef AVLTREE_H
#define AVLTREE_H

typedef struct tree_s {
    struct tree_s *tree_l, *tree_r;	/* left & right branches */
    char *tree_p;		/* data */
    short tree_b;		/* balance information */
}      tree;

void tree_init(tree **);
char *tree_srch(tree *, int (*) (), char *);
void tree_add(tree **, int (*) (), char *, int (*) ());
int tree_delete(tree **, int (*) (), char *, int (*) ());
int tree_trav(tree **, int (*) ());
void tree_mung(tree **, int (*) ());

#endif	/* ! AVLTREE_H */
