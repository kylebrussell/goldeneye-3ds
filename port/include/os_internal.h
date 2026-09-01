#ifndef GE_PORT_OS_INTERNAL_H
#define GE_PORT_OS_INTERNAL_H

/* The libaudio voice list only needs the public interrupt-mask API.  Pulling
 * the N64 kernel's private thread declarations into the native build also
 * selects incompatible duplicate OS records. */
#include <os.h>

#endif
