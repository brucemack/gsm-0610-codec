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
#include <iostream>
#include <bitset>
#include <string>
#include <fstream>
#include <cstring>

#include "fixed_math.h"
#include "gsm-0610-codec/Parameters.h"
#include "gsm-0610-codec/Encoder.h"
#include "gsm-0610-codec/Decoder.h"
#include "gsm-0610-codec/wav_util.h"

// Utility
//#define q15_to_f32(a) ((float)(a) / 32768.0f)

using namespace kc1fsz;

static void pack_tests() {

    {    
        PackingState state;
        uint8_t area[2] = { 0, 0 };
        // Pack two parameters and make sure they go to the right place
        Parameters::pack1(area, &state, 0b101, 3);
        assert(area[0] == 0b10100000);
        Parameters::pack1(area, &state, 0b01010101, 8);
        assert(area[0] == 0b10101010);
        assert(area[1] == 0b10100000);
        
        state.reset();
        assert(Parameters::unpack1(area, &state, 3) == 0b101);
        uint8_t x = Parameters::unpack1(area, &state, 8);
        assert(x == 0b01010101);
    }

    {
        PackingState state;
        uint8_t area[33];
        Parameters parms;
        parms.pack(area, &state);
        assert(state.bitsUsed() == 264);
        assert(Parameters::isValidFrame(area));

        PackingState state2;
        Parameters parms2;
        parms2.unpack(area, &state2);
        assert(state2.bitsUsed() == 264);
        assert(parms2.isEqualTo(parms));
   }

}

static void test_wav(const char* inFn, const char* outFn) {

    std::string inp_fn = inFn;
    std::ifstream inp_file(inp_fn, std::ios::binary);
    if (!inp_file.good()) {
        assert(false);
    }

    int16_t pcmData[160 * 2048];
    uint32_t pcmPtr = 0;

    // Read through the files in tandem and compare
    while (!inp_file.eof()) {

        // Read a segment of sound and convert it to 16-bit 
        uint8_t f[1024];
        inp_file.read((char*)f, 160 * 2);
        int r = inp_file.gcount() / 2;
        // NOTE: We are not making any assumptions about Endianness 
        uint16_t p = 0;
        for (uint16_t i = 0; i < r; i++) {
            // LSBs first
            uint16_t sample = (uint16_t)f[p + 1];
            sample = sample << 8;
            sample |= (uint16_t)f[p];
            pcmData[pcmPtr++] = sample;
            p += 2;
        }
    }

    inp_file.close();

    std::ofstream out_file(outFn, std::ios::binary);
    if (!out_file.good()) {
        assert(false);
    }

    encodeFromPCM16((const int16_t*)pcmData, pcmPtr, out_file, 8000);

    out_file.close();

}

static int encoder_test(const char* baseFn) {

    // 76 parameters, each coded in 16-bit words
    assert(sizeof(Parameters) == 76 * 2);

    // This is stateful so we keep it outside of the mail loop
    Encoder encoder;
    int segmentCount = 0;

    std::string inp_fn = baseFn;
    inp_fn += ".inp";
    std::ifstream inp_file(inp_fn, std::ios::binary);
    if (!inp_file.good()) {
        assert(false);
    }

    std::string cod_fn = baseFn;
    cod_fn += ".cod";
    std::ifstream cod_file(cod_fn, std::ios::binary);
    if (!cod_file.good()) {
        assert(false);
    }

    // Read through the files in tandem and compare
    while (!inp_file.eof() && !cod_file.eof()) {

        // Read a segment of sound and convert it to 16-bit 
        uint8_t f[160 * 2];
        inp_file.read((char*)f, 160 * 2);
        if (!inp_file) {
            break;
        }
        // NOTE: We are not making any assumptions about Endianness 
        int16_t inp_pcm[160];
        uint16_t p = 0;
        for (uint16_t i = 0; i < 160; i++) {
            // LSBs first
            uint16_t sample = (uint16_t)f[p + 1];
            sample = sample << 8;
            sample |= (uint16_t)f[p];
            inp_pcm[i] = sample;
            p += 2;
        }

        Parameters expected_params;
        // NOTE: THERE IS AN ENDIANNESS ASSUMPTION HERE!
        cod_file.read((char*)&expected_params, 76 * 2);
        if (!cod_file) {
            break;
        }

        // Do the encoding and check
        Parameters computed_params;
        encoder.encode(inp_pcm, &computed_params);

        assert(computed_params.isEqualTo(expected_params));

        segmentCount++;
    }

    inp_file.close();
    cod_file.close();

    return segmentCount;
}

static int decoder_test(const char* baseFn) {

    // This is stateful so we keep it outside of the mail loop
    Decoder decoder;
    int segmentCount = 0;

    std::string cod_fn = baseFn;
    cod_fn += ".cod";
    std::ifstream cod_file(cod_fn, std::ios::binary);
    if (!cod_file.good()) {
        assert(false);
    }

    std::string out_fn = baseFn;
    out_fn += ".out";
    std::ifstream out_file(out_fn, std::ios::binary);
    if (!out_file.good()) {
        assert(false);
    }

    // Read through the files in tandem and compare
    while (!out_file.eof() && !cod_file.eof()) {

        Parameters params;
        // NOTE: THERE IS AN ENDIANNESS ASSUMPTION HERE!
        cod_file.read((char*)&params, 76 * 2);
        if (!cod_file) {
            break;
        }

        // Read a segment of sound and convert it to 16-bit 
        uint8_t f[160 * 2];
        out_file.read((char*)f, 160 * 2);
        if (!out_file) {
            break;
        }
        // NOTE: We are not making any assumptions about Endianness 
        int16_t expected_pcm[160];
        uint16_t p = 0;
        for (uint16_t i = 0; i < 160; i++) {
            // LSBs first
            uint16_t sample = (uint16_t)f[p + 1];
            sample = sample << 8;
            sample |= (uint16_t)f[p];
            expected_pcm[i] = sample;
            p += 2;
        }

        // Do the Decoding and check
        int16_t computed_pcm[160];
        decoder.decode(&params, computed_pcm);

        assert(memcmp((void *)expected_pcm, (void*)computed_pcm, 160 * 2) == 0);

        segmentCount++;
    }

    out_file.close();
    cod_file.close();

    return segmentCount;
}

static void etsi_test_files() {

    // Run all tests on DISK #1.  
    assert(encoder_test("../tests/data/Seq01") == 584);
    assert(decoder_test("../tests/data/Seq01") == 584);
    assert(encoder_test("../tests/data/Seq02") == 947);
    assert(decoder_test("../tests/data/Seq02") == 947);
    assert(encoder_test("../tests/data/Seq03") == 673);
    assert(decoder_test("../tests/data/Seq03") == 673);
    assert(encoder_test("../tests/data/Seq04") == 520);
    assert(decoder_test("../tests/data/Seq04") == 520);
    // Decoder-only test
    assert(decoder_test("../tests/data/Seq05") == 64);
    
    // Make some waves!
    test_wav("../tests/data/Seq01.inp", "../tmp/Seq01.wav");
    test_wav("../tests/data/Seq02.inp", "../tmp/Seq02.wav");
    test_wav("../tests/data/Seq03.inp", "../tmp/Seq03.wav");
    test_wav("../tests/data/Seq04.inp", "../tmp/Seq04.wav");
}

int main(int, const char**) {

    pack_tests();
    etsi_test_files();

    // A demonstration of encoding a "normal" .WAV file
    {   
        std::string inp_fn = "../tests/data/male-1.wav";
        std::string out_fn = "../tmp/male-1-out.wav";

        const uint32_t in_pcm_max = 160 * 1024;
        int16_t in_pcm[in_pcm_max];

        std::ifstream inp_file(inp_fn, std::ios::binary);
        if (!inp_file.good()) {
            assert(false);
        }
        int samples = decodeToPCM16(inp_file, in_pcm, in_pcm_max);
        inp_file.close();

        uint16_t segments = samples / 160;

        Encoder encoder;
        Decoder decoder;

        int16_t out_pcm[160 * 1024];

        for (uint16_t segment = 0; segment < segments; segment++) {

            // Do the encoding 
            Parameters params;
            encoder.encode(&(in_pcm[segment * 160]), &params);

            // Do the decoding
            decoder.decode(&params, &(out_pcm[segment * 160]));
        }

        std::ofstream out_file(out_fn, std::ios::binary);
        if (!out_file.good()) {
            assert(false);
        }
        encodeFromPCM16(out_pcm, samples, out_file, 8000);
        out_file.close();
    }
}
