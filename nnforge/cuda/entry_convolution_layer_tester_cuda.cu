/*
 *  Copyright 2011-2016 Maxim Milakov
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

#include "entry_convolution_layer_tester_cuda.h"

#include <cuda_runtime.h>

namespace nnforge
{
	namespace cuda
	{
		__global__ void entry_convolution_kernel(
			float * __restrict output,
			const float * __restrict input,
			int neuron_count_per_feature_map,
			int input_feature_map_count,
			int output_feature_map_count,
			int input_neuron_count,
			int output_neuron_count,
			int entry_count)
		{
			int neuron_id = blockIdx.x;
			int total_thread_id = blockIdx.y * blockDim.x + threadIdx.x;
			int feature_map_id = total_thread_id >> 5;
			int lane_id = total_thread_id & 31;
			int entry_id = blockIdx.z;

			if ((neuron_id < neuron_count_per_feature_map) && (feature_map_id < output_feature_map_count) && (entry_id < entry_count))
			{
				const float * in_base1 = input + entry_id * 2 * input_neuron_count + neuron_id;
				const float * in_base2 = in_base1 + input_neuron_count;

				int base_input_index1 = 0;
				int base_input_index2 = feature_map_id;
				if (feature_map_id > (input_feature_map_count - 1))
				{
					base_input_index1 = feature_map_id - (input_feature_map_count - 1);
					base_input_index2 = (input_feature_map_count - 1);
				}
				int iteration_count = min(input_feature_map_count - base_input_index1, base_input_index2 + 1);

				float sum = 0.0F;
				#pragma unroll 4
				for(int i = lane_id; i < iteration_count; i += 32)
					sum += in_base1[(base_input_index1 + i) * neuron_count_per_feature_map] * in_base2[(base_input_index2 - i) * neuron_count_per_feature_map];

				#pragma unroll
				for(int tx = 16; tx > 0; tx >>= 1)
#ifdef __CUDACC_VER_MAJOR__
#if __CUDACC_VER_MAJOR__ < 9
					sum += __shfl_down(sum, tx);
#else
					sum += __shfl_down_sync(0xFFFFFFFF, sum, tx);
#endif
#endif

				if (lane_id == 0)
					output[entry_id * output_neuron_count + feature_map_id * neuron_count_per_feature_map + neuron_id] = sum;
			}
		}

		void entry_convolution_layer_tester_cuda::enqueue_forward_propagation(
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
			int warps_per_threadblock = 8;
			int threadblock_size = warps_per_threadblock * 32;
			int threadblocks_to_cover_all_feature_maps = (output_configuration_specific.feature_map_count + warps_per_threadblock - 1) / warps_per_threadblock;

			entry_convolution_kernel<<<dim3(output_elem_count_per_feature_map, threadblocks_to_cover_all_feature_maps, entry_count), threadblock_size, 0, stream_id>>>(
				*output_buffer,
				*input_buffers[0],
				output_elem_count_per_feature_map,
				input_configuration_specific_list[0].feature_map_count,
				output_configuration_specific.feature_map_count,
				input_elem_count_per_entry_list[0],
				output_elem_count_per_entry,
				entry_count);
		}
	}
}
