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

namespace kc1fsz {

/**
 * Performs the addition (var1+var2) with overflow control and saturation; the result is set at +32767
 * when overflow occurs or at -32768 when underflow occurs.
 */
int16_t add(int16_t var1, int16_t var2) {
    // Implementation for a 32-bit native system
    int32_t var1_ext = var1, var2_ext = var2;
    int32_t res = var1_ext + var2_ext;
    // Saturate
    if (res > 32767) {
        return 32767;
    } else if (res < -32768) {
        return -32768;
    } else {
        return (int32_t)res;
    }
}

/**
 * Performs the subtraction (var1-var2) with overflow control and saturation; the result is set at
 * +32767 when overflow occurs or at -32768 when underflow occurs.
 */
int16_t sub(int16_t var1, int16_t var2) {
    // Implementation for a 32-bit native system
    int32_t var1_ext = var1, var2_ext = var2;
    int32_t res = var1_ext - var2_ext;
    // Saturate
    if (res > 32767) {
        return 32767;
    } else if (res < -32768) {
        return -32768;
    } else {
        return (int32_t)res;
    }
}

/**
 * Performs the multiplication of var1 by var2 and gives a 16 bits result which is scaled i.e.
 * mult(var1,var2 ) = (var1 times var2) >> 15 and mult(-32768, -32768) = 32767.
 */
int16_t mult(int16_t var1, int16_t var2) {
    // Special case of -1 x -1
    if (var1 == -32768 && var2 == -32768) {
        return 32767;
    }
    // Implementation for a 32-bit native system
    int32_t var1_ext = var1, var2_ext = var2;
    int32_t res = var1_ext * var2_ext;
    return (int16_t)(res >> 15);
}

/**
 * Same as mult but with rounding i.e. mult_r( var1, var2 ) = ( (var1 times var2) + 16384 ) >> 15
 * and mult_r( -32768, -32768 ) = 32767
 */
int16_t mult_r(int16_t var1, int16_t var2) {
    // Special case of -1 x -1
    if (var1 == -32768 && var2 == -32768) {
        return 32767;
    }
    // Implementation for a 32-bit native system
    int32_t var1_ext = var1, var2_ext = var2;
    // Add the 0.5 to force a round of the final LSB
    int32_t res = (var1_ext * var2_ext) + 16384;
    return (int16_t)(res >> 15);
}

/** 
 * Absolute value of var1; abs(-32768) = 32767
 */ 
int16_t s_abs(int16_t var1) {
    // Special case of -1
    if (var1 == -32768) {
        return 32767;
    }
    if (var1 < 0) {
        return -var1;
    }
    return var1;
}

/**
 * div produces a result which is the fractional integer division of var1 by var2; var1 and var2 shall
 * be positive and var2 shall be greater or equal to var1; The result is positive (leading bit equal to 0)
 * and truncated to 16 bits. if var1 == var2 then div( var1, var2 ) = 32767
 * 
 * NOTE: See section 5.2.5 - this is an out-of-the-book implementation
 */
int16_t div(int16_t num, int16_t denum) {
    assert(num >= 0);
    assert(num >= 0);
    assert(denum >= num);

    // Special case
    if (num == denum) {
        return 32767;
    }

    int32_t L_num = num;
    int32_t L_denum = denum;
    int16_t div = 0;
    
    for (uint16_t k = 0; k <= 14; k++) {
        div = div << 1;
        L_num = L_num << 1;
        if (L_num >= L_denum) {
            L_num = L_sub(L_num, L_denum);
            div = add(div, 1);
        }
    }
    return div;
}

/** 
 * L_mult is a 32 bit result for the multiplication of var1 times var2 with a one bit shift left.
 * L_mult( var1, var2 ) = ( var1 times var2 ) << 1. The condition L_mult (-32768, -32768 ) does not
 * occur in the [GSM] algorithm.
 * 
 * NOTE: This function incorporates multiplication and switching from q15 to q31 in a single
 * operation.
 */
int32_t L_mult(int16_t var1, int16_t var2) {
    return ((int32_t)var1 * (int32_t)var2) << 1;
}

/**
 * 32 bits addition of two 32 bits variables (L_var1 + L_var2) with overflow control and
 * saturation; the result is set at 2147483647 when overflow occurs and at -2147483648 when
 * underflow occurs.
 */
int32_t L_add(int32_t L_var1, int32_t L_var2) {
    // Here we use the platform support:

/*
#ifdef PICO_BUILD
    // CHANGED ON PI PICO - For some reason int32_t and int are 
    // not compatible on the platform, even though they are 
    // the same length.
    int L_res;
#else
    int32_t L_res;
#endif
*/
    // BRM 2025-02-01 - Switching default to int
    int L_res;

    bool of = __builtin_sadd_overflow(L_var1, L_var2, &L_res);
    // On overflow we can saturate the result
    if (of) {
        if (L_res < 0) {
            return 2147483647;
        } else if (L_res > 0) {
            return -2147483648;
        } else {
            return 0;
        }
    } else {
        return L_res;
    }
}

/**
 * 32 bits subtraction of two 32 bits variables (L_var1 - L_var2) with overflow control and
 * saturation; the result is set at 2147483647 when overflow occurs and at -2147483648 when
 * underflow occurs.
*/
int32_t L_sub(int32_t L_var1, int32_t L_var2) {
    // Here we use the platform support:

#ifdef PICO_BUILD
    // CHANGED ON PI PICO - For some reason int32_t and int are 
    // not compatible on the platform, even though they are 
    // the same length.
    int L_res;
#else
    int32_t L_res;
#endif

    bool of = __builtin_ssub_overflow(L_var1, L_var2, &L_res);
    // On overflow we can saturate the result
    if (of) {
        if (L_res < 0) {
            return 2147483647;
        } else if (L_res > 0) {
            return -2147483648;
        } else {
            return 0;
        }
    } else {
        return L_res;
    }
}

/**
 * norm produces the number of left shifts needed to normalize the 32 bits variable L_var1 for
 * positive values on the interval with minimum of 1073741824 and maximum of 2147483647 and
 * for negative values on the interval with minimum of -2147483648 and maximum of -1073741824;
 * in order to normalize the result, the following operation shall be done: L_norm_var1 = L_var1 <<
 * norm(L_var1)
*/
int16_t norm(int32_t L_var1) {

    const int32_t normMask = 0b01000000000000000000000000000000;
    int16_t count = 0;

    if (L_var1 == 0 || L_var1 == -2147483648) {
        return 0;
    } else if (L_var1 == -1073741824) {
        return 0;
    } else if (L_var1 > 0) {
        while ((L_var1 & normMask) == 0) {
            L_var1 = L_var1 << 1;
            count++;
        }
    } else {
        while ((L_var1 & normMask) != 0) {
            L_var1 = L_var1 << 1;
            count++;
        }
    }
    return count;
}

}
