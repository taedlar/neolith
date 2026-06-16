#pragma once

#ifdef __cplusplus
extern "C" {
#endif
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
char *tree_srch(tree *, int (*pfi_compare) (void *, void *), char *);
void tree_add(tree **, int (*pfi_compare) (void *, void *), char *, int (*pfi_action) (void *));
int tree_delete(tree **, int (*pfi_compare) (void *, void *), char *, int (*pfi_action) (void *));
int tree_trav(tree **, int (*pfi_action) (void *));
void tree_mung(tree **, int (*pfi_action) (void *));

#ifdef __cplusplus
}
#endif
