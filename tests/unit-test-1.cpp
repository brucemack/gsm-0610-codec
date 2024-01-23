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
        assert(area[0] == 0b00000101);
        Parameters::pack1(area, &state, 0b01010101, 8);
        assert(area[0] == 0b10101101);
        assert(area[1] == 0b00000010);
        
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
        assert(state.bitsUsed() == 260);

        state.reset();
        parms.unpack(area, &state);
        assert(state.bitsUsed() == 260);
   }

}

static void math_tests() {

    // A quick demo of the quantization used in section 3.1.7. We start with 
    // a number pre-scaled by 1/64.  An additional 9 shifts allows the 
    // q15 value to be treated like an integer - works for negatives as well.
    {
        // Start with 0.5, which is 32 shifted to the right by 6
        int16_t a = 16384;
        // Shift another 9, so the total shift is now 15
        a = a >> 9;
        // When we treat this like an integer we should get the 32 back
        assert(a == 32);

        // Start with -0.5, which is -32 shifted to the right by 6
        a = -16384;
        // Shift another 9, so the total shift is now 15
        a = a >> 9;
        // When we treat this like an integer we should get the -32 back
        assert(a == -32);
    }

    // Add with saturation
    {
        // Check overflow cases
        int16_t a = 32000, b = 1000;
        assert(sizeof(a) == 2);
        assert(add(a, b) == 32767);
        a = -32000;
        b = -1000;
        assert(add(a, b) == -32768);

        a = -1;
        assert((uint16_t)a == 0b1111111111111111);
        a = -2;
        assert((uint16_t)a == 0b1111111111111110);
        //std::bitset<16> y(a);
        //cout << y << endl;
        a = -32768;
        assert((uint16_t)a == 0b1000000000000000);
        a = 32767;
        assert((uint16_t)a == 0b0111111111111111);
    }

    // Subtraction with saturation
    {
        // Check overflow cases
        int16_t a = -32000, b = 1000;
        assert(sub(a, b) == -32768);
        a = 32000;
        b = -1000;
        assert(sub(a, b) == 32767);
    }

    // Mult
    {
        // -1 * 0.5 (approximately, but slightly smaller)
        int16_t a = -32768, b = 32767 / 2;
        assert(mult(a, b) == -16383);

        // -1 * -1
        a = -32768;
        b = -32768;
        // Make sure we wrap here in this special case
        assert(mult(a, b) == 32767);

        // -1 * 0.99999999....
        a = -32768;
        b = 32767;
        // Should be very close to -1
        assert(mult(a, b) == -32767);

        // 0.999999.... * -1
        a = 32767;
        b = -32768;
        // Should be very close to -1
        assert(mult(a, b) == -32767);
    }

    // Mult with rounding
    {
        // -1 * 0.5 (approximately, but slightly smaller)
        int16_t a = -32768, b = 32767 / 2;
        assert(mult_r(a, b) == -16383);

        // -1 * 0.5 (approximately, but slightly larger). 
        a = -32768;
        b = 32768 / 2;
        assert(mult_r(a, b) == -16384);

        // 0.5 * 0.5 
        a = 16384;
        b = 16384;
        assert(mult_r(a, b) == 8192);

        // -0.5 * 0.5 
        a = -16384;
        b = 16384;
        assert(mult_r(a, b) == -8192);

        // -1 * -1
        a = -32768;
        b = -32768;
        // Make sure we wrap here in this special case
        assert(mult_r(a, b) == 32767);

        // -1 * 0.99999999....
        a = -32768;
        b = 32767;
        // Should be very close to -1
        assert(mult_r(a, b) == -32767);

        // 0.999999.... * -1
        a = 32767;
        b = -32768;
        // Should be very close to -1
        assert(mult_r(a, b) == -32767);

        a = 32767;
        b = 0;
        assert(mult_r(a, b) == 0);

        a = 32766;
        b = 0;
        assert(mult_r(a, b) == 0);
    }

    // Abs
    {
        assert(s_abs(-32767) == 32767);
        assert(s_abs(32767) == 32767);
        // Saturation case
        assert(s_abs(-32768) == 32767);
    }

    // div
    {
        int16_t n, d;
        n = 32768 / 4;
        d = 32768 / 2;
        // 0.25 / 0.5 should equal 0.5
        assert(div(n, d) == 32768 / 2);
        // 0.25 / 0.25 should saturate to +32767
        assert(div(n, n) == 32767);
    }

    // 32-bit addition
    {
        int32_t a, b;
        a = 2147483647;
        b = 1;
        // We should saturate here
        assert(L_add(a, b) == a);

        a = -2147483648;
        b = -1;
        // We should saturate here
        assert(L_add(a, b) == a);
    }

    // 32-bit subtraction
    {
        int32_t a, b;
        a = 2147483647;
        b = -1;
        // We should saturate here
        assert(L_sub(a, b) == a);

        a = -2147483648;
        b = 1;
        // We should saturate here
        assert(L_sub(a, b) == a);
    }

    /*
    // 32-bit multiplication
    {
        int32_t a, b;
        // 0.9999999
        a = 2147483647;
        b = 2147483648 / 2;
        assert(L_mult(a, b) == -2147483648);
    }
    */

    // Normalization 
    {
        int32_t a;
        a = 2147483647;
        assert(norm(a) == 0);
        a = 1073741825;
        assert(norm(a) == 0);
        a = 1073741824;
        assert(norm(a) == 0);
        a = 1073741823;
        assert(norm(a) == 1);

        a = -2147483648;
        assert(norm(a) == 0);
        a = -2147483647;
        assert(norm(a) == 0);
        a = -1073741825;
        assert((uint32_t)a == 0b10111111111111111111111111111111);
        assert(norm(a) == 0);
        a = -1073741824;
        assert((uint32_t)a == 0b11000000000000000000000000000000);
        assert(norm(a) == 0);
        a = -1073741823;
        assert((uint32_t)a == 0b11000000000000000000000000000001);
        assert(norm(a) == 1);
    }
}

/*
static void make_pcm_file() {

    std::string inp_fn = "../tests/gsm-06.10/data/Seq01.inp";
    std::ifstream ifile(inp_fn, std::ios::binary);
    if (!ifile.good()) {
        return;
    }
    
    std::string out_fn = "c:/tmp/out.txt";
    std::ofstream ofile(out_fn);
    if (!ofile.good()) {
        return;
    }

    uint32_t sampleCount = 0;

    uint8_t f[2];
    while (ifile.read((char*)f, 2)) {
        uint16_t sample = (uint16_t)f[1];
        sample = sample << 8;
        sample |= (uint16_t)f[0];
        // Anything in the low 3?
        if ((sample & 0b111) != 0) {
            std::cout << "ERROR" << std::endl;
            return;
        }
        ofile << (int16_t)sample << std::endl;
        sampleCount++;
    }

    ifile.close();
    ofile.close();
    std::cout << "Convert done " << sampleCount / 160 << std::endl;
}
*/

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

    //assert(segmentCount == 584);

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

    math_tests();
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
