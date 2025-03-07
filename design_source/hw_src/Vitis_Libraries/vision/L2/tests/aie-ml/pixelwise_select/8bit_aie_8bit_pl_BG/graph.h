/*
 * Copyright 2021 Xilinx, Inc.
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

#ifndef ADF_GRAPH_H
#define ADF_GRAPH_H

#include <adf.h>
#include "kernels.h"
#include "config.h"

using namespace adf;

class pixelwiseSelectGraph : public adf::graph {
   private:
    kernel k1;

   public:
    input_plio in_f, in_m, in_b;
    output_plio out;

    pixelwiseSelectGraph() {
        // create kernels
        k1 = kernel::create(pixelwiseSelect);

        in_f = input_plio::create("DataIn1", adf::plio_128_bits, "data/input_frame.txt");
        in_m = input_plio::create("DataIn2", adf::plio_128_bits, "data/input_mask.txt");
        in_b = input_plio::create("DataIn3", adf::plio_128_bits, "data/input_frame.txt");
        out = output_plio::create("DataOut1", adf::plio_128_bits, "data/output.txt");

        // create nets to connect kernels and IO ports
        connect<>(in_f.out[0], k1.in[0]);
        connect<>(in_m.out[0], k1.in[1]);
        connect<>(in_b.out[0], k1.in[2]);
        connect<>(k1.out[0], out.in[0]);

        adf::dimensions(k1.in[0]) = {ELEM_WITH_METADATA};
        adf::dimensions(k1.in[1]) = {ELEM_WITH_METADATA};
        adf::dimensions(k1.in[2]) = {ELEM_WITH_METADATA};
        adf::dimensions(k1.out[0]) = {ELEM_WITH_METADATA};

        // specify kernel sources
        source(k1) = "xf_pixelwiseSelect.cc";

        // specify kernel run times
        runtime<ratio>(k1) = 0.5;
    }
};
#endif
