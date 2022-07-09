// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2022, Advanced Micro Devices, Inc. All rights reserved.

#include <cstdlib>
#include <numeric>
#include "ck/ck.hpp"
#include "ck/tensor_operation/gpu/device/tensor_layout.hpp"
#include "ck/tensor_operation/gpu/device/device_convnd_backward_weight_xdl_c_shuffle_nhwc_kyxc_nhwk.hpp"
#include "ck/tensor_operation/gpu/element/element_wise_operation.hpp"
#include "ck/library/tensor_operation_instance/add_device_operation_instance.hpp"

namespace ck {
namespace tensor_operation {
namespace device {
namespace instance {

using BF16 = bhalf_t;
using F32  = float;

template <ck::index_t... Is>
using S = ck::Sequence<Is...>;

using PassThrough = ck::tensor_operation::element_wise::PassThrough;
static constexpr auto ConvBwdWeightDefault =
    ck::tensor_operation::device::ConvolutionBackwardWeightSpecialization::Default;

static constexpr auto ConvBwdWeightFilter1x1Stride1Pad0 =
    ck::tensor_operation::device::ConvolutionBackwardWeightSpecialization::Filter1x1Stride1Pad0;

// Compilation parameters for in[n, hi, wi, c] * wei[k, y, x, c] = out[n, ho, wo, k]
using device_conv2d_bwd_weight_xdl_c_shuffle_nhwc_kyxc_nhwk_bf16_instances = std::tuple<
    // clang-format off
        //#################################################################################| InData| WeiData| OutData| AccData|          In|         Wei|         Out|          ConvBackward|    Num|  Block|  MPer|  NPer| K0Per| K1| MPer| NPer| MXdl| NXdl|  ABlockTransfer|   ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockLds|   BBlockTransfer|  BBlockTransfer| BBlockTransfer| BlockTransfer| BBlockTransfer| BBlockTransfer| BBlockLds|    CShuffle|    CShuffle|    CBlockTransfer| CBlockTransfer|
        //#################################################################################|   Type|    Type|    Type|    Type| Elementwise| Elementwise| Elementwise|                Weight|    Dim|   Size| Block| Block| Block|   |  XDL|  XDL|  Per|  Per|   ThreadCluster|    ThreadCluster| SrcAccessOrder|   SrcVectorDim|      SrcScalar|      DstScalar| AddExtraM|    ThreadCluster|   ThreadCluster| SrcAccessOrder|  SrcVectorDim|      SrcScalar|      DstScalar| AddExtraN| MXdlPerWave| NXdlPerWave|    ClusterLengths|ScalarPerVector|
        //#################################################################################|       |        |        |        |   Operation|   Operation|   Operation|        Specialization|Spatial|       |      |      |      |   |     |     | Wave| Wave| Lengths_K0_M_K1|     ArrangeOrder|               |               |      PerVector|   PerVector_K1|          |  Lengths_K0_N_K1|    ArrangeOrder|               |              |      PerVector|   PerVector_K1|          |  PerShuffle|  PerShuffle|  MBlock_MPerBlock|   NWaveNPerXdl|
        //#################################################################################|       |        |        |        |            |            |            |                      |       |       |      |      |      |   |     |     |     |     |                |                 |               |               |               |               |          |                 |                |               |              |               |               |          |            |            |  NBlock_NPerBlock|               |
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightDefault,     2,      256,   256,   128,    4,  8,   32,   32,    4,    2,  S<1, 4, 32, 2>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              4,    true,     S<1, 4, 16, 4>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              2,      true,        1,            1,     S<1, 32, 1, 8>,        4>,
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightDefault,     2,      256,   128,   256,    4,  8,   32,   32,    2,    4,  S<1, 4, 16, 4>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              2,    true,     S<1, 4, 32, 2>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              4,      true,        1,            1,     S<1, 32, 1, 8>,        4>,   
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightDefault,     2,      128,   128,   128,    4,  8,   32,   32,    4,    2,  S<1, 4, 16, 2>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              4,    true,     S<1, 4, 16, 2>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              4,      true,        1,            1,     S<1, 32, 1, 4>,        4>,   
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightDefault,     2,      256,   128,   128,    4,  8,   32,   32,    2,    2,  S<1, 4, 16, 4>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              2,    true,     S<1, 4, 16, 4>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              2,      true,        1,            1,     S<1, 32, 1, 4>,        4>,   
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightDefault,     2,      128,   128,    64,    4,  8,   32,   32,    2,    2,  S<1, 4, 16, 2>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              4,    true,     S<1, 4, 8,  4>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              2,      true,        1,            1,     S<1, 32, 1, 4>,        4>,   
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightDefault,     2,      128,    64,   128,    4,  8,   32,   32,    2,    2,  S<1, 4, 8,  4>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              2,    true,     S<1, 4, 16, 2>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              4,      true,        1,            1,     S<1, 32, 1, 4>,        4>,   
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightDefault,     2,       64,    64,    64,    4,  8,   32,   32,    2,    2,  S<1, 4, 8,  2>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              4,    true,     S<1, 4, 8,  2>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              4,      true,        1,            1,     S<1, 16, 1, 4>,        4>,   
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightDefault,     2,      256,   128,    64,    4,  8,   32,   32,    2,    1,  S<1, 4, 16, 4>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              2,    true,     S<1, 4, 8,  8>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              1,      true,        1,            1,     S<1, 32, 1, 4>,        4>,   
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightDefault,     2,      256,    64,   128,    4,  8,   32,   32,    1,    2,  S<1, 4, 8,  8>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              1,    true,     S<1, 4, 16, 4>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              2,      true,        1,            1,     S<1, 32, 1, 4>,        4>,   
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightDefault,     2,      128,   128,    32,    4,  8,   32,   32,    2,    1,  S<1, 4, 16, 2>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              4,    true,     S<1, 4, 4,  8>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              1,      true,        1,            1,     S<1, 32, 1, 4>,        4>,   
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightDefault,     2,      128,    32,   128,    4,  8,   32,   32,    1,    2,  S<1, 4, 4,  8>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              1,    true,     S<1, 4, 16, 2>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              4,      true,        1,            1,     S<1, 32, 1, 4>,        4>,   
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightDefault,     2,       64,    64,    32,    4,  8,   32,   32,    2,    1,  S<1, 4, 8,  2>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              4,    true,     S<1, 4, 4,  4>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              2,      true,        1,            1,     S<1, 16, 1, 4>,        4>,   
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightDefault,     2,       64,    32,    64,    4,  8,   32,   32,    1,    2,  S<1, 4, 4,  4>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              2,    true,     S<1, 4, 8,  2>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              4,      true,        1,            1,     S<1, 16, 1, 4>,        4>
    // clang-format on
    >;

using device_conv2d_bwd_weight_xdl_nhwc_kyxc_nhwk_1x1_s1_p0_bf16_instances = std::tuple<
    // clang-format off
        //#################################################################################| InData| WeiData| OutData| AccData|          In|         Wei|         Out|                       ConvBackward|    Num|  Block|  MPer|  NPer| K0Per| K1| MPer| NPer| MXdl| NXdl|  ABlockTransfer|   ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockLds|   BBlockTransfer|  BBlockTransfer| BBlockTransfer| BlockTransfer| BBlockTransfer| BBlockTransfer| BBlockLds|    CShuffle|    CShuffle|    CBlockTransfer| CBlockTransfer|
        //#################################################################################|   Type|    Type|    Type|    Type| Elementwise| Elementwise| Elementwise|                             Weight|    Dim|   Size| Block| Block| Block|   |  XDL|  XDL|  Per|  Per|   ThreadCluster|    ThreadCluster| SrcAccessOrder|   SrcVectorDim|      SrcScalar|      DstScalar| AddExtraM|    ThreadCluster|   ThreadCluster| SrcAccessOrder|  SrcVectorDim|      SrcScalar|      DstScalar| AddExtraN| MXdlPerWave| NXdlPerWave|    ClusterLengths|ScalarPerVector|
        //#################################################################################|       |        |        |        |   Operation|   Operation|   Operation|                     Specialization|Spatial|       |      |      |      |   |     |     | Wave| Wave| Lengths_K0_M_K1|     ArrangeOrder|               |               |      PerVector|   PerVector_K1|          |  Lengths_K0_N_K1|    ArrangeOrder|               |              |      PerVector|   PerVector_K1|          |  PerShuffle|  PerShuffle|  MBlock_MPerBlock|   NWaveNPerXdl|
        //#################################################################################|       |        |        |        |            |            |            |                                   |       |       |      |      |      |   |     |     |     |     |                |                 |               |               |               |               |          |                 |                |               |              |               |               |          |            |            |  NBlock_NPerBlock|               |
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightFilter1x1Stride1Pad0,     2,      256,   256,   128,    4,  8,   32,   32,    4,    2,  S<1, 4, 32, 2>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              4,    true,     S<1, 4, 16, 4>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              2,      true,        1,            1,     S<1, 32, 1, 8>,        4>,
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightFilter1x1Stride1Pad0,     2,      256,   128,   256,    4,  8,   32,   32,    2,    4,  S<1, 4, 16, 4>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              2,    true,     S<1, 4, 32, 2>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              4,      true,        1,            1,     S<1, 32, 1, 8>,        4>,   
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightFilter1x1Stride1Pad0,     2,      128,   128,   128,    4,  8,   32,   32,    4,    2,  S<1, 4, 16, 2>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              4,    true,     S<1, 4, 16, 2>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              4,      true,        1,            1,     S<1, 32, 1, 4>,        4>,   
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightFilter1x1Stride1Pad0,     2,      256,   128,   128,    4,  8,   32,   32,    2,    2,  S<1, 4, 16, 4>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              2,    true,     S<1, 4, 16, 4>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              2,      true,        1,            1,     S<1, 32, 1, 4>,        4>,   
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightFilter1x1Stride1Pad0,     2,      128,   128,    64,    4,  8,   32,   32,    2,    2,  S<1, 4, 16, 2>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              4,    true,     S<1, 4, 8,  4>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              2,      true,        1,            1,     S<1, 32, 1, 4>,        4>,   
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightFilter1x1Stride1Pad0,     2,      128,    64,   128,    4,  8,   32,   32,    2,    2,  S<1, 4, 8,  4>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              2,    true,     S<1, 4, 16, 2>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              4,      true,        1,            1,     S<1, 32, 1, 4>,        4>,   
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightFilter1x1Stride1Pad0,     2,       64,    64,    64,    4,  8,   32,   32,    2,    2,  S<1, 4, 8,  2>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              4,    true,     S<1, 4, 8,  2>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              4,      true,        1,            1,     S<1, 16, 1, 4>,        4>,   
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightFilter1x1Stride1Pad0,     2,      256,   128,    64,    4,  8,   32,   32,    2,    1,  S<1, 4, 16, 4>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              2,    true,     S<1, 4, 8,  8>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              1,      true,        1,            1,     S<1, 32, 1, 4>,        4>,   
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightFilter1x1Stride1Pad0,     2,      256,    64,   128,    4,  8,   32,   32,    1,    2,  S<1, 4, 8,  8>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              1,    true,     S<1, 4, 16, 4>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              2,      true,        1,            1,     S<1, 32, 1, 4>,        4>,   
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightFilter1x1Stride1Pad0,     2,      128,   128,    32,    4,  8,   32,   32,    2,    1,  S<1, 4, 16, 2>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              4,    true,     S<1, 4, 4,  8>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              1,      true,        1,            1,     S<1, 32, 1, 4>,        4>,   
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightFilter1x1Stride1Pad0,     2,      128,    32,   128,    4,  8,   32,   32,    1,    2,  S<1, 4, 4,  8>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              1,    true,     S<1, 4, 16, 2>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              4,      true,        1,            1,     S<1, 32, 1, 4>,        4>,   
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightFilter1x1Stride1Pad0,     2,       64,    64,    32,    4,  8,   32,   32,    2,    1,  S<1, 4, 8,  2>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              4,    true,     S<1, 4, 4,  4>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              2,      true,        1,            1,     S<1, 16, 1, 4>,        4>,   
        DeviceConvndBwdWeightXdl_C_Shuffle_Input_N_Hi_Wi_C_Weight_K_Y_X_C_Output_N_Ho_Wo_K<   BF16,    BF16,    BF16,     F32, PassThrough, PassThrough, PassThrough,  ConvBwdWeightFilter1x1Stride1Pad0,     2,       64,    32,    64,    4,  8,   32,   32,    1,    2,  S<1, 4, 4,  4>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              2,    true,     S<1, 4, 8,  2>,   S<0, 3, 1, 2>,   S<0, 2, 1, 3>,             2,              8,              4,      true,        1,            1,     S<1, 16, 1, 4>,        4>
    // clang-format on
    >;

void add_device_conv2d_bwd_weight_xdl_nhwc_kyxc_nhwk_bf16_instances(
    std::vector<DeviceConvBwdWeightPtr<PassThrough, PassThrough, PassThrough>>& instances)
{
    add_device_operation_instances(
        instances, device_conv2d_bwd_weight_xdl_c_shuffle_nhwc_kyxc_nhwk_bf16_instances{});
    add_device_operation_instances(
        instances, device_conv2d_bwd_weight_xdl_nhwc_kyxc_nhwk_1x1_s1_p0_bf16_instances{});
}

} // namespace instance
} // namespace device
} // namespace tensor_operation
} // namespace ck
