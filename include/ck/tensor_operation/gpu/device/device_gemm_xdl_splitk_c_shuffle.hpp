#ifndef DEVICE_GEMM_XDL_SPLITK_C_SHUFFLE_HPP
#define DEVICE_GEMM_XDL_SPLITK_C_SHUFFLE_HPP

#include <iostream>
#include <sstream>
#include "device.hpp"
#include "device_base.hpp"
#include "device_gemm.hpp"
#include "common_header.hpp"
#include "device_gemm_xdl_splitk.hpp"
#include "tensor_layout.hpp"
#include "tensor_descriptor.hpp"
#include "tensor_descriptor_helper.hpp"
#include "gridwise_gemm_xdl_cshuffle_v1.hpp"
#include "gemm_specialization.hpp"
#include "batched_gemm_util.hpp"

namespace ck {
namespace tensor_operation {
namespace device {

/*
 * \brief Wrapper function of GridwiseGemm::Run to realize a customized BatchedGemm for splitK.
 *
 * The main difference from \see \link device_batched_gemm_xdl.hpp kernel_batched_gemm_xdlops_v2r3
 * is that there are 2 different tensor descriptors for matrix A and B.
 */
template <typename GridwiseGemm,
          typename FloatAB,
          typename FloatC,
          typename AElementwiseOperation,
          typename BElementwiseOperation,
          typename CElementwiseOperation,
          typename AGridDesc_AK0_M_AK1,
          typename BGridDesc_BK0_N_BK1,
          typename AGridDesc_AK0_M_AK1_Tail,
          typename BGridDesc_BK0_N_BK1_Tail,
          typename CGridDescriptor_MBlock_MPerBlock_NBlock_NPerBlock,
          typename ComputePtrOffsetOfBatch,
          typename Block2CTileMap,
          bool HasMainKBlockLoop,
          bool TailHasMainKBlockLoop>
__global__ void
#if CK_USE_LAUNCH_BOUNDS
    __launch_bounds__(CK_MAX_THREAD_PER_BLOCK, CK_MIN_BLOCK_PER_CU)
#endif
        kernel_batched_gemm_xdl_cshuffle_v1(
            const FloatAB* __restrict__ p_a_grid,
            const FloatAB* __restrict__ p_b_grid,
            FloatC* __restrict__ p_c_grid,
            const index_t batch_count,
            const AElementwiseOperation a_element_op,
            const BElementwiseOperation b_element_op,
            const CElementwiseOperation c_element_op,
            const AGridDesc_AK0_M_AK1 a_grid_desc_ak0_m_ak1,
            const BGridDesc_BK0_N_BK1 b_grid_desc_bk0_n_bk1,
            const AGridDesc_AK0_M_AK1_Tail a_grid_desc_ak0_m_ak1_tail,
            const BGridDesc_BK0_N_BK1_Tail b_grid_desc_bk0_n_bk1_tail,
            const CGridDescriptor_MBlock_MPerBlock_NBlock_NPerBlock
                c_grid_desc_mblock_mperblock_nblock_nperblock,
            const ComputePtrOffsetOfBatch compute_ptr_offset_of_batch,
            const Block2CTileMap block_2_ctile_map)
{
#if(!defined(__HIP_DEVICE_COMPILE__) || defined(__gfx908__) || defined(__gfx90a__))
    const index_t num_blocks_per_batch =
        __builtin_amdgcn_readfirstlane(get_grid_size() / batch_count);
    const index_t g_idx = __builtin_amdgcn_readfirstlane(get_block_1d_id() / num_blocks_per_batch);

    const long_index_t a_batch_offset = __builtin_amdgcn_readfirstlane(
        static_cast<long_index_t>(compute_ptr_offset_of_batch.GetAPtrOffset(g_idx)));
    const long_index_t b_batch_offset = __builtin_amdgcn_readfirstlane(
        static_cast<long_index_t>(compute_ptr_offset_of_batch.GetBPtrOffset(g_idx)));
    const long_index_t c_batch_offset = __builtin_amdgcn_readfirstlane(
        static_cast<long_index_t>(compute_ptr_offset_of_batch.GetCPtrOffset(g_idx)));

    __shared__ char p_shared[GridwiseGemm::GetSharedMemoryNumberOfByte()];

    if(g_idx < batch_count - 1)
    {
        GridwiseGemm::template Run<HasMainKBlockLoop>(p_a_grid + a_batch_offset,
                                                      p_b_grid + b_batch_offset,
                                                      p_c_grid + c_batch_offset,
                                                      p_shared,
                                                      a_element_op,
                                                      b_element_op,
                                                      c_element_op,
                                                      a_grid_desc_ak0_m_ak1,
                                                      b_grid_desc_bk0_n_bk1,
                                                      c_grid_desc_mblock_mperblock_nblock_nperblock,
                                                      block_2_ctile_map);
    }
    else
    {
        GridwiseGemm::template Run<TailHasMainKBlockLoop>(
            p_a_grid + a_batch_offset,
            p_b_grid + b_batch_offset,
            p_c_grid + c_batch_offset,
            p_shared,
            a_element_op,
            b_element_op,
            c_element_op,
            a_grid_desc_ak0_m_ak1_tail,
            b_grid_desc_bk0_n_bk1_tail,
            c_grid_desc_mblock_mperblock_nblock_nperblock,
            block_2_ctile_map);
    }
#else
    ignore = p_a_grid;
    ignore = p_b_grid;
    ignore = p_c_grid;
    ignore = batch_count;
    ignore = a_element_op;
    ignore = b_element_op;
    ignore = c_element_op;
    ignore = a_grid_desc_ak0_m_ak1;
    ignore = b_grid_desc_bk0_n_bk1;
    ignore = a_grid_desc_ak0_m_ak1_tail;
    ignore = b_grid_desc_bk0_n_bk1_tail;
    ignore = c_grid_desc_mblock_mperblock_nblock_nperblock;
    ignore = compute_ptr_offset_of_batch;
    ignore = block_2_ctile_map;
#endif // end of if (defined(__gfx908__) || defined(__gfx90a__))
}

template <typename ALayout,
          typename BLayout,
          typename CLayout,
          typename ADataType,
          typename BDataType,
          typename CDataType,
          typename GemmAccDataType,
          typename CShuffleDataType,
          typename AElementwiseOperation,
          typename BElementwiseOperation,
          typename CElementwiseOperation,
          GemmSpecialization GemmSpec,
          index_t NumGemmKPrefetchStage,
          index_t BlockSize,
          index_t MPerBlock,
          index_t NPerBlock,
          index_t KPerBlock,
          index_t AK1,
          index_t BK1,
          index_t MPerXDL,
          index_t NPerXDL,
          index_t MXdlPerWave,
          index_t NXdlPerWave,
          typename ABlockTransferThreadClusterLengths_AK0_M_AK1,
          typename ABlockTransferThreadClusterArrangeOrder,
          typename ABlockTransferSrcAccessOrder,
          index_t ABlockTransferSrcVectorDim,
          index_t ABlockTransferSrcScalarPerVector,
          index_t ABlockTransferDstScalarPerVector_AK1,
          bool ABlockLdsExtraM,
          typename BBlockTransferThreadClusterLengths_BK0_N_BK1,
          typename BBlockTransferThreadClusterArrangeOrder,
          typename BBlockTransferSrcAccessOrder,
          index_t BBlockTransferSrcVectorDim,
          index_t BBlockTransferSrcScalarPerVector,
          index_t BBlockTransferDstScalarPerVector_BK1,
          bool BBlockLdsExtraN,
          index_t CShuffleMXdlPerWavePerShuffle,
          index_t CShuffleNXdlPerWavePerShuffle,
          typename CShuffleBlockTransferClusterLengths_MBlock_MPerBlock_NBlock_NPerBlock,
          index_t CShuffleBlockTransferScalarPerVector_NPerBlock>
struct DeviceGemmXdlSplitKCShuffle
    : public DeviceGemm<AElementwiseOperation, BElementwiseOperation, CElementwiseOperation>
{
    using DeviceOp = DeviceGemmXdlSplitKCShuffle;

    static constexpr auto I0 = Number<0>{};
    static constexpr auto I1 = Number<1>{};
    static constexpr auto I2 = Number<2>{};

    template <index_t K1>
    static auto GetActualBatchAndKSplitted(index_t K, index_t KBatch)
    {
        const index_t K0PerBlock   = KPerBlock / K1;
        const index_t K0           = math::integer_divide_ceil(K, KPerBlock * KBatch) * K0PerBlock;
        const index_t KSplitted    = K0 * K1;
        const index_t actual_batch = math::integer_divide_ceil(K, KSplitted);

        return std::make_pair(actual_batch, KSplitted);
    }

    template <bool IsTail>
    static auto MakeAGridDescriptor_AK0_M_AK1(index_t MRaw, index_t KRaw, index_t StrideA);
    template <bool IsTail>
    static auto MakeBGridDescriptor_BK0_N_BK1(index_t KRaw, index_t NRaw, index_t StrideB);

    /*
     * No padding in K
     */
    template <>
    static auto MakeAGridDescriptor_AK0_M_AK1<false>(index_t MRaw, index_t K, index_t StrideA)
    {
#if 1
        return MakeAGridDescriptor_AK0_M_AK1<true>(MRaw, K, StrideA);
#else
        assert(K % KPerBlock == 0);
        assert(K % AK1 == 0);

        const auto a_grid_desc_mraw_k = [&]() {
            if constexpr(is_same_v<tensor_layout::gemm::RowMajor, ALayout>)
            {
                return make_naive_tensor_descriptor(make_tuple(MRaw, K), make_tuple(StrideA, I1));
            }
            else if constexpr(is_same_v<tensor_layout::gemm::ColumnMajor, ALayout>)
            {
                return make_naive_tensor_descriptor(make_tuple(MRaw, K), make_tuple(I1, StrideA));
            }
        }();

        const auto M = math::integer_divide_ceil(MRaw, MPerBlock) * MPerBlock;

        const auto MPad = M - MRaw;
        const auto AK0  = K / AK1;

        if constexpr(GemmSpec == GemmSpecialization::MPadding ||
                     GemmSpec == GemmSpecialization::MKPadding ||
                     GemmSpec == GemmSpecialization::MNKPadding)
        {
            // pad M, but not K
            const auto a_grid_desc_ak0_m_ak1 =
                transform_tensor_descriptor(a_grid_desc_mraw_k,
                                            make_tuple(make_unmerge_transform(make_tuple(AK0, AK1)),
                                                       make_right_pad_transform(MRaw, MPad)),
                                            make_tuple(Sequence<1>{}, Sequence<0>{}),
                                            make_tuple(Sequence<0, 2>{}, Sequence<1>{}));

            return a_grid_desc_ak0_m_ak1;
        }
        else
        {
            // not pad M or K
            const auto a_grid_desc_ak0_m_ak1 =
                transform_tensor_descriptor(a_grid_desc_mraw_k,
                                            make_tuple(make_unmerge_transform(make_tuple(AK0, AK1)),
                                                       make_pass_through_transform(MRaw)),
                                            make_tuple(Sequence<1>{}, Sequence<0>{}),
                                            make_tuple(Sequence<0, 2>{}, Sequence<1>{}));

            return a_grid_desc_ak0_m_ak1;
        }
#endif
    }

    template <>
    static auto MakeBGridDescriptor_BK0_N_BK1<false>(index_t K, index_t NRaw, index_t StrideB)
    {
#if 1
        return MakeBGridDescriptor_BK0_N_BK1<true>(K, NRaw, StrideB);
#else
        assert(K % KPerBlock == 0);
        assert(K % BK1 == 0);

        const auto b_grid_desc_nraw_k = [&]() {
            if constexpr(is_same<tensor_layout::gemm::RowMajor, BLayout>::value)
            {
                return make_naive_tensor_descriptor(make_tuple(NRaw, K), make_tuple(I1, StrideB));
            }
            else if constexpr(is_same<tensor_layout::gemm::ColumnMajor, BLayout>::value)
            {
                return make_naive_tensor_descriptor(make_tuple(NRaw, K), make_tuple(StrideB, I1));
            }
        }();

        const auto N = math::integer_divide_ceil(NRaw, NPerBlock) * NPerBlock;

        const auto NPad = N - NRaw;
        const auto BK0  = K / BK1;

        if constexpr(GemmSpec == GemmSpecialization::NPadding ||
                     GemmSpec == GemmSpecialization::NKPadding ||
                     GemmSpec == GemmSpecialization::MNKPadding)
        {
            // pad N, but not K
            const auto b_grid_desc_bk0_n_bk1 =
                transform_tensor_descriptor(b_grid_desc_nraw_k,
                                            make_tuple(make_unmerge_transform(make_tuple(BK0, BK1)),
                                                       make_right_pad_transform(NRaw, NPad)),
                                            make_tuple(Sequence<1>{}, Sequence<0>{}),
                                            make_tuple(Sequence<0, 2>{}, Sequence<1>{}));

            return b_grid_desc_bk0_n_bk1;
        }
        else
        {
            // not pad N or K
            const auto b_grid_desc_bk0_n_bk1 =
                transform_tensor_descriptor(b_grid_desc_nraw_k,
                                            make_tuple(make_unmerge_transform(make_tuple(BK0, BK1)),
                                                       make_pass_through_transform(NRaw)),
                                            make_tuple(Sequence<1>{}, Sequence<0>{}),
                                            make_tuple(Sequence<0, 2>{}, Sequence<1>{}));

            return b_grid_desc_bk0_n_bk1;
        }
#endif
    }

    template <>
    static auto MakeAGridDescriptor_AK0_M_AK1<true>(index_t MRaw, index_t KRaw, index_t StrideA)
    {
        const auto a_grid_desc_mraw_kraw = [&]() {
            if constexpr(is_same_v<tensor_layout::gemm::RowMajor, ALayout>)
            {
                return make_naive_tensor_descriptor(make_tuple(MRaw, KRaw),
                                                    make_tuple(StrideA, I1));
            }
            else if constexpr(is_same_v<tensor_layout::gemm::ColumnMajor, ALayout>)
            {
                return make_naive_tensor_descriptor(make_tuple(MRaw, KRaw),
                                                    make_tuple(I1, StrideA));
            }
        }();

        const auto M = math::integer_divide_ceil(MRaw, MPerBlock) * MPerBlock;
        const auto K = math::integer_divide_ceil(KRaw, KPerBlock) * KPerBlock;

        const auto MPad = M - MRaw;
        const auto KPad = K - KRaw;
        assert(K % AK1 == 0);
        const auto AK0 = K / AK1;

        if constexpr(GemmSpec == GemmSpecialization::MPadding ||
                     GemmSpec == GemmSpecialization::MKPadding ||
                     GemmSpec == GemmSpecialization::MNKPadding)
        {
            // pad both M and K

            const auto a_grid_desc_m_k =
                transform_tensor_descriptor(a_grid_desc_mraw_kraw,
                                            make_tuple(make_right_pad_transform(MRaw, MPad),
                                                       make_right_pad_transform(KRaw, KPad)),
                                            make_tuple(Sequence<0>{}, Sequence<1>{}),
                                            make_tuple(Sequence<0>{}, Sequence<1>{}));

            const auto a_grid_desc_ak0_m_ak1 =
                transform_tensor_descriptor(a_grid_desc_m_k,
                                            make_tuple(make_unmerge_transform(make_tuple(AK0, AK1)),
                                                       make_pass_through_transform(M)),
                                            make_tuple(Sequence<1>{}, Sequence<0>{}),
                                            make_tuple(Sequence<0, 2>{}, Sequence<1>{}));

            return a_grid_desc_ak0_m_ak1;
        }
        else
        {
            // pad K, but not M
            const auto a_grid_desc_m_k = transform_tensor_descriptor(
                a_grid_desc_mraw_kraw,
                make_tuple(make_pass_through_transform(MRaw), make_right_pad_transform(KRaw, KPad)),
                make_tuple(Sequence<0>{}, Sequence<1>{}),
                make_tuple(Sequence<0>{}, Sequence<1>{}));

            const auto a_grid_desc_ak0_m_ak1 =
                transform_tensor_descriptor(a_grid_desc_m_k,
                                            make_tuple(make_unmerge_transform(make_tuple(AK0, AK1)),
                                                       make_pass_through_transform(MRaw)),
                                            make_tuple(Sequence<1>{}, Sequence<0>{}),
                                            make_tuple(Sequence<0, 2>{}, Sequence<1>{}));

            return a_grid_desc_ak0_m_ak1;
        }
    }

    template <>
    static auto MakeBGridDescriptor_BK0_N_BK1<true>(index_t KRaw, index_t NRaw, index_t StrideB)
    {
        const auto b_grid_desc_nraw_kraw = [&]() {
            if constexpr(is_same<tensor_layout::gemm::RowMajor, BLayout>::value)
            {
                return make_naive_tensor_descriptor(make_tuple(NRaw, KRaw),
                                                    make_tuple(I1, StrideB));
            }
            else if constexpr(is_same<tensor_layout::gemm::ColumnMajor, BLayout>::value)
            {
                return make_naive_tensor_descriptor(make_tuple(NRaw, KRaw),
                                                    make_tuple(StrideB, I1));
            }
        }();

        const auto N = math::integer_divide_ceil(NRaw, NPerBlock) * NPerBlock;
        const auto K = math::integer_divide_ceil(KRaw, KPerBlock) * KPerBlock;

        const auto NPad = N - NRaw;
        const auto KPad = K - KRaw;

        assert(K % BK1 == 0);
        const auto BK0 = K / BK1;

        if constexpr(GemmSpec == GemmSpecialization::NPadding ||
                     GemmSpec == GemmSpecialization::NKPadding ||
                     GemmSpec == GemmSpecialization::MNKPadding)
        { // pad both N and K
            const auto b_grid_desc_n_k =
                transform_tensor_descriptor(b_grid_desc_nraw_kraw,
                                            make_tuple(make_right_pad_transform(NRaw, NPad),
                                                       make_right_pad_transform(KRaw, KPad)),
                                            make_tuple(Sequence<0>{}, Sequence<1>{}),
                                            make_tuple(Sequence<0>{}, Sequence<1>{}));

            const auto b_grid_desc_bk0_n_bk1 =
                transform_tensor_descriptor(b_grid_desc_n_k,
                                            make_tuple(make_unmerge_transform(make_tuple(BK0, BK1)),
                                                       make_pass_through_transform(N)),
                                            make_tuple(Sequence<1>{}, Sequence<0>{}),
                                            make_tuple(Sequence<0, 2>{}, Sequence<1>{}));

            return b_grid_desc_bk0_n_bk1;
        }
        else // pad K, but not N
        {
            const auto b_grid_desc_n_k = transform_tensor_descriptor(
                b_grid_desc_nraw_kraw,
                make_tuple(make_pass_through_transform(NRaw), make_right_pad_transform(KRaw, KPad)),
                make_tuple(Sequence<0>{}, Sequence<1>{}),
                make_tuple(Sequence<0>{}, Sequence<1>{}));

            const auto b_grid_desc_bk0_n_bk1 =
                transform_tensor_descriptor(b_grid_desc_n_k,
                                            make_tuple(make_unmerge_transform(make_tuple(BK0, BK1)),
                                                       make_pass_through_transform(NRaw)),
                                            make_tuple(Sequence<1>{}, Sequence<0>{}),
                                            make_tuple(Sequence<0, 2>{}, Sequence<1>{}));

            return b_grid_desc_bk0_n_bk1;
        }
    }

    static auto MakeCGridDescriptor_M_N(index_t MRaw, index_t NRaw, index_t StrideC)
    {
        const auto c_grid_desc_mraw_nraw = [&]() {
            if constexpr(is_same<tensor_layout::gemm::RowMajor, CLayout>::value)
            {
                return make_naive_tensor_descriptor(make_tuple(MRaw, NRaw),
                                                    make_tuple(StrideC, I1));
            }
            else if constexpr(is_same<tensor_layout::gemm::ColumnMajor, CLayout>::value)
            {
                return make_naive_tensor_descriptor(make_tuple(MRaw, NRaw),
                                                    make_tuple(I1, StrideC));
            }
        }();

        const auto M = math::integer_divide_ceil(MRaw, MPerBlock) * MPerBlock;
        const auto N = math::integer_divide_ceil(NRaw, NPerBlock) * NPerBlock;

        const auto MPad = M - MRaw;
        const auto NPad = N - NRaw;

        if constexpr(GemmSpec == GemmSpecialization::MNPadding ||
                     GemmSpec == GemmSpecialization::MNKPadding)
        {
            // pad M and N
            return transform_tensor_descriptor(c_grid_desc_mraw_nraw,
                                               make_tuple(make_right_pad_transform(MRaw, MPad),
                                                          make_right_pad_transform(NRaw, NPad)),
                                               make_tuple(Sequence<0>{}, Sequence<1>{}),
                                               make_tuple(Sequence<0>{}, Sequence<1>{}));
        }
        else if constexpr(GemmSpec == GemmSpecialization::MPadding ||
                          GemmSpec == GemmSpecialization::MKPadding)
        {
            // pad M, but not N
            return transform_tensor_descriptor(
                c_grid_desc_mraw_nraw,
                make_tuple(make_right_pad_transform(MRaw, MPad), make_pass_through_transform(NRaw)),
                make_tuple(Sequence<0>{}, Sequence<1>{}),
                make_tuple(Sequence<0>{}, Sequence<1>{}));
        }
        else if constexpr(GemmSpec == GemmSpecialization::NPadding ||
                          GemmSpec == GemmSpecialization::NKPadding)
        {
            // pad N, but not M
            return transform_tensor_descriptor(
                c_grid_desc_mraw_nraw,
                make_tuple(make_pass_through_transform(MRaw), make_right_pad_transform(NRaw, NPad)),
                make_tuple(Sequence<0>{}, Sequence<1>{}),
                make_tuple(Sequence<0>{}, Sequence<1>{}));
        }
        else
        {
            // not pad M or N
            return c_grid_desc_mraw_nraw;
        }
    }

    using AGridDesc_AK0_M_AK1      = decltype(MakeAGridDescriptor_AK0_M_AK1<false>(1, 1, 1));
    using BGridDesc_BK0_N_BK1      = decltype(MakeBGridDescriptor_BK0_N_BK1<false>(1, 1, 1));
    using AGridDesc_AK0_M_AK1_Tail = decltype(MakeAGridDescriptor_AK0_M_AK1<true>(1, 1, 1));
    using BGridDesc_BK0_N_BK1_Tail = decltype(MakeBGridDescriptor_BK0_N_BK1<true>(1, 1, 1));
    using CGridDesc_M_N            = decltype(MakeCGridDescriptor_M_N(1, 1, 1));

    struct ComputePtrOffsetOfStridedBatch
    {
        ComputePtrOffsetOfStridedBatch(const index_t BatchStrideA, const index_t BatchStrideB)
            : BatchStrideA_(BatchStrideA), BatchStrideB_(BatchStrideB)
        {
        }

        __host__ __device__ constexpr long_index_t GetAPtrOffset(index_t g_idx) const
        {
            return g_idx * static_cast<long_index_t>(BatchStrideA_);
        }

        __host__ __device__ constexpr long_index_t GetBPtrOffset(index_t g_idx) const
        {
            return g_idx * static_cast<long_index_t>(BatchStrideB_);
        }

        __host__ __device__ constexpr long_index_t GetCPtrOffset(index_t g_idx) const
        {
            ignore = g_idx;
            return 0;
        }

        private:
        index_t BatchStrideA_;
        index_t BatchStrideB_;
        // index_t BatchStrideC_; // always zero
    };

    // GridwiseGemm
    using GridwiseGemm = GridwiseGemm_k0mk1_k0nk1_mn_xdl_cshuffle_v1<
        ADataType, // TODO: distinguish A/B datatype
        GemmAccDataType,
        CShuffleDataType,
        CDataType,
        AElementwiseOperation,
        BElementwiseOperation,
        CElementwiseOperation,
        InMemoryDataOperationEnum::AtomicAdd,
        NumGemmKPrefetchStage,
        BlockSize,
        MPerBlock,
        NPerBlock,
        KPerBlock,
        AK1,
        BK1,
        MPerXDL,
        NPerXDL,
        MXdlPerWave,
        NXdlPerWave,
        ABlockTransferThreadClusterLengths_AK0_M_AK1,
        ABlockTransferThreadClusterArrangeOrder,
        ABlockTransferSrcAccessOrder,
        ABlockTransferSrcVectorDim,
        ABlockTransferSrcScalarPerVector,
        ABlockTransferDstScalarPerVector_AK1,
        false,
        ABlockLdsExtraM,
        BBlockTransferThreadClusterLengths_BK0_N_BK1,
        BBlockTransferThreadClusterArrangeOrder,
        BBlockTransferSrcAccessOrder,
        BBlockTransferSrcVectorDim,
        BBlockTransferSrcScalarPerVector,
        BBlockTransferDstScalarPerVector_BK1,
        false,
        BBlockLdsExtraN,
        CShuffleMXdlPerWavePerShuffle,
        CShuffleNXdlPerWavePerShuffle,
        CShuffleBlockTransferClusterLengths_MBlock_MPerBlock_NBlock_NPerBlock,
        CShuffleBlockTransferScalarPerVector_NPerBlock>;

    using CGridDescriptor_MBlock_MPerBlock_NBlock_NPerBlock = decltype(
        GridwiseGemm::MakeCGridDescriptor_MBlock_MPerBlock_NBlock_NPerBlock(CGridDesc_M_N{}));
    using Block2CTileMap =
        decltype(BatchedGemmUtil::MakeBlock2CTileMap<MPerBlock, NPerBlock>(1, 1, 1));

    struct Argument : public BaseArgument
    {
        Argument(const ADataType* p_a_grid,
                 const BDataType* p_b_grid,
                 CDataType* p_c_grid,
                 index_t MRaw,
                 index_t NRaw,
                 index_t KRaw,
                 index_t StrideA,
                 index_t StrideB,
                 index_t StrideC,
                 AElementwiseOperation a_element_op,
                 BElementwiseOperation b_element_op,
                 CElementwiseOperation c_element_op,
                 index_t k_batch)
            : p_a_grid_{p_a_grid},
              p_b_grid_{p_b_grid},
              p_c_grid_{p_c_grid},
              BatchCount_(k_batch),
              compute_ptr_offset_of_batch_{0, 0},
              block_2_ctile_map_{},
              a_element_op_{a_element_op},
              b_element_op_{b_element_op},
              c_element_op_{c_element_op}
        {
            const auto actual_batch_and_ksplitted_A =
                GetActualBatchAndKSplitted<AK1>(KRaw, k_batch);
            const auto actual_batch_and_ksplitted_B =
                GetActualBatchAndKSplitted<BK1>(KRaw, k_batch);
            assert(actual_batch_and_ksplitted_A.first == actual_batch_and_ksplitted_B.first);
            BatchCount_           = actual_batch_and_ksplitted_A.first;
            const auto AKSplitted = actual_batch_and_ksplitted_A.second;
            const auto BKSplitted = actual_batch_and_ksplitted_B.second;

            a_grid_desc_ak0_m_ak1_ =
                DeviceOp::MakeAGridDescriptor_AK0_M_AK1<false>(MRaw, AKSplitted, StrideA);
            b_grid_desc_bk0_n_bk1_ =
                DeviceOp::MakeBGridDescriptor_BK0_N_BK1<false>(BKSplitted, NRaw, StrideB);
            c_grid_desc_m_n_ = DeviceOp::MakeCGridDescriptor_M_N(MRaw, NRaw, StrideC);

            is_valid_ = GridwiseGemm::CheckValidity(
                a_grid_desc_ak0_m_ak1_, b_grid_desc_bk0_n_bk1_, c_grid_desc_m_n_);

            if(KRaw != AKSplitted * BatchCount_ || KRaw != BKSplitted * BatchCount_)
            {
                has_tail_         = true;
                const auto AKTail = KRaw - AKSplitted * (BatchCount_ - 1);
                const auto BKTail = KRaw - BKSplitted * (BatchCount_ - 1);

                a_grid_desc_ak0_m_ak1_tail_ =
                    DeviceOp::MakeAGridDescriptor_AK0_M_AK1<true>(MRaw, AKTail, StrideA);
                b_grid_desc_bk0_n_bk1_tail_ =
                    DeviceOp::MakeBGridDescriptor_BK0_N_BK1<true>(BKTail, NRaw, StrideB);

                is_valid_ &= GridwiseGemm::CheckValidity(
                    a_grid_desc_ak0_m_ak1_tail_, b_grid_desc_bk0_n_bk1_tail_, c_grid_desc_m_n_);
            }

            if(is_valid_)
            {
                c_grid_desc_mblock_mperblock_nblock_nperblock_ =
                    GridwiseGemm::MakeCGridDescriptor_MBlock_MPerBlock_NBlock_NPerBlock(
                        c_grid_desc_m_n_);

                const index_t a_batch_stride = [AKSplitted, StrideA]() {
                    if constexpr(is_same<tensor_layout::gemm::RowMajor, ALayout>::value)
                    {
                        ignore = StrideA;
                        return AKSplitted;
                    }
                    else if constexpr(is_same<tensor_layout::gemm::ColumnMajor, ALayout>::value)
                    {
                        return AKSplitted * StrideA;
                    }
                }();

                const index_t b_batch_stride = [BKSplitted, StrideB]() {
                    if constexpr(is_same<tensor_layout::gemm::RowMajor, BLayout>::value)
                    {
                        return BKSplitted * StrideB;
                    }
                    else if constexpr(is_same<tensor_layout::gemm::ColumnMajor, BLayout>::value)
                    {
                        ignore = StrideB;
                        return BKSplitted;
                    }
                }();

                compute_ptr_offset_of_batch_ =
                    ComputePtrOffsetOfStridedBatch{a_batch_stride, b_batch_stride};

                block_2_ctile_map_ = BatchedGemmUtil::MakeBlock2CTileMap<MPerBlock, NPerBlock>(
                    BatchCount_, c_grid_desc_m_n_.GetLength(I0), c_grid_desc_m_n_.GetLength(I1));
            }
        }

        //  private:
        const ADataType* p_a_grid_;
        const BDataType* p_b_grid_;
        CDataType* p_c_grid_;
        index_t BatchCount_;
        bool has_tail_;
        bool is_valid_;
        AGridDesc_AK0_M_AK1 a_grid_desc_ak0_m_ak1_;
        BGridDesc_BK0_N_BK1 b_grid_desc_bk0_n_bk1_;
        AGridDesc_AK0_M_AK1_Tail a_grid_desc_ak0_m_ak1_tail_;
        BGridDesc_BK0_N_BK1_Tail b_grid_desc_bk0_n_bk1_tail_;
        CGridDesc_M_N c_grid_desc_m_n_;
        CGridDescriptor_MBlock_MPerBlock_NBlock_NPerBlock
            c_grid_desc_mblock_mperblock_nblock_nperblock_;
        ComputePtrOffsetOfStridedBatch compute_ptr_offset_of_batch_;
        Block2CTileMap block_2_ctile_map_;
        AElementwiseOperation a_element_op_;
        BElementwiseOperation b_element_op_;
        CElementwiseOperation c_element_op_;
    };

    // Invoker
    struct Invoker : public BaseInvoker
    {
        using Argument = DeviceGemmXdlSplitKCShuffle::Argument;

        float Run(const Argument& arg, int nrepeat = 1)
        {
            {
                std::cout << "k_batch = " << arg.BatchCount_ << "\n";
                std::cout << "arg.a_grid_desc_ak0_m_ak1_{"
                          << arg.a_grid_desc_ak0_m_ak1_.GetLength(I0) << ", "
                          << arg.a_grid_desc_ak0_m_ak1_.GetLength(I1) << ", "
                          << arg.a_grid_desc_ak0_m_ak1_.GetLength(I2) << "}" << std::endl;

                std::cout << "arg.b_grid_desc_bk0_n_bk1_{"
                          << arg.b_grid_desc_bk0_n_bk1_.GetLength(I0) << ", "
                          << arg.b_grid_desc_bk0_n_bk1_.GetLength(I1) << ", "
                          << arg.b_grid_desc_bk0_n_bk1_.GetLength(I2) << "}" << std::endl;

                std::cout << "arg.c_grid_desc_m_n_{ " << arg.c_grid_desc_m_n_.GetLength(I0) << ", "
                          << arg.c_grid_desc_m_n_.GetLength(I1) << "}" << std::endl;

                if(arg.has_tail_)
                {

                    std::cout << "arg.a_grid_desc_ak0_m_ak1_tail_{"
                              << arg.a_grid_desc_ak0_m_ak1_tail_.GetLength(I0) << ", "
                              << arg.a_grid_desc_ak0_m_ak1_tail_.GetLength(I1) << ", "
                              << arg.a_grid_desc_ak0_m_ak1_tail_.GetLength(I2) << "}" << std::endl;

                    std::cout << "arg.b_grid_desc_bk0_n_bk1_tail_{"
                              << arg.b_grid_desc_bk0_n_bk1_tail_.GetLength(I0) << ", "
                              << arg.b_grid_desc_bk0_n_bk1_tail_.GetLength(I1) << ", "
                              << arg.b_grid_desc_bk0_n_bk1_tail_.GetLength(I2) << "}" << std::endl;
                }
            }

            if(!arg.is_valid_)
            {
                throw std::runtime_error(
                    "wrong! GridwiseBatchedGemm_km_kn_m0m1n0n1_xdlops_v2r3 has invalid setting");
            }

            const index_t grid_size =
                GridwiseGemm::CalculateGridSize(arg.c_grid_desc_m_n_) * arg.BatchCount_;

            const auto K0                     = arg.a_grid_desc_ak0_m_ak1_.GetLength(I0);
            const bool has_main_k0_block_loop = GridwiseGemm::CalculateHasMainK0BlockLoop(K0);

            float ave_time = 0;

            if(arg.has_tail_)
            {
                const auto K0_tail = arg.a_grid_desc_ak0_m_ak1_tail_.GetLength(I0);
                const bool tail_has_main_k0_block_loop =
                    GridwiseGemm::CalculateHasMainK0BlockLoop(K0_tail);

                const auto Run = [&](const auto& kernel) {
                    if(nrepeat == 0)
                    {
                        launch_kernel(kernel,
                                      dim3(grid_size),
                                      dim3(BlockSize),
                                      0,
                                      arg.p_a_grid_,
                                      arg.p_b_grid_,
                                      arg.p_c_grid_,
                                      arg.BatchCount_,
                                      arg.a_element_op_,
                                      arg.b_element_op_,
                                      arg.c_element_op_,
                                      arg.a_grid_desc_ak0_m_ak1_,
                                      arg.b_grid_desc_bk0_n_bk1_,
                                      arg.a_grid_desc_ak0_m_ak1_tail_,
                                      arg.b_grid_desc_bk0_n_bk1_tail_,
                                      arg.c_grid_desc_mblock_mperblock_nblock_nperblock_,
                                      arg.compute_ptr_offset_of_batch_,
                                      arg.block_2_ctile_map_);
                        return 0.0f;
                    }
                    else
                    {
                        return launch_and_time_kernel(
                            kernel,
                            nrepeat,
                            dim3(grid_size),
                            dim3(BlockSize),
                            0,
                            arg.p_a_grid_,
                            arg.p_b_grid_,
                            arg.p_c_grid_,
                            arg.BatchCount_,
                            arg.a_element_op_,
                            arg.b_element_op_,
                            arg.c_element_op_,
                            arg.a_grid_desc_ak0_m_ak1_,
                            arg.b_grid_desc_bk0_n_bk1_,
                            arg.a_grid_desc_ak0_m_ak1_tail_,
                            arg.b_grid_desc_bk0_n_bk1_tail_,
                            arg.c_grid_desc_mblock_mperblock_nblock_nperblock_,
                            arg.compute_ptr_offset_of_batch_,
                            arg.block_2_ctile_map_);
                    }
                };

                if(has_main_k0_block_loop && tail_has_main_k0_block_loop)
                {
                    const auto kernel = kernel_batched_gemm_xdl_cshuffle_v1<
                        GridwiseGemm,
                        ADataType, // TODO: distiguish A/B datatype
                        CDataType,
                        AElementwiseOperation,
                        BElementwiseOperation,
                        CElementwiseOperation,
                        DeviceOp::AGridDesc_AK0_M_AK1,
                        DeviceOp::BGridDesc_BK0_N_BK1,
                        DeviceOp::AGridDesc_AK0_M_AK1_Tail,
                        DeviceOp::BGridDesc_BK0_N_BK1_Tail,
                        CGridDescriptor_MBlock_MPerBlock_NBlock_NPerBlock,
                        ComputePtrOffsetOfStridedBatch,
                        Block2CTileMap,
                        true,
                        true>;

                    ave_time = Run(kernel);
                }
                else if(has_main_k0_block_loop && !tail_has_main_k0_block_loop)
                {
                    const auto kernel = kernel_batched_gemm_xdl_cshuffle_v1<
                        GridwiseGemm,
                        ADataType, // TODO: distiguish A/B datatype
                        CDataType,
                        AElementwiseOperation,
                        BElementwiseOperation,
                        CElementwiseOperation,
                        DeviceOp::AGridDesc_AK0_M_AK1,
                        DeviceOp::BGridDesc_BK0_N_BK1,
                        DeviceOp::AGridDesc_AK0_M_AK1_Tail,
                        DeviceOp::BGridDesc_BK0_N_BK1_Tail,
                        CGridDescriptor_MBlock_MPerBlock_NBlock_NPerBlock,
                        ComputePtrOffsetOfStridedBatch,
                        Block2CTileMap,
                        true,
                        false>;

                    ave_time = Run(kernel);
                }
                else if(!has_main_k0_block_loop && tail_has_main_k0_block_loop)
                {
                    const auto kernel = kernel_batched_gemm_xdl_cshuffle_v1<
                        GridwiseGemm,
                        ADataType, // TODO: distiguish A/B datatype
                        CDataType,
                        AElementwiseOperation,
                        BElementwiseOperation,
                        CElementwiseOperation,
                        DeviceOp::AGridDesc_AK0_M_AK1,
                        DeviceOp::BGridDesc_BK0_N_BK1,
                        DeviceOp::AGridDesc_AK0_M_AK1_Tail,
                        DeviceOp::BGridDesc_BK0_N_BK1_Tail,
                        CGridDescriptor_MBlock_MPerBlock_NBlock_NPerBlock,
                        ComputePtrOffsetOfStridedBatch,
                        Block2CTileMap,
                        false,
                        true>;

                    ave_time = Run(kernel);
                }
                else
                {
                    const auto kernel = kernel_batched_gemm_xdl_cshuffle_v1<
                        GridwiseGemm,
                        ADataType, // TODO: distiguish A/B datatype
                        CDataType,
                        AElementwiseOperation,
                        BElementwiseOperation,
                        CElementwiseOperation,
                        DeviceOp::AGridDesc_AK0_M_AK1,
                        DeviceOp::BGridDesc_BK0_N_BK1,
                        DeviceOp::AGridDesc_AK0_M_AK1_Tail,
                        DeviceOp::BGridDesc_BK0_N_BK1_Tail,
                        CGridDescriptor_MBlock_MPerBlock_NBlock_NPerBlock,
                        ComputePtrOffsetOfStridedBatch,
                        Block2CTileMap,
                        false,
                        false>;

                    ave_time = Run(kernel);
                }
            }
            else
            {
                const auto Run = [&](const auto& kernel) {
                    if(nrepeat == 0)
                    {
                        launch_kernel(kernel,
                                      dim3(grid_size),
                                      dim3(BlockSize),
                                      0,
                                      arg.p_a_grid_,
                                      arg.p_b_grid_,
                                      arg.p_c_grid_,
                                      arg.BatchCount_,
                                      arg.a_element_op_,
                                      arg.b_element_op_,
                                      arg.c_element_op_,
                                      arg.a_grid_desc_ak0_m_ak1_,
                                      arg.b_grid_desc_bk0_n_bk1_,
                                      arg.c_grid_desc_mblock_mperblock_nblock_nperblock_,
                                      arg.compute_ptr_offset_of_batch_,
                                      arg.block_2_ctile_map_);
                        return 0.0f;
                    }
                    else
                    {
                        return launch_and_time_kernel(
                            kernel,
                            nrepeat,
                            dim3(grid_size),
                            dim3(BlockSize),
                            0,
                            arg.p_a_grid_,
                            arg.p_b_grid_,
                            arg.p_c_grid_,
                            arg.BatchCount_,
                            arg.a_element_op_,
                            arg.b_element_op_,
                            arg.c_element_op_,
                            arg.a_grid_desc_ak0_m_ak1_,
                            arg.b_grid_desc_bk0_n_bk1_,
                            arg.c_grid_desc_mblock_mperblock_nblock_nperblock_,
                            arg.compute_ptr_offset_of_batch_,
                            arg.block_2_ctile_map_);
                    }
                };

                if(has_main_k0_block_loop)
                {
                    const auto kernel = ck::kernel_batched_gemm_xdl_cshuffle_v1<
                        GridwiseGemm,
                        ADataType, // TODO: distiguish A/B datatype
                        CDataType,
                        AElementwiseOperation,
                        BElementwiseOperation,
                        CElementwiseOperation,
                        DeviceOp::AGridDesc_AK0_M_AK1,
                        DeviceOp::BGridDesc_BK0_N_BK1,
                        CGridDescriptor_MBlock_MPerBlock_NBlock_NPerBlock,
                        ComputePtrOffsetOfStridedBatch,
                        Block2CTileMap,
                        true>;

                    ave_time = Run(kernel);
                }
                else
                {
                    const auto kernel = ck::kernel_batched_gemm_xdl_cshuffle_v1<
                        GridwiseGemm,
                        ADataType, // TODO: distiguish A/B datatype
                        CDataType,
                        AElementwiseOperation,
                        BElementwiseOperation,
                        CElementwiseOperation,
                        DeviceOp::AGridDesc_AK0_M_AK1,
                        DeviceOp::BGridDesc_BK0_N_BK1,
                        CGridDescriptor_MBlock_MPerBlock_NBlock_NPerBlock,
                        ComputePtrOffsetOfStridedBatch,
                        Block2CTileMap,
                        false>;

                    ave_time = Run(kernel);
                }
            }
            return ave_time;
        }

        // polymorphic
        float Run(const BaseArgument* p_arg, int nrepeat = 1) override
        {
            return Run(*dynamic_cast<const Argument*>(p_arg), nrepeat);
        }
    };

    static constexpr bool IsValidCompilationParameter()
    {
        // TODO: properly implement this check
        return true;
    }

    static bool IsSupportedArgument(const Argument& arg)
    {
        return GridwiseGemm::CheckValidity(
            arg.a_grid_desc_ak0_m_ak1_, arg.b_grid_desc_bk0_n_bk1_, arg.c_grid_desc_m_n_);
    }

    // polymorphic
    bool IsSupportedArgument(const BaseArgument* p_arg) override
    {
        return IsSupportedArgument(*dynamic_cast<const Argument*>(p_arg));
    }

    static auto MakeArgument(const ADataType* p_a,
                             const BDataType* p_b,
                             CDataType* p_c,
                             index_t M,
                             index_t N,
                             index_t K,
                             index_t StrideA,
                             index_t StrideB,
                             index_t StrideC,
                             AElementwiseOperation a_element_op,
                             BElementwiseOperation b_element_op,
                             CElementwiseOperation c_element_op,
                             index_t BatchCount)
    {
        return Argument{p_a,
                        p_b,
                        p_c,
                        M,
                        N,
                        K,
                        StrideA,
                        StrideB,
                        StrideC,
                        a_element_op,
                        b_element_op,
                        c_element_op,
                        BatchCount};
    }

    static auto MakeInvoker() { return Invoker{}; }

    // polymorphic
    std::unique_ptr<BaseArgument> MakeArgumentPointer(const void* p_a,
                                                      const void* p_b,
                                                      void* p_c,
                                                      index_t M,
                                                      index_t N,
                                                      index_t K,
                                                      index_t StrideA,
                                                      index_t StrideB,
                                                      index_t StrideC,
                                                      AElementwiseOperation a_element_op,
                                                      BElementwiseOperation b_element_op,
                                                      CElementwiseOperation c_element_op,
                                                      index_t BatchCount) override
    {
        return std::make_unique<Argument>(static_cast<const ADataType*>(p_a),
                                          static_cast<const BDataType*>(p_b),
                                          static_cast<CDataType*>(p_c),
                                          M,
                                          N,
                                          K,
                                          StrideA,
                                          StrideB,
                                          StrideC,
                                          a_element_op,
                                          b_element_op,
                                          c_element_op,
                                          BatchCount);
    }

    // polymorphic
    std::unique_ptr<BaseInvoker> MakeInvokerPointer() override
    {
        return std::make_unique<Invoker>(Invoker{});
    }

    // polymorphic
    std::string GetTypeString() const override
    {
        auto str = std::stringstream();

        // clang-format off
        str << "DeviceGemmXdlSplitKCShuffle"
            << "<"
            << BlockSize << ", "
            << MPerBlock << ", "
            << NPerBlock << ", "
            << KPerBlock << ", "
            << AK1 << ", "
            << BK1
            << ">";
        // clang-format on

        return str.str();
    }
}; // namespace device

} // namespace device
} // namespace tensor_operation
} // namespace ck
#endif
