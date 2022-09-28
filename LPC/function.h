/*  $Id: function.h,v 1.1.1.1 2002/11/23 07:57:08 annihilator Exp $

    This program is a part of Neolith project distribution. The Neolith
    project is based on MudOS v22pre5 LPmud driver. Read doc/Copyright
    before you try to use, modify or distribute this program.

    For more information about Neolith project, please visit:

    http://www.es2.muds.net/neolith
 */

#ifndef	LPC_FUNCTION_H
#define	LPC_FUNCTION_H

/* codes returned by the functionp() efun */

#define	FP_LOCAL		2
#define	FP_EFUN			3
#define	FP_SIMUL		4
#define	FP_FUNCTIONAL		5

/* internal use */
#define	FP_G_VAR		6
#define	FP_L_VAR		7
#define	FP_ANONYMOUS		8
#define	FP_THIS_OBJECT		0x10

/* additional flags */
#define	FP_MASK			0x0f
#define	FP_HAS_ARGUMENTS	0x10
#define	FP_OWNER_DESTED		0x20
#define	FP_NOT_BINDABLE		0x40

#endif	/* ! LPC_FUNCTION_H */
