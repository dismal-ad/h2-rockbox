#ifndef _FRACMUL_H
#define _FRACMUL_H

#include <stdint.h>
#include "gcc_extensions.h"

/** FRACTIONAL MULTIPLICATION
 *  Multiply two fixed point numbers with 31 fractional bits:
 *      FRACMUL(x, y)
 *
 *  Multiply two fixed point numbers with 31 fractional bits,
 *          then shift left by z bits:
 *      FRACMUL_SHL(x, y, z)
 *          NOTE: z must be in the range 1-8 on Coldfire targets.
 */


/* A bunch of fixed point assembler helper macros */

static inline int32_t FRACMUL(int32_t x, int32_t y)
{
    return (int32_t) (((int64_t)x * y) >> 31);
}

static inline int32_t FRACMUL_SHL(int32_t x, int32_t y, int z)
{
    return (int32_t) (((int64_t)x * y) >> (31 - z));
}

#endif
