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

using namespace std;
using namespace kc1fsz;

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

int main(int, const char**) {
    math_tests();
}