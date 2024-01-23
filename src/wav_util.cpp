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
#include <cstdint>
#include <cstring>

#include "gsm-0610-codec/wav_util.h"

using namespace std;

namespace kc1fsz {

/**
 * Writes a 32-bit integer in little-endian format.
 */
static void write32LE(ostream& str, uint32_t i) {
    uint8_t b;
    b = i & 0xff;
    str.write((char*)&b, 1);
    b = (i >> 8) & 0xff;
    str.write((char*)&b, 1);
    b = (i >> 16) & 0xff;
    str.write((char*)&b, 1);
    b = (i >> 24) & 0xff;
    str.write((char*)&b, 1);
}

/**
 * Writes a 16-bit integer in little-endian format.
 */
static void write16LE(ostream& str, uint16_t i) {
    uint8_t b;
    b = i & 0xff;
    str.write((char*)&b, 1);
    b = (i >> 8) & 0xff;
    str.write((char*)&b, 1);
}

static uint32_t read32LE(istream& str) {
    uint32_t result;
    uint8_t b[4];
    str.read((char*)b, 4);
    result = b[3];
    result <<= 8;
    result |= b[2];
    result <<= 8;
    result |= b[1];
    result <<= 8;
    result |= b[0];
    return result;
}

static uint16_t read16LE(istream& str) {
    uint16_t result;
    uint8_t b[2];
    str.read((char*)b, 2);
    result = b[1];
    result <<= 8;
    result |= b[0];
    return result;
}

/**
 * Here is a decent reference: http://soundfile.sapp.org/doc/WaveFormat/
 */
void encodeFromPCM16(const int16_t pcm[], uint32_t samples, std::ostream& str, 
    uint16_t samplesPerSecond) {

    uint32_t len = (samples * 2) + 36;

    str.write("RIFF", 4);
    write32LE(str, len);
    str.write("WAVE", 4);

    uint16_t i16;
    uint32_t i32;

    str.write("fmt ", 4);

    // Sub Chunk 1 size
    i32 = 16;
    write32LE(str, i32);

    // PCM format
    i16 = 1;
    write16LE(str, i16);

    // Number of channels
    i16 = 1;
    write16LE(str, i16);

    // Sample rate
    i32 = samplesPerSecond;
    write32LE(str, i32);

    // Byte rate
    i32 = samplesPerSecond * 2;
    write32LE(str, i32);

    // Block align
    i16 = 2;
    write16LE(str, i16);

    // Bits per sample
    i16 = 16;
    write16LE(str, i16);

    str.write("data", 4);
    // Bytes of audio
    i32 = samples * 2;
    write32LE(str, i32);
    // The actual data 
    for (uint32_t i = 0; i < samples; i++) {
        write16LE(str, pcm[i]);
    }   
}

int decodeToPCM16(std::istream& str, int16_t pcm[], uint32_t maxSamples) {

    char buf[4];
    // RIFF
    str.read(buf, 4);
    if (!memcmp(buf, "RIFF", 4) == 0) {
        return -1;
    }

    //uint32_t totalLen = read32LE(str);
    read32LE(str);

    // WAVE
    str.read(buf, 4);
    if (!memcmp(buf, "WAVE", 4) == 0) {
        return -1;
    }

    // fmt
    str.read(buf, 4);
    if (!memcmp(buf, "fmt ", 4) == 0) {
        return -1;
    }

    // Sub Chunk 1 size
    //uint32_t subChunkSize1 = read32LE(str);
    read32LE(str);

    // Format
    uint16_t format = read16LE(str);
    if (format != 1) {
        std::cout << "Format " << format << std::endl;
        return -2;
    }

    // Channels
    uint16_t channels = read16LE(str);
    if (channels != 1) {
        return -3;
    }

    //uint32_t sampleRate = read32LE(str);
    read32LE(str);
    //uint32_t byteRate = read32LE(str);
    read32LE(str);
    //uint16_t blockAlign = read16LE(str);
    read16LE(str);

    uint16_t bitsPerSample = read16LE(str);
    if (bitsPerSample != 16) {
        return -4;
    }

    // data
    str.read(buf, 4);
    if (!memcmp(buf, "data", 4) == 0) {
        return -1;
    }

    uint32_t bytes = read32LE(str);
    uint32_t samples = bytes / 2;

    for (uint32_t i = 0; i < samples && i < maxSamples; i++) {
        pcm[i] = read16LE(str);
    }

    if (samples > maxSamples) {
        return maxSamples;
    } else {
        return samples;        
    }
}

}

