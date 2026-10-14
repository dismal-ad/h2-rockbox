/*
 * WMA compatible decoder
 * Copyright (c) 2002 The FFmpeg Project.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef CODECLIB_MDCT_H_INCLUDED
#define CODECLIB_MDCT_H_INCLUDED

//#include "types.h"
#include "fft.h"

void ff_imdct_calc(unsigned int nbits, fixed32 *output, const fixed32 *input);
void ff_imdct_half(unsigned int nbits, fixed32 *output, const fixed32 *input);

static inline fixed32 fixmul32b(fixed32 x, fixed32 y)
{
    fixed64 temp;

    temp = x;
    temp *= y;

    temp >>= 31;        //16+31-16 = 31 bits

    return (fixed32)temp;
}


static inline
void CMUL(fixed32 *pre,
          fixed32 *pim,
          fixed32 are,
          fixed32 aim,
          fixed32 bre,
          fixed32 bim)
{
    //int64_t x,y;
    fixed32 _aref = are;
    fixed32 _aimf = aim;
    fixed32 _bref = bre;
    fixed32 _bimf = bim;
    fixed32 _r1 = fixmul32b(_bref, _aref);
    fixed32 _r2 = fixmul32b(_bimf, _aimf);
    fixed32 _r3 = fixmul32b(_bref, _aimf);
    fixed32 _r4 = fixmul32b(_bimf, _aref);
    *pre = _r1 - _r2;
    *pim = _r3 + _r4;

}


#endif // CODECLIB_MDCT_H_INCLUDED
