#ifndef _SETJMP_H_
#define _SETJMP_H_

/* Combined parts of include/setjmp.h and include/machine/setjmp.h in 
 * newlib 1.17.0, with minor changes for Rockbox.
 */

#include "config.h"

/* necv70 was 9 as well. */

#ifdef CPU_MIPS
#if CPU_MIPS == 64
#define _JBTYPE long long
#endif
#ifdef __mips_soft_float
#define _JBLEN 11
#else
#define _JBLEN 23
#endif
#endif

#ifdef _JBLEN
#ifdef _JBTYPE
typedef _JBTYPE jmp_buf[_JBLEN];
#else
typedef int jmp_buf[_JBLEN];
#endif
#else
typedef int jmp_buf;
#endif


extern void longjmp(jmp_buf __jmpb, int __retval);
extern int  setjmp(jmp_buf __jmpb);

#endif // _SETJMP_H_
