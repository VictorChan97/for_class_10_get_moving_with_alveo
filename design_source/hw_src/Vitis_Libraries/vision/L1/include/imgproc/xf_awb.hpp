/*
 * Copyright (C) 2019-2022, Xilinx, Inc.
 * Copyright (C) 2022-2023, Advanced Micro Devices, Inc.
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

#ifndef _XF_AWB_HPP_
#define _XF_AWB_HPP_

#include "common/xf_common.hpp"
#include "hls_stream.h"

#ifndef XF_IN_STEP
#define XF_IN_STEP 8
#endif
#ifndef XF_OUT_STEP
#define XF_OUT_STEP 8
#endif

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

/**Utility macros and functions**/

template <typename T>
T xf_satcast_awb(int in_val){};

template <>
inline ap_uint<8> xf_satcast_awb<ap_uint<8> >(int v) {
    return (v > 255 ? 255 : v);
};
template <>
inline ap_uint<10> xf_satcast_awb<ap_uint<10> >(int v) {
    return (v > 1023 ? 1023 : v);
};
template <>
inline ap_uint<12> xf_satcast_awb<ap_uint<12> >(int v) {
    return (v > 4095 ? 4095 : v);
};
template <>
inline ap_uint<16> xf_satcast_awb<ap_uint<16> >(int v) {
    return (v > 65535 ? 65535 : v);
};

#include "xf_awb_norm.hpp"

namespace xf {
namespace cv {

template <int SRC_T,
          int DST_T,
          int ROWS,
          int COLS,
          int NPC,
          int XFCVDEPTH_IN_1 = _XFCVDEPTH_DEFAULT,
          int XFCVDEPTH_OUT_1 = _XFCVDEPTH_DEFAULT,
          int PLANES,
          int DEPTH_SRC,
          int DEPTH_DST,
          int WORDWIDTH_SRC,
          int WORDWIDTH_DST,
          int TC>
void AWBGainUpdateKernel(xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1>& src1,
                         xf::cv::Mat<DST_T, ROWS, COLS, NPC, XFCVDEPTH_OUT_1>& dst,
                         float thresh,
                         int i_gain[3]) {
    int width = src1.cols >> XF_BITSHIFT(NPC);
    int height = src1.rows;

    printf("%d %d %d\n", i_gain[0], i_gain[1], i_gain[2]);

    const int STEP = XF_DTPIXELDEPTH(SRC_T, NPC);

    XF_TNAME(SRC_T, NPC) in_pix, out_pix;

    for (int i = 0; i < height; i++) {
// clang-format off
#pragma HLS LOOP_TRIPCOUNT min=ROWS max=ROWS
#pragma HLS LOOP_FLATTEN OFF
    // clang-format on
    ColLoop1:
        for (int j = 0; j < width; j++) {
// clang-format off
#pragma HLS LOOP_TRIPCOUNT min=TC max=TC
#pragma HLS pipeline
            // clang-format on
            in_pix = src1.read(i * width + j);

            for (int p = 0; p < XF_NPIXPERCYCLE(NPC) * PLANES; p++) {
// clang-format off
#pragma HLS unroll
                // clang-format on
                XF_CTUNAME(SRC_T, NPC)
                val = in_pix.range(p * STEP + STEP - 1, p * STEP);
                ap_uint<40> outval = ((val * i_gain[p % 3]) >> STEP);

                out_pix.range(p * STEP + STEP - 1, p * STEP) = xf_satcast_awb<XF_CTUNAME(SRC_T, NPC)>(outval);
            }

            dst.write(i * width + j, out_pix);
        }
    }
}

template <int SRC_T,
          int DST_T,
          int ROWS,
          int COLS,
          int NPC,
          int XFCVDEPTH_IN_1 = _XFCVDEPTH_DEFAULT,
          int XFCVDEPTH_OUT_1 = _XFCVDEPTH_DEFAULT,
          int PLANES,
          int DEPTH_SRC,
          int DEPTH_DST,
          int WORDWIDTH_SRC,
          int WORDWIDTH_DST,
          int TC>
void AWBChannelGainKernel(xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1>& src1,
                          xf::cv::Mat<DST_T, ROWS, COLS, NPC, XFCVDEPTH_OUT_1>& dst,
                          float thresh,
                          int i_gain[3]) {
    int width = src1.cols >> XF_BITSHIFT(NPC);
    int height = src1.rows;

    const int STEP = XF_DTPIXELDEPTH(SRC_T, NPC);
    ap_uint<13> i = 0, j = 0;

    XF_TNAME(SRC_T, NPC) in_pix, out_pix;
    XF_CTUNAME(SRC_T, NPC) r, g, b, b1 = 0, g1 = 0, r1 = 0;

    XF_SNAME(WORDWIDTH_DST) pxl_pack_out;
    XF_SNAME(WORDWIDTH_SRC) pxl_pack1, pxl_pack2;

    int maxval = (1 << (XF_DTPIXELDEPTH(SRC_T, NPC))) - 1; // 65535.0f;

    ap_ufixed<32, 32> thresh255 = ap_ufixed<32, 32>(thresh * maxval);

    int minRGB, maxRGB;

    ap_ufixed<40, 40> tmpsum_vals[(1 << XF_BITSHIFT(NPC)) * PLANES];

    ap_ufixed<40, 40> sum[PLANES];
// clang-format off
#pragma HLS ARRAY_PARTITION variable=tmpsum_vals complete dim=0
#pragma HLS ARRAY_PARTITION variable=sum complete dim=0
    // clang-format on

    for (j = 0; j < ((1 << XF_BITSHIFT(NPC)) * PLANES); j++) {
// clang-format off
#pragma HLS UNROLL
        // clang-format on
        tmpsum_vals[j] = 0;
    }
    for (j = 0; j < PLANES; j++) {
// clang-format off
#pragma HLS UNROLL
        // clang-format on
        sum[j] = 0;
    }

    int p = 0, read_index = 0;

Row_Loop:
    for (i = 0; i < height; i++) {
// clang-format off
#pragma HLS LOOP_TRIPCOUNT min=ROWS max=ROWS
    // clang-format on
    Col_Loop:
        for (j = 0; j < (width); j++) {
// clang-format off
#pragma HLS LOOP_TRIPCOUNT min=COLS/NPC max=COLS/NPC
#pragma HLS pipeline II=1
#pragma HLS LOOP_FLATTEN OFF
            // clang-format on
            XF_TNAME(SRC_T, NPC) in_buf;
            in_buf = src1.read(i * width + j);

            dst.write(i * width + j, in_buf);

        PLANES_LOOP:
            for (int p = 0; p < XF_NPIXPERCYCLE(NPC) * PLANES; p = p + PLANES) {
// clang-format off
#pragma HLS unroll
                // clang-format on
                XF_CTUNAME(SRC_T, NPC)
                val1 = in_buf.range(p * STEP + STEP - 1, p * STEP);
                XF_CTUNAME(SRC_T, NPC)
                val2 = in_buf.range(p * STEP + (2 * STEP) - 1, p * STEP + STEP);
                XF_CTUNAME(SRC_T, NPC)
                val3 = in_buf.range(p * STEP + (3 * STEP) - 1, p * STEP + 2 * STEP);

                minRGB = MIN(val1, MIN(val2, val3));
                maxRGB = MAX(val1, MAX(val2, val3));

                if ((maxRGB - minRGB) * maxval > thresh255 * maxRGB) continue;

                tmpsum_vals[p] = tmpsum_vals[p] + val1;
                tmpsum_vals[(p) + 1] = tmpsum_vals[(p) + 1] + val2;
                tmpsum_vals[(p) + 2] = tmpsum_vals[(p) + 2] + val3;
            }
        }
    }

    for (int c = 0; c < PLANES; c++) {
        for (j = 0; j < (1 << XF_BITSHIFT(NPC)); j++) {
// clang-format off
#pragma HLS UNROLL
            // clang-format on
            sum[c] = (sum[c] + tmpsum_vals[j * PLANES + c]);
        }
    }

    ap_ufixed<40, 40> max_sum_fixed = MAX(sum[0], MAX(sum[1], sum[2]));

    printf("%ld %ld %ld\n", (unsigned long)sum[0], (unsigned long)sum[1], (unsigned long)sum[2]);

    ap_ufixed<40, 2> bval = (float)0.1;
    ap_ufixed<40, 40> zero = 0;

    ap_ufixed<56, STEP> dinB1;
    ap_ufixed<56, STEP> dinG1;
    ap_ufixed<56, STEP> dinR1;

    if (sum[0] < bval) {
        dinB1 = 0;
    } else {
        dinB1 = (ap_ufixed<56, STEP>)((ap_ufixed<56, STEP>)max_sum_fixed / sum[0]);
    }

    if (sum[1] < bval) {
        dinG1 = 0;
    } else {
        dinG1 = (ap_ufixed<56, STEP>)((ap_ufixed<56, STEP>)max_sum_fixed / sum[1]);
    }
    if (sum[2] < bval) {
        dinR1 = 0;
    } else {
        dinR1 = (ap_ufixed<56, STEP>)((ap_ufixed<56, STEP>)max_sum_fixed / sum[2]);
    }

    ap_ufixed<56, STEP> gain_max1 = MAX(dinB1, MAX(dinG1, dinR1));

    if (gain_max1 > 0) {
        dinB1 /= gain_max1;
        dinG1 /= gain_max1;
        dinR1 /= gain_max1;
    }

    float a1 = dinB1;
    float a2 = dinG1;
    float a3 = dinR1;

    printf("division values : %f %f %f\n", a1, a2, a3);

    // int i_gain[3] = {0, 0, 0};
    i_gain[0] = (dinB1 * (1 << STEP));
    i_gain[1] = (dinG1 * (1 << STEP));
    i_gain[2] = (dinR1 * (1 << STEP));
}

template <int SRC_T,
          int DST_T,
          int ROWS,
          int COLS,
          int NPC = 1,
          int XFCVDEPTH_IN_1 = _XFCVDEPTH_DEFAULT,
          int XFCVDEPTH_OUT_1 = _XFCVDEPTH_DEFAULT,
          int DEPTH_SRC,
          int WB_TYPE,
          int HISTSIZE>
void AWBNormalizationkernel(xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1>& src,
                            xf::cv::Mat<DST_T, ROWS, COLS, NPC, XFCVDEPTH_OUT_1>& dst,
                            float inputMax,
                            float min_Value[3],
                            float max_Value[3]) {
    // float min_Value[3], invValue[3];
    // clang-format off
	AWBNormalizationkernel_post<SRC_T, DST_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1, XFCVDEPTH_OUT_1, HISTSIZE>(src, dst, inputMax, min_Value, max_Value);
}

template <int SRC_T,
          int DST_T,
          int ROWS,
          int COLS,
          int NPC = 1,
          int XFCVDEPTH_IN_1 = _XFCVDEPTH_DEFAULT,
          int XFCVDEPTH_OUT_1 = _XFCVDEPTH_DEFAULT,
          int DEPTH_SRC,
          int WB_TYPE,
          int HISTSIZE>
void AWBNormalizationkernel_multi(xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1>& src,
                                  xf::cv::Mat<DST_T, ROWS, COLS, NPC, XFCVDEPTH_OUT_1>& dst,
                                  uint32_t hist[3][HISTSIZE],
                                  float p,
                                  float inputMin,
                                  float inputMax,
                                  float outputMin,
                                  float outputMax,
                                  unsigned short org_height) {
// clang-format off
#pragma HLS INLINE OFF
    // clang-format on
    short width = dst.cols >> XF_BITSHIFT(NPC);
    short height = dst.rows;
    const int STEP = XF_DTPIXELDEPTH(SRC_T, NPC);

    ap_uint<STEP + 1> bins = HISTSIZE; // number of bins at each histogram level

    ap_uint<STEP + 1> nElements = HISTSIZE; // int(pow((float)bins, (float)depth));

    int total = dst.cols * org_height;

    ap_fixed<STEP + 8, STEP + 2> min_vals = inputMin - 0.5f;

    ap_fixed<STEP + 8, STEP + 2> max_vals = inputMax + 0.5f;

    ap_fixed<STEP + 8, STEP + 2> minValue[3] = {min_vals, min_vals, min_vals}; //{-0.5, -0.5, -0.5};
    ap_fixed<STEP + 8, STEP + 2> maxValue[3] = {max_vals, max_vals, max_vals}; //{12287.5, 16383.5, 12287.5};
    ap_fixed<STEP + 8, 4> s1 = p;
    ap_fixed<STEP + 8, 4> s2 = p;

    int rval = s1 * total / 100;

    int rval1 = (100 - s2) * total / 100;

// clang-format off
#pragma HLS ARRAY_PARTITION variable = minValue complete dim = 0
#pragma HLS ARRAY_PARTITION variable = maxValue complete dim = 0
    // clang-format on

    for (int j = 0; j < 3; ++j)
    // searching for s1 and s2
    {
        ap_uint<STEP + 2> p1 = 0;
        ap_uint<STEP + 2> p2 = bins - 1;
        ap_uint<32> n1 = 0;
        ap_uint<32> n2 = total;

        ap_fixed<STEP + 8, STEP + 2> interval = (max_vals - min_vals) / bins;

        for (int k = 0; k < 1; ++k)
        // searching for s1 and s2
        {
            int value = hist[j][p1];
            int value1 = hist[j][p2];

            while (n1 + hist[j][p1] < rval && p1 < HISTSIZE) {
#pragma HLS PIPELINE
#pragma HLS LOOP_TRIPCOUNT min = 255 max = 255
#pragma HLS DEPENDENCE variable = hist array intra false
#pragma HLS DEPENDENCE variable = minValue intra false
                n1 += hist[j][p1++];
                minValue[j] += interval;
            }

            while (n2 - hist[j][p2] > rval1 && p2 != 0) {
#pragma HLS PIPELINE
#pragma HLS LOOP_TRIPCOUNT min = 255 max = 255
#pragma HLS DEPENDENCE variable = hist array intra false
#pragma HLS DEPENDENCE variable = maxValue intra false
                n2 -= hist[j][p2--];
                maxValue[j] -= interval;
            }
        }
    }

    ap_fixed<STEP + 8, STEP + 2> maxmin_diff[3];
// clang-format off
#pragma HLS ARRAY_PARTITION variable = maxmin_diff complete dim = 0
    // clang-format on

    ap_fixed<STEP + 8, STEP + 2> newmax = inputMax;
    ap_fixed<STEP + 8, STEP + 2> newmin = 0.0f;
    maxmin_diff[0] = maxValue[0] - minValue[0];

    maxmin_diff[1] = maxValue[1] - minValue[1];

    maxmin_diff[2] = maxValue[2] - minValue[2];

    ap_fixed<STEP + 8, STEP + 2> newdiff = newmax - newmin;

    XF_TNAME(SRC_T, NPC) in_buf_n, in_buf_n1, out_buf_n;

    printf("valuesmin max :%f %f %f %f %f %f\n", (float)maxValue[0], (float)maxValue[1], (float)maxValue[2],
           (float)minValue[0], (float)minValue[1], (float)minValue[2]);
    int pval = 0, read_index = 0, write_index = 0;
    ap_uint<13> row, col;

    ap_fixed<STEP + 16, 2> inv_val[3];

    if (maxmin_diff[0] != 0) inv_val[0] = ((ap_fixed<STEP + 16, 2>)1 / maxmin_diff[0]);
    if (maxmin_diff[1] != 0) inv_val[1] = ((ap_fixed<STEP + 16, 2>)1 / maxmin_diff[1]);
    if (maxmin_diff[2] != 0) inv_val[2] = ((ap_fixed<STEP + 16, 2>)1 / maxmin_diff[2]);

Row_Loop1:
    for (row = 0; row < height; row++) {
// clang-format off
#pragma HLS LOOP_TRIPCOUNT min=ROWS max=ROWS
    // clang-format on

    Col_Loop1:
        for (col = 0; col < width; col++) {
// clang-format off
#pragma HLS LOOP_TRIPCOUNT min=COLS/NPC max=COLS/NPC
#pragma HLS pipeline II=1
#pragma HLS LOOP_FLATTEN OFF
            // clang-format on
            in_buf_n = src.read(read_index++);

            ap_fixed<STEP + 8, STEP + 2> value = 0;
            ap_fixed<STEP + STEP + 16, STEP + 8> divval = 0;
            ap_fixed<STEP + 16, STEP + 16> finalmul = 0;
            ap_int<32> dstval;

            for (int p = 0, bit = 0; p < XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC); p++, bit = p % 3) {
// clang-format off
#pragma HLS unroll
                // clang-format on
                XF_CTUNAME(SRC_T, NPC)
                val = in_buf_n.range(p * STEP + STEP - 1, p * STEP);
                value = val - minValue[bit];
                divval = value * inv_val[p % 3];
                finalmul = divval * newdiff;
                dstval = (int)(finalmul + newmin);
                if (dstval.range(31, 31) == 1) {
                    dstval = 0;
                }
                out_buf_n.range(p * STEP + STEP - 1, p * STEP) = xf_satcast_awb<XF_CTUNAME(SRC_T, NPC)>(dstval);
            }

            dst.write(row * width + col, out_buf_n);
        }
    }
}

//////////////

template <int SRC_T,
          int DST_T,
          int ROWS,
          int COLS,
          int NPC = 1,
          int XFCVDEPTH_IN_1 = _XFCVDEPTH_DEFAULT,
          int XFCVDEPTH_IN_2 = _XFCVDEPTH_DEFAULT,
          int DEPTH_SRC,
          int WB_TYPE,
          int HISTSIZE>
void AWBhistogramkernel(xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1>& src1,
                        xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_2>& src2,
                        uint32_t hist[3][HISTSIZE],
                        float p,
                        float inputMin,
                        float inputMax,
                        float outputMin,
                        float outputMax) {
// clang-format off
#pragma HLS INLINE OFF
    // clang-format on

    const int STEP = XF_DTPIXELDEPTH(SRC_T, NPC);

    int width = src1.cols >> XF_BITSHIFT(NPC);
    int height = src1.rows;

    XF_TNAME(SRC_T, NPC) in_pix, in_pix1, out_pix;
    int writenct = 0;

    //******************** Simple white balance ********************

    int depth = 3; // depth of histogram tree

    int bins = HISTSIZE; // number of bins at each histogram level

    int nElements = HISTSIZE; // int(pow((float)bins, (float)depth));

    int val[3];

// histogram initialization

INITIALIZE_HIST:
    for (int k = 0; k < HISTSIZE; k++) {
// clang-format off
#pragma HLS PIPELINE
#pragma HLS LOOP_TRIPCOUNT min=HISTSIZE max=HISTSIZE
    // clang-format on
    INITIALIZE:
        for (int hi = 0; hi < 3; hi++) {
// clang-format off
#pragma HLS UNROLL
            // clang-format on
            hist[hi][k] = 0;
        }
    }

    // Temporary array used while computing histogram
    ap_uint<32> tmp_hist[XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC)][HISTSIZE];
    ap_uint<32> tmp_hist1[XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC)][HISTSIZE];
// clang-format off
#pragma HLS bind_storage variable=tmp_hist type=RAM_T2P impl=BRAM
#pragma HLS bind_storage variable=tmp_hist1 type=RAM_T2P impl=BRAM
#pragma HLS ARRAY_PARTITION variable=tmp_hist complete dim=1
#pragma HLS ARRAY_PARTITION variable=tmp_hist1 complete dim=1
    // clang-format on
    XF_TNAME(SRC_T, NPC) in_buf, in_buf1, temp_buf;

    bool flag = 0;

HIST_INITIALIZE_LOOP:
    for (ap_uint<32> i = 0; i < HISTSIZE; i++) //
    {
// clang-format off
#pragma HLS PIPELINE
        // clang-format on
        for (ap_uint<5> j = 0; j < XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC); j++) {
// clang-format off
#pragma HLS LOOP_TRIPCOUNT min=HISTSIZE max=HISTSIZE
            // clang-format on
            tmp_hist[j][i] = 0;
            tmp_hist1[j][i] = 0;
        }
    }

    static uint32_t old[XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC)] = {};
    static uint32_t old1[XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC)] = {};
    static uint32_t acc[XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC)] = {};
    static uint32_t acc1[XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC)] = {};
#pragma HLS ARRAY_PARTITION variable = old complete
#pragma HLS ARRAY_PARTITION variable = old1 complete
#pragma HLS ARRAY_PARTITION variable = acc complete
#pragma HLS ARRAY_PARTITION variable = acc1 complete

    int readcnt = 0;
    ap_fixed<STEP + 8, STEP + 2> min_vals = inputMin - 0.5f;
    ap_fixed<STEP + 8, STEP + 2> max_vals = inputMax + 0.5f;

    ap_fixed<STEP + 8, STEP + 2> minValue = min_vals, minValue1 = min_vals;
    ap_fixed<STEP + 8, STEP + 2> maxValue = max_vals, maxValue1 = max_vals;

    ap_fixed<STEP + 8, STEP + 2> interval = ap_fixed<STEP + 8, STEP + 2>(maxValue - minValue) / bins;

    ap_fixed<STEP + 8, 2> internal_inv = ((ap_fixed<STEP + 8, 2>)1 / interval);
    int pos = 0, pos1 = 0;
    int currentBin = 0, currentBin1 = 0;
ROW_LOOP:
    for (int row = 0; row != (height); row++) // histogram filling
    {
// clang-format off
#pragma HLS LOOP_TRIPCOUNT min=1 max=ROWS
    // clang-format on
    COL_LOOP:
        for (int col = 0; col < (width); col = col + 2) // histogram filling
        {
// clang-format off
#pragma HLS PIPELINE II=2
#pragma HLS LOOP_FLATTEN OFF
#pragma HLS LOOP_TRIPCOUNT min=1 max=COLS/4
            // clang-format on

            in_pix = src1.read(row * (width) + col);
            in_pix1 = src1.read((row * (width) + col) + 1);

            src2.write(row * (width) + col, in_pix);
            src2.write(row * (width) + col + 1, in_pix1);

        PLANES_LOOP:
            for (ap_uint<9> j = 0; j < XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC); j++) {
// clang-format off
#pragma HLS DEPENDENCE variable=tmp_hist array intra false
#pragma HLS DEPENDENCE variable=tmp_hist1 array intra false
#pragma HLS UNROLL
                // clang-format on

                XF_CTUNAME(SRC_T, NPC) val = 0, val1 = 0;
                val = in_pix.range(j * STEP + STEP - 1, j * STEP);
                val1 = in_pix1.range(j * STEP + STEP - 1, j * STEP);

                currentBin = int((val - minValue) * internal_inv);
                currentBin1 = int((val1 - minValue1) * internal_inv);

                uint32_t tmp = old[j];
                uint32_t tmp1 = old1[j];
                uint32_t tmp_acc = acc[j];
                uint32_t tmp_acc1 = acc1[j];
                if (tmp == currentBin) {
                    tmp_acc = tmp_acc + 1;
                } else {
                    tmp_hist[j][tmp] = tmp_acc;
                    tmp_acc = tmp_hist[j][currentBin] + 1;
                }
                if (tmp1 == currentBin1) {
                    tmp_acc1 = tmp_acc1 + 1;
                } else {
                    tmp_hist1[j][tmp1] = tmp_acc1;
                    tmp_acc1 = tmp_hist1[j][currentBin1] + 1;
                }
                old[j] = currentBin;
                old1[j] = currentBin1;
                acc[j] = tmp_acc;
                acc1[j] = tmp_acc1;
            }
        }
    }
END_HIST_LOOP:
    for (ap_uint<5> ch_ppc = 0; ch_ppc < (XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC)); ch_ppc++) {
// clang-format off
        #pragma HLS LOOP_TRIPCOUNT min=1 max=6
		#pragma HLS UNROLL
        // clang-format on
        uint32_t tmp = old[ch_ppc];
        uint32_t tmp1 = old1[ch_ppc];
        tmp_hist[ch_ppc][tmp] = acc[ch_ppc];
        tmp_hist1[ch_ppc][tmp1] = acc1[ch_ppc];
    }

    //	Now merge computed partial histograms
    const int num_ch = XF_CHANNELS(SRC_T, NPC);

MERGE_HIST_LOOP:
    for (ap_uint<32> i = 0; i < HISTSIZE; i++) {
// clang-format off
#pragma HLS pipeline
    // clang-format on

    MERGE_HIST_CH_UNROLL:
        for (ap_uint<5> ch = 0; ch < num_ch; ch++) {
// clang-format off
#pragma HLS UNROLL
            // clang-format on

            uint32_t value = 0;

        MERGE_HIST_NPPC_UNROLL:
            for (ap_uint<5> p = 0; p < XF_NPIXPERCYCLE(NPC); p++) {
// clang-format off
#pragma HLS UNROLL
                // clang-format on
                value += tmp_hist[p * num_ch + ch][i] + tmp_hist1[p * num_ch + ch][i];
            }

            hist[ch][i] = value;
        }
    }
}

template <int SRC_T,
          int DST_T,
          int ROWS,
          int COLS,
          int NPC = 1,
          int XFCVDEPTH_IN_1 = _XFCVDEPTH_DEFAULT,
          int XFCVDEPTH_IN_2 = _XFCVDEPTH_DEFAULT,
          int DEPTH_SRC,
          int WB_TYPE,
          int HISTSIZE>
void AWBhistogramkernel_multi(xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1>& src1,
                              xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_2>& src2,
                              uint32_t hist[3][HISTSIZE],
                              float p,
                              float inputMin,
                              float inputMax,
                              float outputMin,
                              float outputMax,
                              int slc_id) {
// clang-format off
#pragma HLS INLINE OFF
    // clang-format on

    const int STEP = XF_DTPIXELDEPTH(SRC_T, NPC);

    int width = src1.cols >> XF_BITSHIFT(NPC);
    int height = src1.rows;

    XF_TNAME(SRC_T, NPC) in_pix, in_pix1, out_pix;
    int writenct = 0;

    //******************** Simple white balance ********************

    int depth = 3; // depth of histogram tree

    int bins = HISTSIZE; // number of bins at each histogram level

    int nElements = HISTSIZE; // int(pow((float)bins, (float)depth));

    int val[3];

    if (slc_id == 0) {
    INITIALIZE_HIST:
        for (int k = 0; k < HISTSIZE; k++) {
// clang-format off
#pragma HLS PIPELINE
#pragma HLS LOOP_TRIPCOUNT min=HISTSIZE max=HISTSIZE
        // clang-format on
        INITIALIZE:
            for (int hi = 0; hi < 3; hi++) {
// clang-format off
#pragma HLS UNROLL
                // clang-format on
                hist[hi][k] = 0;
            }
        }
    }
    // Temporary array used while computing histogram
    ap_uint<32> tmp_hist[XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC)][HISTSIZE];
    ap_uint<32> tmp_hist1[XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC)][HISTSIZE];
// clang-format off
#pragma HLS bind_storage variable=tmp_hist type=RAM_T2P impl=BRAM
#pragma HLS bind_storage variable=tmp_hist1 type=RAM_T2P impl=BRAM
#pragma HLS ARRAY_PARTITION variable=tmp_hist complete dim=1
#pragma HLS ARRAY_PARTITION variable=tmp_hist1 complete dim=1
    // clang-format on
    XF_TNAME(SRC_T, NPC) in_buf, in_buf1, temp_buf;

    bool flag = 0;

HIST_INITIALIZE_LOOP:
    for (ap_uint<32> i = 0; i < HISTSIZE; i++) //
    {
// clang-format off
#pragma HLS PIPELINE
        // clang-format on
        for (ap_uint<5> j = 0; j < XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC); j++) {
// clang-format off
#pragma HLS LOOP_TRIPCOUNT min=HISTSIZE max=HISTSIZE
            // clang-format on
            tmp_hist[j][i] = 0;
            tmp_hist1[j][i] = 0;
        }
    }

    uint32_t old[XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC)] = {};
    uint32_t old1[XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC)] = {};
    uint32_t acc[XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC)] = {};
    uint32_t acc1[XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC)] = {};
#pragma HLS ARRAY_PARTITION variable = old complete
#pragma HLS ARRAY_PARTITION variable = old1 complete
#pragma HLS ARRAY_PARTITION variable = acc complete
#pragma HLS ARRAY_PARTITION variable = acc1 complete

    int readcnt = 0;
    ap_fixed<STEP + 8, STEP + 2> min_vals = inputMin - 0.5f;
    ap_fixed<STEP + 8, STEP + 2> max_vals = inputMax + 0.5f;

    ap_fixed<STEP + 8, STEP + 2> minValue = min_vals, minValue1 = min_vals;
    ap_fixed<STEP + 8, STEP + 2> maxValue = max_vals, maxValue1 = max_vals;

    ap_fixed<STEP + 8, STEP + 2> interval = ap_fixed<STEP + 8, STEP + 2>(maxValue - minValue) / bins;

    ap_fixed<STEP + 8, 2> internal_inv = ((ap_fixed<STEP + 8, 2>)1 / interval);
    int pos = 0, pos1 = 0;
    int currentBin = 0, currentBin1 = 0;
ROW_LOOP:
    for (int row = 0; row != (height); row++) // histogram filling
    {
// clang-format off
#pragma HLS LOOP_TRIPCOUNT min=1 max=ROWS
    // clang-format on
    COL_LOOP:
        for (int col = 0; col < (width); col = col + 2) // histogram filling
        {
// clang-format off
#pragma HLS PIPELINE II=2
#pragma HLS LOOP_FLATTEN OFF
#pragma HLS LOOP_TRIPCOUNT min=1 max=COLS/4
            // clang-format on

            in_pix = src1.read(row * (width) + col);
            in_pix1 = src1.read((row * (width) + col) + 1);

            src2.write(row * (width) + col, in_pix);
            src2.write(row * (width) + col + 1, in_pix1);

        PLANES_LOOP:
            for (ap_uint<9> j = 0; j < XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC); j++) {
// clang-format off
#pragma HLS DEPENDENCE variable=tmp_hist array intra false
#pragma HLS DEPENDENCE variable=tmp_hist1 array intra false
#pragma HLS UNROLL
                // clang-format on

                XF_CTUNAME(SRC_T, NPC) val = 0, val1 = 0;
                val = in_pix.range(j * STEP + STEP - 1, j * STEP);
                val1 = in_pix1.range(j * STEP + STEP - 1, j * STEP);

                currentBin = int((val - minValue) * internal_inv);
                currentBin1 = int((val1 - minValue1) * internal_inv);

                uint32_t tmp = old[j];
                uint32_t tmp1 = old1[j];
                uint32_t tmp_acc = acc[j];
                uint32_t tmp_acc1 = acc1[j];
                if (tmp == currentBin) {
                    tmp_acc = tmp_acc + 1;
                } else {
                    tmp_hist[j][tmp] = tmp_acc;
                    tmp_acc = tmp_hist[j][currentBin] + 1;
                }
                if (tmp1 == currentBin1) {
                    tmp_acc1 = tmp_acc1 + 1;
                } else {
                    tmp_hist1[j][tmp1] = tmp_acc1;
                    tmp_acc1 = tmp_hist1[j][currentBin1] + 1;
                }
                old[j] = currentBin;
                old1[j] = currentBin1;
                acc[j] = tmp_acc;
                acc1[j] = tmp_acc1;
            }
        }
    }
END_HIST_LOOP:
    for (ap_uint<5> ch_ppc = 0; ch_ppc < (XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC)); ch_ppc++) {
// clang-format off
        #pragma HLS LOOP_TRIPCOUNT min=1 max=6
		#pragma HLS UNROLL
        // clang-format on
        uint32_t tmp = old[ch_ppc];
        uint32_t tmp1 = old1[ch_ppc];
        tmp_hist[ch_ppc][tmp] = acc[ch_ppc];
        tmp_hist1[ch_ppc][tmp1] = acc1[ch_ppc];
    }

    //	Now merge computed partial histograms
    const int num_ch = XF_CHANNELS(SRC_T, NPC);

MERGE_HIST_LOOP:
    for (ap_uint<32> i = 0; i < HISTSIZE; i++) {
// clang-format off
#pragma HLS pipeline
    // clang-format on

    MERGE_HIST_CH_UNROLL:
        for (ap_uint<5> ch = 0; ch < num_ch; ch++) {
// clang-format off
#pragma HLS UNROLL
            // clang-format on

            uint32_t value = 0;

        MERGE_HIST_NPPC_UNROLL:
            for (ap_uint<5> p = 0; p < XF_NPIXPERCYCLE(NPC); p++) {
// clang-format off
#pragma HLS UNROLL
                // clang-format on
                value += tmp_hist[p * num_ch + ch][i] + tmp_hist1[p * num_ch + ch][i];
            }

            hist[ch][i] += value;
        }
    }
}
template <int SRC_T,
          int DST_T,
          int ROWS,
          int COLS,
          int NPC = 1,
          int USE_URAM = 0,
          int XFCVDEPTH_IN_1 = _XFCVDEPTH_DEFAULT,
          int XFCVDEPTH_IN_2 = _XFCVDEPTH_DEFAULT,
          int DEPTH_SRC,
          int WB_TYPE,
          int HISTSIZE>
void AWBhistogramkernel_imp(xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1>& src1,
                            //                            xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_2>& src2,
                            uint32_t hist[3][HISTSIZE],
                            float p,
                            float inputMin,
                            float inputMax,
                            float outputMin,
                            float outputMax) {
// clang-format off
#pragma HLS INLINE OFF
    // clang-format on

    const int STEP = XF_DTPIXELDEPTH(SRC_T, NPC);

    int width = src1.cols >> XF_BITSHIFT(NPC);
    int height = src1.rows;

    XF_TNAME(SRC_T, NPC) in_pix, in_pix1, out_pix;
    int writenct = 0;

    //******************** Simple white balance ********************

    int depth = 3; // depth of histogram tree

    int bins = HISTSIZE; // number of bins at each histogram level

    int nElements = HISTSIZE; // int(pow((float)bins, (float)depth));

    int val[3];

// histogram initialization

INITIALIZE_HIST:
    for (int k = 0; k < HISTSIZE; k++) {
// clang-format off
#pragma HLS PIPELINE
#pragma HLS LOOP_TRIPCOUNT min=HISTSIZE max=HISTSIZE
    // clang-format on
    INITIALIZE:
        for (int hi = 0; hi < 3; hi++) {
// clang-format off
#pragma HLS UNROLL
            // clang-format on
            hist[hi][k] = 0;
        }
    }

    // Temporary array used while computing histogram
    ap_uint<32> tmp_hist[XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC)][HISTSIZE];
    // clang-format off

if(USE_URAM){
    #pragma HLS bind_storage variable=tmp_hist type=RAM_T2P impl=URAM
    #pragma HLS ARRAY_PARTITION variable=tmp_hist complete dim=1
}else{
    #pragma HLS bind_storage variable=tmp_hist type=RAM_T2P impl=BRAM
    #pragma HLS ARRAY_PARTITION variable=tmp_hist complete dim=1
}
    // clang-format on
    XF_TNAME(SRC_T, NPC) in_buf, in_buf1, temp_buf;

    bool flag = 0;

HIST_INITIALIZE_LOOP:
    for (ap_uint<32> i = 0; i < HISTSIZE; i++) //
    {
// clang-format off
#pragma HLS PIPELINE
        // clang-format on
        for (ap_uint<5> j = 0; j < XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC); j++) {
// clang-format off
#pragma HLS LOOP_TRIPCOUNT min=HISTSIZE max=HISTSIZE
            // clang-format on
            tmp_hist[j][i] = 0;
        }
    }

    static uint32_t old[XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC)] = {};
    uint32_t acc_rd[XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC)] = {};
    uint32_t acc_wr[XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC)] = {};
#pragma HLS ARRAY_PARTITION variable = old complete
#pragma HLS ARRAY_PARTITION variable = acc_rd complete
#pragma HLS ARRAY_PARTITION variable = acc_wr complete

    int readcnt = 0;
    ap_fixed<STEP + 8, STEP + 2> min_vals = inputMin - 0.5f;
    ap_fixed<STEP + 8, STEP + 2> max_vals = inputMax + 0.5f;

    ap_fixed<STEP + 8, STEP + 2> minValue = min_vals, minValue1 = min_vals;
    ap_fixed<STEP + 8, STEP + 2> maxValue = max_vals, maxValue1 = max_vals;

    ap_fixed<STEP + 8, STEP + 2> interval = ap_fixed<STEP + 8, STEP + 2>(maxValue - minValue) / bins;

    ap_fixed<STEP + 8, 2> internal_inv = ((ap_fixed<STEP + 8, 2>)1 / interval);

    int pos = 0, pos1 = 0;
    int currentBin = 0; //, currentBin1 = 0;

ROW_LOOP:
    for (int row = 0; row != (height); row++) // histogram filling
    {
// clang-format off
#pragma HLS LOOP_TRIPCOUNT min=1 max=ROWS
    // clang-format on
    COL_LOOP:
        for (int col = 0; col < (width); col = col + 1) // histogram filling
        {
// clang-format off
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_FLATTEN OFF
#pragma HLS LOOP_TRIPCOUNT min=1 max=COLS
            // clang-format on

            in_pix = src1.read(row * (width) + col);

        //            src2.write(row * (width) + col, in_pix);

        PLANES_LOOP:
            for (ap_uint<9> j = 0; j < XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC); j++) {
// clang-format off
#pragma HLS DEPENDENCE variable=tmp_hist array intra false
#pragma HLS UNROLL
                // clang-format on

                XF_CTUNAME(SRC_T, NPC) val = 0;
                val = in_pix.range(j * STEP + STEP - 1, j * STEP);

                currentBin = int((val - minValue) * internal_inv);

                if (currentBin == old[j]) {
                    acc_rd[j] = acc_wr[j];
                } else {
                    acc_rd[j] = tmp_hist[j][currentBin];
                }

                tmp_hist[j][old[j]] = acc_wr[j];

                acc_wr[j] = acc_rd[j] + 1;
                old[j] = currentBin;
            }
        }
    }
END_HIST_LOOP:
    for (ap_uint<5> ch_ppc = 0; ch_ppc < (XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC)); ch_ppc++) {
// clang-format off
        #pragma HLS LOOP_TRIPCOUNT min=1 max=6
		#pragma HLS UNROLL
        // clang-format on
        uint32_t tmp = old[ch_ppc];
        tmp_hist[ch_ppc][tmp] = acc_wr[ch_ppc];
    }

    //	Now merge computed partial histograms
    const int num_ch = XF_CHANNELS(SRC_T, NPC);

MERGE_HIST_LOOP:
    for (ap_uint<32> i = 0; i < HISTSIZE; i++) {
// clang-format off
#pragma HLS pipeline
    // clang-format on
    MERGE_HIST_CH_UNROLL:
        for (ap_uint<5> ch = 0; ch < num_ch; ch++) {
// clang-format off
#pragma HLS UNROLL
            // clang-format on

            uint32_t value = 0;

        MERGE_HIST_NPPC_UNROLL:
            for (ap_uint<5> p = 0; p < XF_NPIXPERCYCLE(NPC); p++) {
// clang-format off
#pragma HLS UNROLL
                // clang-format on
                value += tmp_hist[p * num_ch + ch][i];
            }

            hist[ch][i] = value;
        }
    }
}

template <int SRC_T,
          int DST_T,
          int ROWS,
          int COLS,
          int NPC = 1,
          int XFCVDEPTH_IN_1 = _XFCVDEPTH_DEFAULT,
          int XFCVDEPTH_IN_2 = _XFCVDEPTH_DEFAULT,
          int DEPTH_SRC,
          int WB_TYPE,
          int HISTSIZE>
void AWBhistogramkernel_imp_multi(xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1>& src1,
                                  xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_2>& src2,
                                  uint32_t hist[3][HISTSIZE],
                                  float p,
                                  float inputMin,
                                  float inputMax,
                                  float outputMin,
                                  float outputMax,
                                  int slc_id) {
// clang-format off
#pragma HLS INLINE OFF
    // clang-format on

    const int STEP = XF_DTPIXELDEPTH(SRC_T, NPC);

    int width = src1.cols >> XF_BITSHIFT(NPC);
    int height = src1.rows;

    XF_TNAME(SRC_T, NPC) in_pix, in_pix1, out_pix;
    int writenct = 0;

    //******************** Simple white balance ********************

    int depth = 3; // depth of histogram tree

    int bins = HISTSIZE; // number of bins at each histogram level

    int nElements = HISTSIZE; // int(pow((float)bins, (float)depth));

    int val[3];

    // histogram initialization
    if (slc_id == 0) {
    INITIALIZE_HIST:
        for (int k = 0; k < HISTSIZE; k++) {
// clang-format off
#pragma HLS PIPELINE
#pragma HLS LOOP_TRIPCOUNT min=HISTSIZE max=HISTSIZE
        // clang-format on
        INITIALIZE:
            for (int hi = 0; hi < 3; hi++) {
// clang-format off
#pragma HLS UNROLL
                // clang-format on
                hist[hi][k] = 0;
            }
        }
    }
    // Temporary array used while computing histogram
    ap_uint<32> tmp_hist[XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC)][HISTSIZE];
// clang-format off
#pragma HLS bind_storage variable=tmp_hist type=RAM_T2P impl=BRAM
#pragma HLS ARRAY_PARTITION variable=tmp_hist complete dim=1
    // clang-format on
    XF_TNAME(SRC_T, NPC) in_buf, in_buf1, temp_buf;

    bool flag = 0;

HIST_INITIALIZE_LOOP:
    for (ap_uint<32> i = 0; i < HISTSIZE; i++) //
    {
// clang-format off
#pragma HLS PIPELINE
        // clang-format on
        for (ap_uint<5> j = 0; j < XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC); j++) {
// clang-format off
#pragma HLS LOOP_TRIPCOUNT min=HISTSIZE max=HISTSIZE
            // clang-format on
            tmp_hist[j][i] = 0;
        }
    }

    static uint32_t old[XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC)] = {};
    uint32_t acc_rd[XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC)] = {};
    uint32_t acc_wr[XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC)] = {};
#pragma HLS ARRAY_PARTITION variable = old complete
#pragma HLS ARRAY_PARTITION variable = acc_rd complete
#pragma HLS ARRAY_PARTITION variable = acc_wr complete

    int readcnt = 0;
    ap_fixed<STEP + 8, STEP + 2> min_vals = inputMin - 0.5f;
    ap_fixed<STEP + 8, STEP + 2> max_vals = inputMax + 0.5f;

    ap_fixed<STEP + 8, STEP + 2> minValue = min_vals, minValue1 = min_vals;
    ap_fixed<STEP + 8, STEP + 2> maxValue = max_vals, maxValue1 = max_vals;

    ap_fixed<STEP + 8, STEP + 2> interval = ap_fixed<STEP + 8, STEP + 2>(maxValue - minValue) / bins;

    ap_fixed<STEP + 8, 2> internal_inv = ((ap_fixed<STEP + 8, 2>)1 / interval);

    int pos = 0, pos1 = 0;
    int currentBin = 0; //, currentBin1 = 0;

ROW_LOOP:
    for (int row = 0; row != (height); row++) // histogram filling
    {
// clang-format off
#pragma HLS LOOP_TRIPCOUNT min=1 max=ROWS
    // clang-format on
    COL_LOOP:
        for (int col = 0; col < (width); col = col + 1) // histogram filling
        {
// clang-format off
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_FLATTEN OFF
#pragma HLS LOOP_TRIPCOUNT min=1 max=COLS
            // clang-format on

            in_pix = src1.read(row * (width) + col);

            src2.write(row * (width) + col, in_pix);

        PLANES_LOOP:
            for (ap_uint<9> j = 0; j < XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC); j++) {
// clang-format off
#pragma HLS DEPENDENCE variable=tmp_hist array intra false
#pragma HLS UNROLL
                // clang-format on

                XF_CTUNAME(SRC_T, NPC) val = 0;
                val = in_pix.range(j * STEP + STEP - 1, j * STEP);

                currentBin = int((val - minValue) * internal_inv);

                if (currentBin == old[j]) {
                    acc_rd[j] = acc_wr[j];
                } else {
                    acc_rd[j] = tmp_hist[j][currentBin];
                }

                tmp_hist[j][old[j]] = acc_wr[j];

                acc_wr[j] = acc_rd[j] + 1;
                old[j] = currentBin;
            }
        }
    }
END_HIST_LOOP:
    for (ap_uint<5> ch_ppc = 0; ch_ppc < (XF_NPIXPERCYCLE(NPC) * XF_CHANNELS(SRC_T, NPC)); ch_ppc++) {
// clang-format off
        #pragma HLS LOOP_TRIPCOUNT min=1 max=6
		#pragma HLS UNROLL
        // clang-format on
        uint32_t tmp = old[ch_ppc];
        tmp_hist[ch_ppc][tmp] = acc_wr[ch_ppc];
    }

    //	Now merge computed partial histograms
    const int num_ch = XF_CHANNELS(SRC_T, NPC);

MERGE_HIST_LOOP:
    for (ap_uint<32> i = 0; i < HISTSIZE; i++) {
// clang-format off
#pragma HLS pipeline
    // clang-format on
    MERGE_HIST_CH_UNROLL:
        for (ap_uint<5> ch = 0; ch < num_ch; ch++) {
// clang-format off
#pragma HLS UNROLL
            // clang-format on

            uint32_t value = 0;

        MERGE_HIST_NPPC_UNROLL:
            for (ap_uint<5> p = 0; p < XF_NPIXPERCYCLE(NPC); p++) {
// clang-format off
#pragma HLS UNROLL
                // clang-format on
                value += tmp_hist[p * num_ch + ch][i];
            }

            hist[ch][i] += value;
        }
    }
}
template <int SRC_T,
          int DST_T,
          int ROWS,
          int COLS,
          int NPC = 1,
          int USE_URAM = 0,
          int WB_TYPE,
          int HISTSIZE,
          int XFCVDEPTH_IN_1 = _XFCVDEPTH_DEFAULT,
          int XFCVDEPTH_OUT_1 = _XFCVDEPTH_DEFAULT>
void AWBhistogram(xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1>& src1,
                  uint32_t histogram0[3][HISTSIZE],
                  uint32_t histogram1[3][HISTSIZE],
                  float thresh,
                  float inputMin,
                  float inputMax,
                  float outputMin,
                  float outputMax,
                  float min_Value[3],
                  float max_Value[3]) {
// clang-format off
#pragma HLS INLINE OFF
    // clang-format on

    AWBhistogramkernel_imp<SRC_T, SRC_T, ROWS, COLS, NPC, USE_URAM, XFCVDEPTH_IN_1, XFCVDEPTH_OUT_1,
                           XF_DEPTH(SRC_T, NPC), 1, HISTSIZE>(src1, histogram0, thresh, inputMin, inputMax, outputMin,
                                                              outputMax);
    awbPreNorm<SRC_T, NPC, HISTSIZE>(src1.cols >> XF_BITSHIFT(NPC), src1.rows, thresh, inputMin, inputMax, histogram1,
                                     min_Value, max_Value);
}

template <int SRC_T,
          int DST_T,
          int ROWS,
          int COLS,
          int NPC = 1,
          int WB_TYPE,
          int HISTSIZE,
          int XFCVDEPTH_IN_1 = _XFCVDEPTH_DEFAULT,
          int XFCVDEPTH_OUT_1 = _XFCVDEPTH_DEFAULT>
void AWBhistogram_multi(xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1>& src1,
                        xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_OUT_1>& src2,
                        uint32_t histogram[3][HISTSIZE],
                        float thresh,
                        float inputMin,
                        float inputMax,
                        float outputMin,
                        float outputMax,
                        int slc_id) {
// clang-format off
#pragma HLS INLINE OFF
    // clang-format on

    AWBhistogramkernel_imp_multi<SRC_T, SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1, XFCVDEPTH_OUT_1, XF_DEPTH(SRC_T, NPC),
                                 1, HISTSIZE>(src1, src2, histogram, thresh, inputMin, inputMax, outputMin, outputMax,
                                              slc_id);
}

template <int SRC_T,
          int DST_T,
          int ROWS,
          int COLS,
          int NPC = 1,
          int WB_TYPE,
          int HISTSIZE,
          int XFCVDEPTH_IN_1 = _XFCVDEPTH_DEFAULT,
          int XFCVDEPTH_OUT_1 = _XFCVDEPTH_DEFAULT>
void AWBNormalization(xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1>& src,
                      xf::cv::Mat<DST_T, ROWS, COLS, NPC, XFCVDEPTH_OUT_1>& dst,
                      float inputMax,
                      float min_Value[3],
                      float max_val[3]) {
// clang-format off
#pragma HLS INLINE OFF
    // clang-format on

    AWBNormalizationkernel<SRC_T, SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1, XFCVDEPTH_OUT_1, XF_DEPTH(SRC_T, NPC), 1,
                           HISTSIZE>(src, dst, inputMax, min_Value, max_val);
}

template <int SRC_T,
          int DST_T,
          int ROWS,
          int COLS,
          int NPC = 1,
          int WB_TYPE,
          int HISTSIZE,
          int XFCVDEPTH_IN_1 = _XFCVDEPTH_DEFAULT,
          int XFCVDEPTH_OUT_1 = _XFCVDEPTH_DEFAULT>
void AWBNormalization_multi(xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1>& src,
                            xf::cv::Mat<DST_T, ROWS, COLS, NPC, XFCVDEPTH_OUT_1>& dst,
                            uint32_t histogram[3][HISTSIZE],
                            float thresh,
                            float inputMin,
                            float inputMax,
                            float outputMin,
                            float outputMax,
                            unsigned short org_height) {
// clang-format off
#pragma HLS INLINE OFF
    // clang-format on

    AWBNormalizationkernel_multi<SRC_T, SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1, XFCVDEPTH_OUT_1, XF_DEPTH(SRC_T, NPC),
                                 1, HISTSIZE>(src, dst, histogram, thresh, inputMin, inputMax, outputMin, outputMax,
                                              org_height);
}

template <int SRC_T,
          int DST_T,
          int ROWS,
          int COLS,
          int NPC = 1,
          int WB_TYPE,
          int XFCVDEPTH_IN_1 = _XFCVDEPTH_DEFAULT,
          int XFCVDEPTH_OUT_1 = _XFCVDEPTH_DEFAULT>
void AWBGainUpdate(xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1>& src1,
                   xf::cv::Mat<DST_T, ROWS, COLS, NPC, XFCVDEPTH_OUT_1>& src2,
                   float thresh,
                   int i_gain[3]) {
    xf::cv::AWBGainUpdateKernel<SRC_T, SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1, XFCVDEPTH_OUT_1, XF_CHANNELS(SRC_T, NPC),
                                XF_DEPTH(SRC_T, NPC), XF_DEPTH(SRC_T, NPC), XF_WORDWIDTH(SRC_T, NPC),
                                XF_WORDWIDTH(SRC_T, NPC), (COLS >> XF_BITSHIFT(NPC))>(src1, src2, thresh, i_gain);
}

template <int SRC_T,
          int DST_T,
          int ROWS,
          int COLS,
          int NPC = 1,
          int WB_TYPE,
          int XFCVDEPTH_IN_1 = _XFCVDEPTH_DEFAULT,
          int XFCVDEPTH_OUT_1 = _XFCVDEPTH_DEFAULT>
void AWBChannelGain(xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1>& src,
                    xf::cv::Mat<DST_T, ROWS, COLS, NPC, XFCVDEPTH_OUT_1>& dst,
                    float thresh,
                    int i_gain[3]) {
    xf::cv::AWBChannelGainKernel<SRC_T, SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1, XFCVDEPTH_OUT_1,
                                 XF_CHANNELS(SRC_T, NPC), XF_DEPTH(SRC_T, NPC), XF_DEPTH(SRC_T, NPC),
                                 XF_WORDWIDTH(SRC_T, NPC), XF_WORDWIDTH(SRC_T, NPC), (COLS >> XF_BITSHIFT(NPC))>(
        src, dst, thresh, i_gain);
}

template <int SRC_T,
          int DST_T,
          int ROWS,
          int COLS,
          int NPC = 1,
          int WB_TYPE,
          int HISTSIZE,
          int XFCVDEPTH_IN_1 = _XFCVDEPTH_DEFAULT,
          int XFCVDEPTH_OUT_1 = _XFCVDEPTH_DEFAULT>
void hist_nor_awb(xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1>& src1,
                  xf::cv::Mat<DST_T, ROWS, COLS, NPC, XFCVDEPTH_OUT_1>& dst,
                  uint32_t hist0[3][HISTSIZE],
                  uint32_t hist1[3][HISTSIZE],
                  unsigned short rows,
                  unsigned short cols,
                  float thresh,
                  float inputMin,
                  float inputMax,
                  float outputMin,
                  float outputMax,
                  int slc_id,
                  unsigned short org_height) {
// clang-format off
#pragma HLS INLINE OFF
    // clang-format on
    xf::cv::Mat<DST_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1> src2(rows, cols);
// clang-format off
#pragma HLS DATAFLOW
    // clang-format on
    AWBhistogram_multi<DST_T, DST_T, ROWS, COLS, NPC, WB_TYPE, HISTSIZE, XFCVDEPTH_IN_1, XFCVDEPTH_IN_1>(
        src1, src2, hist0, thresh, inputMin, inputMax, outputMin, outputMax, slc_id);

    AWBNormalization_multi<DST_T, DST_T, ROWS, COLS, NPC, WB_TYPE, HISTSIZE, XFCVDEPTH_IN_1, XFCVDEPTH_OUT_1>(
        src2, dst, hist1, thresh, inputMin, inputMax, outputMin, outputMax, org_height);
}

template <int SRC_T,
          int DST_T,
          int ROWS,
          int COLS,
          int NPC = 1,
          int WB_TYPE,
          int HISTSIZE,
          int XFCVDEPTH_IN_1 = _XFCVDEPTH_DEFAULT,
          int XFCVDEPTH_OUT_1 = _XFCVDEPTH_DEFAULT>
void hist_nor_awb_wrap(xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1>& src1,
                       xf::cv::Mat<DST_T, ROWS, COLS, NPC, XFCVDEPTH_OUT_1>& dst,
                       uint32_t hist0[3][HISTSIZE],
                       uint32_t hist1[3][HISTSIZE],
                       unsigned short rows,
                       unsigned short cols,
                       float thresh,
                       float inputMin,
                       float inputMax,
                       float outputMin,
                       float outputMax,
                       bool& flag,
                       bool& eof,
                       int slc_id,
                       unsigned short org_height) {
// clang-format off
#pragma HLS INLINE OFF
    // clang-format on

    if (!flag) {
        hist_nor_awb<DST_T, DST_T, ROWS, COLS, NPC, WB_TYPE, HISTSIZE, XFCVDEPTH_IN_1, XFCVDEPTH_OUT_1>(
            src1, dst, hist0, hist1, rows, cols, thresh, inputMin, inputMax, outputMin, outputMax, slc_id, org_height);

        if (eof) flag = 1;

    } else {
        hist_nor_awb<DST_T, DST_T, ROWS, COLS, NPC, WB_TYPE, HISTSIZE, XFCVDEPTH_IN_1, XFCVDEPTH_OUT_1>(
            src1, dst, hist1, hist0, rows, cols, thresh, inputMin, inputMax, outputMin, outputMax, slc_id, org_height);

        if (eof) flag = 0;
    }

    return;
}

template <int SRC_T,
          int DST_T,
          int ROWS,
          int COLS,
          int NPC = 1,
          int WB_TYPE,
          int HISTSIZE,
          int STREAMS = 2,
          int XFCVDEPTH_IN_1 = _XFCVDEPTH_DEFAULT,
          int XFCVDEPTH_OUT_1 = _XFCVDEPTH_DEFAULT>
void hist_nor_awb_multi(xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1>& src1,
                        xf::cv::Mat<DST_T, ROWS, COLS, NPC, XFCVDEPTH_OUT_1>& dst,
                        uint32_t hist0[STREAMS][3][HISTSIZE],
                        uint32_t hist1[STREAMS][3][HISTSIZE],
                        unsigned short rows,
                        unsigned short cols,
                        float inputMin,
                        float inputMax,
                        float outputMin,
                        float outputMax,
                        bool flag[STREAMS],
                        bool eof[STREAMS],
                        unsigned short pawb[STREAMS],
                        int strm_id,
                        int slc_id,
                        unsigned short org_height) {
// clang-format off

#pragma HLS ARRAY_PARTITION variable=hist0 complete dim=1
#pragma HLS ARRAY_PARTITION variable=hist0 complete dim=2

#pragma HLS ARRAY_PARTITION variable=hist1 complete dim=1
#pragma HLS ARRAY_PARTITION variable=hist1 complete dim=2

#pragma HLS ARRAY_PARTITION variable=flag complete dim=1
#pragma HLS ARRAY_PARTITION variable=eof complete dim=1
#pragma HLS ARRAY_PARTITION variable=pawb complete dim=1

    // clang-format on
    float thresh = (float)pawb[strm_id] / 256;

    hist_nor_awb_wrap<DST_T, DST_T, ROWS, COLS, NPC, WB_TYPE, HISTSIZE, XFCVDEPTH_IN_1, XFCVDEPTH_OUT_1>(
        src1, dst, hist0[strm_id], hist1[strm_id], rows, cols, thresh, inputMin, inputMax, outputMin, outputMax,
        flag[strm_id], eof[strm_id], slc_id, org_height);
}

template <int SRC_T,
          int DST_T,
          int ROWS,
          int COLS,
          int NPC = 1,
          int WB_TYPE,
          int XFCVDEPTH_IN_1 = _XFCVDEPTH_DEFAULT,
          int XFCVDEPTH_OUT_1 = _XFCVDEPTH_DEFAULT>
void chgain_update_awb(xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1>& src1,
                       xf::cv::Mat<DST_T, ROWS, COLS, NPC, XFCVDEPTH_OUT_1>& dst,
                       float thresh,
                       int i_gain0[3],
                       int i_gain1[3],
                       unsigned short rows,
                       unsigned short cols) {
// clang-format off
#pragma HLS INLINE OFF
    // clang-format on  
    xf::cv::Mat<DST_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1> src2(rows, cols); 

// clang-format off
#pragma HLS DATAFLOW
    // clang-format on                        
    AWBChannelGain<DST_T, DST_T, ROWS, COLS, NPC, WB_TYPE, XFCVDEPTH_IN_1, XFCVDEPTH_IN_1>(src1, src2, thresh, i_gain0);
    AWBGainUpdate<DST_T, DST_T, ROWS, COLS, NPC, WB_TYPE, XFCVDEPTH_IN_1, XFCVDEPTH_OUT_1>(src2, dst, thresh, i_gain1);
   
}    
template <int SRC_T,
          int DST_T,
          int ROWS,
          int COLS,
          int NPC = 1,
          int WB_TYPE,
          int XFCVDEPTH_IN_1 = _XFCVDEPTH_DEFAULT,
          int XFCVDEPTH_OUT_1 = _XFCVDEPTH_DEFAULT>
void chgain_update_awb_wrap(xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1>& src1,
                    xf::cv::Mat<DST_T, ROWS, COLS, NPC, XFCVDEPTH_OUT_1>& dst,
                    float thresh,
                    int i_gain0[3],
                    int i_gain1[3],
                    unsigned short rows,
                    unsigned short cols,
                    bool &flag,
                    bool &eof) {
// clang-format off
#pragma HLS INLINE OFF
    // clang-format on                       
 
    if(!flag){ 
        chgain_update_awb<DST_T, DST_T, ROWS, COLS, NPC, WB_TYPE, XFCVDEPTH_IN_1, XFCVDEPTH_OUT_1>(src1, dst, thresh, i_gain0, i_gain1, rows, cols);
        
        if(eof) flag = 1;
    }
    else {
               
        chgain_update_awb<DST_T, DST_T, ROWS, COLS, NPC, WB_TYPE, XFCVDEPTH_IN_1, XFCVDEPTH_OUT_1>(src1, dst, thresh, i_gain1, i_gain0, rows, cols);
        
        if(eof) flag = 0;                
    }  
    return;
}    
template <int SRC_T,
          int DST_T,
          int ROWS,
          int COLS,
          int NPC = 1,
          int WB_TYPE,
          int STREAMS = 2,
          int XFCVDEPTH_IN_1 = _XFCVDEPTH_DEFAULT,
          int XFCVDEPTH_OUT_1 = _XFCVDEPTH_DEFAULT>
void chgain_update_awb_multi(xf::cv::Mat<SRC_T, ROWS, COLS, NPC, XFCVDEPTH_IN_1>& src1,
                    xf::cv::Mat<DST_T, ROWS, COLS, NPC, XFCVDEPTH_OUT_1>& dst,
                    int i_gain0[STREAMS][3],
                    int i_gain1[STREAMS][3],
                    unsigned short rows,
                    unsigned short cols,
                    bool flag[STREAMS],
                    bool eof[STREAMS],
                    unsigned short pawb[STREAMS],
                    int strm_id) {
                   
                                      
// clang-format off
#pragma HLS ARRAY_PARTITION variable= pawb dim=1 complete
#pragma HLS ARRAY_PARTITION variable=i_gain0 complete dim=1
#pragma HLS ARRAY_PARTITION variable=i_gain0 complete dim=2


#pragma HLS ARRAY_PARTITION variable=i_gain1 complete dim=1
#pragma HLS ARRAY_PARTITION variable=i_gain1 complete dim=2


#pragma HLS ARRAY_PARTITION variable=flag complete dim=1
#pragma HLS ARRAY_PARTITION variable=eof complete dim=1

    // clang-format on
    float thresh = (float)pawb[strm_id] / 256;

    chgain_update_awb_wrap<DST_T, DST_T, ROWS, COLS, NPC, WB_TYPE, XFCVDEPTH_IN_1, XFCVDEPTH_OUT_1>(
        src1, dst, thresh, i_gain0[strm_id], i_gain1[strm_id], rows, cols, flag[strm_id], eof[strm_id]);
}
}
}
#endif //_XF_AWB_HPP_
