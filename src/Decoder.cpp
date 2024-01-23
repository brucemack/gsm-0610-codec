/**
 * GSM 06.10 CODEC
 * Copyright (C) 2024, Bruce MacKinnon 
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * NOT FOR COMMERCIAL USE WITHOUT PERMISSION.
 */
#include <cassert>
#include "fixed_math.h"

#include "gsm-0610-codec/Encoder.h"
#include "gsm-0610-codec/Decoder.h"

// Utility
//#define q15_to_f32(a) ((float)(a) / 32768.0f)

// Sanity checking function for index bounds
// USE THIS VERSION FOR DEVELOPMENT/TESTING
//#define IX(x, lo, hi) (_checkIx(x, lo, hi))
// USE THIS VERSION FOR PRODUCTION
#define IX(x, lo, hi) (x)

namespace kc1fsz {

// Sanity checking function for index bounds
//static uint16_t _checkIx(uint16_t x, uint16_t lo, uint16_t hi) {
//    assert(x >= lo && x <= hi);
//    return x;
//}

Decoder::Decoder() {
    reset();
}

void Decoder::reset() {
    _nrp = 40;
    for (uint16_t k = 0; k <= 159; k++) {
        _drp[k] = 0;
    }
    for (uint16_t i = 0; i <= 8; i++) {
        _LARpp_last[i] = 0;
    }
    for (uint16_t i = 0; i <= 8; i++) {
        _v[i] = 0;
    }
    _msr = 0;
}

void Decoder::decode(const Parameters* input, int16_t* outputPcm) {

    // This will be filled one sub-segment at a time.  It is 
    // essentially the dr' signal for each sub-segment.
    int16_t wt[160];

    // This part runs four times, once for each sub-segment.  In keeping with 
    // the draft convention, we use "j" to denote the sub-segment.
    for (uint16_t j = 0; j < 4; j++) {

        // Section 5.3.1 - RPE Decoding 
        // The goal here is to reconstruct the long-term residual erp[0..39] signal
        // from the received parameters for this sub-segment (Mc, xmaxc, xMc[]).

        int16_t exp, mant, itest;

        // Compute exponent and mantissa of the decoded version of xmaxc
        exp = 0;
        mant = 0;

        if (input->subSegs[j].xmaxc > 15) {
            exp = sub((input->subSegs[j].xmaxc >> 3), 1);    
        }
        mant = sub(input->subSegs[j].xmaxc, (exp << 3));

        // Normalize mantissa0 <= mant <= 7
        if (mant == 0) {
            exp = -4;
            mant = 15;
        } else {
            itest = 0;
            for (uint16_t i = 0; i <= 2; i++) {
                if (mant > 7) {
                    itest = 1;
                }
                if (itest == 0) {
                    mant = add((mant << 1), 1);
                }
                if (itest == 0) {
                    exp = sub(exp, 1);
                }
            }
        }
        mant = sub(mant, 8);

        // Encoder Section 5.2.16 - APCM inverse quantization
        // Encoder Section 5.2.17 RPE grid positioning
        int16_t erp[40];
        Encoder::inverseAPCM(input, j, exp, mant, erp);

        // Section 5.3.2 - Long-Term Synthesis Filtering
        // Use bc abd Nc to realize the long-term synthesis filtering

        int16_t Nr = input->subSegs[j].Nc;
        if (input->subSegs[j].Nc < 40) {
            Nr = _nrp;
        } else if (input->subSegs[j].Nc > 120) {
            Nr = _nrp;
        }
        _nrp = Nr;

        // Decoding of the LTP gain bc
        int16_t brp = Encoder::QLB[input->subSegs[j].bc];

        // Computation of the reconstructed short term residual signal drp[0..39]
        for (int16_t k = 0; k <= 39; k++) {
            // NOTE: Index for _drp[] is different from draft doc
            int16_t drpp = mult_r(brp, _drp[IX((k - Nr) + 120, 0, 119)]);
            // NOTE: Index for _drp[] is different from draft doc
            _drp[IX((k + 120), 120, 159)] = add(erp[k], drpp);
        }

        // Update the reconstructed short-term residual signal drp[-1..-120]
        for (int16_t k = 0; k <= 119; k++) {
            _drp[IX((-120 + k) + 120, 0, 119)] = _drp[IX((-80 + k) + 120, 40, 159)];
        }

        // Load up the right part of the wt[] vector, based on which sub-segment
        // we are working on.
        for (int16_t k = 0; k <= 39; k++) {
            // NOTE: _drp[] index differs from draft
            wt[IX((j * 40) + k, 0, 159)] = _drp[IX(k + 120, 120, 159)];
        }
    }

    // Section 5.3.3 - Computation of the decoded reflection coefficients
    // The goal is to reconstruct rrp[1..8] 

    int16_t rrp[4][9];
    Encoder::decodeReflectionCoefficients(input, _LARpp_last, rrp);

    // NUMERICAL NOTE: At this point rrp[] is at full scale

    // Section 5.3.4 - Short term synthesis filtering section
    //
    // This procedure uses the drp[0..39] signal and produces the sr[0...159] 
    // which is the output of the short-term synthesis filter.
    //
    // We also take the chance to perform the post-processing on the
    // output samples.

    for (int16_t k = 0; k <= 159; k++) {
        // Remember that the filter coefficients change as we move across 
        // the segment.  IMPORTANT: ZONE != SUB-SEGMENT!!
        int16_t zone = Encoder::k2zone(k);
        // See figure 3.5 on page 26 
        int16_t sri = wt[k];
        for (int16_t i = 1; i <= 8; i++) {
            sri = sub(sri, mult_r(rrp[zone][IX(9 - i, 1, 8)], _v[IX(8 - i, 0, 7)]));
            // Moving forward on _v[]
            _v[IX(9 - i, 1, 8)] = add(_v[IX(8 - i, 0, 7)], mult_r(rrp[zone][IX(9 - i, 1, 8)], sri));
        }
        _v[0] = sri;

        // Section 5.3.5 - Deemphasis filtering
        // 28180/32767 = 0.86
        //int16_t temp = add(sr[k], mult_r(_msr, 28180));
        int16_t temp = add(sri, mult_r(_msr, 28180));
        _msr = temp;

        // Section 5.3.6 - Up-scaling of the output signal
        int16_t srop = add(_msr, _msr);

        // Section 5.3.7 - Truncation of the output variable
        outputPcm[k] = srop & 0xfff8;
    }
}

}
