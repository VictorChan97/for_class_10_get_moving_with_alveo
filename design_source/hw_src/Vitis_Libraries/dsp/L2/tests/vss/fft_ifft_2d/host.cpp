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
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <unistd.h>
#include <xrt/xrt_device.h>
#include <xrt/xrt_kernel.h>

#include <experimental/xrt_aie.h>
#include <experimental/xrt_graph.h>
#include <experimental/xrt_ip.h>

template <unsigned int TP_POINT_SIZE>
constexpr unsigned int fnSqrt() {
    unsigned int sqrtVal =
        TP_POINT_SIZE == 65536
            ? 256
            : TP_POINT_SIZE == 16384
                  ? 128
                  : TP_POINT_SIZE == 4096
                        ? 64
                        : TP_POINT_SIZE == 1024
                              ? 32
                              : TP_POINT_SIZE == 256 ? 16 : TP_POINT_SIZE == 64 ? 8 : TP_POINT_SIZE == 16 ? 4 : 8;
    return sqrtVal;
}

static const char* STR_ERROR = "ERROR:   ";
static const char* STR_PASSED = "PASSED:  ";
static const char* STR_USAGE = "USAGE:   ";
static const char* STR_INFO = "INFO:    ";

// ------------------------------------------------------------
// DDR Parameters
// ------------------------------------------------------------

typedef int32_t TT_DATA; // Assume int32 data for I/O data (will be cint32)

static constexpr int32_t NUM_ITER = -1; // Let the graph run and have DMA_SNK terminate things
static constexpr int32_t LOOP_CNT_I = 8;
static constexpr int32_t LOOP_CNT_O = 4; // Try to stop it early
static constexpr int32_t LOOP_SEL = 0;   // ID of loop to capture by DDR SNK PL HLS block
static constexpr unsigned NFFT_1D = fnSqrt<POINT_SIZE>();
static constexpr unsigned DEPTH = POINT_SIZE; // 1 transform
static constexpr unsigned DDR_WORD_DEPTH_I = DEPTH;
static constexpr unsigned DDR_WORD_DEPTH_O = DEPTH;
static constexpr unsigned NUM_SAMPLES_I = 2 * DDR_WORD_DEPTH_I; // 2 x 32-bit (int32) words for cint32 I/O
static constexpr unsigned NUM_SAMPLES_O = 2 * DDR_WORD_DEPTH_O; // 2 x 32-bit (int32) words for cint32 I/O

static constexpr unsigned DDR_BUFFSIZE_I_BYTES = NUM_SAMPLES_I * 4; // Each real/imag sample is 4 bytes
static constexpr unsigned DDR_BUFFSIZE_O_BYTES = NUM_SAMPLES_O * 4; // Each real/imag sample is 4 bytes

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------

int main(int argc, char* argv[]) {
    // ------------------------------------------------------------
    // Load XCLBIN
    // ------------------------------------------------------------
    char xclbinFilename[] = "kernel.xclbin";
    unsigned dev_index = 0;
    auto my_device = xrt::device(dev_index);
    std::cout << STR_PASSED << "auto my_device = xrt::device(" << dev_index << ")" << std::endl;

    auto xclbin_uuid = my_device.load_xclbin(xclbinFilename);
    std::cout << STR_PASSED << "auto xclbin_uuid = my_device.load_xclbin(" << xclbinFilename << ")" << std::endl;

    // ------------------------------------------------------------
    // Load and Run AIE graph
    // ------------------------------------------------------------

    // auto my_graph  = xrt::graph(my_device, xclbin_uuid, "aie_dut");
    auto my_graph = xrt::graph(my_device, xclbin_uuid, "fft_tb");

    std::cout << STR_PASSED << "auto my_graph  = xrt::graph(my_device, xclbin_uuid, \"fft_tb\")" << std::endl;

    my_graph.reset();
    std::cout << STR_PASSED << "my_graph.reset()" << std::endl;

    my_graph.run(NUM_ITER);
    std::cout << STR_PASSED << "my_graph.run( NUM_ITER=" << NUM_ITER << " )" << std::endl;

    // ------------------------------------------------------------
    // Load and Start DDR Source/Sink PL Kernels
    // ------------------------------------------------------------

    auto dma_src = xrt::kernel(my_device, xclbin_uuid, "ifft_dma_src_wrapper:{dma_src}");
    std::cout << STR_PASSED << "auto dma_src = xrt::kernel(my_device, xclbin_uuid, \"ifft_dma_src_wrapper:{dma_src}\")"
              << std::endl;

    auto dma_snk = xrt::kernel(my_device, xclbin_uuid, "ifft_dma_snk_wrapper:{dma_snk}");
    std::cout << STR_PASSED << "auto dma_snk = xrt::kernel(my_device, xclbin_uuid, \"ifft_dma_snk_wrapper:{dma_snk}\")"
              << std::endl;

    // auto transpose = xrt::kernel(my_device, xclbin_uuid, "transpose_wrapper:{transpose}");
    // std::cout << STR_PASSED << "auto dma_snk = xrt::kernel(my_device, xclbin_uuid,
    // \"transpose_wrapper:{transpose}\")" << std::endl;

    xrt::run dma_src_run = xrt::run(dma_src);
    std::cout << STR_PASSED << "xrt::run dma_src_run = xrt::run(dma_src)" << std::endl;

    xrt::run dma_snk_run = xrt::run(dma_snk);
    std::cout << STR_PASSED << "xrt::run dma_snk_run = xrt::run(dma_snk)" << std::endl;

    // xrt::run transpose_run = xrt::run(transpose);
    // std::cout << STR_PASSED << "xrt::run transpose_run = xrt::run(transpose)" << std::endl;
    // ------------------------------------------------------------
    // Configure DDR Buffer Objects
    // ------------------------------------------------------------

    auto dma_src_bo = xrt::bo(my_device, DDR_BUFFSIZE_I_BYTES, dma_src.group_id(0));
    std::cout << STR_PASSED
              << "dma_src_bo = xrt::bo(my_device, DDR_BUFFSIZE_I_BYTES, dma_src.group_id(0) (=" << dma_src.group_id(0)
              << "))" << std::endl;

    auto dma_src_bo_mapped = dma_src_bo.map<TT_DATA*>();
    std::cout << STR_PASSED << "auto dma_src_bo_mapped = dma_src_bo.map<TT_DATA*>()" << std::endl;

    auto dma_snk_bo = xrt::bo(my_device, DDR_BUFFSIZE_O_BYTES, dma_snk.group_id(0));
    std::cout << STR_PASSED
              << "dma_snk_bo = xrt::bo(my_device, DDR_BUFFSIZE_O_BYTES, dma_snk.group_id(0) (=" << dma_snk.group_id(0)
              << "))" << std::endl;

    auto dma_snk_bo_mapped = dma_snk_bo.map<TT_DATA*>();
    std::cout << STR_PASSED << "auto dma_snk_bo_mapped = dma_snk_bo.map<TT_DATA*>()" << std::endl;

    // Open stimulus input file:
    std::ifstream ss_i;
    ss_i.open("input_front.txt", std::ifstream::in);
    if (ss_i.is_open() == 0) {
        std::cout << STR_ERROR << "failed to open input_front.txt" << std::endl;
        return (33);
    }

    // Read data from input file:
    for (unsigned ss = 0; ss < NUM_SAMPLES_I; ss += 2) {
        TT_DATA val_re, val_im;
        ss_i >> val_re >> val_im;
        dma_src_bo_mapped[ss] = val_re;
        dma_src_bo_mapped[ss + 1] = val_im;
    }
    ss_i.close();
    std::cout << STR_PASSED << "Successfully read input file sig_i.txt" << std::endl;

    // ------------------------------------------------------------
    // Load and start PL kernels
    // ------------------------------------------------------------

    dma_src_run.set_arg(0, dma_src_bo);
    std::cout << STR_PASSED << "dma_src_run.set_arg( 0, dma_src_bo )" << std::endl;

    dma_src_run.set_arg(1, LOOP_CNT_I);
    std::cout << STR_PASSED << "dma_src_run.set_arg( 1, LOOP_CNT_I=" << LOOP_CNT_I << " )" << std::endl;

    dma_snk_run.set_arg(0, dma_snk_bo);
    std::cout << STR_PASSED << "dma_snk.run.set_arg( 0, dma_snk_bo )" << std::endl;

    dma_snk_run.set_arg(1, LOOP_SEL);
    std::cout << STR_PASSED << "dma_snk_run.set_arg( 1, LOOP_SEL=" << LOOP_SEL << " )" << std::endl;

    dma_snk_run.set_arg(2, LOOP_CNT_O);
    std::cout << STR_PASSED << "dma_snk_run.set_arg( 2, LOOP_CNT_O=" << LOOP_CNT_O << " )" << std::endl;

    dma_src_run.start();
    std::cout << STR_PASSED << "dma_src_run.start()" << std::endl;

    dma_snk_run.start();
    std::cout << STR_PASSED << "dma_snk_run.start()" << std::endl;
    std::cout << STR_PASSED << "dma_src_run.start()" << std::endl;

    // transpose_run.start();
    // std::cout << STR_PASSED << "transpose_run.start()" << std::endl;
    // // Wait for all kernels to end:
    // std::cout << std::endl << STR_INFO << "Waiting for kernels to end..." << std::endl << std::endl;

    dma_snk_run.wait();
    std::cout << STR_PASSED << "dma_snk_run.wait()" << std::endl;

    // ------------------------------------------------------------
    // Retrieve Results
    // ------------------------------------------------------------

    dma_snk_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    std::cout << STR_PASSED << "dma_snk_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE)" << std::endl;

    // Open GOLDEN results file:
    std::ifstream ss_o;
    ss_o.open("ref_output.txt", std::ifstream::in);
    if (ss_o.is_open() == 0) {
        std::cout << STR_ERROR << "failed to open ref_output.txt" << std::endl;
        return (34);
    }

    // Open ACTUAL results file:
    std::ofstream ss_a;
    ss_a.open("sig_a.txt", std::ofstream::out);
    if (ss_a.is_open() == 0) {
        std::cout << STR_ERROR << "failed to open sig_a.txt" << std::endl;
        return (35);
    }

    // Validate results:
    bool flag = 0;
    int level = (1 << 4);
    for (unsigned ss = 0; ss < NUM_SAMPLES_O; ss += 2) {
        int val_g_re, val_g_im;
        ss_o >> val_g_re >> val_g_im;
        int val_a_re = dma_snk_bo_mapped[ss];
        int val_a_im = dma_snk_bo_mapped[ss + 1];
        ss_a << val_a_re << " " << val_a_im << std::endl;
        int err_re = abs(val_g_re - val_a_re);
        int err_im = abs(val_g_im - val_a_im);
        bool this_flag = (err_re > level) || (err_im > level); // Matlab is not bit accurate
        std::cout << "ss: " << (ss >> 1) << "  Gld: " << val_g_re << " " << val_g_im << "  Act: " << val_a_re << " "
                  << val_a_im << "  Err: " << err_re << " " << err_im;
        if (this_flag == 1) std::cout << " ***";
        std::cout << std::endl;
        flag |= this_flag;
    }
    ss_o.close();
    ss_a.close();

    std::cout << "Level: " << level << std::endl;

    // Done:
    if (flag == 0)
        std::cout << std::endl << "--- PASSED ---" << std::endl;
    else
        std::cout << std::endl << "*** FAILED ***" << std::endl;
    return (flag);
}
