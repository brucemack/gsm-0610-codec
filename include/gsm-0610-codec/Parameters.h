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
#ifndef _Parameters_h
#define _Parameters_h

#include <cstdint>

namespace kc1fsz {

class PackingState {
public:

    PackingState();

    uint16_t bitPtr;
    uint16_t bytePtr;

    uint16_t bitsUsed() const;
    void reset();
};

/**
 * Sub-segment parameters.
 * 
 * NOTE: The parameters are represented by 16-bit integers for convenience
 * only - none of the parameters require that precision.
 */
class SubSegParameters {
public:

    SubSegParameters();

    uint16_t Nc;
    uint16_t bc;
    uint16_t Mc;
    uint16_t xmaxc;
    uint16_t xMc[13];

    bool isEqualTo(const SubSegParameters& other) const;

    /**
     * Packs the sub-segment parameters into the specified area.
     * Please see table 1.1 on page 11 for full information.
    */
    void pack(uint8_t* stream, PackingState* streamState) const;
    
    void unpack(const uint8_t* stream, PackingState* streamState);
};

/**
 * Segment parameters.
 * 
 * NOTE: The parameters are represented by 16-bit integers for convenience
 * only - none of the parameters require that precision.
 */
class Parameters {
public:

    Parameters();

    uint16_t LARc[8];
    SubSegParameters subSegs[4];

    bool isEqualTo(const Parameters& other) const;
    
    /**
     * This function will write 33 bytes of the stream area, so the caller 
     * is responsible for making sure that space is available.
     * 
     * This follows RFC 3551 in section 4.5.8.1
     * 
     * https://datatracker.ietf.org/doc/html/rfc3551#section-4.5.8.1
     */
    void pack(uint8_t* stream, PackingState* streamState) const;

    /**
     * Same as above, but packing state is internal.
     */
    void pack(uint8_t* stream) const;

    static bool isValidFrame(const uint8_t* buf);

    /**
     * This function will read 33 bytes of the stream area, so the caller
     * is responsible for making sure that the space is available.
     * 
     * This follows RFC 3551 in section 4.5.8.1
     * 
     * https://datatracker.ietf.org/doc/html/rfc3551#section-4.5.8.1
     */    
    void unpack(const uint8_t* stream, PackingState* streamState);

    /**
     * Same as above, but packing state is internal.
     */
    void unpack(const uint8_t* stream);

    /**
     * Packs one parameter to the specified stream.
     * NOTE: Only works for parameters <= 8 bits (as needed)
     */
    static void pack1(uint8_t* stream, PackingState* streamState, uint16_t parameter, 
        uint16_t bits);

    /**
     * Unpacks one parameter from the specified stream.
     * NOTE: Only works for parameters <= 8 bits (as needed)
     */
    static uint8_t unpack1(const uint8_t* stream, PackingState* streamState, uint16_t bits);
};

}    

#endif
