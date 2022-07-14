#ifndef CK_GRIDWISE_DIRECT_CONV_BIAS_ACTIVATION_ADD_AVX2_HPP
#define CK_GRIDWISE_DIRECT_CONV_BIAS_ACTIVATION_ADD_AVX2_HPP

#include "ck/utility/common_header.hpp"
#include "ck/tensor_description/multi_index_transform_helper.hpp"
#include "ck/tensor_description/tensor_descriptor.hpp"
#include "ck/tensor_description/tensor_descriptor_helper.hpp"
#include "ck/tensor_operation/cpu/block/blockwise_gemm_avx2.hpp"
#include "ck/tensor_operation/cpu/thread/threadwise_tensor_slice_transfer_avx2.hpp"
#include "ck/tensor_operation/cpu/thread/threadwise_tensor_slice_transfer_avx2_specialization.hpp"
#include "ck/utility/dynamic_buffer_cpu.hpp"
#include "ck/utility/envvar.hpp"
#include <utility>
#include <unistd.h>
#include <omp.h>
#include <pthread.h>

namespace ck {
namespace cpu {

template <typename GridwiseDirectConv,
          typename FloatA,
          typename FloatB,
          typename FloatC,
          typename FloatC0,
          typename FloatC1,
          typename AGridDesc,
          typename BGridDesc,
          typename CGridDesc,
          typename C0GridDesc,
          typename C1GridDesc,
          typename AElementwiseOperation,
          typename BElementwiseOperation,
          typename CElementwiseOperation>
void kernel_direct_conv_nhwc_bias_activation_add_avx_mxn(
    const GridwiseDirectConv& gridwise_direct_conv,
    const FloatA* __restrict__ p_a_grid,
    const FloatB* __restrict__ p_b_grid,
    FloatC* __restrict__ p_c_grid,
    const FloatC0* __restrict__ p_c0_grid,
    const FloatC1* __restrict__ p_c1_grid,
    const AGridDesc& a_grid_desc,
    const BGridDesc& b_grid_desc,
    const CGridDesc& c_grid_desc,
    const C0GridDesc& c0_grid_desc,
    const C1GridDesc& c1_grid_desc,
    ck::index_t N,
    ck::index_t K,
    ck::index_t C,
    std::vector<ck::index_t> input_spatial_lengths,
    std::vector<ck::index_t> filter_spatial_lengths,
    std::vector<ck::index_t> output_spatial_lengths,
    std::vector<ck::index_t> conv_filter_strides,
    std::vector<ck::index_t> conv_filter_dilations,
    std::vector<ck::index_t> input_left_pads,
    std::vector<ck::index_t> input_right_pads,
    const AElementwiseOperation& a_element_op,
    const BElementwiseOperation& b_element_op,
    const CElementwiseOperation& c_element_op)
{
    gridwise_direct_conv.Run(p_a_grid,
                             p_b_grid,
                             p_c_grid,
                             p_c0_grid,
                             p_c1_grid,
                             a_grid_desc,
                             b_grid_desc,
                             c_grid_desc,
                             c0_grid_desc,
                             c1_grid_desc,
                             N,
                             K,
                             C,
                             input_spatial_lengths,
                             filter_spatial_lengths,
                             output_spatial_lengths,
                             conv_filter_strides,
                             conv_filter_dilations,
                             input_left_pads,
                             input_right_pads,
                             a_element_op,
                             b_element_op,
                             c_element_op);
}

template <typename FloatA,
          typename FloatB,
          typename FloatC,
          typename FloatC0,
          typename FloatC1,
          typename AGridDesc,
          typename BGridDesc,
          typename CGridDesc,
          typename C0GridDesc,
          typename C1GridDesc,
          typename AElementwiseOperation,
          typename BElementwiseOperation,
          typename CElementwiseOperation,
          typename ThreadwiseGemm_Dispatch,
          typename AThreadwiseCopy,
          typename BThreadwiseCopy,
          typename CThreadwiseCopy,
          typename ThreadMNAccessOrder, // how we acces gemm MN to utilize micro kernel
          bool UseALocalBuffer,
          bool UseBLocalBuffer,
          bool UseCLocalBuffer // if true, will allocate a buffer and write to it in kernel, then
                               // copy back to block buffer (need CThreadwiseCopy).
                               // if false, will write to C directly (no need CThreadwiseCopy)
          >
struct GridwiseDirectConvNHWCBiasActivationAddAvx2
{
    ck::tensor_operation::cpu::device::DeviceConvFwdDynamicTunable dynamic_tunable;
    static constexpr auto I0 = Number<0>{};
    static constexpr auto I1 = Number<1>{};

    // static constexpr auto Avx2RegisterVector = 8;   // 8 floats
    static constexpr index_t MemAlignmentByte = 32; // 256bit

    GridwiseDirectConvNHWCBiasActivationAddAvx2(
        const ck::tensor_operation::cpu::device::DeviceConvFwdDynamicTunable dynamic_tunable_)
        : dynamic_tunable(dynamic_tunable_)
    {
    }

    static auto GetABlockDescriptor(const ck::index_t m_per_blk,
                                    const ck::index_t k_per_blk,
                                    const AGridDesc& a_grid_desc)
    {
        if constexpr(UseALocalBuffer)
        {
            if constexpr(std::is_same<typename ThreadwiseGemm_Dispatch::MatrixALayout,
                                      ck::tensor_layout::gemm::RowMajor>::value)
            {
                // A : M, K
                auto a_block_desc_m_k =
                    make_naive_tensor_descriptor_packed(make_tuple(m_per_blk, k_per_blk));
                return a_block_desc_m_k;
            }
            else
            {
                // A : K, M
                auto a_block_desc_k_m = make_naive_tensor_descriptor_packed(
                    make_tuple(k_per_blk,
                               math::integer_least_multiple(
                                   m_per_blk, ThreadwiseGemm_Dispatch::MatrixAMinVectorSize)));
                return a_block_desc_k_m;
            }
        }
        else
        {
            return a_grid_desc;
        }
    }

    static auto GetBBlockDescriptor(const ck::index_t k_per_blk,
                                    const ck::index_t n_per_blk,
                                    const BGridDesc& b_grid_desc)
    {
        if constexpr(UseBLocalBuffer)
        {
            // n_per_blk should be 8x
            if constexpr(std::is_same<typename ThreadwiseGemm_Dispatch::MatrixBLayout,
                                      ck::tensor_layout::gemm::RowMajor>::value)
            {
                // B : K, N
                auto b_block_desc_k_n =
                    make_naive_tensor_descriptor_packed(make_tuple(k_per_blk, n_per_blk));
                return b_block_desc_k_n;
            }
            else
            {
                // B : N/8, K, N8
                auto b_block_desc_n0_k_n1 = make_naive_tensor_descriptor_packed(
                    make_tuple(math::integer_divide_ceil(
                                   n_per_blk, ThreadwiseGemm_Dispatch::MatrixBMinVectorSize),
                               k_per_blk,
                               ThreadwiseGemm_Dispatch::MatrixBMinVectorSize));
                return b_block_desc_n0_k_n1;
            }
        }
        else
        {
            return b_grid_desc;
        }
    }

    static auto GetCBlockDescriptor(const ck::index_t m_per_blk,
                                    const ck::index_t n_per_blk,
                                    const CGridDesc& c_grid_desc)
    {
        if constexpr(UseCLocalBuffer)
        {
            return make_naive_tensor_descriptor_packed(make_tuple(m_per_blk, n_per_blk));
        }
        else
            return c_grid_desc;
    }

    static auto GetASliceLength(const ck::index_t m_per_blk, const ck::index_t k_per_blk)
    {
        if constexpr(std::is_same<typename ThreadwiseGemm_Dispatch::MatrixALayout,
                                  ck::tensor_layout::gemm::RowMajor>::value)
        {
            // A : M, K
            return ck::make_multi_index(m_per_blk, k_per_blk);
        }
        else
        {
            // A : K, M
            return ck::make_multi_index(
                k_per_blk,
                math::integer_least_multiple(m_per_blk,
                                             ThreadwiseGemm_Dispatch::MatrixAMinVectorSize));
        }
    }

    static auto GetBSliceLength(const ck::index_t k_per_blk, const ck::index_t n_per_blk)
    {
        // n_per_blk should be 8x
        if constexpr(std::is_same<typename ThreadwiseGemm_Dispatch::MatrixBLayout,
                                  ck::tensor_layout::gemm::RowMajor>::value)
        {
            // B : K, N
            return ck::make_multi_index(
                k_per_blk,
                math::integer_least_multiple(n_per_blk,
                                             ThreadwiseGemm_Dispatch::MatrixBMinVectorSize));
        }
        else
        {
            // B : N/8, K, N8
            return ck::make_multi_index(
                math::integer_divide_ceil(n_per_blk, ThreadwiseGemm_Dispatch::MatrixBMinVectorSize),
                k_per_blk,
                ThreadwiseGemm_Dispatch::MatrixBMinVectorSize);
        }
    }

    static auto GetCSliceLength(const ck::index_t m_per_blk, const ck::index_t n_per_blk)
    {
        return ck::make_multi_index(m_per_blk, n_per_blk);
    }

    static auto GetAIndex(const ck::index_t i_m, const ck::index_t i_k)
    {
        if constexpr(std::is_same<typename ThreadwiseGemm_Dispatch::MatrixALayout,
                                  ck::tensor_layout::gemm::RowMajor>::value)
        {
            // A : M, K
            return ck::make_multi_index(i_m, i_k);
        }
        else
        {
            // A : K, M
            return ck::make_multi_index(i_k, i_m);
        }
    }

    static auto GetBIndex(const ck::index_t i_k, const ck::index_t i_n)
    {
        // i_n should be 8x
        if constexpr(std::is_same<typename ThreadwiseGemm_Dispatch::MatrixBLayout,
                                  ck::tensor_layout::gemm::RowMajor>::value)
        {
            // B : K, N
            return ck::make_multi_index(i_k, i_n);
        }
        else
        {
            // B : N/8, K, N8
            return ck::make_multi_index(i_n / ThreadwiseGemm_Dispatch::MatrixBMinVectorSize,
                                        i_k,
                                        i_n % ThreadwiseGemm_Dispatch::MatrixBMinVectorSize);
        }
    }

    static auto GetCIndex(const ck::index_t i_m, const ck::index_t i_n)
    {
        return ck::make_multi_index(i_m, i_n);
    }

    bool CheckValidity(const AGridDesc& a_grid_desc,
                       const BGridDesc& b_grid_desc,
                       const CGridDesc& c_grid_desc)
    {
        // TODO: also check validity of all components (blockwise-copy, threadwise-copy, etc)
        bool is_valid    = true;
        const auto GemmN = c_grid_desc.GetLength(I1);
        if constexpr(UseCLocalBuffer)
        {
            if(dynamic_tunable.loop_over_spec ==
                   ck::tensor_operation::cpu::device::
                       ConvolutionForwardBlockLoopOverSpecialization_t::LoopOver_MKN &&
               dynamic_tunable.n_per_block < GemmN)
                is_valid &= false;
        }
        else
        {
            // TODO: need check c grid is simple transform?
            if(GemmN % 8 != 0)
                is_valid &= false;
        }
        return is_valid;
    }

    static intptr_t
    GetBBlockStartOffset(const BGridDesc& b_grid_desc, const intptr_t i_k, const intptr_t i_n)
    {
        if constexpr(std::is_same<typename ThreadwiseGemm_Dispatch::MatrixBLayout,
                                  ck::tensor_layout::gemm::RowMajor>::value)
        {
            // K * N
            return i_n;
        }
        else
        {
            // N/8 * K * 8
            return i_n * b_grid_desc.GetTransforms()[Number<0>{}].GetUpperLengths()[Number<1>{}] +
                   i_k * 8;
        }
    }

    static intptr_t
    GetCBlockStartOffset(const CGridDesc& c_grid_desc, const intptr_t i_m, const intptr_t i_n)
    {
        return i_m * c_grid_desc.GetTransforms()[Number<0>{}].GetUpperLengths()[Number<1>{}] + i_n;
    }

    static intptr_t GetBLeadingElement(const BGridDesc& b_grid_desc)
    {
        if constexpr(std::is_same<typename ThreadwiseGemm_Dispatch::MatrixBLayout,
                                  ck::tensor_layout::gemm::RowMajor>::value)
        {
            // K * N
            return b_grid_desc.GetTransforms()[Number<0>{}].GetUpperLengths()[Number<1>{}];
        }
        else
        {
            // N/8 * K * 8
            return b_grid_desc.GetLength(Number<1>{}) * b_grid_desc.GetLength(Number<2>{});
        }
    }

    static intptr_t GetCLeadingElement(const CGridDesc& c_grid_desc)
    {
        return c_grid_desc.GetTransforms()[Number<0>{}].GetUpperLengths()[Number<1>{}];
    }

    void Run(const FloatA* __restrict__ p_a_grid,
             const FloatB* __restrict__ p_b_grid,
             FloatC* __restrict__ p_c_grid,
             const FloatC0* __restrict__ p_c0_grid,
             const FloatC1* __restrict__ p_c1_grid,
             const AGridDesc& a_grid_desc,
             const BGridDesc& b_grid_desc,
             const CGridDesc& c_grid_desc,
             const C0GridDesc& c0_grid_desc,
             const C1GridDesc& c1_grid_desc,
             ck::index_t N,
             ck::index_t K,
             ck::index_t C,
             std::vector<ck::index_t> input_spatial_lengths,
             std::vector<ck::index_t> filter_spatial_lengths,
             std::vector<ck::index_t> output_spatial_lengths,
             std::vector<ck::index_t> conv_filter_strides,
             std::vector<ck::index_t> conv_filter_dilations,
             std::vector<ck::index_t> input_left_pads,
             std::vector<ck::index_t> input_right_pads,
             const AElementwiseOperation& a_element_op,
             const BElementwiseOperation& b_element_op,
             const CElementwiseOperation& c_element_op) const
    {
        const ck::index_t m_per_thread = ThreadwiseGemm_Dispatch::ThreadMaxMr;
        const ck::index_t n_per_thread = ThreadwiseGemm_Dispatch::ThreadMaxNr;
        const ck::index_t k_per_thread = C;

        const auto GemmM = c_grid_desc.GetLength(I0);
        const auto GemmN = c_grid_desc.GetLength(I1);
        const auto GemmK = a_grid_desc.GetLength(I1);

        const intptr_t Hi = input_spatial_lengths[0];
        const intptr_t Wi = input_spatial_lengths[1];

        const intptr_t Ho = output_spatial_lengths[0];
        const intptr_t Wo = output_spatial_lengths[1];

        const intptr_t Y = filter_spatial_lengths[0];
        const intptr_t X = filter_spatial_lengths[1];

        const intptr_t Sy = conv_filter_strides[0];
        const intptr_t Sx = conv_filter_strides[1];

        const intptr_t Dy = conv_filter_dilations[0];
        const intptr_t Dx = conv_filter_dilations[1];

        const intptr_t Py = input_left_pads[0];
        const intptr_t Px = input_left_pads[1];

        const intptr_t X_Dx = X * Dx;
        // const index_t Y_Dy  = Y * Dy;

        // const index_t InRightPadH = input_right_pads[0];
        // const index_t InRightPadW = input_right_pads[1];

        constexpr auto a_block_copy_dim = AGridDesc::GetNumOfDimension();

        constexpr auto b_block_copy_dim = BGridDesc::GetNumOfDimension();

        auto a_grid_buf = ck::cpu::make_dynamic_buffer<ck::AddressSpaceEnum::Global>(
            const_cast<FloatA*>(p_a_grid), a_grid_desc.GetElementSpaceSize());

        auto b_grid_buf = ck::cpu::make_dynamic_buffer<ck::AddressSpaceEnum::Global>(
            const_cast<FloatB*>(p_b_grid), b_grid_desc.GetElementSpaceSize());

        auto c_grid_buf = ck::cpu::make_dynamic_buffer<ck::AddressSpaceEnum::Global>(
            reinterpret_cast<FloatC*>(p_c_grid), c_grid_desc.GetElementSpaceSize());

        auto c0_grid_buf = ck::cpu::make_dynamic_buffer<ck::AddressSpaceEnum::Global>(
            reinterpret_cast<const FloatC0*>(p_c0_grid), c0_grid_desc.GetElementSpaceSize());

        auto c1_grid_buf = ck::cpu::make_dynamic_buffer<ck::AddressSpaceEnum::Global>(
            reinterpret_cast<const FloatC1*>(p_c1_grid), c1_grid_desc.GetElementSpaceSize());

        int total_threads = omp_get_max_threads();

        if(total_threads > 1 && ck::getenv_int("CK_CPU_BIND_CORE", 0) != 0)
        {
#pragma omp parallel
            {
                int tid = omp_get_thread_num();
                cpu_set_t set;
                CPU_ZERO(&set);

                CPU_SET(tid, &set);

                if(sched_setaffinity(0, sizeof(set), &set) == -1)
                {
                    throw std::runtime_error("wrong! fail to set thread affinity");
                }
            }
        }

        auto devide_thread = [](ck::index_t n_, ck::index_t length_, ck::index_t factor_) {
            ck::index_t t_ = n_;
            while(t_ > length_ && (t_ % factor_ == 0))
            {
                t_ /= factor_;
            }
            return t_;
        };
        const intptr_t num_works_n  = N;
        const intptr_t num_works_ho = Ho;
        // const intptr_t num_works_nho = N * Ho;
        const intptr_t num_works_wo = math::integer_divide_ceil(Wo, m_per_thread);
        const intptr_t num_works_k  = math::integer_divide_ceil(K, n_per_thread);

        auto distribute_num_threads_n_ho_wo_k = [&](ck::index_t& num_threads_n_,
                                                    ck::index_t& num_threads_ho_,
                                                    ck::index_t& num_threads_wo_,
                                                    ck::index_t& num_threads_k_) {
            // TODO: only consider multiply of 2 to divide threads

            ck::index_t num_threads = total_threads;

            num_threads_n_ = devide_thread(num_threads, num_works_n, 2);
            num_threads    = num_threads / num_threads_n_;

            num_threads_ho_ = devide_thread(num_threads, num_works_ho, 2);
            num_threads     = num_threads / num_threads_ho_;

            num_threads_wo_ = devide_thread(num_threads, num_works_wo, 2);
            num_threads     = num_threads / num_threads_wo_;

            num_threads_k_ = devide_thread(num_threads, num_works_k, 2);
            // num_threads = num_threads / num_threads_k_;
        };

        ck::index_t num_threads_n;
        ck::index_t num_threads_ho;
        ck::index_t num_threads_wo;
        ck::index_t num_threads_k;

        distribute_num_threads_n_ho_wo_k(
            num_threads_n, num_threads_ho, num_threads_wo, num_threads_k);

        const ck::index_t num_works_n_per_thread =
            math::integer_divide_ceil(num_works_n, num_threads_n);
        const ck::index_t num_works_ho_per_thread =
            math::integer_divide_ceil(num_works_ho, num_threads_ho);
        const ck::index_t num_works_wo_per_thread =
            math::integer_divide_ceil(num_works_wo, num_threads_wo);
        const ck::index_t num_works_k_per_thread =
            math::integer_divide_ceil(num_works_k, num_threads_k);

        // printf("num_threads_nho:%d, num_threads_wo:%d, num_threads_k:%d |
        // num_works_nho_per_thread:%d, num_works_wo_per_thread:%d, num_works_k_per_thread:%d\n",
        //    num_threads_nho, num_threads_wo, num_threads_k, num_works_nho_per_thread,
        //    num_works_wo_per_thread, num_works_k_per_thread); fflush(stdout);

        if((X - 1) * Dx + 1 <= Px || (Y - 1) * Dy + 1 <= Py)
        {
            // padding zero case, outpout will have zero due to upsampling
            // TODO: This is ugly and slow
            ck::cpu::avx2_util::memset32_avx2(&c_grid_buf.p_data_[0], 0, N * Ho * Wo * K);
            // printf("___ clear\n");
        }

        if(dynamic_tunable.loop_over_spec ==
           ck::tensor_operation::cpu::device::ConvolutionForwardBlockLoopOverSpecialization_t::
               LoopOver_MNK)
        {
            // only parallel in gemm m dim
#pragma omp parallel
            {
                DeviceAlignedMemCPU a_block_mem(
                    UseALocalBuffer ? m_per_thread * k_per_thread * sizeof(FloatA) : 0,
                    MemAlignmentByte);
                DeviceAlignedMemCPU b_block_mem(
                    UseBLocalBuffer ? k_per_thread * n_per_thread * sizeof(FloatB) : 0,
                    MemAlignmentByte);
                DeviceAlignedMemCPU c_block_mem(
                    UseCLocalBuffer ? (m_per_thread * n_per_thread * sizeof(FloatC)) : 0,
                    MemAlignmentByte);

                auto a_block_buf = ck::cpu::make_dynamic_buffer<ck::AddressSpaceEnum::Global>(
                    UseALocalBuffer ? reinterpret_cast<FloatA*>(a_block_mem.mpDeviceBuf)
                                    : const_cast<FloatA*>(p_a_grid),
                    UseALocalBuffer ? a_block_mem.mMemSize / sizeof(FloatA)
                                    : a_grid_desc.GetElementSpaceSize());

                auto b_block_buf = ck::cpu::make_dynamic_buffer<ck::AddressSpaceEnum::Global>(
                    UseBLocalBuffer ? reinterpret_cast<FloatB*>(b_block_mem.mpDeviceBuf)
                                    : const_cast<FloatB*>(p_b_grid),
                    UseBLocalBuffer ? b_block_mem.mMemSize / sizeof(FloatB)
                                    : b_grid_desc.GetElementSpaceSize());

                auto c_block_buf = ck::cpu::make_dynamic_buffer<ck::AddressSpaceEnum::Global>(
                    UseCLocalBuffer ? reinterpret_cast<FloatC*>(c_block_mem.mpDeviceBuf)
                                    : reinterpret_cast<FloatC*>(p_c_grid),
                    UseCLocalBuffer ? c_block_mem.mMemSize / sizeof(FloatC)
                                    : c_grid_desc.GetElementSpaceSize());

                ck::index_t tid         = omp_get_thread_num();
                const ck::index_t tid_n = tid % num_threads_n;
                tid /= num_threads_n;
                const ck::index_t tid_ho = tid % num_threads_ho;
                tid /= num_threads_ho;
                const ck::index_t tid_wo = tid % num_threads_wo;
                tid /= num_threads_wo;
                const ck::index_t tid_k = tid;

                ck::cpu::ThreadwiseGemmParam param;
                // param.Kr          = k_per_block;
                param.lda   = Sx * C * sizeof(FloatA);
                param.ldb   = GetBLeadingElement(b_grid_desc) * sizeof(FloatB);
                param.ldc   = GetCLeadingElement(c_grid_desc) * sizeof(FloatC);
                param.alpha = 1.0f; // TODO
                param.Kr    = C;

                // ihi = iho * s_stride_h + iy * s_dilation_h - s_pad_h
                // iwi = iwo * s_stride_w + ix * s_dilation_w - s_pad_w
                // ck::index_t i_nho = tid_nho * num_works_nho_per_thread;
                // ck::index_t i_ho  = i_nho % Ho;
                // ck::index_t i_n   = i_nho / Ho;

                // auto accumulate_n_ho = [&]() {
                //     i_ho++;
                //     if(i_ho >= Wo)
                //     {
                //         i_ho = 0;
                //         i_n++;
                //     }
                // };

                for(intptr_t i_n = tid_n * num_works_n_per_thread;
                    (i_n < (tid_n + 1) * num_works_n_per_thread) && i_n < num_works_n;
                    i_n += 1)
                {
                    for(intptr_t i_ho = tid_ho * num_works_ho_per_thread;
                        (i_ho < (tid_ho + 1) * num_works_ho_per_thread) && i_ho < num_works_ho;
                        i_ho += 1)
                    {
                        // for input
                        intptr_t i_hi_no_y = i_ho * Sy - Py;

                        for(intptr_t i_wo = tid_wo * num_works_wo_per_thread * m_per_thread;
                            i_wo < (tid_wo + 1) * num_works_wo_per_thread * m_per_thread &&
                            i_wo < Wo;
                            i_wo += m_per_thread)
                        {
                            intptr_t current_wo_size_no_dx =
                                ck::math::min(Wo - i_wo, (intptr_t)m_per_thread);
                            intptr_t i_wi_no_x = i_wo * Sx - Px;

                            // printf("-- i_nho:%d, i_wo:%d, num_works_nho:%d,
                            // num_threads_nho:%d(Hi:%d,nWi:%d)\n",
                            //    i_nho, i_wo, num_works_nho, num_threads_nho, Hi,
                            //    Wi);fflush(stdout);

                            for(intptr_t i_k = tid_k * num_works_k_per_thread * n_per_thread;
                                i_k < (tid_k + 1) * num_works_k_per_thread * n_per_thread;
                                i_k += n_per_thread)
                            {
                                intptr_t i_dx    = 0;
                                intptr_t i_dy    = 0;
                                bool accmulate_c = false;

                                intptr_t current_k_size =
                                    ck::math::min(K - i_k, (intptr_t)n_per_thread);

                                auto accumulate_dy_dx = [&]() {
                                    i_dx += Dx;
                                    if(i_dx >= X_Dx)
                                    {
                                        i_dx = 0;
                                        i_dy += Dy;
                                    }
                                };

                                for(intptr_t i_yxc = 0; i_yxc < (Y * X * C);
                                    i_yxc += C, accumulate_dy_dx())
                                {
                                    intptr_t current_i_wo = i_wo;
                                    intptr_t i_hi         = i_hi_no_y + i_dy;
                                    if(i_hi < 0 || i_hi >= Hi)
                                        continue;

                                    intptr_t i_wi            = i_wi_no_x + i_dx;
                                    intptr_t current_wo_size = current_wo_size_no_dx;
                                    intptr_t pad_wo_size = 0; // when left pad, we may never have
                                                              // a chance to clear zero (like
                                    // padding) we need to manually clear that

                                    if(i_wi < 0)
                                    {
                                        intptr_t wi_to_zero_length =
                                            -i_wi; // keep this a possitive number
                                        intptr_t steps_wo_turn_possitive =
                                            (wi_to_zero_length + Sx - 1) /
                                            Sx; // how many steps need to move wo, to let wi to be
                                                // possitive

                                        current_wo_size -= steps_wo_turn_possitive;
                                        if(current_wo_size <= 0)
                                            continue;
                                        current_i_wo += steps_wo_turn_possitive;
                                        if(!accmulate_c)
                                            pad_wo_size =
                                                steps_wo_turn_possitive; // if already accumulating,
                                                                         // no need to manually set
                                        i_wi += steps_wo_turn_possitive *
                                                Sx; // now i_wi will be a possitive number
                                    }

                                    if(i_wi >= Wi)
                                        continue;

                                    // shrink right wi/wo
                                    if((i_wi + ((current_wo_size - 1) * Sx)) >= Wi)
                                    {
                                        // printf("  ->[r] i_wi:%d, r:%d(%d), ", i_wi, i_wi +
                                        // ((current_wo_size - 1) * Sx), current_wo_size);
                                        current_wo_size = (Wi - 1 - i_wi) / Sx +
                                                          1; // NOTE: this be careful why here
                                                             // should be compute like this.
                                        if(current_wo_size <= 0)
                                            continue;
                                    }

                                    param.accmulate_c = accmulate_c ? 1 : 0;
                                    accmulate_c       = true;

                                    intptr_t current_input_offset =
                                        i_n * Hi * Wi * C + i_hi * Wi * C + i_wi * C;

                                    if(pad_wo_size != 0)
                                    {
                                        for(intptr_t i_wo_pad = 0; i_wo_pad < pad_wo_size;
                                            i_wo_pad++)
                                        {
                                            const intptr_t offset_c = GetCBlockStartOffset(
                                                c_grid_desc,
                                                (i_n * Ho + i_ho) * Wo + i_wo_pad,
                                                i_k);

                                            // printf("pad_wo_size:%d, current_k_block_size:%d,
                                            // clear offset_c:%d\n",
                                            //             pad_wo_size, current_k_size,
                                            //             offset_c);fflush(stdout);
                                            ck::cpu::avx2_util::memset32_avx2(
                                                &c_block_buf.p_data_[offset_c], 0, current_k_size);
                                        }
                                    }

                                    const intptr_t offset_a = current_input_offset;
                                    const intptr_t offset_b =
                                        GetBBlockStartOffset(b_grid_desc, i_yxc, i_k);
                                    const intptr_t offset_c = GetCBlockStartOffset(
                                        c_grid_desc, (i_n * Ho + i_ho) * Wo + current_i_wo, i_k);

                                    // printf("offset_a:%lu, offset_b:%lu, offset_c:%lu, i_n:%d,
                                    // i_hi:%d, i_wi:%d, i_dx:%d, i_dy:%d, i_k:%d, i_ho:%d, i_wo:%d,
                                    // current_wo_size:%d, current_k_size:%d, i_nho:%d, lda:%d,
                                    // ldb:%d, ldc:%d, acc:%d\n",
                                    //     offset_a, offset_b, offset_c, i_n, i_hi, i_wi, i_dx,
                                    //     i_dy, i_k, i_ho, current_i_wo, current_wo_size,
                                    //     current_k_size, i_nho, param.lda / sizeof(FloatA),
                                    //     param.ldb / sizeof(FloatB), param.ldc / sizeof(FloatC),
                                    //     param.accmulate_c); fflush(stdout);

                                    param.p_a = &a_block_buf.p_data_[offset_a];
                                    param.p_b = &b_block_buf.p_data_[offset_b];
                                    param.p_c = &c_block_buf.p_data_[offset_c];

                                    ThreadwiseGemm_Dispatch::Run(
                                        &param, current_wo_size, current_k_size);
                                }
                            }
                        }

                        // thread block wise fusion
                        for(intptr_t i_wo = tid_wo * num_works_wo_per_thread * m_per_thread;
                            i_wo < (tid_wo + 1) * num_works_wo_per_thread * m_per_thread &&
                            i_wo < Wo;
                            i_wo += 1)
                        {
                            const intptr_t n_size =
                                ck::math::min(K - tid_k * num_works_k_per_thread * n_per_thread,
                                              num_works_k_per_thread * n_per_thread);
                            if constexpr(CThreadwiseCopy::FuseBias && CThreadwiseCopy::FuseAdd)
                            {
                                const intptr_t offset_c = GetCBlockStartOffset(
                                    c_grid_desc, (i_n * Ho + i_ho) * Wo + i_wo, 0);
                                const intptr_t offset_c0 = 0;
                                avx2_util::memcpy32_avx2_with_extra_2src(
                                    &c_block_buf.p_data_[offset_c],
                                    &c_block_buf.p_data_[offset_c],
                                    &c0_grid_buf.p_data_[offset_c0],
                                    &c1_grid_buf.p_data_[offset_c],
                                    n_size,
                                    c_element_op);
                            }
                            else
                            {
                            }
                        }
                    }
                }
            }
        }
        else if(dynamic_tunable.loop_over_spec ==
                ck::tensor_operation::cpu::device::ConvolutionForwardBlockLoopOverSpecialization_t::
                    LoopOver_MKN)
        {
// only parallel in gemm m dim
#pragma omp parallel
            {
                DeviceAlignedMemCPU a_block_mem(
                    UseALocalBuffer ? m_per_thread * k_per_thread * sizeof(FloatA) : 0,
                    MemAlignmentByte);
                DeviceAlignedMemCPU b_block_mem(
                    UseBLocalBuffer ? k_per_thread * n_per_thread * sizeof(FloatB) : 0,
                    MemAlignmentByte);
                DeviceAlignedMemCPU c_block_mem(
                    UseCLocalBuffer ? (m_per_thread * n_per_thread * sizeof(FloatC)) : 0,
                    MemAlignmentByte);

                auto a_block_buf = ck::cpu::make_dynamic_buffer<ck::AddressSpaceEnum::Global>(
                    UseALocalBuffer ? reinterpret_cast<FloatA*>(a_block_mem.mpDeviceBuf)
                                    : const_cast<FloatA*>(p_a_grid),
                    UseALocalBuffer ? a_block_mem.mMemSize / sizeof(FloatA)
                                    : a_grid_desc.GetElementSpaceSize());

                auto b_block_buf = ck::cpu::make_dynamic_buffer<ck::AddressSpaceEnum::Global>(
                    UseBLocalBuffer ? reinterpret_cast<FloatB*>(b_block_mem.mpDeviceBuf)
                                    : const_cast<FloatB*>(p_b_grid),
                    UseBLocalBuffer ? b_block_mem.mMemSize / sizeof(FloatB)
                                    : b_grid_desc.GetElementSpaceSize());

                auto c_block_buf = ck::cpu::make_dynamic_buffer<ck::AddressSpaceEnum::Global>(
                    UseCLocalBuffer ? reinterpret_cast<FloatC*>(c_block_mem.mpDeviceBuf)
                                    : reinterpret_cast<FloatC*>(p_c_grid),
                    UseCLocalBuffer ? c_block_mem.mMemSize / sizeof(FloatC)
                                    : c_grid_desc.GetElementSpaceSize());

                ck::cpu::ThreadwiseGemmParam param;
                // param.Kr          = k_per_block;
                param.lda   = Sx * C * sizeof(FloatA);
                param.ldb   = GetBLeadingElement(b_grid_desc) * sizeof(FloatB);
                param.ldc   = GetCLeadingElement(c_grid_desc) * sizeof(FloatC);
                param.alpha = 1.0f; // TODO
                param.Kr    = C;

                // ihi = iho * s_stride_h + iy * s_dilation_h - s_pad_h
                // iwi = iwo * s_stride_w + ix * s_dilation_w - s_pad_w
                ck::index_t tid         = omp_get_thread_num();
                const ck::index_t tid_n = tid % num_threads_n;
                tid /= num_threads_n;
                const ck::index_t tid_ho = tid % num_threads_ho;
                tid /= num_threads_ho;
                const ck::index_t tid_wo = tid % num_threads_wo;
                tid /= num_threads_wo;
                const ck::index_t tid_k = tid;

                for(intptr_t i_n = tid_n * num_works_n_per_thread;
                    (i_n < (tid_n + 1) * num_works_n_per_thread) && i_n < num_works_n;
                    i_n += 1)
                {
                    for(intptr_t i_ho = tid_ho * num_works_ho_per_thread;
                        (i_ho < (tid_ho + 1) * num_works_ho_per_thread) && i_ho < num_works_ho;
                        i_ho += 1)
                    {
                        // for input
                        intptr_t i_hi_no_y = i_ho * Sy - Py;

                        for(intptr_t i_wo = tid_wo * num_works_wo_per_thread * m_per_thread;
                            i_wo < (tid_wo + 1) * num_works_wo_per_thread * m_per_thread &&
                            i_wo < Wo;
                            i_wo += m_per_thread)
                        {
                            intptr_t current_wo_size_no_dx =
                                ck::math::min(Wo - i_wo, (intptr_t)m_per_thread);
                            intptr_t i_wi_no_x = i_wo * Sx - Px;

                            intptr_t i_dx    = 0;
                            intptr_t i_dy    = 0;
                            bool accmulate_c = false;
                            // printf("-- [%d] i_n:%d, i_ho:%d, i_wo:%d, num_works_n:%d,
                            // num_threads_n:%d(Hi:%d, Wi:%d), current_wo_size_no_dx:%d,
                            // m_per_thread:%d\n",
                            //     tid, i_n, i_ho, i_wo, num_works_n, num_threads_n, Hi, Wi,
                            //     current_wo_size_no_dx, m_per_thread);fflush(stdout);

                            auto accumulate_dy_dx = [&]() {
                                i_dx += Dx;
                                if(i_dx >= X_Dx)
                                {
                                    i_dx = 0;
                                    i_dy += Dy;
                                }
                            };

                            for(intptr_t i_yxc = 0; i_yxc < (Y * X * C);
                                i_yxc += C, accumulate_dy_dx())
                            {
                                intptr_t current_i_wo = i_wo;
                                intptr_t i_hi         = i_hi_no_y + i_dy;
                                bool run_pad_only     = false;
                                if(i_hi < 0 || i_hi >= Hi)
                                    continue;

                                intptr_t i_wi            = i_wi_no_x + i_dx;
                                intptr_t current_wo_size = current_wo_size_no_dx;
                                intptr_t pad_wo_size     = 0; // when left pad, we may never have a
                                                              // chance to clear zero (like
                                // padding) we need to manually clear that

                                /* left corner shift
                                 * when i_wi is negative, need shift i_wo to right to make i_wi
                                 * possitive sx  px   i_wi              steps_wo_turn_possitive
                                 * 1 0
                                 * 0, 1, 2....       0 2   0    0, 2, 4...        0 1   1    -1,
                                 * 0, 1.... 1 2   1    -1, 1, 3....      1 2   2    -2, 0, 2... 1 2
                                 * 3    -3, -1, 1...      2 3   1    -1, 2, 5...       1 3   2 -2,
                                 * 1, 4....      1 3   3    -3, 0, 3          1 3   4    -4,
                                 * -1, 2...      2
                                 */
                                if(i_wi < 0)
                                {
                                    intptr_t wi_to_zero_length =
                                        -i_wi; // keep this a possitive number
                                    intptr_t steps_wo_turn_possitive =
                                        (wi_to_zero_length + Sx - 1) /
                                        Sx; // how many steps need to move wo, to let wi to be
                                            // possitive

                                    current_wo_size -= steps_wo_turn_possitive;
                                    // printf("--- current_wo_size:%d, i_wi:%d\n", current_wo_size,
                                    // i_wi);
                                    if(current_wo_size <= 0)
                                        continue;
                                    current_i_wo += steps_wo_turn_possitive;
                                    if(!accmulate_c)
                                        pad_wo_size =
                                            steps_wo_turn_possitive; // if already accumulating, no
                                                                     // need to manually set
                                    i_wi += steps_wo_turn_possitive *
                                            Sx; // now i_wi will be a possitive number
                                }

                                if(i_wi >= Wi)
                                {
                                    continue;
                                }
                                // shrink right wi/wo
                                if((i_wi + ((current_wo_size - 1) * Sx)) >= Wi)
                                {
                                    // printf("  ->[r] i_wi:%d, r:%d(%d), ", i_wi, i_wi +
                                    // ((current_wo_size - 1) * Sx), current_wo_size);
                                    current_wo_size =
                                        (Wi - 1 - i_wi) / Sx + 1; // NOTE: this be careful why here
                                                                  // should be compute like this.
                                    if(current_wo_size <= 0)
                                        continue;
                                }

                                param.accmulate_c = accmulate_c ? 1 : 0;
                                accmulate_c       = true;

                                intptr_t current_input_offset =
                                    i_n * Hi * Wi * C + i_hi * Wi * C + i_wi * C;

                                if(pad_wo_size != 0)
                                {
                                    // manually clear zero. this may and only may need once along
                                    // the gemm_k reduction
                                    intptr_t i_k = tid_k * num_works_k_per_thread * n_per_thread;
                                    intptr_t current_k_block_size = ck::math::min(
                                        K - i_k, (intptr_t)num_works_k_per_thread * n_per_thread);

                                    const intptr_t offset_c = GetCBlockStartOffset(
                                        c_grid_desc, (i_n * Ho + i_ho) * Wo, i_k);

                                    // printf("[%d] pad_wo_size:%d, current_k_block_size:%d,
                                    // offset_c:%d, i_wo:%d\n",
                                    //     tid, pad_wo_size, current_k_block_size, offset_c,
                                    //     i_wo);fflush(stdout);
                                    ck::cpu::avx2_util::memset32_avx2(
                                        &c_block_buf.p_data_[offset_c],
                                        0,
                                        current_k_block_size * pad_wo_size);
                                }

                                if(run_pad_only)
                                    continue;

                                for(intptr_t i_k = tid_k * num_works_k_per_thread * n_per_thread;
                                    i_k < (tid_k + 1) * num_works_k_per_thread * n_per_thread;
                                    i_k += n_per_thread)
                                {
                                    intptr_t current_k_size =
                                        ck::math::min(K - i_k, (intptr_t)n_per_thread);

                                    const intptr_t offset_a = current_input_offset;
                                    const intptr_t offset_b =
                                        GetBBlockStartOffset(b_grid_desc, i_yxc, i_k);
                                    const intptr_t offset_c = GetCBlockStartOffset(
                                        c_grid_desc, (i_n * Ho + i_ho) * Wo + current_i_wo, i_k);

                                    // printf("[%d] offset_a:%lu, offset_b:%lu, offset_c:%lu,
                                    // i_n:%d, i_hi:%d, i_wi:%d, i_dx:%d, i_dy:%d, i_k:%d, i_ho:%d,
                                    // i_wo:%d, current_wo_size:%d, i_n:%d, i_ho:%d, lda:%d,
                                    // ldb:%d\n",
                                    //     tid, offset_a, offset_b, offset_c, i_n, i_hi, i_wi, i_dx,
                                    //     i_dy, i_k, i_ho, current_i_wo, current_wo_size, i_n,
                                    //     i_ho, param.lda / sizeof(FloatA), param.ldb /
                                    //     sizeof(FloatB)); fflush(stdout);

                                    param.p_a = &a_block_buf.p_data_[offset_a];
                                    param.p_b = &b_block_buf.p_data_[offset_b];
                                    param.p_c = &c_block_buf.p_data_[offset_c];

                                    ThreadwiseGemm_Dispatch::Run(
                                        &param, current_wo_size, current_k_size);
                                }
                            }
                        }

                        // thread block wise fusion
                        for(intptr_t i_wo = tid_wo * num_works_wo_per_thread * m_per_thread;
                            i_wo < (tid_wo + 1) * num_works_wo_per_thread * m_per_thread &&
                            i_wo < Wo;
                            i_wo += 1)
                        {
                            const intptr_t n_size =
                                ck::math::min(K - tid_k * num_works_k_per_thread * n_per_thread,
                                              num_works_k_per_thread * n_per_thread);
                            if constexpr(CThreadwiseCopy::FuseBias && CThreadwiseCopy::FuseAdd)
                            {
                                const intptr_t offset_c = GetCBlockStartOffset(
                                    c_grid_desc, (i_n * Ho + i_ho) * Wo + i_wo, 0);
                                const intptr_t offset_c0 = 0;
                                avx2_util::memcpy32_avx2_with_extra_2src(
                                    &c_block_buf.p_data_[offset_c],
                                    &c_block_buf.p_data_[offset_c],
                                    &c0_grid_buf.p_data_[offset_c0],
                                    &c1_grid_buf.p_data_[offset_c],
                                    n_size,
                                    c_element_op);
                            }
                            else
                            {
                            }
                        }
                    }
                }
            }
        }
    }
};

} // namespace cpu
} // namespace ck

#endif
