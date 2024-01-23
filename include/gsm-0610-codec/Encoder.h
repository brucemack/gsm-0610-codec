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
#ifndef _Encoder_h
#define _Encoder_h

#include "Parameters.h"

namespace kc1fsz {

class Encoder {

public:

    // See table 5.1 on page 43
    // NOTE: The 0th entry is not used.  The draft uses index [1..8]

    // This matrix is scaled-down by 32
    // 20480/32767=0.625 which is 20/32
    static constexpr int16_t A[9] = { 0, 20480, 20480, 20480, 20480, 13964, 15360, 8534, 9036 };

    // This matrix is scaled-down by 64
    // 2048/32767=0.0625 which is 4/64
    static constexpr int16_t B[9] = { 0, 0, 0, 2048, -2560, 94, -1792, -341, -1144 };
    static constexpr int16_t MIC[9] = { 0, -32, -32, -16, -16, -8, -8, -4, -4 };
    static constexpr int16_t MAC[9] = { 0, 31, 31, 15, 15, 7, 7, 3, 3 };

    // See table 5.2 on page 43
    // This is used to invert the multiplication by A[] above.
    // NOTE: The 0th entry is not used.  The draft uses index [1..8]
    static constexpr int16_t INVA[9] = { 0, 13107, 13107, 13107, 13107, 19223, 17476, 31454, 29708 };

    // Table 5.3a: Decision level of the LTP gain quantizer
    // See page 43
    static constexpr int16_t DLB[4] = { 6554, 16384, 26214, 32767 };

    // Table 5.3b: Quantization levels of the LTP gain quantizer
    // See page 43
    static constexpr int16_t QLB[4] = { 3277, 11469, 21299, 32767 };

    // Table 5.4: Coefficients of the weighting filter
    // See page 43
    static constexpr int16_t H[11] = { -134, -374, 0, 2054, 5741, 8192, 5741, 2054, 0, -374, -134 };

    // Table 5.5: Normalized inverse mantissa used to compute xM/xmax
    // See page 44
    static constexpr int16_t NRFAC[8] = { 29128, 26215, 23832, 21846, 20165, 18725, 17476, 16384 };

    // Table 5.6: Normalized direct mantissa used to compute xM/xmax
    // See page 44
    static constexpr int16_t FAC[8] = { 18431, 20479, 22527, 24575, 26623, 28671, 30719, 32767 };

    /**
     * Converts an index k[0..159] to the zone[0..3] as defined in Table 3.2.
     */
    static uint16_t k2zone(uint16_t k);

    Encoder(bool homingSupported = true);
    
    /**
     * Sets the encoder back to the "home" state.
     */
    void reset();

    /**
     * Encodes a 160-sample frame and returns the parameters.
     * IMPORTANT: THE CALLER MUST ENSURE THAT inputPcm[] CONTAINS 160 SAMPLES.
    */
    void encode(const int16_t inputPcm[], Parameters* out);

    /**
     * Reconstructs the reflection coefficients in rp[] from the parameters. Uses
     * and updates LRPpp_last in the process.
     */
    static void decodeReflectionCoefficients(const Parameters* params, 
        int16_t* LARpp_last, int16_t rp[][9]);

    /**
     * Reverses the APCM coding of a pulse.
     * 
     * @params j The sub-segment number [0..3]
     */
    static void inverseAPCM(const Parameters* params, int16_t j, int16_t exp, int16_t mant, int16_t xMp[]);

    /**
     * Determines whether the frame is an Encoder Homing Frame.
     * 
     * The encoder-homing-frame consists of 160 identical samples, each 13 bits long, with the least
     * significant bit set to "one" and all other bits set to "zero". When written to 16-bit words 
     * with left justification, the samples have a value of 0008 hex
     */
    static bool isHomingFrame(const int16_t frame[]);

private:

    bool _homingSupported;
    bool _lastFrameHome;
    // State preserved between segments
    int16_t _z1;
    int32_t _L_z2;
    int16_t _mp;
    int16_t _LARpp_last[9];
    int16_t _u[8];
    // NOTE: Indexing in draft document is -120 to -1, but 
    // we treat this as 0 to 119.
    int16_t _dp[120];
};

}

#endif
