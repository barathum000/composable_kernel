// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2022, Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include <iostream>
#include <vector>

#include "device_base.hpp"

namespace ck {
namespace tensor_operation {
namespace device {

template <typename A0Layout,
          typename B0Layout,
          typename D0sLayout,
          typename B1Layout,
          typename C1Layout,
          typename D1sLayout,
          typename A0DataType,
          typename B0DataType,
          typename D0sDataType,
          typename B1DataType,
          typename C1DataType,
          typename D1sDataType,
          typename A0ElementwiseOperation,
          typename B0ElementwiseOperation,
          typename Acc0ElementwiseOperation,
          typename D0ElementwiseOperation,
          typename B1ElementwiseOperation,
          typename C1ElementwiseOperation,
          typename D1ElementwiseOperation>
struct DeviceBatchedGemmBiasGeluGemmBias : public BaseOperator
{
    static constexpr index_t NumD1Tensor = D1sDataType::Size();

    virtual std::unique_ptr<BaseArgument>
    MakeArgumentPointer(const void* p_a,
                        const void* p_b0,
                        std::array<const void*, NumD1Tensor> p_d0s,
                        const void* p_b1,
                        void* p_c,
                        std::array<const void*, NumD1Tensor> p_d1s,
                        ck::index_t M,
                        ck::index_t N,
                        ck::index_t K,
                        ck::index_t O,
                        ck::index_t Batch,
                        ck::index_t StrideA,
                        ck::index_t StrideB0,
                        std::array<ck::index_t, NumD1Tensor> StrideD0s,
                        ck::index_t StrideB1,
                        ck::index_t StrideC,
                        std::array<ck::index_t, NumD1Tensor> StrideD1s,
                        ck::index_t BatchStrideA0,
                        ck::index_t BatchStrideB0,
                        std::array<ck::index_t, NumD1Tensor> BatchStrideD0s,
                        ck::index_t BatchStrideB1,
                        ck::index_t BatchStrideC1,
                        std::array<ck::index_t, NumD1Tensor> BatchStrideD1s,
                        A0ElementwiseOperation a0_element_op,
                        B0ElementwiseOperation b0_element_op,
                        Acc0ElementwiseOperation acc0_element_op,
                        D0ElementwiseOperation d0_element_op,
                        B1ElementwiseOperation b1_element_op,
                        C1ElementwiseOperation c1_element_op,
                        D1ElementwiseOperation d1_element_op) = 0;

    virtual std::unique_ptr<BaseInvoker> MakeInvokerPointer() = 0;
};

} // namespace device
} // namespace tensor_operation
} // namespace ck
