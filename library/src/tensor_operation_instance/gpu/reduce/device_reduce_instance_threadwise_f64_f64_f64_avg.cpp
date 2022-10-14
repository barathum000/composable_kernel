// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2022, Advanced Micro Devices, Inc. All rights reserved.

#include "ck/utility/reduction_enums.hpp"
#include "ck/library/tensor_operation_instance/gpu/reduce/device_reduce_instance_threadwise.hpp"

namespace ck {
namespace tensor_operation {
namespace device {
namespace instance {

// clang-format off
// InDataType | AccDataType | OutDataType | ReduceOpId | PropagateNan | UseIndex | Rank | NumReduceDim
ADD_THREADWISE_INST(double, double, double, ReduceTensorOp::AVG, false, false, 4, 3); // for AVG
ADD_THREADWISE_INST(double, double, double, ReduceTensorOp::AVG, false, false, 4, 4);       
ADD_THREADWISE_INST(double, double, double, ReduceTensorOp::AVG, false, false, 4, 1);       
ADD_THREADWISE_INST(double, double, double, ReduceTensorOp::AVG, false, false, 2, 1);
// clang-format on

} // namespace instance
} // namespace device
} // namespace tensor_operation
} // namespace ck
