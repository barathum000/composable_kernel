// SPDX-License-Identifier: MIT
// Copyright (c) 2018-2022, Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "ck/utility/common_header.hpp"

namespace ck {

struct GridwiseGemmPipeline_v2
{
    __host__ __device__ static constexpr bool IsSupported(index_t num_loop)
    {
        // TODO: improve applicability
        return num_loop % 2 == 0;
    }

    __host__ __device__ static constexpr bool CalculateHasMainLoop(index_t num_loop)
    {
        return (num_loop / 2) > 1;
    }

    template <bool HasMainLoop,
              typename AGridDesc,
              typename ABlockDesc,
              typename ABlockTransfer,
              typename AGridBuffer,
              typename ABlockBuffer,
              typename ABlockTransferStep,
              typename BGridDesc,
              typename BBlockDesc,
              typename BBlockTransfer,
              typename BGridBuffer,
              typename BBlockBuffer,
              typename BBlockTransferStep,
              typename BlockwiseGemm,
              typename CThreadBuffer>
    __device__ static void Run(const AGridDesc& a_grid_desc,
                               const ABlockDesc& a_block_desc,
                               ABlockTransfer& a_blockwise_copy,
                               const AGridBuffer& a_grid_buf,
                               ABlockBuffer& a_block_buf,
                               const ABlockTransferStep& a_block_copy_step,
                               const BGridDesc& b_grid_desc,
                               const BBlockDesc& b_block_desc,
                               BBlockTransfer& b_blockwise_copy,
                               const BGridBuffer& b_grid_buf,
                               BBlockBuffer& b_block_buf,
                               const BBlockTransferStep& b_block_copy_step,
                               const BlockwiseGemm& blockwise_gemm,
                               CThreadBuffer& c_thread_buf,
                               index_t num_loop)
    {
        // global read 0
        a_blockwise_copy.RunRead(a_grid_desc, a_grid_buf);
        b_blockwise_copy.RunRead(b_grid_desc, b_grid_buf);

        // move to 1
        a_blockwise_copy.MoveSrcSliceWindow(a_grid_desc, a_block_copy_step);
        b_blockwise_copy.MoveSrcSliceWindow(b_grid_desc, b_block_copy_step);

        // Initialize C
        c_thread_buf.Clear();

        // LDS write 0
        a_blockwise_copy.RunWrite(a_block_desc, a_block_buf);
        // global Read 1
        a_blockwise_copy.RunRead(a_grid_desc, a_grid_buf);

        // LDS write 0
        b_blockwise_copy.RunWrite(b_block_desc, b_block_buf);
        // global Read 1
        b_blockwise_copy.RunRead(b_grid_desc, b_grid_buf);

        // main body
        if constexpr(HasMainLoop)
        {
            index_t i = 0;

            do
            {
                block_sync_lds();

                // GEMM i
                blockwise_gemm.Run(a_block_buf, b_block_buf, c_thread_buf);

                block_sync_lds();

                // move to i + 2
                a_blockwise_copy.MoveSrcSliceWindow(a_grid_desc, a_block_copy_step);
                b_blockwise_copy.MoveSrcSliceWindow(b_grid_desc, b_block_copy_step);

                // LDS write i + 1
                a_blockwise_copy.RunWrite(a_block_desc, a_block_buf);
                // global read i + 2
                a_blockwise_copy.RunRead(a_grid_desc, a_grid_buf);

                // LDS write i + 1
                b_blockwise_copy.RunWrite(b_block_desc, b_block_buf);
                // global read i + 2
                b_blockwise_copy.RunRead(b_grid_desc, b_grid_buf);

#if !defined(IGLP_OPT_STRATEGY)
#define IGLP_OPT_STRATEGY 1
#endif

#if defined(ENABLE_PIPELINE_V2_OPT)
#if IGLP_OPT_STRATEGY == 1
                // 8 MFMAs
                __builtin_amdgcn_sched_group_barrier(0x020, 2, 0); // VMEM read
                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA

                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA

                __builtin_amdgcn_sched_group_barrier(0x020, 2, 0); // VMEM read
                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA

                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA

                __builtin_amdgcn_sched_group_barrier(0x020, 1, 0); // VMEM read
                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA

                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA

                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA

                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA
#elif IGLP_OPT_STRATEGY == 2
                // 16 MFMAs
                // cluster #1
                __builtin_amdgcn_sched_group_barrier(0x020, 2, 0); // VMEM read
                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA

                __builtin_amdgcn_sched_group_barrier(0x020, 1, 0); // VMEM read
                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA

                __builtin_amdgcn_sched_group_barrier(0x020, 2, 0); // VMEM read
                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA

                __builtin_amdgcn_sched_group_barrier(0x020, 1, 0); // VMEM read
                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA

                __builtin_amdgcn_sched_group_barrier(0x020, 2, 0); // VMEM read
                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA

                __builtin_amdgcn_sched_group_barrier(0x020, 1, 0); // VMEM read
                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA

                __builtin_amdgcn_sched_group_barrier(0x020, 2, 0); // VMEM read
                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA

                __builtin_amdgcn_sched_group_barrier(0x020, 1, 0); // VMEM read
                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA
                // cluster #2
                __builtin_amdgcn_sched_group_barrier(0x020, 2, 0); // VMEM read
                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA

                __builtin_amdgcn_sched_group_barrier(0x020, 1, 0); // VMEM read
                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA

                __builtin_amdgcn_sched_group_barrier(0x020, 2, 0); // VMEM read
                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA

                __builtin_amdgcn_sched_group_barrier(0x020, 1, 0); // VMEM read
                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA

                __builtin_amdgcn_sched_group_barrier(0x020, 2, 0); // VMEM read
                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA

                __builtin_amdgcn_sched_group_barrier(0x020, 1, 0); // VMEM read
                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA

                __builtin_amdgcn_sched_group_barrier(0x020, 2, 0); // VMEM read
                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA

                __builtin_amdgcn_sched_group_barrier(0x020, 1, 0); // VMEM read
                __builtin_amdgcn_sched_group_barrier(0x008, 1, 0); // MFMA
#endif
#endif

                ++i;
            } while(i < (num_loop - 2));
        }

        // tail
        {
            block_sync_lds();

            // GEMM num_loop - 2
            blockwise_gemm.Run(a_block_buf, b_block_buf, c_thread_buf);

            block_sync_lds();

            // LDS write num_loop - 1
            a_blockwise_copy.RunWrite(a_block_desc, a_block_buf);
            b_blockwise_copy.RunWrite(b_block_desc, b_block_buf);

            block_sync_lds();

            // GEMM num_loop - 1
            blockwise_gemm.Run(a_block_buf, b_block_buf, c_thread_buf);
        }
    }
};

} // namespace ck
