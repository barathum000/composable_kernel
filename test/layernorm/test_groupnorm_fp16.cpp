// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2022, Advanced Micro Devices, Inc. All rights reserved.

#include "gtest/gtest.h"
#include "test_groupnorm_util.hpp"

template <ck::index_t N>
using I = ck::Number<N>;

template <typename Tuple>
class TestGroupnormFP16 : public ck::TestGroupnorm<Tuple>
{
};

// clang-format off
using KernelTypes = ::testing::Types<
//  XDataType, GammaDataType, BetaDataType, AccDataType, YDataType, Rank, NumReduceDim, BlockSize, MThreadClusterSize, KThreadClusterSize, MThreadSliceSize, KThreadSliceSize, XYSrcVectorDim, XSrcVectorSize, GammaSrcVectorDim , GammaSrcVectorSize, BetaSrcVectorDim, BetaSrcVectorSize, YDstVectorSize>
    std::tuple<ck::half_t, ck::half_t, ck::half_t, float, ck::half_t, I<5>, I<3>, I<256>, I<8>, I<32>, I<1>, I<8>, I<1>, I<8>, I<1>, I<8>, I<1>, I<8>, I<8>>,
    std::tuple<ck::half_t, ck::half_t, ck::half_t, float, ck::half_t, I<5>, I<3>, I<256>, I<8>, I<32>, I<2>, I<8>, I<1>, I<8>, I<1>, I<8>, I<1>, I<8>, I<8>>,
    std::tuple<ck::half_t, ck::half_t, ck::half_t, float, ck::half_t, I<5>, I<3>, I<256>, I<4>, I<64>, I<1>, I<8>, I<1>, I<8>, I<1>, I<8>, I<1>, I<8>, I<8>>,
    std::tuple<ck::half_t, ck::half_t, ck::half_t, float, ck::half_t, I<5>, I<3>, I<256>, I<4>, I<64>, I<2>, I<8>, I<1>, I<8>, I<1>, I<8>, I<1>, I<8>, I<8>>,
    std::tuple<ck::half_t, ck::half_t, ck::half_t, float, ck::half_t, I<5>, I<3>, I<256>, I<2>, I<128>, I<1>, I<8>, I<1>, I<8>, I<1>, I<8>, I<1>, I<8>, I<8>>,
    std::tuple<ck::half_t, ck::half_t, ck::half_t, float, ck::half_t, I<5>, I<3>, I<256>, I<2>, I<128>, I<2>, I<8>, I<1>, I<8>, I<1>, I<8>, I<1>, I<8>, I<8>>,
    std::tuple<ck::half_t, ck::half_t, ck::half_t, float, ck::half_t, I<5>, I<3>, I<256>, I<1>, I<256>, I<1>, I<8>, I<1>, I<8>, I<1>, I<8>, I<1>, I<8>, I<8>>,
    std::tuple<ck::half_t, ck::half_t, ck::half_t, float, ck::half_t, I<5>, I<3>, I<256>, I<1>, I<256>, I<2>, I<8>, I<1>, I<8>, I<1>, I<8>, I<1>, I<8>, I<8>>
    >;
// clang-format on
TYPED_TEST_SUITE(TestGroupnormFP16, KernelTypes);
TYPED_TEST(TestGroupnormFP16, Test_FP16) { this->Run(); }
