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

#ifndef XF_MACHINE_DUT_H
#define XF_MACHINE_DUT_H

#include "ap_int.h"

extern "C" void naiveBayesTrain_kernel(const int num_of_class,
                                       const int num_of_terms,
                                       ap_uint<512>* buf_in,
                                       ap_uint<512>* buf_out0,
                                       ap_uint<512>* buf_out1);

#endif
