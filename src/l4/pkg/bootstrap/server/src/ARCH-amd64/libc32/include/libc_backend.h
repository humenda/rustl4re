/* */



/* This file defines the backend interface */
/* of the kernel c-library. */

#ifndef __LIBC_BACKEND_H__
#define __LIBC_BACKEND_H__

#include <stddef.h>
#include <cdefs.h>

__BEGIN_DECLS

/** 
 * The text output backend. 
 * 
 * This function must be provided to the c-library for
 * text output. It must simply send len characters of s
 * to an output device.
 *
 * @param s the string to send (not zero terminated).
 * @param len the number of characters.
 * @return 1 on success, 0 else.
 */
int __libc_backend_outs( const char *s, size_t len );


/**
 * The text input backend.
 *
 * This function must be provided to the c-library for 
 * text input. It has to block til len characters are
 * read or a newline is reached. The retrurn value gives 
 * the number of characters virtually read.
 *
 * @param s a poiznter to the buffer for the read text.
 * @param len the size of the buffer.
 * @return the number of characters virtually read.
 */
int __libc_backend_ins( char *s, size_t len );

__END_DECLS


#endif //__LIBC_BACKEND_H__
