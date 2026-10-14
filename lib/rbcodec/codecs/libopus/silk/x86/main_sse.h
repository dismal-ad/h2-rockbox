/* Copyright (c) 2014, Cisco Systems, INC
   Written by XiangMingZhu WeiZhou MinPeng YanWang

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef MAIN_SSE_H
#define MAIN_SSE_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

# if defined(OPUS_X86_MAY_HAVE_SSE4_1)

void silk_noise_shape_quantizer(
    silk_nsq_state      *NSQ,                   /* I/O  NSQ state                       */
    opus_int            signalType,             /* I    Signal type                     */
    const opus_int32    x_sc_Q10[],             /* I                                    */
    opus_int8           pulses[],               /* O                                    */
    opus_int16          xq[],                   /* O                                    */
    opus_int32          sLTP_Q15[],             /* I/O  LTP state                       */
    const opus_int16    a_Q12[],                /* I    Short term prediction coefs     */
    const opus_int16    b_Q14[],                /* I    Long term prediction coefs      */
    const opus_int16    AR_shp_Q13[],           /* I    Noise shaping AR coefs          */
    opus_int            lag,                    /* I    Pitch lag                       */
    opus_int32          HarmShapeFIRPacked_Q14, /* I                                    */
    opus_int            Tilt_Q14,               /* I    Spectral tilt                   */
    opus_int32          LF_shp_Q14,             /* I                                    */
    opus_int32          Gain_Q16,               /* I                                    */
    opus_int            Lambda_Q10,             /* I                                    */
    opus_int            offset_Q10,             /* I                                    */
    opus_int            length,                 /* I    Input length                    */
    opus_int            shapingLPCOrder,        /* I    Noise shaping AR filter order   */
    opus_int            predictLPCOrder,        /* I    Prediction filter order         */
    int                 arch                    /* I    Architecture                    */
);

/**************************/
/* Noise level estimation */
/**************************/
void silk_VAD_GetNoiseLevels(
    const opus_int32            pX[ VAD_N_BANDS ],  /* I    subband energies                            */
    silk_VAD_state              *psSilk_VAD         /* I/O  Pointer to Silk VAD state                   */
);

#  define OVERRIDE_silk_VAD_GetSA_Q8

opus_int silk_VAD_GetSA_Q8_sse4_1(
    silk_encoder_state *psEnC,
    const opus_int16   pIn[]
);

#if defined(OPUS_X86_PRESUME_SSE4_1)
#define silk_VAD_GetSA_Q8(psEnC, pIn, arch) ((void)(arch),silk_VAD_GetSA_Q8_sse4_1(psEnC, pIn))

#else

#  define silk_VAD_GetSA_Q8(psEnC, pIn, arch) \
     ((*SILK_VAD_GETSA_Q8_IMPL[(arch) & OPUS_ARCHMASK])(psEnC, pIn))

extern opus_int (*const SILK_VAD_GETSA_Q8_IMPL[OPUS_ARCHMASK + 1])(
     silk_encoder_state *psEnC,
     const opus_int16   pIn[]);

#endif

# endif
#endif
