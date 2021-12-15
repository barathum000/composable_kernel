#ifndef CK_GRIDWISE_GEMM_XDLOPS_V3R1_HPP
#define CK_GRIDWISE_GEMM_XDLOPS_V3R1_HPP

#include "common_header.hpp"
#include "multi_index_transform_helper.hpp"
#include "tensor_descriptor.hpp"
#include "tensor_descriptor_helper.hpp"
#include "blockwise_gemm_xdlops.hpp"
#include "blockwise_tensor_slice_transfer.hpp"
#include "threadwise_tensor_slice_transfer.hpp"
#include "threadwise_tensor_slice_set.hpp"

namespace ck {

template <typename GridwiseGemm,
          typename FloatAB,
          typename FloatC,
          typename AGridDesc_K0_M_K1,
          typename BGridDesc_K0_N_K1,
          typename CGridDescriptor_MBlock_MRepeat_MWaveMPerXdl_NBlock_NRepeat_NWaveNPerXdl,
          typename AElementwiseOperation,
          typename BElementwiseOperation,
          typename CElementwiseOperation,
          typename Block2CTileMap,
          bool HasMainKBlockLoop>
__global__ void
#if CK_USE_LAUNCH_BOUNDS
    __launch_bounds__(CK_MAX_THREAD_PER_BLOCK, CK_MIN_BLOCK_PER_CU)
#endif
        kernel_gemm_xdlops_v3r1(
            const FloatAB* __restrict__ p_a_grid,
            const FloatAB* __restrict__ p_b_grid,
            FloatC* __restrict__ p_c_grid,
            const AGridDesc_K0_M_K1 a_grid_desc_k0_m_k1,
            const BGridDesc_K0_N_K1 b_grid_desc_k0_n_k1,
            const CGridDescriptor_MBlock_MRepeat_MWaveMPerXdl_NBlock_NRepeat_NWaveNPerXdl
                c_grid_desc_mblock_mrepeat_mwavemperxdl_nblock_nrepeat_nwavenperxdl,
            const AElementwiseOperation a_element_op,
            const BElementwiseOperation b_element_op,
            const CElementwiseOperation c_element_op,
            const Block2CTileMap block_2_ctile_map)
{
    __shared__ char p_shared[GridwiseGemm::GetSharedMemoryNumberOfByte()];

    GridwiseGemm::template Run<HasMainKBlockLoop>(
        p_a_grid,
        p_b_grid,
        p_c_grid,
        p_shared,
        a_grid_desc_k0_m_k1,
        b_grid_desc_k0_n_k1,
        c_grid_desc_mblock_mrepeat_mwavemperxdl_nblock_nrepeat_nwavenperxdl,
        a_element_op,
        b_element_op,
        c_element_op,
        block_2_ctile_map);
}

template <index_t BlockSize,
          typename FloatAB,
          typename FloatAcc,
          typename FloatC,
          InMemoryDataOperationEnum_t CGlobalMemoryDataOperation,
          typename AGridDesc_K0_M_K1,
          typename BGridDesc_K0_N_K1,
          typename CGridDesc_M_N,
          typename AElementwiseOperation,
          typename BElementwiseOperation,
          typename CElementwiseOperation,
          index_t MPerBlock,
          index_t NPerBlock,
          index_t K0PerBlock,
          index_t MPerXdl,
          index_t NPerXdl,
          index_t K1Value,
          index_t MRepeat,
          index_t NRepeat,
          typename ABlockTransferThreadSliceLengths_K0_M_K1,
          typename ABlockTransferThreadClusterLengths_K0_M_K1,
          typename ABlockTransferThreadClusterArrangeOrder,
          typename ABlockTransferSrcAccessOrder,
          index_t ABlockTransferSrcVectorDim,
          index_t ABlockTransferSrcScalarPerVector,
          index_t ABlockTransferDstScalarPerVector_K1,
          bool AThreadTransferSrcResetCoordinateAfterRun,
          typename BBlockTransferThreadSliceLengths_K0_N_K1,
          typename BBlockTransferThreadClusterLengths_K0_N_K1,
          typename BBlockTransferThreadClusterArrangeOrder,
          typename BBlockTransferSrcAccessOrder,
          index_t BBlockTransferSrcVectorDim,
          index_t BBlockTransferSrcScalarPerVector,
          index_t BBlockTransferDstScalarPerVector_K1,
          bool BThreadTransferSrcResetCoordinateAfterRun,
          typename CThreadTransferSrcDstAccessOrder,
          index_t CThreadTransferSrcDstVectorDim,
          index_t CThreadTransferDstScalarPerVector,
          typename AGridStepHacks,
          typename BGridStepHacks,
          typename CGridStepHacks,
          typename AGridMoveSliceWindowStepHacks,
          typename BGridMoveSliceWindowStepHacks,
          bool CAccessOrderMRepeatNRepeat,
          bool ABlockLdsExtraM,
          bool BBlockLdsExtraN>
struct GridwiseGemm_k0mk1_k0nk1_mn_xdlops_v3r1
{
    static constexpr auto I0 = Number<0>{};
    static constexpr auto I1 = Number<1>{};
    static constexpr auto I2 = Number<2>{};
    static constexpr auto I3 = Number<3>{};
    static constexpr auto I4 = Number<4>{};
    static constexpr auto I5 = Number<5>{};
    static constexpr auto I6 = Number<6>{};
    static constexpr auto I7 = Number<7>{};

    // K1 should be Number<...>
    static constexpr auto K1 = Number<K1Value>{};

    __host__ __device__ static constexpr index_t GetSharedMemoryNumberOfByte()
    {
        constexpr auto max_lds_align = K1;

        // A matrix in LDS memory, dst of blockwise copy
        constexpr auto a_block_desc_k0_m_k1 = [&]() {
            if constexpr(ABlockLdsExtraM)
            {
                return make_naive_tensor_descriptor(
                    make_tuple(Number<K0PerBlock>{}, Number<MPerBlock>{}, K1),
                    make_tuple(Number<MPerBlock + 1>{} * K1, K1, I1));
            }
            else
            {
                return make_naive_tensor_descriptor_aligned(
                    make_tuple(Number<K0PerBlock>{}, Number<MPerBlock>{}, K1), max_lds_align);
            }
        }();

        // B matrix in LDS memory, dst of blockwise copy
        constexpr auto b_block_desc_k0_n_k1 = [&]() {
            if constexpr(BBlockLdsExtraN)
            {
                return make_naive_tensor_descriptor(
                    make_tuple(Number<K0PerBlock>{}, Number<NPerBlock>{}, K1),
                    make_tuple(Number<NPerBlock + 1>{} * K1, K1, I1));
            }
            else
            {
                return make_naive_tensor_descriptor_aligned(
                    make_tuple(Number<K0PerBlock>{}, Number<NPerBlock>{}, K1), max_lds_align);
            }
        }();

        // LDS allocation for A and B: be careful of alignment
        constexpr auto a_block_space_size =
            math::integer_least_multiple(a_block_desc_k0_m_k1.GetElementSpaceSize(), max_lds_align);

        constexpr auto b_block_space_size =
            math::integer_least_multiple(b_block_desc_k0_n_k1.GetElementSpaceSize(), max_lds_align);

        return (a_block_space_size + b_block_space_size) * sizeof(FloatAB);
    }

    // block_id to matrix tile idx (m0, n0) mapping are controlled by {M01, N01}
    __host__ __device__ static constexpr bool
    CheckValidity(const AGridDesc_K0_M_K1& a_grid_desc_k0_m_k1,
                  const BGridDesc_K0_N_K1& b_grid_desc_k0_n_k1,
                  const CGridDesc_M_N& c_grid_desc_m_n,
                  index_t M01,
                  index_t N01)
    {
        static_assert(is_known_at_compile_time<remove_cv_t<decltype(K1)>>::value,
                      "wrong! K1 need to be known at compile-time");

        static_assert((MPerBlock % (MPerXdl * MRepeat) == 0) &&
                          (NPerBlock % (NRepeat * NPerXdl)) == 0,
                      "Invalid tuning param!");

        const auto M  = a_grid_desc_k0_m_k1.GetLength(I1);
        const auto N  = b_grid_desc_k0_n_k1.GetLength(I1);
        const auto K0 = a_grid_desc_k0_m_k1.GetLength(I0);

        if(!(M == c_grid_desc_m_n.GetLength(I0) && N == c_grid_desc_m_n.GetLength(I1) &&
             K0 == b_grid_desc_k0_n_k1.GetLength(I0) && K1 == a_grid_desc_k0_m_k1.GetLength(I2) &&
             K1 == b_grid_desc_k0_n_k1.GetLength(I2)))
            return false;

        if(!(M % MPerBlock == 0 && N % NPerBlock == 0 && K0 % K0PerBlock == 0))
            return false;

        // check M01, N01
        constexpr auto M1 = Number<MPerBlock>{};
        constexpr auto N1 = Number<NPerBlock>{};

        const auto M0 = M / M1;
        const auto N0 = N / N1;

        if(!(M0 % M01 == 0 && N0 % N01 == 0))
            return false;

        // TODO: also check validity of all components (blockwise-copy, threadwise-copy, etc)
        return true;
    }

    __host__ __device__ static constexpr index_t
    CalculateGridSize(const CGridDesc_M_N& c_grid_desc_m_n)
    {
        const auto M = c_grid_desc_m_n.GetLength(I0);
        const auto N = c_grid_desc_m_n.GetLength(I1);

        const index_t grid_size = (M / MPerBlock) * (N / NPerBlock);

        return grid_size;
    }

    __host__ __device__ static constexpr bool CalculateHasMainK0BlockLoop(index_t K0)
    {
        const bool has_main_k0_block_loop = (K0 / K0PerBlock) > 1;

        return has_main_k0_block_loop;
    }

    __host__ __device__ static constexpr auto
    MakeCGridDescriptor_MBlock_MRepeat_MWaveMPerXdl_NBlock_NRepeat_NWaveNPerXdl(
        const CGridDesc_M_N& c_grid_desc_m_n)
    {
        const auto M = c_grid_desc_m_n.GetLength(I0);
        const auto N = c_grid_desc_m_n.GetLength(I1);

        const auto MBlock = M / MPerBlock;
        const auto NBlock = N / NPerBlock;

        constexpr index_t MWave = MPerBlock / (MRepeat * MPerXdl);
        constexpr index_t NWave = NPerBlock / (NRepeat * NPerXdl);

        const auto c_grid_desc_mblock_mrepeat_mwavemperxdl_nblock_nrepeat_nwavenperxdl =
            transform_tensor_descriptor(
                c_grid_desc_m_n,
                make_tuple(make_unmerge_transform(
                               make_tuple(MBlock, Number<MRepeat>{}, Number<MWave * MPerXdl>{})),
                           make_unmerge_transform(
                               make_tuple(NBlock, Number<NRepeat>{}, Number<NWave * NPerXdl>{}))),
                make_tuple(Sequence<0>{}, Sequence<1>{}),
                make_tuple(Sequence<0, 1, 2>{}, Sequence<3, 4, 5>{}));

        return c_grid_desc_mblock_mrepeat_mwavemperxdl_nblock_nrepeat_nwavenperxdl;
    }

    // return block_id to C matrix tile idx (m0, n0) mapping
    __host__ __device__ static constexpr auto
    MakeBlock2CTileMap(const CGridDesc_M_N& c_grid_desc_m_n, index_t M01, index_t N01)
    {
        const auto M = c_grid_desc_m_n.GetLength(I0);
        const auto N = c_grid_desc_m_n.GetLength(I1);

        constexpr auto M1 = Number<MPerBlock>{};
        constexpr auto N1 = Number<NPerBlock>{};

        const auto M0 = M / M1;
        const auto N0 = N / N1;

        const auto M00 = M0 / M01;
        const auto N00 = N0 / N01;

        const auto m00_m01_n00_n01_to_m0_n0_block_cluster_adaptor =
            make_single_stage_tensor_adaptor(
                make_tuple(make_unmerge_transform(make_tuple(M00, M01)),
                           make_unmerge_transform(make_tuple(N00, N01))),
                make_tuple(Sequence<0>{}, Sequence<1>{}),
                make_tuple(Sequence<0, 2>{}, Sequence<1, 3>{}));

        const auto c_blockid_to_m00_m01_n00_n01_block_cluster_adaptor =
            make_single_stage_tensor_adaptor(
                make_tuple(make_merge_transform(make_tuple(M00, N00, M01, N01))),
                make_tuple(Sequence<0, 1, 2, 3>{}),
                make_tuple(Sequence<0>{}));

        const auto c_blockid_to_m0_n0_block_cluster_adaptor =
            chain_tensor_adaptors(m00_m01_n00_n01_to_m0_n0_block_cluster_adaptor,
                                  c_blockid_to_m00_m01_n00_n01_block_cluster_adaptor);

        return c_blockid_to_m0_n0_block_cluster_adaptor;
    }
    using CGridDescriptor_MBlock_MRepeat_MWaveMPerXdl_NBlock_NRepeat_NWaveNPerXdl =
        remove_cvref_t<decltype(
            MakeCGridDescriptor_MBlock_MRepeat_MWaveMPerXdl_NBlock_NRepeat_NWaveNPerXdl(
                CGridDesc_M_N{}))>;

    using Block2CTileMap = remove_cvref_t<decltype(MakeBlock2CTileMap(CGridDesc_M_N{}, 1, 1))>;

    template <bool HasMainKBlockLoop>
    __device__ static void
    Run(const FloatAB* __restrict__ p_a_grid,
        const FloatAB* __restrict__ p_b_grid,
        FloatC* __restrict__ p_c_grid,
        void* __restrict__ p_shared,
        const AGridDesc_K0_M_K1& a_grid_desc_k0_m_k1,
        const BGridDesc_K0_N_K1& b_grid_desc_k0_n_k1,
        const CGridDescriptor_MBlock_MRepeat_MWaveMPerXdl_NBlock_NRepeat_NWaveNPerXdl&
            c_grid_desc_mblock_mrepeat_mwavemperxdl_nblock_nrepeat_nwavenperxdl,
        const AElementwiseOperation& a_element_op,
        const BElementwiseOperation& b_element_op,
        const CElementwiseOperation& c_element_op,
        const Block2CTileMap& block_2_ctile_map)
    {
        const auto a_grid_buf = make_dynamic_buffer<AddressSpaceEnum_t::Global>(
            p_a_grid, a_grid_desc_k0_m_k1.GetElementSpaceSize());
        const auto b_grid_buf = make_dynamic_buffer<AddressSpaceEnum_t::Global>(
            p_b_grid, b_grid_desc_k0_n_k1.GetElementSpaceSize());
        auto c_grid_buf = make_dynamic_buffer<AddressSpaceEnum_t::Global>(
            p_c_grid,
            c_grid_desc_mblock_mrepeat_mwavemperxdl_nblock_nrepeat_nwavenperxdl
                .GetElementSpaceSize());

        const auto K0 = a_grid_desc_k0_m_k1.GetLength(I0);

        // divide block work by [M, N]
        const auto block_work_idx =
            block_2_ctile_map.CalculateBottomIndex(make_multi_index(get_block_1d_id()));

        // HACK: this force m/n_block_data_idx_on_grid into SGPR
        const index_t m_block_data_idx_on_grid =
            __builtin_amdgcn_readfirstlane(block_work_idx[I0] * MPerBlock);

        const index_t n_block_data_idx_on_grid =
            __builtin_amdgcn_readfirstlane(block_work_idx[I1] * NPerBlock);

        // lds max alignment
        constexpr auto max_lds_align = K1;

        // A matrix in LDS memory, dst of blockwise copy
        constexpr auto a_block_desc_k0_m_k1 = [&]() {
            if constexpr(ABlockLdsExtraM)
            {
                return make_naive_tensor_descriptor(
                    make_tuple(Number<K0PerBlock>{}, Number<MPerBlock>{}, K1),
                    make_tuple(Number<MPerBlock + 1>{} * K1, K1, I1));
            }
            else
            {
                return make_naive_tensor_descriptor_aligned(
                    make_tuple(Number<K0PerBlock>{}, Number<MPerBlock>{}, K1), max_lds_align);
            }
        }();

        // B matrix in LDS memory, dst of blockwise copy
        constexpr auto b_block_desc_k0_n_k1 = [&]() {
            if constexpr(BBlockLdsExtraN)
            {
                return make_naive_tensor_descriptor(
                    make_tuple(Number<K0PerBlock>{}, Number<NPerBlock>{}, K1),
                    make_tuple(Number<NPerBlock + 1>{} * K1, K1, I1));
            }
            else
            {
                return make_naive_tensor_descriptor_aligned(
                    make_tuple(Number<K0PerBlock>{}, Number<NPerBlock>{}, K1), max_lds_align);
            }
        }();

        // A matrix blockwise copy
        auto a_blockwise_copy =
            BlockwiseTensorSliceTransfer_v4<BlockSize,
#if 0
                                            AElementwiseOperation,
#else
                                            ck::tensor_operation::element_wise::PassThrough,
#endif
                                            InMemoryDataOperationEnum_t::Set,
                                            Sequence<K0PerBlock, MPerBlock, K1>,
                                            ABlockTransferThreadSliceLengths_K0_M_K1,
                                            ABlockTransferThreadClusterLengths_K0_M_K1,
                                            ABlockTransferThreadClusterArrangeOrder,
                                            FloatAB,
                                            FloatAB,
                                            decltype(a_grid_desc_k0_m_k1),
                                            decltype(a_block_desc_k0_m_k1),
                                            ABlockTransferSrcAccessOrder,
                                            Sequence<1, 0, 2>,
                                            ABlockTransferSrcVectorDim,
                                            2,
                                            ABlockTransferSrcScalarPerVector,
                                            ABlockTransferDstScalarPerVector_K1,
                                            1,
                                            1,
                                            AThreadTransferSrcResetCoordinateAfterRun,
                                            true>(a_grid_desc_k0_m_k1,
                                                  make_multi_index(0, m_block_data_idx_on_grid, 0),
                                                  a_block_desc_k0_m_k1,
                                                  make_multi_index(0, 0, 0),
                                                  a_element_op);

        // B matrix blockwise copy
        auto b_blockwise_copy =
            BlockwiseTensorSliceTransfer_v4<BlockSize,
#if 0
                                            BElementwiseOperation,
#else
                                            ck::tensor_operation::element_wise::PassThrough,
#endif
                                            InMemoryDataOperationEnum_t::Set,
                                            Sequence<K0PerBlock, NPerBlock, K1>,
                                            BBlockTransferThreadSliceLengths_K0_N_K1,
                                            BBlockTransferThreadClusterLengths_K0_N_K1,
                                            BBlockTransferThreadClusterArrangeOrder,
                                            FloatAB,
                                            FloatAB,
                                            decltype(b_grid_desc_k0_n_k1),
                                            decltype(b_block_desc_k0_n_k1),
                                            BBlockTransferSrcAccessOrder,
                                            Sequence<1, 0, 2>,
                                            BBlockTransferSrcVectorDim,
                                            2,
                                            BBlockTransferSrcScalarPerVector,
                                            BBlockTransferDstScalarPerVector_K1,
                                            1,
                                            1,
                                            BThreadTransferSrcResetCoordinateAfterRun,
                                            true>(b_grid_desc_k0_n_k1,
                                                  make_multi_index(0, n_block_data_idx_on_grid, 0),
                                                  b_block_desc_k0_n_k1,
                                                  make_multi_index(0, 0, 0),
                                                  b_element_op);

        // GEMM definition
        //   c_mtx += transpose(a_mtx) * b_mtx
        //     a_mtx[K0PerBlock, MPerBlock] is in LDS
        //     b_mtx[K0PerBlock, NPerBlock] is in LDS
        //     c_mtx[MPerBlock, NPerBlock] is distributed among threads, and saved in
        //       register
        // sanity check

        auto blockwise_gemm =
            BlockwiseGemmXdlops_k0mk1_k0nk1_m0n0m1n1m2m3m4n2_v1<BlockSize,
                                                                FloatAB,
                                                                FloatAcc,
                                                                decltype(a_block_desc_k0_m_k1),
                                                                decltype(b_block_desc_k0_n_k1),
                                                                MPerXdl,
                                                                NPerXdl,
                                                                MRepeat,
                                                                NRepeat,
                                                                K1>{};

        auto c_thread_buf = blockwise_gemm.GetCThreadBuffer();

        // LDS allocation for A and B: be careful of alignment
        constexpr auto a_block_space_size =
            math::integer_least_multiple(a_block_desc_k0_m_k1.GetElementSpaceSize(), max_lds_align);

        auto a_block_buf = make_dynamic_buffer<AddressSpaceEnum_t::Lds>(
            static_cast<FloatAB*>(p_shared), a_block_desc_k0_m_k1.GetElementSpaceSize());

        auto b_block_buf = make_dynamic_buffer<AddressSpaceEnum_t::Lds>(
            static_cast<FloatAB*>(p_shared) + a_block_space_size,
            b_block_desc_k0_n_k1.GetElementSpaceSize());

        constexpr auto a_block_slice_copy_step = make_multi_index(K0PerBlock, 0, 0);
        constexpr auto b_block_slice_copy_step = make_multi_index(K0PerBlock, 0, 0);

        // hack to control index calculation when iterating over A and B matrix for threadwise copy
        constexpr auto a_k0_m_k1_grid_step_hacks = AGridStepHacks{};
        constexpr auto b_k0_n_k1_grid_step_hacks = BGridStepHacks{};

        // hack to control index calculation when move slice window for A and B matrix for
        // threadwise copy
        constexpr auto a_k0_m_k1_grid_move_slice_window_step_hack = AGridMoveSliceWindowStepHacks{};
        constexpr auto b_k0_n_k1_grid_move_slice_window_step_hack = BGridMoveSliceWindowStepHacks{};

        // preload data into LDS
        {
            a_blockwise_copy.RunRead(a_grid_desc_k0_m_k1, a_grid_buf, a_k0_m_k1_grid_step_hacks);
            b_blockwise_copy.RunRead(b_grid_desc_k0_n_k1, b_grid_buf, b_k0_n_k1_grid_step_hacks);

            a_blockwise_copy.RunWrite(a_block_desc_k0_m_k1, a_block_buf);
            b_blockwise_copy.RunWrite(b_block_desc_k0_n_k1, b_block_buf);
        }

        // main body
        index_t k0_block_data_begin = 0;

        c_thread_buf.Clear();

        if constexpr(HasMainKBlockLoop)
        {
            do
            {
                a_blockwise_copy.MoveSrcSliceWindow(a_grid_desc_k0_m_k1,
                                                    a_block_slice_copy_step,
                                                    a_k0_m_k1_grid_move_slice_window_step_hack);
                b_blockwise_copy.MoveSrcSliceWindow(b_grid_desc_k0_n_k1,
                                                    b_block_slice_copy_step,
                                                    b_k0_n_k1_grid_move_slice_window_step_hack);

                a_blockwise_copy.RunRead(
                    a_grid_desc_k0_m_k1, a_grid_buf, a_k0_m_k1_grid_step_hacks);

                block_sync_lds();

                b_blockwise_copy.RunRead(
                    b_grid_desc_k0_n_k1, b_grid_buf, b_k0_n_k1_grid_step_hacks);

                blockwise_gemm.Run(a_block_buf, b_block_buf, c_thread_buf);

                block_sync_lds();

                a_blockwise_copy.RunWrite(a_block_desc_k0_m_k1, a_block_buf);
                b_blockwise_copy.RunWrite(b_block_desc_k0_n_k1, b_block_buf);

                k0_block_data_begin += K0PerBlock;
            } while(k0_block_data_begin < (K0 - K0PerBlock));
        }

        // tail
        {
            block_sync_lds();

            blockwise_gemm.Run(a_block_buf, b_block_buf, c_thread_buf);
        }

        // shuffle and write out
        {
#if 1
            // TODO: make it tunable
            constexpr index_t MRepeatPerShuffle_CCopy = 1;
            constexpr index_t NRepeatPerShuffle_CCopy = 1;

            // TODO: this is hardcoded, only works for BlockSize = 256. fix it!
            constexpr index_t MRepeatThread_CCopy = 1;
            constexpr index_t MThread_CCopy       = 32;
            constexpr index_t NRepeatThread_CCopy = 1;
            constexpr index_t NThread_CCopy       = 8;

            // vector length for blockwise copy from LDS to global
            constexpr index_t NScalarPerVector_CCopy = 8;
#else
            // TODO: make it tunable
            constexpr index_t MRepeatPerShuffle_CCopy = 1;
            constexpr index_t NRepeatPerShuffle_CCopy = 2;

            // TODO: this is hardcoded, only works for BlockSize = 256. fix it!
            constexpr index_t MRepeatThread_CCopy = 1;
            constexpr index_t MThread_CCopy       = 16;
            constexpr index_t NRepeatThread_CCopy = 2;
            constexpr index_t NThread_CCopy       = 8;

            // vector length for blockwise copy from LDS to global
            constexpr index_t NScalarPerVector_CCopy = 8;
#endif

            static_assert(MRepeat % MRepeatPerShuffle_CCopy == 0 &&
                              NRepeat % NRepeatPerShuffle_CCopy == 0,
                          "wrong!");

            constexpr index_t MWave = MPerBlock / (MRepeat * MPerXdl);
            constexpr index_t NWave = NPerBlock / (NRepeat * NPerXdl);

            constexpr index_t MPerBlock_CCopy = MWave * MPerXdl;
            constexpr index_t NPerBlock_CCopy = NWave * NPerXdl;

            constexpr index_t MPerThread_CCopy = MPerBlock_CCopy / MThread_CCopy;
            constexpr index_t NPerThread_CCopy = NPerBlock_CCopy / NThread_CCopy;

            constexpr index_t MRepeatPerThread_CCopy =
                MRepeatPerShuffle_CCopy / MRepeatThread_CCopy;
            constexpr index_t NRepeatPerThread_CCopy =
                NRepeatPerShuffle_CCopy / NRepeatThread_CCopy;

            // TODO: hacky, fix it!
            constexpr auto c_thread_desc_m0_n0_m1_n1_m2_m3_m4_n2 =
                blockwise_gemm.GetCThreadDescriptor_M0_N0_M1_N1_M2_M3_M4_N2();

            // TODO: hacky, fix it!
            // c_block_desc_m0_n0_m1_n1_m2_m3_m4_n2_tmp is only used to get lengths
            constexpr auto c_block_desc_m0_n0_m1_n1_m2_m3_m4_n2_tmp =
                blockwise_gemm.GetCBlockDescriptor_M0_N0_M1_N1_M2_M3_M4_N2();

            constexpr auto M0 = c_block_desc_m0_n0_m1_n1_m2_m3_m4_n2_tmp.GetLength(I0);
            constexpr auto N0 = c_block_desc_m0_n0_m1_n1_m2_m3_m4_n2_tmp.GetLength(I1);
            constexpr auto M1 = c_block_desc_m0_n0_m1_n1_m2_m3_m4_n2_tmp.GetLength(I2);
            constexpr auto N1 = c_block_desc_m0_n0_m1_n1_m2_m3_m4_n2_tmp.GetLength(I3);
            constexpr auto M2 = c_block_desc_m0_n0_m1_n1_m2_m3_m4_n2_tmp.GetLength(I4);
            constexpr auto M3 = c_block_desc_m0_n0_m1_n1_m2_m3_m4_n2_tmp.GetLength(I5);
            constexpr auto M4 = c_block_desc_m0_n0_m1_n1_m2_m3_m4_n2_tmp.GetLength(I6);
            constexpr auto N2 = c_block_desc_m0_n0_m1_n1_m2_m3_m4_n2_tmp.GetLength(I7);

            constexpr auto c_block_desc_mblock_mrepeat_mwavemperxdl_nblock_nrepeat_nwavenperxdl =
                make_naive_tensor_descriptor_packed(make_tuple(I1,
                                                               Number<MRepeatPerShuffle_CCopy>{},
                                                               Number<MWave * MPerXdl>{},
                                                               I1,
                                                               Number<NRepeatPerShuffle_CCopy>{},
                                                               Number<NWave * NPerXdl>{}));

            auto c_block_buf = make_dynamic_buffer<AddressSpaceEnum_t::Lds>(
                static_cast<FloatC*>(p_shared),
                c_block_desc_mblock_mrepeat_mwavemperxdl_nblock_nrepeat_nwavenperxdl
                    .GetElementSpaceSize());

            constexpr auto c_block_desc_m0_n0_m1_n1_m2_m3_m4_n2 = transform_tensor_descriptor(
                c_block_desc_mblock_mrepeat_mwavemperxdl_nblock_nrepeat_nwavenperxdl,
                make_tuple(make_freeze_transform(I0), // freeze mblock
                           make_pass_through_transform(
                               Number<MRepeatPerShuffle_CCopy>{}), // M0 (MRepeat) per shuffle
                           make_unmerge_transform(
                               make_tuple(M1, M2, M3, M4)), // M1 = MWave, M2 * M3 * M4 = MPerXdl
                           make_freeze_transform(I0),       // freeze nblock
                           make_pass_through_transform(
                               Number<NRepeatPerShuffle_CCopy>{}), // N0 (NRepeat) per shuffle
                           make_unmerge_transform(
                               make_tuple(N1, N2))), // M1 = MWave, M2 * M3 * M4 = MPerXdl
                make_tuple(Sequence<0>{},
                           Sequence<1>{},
                           Sequence<2>{},
                           Sequence<3>{},
                           Sequence<4>{},
                           Sequence<5>{}),
                make_tuple(Sequence<>{},
                           Sequence<0>{},
                           Sequence<2, 4, 5, 6>{},
                           Sequence<>{},
                           Sequence<1>{},
                           Sequence<3, 7>{})

            );

            // calculate origin of thread output tensor on global memory
            //     blockwise GEMM c matrix starting index
            const auto c_thread_mtx_on_block =
                blockwise_gemm.CalculateCThreadOriginDataIndex(I0, I0, I0, I0);

            const index_t m_thread_data_on_block = c_thread_mtx_on_block[I0];
            const index_t n_thread_data_on_block = c_thread_mtx_on_block[I1];

            const auto m_thread_data_on_block_to_m0_m1_m2_m3_m4_adaptor =
                make_single_stage_tensor_adaptor(
                    make_tuple(make_merge_transform(make_tuple(M0, M1, M2, M3, M4))),
                    make_tuple(Sequence<0, 1, 2, 3, 4>{}),
                    make_tuple(Sequence<0>{}));

            const auto m_thread_data_on_block_idx =
                m_thread_data_on_block_to_m0_m1_m2_m3_m4_adaptor.CalculateBottomIndex(
                    make_multi_index(m_thread_data_on_block));

            const auto n_thread_data_on_block_to_n0_n1_n2_adaptor =
                make_single_stage_tensor_adaptor(
                    make_tuple(make_merge_transform(make_tuple(N0, N1, N2))),
                    make_tuple(Sequence<0, 1, 2>{}),
                    make_tuple(Sequence<0>{}));

            const auto n_thread_data_on_block_idx =
                n_thread_data_on_block_to_n0_n1_n2_adaptor.CalculateBottomIndex(
                    make_multi_index(n_thread_data_on_block));

            // VGPR to LDS
            auto c_thread_copy_vgpr_to_lds = ThreadwiseTensorSliceTransfer_v1r3<
                FloatAcc,
                FloatC,
                decltype(c_thread_desc_m0_n0_m1_n1_m2_m3_m4_n2),
                decltype(c_block_desc_m0_n0_m1_n1_m2_m3_m4_n2),
                ck::tensor_operation::element_wise::PassThrough,
                Sequence<MRepeatPerShuffle_CCopy, NRepeatPerShuffle_CCopy, I1, I1, M2, I1, M4, I1>,
                Sequence<0, 1, 2, 3, 4, 5, 6, 7>,
                7,
                1,
                InMemoryDataOperationEnum_t::Set,
                1,
                true>{c_block_desc_m0_n0_m1_n1_m2_m3_m4_n2,
                      make_multi_index(0,
                                       0,
                                       m_thread_data_on_block_idx[I1],
                                       n_thread_data_on_block_idx[I1],
                                       m_thread_data_on_block_idx[I2],
                                       m_thread_data_on_block_idx[I3],
                                       m_thread_data_on_block_idx[I4],
                                       n_thread_data_on_block_idx[I2]),
                      ck::tensor_operation::element_wise::PassThrough{}};

            auto c_block_copy_lds_to_global = BlockwiseTensorSliceTransfer_v4<
                BlockSize,                                       // index_t BlockSize,
                ck::tensor_operation::element_wise::PassThrough, // SrcElementwiseOperation,
                CGlobalMemoryDataOperation,                      // DstInMemOp,
                Sequence<1,
                         MRepeatPerShuffle_CCopy,
                         MPerBlock_CCopy,
                         1,
                         NRepeatPerShuffle_CCopy,
                         NPerBlock_CCopy>, // BlockSliceLengths,
                Sequence<1,
                         MRepeatPerShuffle_CCopy,
                         MPerThread_CCopy,
                         1,
                         NRepeatPerShuffle_CCopy,
                         NPerThread_CCopy>, // ThreadSliceLengths,
                Sequence<1,
                         MRepeatPerThread_CCopy,
                         MThread_CCopy,
                         1,
                         NRepeatPerThread_CCopy,
                         NThread_CCopy>,    // ThreadClusterLengths,
                Sequence<0, 1, 2, 3, 4, 5>, // typename ThreadClusterArrangeOrder,
                FloatC,                     // typename SrcData,
                FloatC,                     // typename DstData,
                decltype(c_block_desc_mblock_mrepeat_mwavemperxdl_nblock_nrepeat_nwavenperxdl),
                decltype(c_grid_desc_mblock_mrepeat_mwavemperxdl_nblock_nrepeat_nwavenperxdl),
                Sequence<0, 1, 2, 3, 4, 5>, // typename SrcDimAccessOrder,
                Sequence<0, 1, 2, 3, 4, 5>, // typename DstDimAccessOrder,
                5,                          // index_t SrcVectorDim,
                5,                          // index_t DstVectorDim,
                NScalarPerVector_CCopy,     // index_t SrcScalarPerVector,
                NScalarPerVector_CCopy,     // index_t DstScalarPerVector,
                1,                          // index_t SrcScalarStrideInVector,
                1,                          // index_t DstScalarStrideInVector,
                true,                       // bool ThreadTransferSrcResetCoordinateAfterRun,
                false>                      // bool ThreadTransferDstResetCoordinateAfterRun>
                {c_block_desc_mblock_mrepeat_mwavemperxdl_nblock_nrepeat_nwavenperxdl,
                 make_multi_index(0, 0, 0, 0, 0, 0),
                 c_grid_desc_mblock_mrepeat_mwavemperxdl_nblock_nrepeat_nwavenperxdl,
                 make_multi_index(block_work_idx[I0], 0, 0, block_work_idx[I1], 0, 0),
                 ck::tensor_operation::element_wise::PassThrough{}};

            constexpr auto mrepeat_forward_step =
                make_multi_index(0, MRepeatPerShuffle_CCopy, 0, 0, 0, 0);
            constexpr auto nrepeat_forward_step =
                make_multi_index(0, 0, 0, 0, NRepeatPerShuffle_CCopy, 0);
            constexpr auto nrepeat_backward_step =
                make_multi_index(0, 0, 0, 0, -NRepeatPerShuffle_CCopy, 0);

            static_for<0, MRepeat, MRepeatPerShuffle_CCopy>{}([&](auto mrepeat_iter) {
                constexpr auto mrepeat = mrepeat_iter;

                static_for<0, NRepeat, NRepeatPerShuffle_CCopy>{}([&](auto nrepeat_iter) {
                    constexpr bool nrepeat_forward_sweep =
                        (mrepeat % (2 * MRepeatPerShuffle_CCopy) == 0);

                    constexpr index_t nrepeat_value =
                        nrepeat_forward_sweep ? nrepeat_iter
                                              : (NRepeat - nrepeat_iter - NRepeatPerShuffle_CCopy);

                    constexpr auto nrepeat = Number<nrepeat_value>{};

                    // make sure it's safe to do ds_write
                    block_sync_lds();

                    // VGPR to LDS
                    c_thread_copy_vgpr_to_lds.Run(
                        c_thread_desc_m0_n0_m1_n1_m2_m3_m4_n2,
                        make_tuple(mrepeat, nrepeat, I0, I0, I0, I0, I0, I0),
                        c_thread_buf,
                        c_block_desc_m0_n0_m1_n1_m2_m3_m4_n2,
                        c_block_buf);

                    // make sure it's safe to do ds_read
                    block_sync_lds();

                    // LDS to global
                    c_block_copy_lds_to_global.Run(
                        c_block_desc_mblock_mrepeat_mwavemperxdl_nblock_nrepeat_nwavenperxdl,
                        c_block_buf,
                        c_grid_desc_mblock_mrepeat_mwavemperxdl_nblock_nrepeat_nwavenperxdl,
                        c_grid_buf);

                    // move on nrepeat dimension
                    if constexpr(nrepeat_forward_sweep &&
                                 (nrepeat < NRepeat - NRepeatPerShuffle_CCopy))
                    {
                        c_block_copy_lds_to_global.MoveDstSliceWindow(
                            c_grid_desc_mblock_mrepeat_mwavemperxdl_nblock_nrepeat_nwavenperxdl,
                            nrepeat_forward_step);
                    }
                    else if constexpr((!nrepeat_forward_sweep) && (nrepeat > 0))
                    {
                        c_block_copy_lds_to_global.MoveDstSliceWindow(
                            c_grid_desc_mblock_mrepeat_mwavemperxdl_nblock_nrepeat_nwavenperxdl,
                            nrepeat_backward_step);
                    }
                });

                // move on mrepeat dimension
                if constexpr(mrepeat < MRepeat - MRepeatPerShuffle_CCopy)
                {
                    c_block_copy_lds_to_global.MoveDstSliceWindow(
                        c_grid_desc_mblock_mrepeat_mwavemperxdl_nblock_nrepeat_nwavenperxdl,
                        mrepeat_forward_step);
                }
            });
        }
    }
};

} // namespace ck
#endif
