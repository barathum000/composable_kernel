#pragma once
#include "config.hpp"

namespace ck {

__device__ constexpr index_t get_wave_size() { return CK_GPU_WAVE_SIZE; }

__device__ index_t get_thread_local_1d_id() { return threadIdx.x; }

__device__ index_t get_thread_global_1d_id() { return blockIdx.x * blockDim.x + threadIdx.x; }

__device__ index_t get_wave_local_1d_id() { return threadIdx.x / get_wave_size(); }

__device__ index_t get_block_1d_id() { return blockIdx.x; }

__device__ index_t get_grid_size() { return gridDim.x; }

__device__ index_t get_block_size() { return blockDim.x; }

} // namespace ck
