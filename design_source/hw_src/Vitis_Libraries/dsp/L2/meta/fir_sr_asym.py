from ctypes import sizeof
from socket import TIPC_SUB_SERVICE
from aie_common import *
from aie_common_fir import *
import json

#### naming ####
#
# Name functions with prefix
#   validate_ for validators, returning boolean result and error message as a tuple.
#   update_ for updators, returning object with default value and refined candidate constraints.
#   info_ for creating information based on parameters
#   fn_ for internal functions
#
# Name function arguments as template parameters, when possible
# so the code matches easier with API definition.


# Example of validator.
#
# The parameter itself will be passed as first argument for validator functions.
# These functions can have extra parameters as arguments, as specified as last part of in `validator`.
# These extra parameters must appear before current one in "parameters" section.
#
# A validator function returns a dictionary, with required boolean key "is_valid",
# and "err_message" if "is_valid" is False.
#

TP_INPUT_WINDOW_VSIZE_min = 4
TP_CASC_LEN_min = 1
TP_CASC_LEN_max = 40
TP_FIR_LEN_min = 4
TP_FIR_LEN_max = 8192

def fnNumLanesStream(*args):
    return fnNumLanes(*args, TP_API=1)


def fnNumColsStream(T_D, T_C, AIE_VARIANT):

    if AIE_VARIANT == 2:
        # AIE_ML API always calls for 4 columns
        return 4

    if AIE_VARIANT == 1:

        # Slight rephrasing (vs traits) to avoid templates and enable runtime check.
        if T_D == "cint16" or T_D == "int16" or (T_D == "int32" and T_C == "int32"):
            return fnNumCols384(T_D, T_C)
        else:
            # int32,  int16
            # cint32,  int16
            # cint32,  int32
            # cint32, cint16
            # float,  float
            # cfloat,  float
            # cfloat, cfloat
            return fnNumCols(T_D, T_C)


def fn_validate_input_window_size(
    TT_DATA, TT_COEFF, TP_FIR_LEN, TP_INPUT_WINDOW_VSIZE, TP_API, TP_SSR=1
):
    # CAUTION: this constant overlaps many factors. The main need is a "strobe" concept that means we unroll until xbuff is back to starting conditions.
    streamRptFactor = 8
    res = fn_validate_min_value("TP_INPUT_WINDOW_VSIZE", TP_INPUT_WINDOW_VSIZE, TP_INPUT_WINDOW_VSIZE_min)
    if (res["is_valid"] == False):
      return res

    # Need to take unrolloing into account
    windowSizeMultiplier = (
        (fnNumLanes(TT_DATA, TT_COEFF, TP_API))
        if TP_API == 0
        else (fnNumLanes(TT_DATA, TT_COEFF, TP_API) * streamRptFactor)
    )

    checkMultipleLanes = fn_windowsize_multiple_lanes(
        TT_DATA, TT_COEFF, TP_INPUT_WINDOW_VSIZE, TP_API, windowSizeMultiplier, TP_SSR
    )
    checkMaxBuffer = fn_max_windowsize_for_buffer(
        TT_DATA, TP_FIR_LEN, TP_INPUT_WINDOW_VSIZE, TP_API, TP_SSR
    )
    # Input samples are round-robin split to each SSR input paths, so total frame size must be divisable by SSR factor.
    checkIfDivisableBySSR = fn_windowsize_divisible_by_param(
        TP_INPUT_WINDOW_VSIZE, TP_SSR
    )

    for check in (checkMultipleLanes, checkMaxBuffer, checkIfDivisableBySSR):
        if check["is_valid"] == False:
            return check

    return isValid


#### validate cascade length ####

# align to num cols coeffs for FIR cascade splitting for optimal mac efficiency
def fnStreamFirRangeRound(T_D, T_C, AIE_VARIANT):
    return fnNumColsStream(T_D, T_C, AIE_VARIANT)


# Calculate FIR range offset for cascaded kernel
def fnFirRangeOffset(TP_FL, TP_CL, TP_KP, TP_Rnd=1, TP_Sym=1):

    # TP_FL - FIR Length, TP_CL - Cascade Length, TP_KP - Kernel Position
    return (
        TP_KP * (fnTrunc(TP_FL, TP_Rnd * TP_CL) // TP_CL)
        + (
            TP_Rnd * TP_KP
            if (TP_FL - fnTrunc(TP_FL, TP_Rnd * TP_CL)) >= TP_Rnd * TP_KP
            else (fnTrunc(TP_FL, TP_Rnd) - fnTrunc(TP_FL, TP_Rnd * TP_CL))
        )
    ) // TP_Sym


def fnFirRangeRemAsym(TP_FL, TP_CL, TP_KP, TT_DATA, TT_COEFF, TP_API, AIE_VARIANT):

    # TP_FL - FIR Length, TP_CL - Cascade Length, TP_KP - Kernel Position
    # this is for last in the cascade
    return fnFirRangeRem(
        TP_FL,
        TP_CL,
        TP_KP,
        ((fnStreamFirRangeRound(TT_DATA, TT_COEFF, AIE_VARIANT)) if (TP_API == 1) else 1),
    )


# Calculate ASYM FIR range for cascaded kernel
def fnFirRangeAsym(TP_FL, TP_CL, TP_KP, TT_DATA, TT_COEFF, TP_API, AIE_VARIANT):

    # TP_FL - FIR Length, TP_CL - Cascade Length, TP_KP - Kernel Position
    # make sure there's no runt filters ( lengths < 4)
    # make Stream architectures rounded to fnStreamFirRangeRound and only last in the chain possibly odd
    # TODO: make Window architectures rounded to fnNumColumnsSrAsym
    return fnFirRange(
        TP_FL,
        TP_CL,
        TP_KP,
        ((fnStreamFirRangeRound(TT_DATA, TT_COEFF, AIE_VARIANT)) if (TP_API == 1) else 1),
    )


# Calculate ASYM FIR range offset for cascaded kernel
def fnFirRangeOffsetAsym(TP_FL, TP_CL, TP_KP, TT_DATA, TT_COEFF, TP_API, AIE_VARIANT):
    # TP_FL - FIR Length, TP_CL - Cascade Length, TP_KP - Kernel Position
    return fnFirRangeOffset(
        TP_FL,
        TP_CL,
        TP_KP,
        ((fnStreamFirRangeRound(TT_DATA, TT_COEFF, AIE_VARIANT)) if (TP_API == 1) else 1),
    )


# function to return Margin length.
def fnFirMargin(TP_FIR_LEN, TT_DATA):
    return CEIL(TP_FIR_LEN, ((256 // 8) // fn_size_by_byte(TT_DATA)))


def fnStreamReadWidth(T_D, T_C):
    # Slight rephrasing (vs traits) to avoid templates and enable runtime check.
    if T_D == "cint16" or T_D == "int16" or (T_D == "int32" and T_C == "int32"):
        return 128
    else:
        # int32,  int16
        # cint32,  int16
        # cint32,  int32
        # cint32, cint16
        # float,  float
        # cfloat,  float
        # cfloat, cfloat
        return 256


# todo - just pad coefficients so this doesn't matter
def fn_fir_len_divisible_ssr(
    TP_FIR_LEN, TP_SSR
):  # when rate changers get SSR do scaling with TP_INTERPOLATE_FACTOR=1, TP_DECIMATE_FACTOR=1
    # check if divisible by SSR
    if TP_FIR_LEN % TP_SSR != 0:
        return isError(
            f"Filter length ({TP_FIR_LEN}) needs to be divisible by SSR ({TP_SSR})."
        )
    return isValid


# This logic is copied from the kernel class.
def fn_get_data_needed(
    TT_DATA, TT_COEFF, TP_FIR_LEN, TP_CASC_LEN, TP_KERNEL_POSITION, TP_SSR, TP_API, AIE_VARIANT
):
    TT_DATA_BYTES = fn_size_by_byte(TT_DATA)
    m_kFirRangeOffset = fnFirRangeOffsetAsym(
        TP_FIR_LEN, TP_CASC_LEN, TP_KERNEL_POSITION, TT_DATA, TT_COEFF, TP_API, AIE_VARIANT
    )
    # FIR Cascade Offset for this kernel position
    m_kFirMarginOffset = fnFirMargin(TP_FIR_LEN, TT_DATA) - TP_FIR_LEN + 1
    # FIR Margin Offset.
    m_kFirMarginRangeOffset = m_kFirMarginOffset + m_kFirRangeOffset
    TP_MODIFY_MARGIN_OFFSET = 1 if (TP_SSR > 1) else 0
    # at least one kernel in ssr designs has modify margin offset.
    m_kFirInitOffset = m_kFirMarginRangeOffset + TP_MODIFY_MARGIN_OFFSET
    m_kDataBuffXOffset = m_kFirInitOffset % (
        (128 // 8) // TT_DATA_BYTES
    )  # Remainder of m_kFirInitOffset divided by 128bit

    TP_FIR_RANGE_LEN = (
        fnFirRangeRemAsym(
            TP_FIR_LEN, TP_CASC_LEN, TP_KERNEL_POSITION, TT_DATA, TT_COEFF, TP_API, AIE_VARIANT
        )
        if (TP_KERNEL_POSITION == (TP_CASC_LEN - 1))  # last Kernel gets remainder taps
        else fnFirRangeAsym(
            TP_FIR_LEN, TP_CASC_LEN, TP_KERNEL_POSITION, TT_DATA, TT_COEFF, TP_API, AIE_VARIANT
        )
    )

    m_kArchFirLen = TP_FIR_RANGE_LEN + m_kDataBuffXOffset

    m_kLanes = (
        fnNumLanes(TT_DATA, TT_COEFF)
        if TP_API == 0
        else fnNumLanesStream(TT_DATA, TT_COEFF)
    )
    m_kDataLoadVsize = (
        (256 // 8 // TT_DATA_BYTES)
        if TP_API == 0
        else (fnStreamReadWidth(TT_DATA, TT_COEFF) // 8 // TT_DATA_BYTES)
    )
    m_kInitDataNeeded = m_kArchFirLen + m_kDataLoadVsize - 1
    return m_kInitDataNeeded


# For streaming FIRs, all the initial data needed for a single mac needs to fit in a single buffer.
def fn_data_needed_within_buffer_size(
    TT_DATA, TT_COEFF, TP_FIR_LEN, TP_CASC_LEN, TP_API, TP_SSR=1, AIE_VARIANT =1
):
    m_kSamplesInBuff = (1024 // 8) // fn_size_by_byte(TT_DATA)
    # only do stuff for streaming
    if TP_API == 1:
        for TP_KP in range(TP_CASC_LEN):
            # Check every kernel's init data needed (different kernels need different DataBuffXOffset)
            m_kInitDataNeeded = fn_get_data_needed(
                TT_DATA,
                TT_COEFF,
                TP_FIR_LEN // TP_SSR,
                TP_CASC_LEN,
                TP_KP,
                TP_SSR,
                TP_API,
                AIE_VARIANT
            )
            if m_kInitDataNeeded > m_kSamplesInBuff:
                return isError(
                    f"Requested parameters: FIR length ({TP_FIR_LEN}), cascade length ({TP_CASC_LEN}) and SSR ({TP_SSR}) result in a kernel ({TP_KP}) that requires more data samples ({m_kInitDataNeeded}) than capacity of a data buffer ({m_kSamplesInBuff}) "
                    f"Please increase the cascade length ({TP_CASC_LEN}) and/or SSR ({TP_SSR})."
                )

    return isValid



def fn_validate_fir_len(
    TT_DATA, TT_COEFF, TP_FIR_LEN, TP_CASC_LEN, TP_SSR, TP_API, TP_USE_COEFF_RELOAD, AIE_VARIANT
):

    lastKernelFirRangeLen = fnFirRangeRemAsym(
        TP_FIR_LEN // TP_SSR, TP_CASC_LEN, (TP_CASC_LEN - 1), TT_DATA, TT_COEFF, TP_API, AIE_VARIANT)
    firLengthMin = 1
    # Check the last kernel only, as it is the shortest in any configuration.
    if lastKernelFirRangeLen < firLengthMin:
        return isError(
        f"Requested Fir Length ({TP_FIR_LEN}) split over ({TP_CASC_LEN}) cascaded kernels and ({TP_SSR}) SSR polyphases results in at least one kernel configured with ({lastKernelFirRangeLen}) number of taps that does not meet minimum ({firLengthMin}) requirement. "\
        f"Please reduce cascade length ({TP_CASC_LEN}) and/or SSR ({TP_SSR}) parameter. "
        )

    res = fn_validate_minmax_value("TP_FIR_LEN", TP_FIR_LEN, TP_FIR_LEN_min, TP_FIR_LEN_max)
    if (res["is_valid"] == False):
        return res
    divCheck = fn_fir_len_divisible_ssr(TP_FIR_LEN, TP_SSR)
    maxLenCheck = fn_max_fir_len_each_kernel(TT_DATA, TP_FIR_LEN, TP_CASC_LEN, TP_USE_COEFF_RELOAD, TP_SSR, TP_API, 1)  # last param refers to symmetry factor
    dataNeededCheck = fn_data_needed_within_buffer_size(TT_DATA, TT_COEFF, TP_FIR_LEN, TP_CASC_LEN, TP_API, TP_SSR, AIE_VARIANT)
    for check in (divCheck, maxLenCheck, dataNeededCheck):
        if check["is_valid"] == False:
            return check

    return isValid

def fn_validate_casc_len(TP_CASC_LEN):
    return fn_validate_minmax_value("TP_CASC_LEN", TP_CASC_LEN, TP_CASC_LEN_min, TP_CASC_LEN_max)


#### validation APIs ####
def validate_TT_COEFF(args):
    TT_DATA = args["TT_DATA"]
    TT_COEFF = args["TT_COEFF"]
    AIE_VARIANT = args["AIE_VARIANT"]
    standard_checks = fn_validate_coeff_type(TT_DATA, TT_COEFF)
    typeCheck = fn_type_sr_support(TT_DATA, TT_COEFF, AIE_VARIANT)
    for check in (standard_checks,typeCheck):
      if check["is_valid"] == False :
        return check
    return isValid


def validate_TP_INPUT_WINDOW_VSIZE(args):
    TP_INPUT_WINDOW_VSIZE = args["TP_INPUT_WINDOW_VSIZE"]
    TT_DATA = args["TT_DATA"]
    TT_COEFF = args["TT_COEFF"]
    TP_FIR_LEN = args["TP_FIR_LEN"]
    TP_API = args["TP_API"]
    TP_SSR = args["TP_SSR"]
    return fn_validate_input_window_size(
        TT_DATA, TT_COEFF, TP_FIR_LEN, TP_INPUT_WINDOW_VSIZE, TP_API, TP_SSR
    )


def validate_TP_DUAL_IP(args):
    TP_NUM_OUTPUTS = args["TP_NUM_OUTPUTS"]
    TP_API = args["TP_API"]
    TP_DUAL_IP = args["TP_DUAL_IP"]
    AIE_VARIANT = args["AIE_VARIANT"]
    return fn_validate_sr_dual_ip(TP_NUM_OUTPUTS, TP_API, TP_DUAL_IP, AIE_VARIANT)


def validate_TP_NUM_OUTPUTS(args):
    TP_NUM_OUTPUTS = args["TP_NUM_OUTPUTS"]
    TP_API = args["TP_API"]
    AIE_VARIANT = args["AIE_VARIANT"]
    return fn_validate_num_outputs(TP_API, TP_NUM_OUTPUTS, AIE_VARIANT)


def validate_TP_SHIFT(args):
    TT_DATA = args["TT_DATA"]
    TP_SHIFT = args["TP_SHIFT"]
    return fn_validate_shift(TT_DATA, TP_SHIFT)

def validate_TP_RND(args):
  TP_RND = args["TP_RND"]
  AIE_VARIANT = args["AIE_VARIANT"]
  return fn_validate_roundMode(TP_RND, AIE_VARIANT)

def validate_TP_SAT(args):
  TP_SAT = args["TP_SAT"]
  return fn_validate_satMode(TP_SAT)

def validate_TP_CASC_LEN(args):
    TP_CASC_LEN = args["TP_CASC_LEN"]
    return fn_validate_casc_len(TP_CASC_LEN)


def validate_TP_SSR(args):
    TP_SSR = args["TP_SSR"]
    return fn_validate_ssr(TP_SSR)


def validate_TP_FIR_LEN(args):
    TT_DATA = args["TT_DATA"]
    TT_COEFF = args["TT_COEFF"]
    TP_FIR_LEN = args["TP_FIR_LEN"]
    TP_CASC_LEN = args["TP_CASC_LEN"]
    TP_SSR = args["TP_SSR"]
    TP_API = args["TP_API"]
    TP_USE_COEFF_RELOAD = args["TP_USE_COEFF_RELOAD"]
    AIE_VARIANT = args["AIE_VARIANT"]

    return fn_validate_fir_len(
        TT_DATA, TT_COEFF, TP_FIR_LEN, TP_CASC_LEN, TP_SSR, TP_API, TP_USE_COEFF_RELOAD, AIE_VARIANT
    )


# Example of updater.
#
# Updater are functions to help GUI to hint user on parameter setting with already given parameters.
# The return object will provide "value" which will be set in the wizard as the dependent parameter is being set.
# The rest of keys are similar to parameter definition, but with candidates of enum or range values refined
# based on previously set values.
#
# An updator function always return a dictionary,
# including key "value" for automatically filled default in GUI as dependent parameters have been set, and
# other keys for overriding the definition of parameter.
#
# For example, if a parameter has definition in JSON as
#  { "name": "foo", "type": "typename", "enum": ["int", "float", "double"] }
# And the updator returns
#  { "value": "int", "enum": ["int", "float"] }
# The GUI would show "int" as default and make "int" and "float" selectable candidates, while disabling "double".
#
# If with given combination, no valid value can be set for the parameter being updated, the upater function
# should set "value" to None, to indicate an error and provide error message via "err_message".
# For example
#  { "value": None, "err_message": "With TT_DATA as 'int' there is no valid option for TT_COEFF" }
#
# In this example, the following is the updater for TT_COEFF, with TT_DATA as the dependent parameter.
# When GUI generates a wizard, TT_DATA should be required first, as it shows up in parameter list first.
# Once user has provided value for TT_DATA, this function will be called and set the value of TT_COEFF.
# Meanwhile, the candidate shown in wizard based on enum will also be updated.
#
def update_TT_COEFF(TT_DATA):
    return {"value": TT_DATA, "enum": [TT_DATA]}


#### port ####


def fn_ceil(m, n):
    return int(((m + n - 1) / n)) * n


def fn_margin_size(TP_FIR_LEN, TT_DATA):
    tmpmargin = (int(TP_FIR_LEN) - 1) * fn_size_by_byte(TT_DATA)
    return fn_ceil(tmpmargin, 32)


def info_ports(args):
    """Standard function creating a static dictionary of information
    for upper software to correctly connect the IP.
    Some IP has dynamic number of ports according to parameter set,
    so port information has to be implemented as a function"""
    TT_DATA = args["TT_DATA"]
    TT_COEFF = args["TT_COEFF"]
    TP_INPUT_WINDOW_VSIZE = args["TP_INPUT_WINDOW_VSIZE"]
    TP_FIR_LEN = args["TP_FIR_LEN"]
    TP_SSR = args["TP_SSR"]
    TP_API = args["TP_API"]
    TP_DUAL_IP = args["TP_DUAL_IP"]
    TP_NUM_OUTPUTS = args["TP_NUM_OUTPUTS"]
    TP_DECIMATE_FACTOR = 1
    TP_INTERPOLATE_FACTOR = 1
    margin_size = fn_margin_size(TP_FIR_LEN // TP_SSR, TT_DATA)
    num_in_ports = TP_SSR
    num_out_ports = TP_SSR
    in_win_size = get_input_window_size(TP_INPUT_WINDOW_VSIZE, num_in_ports, TP_API, TP_DUAL_IP)
    out_win_size = get_output_window_size(TP_INPUT_WINDOW_VSIZE, num_out_ports, TP_API, TP_NUM_OUTPUTS, TP_DECIMATE_FACTOR, TP_INTERPOLATE_FACTOR)

    in_ports = get_port_info( "in", "in", TT_DATA, in_win_size, num_in_ports, marginSize=margin_size, TP_API=TP_API)
    in2_ports = (get_port_info( "in2", "in", TT_DATA, in_win_size, num_in_ports, marginSize=margin_size, TP_API=TP_API, ) if (args["TP_DUAL_IP"] == 1) else [])
    coeff_ports = (get_parameter_port_info("coeff", "in", TT_COEFF, TP_SSR, TP_FIR_LEN, "async") if (args["TP_USE_COEFF_RELOAD"] == 1) else [] )

    # decimate by 2 for halfband
    out_ports = get_port_info( "out", "out", TT_DATA, out_win_size, num_out_ports, TP_API=TP_API,)
    out2_ports = (get_port_info( "out2", "out", TT_DATA, out_win_size, num_out_ports, TP_API=TP_API, ) if (args["TP_NUM_OUTPUTS"] == 2) else [])
    return in_ports + in2_ports + coeff_ports + out_ports + out2_ports


#### graph generator ####
# def get_param_list(**kwargs):
#   [f"{value}{comma}} //{key}" for key, value in kwargs.iteritems() for comma in "," ]

# Returns formatted string with taps
def fn_get_taps_vector(TT_COEFF, coeff_list):

    cplx = fn_is_complex(TT_COEFF)

    # todo, reformat this to use list comprehension
    taps = f"{{"
    # complex pair
    if cplx:
        taps += ", ".join(
            [
                f"{{{coeff_list[2*i]} , {coeff_list[2*i+1]}}}"
                for i in range(int(len(coeff_list) / 2))
            ]
        )
    else:
        taps += ", ".join([str(coeff_list[i]) for i in range(len(coeff_list))])
    taps += f"}}"

    return taps


def generate_graph(graphname, args):

    if graphname == "":
        graphname = "default_graphname"

    TT_COEFF = args["TT_COEFF"]
    TT_DATA = args["TT_DATA"]
    TP_FIR_LEN = args["TP_FIR_LEN"]
    TP_SHIFT = args["TP_SHIFT"]
    TP_RND = args["TP_RND"]
    TP_CASC_LEN = args["TP_CASC_LEN"]
    TP_INPUT_WINDOW_VSIZE = args["TP_INPUT_WINDOW_VSIZE"]
    TP_USE_COEFF_RELOAD = args["TP_USE_COEFF_RELOAD"]
    TP_NUM_OUTPUTS = args["TP_NUM_OUTPUTS"]
    TP_DUAL_IP = args["TP_DUAL_IP"]
    TP_API = args["TP_API"]
    TP_SSR = args["TP_SSR"]
    coeff_list = args["coeff"]
    TP_SAT = args["TP_SAT"]

    taps = fn_get_taps_vector(TT_COEFF, coeff_list)
    constr_args_str = f"taps" if TP_USE_COEFF_RELOAD == 0 else ""
    dual_ip_declare_str = (
        f"ssr_port_array<input> in2;" if TP_DUAL_IP == 1 else "// No dual input"
    )
    dual_ip_connect_str = (
        f"adf::connect<> net_in2(in2[i], filter.in2[i]);"
        if TP_DUAL_IP == 1
        else "// No dual input"
    )
    coeff_ip_declare_str = (
        f"ssr_port_array<input> coeff;"
        if TP_USE_COEFF_RELOAD == 1
        else "//No coeff port"
    )
    coeff_ip_connect_str = (
        f"adf::connect<> net_coeff(coeff[i], filter.coeff[i]);"
        if TP_USE_COEFF_RELOAD == 1
        else "//No coeff port"
    )
    dual_op_declare_str = (
        f"ssr_port_array<output> out2;" if TP_NUM_OUTPUTS == 2 else "// No dual output"
    )
    dual_op_connect_str = (
        f"adf::connect<> net_out2(filter.out2[i], out2[i]);"
        if TP_NUM_OUTPUTS == 2
        else "// No dual output"
    )

    # Use formatted multi-line string to avoid a lot of \n and \t
    code = f"""
class {graphname} : public adf::graph {{
public:
  static constexpr unsigned int TP_SSR = {TP_SSR};
  template <typename dir>
  using ssr_port_array = std::array<adf::port<dir>, TP_SSR>;

  ssr_port_array<input> in;
  {dual_ip_declare_str}
  {coeff_ip_declare_str}
  ssr_port_array<output> out;
  {dual_op_declare_str}

  std::vector<{TT_COEFF}> taps = {taps};
  xf::dsp::aie::fir::sr_asym::fir_sr_asym_graph<
    {TT_DATA}, //TT_DATA
    {TT_COEFF}, //TT_COEFF
    {TP_FIR_LEN}, //TP_FIR_LEN
    {TP_SHIFT}, //TP_SHIFT
    {TP_RND}, //TP_RND
    {TP_INPUT_WINDOW_VSIZE}, //TP_INPUT_WINDOW_VSIZE
    {TP_CASC_LEN}, //TP_CASC_LEN
    {TP_USE_COEFF_RELOAD}, //TP_USE_COEFF_RELOAD
    {TP_NUM_OUTPUTS}, //TP_NUM_OUTPUTS
    {TP_DUAL_IP}, //TP_DUAL_IP
    {TP_API}, //TP_API
    {TP_SSR}, //TP_SSR
    {TP_SAT} //TP_SAT
  > filter;

  {graphname}() : filter({constr_args_str}) {{
    adf::kernel *filter_kernels = filter.getKernels();
    for (int i=0; i < 1; i++) {{
      adf::runtime<ratio>(filter_kernels[i]) = 0.9;
    }}
    for (int i=0; i < TP_SSR; i++) {{
      adf::connect<> net_in(in[i], filter.in[i]);
      {dual_ip_connect_str}
      {coeff_ip_connect_str}
      adf::connect<> net_out(filter.out[i], out[i]);
      {dual_op_connect_str}
    }}
  }}

}};
"""
    out = {}
    out["graph"] = code
    out["port_info"] = info_ports(args)
    out["headerfile"] = "fir_sr_asym_graph.hpp"
    out["searchpaths"] = [
        "L2/include/aie",
        "L2/tests/aie/common/inc",
        "L1/include/aie",
        "L1/src/aie",
        "L1/tests/aie/inc",
        "L1/tests/aie/src",
    ]
    return out
