// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2022, Advanced Micro Devices, Inc. All rights reserved.

#include <iostream>
#include <numeric>
#include <initializer_list>
#include <cstdlib>

#include "profiler/include/profile_conv_bwd_data_impl.hpp"

namespace {

enum struct ConvLayout
{
    NCHW_KYXC_NKHW, // 0
    NHWC_KYXC_NHWK, // 1
};

enum struct ConvDataType
{
    F32_F32_F32,    // 0
    F16_F16_F16,    // 1
    BF16_BF16_BF16, // 2
    INT8_INT8_INT8, // 3
};

static void print_helper_msg()
{
    std::cout
        << "arg1: tensor operation (conv_bwd_data: Convolution Backward Data)\n"
        << "arg2: data type (0: Input fp32, Weight fp32, Output fp32\n"
        << "                 1: Input fp16, Weight fp16, Output fp16\n"
        << "                 2: Input bf16, Weight bf16, Output bf16\n"
        << "                 3: Input int8, Weight int8, Output int8)\n"
        << "arg3: tensor layout (0: Input[N, C, Hi, Wi], Weight[K, C, Y, X], Output[N, K, Ho, Wo]\n"
        << "                     1: Input[N, Hi, Wi, C], Weight[K, Y, X, C], Output[N, Ho, Wo, "
           "K])\n"
        << "arg4: verification (0: no, 1: yes)\n"
        << "arg5: initialization (0: no init, 1: integer value, 2: decimal value)\n"
        << "arg6: print tensor value (0: no; 1: yes)\n"
        << "arg7: time kernel (0: no, 1: yes)\n"
        << "arg8: N spatial dimensions\n"
        << "Following arguments (depending on number of spatial dims):\n"
        << " N, K, C, \n"
        << " <filter spatial dimensions>, (ie Y, X for 2D)\n"
        << " <input image spatial dimensions>, (ie Hi, Wi for 2D)\n"
        << " <strides>, (ie Sy, Sx for 2D)\n"
        << " <dilations>, (ie Dy, Dx for 2D)\n"
        << " <left padding>, (ie LeftPy, LeftPx for 2D)\n"
        << " <right padding>, (ie RightPy, RightPx for 2D)\n"
        << std::endl;
}

ck::utils::conv::ConvParam parse_conv_params(int num_dim_spatial, int arg_idx, char* const argv[])
{
    const ck::index_t N = std::stoi(argv[arg_idx++]);
    const ck::index_t K = std::stoi(argv[arg_idx++]);
    const ck::index_t C = std::stoi(argv[arg_idx++]);

    std::vector<ck::index_t> filter_spatial_lengths(num_dim_spatial);
    std::vector<ck::index_t> input_spatial_lengths(num_dim_spatial);
    std::vector<ck::index_t> conv_filter_strides(num_dim_spatial);
    std::vector<ck::index_t> conv_filter_dilations(num_dim_spatial);
    std::vector<ck::index_t> input_left_pads(num_dim_spatial);
    std::vector<ck::index_t> input_right_pads(num_dim_spatial);

    for(int i = 0; i < num_dim_spatial; ++i)
    {
        filter_spatial_lengths[i] = std::stoi(argv[arg_idx++]);
    }

    for(int i = 0; i < num_dim_spatial; ++i)
    {
        input_spatial_lengths[i] = std::stoi(argv[arg_idx++]);
    }

    for(int i = 0; i < num_dim_spatial; ++i)
    {
        conv_filter_strides[i] = std::stoi(argv[arg_idx++]);
    }

    for(int i = 0; i < num_dim_spatial; ++i)
    {
        conv_filter_dilations[i] = std::stoi(argv[arg_idx++]);
    }

    for(int i = 0; i < num_dim_spatial; ++i)
    {
        input_left_pads[i] = std::stoi(argv[arg_idx++]);
    }

    for(int i = 0; i < num_dim_spatial; ++i)
    {
        input_right_pads[i] = std::stoi(argv[arg_idx++]);
    }

    return ck::utils::conv::ConvParam{num_dim_spatial,
                                      N,
                                      K,
                                      C,
                                      filter_spatial_lengths,
                                      input_spatial_lengths,
                                      conv_filter_strides,
                                      conv_filter_dilations,
                                      input_left_pads,
                                      input_right_pads};
}

} // namespace

int profile_conv_bwd_data(int argc, char* argv[])
{
    // 8 for control, 1 for num_dim_spatial
    if(argc < 9)
    {
        print_helper_msg();
        return 1;
    }

    const auto data_type       = static_cast<ConvDataType>(std::stoi(argv[2]));
    const auto layout          = static_cast<ConvLayout>(std::stoi(argv[3]));
    const bool do_verification = std::stoi(argv[4]);
    const int init_method      = std::stoi(argv[5]);
    const bool do_log          = std::stoi(argv[6]);
    const bool time_kernel     = std::stoi(argv[7]);
    const int num_dim_spatial  = std::stoi(argv[8]);

    // 8 for control, 1 for num_dim_spatial, 3 for N/K/C, and 6 * num_dim_spatial
    if(argc != 8 + 4 + 6 * num_dim_spatial)
    {
        print_helper_msg();
        return 1;
    }

    const auto params = parse_conv_params(num_dim_spatial, 9, argv);

    using F32  = float;
    using F16  = ck::half_t;
    using BF16 = ck::bhalf_t;
    using INT8 = int8_t;

    using NWC   = ck::tensor_layout::convolution::NWC;
    using NHWC  = ck::tensor_layout::convolution::NHWC;
    using NDHWC = ck::tensor_layout::convolution::NDHWC;

    using KXC   = ck::tensor_layout::convolution::KXC;
    using KYXC  = ck::tensor_layout::convolution::KYXC;
    using KZYXC = ck::tensor_layout::convolution::KZYXC;

    using NWK   = ck::tensor_layout::convolution::NWK;
    using NHWK  = ck::tensor_layout::convolution::NHWK;
    using NDHWK = ck::tensor_layout::convolution::NDHWK;

    constexpr auto I1 = ck::Number<1>{};
    constexpr auto I2 = ck::Number<2>{};
    constexpr auto I3 = ck::Number<3>{};

    auto profile = [&](auto num_dim_spatial_tmp,
                       auto in_layout,
                       auto wei_layout,
                       auto out_layout,
                       auto in_type,
                       auto wei_type,
                       auto out_type) {
        constexpr ck::index_t NDimSpatial = num_dim_spatial_tmp.value;

        using InLayout  = decltype(in_layout);
        using WeiLayout = decltype(wei_layout);
        using OutLayout = decltype(out_layout);

        using InDataType  = decltype(in_type);
        using WeiDataType = decltype(wei_type);
        using OutDataType = decltype(out_type);

        bool pass = ck::profiler::profile_conv_bwd_data_impl<NDimSpatial,
                                                             InLayout,
                                                             WeiLayout,
                                                             OutLayout,
                                                             InDataType,
                                                             WeiDataType,
                                                             OutDataType>(
            do_verification, init_method, do_log, time_kernel, params);

        return pass ? 0 : 1;
    };

    if(num_dim_spatial == 1 && layout == ConvLayout::NHWC_KYXC_NHWK)
    {
        if(data_type == ConvDataType::F32_F32_F32)
        {
            return profile(I1, NWC{}, KXC{}, NWK{}, F32{}, F32{}, F32{});
        }
        else if(data_type == ConvDataType::F16_F16_F16)
        {
            return profile(I1, NWC{}, KXC{}, NWK{}, F16{}, F16{}, F16{});
        }
        else if(data_type == ConvDataType::BF16_BF16_BF16)
        {
            return profile(I1, NWC{}, KXC{}, NWK{}, BF16{}, BF16{}, BF16{});
        }
        else if(data_type == ConvDataType::INT8_INT8_INT8)
        {
            return profile(I1, NWC{}, KXC{}, NWK{}, INT8{}, INT8{}, INT8{});
        }
    }
    else if(num_dim_spatial == 2 && layout == ConvLayout::NHWC_KYXC_NHWK)
    {
        if(data_type == ConvDataType::F32_F32_F32)
        {
            return profile(I2, NHWC{}, KYXC{}, NHWK{}, F32{}, F32{}, F32{});
        }
        else if(data_type == ConvDataType::F16_F16_F16)
        {
            return profile(I2, NHWC{}, KYXC{}, NHWK{}, F16{}, F16{}, F16{});
        }
        else if(data_type == ConvDataType::BF16_BF16_BF16)
        {
            return profile(I2, NHWC{}, KYXC{}, NHWK{}, BF16{}, BF16{}, BF16{});
        }
        else if(data_type == ConvDataType::INT8_INT8_INT8)
        {
            return profile(I2, NHWC{}, KYXC{}, NHWK{}, INT8{}, INT8{}, INT8{});
        }
    }
    else if(num_dim_spatial == 3 && layout == ConvLayout::NHWC_KYXC_NHWK)
    {
        if(data_type == ConvDataType::F32_F32_F32)
        {
            return profile(I3, NDHWC{}, KZYXC{}, NDHWK{}, F32{}, F32{}, F32{});
        }
        else if(data_type == ConvDataType::F16_F16_F16)
        {
            return profile(I3, NDHWC{}, KZYXC{}, NDHWK{}, F16{}, F16{}, F16{});
        }
        else if(data_type == ConvDataType::BF16_BF16_BF16)
        {
            return profile(I3, NDHWC{}, KZYXC{}, NDHWK{}, BF16{}, BF16{}, BF16{});
        }
        else if(data_type == ConvDataType::INT8_INT8_INT8)
        {
            return profile(I3, NDHWC{}, KZYXC{}, NDHWK{}, INT8{}, INT8{}, INT8{});
        }
    }

    std::cout << "this data_type & layout is not implemented" << std::endl;

    return 1;
}
