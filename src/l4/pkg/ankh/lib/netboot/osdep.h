#ifndef	_OSDEP_H
#define _OSDEP_H

#define __unused __attribute__((unused))
#define __aligned __attribute__((aligned(16)))

/* ANSI prototyping macro */
#ifdef	__STDC__
#  define	P(x)	x
#else
#  define	P(x)	()
#endif

#endif
