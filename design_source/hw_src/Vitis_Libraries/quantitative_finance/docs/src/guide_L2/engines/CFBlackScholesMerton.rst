..
   .. Copyright © 2019–2023 Advanced Micro Devices, Inc

.. `Terms and Conditions <https://www.amd.com/en/corporate/copyright>`_.

.. meta::
   :keywords: Black-Scholes-Merton
   :description: Black-Scholes-Merton models the dynamics of a financial market containing derivative investment instruments. 
   :xlnxdocumentclass: Document
   :xlnxdocumenttype: Tutorials


.. _black_scholes_merton_engine:

***************************************************
Internal Design of Closed Form Black-Scholes-Merton
***************************************************

Overview
========

Black-Scholes-Merton models the dynamics of a financial market containing derivative investment instruments.

In the specific case of pricing European Options, the Black-Scholes-Merton (BSM) model has a closed-form solution for the price and the associated 'Greeks' which measure the sensitivity of the price to changes in the parameter.  The availability of such a solution means that the results can be found by direct evaluation rather than, for example, a Monte-Carlo simulation or otherwise. This allows for considerably greater throughput and the possibility to produce multiple results on every clock cycle by evaluating the equations in parallel.

The closed-form equations for the price and associated Greeks can be found online (see for example https://en.wikipedia.org/wiki/Black-Scholes_model) and is not reproduced here.


Design Structure
================

The heart of the BSM closed-form solver is a solver which implements the closed-form equations.  It takes one set of input parameters (S,V,r,t,K) and a flag to control which type of option (call or put) should be calculated. Then, it returns the appropriate price and the associated Greeks. This function is mostly standard C++ with some exception of using the HLS maths library.

Layered on top is the HLS specific kernel wrapper which is responsible for gathering the input data sets (from DDR or HBM for example), converting them to parallel streams and passing them into the kernel. Then, the kernel writes the results back. The kernel level is where the HLS #pragmas are used to control the amount of pipelining and unrolling.


cfBSMEngine (cf_bsm.hpp)
======================

This is the code for the solver itself, and it is templated to accept different data types. It uses standard C++ and allows the code to be easily used in a software only environment by swapping to the standard math namespace. The code is a straightforward implementation of the closed form equations above, with some small optimizations for FPGA implementation. In particular, it is clear from the equations that the price and Greeks contain many of the same elements, so the code implementation is structured to calculate these sub-components and reuse them in the subsequent calculations. It separates the computation into a number of smaller steps that can be computed in parallel. The process also improves throughput and reduces resource utilization. Another optimization is the calculation which require the Normal CDF (often called Phi). It is resource heavy in standard math library, so an approximation is used instead of the accurate result.


bsm_kernel (bsm_kernel.cpp)
===========================

The kernel is the HLS wrapper level which implements the pipelining and parallelization to allow high throughput.  The kernel uses a dataflow methodology with streams to pass the data through the design.

In the top level, the input and output ports are 512 bit wide, which is designed to match the whole DDR bus width and allowing vector access. In the case of float data type (four bytes), sixteen parameters can be accessed from the bus in parallel.  Each port is connected to its own AXI master with arbitration handled by the AXI switch and DDR controller under the hood.

These ports are interfaced via functions in bus_interface.hpp which convert between the wide bus and a template number of streams. Once input stream form, each stream is passed to a separate instance of the cfBSMEngine engine.  The cfBSMEngine engine is wrapped inside bsm_stream_wrapper(), which handles the stream processing.  Here the II and loop unrolling is controlled.  One cfBSMEngine engine is instanced per stream allowing for parallel processing of multiple parameter sets.  Additionally, the engines are in an II=1 loop, so that each engine can produce one price and its associated Greeks on each cycle.


Theoretical throughput
======================

The BSM solver demonstration is configured to build one kernel (consisting of some number of cfBSMEngine engines) that connected to a single DDR bank. This particular design is bandwidth constrained, so the number of usable engines can be determined by considering the data requirements as follows: one single cfBSMEngine instance requires 5 input parameters and returns 6 values (price and Greeks) for a total of 11 float values (44 bytes) transferred every clock cycle. At 200MHz for example, it requires a data bandwidth of 200e6 * 44 = 8.8GB/s.  The theoretical bandwidth of a single DDR bank as used in the U200 is 19.2 GB/s so that two cfBSMEngine engines in parallel could achieve 17.6GB/s at perfect utilization. Three engines would exceed the capabilities of one DDR, as would two engines and a higher FMAX, so two engine is the configuration selected for the demonstration design. In practice of course, it is not possible to achieve the theoretical maximum due to limitations in the AXI interconnect, DDR controller, access patterns, and so on.


Resource Utilization
====================
The hardware resources are listed in :numref:`tab1CFBSM`.  This is for the demonstration as configured by default (two cfBSMEngine engines) achieving a 300 MHz clock rate.

.. _tab1CFBSM:

.. table:: Hardware resources for single kernel with two parallel cfBSMEngine engines.
    :align: center

    +--------------------------+----------+----------+----------+----------+----------+-----------------+
    |          Engines         |   BRAM   |    DSP   | Register |    LUT   |  Latency | clock period(ns)|
    +--------------------------+----------+----------+----------+----------+----------+-----------------+
    | bsm_kernel               |    374   |    520   |  101830  |   80272  |   334    |       3.333     |
    +--------------------------+----------+----------+----------+----------+----------+-----------------+


Throughput
==========

The hardware implementation, running on the U200 displays the kernel throughput (this is the kernel execution time but not the host->device and device->host copies of the data). A large data set of over 4 million parameters is used to overcome any initial pipeline latency and fully occupy the device buffers.

    [bash]$ ./bsm_test ./xclbin/bsm_kernel.hw.u200.xclbin 4194304

This achieves around 203 million options per second, which is approximately 8.9GB/s of data transferred. This is about half of the theoretical DDR bandwidth, but around 80% of that achieved by the 'xbutil validate' DDR bandwidth test. This can be taken as a more realistic target as it includes any platform overhead which is also incurred in the BSM solver.

.. toctree::
   :maxdepth: 1
