#ifndef _WMAPRO_MATH_H_
#define _WMAPRO_MATH_H_

#include <inttypes.h>

/* rockbox: not used
#define fixtof16(x)       (float)((float)(x) / (float)(1 << 16))
#define fixtof31(x)       (float)((float)(x) / (float)(1 << 31))
#define ftofix16(x)       ((int32_t)((x) * (float)(1 << 16) + ((x) < 0 ? -0.5:0.5)))
#define ftofix31(x)       ((int32_t)((x) * (float)(1 << 31) + ((x) < 0 ? -0.5:0.5)))
*/

    static inline int32_t fixmulshift(int32_t x, int32_t y, int shamt)
    {
        int64_t temp;
        temp = x;
        temp *= y;
    
        temp >>= shamt;
    
        return (int32_t)temp;
    }
    
    static inline int32_t fixmul31(int32_t x, int32_t y)
    {
        int64_t temp;
        temp = x;
        temp *= y;
    
        temp >>= 31;
    
        return (int32_t)temp;
    }
    
    static inline int32_t fixmul24(int32_t x, int32_t y)
    {
        int64_t temp;
        temp = x;
        temp *= y;
    
        temp >>= 24;
    
        return (int32_t)temp;
    }
    
    static inline int32_t fixmul16(int32_t x, int32_t y)
    {
        int64_t temp;
        temp = x;
        temp *= y;
    
        temp >>= 16;
    
        return (int32_t)temp;
    }

    #define VECT_MUL_WIN_KERNEL(i, j, s0, s1, wi, wj) \
        dst[i] = fixmul31(wj, s0) - fixmul31(wi, s1); \
        dst[j] = fixmul31(wi, s0) + fixmul31(wj, s1);

static inline void vector_fixmul_window(int32_t *dst, const int32_t *src0, 
                                   const int32_t *src1, const int32_t *win, 
                                   int len)
{
    int i, j;
    dst += len;
    win += len;
    src0+= len;
    for(i=-len, j=len-1; i<0; i++, j--) {
        int32_t s0 = src0[i]; /* s0 = src0[      0 ... len-1] */
        int32_t s1 = src1[j]; /* s1 = src1[2*len-1 ... len]   */
        int32_t wi = -win[i]; /* wi = -win[      0 ... len-1] */
        int32_t wj = -win[j]; /* wj = -win[2*len-1 ... len]   */
        VECT_MUL_WIN_KERNEL(i, j, s0, s1, wi, wj);
    }
}

    #define VECT_MUL_SCALAR_KERNEL(dst, src, mul) \
        dst[i  ] = fixmul16(src[i  ], mul); \
        dst[i+1] = fixmul16(src[i+1], mul); \
        dst[i+2] = fixmul16(src[i+2], mul); \
        dst[i+3] = fixmul16(src[i+3], mul);

static inline void vector_fixmul_scalar(int32_t *dst, const int32_t *src, 
                                        int32_t mul, int len)
{
    /* len is _always_ a multiple of 4, because len is the difference of sfb's
     * which themselves are always a multiple of 4. */
    int i;
    for (i=0; i<len; i+=4) {
        VECT_MUL_SCALAR_KERNEL(dst, src, mul);
    }
}

static inline int av_clip(int a, int amin, int amax)
{
    if      (a < amin) return amin;
    else if (a > amax) return amax;
    else               return a;
}
#endif /* _WMAPRO_MATH_H_ */
