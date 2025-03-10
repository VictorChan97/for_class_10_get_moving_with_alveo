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

#ifndef _XF_3DLUT_CONFIG_H_
#define _XF_3DLUT_CONFIG_H_

#include "hls_stream.h"
#include "ap_int.h"
#include "common/xf_common.hpp"
#include "common/xf_utility.hpp"
#include "imgproc/xf_3dlut.hpp"

#define WIDTH 1920
// 7680//3840//
#define HEIGHT 1080
// 4320//2160//

#define XF_CV_DEPTH_IN_1 2
#define XF_CV_DEPTH_IN_2 2
#define XF_CV_DEPTH_OUT_1 2

#define NPPCX XF_NPPC1

#define T_8U 1

#define XF_USE_URAM 1

#define INPUT_PTR_WIDTH 32
#define OUTPUT_PTR_WIDTH 32
#define LUT_PTR_WIDTH 128

#define IN_TYPE XF_8UC3
#define OUT_TYPE XF_8UC3

static constexpr int ERROR_THRESHOLD = 5;
static constexpr int LUT_DIM = 33;
static constexpr int SQ_LUTDIM = LUT_DIM * LUT_DIM;

#endif
// _XF_3DLUT_CONFIG_H_
