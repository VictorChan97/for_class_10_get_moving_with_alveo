from fft_ifft_dit_1ch import *
from aie_common import *
import json
import math

def fn_get_parallel_power(ssr, api, variant):
    pp = -1
    if fn_is_power_of_two(ssr):
      if api == 0:#window
          if variant == 1: #window AIE1
              pp = fn_log2(ssr) 
          else:            #window AIE2
              pp = fn_log2(ssr) 
      else: #stream
          if variant == 1: #stream  AIE1
              pp = fn_log2(ssr) - 1
          else:            #stream  AIE2
              pp = fn_log2(ssr) 

    return pp

def vmc_validate_twiddle_type(args):
    aie_variant = args["AIE_VARIANT"]
    data_type = args["data_type"]
    twiddle_type = args["twiddle_type"]
    return fn_validate_twiddle_type(aie_variant, data_type, twiddle_type)

def vmc_validate_TP_RND(args):
  rnd_mode = args["rnd_mode"]
  AIE_VARIANT = args["AIE_VARIANT"]
  return fn_validate_round_val(rnd_mode, AIE_VARIANT)

#### VMC validators ####
def vmc_validate_point_size(args, api, ssr):
    variant = args["AIE_VARIANT"]
    pp = fn_get_parallel_power(ssr, api, variant)

    if pp == -1:
      return isError(f"Invalid SSR value specified. The value should be of the form 2^N between 2 and 512.")

    point_size = args["point_size"]
    dyn_point_size = 0
    data_type = args["data_type"]
    api = 1
    AIE_VARIANT = args["AIE_VARIANT"]
    return fn_validate_point_size(point_size, dyn_point_size, data_type, pp, api, AIE_VARIANT)

def vmc_validate_point_size_stream(args):
    api = 1
    ssr = args["ssr"]
    return vmc_validate_point_size(args, api, ssr)

def vmc_validate_point_size_window(args):
    api = 0
    ssr = 1
    return vmc_validate_point_size(args, api, ssr)


def vmc_validate_shift_val(args):
    data_type = args["data_type"]
    shift_val = args["shift_val"]
    return fn_validate_shift_val(data_type, shift_val)

def vmc_validate_input_window_size(args):
    point_size = args["point_size"]
    input_window_size = args["input_window_size"]
    dyn_point_size = 0;
    return fn_validate_window_size(point_size, input_window_size, dyn_point_size)

def vmc_validate_casc_length(args, api, ssr):
    data_type = args["data_type"]
    point_size = args["point_size"]
    casc_length = args["casc_length"]
    variant = args["AIE_VARIANT"]
    pp = fn_get_parallel_power(ssr, api, variant)

    if pp == -1:
      return isError(f"Invalid SSR value specified. The value should be of the form 2^N between 2 and 512.")

    return fn_validate_casc_len(data_type, point_size, pp, casc_length)

def vmc_validate_casc_length_stream(args):
    api = 1
    ssr = args["ssr"]    
    return vmc_validate_casc_length(args, api, ssr)

def vmc_validate_casc_length_window(args):
    api = 0
    ssr = 1
    return vmc_validate_casc_length(args, api, ssr)

def vmc_validate_sat_mode(args):
    sat_mode = args["sat_mode"]
    return fn_validate_satMode(sat_mode);

def vmc_validate_twiddle_mode(args):
    twiddle_mode = args["twiddle_mode"]
    return fn_validate_twiddleMode(twiddle_mode);

def vmc_validate_ssr(args,api):
    ssr = args["ssr"]
    variant = args["AIE_VARIANT"]
    if ssr < 1 or ssr > 512:
      return isError(f"Minimum value for parameter SSR is 1 and 512, respectively, but got {ssr}. ")
    pp = fn_get_parallel_power(ssr, api, variant)

    if pp == -1:
      return isError(f"Invalid SSR value specified. The value should be of the form 2^N between 2 and 512.")

    return fn_validate_parallel_power(api, pp)

#In VMC the stream and windowed variants of FFT are separate IP, so neither has TP_API.
#similarly, FFT and IFFT are separate units (except when dynamic because that take direction as a runtime arg)
def vmc_validate_ssr_stream(args):
    api = 1
    return vmc_validate_ssr(args, api)

def vmc_validate_ssr_window(args):
    api = 0
    return vmc_validate_ssr(args, api)

# Get twiddle types
k_twiddle_type = {"cfloat":"cfloat", "cint32":"cint16", "cint16":"cint16"}

def fn_get_twiddle_type(data_type):
    return k_twiddle_type[data_type]

