/*
 * Copyright (C) 2019-2022, Xilinx, Inc.
 * Copyright (C) 2022-2024, Advanced Micro Devices, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _DSPLIB_CONV_CORR_TRAITS_HPP_
#define _DSPLIB_CONV_CORR_TRAITS_HPP_

#ifndef INLINE_DECL
#define INLINE_DECL inline __attribute__((always_inline))
#endif
#ifndef NOINLINE_DECL
#define NOINLINE_DECL inline __attribute__((noinline))
#endif

/*
    Convolution and Correlation traits.
    This file contains sets of overloaded, templatized and specialized templatized functions which
    encapsulate properties of the intrinsics used by the main kernal class. Specifically,
    this file does not contain any vector types or intrinsics since it is required for construction
    and therefore must be suitable for the aie compiler graph-level compilation.
*/

#include <stdio.h>
#include <type_traits>
#include <adf.h>
#include "device_defs.h"
#include "aie_api/aie_adf.hpp"
#include "fir_utils.hpp"

// using namespace std;
namespace xf {
namespace dsp {
namespace aie {
namespace conv_corr {

// Function to return the size of Y Reg
static constexpr unsigned MAX_WIDTH_Y_REG() {
    return 1024;
};

// Function to return the size of X Reg
static constexpr unsigned MAX_WIDTH_X_REG() {
    return 512;
};

// Function to return the size of bits to load on AIE
static constexpr unsigned MAX_BITS_LOAD_ON_AIE() {
    return 256;
};

// Function to return the size of bits to load on AIE
static constexpr unsigned MAX_BUFFER_LEN_ON_AIE_IN_BITS() {
    return 65536;
}; // 8192bytes --> (8192*8) bits

// Function to return the default lanes based on choosen architecture
template <typename TT_DATA_F, typename TT_DATA_G>
INLINE_DECL constexpr unsigned int aie_CC_NumLanes() {
    return 32; // defualt return for AIE2
};

// Function to return the default MULS/MACS based on choosen architecture
template <typename TT_DATA_F, typename TT_DATA_G>
INLINE_DECL constexpr unsigned int aie_CC_NumMuls() {
    return 256; // defualt return for AIE2
};

// Function to return the default sample size of F Sig
template <typename TT_DATA_F>
INLINE_DECL constexpr unsigned int aie_CC_Fsample_Size() {
    return 8; // defualt sample size is 8
};

// Function to return the default sample size of G Sig
template <typename TT_DATA_G>
INLINE_DECL constexpr unsigned int aie_CC_Gsample_Size() {
    return 8; // defualt sample size is 8
};

// Function to return the max sample size of G Signal
template <typename TT_DATA_G>
INLINE_DECL constexpr unsigned int aie_CC_Size() {
    return 32; // defualt multiple factor is 32 for both AIE-1 and AIE-2
};

// for io buffer cases
// function to return the number of acc's lanes for a type combo
#ifdef _SUPPORTS_DEVICE_AIE_1_
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumLanes<int8, int8>() {
    return 16;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumLanes<int16, int8>() {
    return 16;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumLanes<int16, int16>() {
    return 16;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumLanes<int32, int16>() {
    return 8;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumLanes<float, float>() {
    return 8;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumLanes<float, cfloat>() {
    return 4;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumLanes<cint16, int16>() {
    return 8;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumLanes<cint16, int32>() {
    return 8;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumLanes<cint16, cint16>() {
    return 8;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumLanes<cint32, int16>() {
    return 4;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumLanes<cint32, cint16>() {
    return 4;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumLanes<cfloat, float>() {
    return 4;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumLanes<cfloat, cfloat>() {
    return 4;
}; //

// function to return the number of multiplications for a type combo
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumMuls<int8, int8>() {
    return 128;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumMuls<int16, int8>() {
    return 64;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumMuls<int16, int16>() {
    return 32;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumMuls<int32, int16>() {
    return 16;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumMuls<float, float>() {
    return 8;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumMuls<float, cfloat>() {
    return 4;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumMuls<cint16, int16>() {
    return 16;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumMuls<cint16, int32>() {
    return 8;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumMuls<cint16, cint16>() {
    return 8;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumMuls<cint32, int16>() {
    return 8;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumMuls<cint32, cint16>() {
    return 4;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumMuls<cfloat, float>() {
    return 4;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumMuls<cfloat, cfloat>() {
    return 4;
}; //

#endif

#ifdef _SUPPORTS_DEVICE_AIE_2_
// _SUPPORTS_DEVICE_AIE_2_
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumLanes<int8, int8>() {
    return 32;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumLanes<int16, int8>() {
    return 16;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumLanes<int16, int16>() {
    return 16;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumLanes<int32, int16>() {
    return 16;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumLanes<float, float>() {
    return 16;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumLanes<bfloat16, bfloat16>() {
    return 16;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumLanes<cint16, int16>() {
    return 16;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumLanes<cint16, int32>() {
    return 16;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumLanes<cint16, cint16>() {
    return 8;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumLanes<cint32, int16>() {
    return 8;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumLanes<cint32, cint16>() {
    return 8;
}; //

// function to return the number of multiplications for a type combo
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumMuls<int8, int8>() {
    return 256;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumMuls<int16, int8>() {
    return 128;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumMuls<int16, int16>() {
    return 64;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumMuls<int32, int16>() {
    return 64;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumMuls<float, float>() {
    return 16;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumMuls<bfloat16, bfloat16>() {
    return 128;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumMuls<cint16, int16>() {
    return 64;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumMuls<cint16, int32>() {
    return 64;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumMuls<cint16, cint16>() {
    return 32;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumMuls<cint32, int16>() {
    return 32;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_NumMuls<cint32, cint16>() {
    return 8;
}; //

#endif

// function to return number of samples for given data type of F Sig
template <>
INLINE_DECL constexpr unsigned int aie_CC_Fsample_Size<int8>() {
    return 8;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_Fsample_Size<int16>() {
    return 16;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_Fsample_Size<int32>() {
    return 32;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_Fsample_Size<float>() {
    return 32;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_Fsample_Size<cint16>() {
    return 32;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_Fsample_Size<cint32>() {
    return 64;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_Fsample_Size<cfloat>() {
    return 64;
}; //
#ifdef _SUPPORTS_DEVICE_AIE_2_
template <>
INLINE_DECL constexpr unsigned int aie_CC_Fsample_Size<bfloat16>() {
    return 16;
}; //
#endif

// function to return number of samples for given data type of G Sig
template <>
INLINE_DECL constexpr unsigned int aie_CC_Gsample_Size<int8>() {
    return 8;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_Gsample_Size<int16>() {
    return 16;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_Gsample_Size<int32>() {
    return 32;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_Gsample_Size<float>() {
    return 32;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_Gsample_Size<cint16>() {
    return 32;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_Gsample_Size<cint32>() {
    return 64;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_Gsample_Size<cfloat>() {
    return 64;
}; //
#ifdef _SUPPORTS_DEVICE_AIE_2_
template <>
INLINE_DECL constexpr unsigned int aie_CC_Gsample_Size<bfloat16>() {
    return 16;
}; //
#endif

// function to return number of samples for given data type
template <>
INLINE_DECL constexpr unsigned int aie_CC_Size<int8>() {
    return 8;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_Size<int16>() {
    return 16;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_Size<int32>() {
    return 32;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_Size<float>() {
    return 32;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_Size<cint16>() {
    return 32;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_Size<cint32>() {
    return 64;
}; //
template <>
INLINE_DECL constexpr unsigned int aie_CC_Size<cfloat>() {
    return 64;
}; //
#ifdef _SUPPORTS_DEVICE_AIE_2_
template <>
INLINE_DECL constexpr unsigned int aie_CC_Size<bfloat16>() {
    return 16;
}; //
#endif

// Function returns number of columns used by MUL/MACs in Conv_Corr
template <typename TT_DATA_F, typename TT_DATA_G>
INLINE_DECL constexpr unsigned int getNumofMULs() {
    return aie_CC_NumMuls<TT_DATA_F, TT_DATA_G>();
};

// Function to return the number of lanes based on given data types
template <typename TT_DATA_F, typename TT_DATA_G>
INLINE_DECL constexpr unsigned int getNumofLanes() {
    return aie_CC_NumLanes<TT_DATA_F, TT_DATA_G>();
};

// Function to return the number of points based on given data types
template <typename TT_DATA_F, typename TT_DATA_G>
INLINE_DECL constexpr unsigned int getNumofPoints() {
    return (aie_CC_NumMuls<TT_DATA_F, TT_DATA_G>() / aie_CC_NumLanes<TT_DATA_F, TT_DATA_G>());
};

// Function to return the number of points based on given data types
template <typename TT_DATA_F>
INLINE_DECL constexpr unsigned int getNum_F_Samples() {
    return aie_CC_Fsample_Size<TT_DATA_F>();
};

// Function to return the number of G Sig. samples to read based on given data types
template <typename TT_DATA_G>
INLINE_DECL constexpr unsigned int getNum_G_Samples() {
    return aie_CC_Gsample_Size<TT_DATA_G>();
};

// Function to return the resultant value of Log base 2
template <unsigned int TP_LENGTH>
INLINE_DECL constexpr unsigned int getLog2() {
    switch (TP_LENGTH) {
        case 4:
            return 2;
            break;
        case 8:
            return 3;
            break;
        case 16:
            return 4;
            break;
        case 32:
            return 5;
            break;
        case 64:
            return 6;
            break;
        case 128:
            return 7;
            break;
        case 256:
            return 8;
            break;
        case 512:
            return 9;
            break;
        case 1024:
            return 10;
            break;
        case 2048:
            return 11;
            break;
        case 4096:
            return 12;
            break;
        case 8192:
            return 13;
            break;
        case 16384:
            return 14;
            break;
        case 32768:
            return 15;
            break;
        case 65536:
            return 16;
            break;
        default:
            printf("Error in PowerofLen\n");
            return 0;
            break;
    }
};

// Function to return the offset to move G Sig Pointer.
template <typename TT_DATA_G, unsigned int TP_DATA_LEN_G>
INLINE_DECL constexpr unsigned int getOffsetGsig() {
    unsigned int G_Load = (MAX_BITS_LOAD_ON_AIE() / aie_CC_Gsample_Size<TT_DATA_G>());
    return ((TP_DATA_LEN_G / G_Load) - 1);
};

// Function to return Maximum supported length based on given DATA TYPE.
template <typename TT_DATA>
INLINE_DECL constexpr unsigned int getMaxLen() {
    return ((MAX_BUFFER_LEN_ON_AIE_IN_BITS() / aie_CC_Size<TT_DATA>()));
};

// Function to return Minimum supported length based on given DATA TYPE.
template <typename TT_DATA>
INLINE_DECL constexpr unsigned int getMinLen() {
    return (((MAX_BITS_LOAD_ON_AIE() << 1) / aie_CC_Size<TT_DATA>()));
};

// Function to return true or false by checking given length is in range or not
template <typename T_DATA, unsigned int TP_SIG_LEN>
constexpr bool isLenInRange() {
    unsigned int minDataLoad = (MAX_BITS_LOAD_ON_AIE() / aie_CC_Size<T_DATA>());
    bool check_len = 0;

    if ((TP_SIG_LEN >= getMinLen<T_DATA>()) && (TP_SIG_LEN <= getMaxLen<T_DATA>())) {
        check_len = (((TP_SIG_LEN % minDataLoad) == 0) ? 1 : 0);
    }
    return check_len;
};

// Function to return the PaddedLength based on given data length and compute mode
template <typename TT_DATA_F,
          typename TT_DATA_G,
          unsigned int compute_mode,
          unsigned int TP_DATA_LEN_F,
          unsigned int TP_DATA_LEN_G>
INLINE_DECL constexpr unsigned int getPaddedLength() {
    unsigned int PaddedLength = 0;
    unsigned int lanes = aie_CC_NumLanes<TT_DATA_F, TT_DATA_G>();
    unsigned int Data_Samples = aie_CC_Fsample_Size<TT_DATA_F>();
    unsigned int data_Load = MAX_BITS_LOAD_ON_AIE() / Data_Samples;

    if (compute_mode == 0) // Full
    {
        PaddedLength = ((TP_DATA_LEN_G - 1) + TP_DATA_LEN_F + (TP_DATA_LEN_G - 1));
        PaddedLength = CEIL(PaddedLength, data_Load);
    } else if (compute_mode == 1) // Same
    {
        PaddedLength = (((TP_DATA_LEN_G >> 1) - 1) + TP_DATA_LEN_F + ((TP_DATA_LEN_G >> 1) - 1));
        PaddedLength = CEIL(PaddedLength, data_Load);
    } else if (compute_mode == 2) {
        if (lanes > data_Load) // Valid
        {
            PaddedLength = (TP_DATA_LEN_F + (data_Load << 1));
            PaddedLength = CEIL(PaddedLength, data_Load);
        } else {
            PaddedLength = (TP_DATA_LEN_F + data_Load);
            PaddedLength = CEIL(PaddedLength, data_Load);
        }
    } else {
        // do nothing
    }

    return PaddedLength;
};

// Function to return the loopcount based on given data length and compute mode
template <unsigned int compute_mode, unsigned int TP_DATA_LEN_F, unsigned int TP_DATA_LEN_G>
INLINE_DECL constexpr unsigned int getLoopCount() {
    unsigned int LoopCount = 0;
    if (compute_mode == 0) // Full
    {
        LoopCount = ((TP_DATA_LEN_F + TP_DATA_LEN_G) - 1);
    } else if (compute_mode == 1) // Same
    {
        LoopCount = TP_DATA_LEN_F;

    } else if (compute_mode == 2) // Valid
    {
        LoopCount = ((TP_DATA_LEN_F - TP_DATA_LEN_G) + 1);
    } else {
        LoopCount = ((TP_DATA_LEN_F + TP_DATA_LEN_G) - 1); // default Full mode
    }

    return LoopCount;
};

// 8x8 16x8 16x16 32x16 c16x16 c16x32 c16xc16 c32x16 c32xc16
// Configuration Defensive check functions
//----------------------------------------------------------------------

// Check Output data type whether its complex if any input is complex
template <typename TT_DATA_F, typename TT_DATA_G, typename TT_DATA_OUT>
INLINE_DECL constexpr bool fnCheckDataOutType() {
    return false;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<int8, int8, int16>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<int16, int8, int16>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<int16, int16, int32>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<int32, int16, int32>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cint16, int16, cint16>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cint16, int16, cint32>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cint16, int32, cint32>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cint16, cint16, cint32>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cint32, int16, cint32>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cint32, cint16, cint32>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<float, float, float>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<float, cfloat, cfloat>() {
    return false;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cfloat, float, cfloat>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cfloat, cfloat, cfloat>() {
    return true;
};

template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cint16, int16, int8>() {
    return false;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cint16, int16, int16>() {
    return false;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cint16, int16, int32>() {
    return false;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cint16, int16, int64>() {
    return false;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cint16, int32, int8>() {
    return false;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cint16, int32, int16>() {
    return false;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cint16, int32, int32>() {
    return false;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cint16, int32, int64>() {
    return false;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cint16, cint16, int8>() {
    return false;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cint16, cint16, int16>() {
    return false;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cint16, cint16, int32>() {
    return false;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cint16, cint16, int64>() {
    return false;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cint32, int16, int8>() {
    return false;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cint32, int16, int16>() {
    return false;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cint32, cint16, int32>() {
    return false;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cint32, cint16, int64>() {
    return false;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<float, cfloat, float>() {
    return false;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cfloat, float, float>() {
    return false;
};
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<cfloat, cfloat, float>() {
    return false;
};
#ifdef _SUPPORTS_DEVICE_AIE_2_
template <>
INLINE_DECL constexpr bool fnCheckDataOutType<bfloat16, bfloat16, float>() {
    return true;
};
#endif

template <typename TT_DATA_F, typename TT_DATA_G>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs() {
    return false;
};
#ifdef _SUPPORTS_DEVICE_AIE_1_
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<int8, int8>() {
    return false;
};
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<int16, int8>() {
    return false;
};
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<int16, int16>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<int32, int16>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<float, float>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<float, cfloat>() {
    return false;
};
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<cint16, int16>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<cint16, int32>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<cint16, cint16>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<cint32, int16>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<cint32, cint16>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<cfloat, float>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<cfloat, cfloat>() {
    return true;
};
#endif

#ifdef _SUPPORTS_DEVICE_AIE_2_
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<int8, int8>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<int16, int8>() {
    return false;
};
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<int16, int16>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<int32, int16>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<float, float>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<bfloat16, bfloat16>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<cint16, int16>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<cint16, int32>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<cint16, cint16>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<cint32, int16>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<cint32, cint16>() {
    return true;
};
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<cfloat, float>() {
    return false;
};
template <>
INLINE_DECL constexpr bool fnCheckDataTypesOfInputs<cfloat, cfloat>() {
    return false;
};
#endif

// Configuration Defensive check function to check TT_OUT is complex if any one input is complex
template <typename TT_DATA_F, typename TT_DATA_G, typename TT_DATA_OUT>
INLINE_DECL constexpr bool fnCheckDataTypeOfOutput() {
    return (fnCheckDataOutType<TT_DATA_F, TT_DATA_G, TT_DATA_OUT>() ? 1 : 0);
}

// Configuration Defensive check function to check Length of F Signal and G Signal
template <typename TT_DATA, unsigned int TP_SIG_LEN>
INLINE_DECL constexpr bool fnCheckLenOfData() {
    return (isLenInRange<TT_DATA, TP_SIG_LEN>() ? 1 : 0);
}

} // namespace conv_corr {
} // namespace aie {
} // namespace dsp {
} // namespace xf {

#endif // _DSPLIB_CONV_CORR_TRAITS_HPP_