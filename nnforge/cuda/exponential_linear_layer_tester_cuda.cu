/*
 *  Copyright 2011-2017 Maxim Milakov
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "exponential_linear_layer_tester_cuda.h"

#include <cuda_runtime.h>
#include <memory>

#include "util_cuda.h"

namespace nnforge
{
	namespace cuda
	{
		static __forceinline__ __device__ float exponential_linear(float x)
		{
			return (x >= 0.0F) ? x : (__expf(x) - 1.0F);
		}

		__global__ void exponential_linear_kernel(
			float4 * __restrict output,
			const float4 * __restrict input,
			int elem_count)
		{
			int elem_id = blockDim.x * blockIdx.x + threadIdx.x;
			if (elem_id < elem_count)
			{
				float4 val = input[elem_id];
				val.x = exponential_linear(val.x);
				val.y = exponential_linear(val.y);
				val.z = exponential_linear(val.z);
				val.w = exponential_linear(val.w);
				output[elem_id] = val;
			}
		}

		void exponential_linear_layer_tester_cuda::enqueue_forward_propagation(
			cudaStream_t stream_id,
			cuda_linear_buffer_device::ptr output_buffer,
			const std::vector<cuda_linear_buffer_device::const_ptr>& schema_data,
			const std::vector<cuda_linear_buffer_device::const_ptr>& data,
			const std::vector<cuda_linear_buffer_device::const_ptr>& data_custom,
			const std::vector<cuda_linear_buffer_device::const_ptr>& input_buffers,
			const std::vector<cuda_linear_buffer_device::const_ptr>& persistent_working_data,
			cuda_linear_buffer_device::ptr temporary_working_fixed_buffer,
			cuda_linear_buffer_device::ptr temporary_working_per_entry_buffer,
			unsigned int entry_count)
		{
			int elem_count = (output_elem_count_per_entry * entry_count + 3) / 4;
			std::pair<dim3, dim3> kernel_dims = cuda_util::get_grid_and_threadblock_sizes_sequential_access(
				*cuda_config,
				elem_count);
			exponential_linear_kernel<<<kernel_dims.first, kernel_dims.second, 0, stream_id>>>(
				*output_buffer,
				*input_buffers[0],
				elem_count);
		}

		int exponential_linear_layer_tester_cuda::get_input_index_layer_can_write() const
		{
			return 0;
		}
	}
}
