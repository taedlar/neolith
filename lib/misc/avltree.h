#pragma once
/*
 * avltree.h
 *
 * Written by Paul Vixie
 */

typedef struct tree_s {
    struct tree_s *tree_l, *tree_r;	/* left & right branches */
    char *tree_p;		/* data */
    short tree_b;		/* balance information */
} tree;

void tree_init(tree **);
char *tree_srch(tree *, int (*pfi_compare) (), char *);
void tree_add(tree **, int (*pfi_compare) (), char *, int (*) ());
int tree_delete(tree **, int (*pfi_compare) (), char *, int (*) ());
int tree_trav(tree **, int (*pfi_action) ());
void tree_mung(tree **, int (*pfi_action) ());
