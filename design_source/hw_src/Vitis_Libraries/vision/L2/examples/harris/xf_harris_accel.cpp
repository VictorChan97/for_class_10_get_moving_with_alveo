/*
 * Copyright 2022 Xilinx, Inc.
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

#include "xf_harris_accel_config.h"
extern "C" {
void cornerHarris_accel(
    ap_uint<INPUT_PTR_WIDTH>* img_inp, ap_uint<OUTPUT_PTR_WIDTH>* img_out, int rows, int cols, int threshold, int k) {
// clang-format off
    #pragma HLS INTERFACE m_axi     port=img_inp  offset=slave bundle=gmem1
    #pragma HLS INTERFACE m_axi     port=img_out  offset=slave bundle=gmem2
   
    #pragma HLS INTERFACE s_axilite port=rows     
    #pragma HLS INTERFACE s_axilite port=cols     
    #pragma HLS INTERFACE s_axilite port=threshold     
    #pragma HLS INTERFACE s_axilite port=k     
    #pragma HLS INTERFACE s_axilite port=return
    // clang-format on

    const int pROWS = HEIGHT;
    const int pCOLS = WIDTH;
    const int pNPC1 = NPPCX;

    xf::cv::Mat<IN_TYPE, HEIGHT, WIDTH, NPPCX, XF_CV_DEPTH_IN> in_mat(rows, cols);
    // clang-format off
    // clang-format on

    xf::cv::Mat<OUT_TYPE, HEIGHT, WIDTH, NPPCX, XF_CV_DEPTH_OUT> out_mat(rows, cols);
// clang-format off
// clang-format on

// clang-format off
    #pragma HLS DATAFLOW
    // clang-format on
    xf::cv::Array2xfMat<INPUT_PTR_WIDTH, IN_TYPE, HEIGHT, WIDTH, NPPCX, XF_CV_DEPTH_IN>(img_inp, in_mat);
    xf::cv::cornerHarris<FILTER_WIDTH, BLOCK_WIDTH, NMS_RADIUS, IN_TYPE, HEIGHT, WIDTH, NPPCX, XF_USE_URAM,
                         XF_CV_DEPTH_IN, XF_CV_DEPTH_OUT>(in_mat, out_mat, threshold, k);
    xf::cv::xfMat2Array<OUTPUT_PTR_WIDTH, OUT_TYPE, HEIGHT, WIDTH, NPPCX, XF_CV_DEPTH_OUT>(out_mat, img_out);
}
}
