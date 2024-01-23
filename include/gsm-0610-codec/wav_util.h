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
#ifndef _wavutil_h
#define _wavutil_h

#include <cstdint>
#include <iostream>

namespace kc1fsz {

/**
 * Takes a list of PCM samples (16-bit) and creates a .WAV stream.
*/
void encodeFromPCM16(const int16_t pcm[], uint32_t samples, std::ostream& str, 
    uint16_t samplesPerSecond);

/**
 * @returns Negative number for error, otherwise the number of samples read.
 */
int decodeToPCM16(std::istream& str, int16_t pcm[], uint32_t maxSamples);

}

#endif


