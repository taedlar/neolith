/*  $Id: uids.h,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */

#ifndef	_LPC_UIDS_H
#define	_LPC_UIDS_H

/*
    ORIGINAL AUTHOR
	Erik Kay

    MODIFIED BY
	[1994-07-09] by Robocoder - modified to use AVL tree
	[2001-06-26] by Annihilator <annihilatro@muds.net>
*/

struct userid_s {
    char *name;
};

extern userid_t *backbone_uid;
extern userid_t *root_uid;

userid_t *add_uid(char *name);
userid_t *set_root_uid(char *name);
userid_t *set_backbone_uid(char *name);

#endif	/* ! _LPC_UIDS_H */
