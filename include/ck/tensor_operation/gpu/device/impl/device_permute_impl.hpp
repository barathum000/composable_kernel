// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2022, Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include <array>
#include <memory>
#include <utility>

#include "ck/utility/math.hpp"
#include "ck/utility/sequence.hpp"
#include "ck/tensor_operation/gpu/device/device_base.hpp"
#include "ck/tensor_operation/gpu/device/device_permute.hpp"
#include "ck/tensor_operation/gpu/device/matrix_padder.hpp"
#include "ck/tensor_operation/gpu/grid/gridwise_permute.hpp"
#include "ck/tensor_description/tensor_descriptor_helper.hpp"

#include "ck/host_utility/kernel_launch.hpp"

namespace ck {
namespace tensor_operation {
namespace device {

// Swap last 2 dimensions
// input shape: [d[0], d[1], d[2], ..., d[NumDim-3], d[NumDim-2], d[NumDim-1]]
//                                                                ^^^^^^^^^^^
// output shape: [d[0], d[1], d[2], ..., d[NumDim-3], d[NumDim-1], d[NumDim-2]]
//                                                    ^^^^^^^^^^^
template <index_t NumDim,
          typename InDataType,
          typename OutDataType,
          typename ElementwiseOperation,
          index_t BlockSize,
          index_t NPerBlock,
          index_t HPerBlock,
          index_t WPerBlock,
          index_t InBlockLdsExtraW,
          typename InBlockTransferThreadClusterLengths,
          typename InBlockTransferThreadClusterArrangeOrder,
          index_t SrcVectorDim,
          index_t DstVectorDim,
          index_t SrcScalarPerVector,
          index_t DstScalarPerVector>
struct DevicePermuteImpl
    : DevicePermuteCRTP<NumDim,
                        InDataType,
                        OutDataType,
                        ElementwiseOperation,
                        DevicePermuteImpl<NumDim,
                                          InDataType,
                                          OutDataType,
                                          ElementwiseOperation,
                                          BlockSize,
                                          NPerBlock,
                                          HPerBlock,
                                          WPerBlock,
                                          InBlockLdsExtraW,
                                          InBlockTransferThreadClusterLengths,
                                          InBlockTransferThreadClusterArrangeOrder,
                                          SrcVectorDim,
                                          DstVectorDim,
                                          SrcScalarPerVector,
                                          DstScalarPerVector>>
{
    static_assert(3 <= NumDim, "Only accept at least 3D dimension tensor");
    static_assert((NumDim - 2) <= SrcVectorDim && SrcVectorDim < NumDim);
    static_assert((NumDim - 2) <= DstVectorDim && DstVectorDim < NumDim);
    static_assert(SrcVectorDim != DstVectorDim);

    template <index_t N = NumDim>
    static auto ConvertArrayToTuple(const std::array<index_t, NumDim>& array)
    {
        static_assert(1 <= N && N <= NumDim);

        return generate_tuple([&](auto I) { return array[I]; }, Number<N>{});
    }

    static auto MakeDescriptor_N_H_W(const std::array<index_t, NumDim>& lengths,
                                     const std::array<index_t, NumDim>& stride)
    {
        // create nd descriptor, shape: [d[0], d[1], d[2], ..., d[NumDim-3], d[NumDim-2],
        // d[NumDim-1]]
        const auto desc =
            make_naive_tensor_descriptor(ConvertArrayToTuple(lengths), ConvertArrayToTuple(stride));

        // merge nd to 3d descriptor, shape: [(d[0] * d[1] * d[2] * ... * d[NumDim-3]), d[NumDim-2],
        // d[NumDim-1]]
        //                                   => [N, H, W]
        const index_t H       = *std::next(rbegin(lengths));
        const index_t W       = *rbegin(lengths);
        const auto desc_n_h_w = transform_tensor_descriptor(
            desc,
            make_tuple(make_merge_transform(ConvertArrayToTuple<NumDim - 2>(lengths)),
                       make_pass_through_transform(H),
                       make_pass_through_transform(W)),
            make_tuple(generate_sequence_v2([&](auto I) { return I; }, Number<NumDim - 2>{}),
                       Sequence<NumDim - 2>{},
                       Sequence<NumDim - 1>{}),
            make_tuple(Sequence<0>{}, Sequence<1>{}, Sequence<2>{}));

        return PadTensorDescriptor(
            desc_n_h_w, make_tuple(NPerBlock, HPerBlock, WPerBlock), Sequence<true, true, true>{});
    }

    using InGridDesc  = decltype(MakeDescriptor_N_H_W({1, 1}, {1, 1}));
    using OutGridDesc = InGridDesc;

    using GridwisePermute = GridwisePermute<
        InGridDesc,
        OutGridDesc,
        InDataType,
        OutDataType,
        ElementwiseOperation,
        BlockSize,
        NPerBlock,
        HPerBlock,
        WPerBlock,
        InBlockLdsExtraW,
        InBlockTransferThreadClusterLengths,
        InBlockTransferThreadClusterArrangeOrder,
        SrcVectorDim - (NumDim - 3), // calculate new SrcVectorDim for the merged descriptor
        DstVectorDim - (NumDim - 3), // calculate new DstVectorDim for the merged descriptor
        SrcScalarPerVector,
        DstScalarPerVector>;

    struct Argument : public BaseArgument
    {
        Argument(const std::array<index_t, NumDim> inLengths,
                 const std::array<index_t, NumDim> inStrides,
                 const std::array<index_t, NumDim> outLengths,
                 const std::array<index_t, NumDim> outStrides,
                 const void* in_dev_buffer,
                 void* out_dev_buffer,
                 ElementwiseOperation elementwise_op)
            : in_dev_buffer_(static_cast<const InDataType*>(in_dev_buffer)),
              out_dev_buffer_(static_cast<OutDataType*>(out_dev_buffer)),
              in_grid_desc_(MakeDescriptor_N_H_W(inLengths, inStrides)),
              out_grid_desc_(MakeDescriptor_N_H_W(outLengths, outStrides)),
              inLengths_(inLengths),
              inStrides_(inStrides),
              outLengths_(outLengths),
              outStrides_(outStrides),
              elementwise_op_(elementwise_op),
              block_2_tile_map_(GridwisePermute::MakeDefaultBlock2TileMap(in_grid_desc_))
        {
        }

        const InDataType* in_dev_buffer_;
        OutDataType* out_dev_buffer_;
        InGridDesc in_grid_desc_;
        OutGridDesc out_grid_desc_;

        std::array<index_t, NumDim> inLengths_;
        std::array<index_t, NumDim> inStrides_;
        std::array<index_t, NumDim> outLengths_;
        std::array<index_t, NumDim> outStrides_;

        ElementwiseOperation elementwise_op_;

        typename GridwisePermute::DefaultBlock2TileMap block_2_tile_map_;
    };

    struct Invoker : BaseInvokerCRTP<Invoker, Argument>
    {
        static float Run(const Argument& arg, const StreamConfig& stream_config = StreamConfig{})
        {
            const index_t grid_size = arg.block_2_tile_map_.CalculateGridSize(arg.in_grid_desc_);

            const auto kernel = kernel_nd_permute<GridwisePermute,
                                                  InGridDesc,
                                                  OutGridDesc,
                                                  InDataType,
                                                  OutDataType,
                                                  ElementwiseOperation,
                                                  typename GridwisePermute::DefaultBlock2TileMap>;

            float elapsed_time = launch_and_time_kernel(stream_config,
                                                        kernel,
                                                        dim3(grid_size),
                                                        dim3(BlockSize),
                                                        0,
                                                        arg.in_grid_desc_,
                                                        arg.out_grid_desc_,
                                                        arg.in_dev_buffer_,
                                                        arg.out_dev_buffer_,
                                                        arg.elementwise_op_,
                                                        arg.block_2_tile_map_);
            return elapsed_time;
        }
    };

    static bool IsSupportedArgument(const Argument& arg)
    {
        constexpr auto GetPaddedLength = [](index_t length, index_t tile_length) {
            return math::integer_divide_ceil(length, tile_length) * tile_length;
        };

        constexpr auto IsScalarPerVectorValid =
            [](index_t length, index_t stride, index_t scalar_per_vector) {
                if(stride == 1 && length % scalar_per_vector == 0)
                {
                    return true;
                }
                else if(stride != 1 && scalar_per_vector == 1)
                {
                    return true;
                }

                return false;
            };

        return IsScalarPerVectorValid(arg.inLengths_[SrcVectorDim],
                                      arg.inStrides_[SrcVectorDim],
                                      SrcScalarPerVector) &&
               IsScalarPerVectorValid(
                   GetPaddedLength(arg.inLengths_[SrcVectorDim],
                                   (SrcVectorDim == NumDim - 2 ? HPerBlock : WPerBlock)),
                   arg.inStrides_[SrcVectorDim],
                   SrcScalarPerVector) &&
               IsScalarPerVectorValid(arg.outLengths_[DstVectorDim],
                                      arg.outStrides_[DstVectorDim],
                                      DstScalarPerVector) &&
               IsScalarPerVectorValid(
                   GetPaddedLength(arg.outLengths_[DstVectorDim],
                                   (DstVectorDim == NumDim - 2 ? HPerBlock : WPerBlock)),
                   arg.inStrides_[DstVectorDim],
                   DstScalarPerVector) &&
               GridwisePermute::CheckValidity(arg.in_grid_desc_, arg.out_grid_desc_);
    };
};

} // namespace device
} // namespace tensor_operation
} // namespace ck
