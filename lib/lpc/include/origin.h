#ifndef	LPC_ORIGIN_H
#define	LPC_ORIGIN_H

#ifdef	__LPC__

/* codes returned by the origin() efun */
#define ORIGIN_BACKEND		"driver"	/* backwards compat */
#define ORIGIN_DRIVER		"driver"
#define ORIGIN_LOCAL		"local"
#define ORIGIN_CALL_OTHER	"call_other"
#define ORIGIN_SIMUL_EFUN	"simul"
#define ORIGIN_CALL_OUT		"call_out"
#define ORIGIN_EFUN		"efun"
/* pseudo frames for call_other function pointers and efun pointer */
#define ORIGIN_FUNCTION_POINTER	"function_pointer"
/* anonymous functions */
#define ORIGIN_FUNCTIONAL	"functional"

#else	/* ! __LPC__ */

#define	ORIGIN_DRIVER		0x01
#define	ORIGIN_LOCAL		0x02
#define	ORIGIN_CALL_OTHER	0x04
#define	ORIGIN_SIMUL_EFUN	0x08
#define	ORIGIN_CALL_OUT		0x10
#define	ORIGIN_EFUN		0x20
/* pseudo frames for call_other function pointers and efun pointer */
#define	ORIGIN_FUNCTION_POINTER	0x40
/* anonymous functions */
#define	ORIGIN_FUNCTIONAL	0x80

#endif	/* ! __LPC__ */

#endif	/* ! LPC_ORIGIN_H */
