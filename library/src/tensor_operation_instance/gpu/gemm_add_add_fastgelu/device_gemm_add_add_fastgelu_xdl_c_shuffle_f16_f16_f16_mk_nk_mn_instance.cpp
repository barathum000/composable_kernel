// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2022, Advanced Micro Devices, Inc. All rights reserved.

#include <cstdlib>

#include "ck/ck.hpp"
#include "ck/tensor_operation/gpu/device/device_gemm_multiple_d_xdl_cshuffle.hpp"
#include "ck/tensor_operation/gpu/element/element_wise_operation.hpp"
#include "ck/library/tensor_operation_instance/device_operation_instance.hpp"

namespace ck {
namespace tensor_operation {
namespace device {
namespace device_gemm_instance {

using F16     = ck::half_t;
using F32     = float;
using F16_F16 = ck::Tuple<F16, F16>;

using Row = ck::tensor_layout::gemm::RowMajor;
using Col = ck::tensor_layout::gemm::ColumnMajor;

template <ck::index_t... Is>
using S = ck::Sequence<Is...>;

using PassThrough    = ck::tensor_operation::element_wise::PassThrough;
using AddAddFastGelu = ck::tensor_operation::element_wise::AddAddFastGelu;

static constexpr auto GemmDefault = ck::tensor_operation::device::GemmSpecialization::Default;

// e = elementwise((a * b), d0, d1)
// outout: e[m, n]
// input: a[m, k], b[n, k], d0[m, n], d1[m ,n]
using device_gemm_add_add_fastgelu_xdl_c_shuffle_f16_f16_f16_mk_nk_mn_instances = std::tuple<
    // clang-format off
        //##############################| ALayout| BLayout| CLayout| AData| BData| AccData| CShuffle|  DsData| EData|           A|           B|            CDE|           GEMM| NumGemmK| Block|  MPer|  NPer|  KPer| AK1| BK1| MPer| NPer| MXdl| NXdl|  ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockTransfer| ABlockLds|  BBlockTransfer| BBlockTransfer| BBlockTransfer| BlockTransfer| BBlockTransfer| BBlockTransfer| BBlockLds|    CShuffle|    CShuffle| CBlockTransferClusterLengths|  CBlockTransfer|
        //##############################|        |        |        |  Type|  Type|    Type| DataType|    Type|  Type| Elementwise| Elementwise|    Elementwise| Spacialization| Prefetch|  Size| Block| Block| Block|    |    |  XDL|  XDL|  Per|  Per|   ThreadCluster|  ThreadCluster| SrcAccessOrder|   SrcVectorDim|      SrcScalar|      DstScalar| AddExtraM|   ThreadCluster|  ThreadCluster| SrcAccessOrder|  SrcVectorDim|      SrcScalar|      DstScalar| AddExtraN| MXdlPerWave| NXdlPerWave|         _MBlock_MWaveMPerXdl| ScalarPerVector|
        //##############################|        |        |        |      |      |        |         |        |      |   Operation|   Operation|      Operation|               |    Stage|      |      |      |      |    |    |     |     | Wave| Wave| Lengths_K0_M_K1|   ArrangeOrder|               |               |      PerVector|   PerVector_K1|          | Lengths_K0_N_K1|   ArrangeOrder|               |              |      PerVector|   PerVector_K1|          |  PerShuffle|  PerShuffle|         _NBlock_NWaveNPerXdl|   _NWaveNPerXdl|
        //##############################|        |        |        |      |      |        |         |        |      |            |            |               |               |         |      |      |      |      |    |    |     |     |     |     |                |               |               |               |               |               |          |                |               |               |              |               |               |          |            |            |                             |                |
        DeviceGemmMultipleD_Xdl_CShuffle<     Row,     Col,     Row,   F16,   F16,     F32,      F32, F16_F16,   F16, PassThrough, PassThrough, AddAddFastGelu,    GemmDefault,        1,   256,   256,   128,    32,   8,   8,   32,   32,    4,    2,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,             2,              8,              8,         1,           1,           1,               S<1, 32, 1, 8>,              8>,
        DeviceGemmMultipleD_Xdl_CShuffle<     Row,     Col,     Row,   F16,   F16,     F32,      F32, F16_F16,   F16, PassThrough, PassThrough, AddAddFastGelu,    GemmDefault,        1,   256,   128,   256,    32,   8,   8,   32,   32,    2,    4,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,             2,              8,              8,         1,           1,           1,               S<1, 32, 1, 8>,              8>,
        DeviceGemmMultipleD_Xdl_CShuffle<     Row,     Col,     Row,   F16,   F16,     F32,      F32, F16_F16,   F16, PassThrough, PassThrough, AddAddFastGelu,    GemmDefault,        1,   128,   128,   128,    32,   8,   8,   32,   32,    4,    2,     S<4, 32, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<4, 32, 1>,     S<1, 0, 2>,     S<1, 0, 2>,             2,              8,              8,         1,           1,           1,               S<1, 16, 1, 8>,              8>,
        DeviceGemmMultipleD_Xdl_CShuffle<     Row,     Col,     Row,   F16,   F16,     F32,      F32, F16_F16,   F16, PassThrough, PassThrough, AddAddFastGelu,    GemmDefault,        1,   256,   128,   128,    32,   8,   8,   32,   32,    2,    2,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,             2,              8,              8,         1,           1,           1,               S<1, 32, 1, 8>,              8>,
        DeviceGemmMultipleD_Xdl_CShuffle<     Row,     Col,     Row,   F16,   F16,     F32,      F32, F16_F16,   F16, PassThrough, PassThrough, AddAddFastGelu,    GemmDefault,        1,   128,   128,    64,    32,   8,   8,   32,   32,    2,    2,     S<4, 32, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<4, 32, 1>,     S<1, 0, 2>,     S<1, 0, 2>,             2,              8,              8,         1,           1,           1,               S<1, 32, 1, 4>,              8>,
        DeviceGemmMultipleD_Xdl_CShuffle<     Row,     Col,     Row,   F16,   F16,     F32,      F32, F16_F16,   F16, PassThrough, PassThrough, AddAddFastGelu,    GemmDefault,        1,   128,    64,   128,    32,   8,   8,   32,   32,    2,    2,     S<4, 32, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<4, 32, 1>,     S<1, 0, 2>,     S<1, 0, 2>,             2,              8,              8,         1,           1,           1,               S<1, 16, 1, 8>,              8>,
        DeviceGemmMultipleD_Xdl_CShuffle<     Row,     Col,     Row,   F16,   F16,     F32,      F32, F16_F16,   F16, PassThrough, PassThrough, AddAddFastGelu,    GemmDefault,        1,    64,    64,    64,    32,   8,   8,   32,   32,    2,    2,     S<4, 16, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<4, 16, 1>,     S<1, 0, 2>,     S<1, 0, 2>,             2,              8,              8,         1,           1,           1,               S<1, 16, 1, 4>,              8>,
        DeviceGemmMultipleD_Xdl_CShuffle<     Row,     Col,     Row,   F16,   F16,     F32,      F32, F16_F16,   F16, PassThrough, PassThrough, AddAddFastGelu,    GemmDefault,        1,   256,   128,    64,    32,   8,   8,   32,   32,    2,    1,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,             2,              8,              8,         1,           1,           1,               S<1, 32, 1, 8>,              8>,
        DeviceGemmMultipleD_Xdl_CShuffle<     Row,     Col,     Row,   F16,   F16,     F32,      F32, F16_F16,   F16, PassThrough, PassThrough, AddAddFastGelu,    GemmDefault,        1,   256,    64,   128,    32,   8,   8,   32,   32,    1,    2,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<4, 64, 1>,     S<1, 0, 2>,     S<1, 0, 2>,             2,              8,              8,         1,           1,           1,               S<1, 32, 1, 8>,              8>,
        DeviceGemmMultipleD_Xdl_CShuffle<     Row,     Col,     Row,   F16,   F16,     F32,      F32, F16_F16,   F16, PassThrough, PassThrough, AddAddFastGelu,    GemmDefault,        1,   128,   128,    32,    32,   8,   8,   32,   32,    2,    1,     S<4, 32, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<4, 32, 1>,     S<1, 0, 2>,     S<1, 0, 2>,             2,              8,              8,         1,           1,           1,               S<1, 32, 1, 4>,              8>,
        DeviceGemmMultipleD_Xdl_CShuffle<     Row,     Col,     Row,   F16,   F16,     F32,      F32, F16_F16,   F16, PassThrough, PassThrough, AddAddFastGelu,    GemmDefault,        1,   128,    32,   128,    32,   8,   8,   32,   32,    1,    2,     S<4, 32, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<4, 32, 1>,     S<1, 0, 2>,     S<1, 0, 2>,             2,              8,              8,         1,           1,           1,               S<1, 16, 1, 8>,              8>,
        DeviceGemmMultipleD_Xdl_CShuffle<     Row,     Col,     Row,   F16,   F16,     F32,      F32, F16_F16,   F16, PassThrough, PassThrough, AddAddFastGelu,    GemmDefault,        1,    64,    64,    32,    32,   8,   8,   32,   32,    2,    1,     S<4, 16, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<4, 16, 1>,     S<1, 0, 2>,     S<1, 0, 2>,             2,              8,              8,         1,           1,           1,               S<1, 16, 1, 4>,              8>,
        DeviceGemmMultipleD_Xdl_CShuffle<     Row,     Col,     Row,   F16,   F16,     F32,      F32, F16_F16,   F16, PassThrough, PassThrough, AddAddFastGelu,    GemmDefault,        1,    64,    32,    64,    32,   8,   8,   32,   32,    1,    2,     S<4, 16, 1>,     S<1, 0, 2>,     S<1, 0, 2>,              2,              8,              8,         1,     S<4, 16, 1>,     S<1, 0, 2>,     S<1, 0, 2>,             2,              8,              8,         1,           1,           1,               S<1, 16, 1, 4>,              8>
    // clang-format on
    >;

void add_device_gemm_add_add_fastgelu_xdl_c_shuffle_f16_f16_f16_mk_nk_mn_instances(
    std::vector<DeviceGemmMultipleDPtr<2, PassThrough, PassThrough, AddAddFastGelu>>& instances)
{
    add_device_operation_instances(
        instances, device_gemm_add_add_fastgelu_xdl_c_shuffle_f16_f16_f16_mk_nk_mn_instances{});
}

} // namespace device_gemm_instance
} // namespace device
} // namespace tensor_operation
} // namespace ck
