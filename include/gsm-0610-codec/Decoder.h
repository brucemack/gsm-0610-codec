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
#ifndef _Decoder_h
#define _Decoder_h

#include "Parameters.h"

namespace kc1fsz {

/**
 * A GSM 06.10 decoder based on the official documentation marked:
 *  "Draft ETSI EN 300 961 V8.0.1 (2000-07)"
 * 
 * https://www.etsi.org/deliver/etsi_EN/300900_300999/300961/08.00.01_40/en_300961v080001o.pdf
 * 
 * GSM decoding must maintain state between frames so a single instance 
 * of this class should be created/maintained per decoding stream. 
 */
class Decoder {
public:

    Decoder();

    /**
     * Returns the decoder to the "home" state.
     */
    void reset();

    /**
     * Converts a set of frame parameters into a single frame of 
     * 160 PCM samples (13-bit, left-aligned).  This implies
     * that the low three bits will be zero.
    */
    void decode(const Parameters* in, int16_t* outputPcm);

private:

    int16_t _nrp;
    int16_t _drp[160];
    int16_t _LARpp_last[9];
    int16_t _v[9];
    int16_t _msr;
};

}

#endif
