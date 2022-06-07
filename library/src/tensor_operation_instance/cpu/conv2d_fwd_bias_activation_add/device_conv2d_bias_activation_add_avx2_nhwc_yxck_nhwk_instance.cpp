#include <stdlib.h>
#include "config.hpp"
#include "convolution_forward_specialization_cpu.hpp"
#include "device_convnd_fwd_bias_activation_add_avx2_nhwc_yxck_nhwk.hpp"
#include "element_wise_operation_cpu.hpp"
#include "device_operation_instance.hpp"

namespace ck {
namespace tensor_operation {
namespace cpu {
namespace device {
namespace device_conv2d_fwd_bias_activation_add_avx2_instance {

using InType  = float;
using WeiType = float;
using OutType = float;
using AccType = float;

static constexpr bool NonTemporalStore = false;

using PT         = ck::tensor_operation::cpu::element_wise::PassThrough;
using AddReluAdd = ck::tensor_operation::cpu::element_wise::AddReluAdd;

static constexpr auto ConvFwdDefault =
    ck::tensor_operation::cpu::device::ConvolutionForwardSpecialization_t::Default;

static constexpr auto ConvFwd1x1P0 =
    ck::tensor_operation::cpu::device::ConvolutionForwardSpecialization_t::Filter1x1Pad0;

static constexpr auto ConvFwd1x1S1P0 =
    ck::tensor_operation::cpu::device::ConvolutionForwardSpecialization_t::Filter1x1Stride1Pad0;

static constexpr auto DefaultGemmKLoop =
    ck::tensor_operation::cpu::device::ConvolutionForwardGemmKSpecialization_t::DefaultGemmKLoop;
static constexpr auto GemmKLoopOverC =
    ck::tensor_operation::cpu::device::ConvolutionForwardGemmKSpecialization_t::NHWC_GemmKLoopOverC;

static constexpr auto LoopOver_MNK = ck::tensor_operation::cpu::device::LoopOver_MNK;
static constexpr auto LoopOver_MKN = ck::tensor_operation::cpu::device::LoopOver_MKN;

// clang-format off
#define DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(a_elem_op, b_elem_op, c_elem_op, m_per_block, n_per_block, k_per_block, m_per_thread, n_per_thread, c_local_buf, bias_along_m) \
    DeviceConvNDFwdBiasActivationAddAvx2_Input_N_Hi_Wi_C_Weight_Y_X_C_K_Output_N_Ho_Wo_K<float , float , float, float , float, a_elem_op, b_elem_op, c_elem_op, ConvFwdDefault, GemmKLoopOverC  , LoopOver_MNK, 2, m_per_block, n_per_block, k_per_block, m_per_thread, n_per_thread, true , true , c_local_buf, bias_along_m>, \
    DeviceConvNDFwdBiasActivationAddAvx2_Input_N_Hi_Wi_C_Weight_Y_X_C_K_Output_N_Ho_Wo_K<float , float , float, float , float, a_elem_op, b_elem_op, c_elem_op, ConvFwd1x1S1P0, GemmKLoopOverC  , LoopOver_MNK, 2, m_per_block, n_per_block, k_per_block, m_per_thread, n_per_thread, true , true , c_local_buf, bias_along_m>, \
    DeviceConvNDFwdBiasActivationAddAvx2_Input_N_Hi_Wi_C_Weight_Y_X_C_K_Output_N_Ho_Wo_K<float , float , float, float , float, a_elem_op, b_elem_op, c_elem_op, ConvFwdDefault, DefaultGemmKLoop, LoopOver_MNK, 2, m_per_block, n_per_block, k_per_block, m_per_thread, n_per_thread, true , true , c_local_buf, bias_along_m>, \
    DeviceConvNDFwdBiasActivationAddAvx2_Input_N_Hi_Wi_C_Weight_Y_X_C_K_Output_N_Ho_Wo_K<float , float , float, float , float, a_elem_op, b_elem_op, c_elem_op, ConvFwd1x1S1P0, GemmKLoopOverC  , LoopOver_MNK, 2, m_per_block, n_per_block, k_per_block, m_per_thread, n_per_thread, false, false, c_local_buf, bias_along_m>, \
    DeviceConvNDFwdBiasActivationAddAvx2_Input_N_Hi_Wi_C_Weight_Y_X_C_K_Output_N_Ho_Wo_K<float , float , float, float , float, a_elem_op, b_elem_op, c_elem_op, ConvFwdDefault, DefaultGemmKLoop, LoopOver_MNK, 2, m_per_block, n_per_block, k_per_block, m_per_thread, n_per_thread, true , false, c_local_buf, bias_along_m>, \
    \
    DeviceConvNDFwdBiasActivationAddAvx2_Input_N_Hi_Wi_C_Weight_Y_X_C_K_Output_N_Ho_Wo_K<float , float , float, float , float, a_elem_op, b_elem_op, c_elem_op, ConvFwdDefault, GemmKLoopOverC  , LoopOver_MKN, 2, m_per_block, n_per_block, k_per_block, m_per_thread, n_per_thread, true , true , c_local_buf, bias_along_m>, \
    DeviceConvNDFwdBiasActivationAddAvx2_Input_N_Hi_Wi_C_Weight_Y_X_C_K_Output_N_Ho_Wo_K<float , float , float, float , float, a_elem_op, b_elem_op, c_elem_op, ConvFwd1x1S1P0, GemmKLoopOverC  , LoopOver_MKN, 2, m_per_block, n_per_block, k_per_block, m_per_thread, n_per_thread, true , true , c_local_buf, bias_along_m>, \
    DeviceConvNDFwdBiasActivationAddAvx2_Input_N_Hi_Wi_C_Weight_Y_X_C_K_Output_N_Ho_Wo_K<float , float , float, float , float, a_elem_op, b_elem_op, c_elem_op, ConvFwdDefault, DefaultGemmKLoop, LoopOver_MKN, 2, m_per_block, n_per_block, k_per_block, m_per_thread, n_per_thread, true , true , c_local_buf, bias_along_m>, \
    DeviceConvNDFwdBiasActivationAddAvx2_Input_N_Hi_Wi_C_Weight_Y_X_C_K_Output_N_Ho_Wo_K<float , float , float, float , float, a_elem_op, b_elem_op, c_elem_op, ConvFwd1x1S1P0, GemmKLoopOverC  , LoopOver_MKN, 2, m_per_block, n_per_block, k_per_block, m_per_thread, n_per_thread, false, false, c_local_buf, bias_along_m>, \
    DeviceConvNDFwdBiasActivationAddAvx2_Input_N_Hi_Wi_C_Weight_Y_X_C_K_Output_N_Ho_Wo_K<float , float , float, float , float, a_elem_op, b_elem_op, c_elem_op, ConvFwdDefault, DefaultGemmKLoop, LoopOver_MKN, 2, m_per_block, n_per_block, k_per_block, m_per_thread, n_per_thread, true , false, c_local_buf, bias_along_m>

// clang-format on

using device_conv2d_fwd_bias_activation_add_avx2_nhwc_yxck_nhwk_f32_instances = std::tuple<
    // clang-format off
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  256, 128,  64,  6, 16, false, false),
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  256, 128, 128,  6, 16, false, false),
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  128, 256, 128,  6, 16, false, false),

    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  512, 240, 128,  4, 24, false, false),
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  512, 256, 128,  6, 16, false, false),

    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  768, 320, 128,  6, 16, false, false),
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  896, 352, 128,  6, 16, false, false),
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd, 1024, 416, 128,  6, 16, false, false)>;
// clang-format on

// use this in single thread, but gemm_n is not multiple of 8
using device_conv2d_fwd_bias_activation_add_avx2_nhwc_yxck_nhwk_f32_local_c_instances = std::tuple<
    // clang-format off
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  256, 128,  64,  6, 16, true, false),
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  256, 128, 128,  6, 16, true, false),
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  128, 256, 128,  6, 16, true, false),

    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  512, 240, 128,  4, 24, true, false),
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  512, 256, 128,  6, 16, true, false),

    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  768, 320, 128,  6, 16, true, false),
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  896, 352, 128,  6, 16, true, false),
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd, 1024, 416, 128,  6, 16, true, false)>;
// clang-format on

// use this in multi thread environment (need local C buffer to avoid cache coherence, although some
// time no local c is better...)
using device_conv2d_fwd_bias_activation_add_avx2_nhwc_yxck_nhwk_f32_mt_instances = std::tuple<
    // clang-format off
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  24,  24, 256,  4, 24, false, false),
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  32,  24, 256,  4, 24, false, false),
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  40,  24, 256,  4, 24, false, false),

    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  48,  24, 256,  4, 24, false, false),
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  48,  48, 256,  4, 24, false, false),
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  56,  24, 256,  4, 24, false, false),

    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  72,  16, 128,  6, 16, false, false),
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  72,  16, 256,  6, 16, false, false),
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  72,  32, 128,  6, 16, false, false),
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  72,  32, 256,  6, 16, false, false),
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  96,  32, 128,  6, 16, false, false),
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  96,  64, 128,  6, 16, false, false),
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  120, 32, 128,  6, 16, false, false),
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  120, 64, 128,  6, 16, false, false),

    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  256, 128, 128,  6, 16, true, false),
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  128, 256, 128,  6, 16, true, false),

    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  512, 240, 128,  4, 24, true, false),
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  512, 256, 128,  6, 16, true, false),

    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  768, 320, 128,  6, 16, true, false),
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd,  896, 352, 128,  6, 16, true, false),
    DEVICE_CONV2D_FWD_BAA_AVX2_NHWC_YXCK_NHWK_F32(PT, PT, AddReluAdd, 1024, 416, 128,  6, 16, true, false)>;
// clang-format on

void add_device_conv2d_fwd_bias_activation_add_avx2_nhwc_yxck_nhwk(
    std::vector<DeviceConvFwdBiasActivationAddPtr<PT, PT, AddReluAdd>>& instances)
{
    ck::tensor_operation::device::add_device_operation_instances(
        instances, device_conv2d_fwd_bias_activation_add_avx2_nhwc_yxck_nhwk_f32_instances{});
}

void add_device_conv2d_fwd_bias_activation_add_avx2_nhwc_yxck_nhwk_local_c(
    std::vector<DeviceConvFwdBiasActivationAddPtr<PT, PT, AddReluAdd>>& instances)
{
    ck::tensor_operation::device::add_device_operation_instances(
        instances,
        device_conv2d_fwd_bias_activation_add_avx2_nhwc_yxck_nhwk_f32_local_c_instances{});
}

void add_device_conv2d_fwd_bias_activation_add_avx2_nhwc_yxck_nhwk_mt(
    std::vector<DeviceConvFwdBiasActivationAddPtr<PT, PT, AddReluAdd>>& instances)
{
    ck::tensor_operation::device::add_device_operation_instances(
        instances, device_conv2d_fwd_bias_activation_add_avx2_nhwc_yxck_nhwk_f32_mt_instances{});
}

} // namespace device_conv2d_fwd_bias_activation_add_avx2_instance
} // namespace device
} // namespace cpu
} // namespace tensor_operation
} // namespace ck
