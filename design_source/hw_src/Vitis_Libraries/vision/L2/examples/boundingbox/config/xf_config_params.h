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
#ifndef _XF_BOUNDINGBOX_CONFIG_H_
#define _XF_BOUNDINGBOX_CONFIG_H_

#include "hls_stream.h"
#include "ap_int.h"

#include "common/xf_common.hpp"
#include "common/xf_utility.hpp"
#include "imgproc/xf_boundingbox.hpp"

/*  set the height and weight  */
#define HEIGHT 2160
#define WIDTH 3840

#define XF_CV_DEPTH_IN -1
#define MAX_BOXES 5

/*configurable*/
#define T_8U 1
#define T_16U 0

#define GRAY 0
#define RGBA 1

#define NPPCX XF_NPPC1

#define IN_TYPE XF_8UC4
#define OUT_TYPE XF_8UC4

#define CV_IN_TYPE CV_8UC4
#define CV_OUT_TYPE CV_8UC4

#define INPUT_CH_TYPE 4
#define OUTPUT_CH_TYPE 4

#define INPUT_PTR_WIDTH 64
#define OUTPUT_PTR_WIDTH 64

#endif
// end of _XF_BOUNDINGBOX_CONFIG_H_
