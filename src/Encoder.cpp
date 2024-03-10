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

uint16_t Encoder::k2zone(uint16_t k) {
    if (k <= 12) {
        return 0;
    } else if (k <= 26) {
        return 1;
    } else if (k <= 39) {
        return 2;
    } else { 
        return 3;
    }
}

Encoder::Encoder(bool homingSupported) 
:   _homingSupported(homingSupported),
    _lastFrameHome(false) {
    reset();
}

void Encoder::reset() {
    _z1 = 0;
    _L_z2 = 0;
    _mp = 0;
    for (uint16_t i = 1; i <= 8; i++) {
        _LARpp_last[i] = 0;
    }
    for (uint16_t i = 0; i < 8; i++) {
        _u[i] = 0;
    }
    for (uint16_t i = 0; i < 120; i++) {
        _dp[IX(i, 0, 119)] = 0;
    }
}

/**
 * Please see https://www.etsi.org/deliver/etsi_EN/300900_300999/300961/08.00.01_40/en_300961v080001o.pdf
 * for the source specification that was used for this CODEC.  References to page/section numbers
 * are taken from that specific version of the document Draft "ETSI EN 300 961 V8.0.1 (2000-07).""
 * 
 * Variable names from the draft are preserved, even when they violate common C++ coding
 * conventions.
*/
void Encoder::encode(const int16_t sop[], Parameters* output) {

    int16_t so[160];
    int16_t sof[160];
    int16_t s[160];
    int16_t s1 = 0;
    int32_t L_s2;
    int16_t temp, temp1, temp2, di, sav;
    int16_t smax;
    int16_t scal;
    int32_t L_ACF[9];
    int16_t ACF[9];
    int16_t P[9];
    int16_t r[9];
    int16_t K[9];
    int32_t L_temp;

    /*
    // Section 5.2.1 - Scaling of the input variable
    for (uint16_t k = 0; k <= 159; k++) {
        // Shift away the 3 low-order (don't care) bits
        so[k] = sop[k] >> 3;
        // Back in q15 format divided by two
        so[k] = so[k] << 2;
    }
    */

    // Section 5.2.1 - Scaling of the input variable
    // Section 5.2.2 - Offset compensation
    for (uint16_t k = 0; k <= 159; k++) {

        // Shift away the 3 low-order (don't care) bits
        // Back in q15 format divided by two
        so[k] = sop[k] >> 1;

        // Compute the non-recursive part
        s1 = sub(so[k], _z1);
        _z1 = so[k];

        // Compute the recursive part
        L_s2 = s1;
        L_s2 = L_s2 << 15;

        // Execution of a 31 by 16 bit multiplication
        int16_t msp = _L_z2 >> 15;
        int16_t lsp = L_sub(_L_z2, (msp << 15));
        int16_t temp = mult_r(lsp, 32735);
        L_s2 = L_add(L_s2, temp);
        _L_z2 = L_add(L_mult(msp, 32735) >> 1, L_s2);

        // Compute sof[k] with rounding
        sof[k] = L_add(_L_z2, 16384) >> 15;
    }

    // Section 5.2.3 - Pre-emphasis
    for (uint16_t k = 0; k <= 159; k++) {
        // -28180/32767 = -0.86
        s[k] = add(sof[k], mult_r(_mp, -28180));
        _mp = sof[k];
    }

    // Section 5.2.4 - Autocorrelation
    //
    // The goal is to compute the array L_ACF[k].  The signal s[i] shall be scaled in order 
    // to avoid an overflow situation.

    // Search for the maximum
    smax = 0;
    for (uint16_t k = 0; k <= 159; k++) {
        int16_t temp = s_abs(s[k]);
        if (temp > smax) {
            smax = temp;
        }
    }

    // Computation of the scaling factor
    int16_t scalauto = 0;
    if (smax == 0) {
        scalauto = 0;
    } else {
        scalauto = sub(4, norm(smax << 16));
    }

    // Scaling of the array s[0..159]
    if (scalauto > 0) {
        int16_t temp = 16384 >> sub(scalauto, 1);
        for (uint16_t k = 0; k <= 159; k++) {
            s[k] = mult_r(s[k], temp);
        }
    }

    // Compute the L_ACF[..]
    for (uint16_t k = 0; k <= 8; k++) {
        L_ACF[k] = 0;
        for (uint16_t i = k; i <= 159; i++) {
            L_temp = L_mult(s[i], s[i - k]);
            L_ACF[k] = L_add(L_ACF[k], L_temp);
        }
    }

    // Rescaling of the array s[0..159]
    if (scalauto > 0) {
        for (uint16_t k = 0; k <= 159; k++) {
            s[k] = s[k] << scalauto;
        }
    }

    // Section 5.2.5 Computation of the reflection coefficients

    // Schur recursion with 16 bit arithmetic

    if (L_ACF[0] == 0) {
        for (uint16_t i = 1; i <= 8; i++) {
            r[i] = 0;
        }
    } else {
        int16_t temp = norm(L_ACF[0]);
        for (uint16_t k = 0; k <= 8; k++) {
            ACF[k] = (L_ACF[k] << temp) >> 16;
        }

        // Initialize P[] and K[] for the recursion
        for (uint16_t i = 1; i <= 7; i++) {
            K[9 - i] = ACF[i];
        }
        for (uint16_t i = 0; i <= 8; i++) {
            P[i] = ACF[i];
        }

        // Compute reflection coefficients
        for (uint16_t n = 1; n <= 8; n++) {
            if (P[0] < s_abs(P[1])) {
                for (uint16_t i = n; i <= 8; i++) {
                    r[i] = 0;
                }
                // Continue with 5.2.6
                break;
            }
            r[n] = div(s_abs(P[1]), P[0]);
            if (P[1] > 0) {
                r[n] = sub(0, r[n]);
            }
            if (n == 8) {
                break;
            }

            // Schur recursion
            P[0] = add(P[0], mult_r(P[1], r[n]));
            for (uint16_t m = 1; m <= 8 - n; m++) {
                P[m] = add(P[m + 1], mult_r(K[9-m], r[n]));
                K[9 - m] = add(K[9 - m], mult_r(P[m + 1], r[n]));
            }
        }
    }

    // 5.2.6 Transformation of reflection coefficients to log-area 
    // ratios.  Actually, we use an approximation of the log-area
    // ratio as defined in the draft spec.

    int16_t LAR[9];

    // r[1..8] are the reflection coefficients.
    // LAR[1..8] are the reflection coefficients transformed to 
    //   log-area ratios "due to favorable quantization characteristics."
    //
    // IMPORTANT: LAR[] comes out right-shifted by 1!
    for (uint16_t i = 1; i <= 8; i++) {

        // The sign of r[i] is removed and added back to the result
        int16_t temp = s_abs(r[IX(i, 1, 8)]);

        // 22118/32767 = 0.675 threshold
        if (temp < 22118) {
            temp = temp >> 1;
        }
        // 31130/32767 = 0.95 threshold
        else if (temp < 31130) {
            temp = sub(temp, 11059);
        } 
        else {
            temp = sub(temp, 26112) << 2;
        }

        // Restore sign of result to be consistent with sign of input
        if (r[IX(i, 1, 8)] < 0) {
            temp = sub(0, temp);
        }

        LAR[IX(i, 1, 8)] = temp;
   }

    // Section 5.2.7. - Quantization and coding of the Log-Area Ratios
    int16_t LARc[9];

    // Computation for quantizing and coding the LAR[1..8] 
    // A[], B[] are lookup tables 5.1
    //
    // A[] is a scaling factor and B[] is an offset factor.
    //
    // LARc[] = nint(A[i] * LAR[i] + B[i])
    //
    // Where: nint(z) = int(z + sign(z) * 0.5)
    //
    // NUMERICAL ADJUSTMENTS TO REMEMBER:
    // 1. LAR[] is pre-divided by 2 in the above calculations
    // 2. A[] is pre-divided by 32 in the lookup table
    //
    for (uint16_t i = 1; i <= 8; i++) {
        
        // temp is LAR/20 pre-divided by 64 (shifted 6 to the right)
        int16_t temp = mult(A[IX(i, 1, 8)], LAR[IX(i, 1, 8)]);
        temp = add(temp, B[IX(i, 1, 8)]);

        // For rounding
        temp = add(temp, 256);
        // Shift an additional 9 to the right, so now we are a total of 15 
        // to the right.  If we treat this like an normal integer instead of 
        // a q15 it is equivalent to shifting to the left by 15.
        LARc[IX(i, 1, 8)] = temp >> 9;

        // Check if LARc[i] lies between MIN and MAX and saturate as needed.
        if (LARc[IX(i, 1, 8)] > MAC[IX(i, 1, 8)]) {
            LARc[IX(i, 1, 8)] = MAC[IX(i, 1, 8)];
        } 
        if (LARc[IX(i, 1, 8)] < MIC[IX(i, 1, 8)]) {
            LARc[IX(i, 1, 8)] = MIC[IX(i, 1, 8)];
        }

        // The equation is used to make all of the LARc[i] positive
        // by subtracting the minimum of the range.
        LARc[IX(i, 1, 8)] = sub(LARc[IX(i, 1, 8)], MIC[IX(i, 1, 8)]);
    }

    // Write out the parameters.
    // NOTE: The draft uses index [1..8] for the LARc array
    for (uint16_t i = 0; i < 8; i++) {
        output->LARc[IX(i, 0, 7)] = LARc[IX(i + 1, 1, 8)];
    }

    // ===== SHORT TERM ANALYSIS FILTERING SECTION ===========================

    // Section 5.2.8 - Decoding of the coded Log-Area Ratios.  
    // This basically reverses the process above to create the r' version
    // of the reflection coefficients.  These will also be used on the 
    // decoder side.

    // We get one set of reflection coefficients for each zone.
    // The coefficients are in rp[0..3][1..8].

    int16_t rp[4][9];
    decodeReflectionCoefficients(output, _LARpp_last, rp);

    // NUMERICAL NOTE: At this point rp[] is back to the original 
    // scale of r[].

    // Section 5.2.10 - Short term analysis filtering

    // This is the short-term residual signal that will be computed.
    // We use the original signal s[] and the INVERSE of the filter
    // that is defined by the coefficients in r'.
    int16_t d[160];

    // NOTE: The zone thing continues here since there are 4 different
    // sets of rp coefficients.

    // Calculate d[] which is the short-term residual.
    //
    // _u[0..7] is used to generate the delay during the application 
    // of the filter.  Notice that _u[7] will be used during the 
    // processing of the first sample in the next call (i.e. state
    // be being carried across segments).

    for (uint16_t k = 0; k <= 159; k++) {
        di = s[k];
        sav = di;
        uint16_t zone = k2zone(k);
        for (uint16_t i = 1; i <= 8; i++) {
            temp = add(_u[IX(i - 1, 0, 7)], mult_r(rp[zone][i], di));
            di = add(di, mult_r(rp[zone][i], _u[IX(i - 1, 0, 7)]));
            _u[IX(i - 1, 0, 7)] = sav;
            sav = temp;
        }
        d[k] = di;
    }

    // ===== LONG TERM PREDICTOR SECTION =====================================

    // This part runs four times, once for each sub-segment.  In keeping with 
    // the draft convention, we use "j" to denote the sub-segment.

    for (uint16_t j = 0; j < 4; j++) {

        int16_t R, S;
        int32_t L_max, L_result, L_power;

        int16_t wt[50];
        int16_t x[40];
        // Long-term residual signal calculated from d - d''
        int16_t e[40];

        // Compute the starting index
        uint16_t kj = j * 40;

        // Section 5.2.11 Calculation of the LTP parameters

        // Search of the optimum scaling of d(j)[0..39]
        int16_t dmax = 0;
        for (uint16_t k = 0; k <= 39; k++) {        
            // NOTE: Sub-segment index
            temp = s_abs(d[kj + k]);
            if (temp > dmax) {
                dmax = temp;
            }
        }

        temp = 0;
        if (dmax == 0) {
            scal = 0;
        } else {
            temp = norm((int32_t)dmax << 16);
        }
        if (temp > 6) {
            scal = 0;
        } else {
            scal = sub(6, temp);
        }

        // Initialization of a working array wt[0..39]
        for (uint16_t k = 0; k <= 39; k++) {
            // NOTE: Sub-segment index
            wt[k] = d[kj + k] >> scal;
        }

        // Search for the maximum cross-correlation and coding of the LTP lag
        L_max = 0;
        // Index for max cross-corelation
        output->subSegs[j].Nc = 40;    

        for (uint16_t lambda = 40; lambda <= 120; lambda++) {
            // Cross correlate with each distinct lag
            L_result = 0;
            for (uint16_t k = 0; k <= 39; k++) {
                // NOTE: Index adjustment vs. draft doc
                L_temp = L_mult(wt[k], _dp[IX((k - lambda) + 120, 0, 119)]);
                L_result = L_add(L_temp, L_result);
            }
            if (L_result > L_max) {
                output->subSegs[j].Nc = lambda;
                L_max = L_result;
            }
        }

        // Rescaling of L_max
        L_max = L_max >> (sub(6, scal));

        // Initialization of a working array wt[0..39]
        for (uint16_t k = 0; k <= 39; k++) {
            // NOTE: Index adjustment vs. draft doc
            wt[k] = _dp[IX((k - output->subSegs[j].Nc) + 120, 0, 119)] >> 3;
        }

        // Compute the power of the reconstructed short term residual signal dp[..]
        L_power = 0;
        for (uint16_t k = 0; k <= 39; k++) {
            L_temp = L_mult(wt[k], wt[k]);
            L_power = L_add(L_temp, L_power);
        }

        // TODO: UNDERSTAND THE SCALING OF wt[], L_max, and L_power at this point.

        // Normalization of L_max and L_power
        if (L_max <= 0) {
            output->subSegs[j].bc = 0;
        } else if (L_max >= L_power) {
            output->subSegs[j].bc = 3;
        } else {
            temp = norm(L_power);
            R = (L_max << temp) >> 16;
            S = (L_power << temp) >> 16;

            // Coding of the LTP gain
            if (R <= mult(S, DLB[0])) {
                output->subSegs[j].bc = 0;
            } else if (R <= mult(S, DLB[1])) {
                output->subSegs[j].bc = 1;
            } else if (R <= mult(S, DLB[2])) {
                output->subSegs[j].bc = 2;
            } else {
                output->subSegs[j].bc = 3;
            }
        }

        // Section 5.2.12 - Long term analysis filtering
        //
        // In this part we have to decode the bc parameter to compute the samples
        // of the estimate of dpp[0..39].

        // Decoding of the coded LTP gain
        int16_t bp = QLB[output->subSegs[j].bc];
        
        // Calculating the array e[0..39] and the array dpp[0..39]
        //
        // e[] is the long-term residual signal and is defined as 
        // e = d - d''
        //
        // The block of 40 long term residual signal samples is obtained by 
        // subtracting 40 estimates of the short term residual signal 
        // from the short term residual signal itself.
        int16_t dpp[40];
        for (uint16_t k = 0; k <= 39; k++) {
            // NOTE: Index adjustment vs. draft doc
            // TODO: MAKE SURE WE ARE COMPARING THE RIGHT THINGS HERE
            dpp[k] = mult_r(bp, _dp[IX((k - output->subSegs[j].Nc) + 120, 0, 119)]);
            // NOTE: Sub-segment adjustment 
            e[IX(k, 0, 39)] = sub(d[kj + k], dpp[k]);
        }

        // ===== RPE ENCODING SECTION =============================================
        //
        // RPE = "Regular Pulse Excitation"
        // The input is e[], the long-term residual. The block of 40 input long term 
        // residual samples are represented by one of 4 candidate sub-sequences of 13 
        // pulses each. The subsequence selected is identified by the RPE grid position (M).

        // Section 5.2.13 - Weighting filter H(z)

        // Initialization of a temporary working array wt[0..49].
        // The data from e[] is centered in this 50-element array,
        for (uint16_t k = 0; k <= 4; k++) {
            wt[k] = 0;
        }    
        for (uint16_t k = 5; k <= 44; k++) {
            wt[k] = e[IX(k - 5, 0, 39)];
        }
        for (uint16_t k = 45; k <= 49; k++) {
            wt[k] = 0;
        }

        // Compute the block-filtered signal x[0..39] via conventional convolution.
        // Because wt[] has been extended to 50 elements this 
        // 40x11 convolution results in a 40 element result.
        for (uint16_t k = 0; k <= 39; k++) {
            // Rounding of the output of the filter
            L_result = 8192;
            for (uint16_t i = 0; i <= 10; i++) {
                L_temp = L_mult(wt[IX(k + i, 0, 49)], H[IX(i, 0, 10)]);
                L_result = L_add(L_result, L_temp);
            }
            // Scaling x4
            L_result = L_add(L_result, L_result);
            L_result = L_add(L_result, L_result);
            // Drop from 32->16 bits, but we still have the 4x
            x[IX(k, 0, 39)] = L_result >> 16;
        }

        // In this next section, we take the filtered signal x[] and down-sample it
        // by a ratio of 3, resulting in 3 interleaved sequences of length 14, 13,
        // and 13, which are split up again into 4 sub-sequences of length 13.

        // Section 5.2.14 - RPE grid selection

        int32_t EM = 0;
        output->subSegs[j].Mc = 0;

        // Figure out what has the maximum energy:
        for (uint16_t m = 0; m <= 3; m++) {
            L_result = 0;
            for (uint16_t i = 0; i <= 12; i++) {
                // Here the 4x gets removed
                temp1 = x[IX(m + (3 * i), 0, 39)] >> 2;
                // Square
                L_temp = L_mult(temp1, temp1);
                L_result = L_add(L_temp, L_result);
            }
            // Is this the max?
            if (L_result > EM) {
                output->subSegs[j].Mc = m;
                EM = L_result;
            }
        }

        // Down-sampling by a factor 3 to get the selected xM[0..12] RPE 
        // sequence.
        // 
        // Basically, we are pulling a 13-point pulse xM[] out of the 
        // x[] signal which will be used to represent the entire x[] signal.
        // This is where the "RPE" term comes from.  We represent x[] 
        // by a repeated use of the pulse in xM[].

        int16_t xM[13];
        for (uint16_t i = 0; i <= 12; i++) {
            xM[i] = x[output->subSegs[j].Mc + (3 * i)];
        }

        // Section 5.2.15 - APCM quantization of the selected RPE sequence.
        // APCM = Adaptive pulse-coded modulation.

        int16_t xmax = 0;
        for (uint16_t i = 0; i <= 12; i++) {
            temp = s_abs(xM[i]);
            if (temp > xmax) {
                xmax = temp;
            }
        }
        
        // Quantizing and coding of xmax to get xmaxc
        int16_t exp, mant;
        exp = 0;
        temp = xmax >> 9;
        int16_t itest = 0;
        for (uint16_t i = 0; i <= 5; i++) {
            if (temp <= 0) {
                itest = 1;
            }
            temp = temp >> 1;
            if (itest == 0) {
                exp = add(exp, 1);
            }
        }
        temp = add(exp, 5);
        output->subSegs[j].xmaxc = add((xmax >> temp), (exp << 3));
        
        // Quantizing and coding of the xM[0..12] RPE sequence to get xMc[0..12]

        // Compute exponent and mantissa of the decoded version of xmaxc
        exp = 0;
        mant = 0;

        if (output->subSegs[j].xmaxc > 15) {
            exp = sub((output->subSegs[j].xmaxc >> 3), 1);    
        }
        mant = sub(output->subSegs[j].xmaxc, (exp << 3));

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

        // Direct computation of xMc[0..12] using table 5.5
        // Normalized by the exponent
        temp1 = sub(6, exp);
        // See table 5.5 (inverse mantissa)
        temp2 = NRFAC[mant];
        for (uint16_t i = 0; i <= 12; i++) {
            temp = xM[i] << temp1;
            temp = mult(temp, temp2);
            // This equation is used to make all the xMc[i] positive
            output->subSegs[j].xMc[i] = add((temp >> 12), 4);
        }

        // Section 5.2.16 - APCM inverse quantization
        // Section 5.2.17 RPE grid positioning
        int16_t ep[40];
        inverseAPCM(output, j, exp, mant, ep);

        // Section 5.2.18 - Update of the reconstructed short term residual
        // signal dp[-120,1].
        for (uint16_t k = 0; k <= 79; k++) {
            // Shift 80 items
            // Original document: 
            // First index: -120 -> -41
            // Second index: -80 -> -1
            //
            // NOTE: Here we have changed the notation from the draft document!
            // So we are actually shifting the 80 entries from [40...118] down 
            // to [0...79].
            _dp[IX((-120 + k) + 120, 0, 119)] = _dp[IX((-80 + k) + 120, 0, 119)];
        }
        // We are filling in 80 to 119
        // d' = d'' + e'
        for (uint16_t k = 0; k <= 39; k++) {
            // NOTE: Here we have changed the notation from the draft document!
            _dp[IX((-40 + k) + 120, 0, 119)] = add(ep[IX(k, 0, 39)], dpp[k]);
        }
    }   

    // Look at the original input frame to determine if it is a homing frame
    if (_homingSupported) {
        if (isHomingFrame(sop)) {
            reset();
            _lastFrameHome = true;
        }
    }
}

bool Encoder::isHomingFrame(const int16_t frame[]) {
    for (uint16_t i = 0; i < 160; i++) {
        if (frame[i] != 1) {
            return false;
        }
    }
    return true;
}

void Encoder::inverseAPCM(const Parameters* params, int16_t j, int16_t exp, int16_t mant, int16_t ep[]) {

    // Section 5.2.16 - APCM inverse quantization
    int16_t xMp[13];
    int16_t temp;
    int16_t temp1 = FAC[mant];
    int16_t temp2 = sub(6, exp);
    int16_t temp3 = 1 << sub(temp2, 1);
    for (uint16_t i = 0; i <= 12; i++) {
        // This subtraction is used to restore the sign of xMc[i]
        temp = sub((params->subSegs[j].xMc[i] << 1), 7);
        temp = temp << 12;
        temp = mult_r(temp1, temp);
        temp = add(temp, temp3);
        xMp[i] = temp >> temp2;
    }

    // Section 5.2.17 RPE grid positioning
    // ep[] is the reconstructed long term residual.  The array starts
    // off with all zeroes and then the pulse values are interspersed
    // based on the Mc index that was determined by the RPE selection (Mc).
    for (uint16_t k = 0; k <= 39; k++) {
        ep[IX(k, 0, 39)] = 0;
    }
    for (uint16_t i = 0; i <= 12; i++) {
        ep[IX(params->subSegs[j].Mc + (3 * i), 0, 39)] = xMp[i];
    }
}

void Encoder::decodeReflectionCoefficients(const Parameters* params, 
    int16_t* LARpp_last, int16_t rp[][9]) {

    int16_t LARpp[9];
    // Here we have four sets of coefficients for different zones
    // in the segment
    int16_t LARp[4];

    // Section 5.2.8 - Decoding of the coded Log-Area Ratios

    // Compute the LARpp[1..8] by reversing the LARc[] values
    for (uint16_t i = 1; i <= 8; i++) {
        // NOTE: The addition of MIC[i] is used to restore the sign of LARc[i]
        int16_t temp1 = add(params->LARc[IX(i - 1, 0, 7)], MIC[IX(i, 1, 8)]) << 10;
        // Remember that B is scaled down twice as much as A, so we need up 
        // shift left to get on equal terms
        int16_t temp2 = B[IX(i, 1, 8)] << 1;
        // Back out B
        temp1 = sub(temp1, temp2);
        // Divide by A 
        temp1 = mult_r(INVA[IX(i, 1, 8)], temp1);
        // What is this for?
        LARpp[IX(i, 1, 8)] = add(temp1, temp1);
        // IMPORTANT: IT APPEARS THAT WE ARE STILL AT HALF-SCALE of the original r[]
    }    

    for (uint16_t i = 1; i <= 8; i++) {

        // Section 5.2.9.1 - Interpolation of the LARpp[1..8] to get the LARp[1..8]

        // Zone 0
        int16_t temp = add((LARpp_last[i] >> 2), (LARpp[i] >> 2));
        LARp[0] = add(temp, (LARpp_last[i] >> 1));
        // Zone 1
        LARp[1] = add((LARpp_last[i] >> 1), (LARpp[i] >> 1));
        // Zone 2
        temp = add((LARpp_last[i] >> 2), (LARpp[i] >> 2));
        LARp[2] = add(temp, (LARpp[i] >> 1));
        // Zone 3
        LARp[3] = LARpp[i];

        // NUMERICAL NOTE: THE LARp[]s ARE AT 0.5 SCALE HERE

        // Section 5.2.9.2 - Computation of the rp[1..8] from the interpolated 
        // LARp[1..8].  This is the reverse of the simplified log-area 
        // quantization.

        for (uint16_t zone = 0; zone < 4; zone++) {
            // The sign will be added back later
            temp = s_abs(LARp[zone]);
            // 11059/32767 = 0.3375 = 0.675/2
            if (temp < 11059) {
                // NUMERICAL NOTE: The 0.5 scale is removed. 
                temp = temp << 1;
            } 
            // 20070/32767 = 0.6125 = 1.225/2
            else if (temp < 20070) {
                // NUMERICAL NOTE: The 0.5 scale is removed. 
                temp = add(temp, 11059);
            } 
            // 26112/32767 ~= 0.796875.  
            // (NOTE: The approximation appears to be off a bit)
            else {
                // NUMERICAL NOTE: The 0.5 scale is removed, and 
                // before the addition so the coefficient is already
                // in full scale.
                temp = add((temp >> 2), 26112);
            }

            // NUMERICAL NOTE: At this point we are at full scale.
            rp[zone][i] = temp;

            // Re-apply the sign if needed
            if (LARp[zone] < 0) {
                rp[zone][i] = sub(0, rp[zone][i]);
            }
        }
    }

    // NUMERICAL NOTE: At this point rp[] is back to the original 
    // scale of r[].

    // Once we've used the LARpp_last, copy the new values for next time
    for (uint16_t i = 1; i <= 8; i++) {
        LARpp_last[i] = LARpp[i];
    }
}

}
