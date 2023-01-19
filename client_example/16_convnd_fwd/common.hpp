// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2023, Advanced Micro Devices, Inc. All rights reserved.

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <numeric>
#include <string>
#include <vector>

#include "ck/ck.hpp"
#include "ck/library/tensor_operation_instance/gpu/grouped_convolution_forward.hpp"
#include "ck/tensor_operation/gpu/device/device_grouped_conv_fwd_multiple_d.hpp"
#include "ck/tensor_operation/gpu/element/element_wise_operation.hpp"

using PassThrough = ck::tensor_operation::element_wise::PassThrough;

struct SimpleDeviceMem
{
    SimpleDeviceMem() = delete;

    SimpleDeviceMem(std::size_t mem_size) : p_mem_{}
    {
        (void)hipMalloc(static_cast<void**>(&p_mem_), mem_size);
    }

    void* GetDeviceBuffer() { return p_mem_; }

    ~SimpleDeviceMem() { (void)hipFree(p_mem_); }

    void* p_mem_;
};

template <ck::index_t NumDimSpatial>
std::size_t GetFlops(const std::array<ck::index_t, NumDimSpatial + 3>& output_lengths,
                     const std::array<ck::index_t, NumDimSpatial + 3>& weights_lengths)
{
    // 2 * G * N * K * C * <output spatial lengths product> * <filter spatial lengths product>

    ck::index_t G = weights_lengths[0];
    ck::index_t N = output_lengths[1];
    ck::index_t K = weights_lengths[1];
    ck::index_t C = weights_lengths[2];

    return static_cast<std::size_t>(2) * G * N * K * C *
           std::accumulate(std::next(std::begin(output_lengths), 3),
                           std::end(output_lengths),
                           static_cast<std::size_t>(1),
                           std::multiplies<>()) *
           std::accumulate(std::next(std::begin(weights_lengths), 3),
                           std::end(weights_lengths),
                           static_cast<std::size_t>(1),
                           std::multiplies<>());
}

template <typename InDataType, ck::index_t NumDimSpatial>
std::size_t GetInputByte(const std::array<ck::index_t, NumDimSpatial + 3>& input_lengths)
{
    // sizeof(InDataType) * (G * N * C * <input spatial lengths product>) +
    return sizeof(InDataType) * std::accumulate(std::begin(input_lengths),
                                                std::end(input_lengths),
                                                static_cast<std::size_t>(1),
                                                std::multiplies<>());
}

template <typename WeiDataType, ck::index_t NumDimSpatial>
std::size_t GetWeightByte(const std::array<ck::index_t, NumDimSpatial + 3>& weights_lengths)
{
    // sizeof(WeiDataType) * (G * K * C * <filter spatial lengths product>) +
    return sizeof(WeiDataType) * std::accumulate(std::begin(weights_lengths),
                                                 std::end(weights_lengths),
                                                 static_cast<std::size_t>(1),
                                                 std::multiplies<>());
}

template <typename OutDataType, ck::index_t NumDimSpatial>
std::size_t GetOutputByte(const std::array<ck::index_t, NumDimSpatial + 3>& output_lengths)
{
    // sizeof(OutDataType) * (G * N * K * <output spatial lengths product>);
    return sizeof(OutDataType) * std::accumulate(std::begin(output_lengths),
                                                 std::end(output_lengths),
                                                 static_cast<std::size_t>(1),
                                                 std::multiplies<std::size_t>());
}

template <typename T, ck::index_t NumDimSpatial>
void print_array(const std::array<T, NumDimSpatial + 3>& a)
{
    for(int i = 0; i < NumDimSpatial + 3; ++i)
    {
        std::cout << a[i] << ", ";
    }
    std::cout << std::endl;
}

template <ck::index_t NumDimSpatial,
          typename InDataType,
          typename WeiDataType,
          typename OutDataType,
          typename InLayout,
          typename WeiLayout,
          typename OutLayout>
bool run_grouped_conv_fwd(std::array<ck::index_t, NumDimSpatial + 3> in_lengths,
                          std::array<ck::index_t, NumDimSpatial + 3> wei_lengths,
                          std::array<ck::index_t, NumDimSpatial + 3> out_lengths)
{
    std::size_t in_mem_size  = GetInputByte<InDataType, NumDimSpatial>(in_lengths);
    std::size_t wei_mem_size = GetWeightByte<WeiDataType, NumDimSpatial>(wei_lengths);
    std::size_t out_mem_size = GetOutputByte<OutDataType, NumDimSpatial>(out_lengths);

    SimpleDeviceMem in(in_mem_size);
    SimpleDeviceMem wei(wei_mem_size);
    SimpleDeviceMem out(out_mem_size);

    std::array<ck::index_t, NumDimSpatial + 3> in_strides;
    std::array<ck::index_t, NumDimSpatial + 3> wei_strides;
    std::array<ck::index_t, NumDimSpatial + 3> out_strides;
    in_strides.fill(0);
    wei_strides.fill(0);
    out_strides.fill(0);
    in_strides.back()  = 1;
    wei_strides.back() = 1;
    out_strides.back() = 1;

    std::partial_sum(rbegin(in_lengths),
                     std::prev(rend(in_lengths)),
                     std::next(rbegin(in_strides)),
                     std::multiplies<>{});
    std::partial_sum(rbegin(wei_lengths),
                     std::prev(rend(wei_lengths)),
                     std::next(rbegin(wei_strides)),
                     std::multiplies<>{});
    std::partial_sum(rbegin(out_lengths),
                     std::prev(rend(out_lengths)),
                     std::next(rbegin(out_strides)),
                     std::multiplies<>{});

    // transpose GNDHWC/GKZYXC/GNDHWK to GNCDHW/GKCZYX/GNKDHW
    std::rotate(rbegin(in_lengths),
                std::next(rbegin(in_lengths)),
                std::next(rbegin(in_lengths), NumDimSpatial + 1));
    std::rotate(rbegin(in_strides),
                std::next(rbegin(in_strides)),
                std::next(rbegin(in_strides), NumDimSpatial + 1));
    std::rotate(rbegin(wei_lengths),
                std::next(rbegin(wei_lengths)),
                std::next(rbegin(wei_lengths), NumDimSpatial + 1));
    std::rotate(rbegin(wei_strides),
                std::next(rbegin(wei_strides)),
                std::next(rbegin(wei_strides), NumDimSpatial + 1));
    std::rotate(rbegin(out_lengths),
                std::next(rbegin(out_lengths)),
                std::next(rbegin(out_lengths), NumDimSpatial + 1));
    std::rotate(rbegin(out_strides),
                std::next(rbegin(out_strides)),
                std::next(rbegin(out_strides), NumDimSpatial + 1));

    std::array<ck::index_t, NumDimSpatial> conv_filter_strides;
    std::array<ck::index_t, NumDimSpatial> conv_filter_dilations;
    std::array<ck::index_t, NumDimSpatial> input_left_pads;
    std::array<ck::index_t, NumDimSpatial> input_right_pads;
    conv_filter_strides.fill(1);
    conv_filter_dilations.fill(1);
    input_left_pads.fill(1);
    input_right_pads.fill(1);

    print_array<ck::index_t, NumDimSpatial>(in_lengths);
    print_array<ck::index_t, NumDimSpatial>(in_strides);
    print_array<ck::index_t, NumDimSpatial>(wei_lengths);
    print_array<ck::index_t, NumDimSpatial>(wei_strides);
    print_array<ck::index_t, NumDimSpatial>(out_lengths);
    print_array<ck::index_t, NumDimSpatial>(out_strides);

    std::size_t flop      = GetFlops<NumDimSpatial>(out_lengths, wei_lengths);
    std::size_t num_bytes = in_mem_size + wei_mem_size + out_mem_size;

    using DeviceOp = ck::tensor_operation::device::DeviceGroupedConvFwdMultipleD<NumDimSpatial,
                                                                                 InLayout,
                                                                                 WeiLayout,
                                                                                 ck::Tuple<>,
                                                                                 OutLayout,
                                                                                 InDataType,
                                                                                 WeiDataType,
                                                                                 ck::Tuple<>,
                                                                                 OutDataType,
                                                                                 PassThrough,
                                                                                 PassThrough,
                                                                                 PassThrough>;
    // get device op instances
    const auto op_ptrs = ck::tensor_operation::device::instance::DeviceOperationInstanceFactory<
        DeviceOp>::GetInstances();

    std::cout << "found " << op_ptrs.size() << " instances" << std::endl;

    std::string best_op_name;
    int best_op_id        = -1;
    float best_avg_time   = std::numeric_limits<float>::max();
    float best_gb_per_sec = 0;
    float best_tflops     = 0;

    // profile device operation instances
    std::cout << "Run all instances and do timing" << std::endl;

    for(int i = 0; i < op_ptrs.size(); ++i)
    {
        auto& op_ptr      = op_ptrs[i];
        auto argument_ptr = op_ptr->MakeArgumentPointer(
            in.GetDeviceBuffer(),
            wei.GetDeviceBuffer(),
            std::array<const void*, 0>{},
            out.GetDeviceBuffer(),
            in_lengths,
            in_strides,
            wei_lengths,
            wei_strides,
            std::array<std::array<ck::index_t, NumDimSpatial + 3>, 0>{{}},
            std::array<std::array<ck::index_t, NumDimSpatial + 3>, 0>{{}},
            out_lengths,
            out_strides,
            conv_filter_strides,
            conv_filter_dilations,
            input_left_pads,
            input_right_pads,
            PassThrough{},
            PassThrough{},
            PassThrough{});

        auto invoker_ptr    = op_ptr->MakeInvokerPointer();
        std::string op_name = op_ptr->GetTypeString();

        if(op_ptr->IsSupportedArgument(argument_ptr.get()))
        {
            float avg_time = invoker_ptr->Run(argument_ptr.get(), StreamConfig{nullptr, true});

            float tflops     = static_cast<float>(flop) / 1.E9 / avg_time;
            float gb_per_sec = num_bytes / 1.E6 / avg_time;

            std::cout << "Perf: " << std::setw(10) << avg_time << " ms, " << tflops << " TFlops, "
                      << gb_per_sec << " GB/s, " << op_name << std::endl;

            if(tflops > best_tflops)
            {
                best_op_id      = i;
                best_op_name    = op_name;
                best_avg_time   = avg_time;
                best_gb_per_sec = gb_per_sec;
                best_tflops     = tflops;
            }
        }
        else
        {
            std::cerr << op_name << " does not support this problem" << std::endl;
        }
    }

    if(best_op_id < 0)
    {
        std::cerr << "no suitable instance" << std::endl;
        return false;
    }

    std::cout << "Best Perf: " << std::setw(10) << best_avg_time << " ms, " << best_tflops
              << " TFlops, " << best_gb_per_sec << " GB/s, " << best_op_name << std::endl;

    // run the best intance
    {
        auto& op_ptr = op_ptrs[best_op_id];
        std::cout << "Run the best instance without timing: " << op_ptr->GetTypeString()
                  << std::endl;
        auto argument_ptr = op_ptr->MakeArgumentPointer(
            in.GetDeviceBuffer(),
            wei.GetDeviceBuffer(),
            std::array<const void*, 0>{},
            out.GetDeviceBuffer(),
            in_lengths,
            in_strides,
            wei_lengths,
            wei_strides,
            std::array<std::array<ck::index_t, NumDimSpatial + 3>, 0>{{}},
            std::array<std::array<ck::index_t, NumDimSpatial + 3>, 0>{{}},
            out_lengths,
            out_strides,
            conv_filter_strides,
            conv_filter_dilations,
            input_left_pads,
            input_right_pads,
            PassThrough{},
            PassThrough{},
            PassThrough{});

        auto invoker_ptr = op_ptr->MakeInvokerPointer();

        if(op_ptr->IsSupportedArgument(argument_ptr.get()))
        {
            invoker_ptr->Run(argument_ptr.get(), StreamConfig{nullptr, false});
        }

        std::cout << "Done" << std::endl;
    }
    return true;
}
