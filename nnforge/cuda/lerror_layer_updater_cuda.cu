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

#include "lerror_layer_updater_cuda.h"

#include <cuda_runtime.h>

#include "util_cuda.h"
#include "../lerror_layer.h"
#include "../neural_network_exception.h"

namespace nnforge
{
	namespace cuda
	{
		extern __shared__ float arr_sh[];
		template<int n_type>
		__global__ void lerror_upd_kernel(
			float * __restrict output,
			const float * __restrict input0,
			const float * __restrict input1,
			const float * __restrict scale_mask,
			int input_feature_map_count,
			int elem_count_per_feature_map,
			float n_value,
			float scale,
			int entry_count)
		{
			int feature_map_id = threadIdx.x;
			int neuron_id = blockIdx.x;
			int entry_id = blockIdx.y;
			int threadblock_size = blockDim.x;

			float err = 0.0F;

			int output_offset = entry_id * elem_count_per_feature_map + neuron_id;

			float mask = 1.0F;
			if (scale_mask)
				mask = scale_mask[output_offset];

			int thread_id = threadIdx.x;
			if (mask != 0.0F)
			{
				int input_offset = (entry_id * input_feature_map_count + feature_map_id) * elem_count_per_feature_map + neuron_id;
				while (feature_map_id < input_feature_map_count)
				{
					float local_err = input0[input_offset] - input1[input_offset];

					if (n_type == 1)
						err += fabsf(local_err);
					else if (n_type == 2)
						err += local_err * local_err;
					else
						err += __powf(fabsf(local_err), n_value);

					feature_map_id += threadblock_size;
					input_offset += threadblock_size * elem_count_per_feature_map;
				}

				int lane_id = thread_id & 31;
				#pragma unroll
				for(int tx = 16; tx > 0; tx >>= 1)
					#ifdef __CUDACC_VER_MAJOR__
					#if __CUDACC_VER_MAJOR__ < 9
					err += __shfl_down(err, tx);
					#else
					err += __shfl_down_sync(0xFFFFFFFF, err, tx);
					#endif
					#endif

				int warp_count = threadblock_size >> 5;
				if (warp_count > 1)
				{
					if (lane_id == 0)
						arr_sh[thread_id >> 5] = err;

					__syncthreads();

					if (thread_id < 32)
					{
						err = 0.0F;
						if (thread_id < warp_count)
							err = arr_sh[thread_id];
						#pragma unroll
						for(int tx = 4; tx > 0; tx >>= 1)
#ifdef __CUDACC_VER_MAJOR__
#if __CUDACC_VER_MAJOR__ < 9
							err += __shfl_down(err, tx);
#else
							err += __shfl_down_sync(0xFFFFFFFF, err, tx);
#endif
#endif
					}
				}
			}
		
			if (thread_id == 0)
				output[output_offset] = err * (mask * scale);
		}

		template<bool add_update_to_destination, int n_type>
		__global__ void mse_backprop_upd_kernel(
			float * __restrict output,
			const float * __restrict deriv_input_neurons,
			const float * __restrict target_input_neurons,
			float n_value,
			float n_value_m1,
			float scale2,
			int elem_count) 
		{
			int elem_id = blockDim.x * blockIdx.x + threadIdx.x;
			if (elem_id < elem_count)
			{
				float gradient;
				float diff = target_input_neurons[elem_id] - deriv_input_neurons[elem_id];

				if (n_type == 1.0F)
				{
					gradient = (diff >= 0.0F ? 1.0F : -1.0F);
				}
				else if (n_type == 2.0F)
				{
					gradient = diff;
				}
				else
				{
					gradient = (diff >= 0.0F ? 1.0F : -1.0F) * __powf(fabsf(diff), n_value_m1);
				}

				if (add_update_to_destination)
					output[elem_id] += scale2 * gradient;
				else
					output[elem_id] = scale2 * gradient;
			}
		}

		template<bool add_update_to_destination, int n_type>
		__global__ void mse_backprop_upd_kernel(
			float * __restrict output,
			const float * __restrict deriv_input_neurons,
			const float * __restrict target_input_neurons,
			const float * __restrict scale_mask,
			float n_value,
			float n_value_m1,
			float scale2,
			int elem_count_per_feature_map,
			int input_feature_map_count,
			int entry_count) 
		{
			int neuron_id = blockDim.x * blockIdx.x + threadIdx.x;
			int feature_map_id = blockDim.y * blockIdx.y + threadIdx.y;
			int entry_id = blockDim.z * blockIdx.z + threadIdx.z;
			if ((neuron_id < elem_count_per_feature_map) && (feature_map_id < input_feature_map_count) && (entry_id < entry_count))
			{
				int elem_id = (entry_id * input_feature_map_count + feature_map_id) * elem_count_per_feature_map + neuron_id;
				float mask = scale_mask[entry_id * elem_count_per_feature_map + neuron_id];
				float gradient = 0.0F;
				if (mask != 0.0F)
				{
					float diff = target_input_neurons[elem_id] - deriv_input_neurons[elem_id];

					if (n_type == 1.0F)
					{
						gradient = (diff >= 0.0F ? 1.0F : -1.0F);
					}
					else if (n_type == 2.0F)
					{
						gradient = diff;
					}
					else
					{
						gradient = (diff >= 0.0F ? 1.0F : -1.0F) * __powf(fabsf(diff), n_value_m1);
					}
				}

				if (add_update_to_destination)
					output[elem_id] += gradient * (mask * scale2);
				else
					output[elem_id] = gradient * (mask * scale2);
			}
		}

		void lerror_layer_updater_cuda::enqueue_forward_propagation(
			cudaStream_t stream_id,
			cuda_linear_buffer_device::ptr output_buffer,
			const std::vector<cuda_linear_buffer_device::const_ptr>& schema_data,
			const std::vector<cuda_linear_buffer_device::const_ptr>& data,
			const std::vector<cuda_linear_buffer_device::const_ptr>& data_custom,
			const std::vector<cuda_linear_buffer_device::const_ptr>& input_buffers,
			const std::vector<cuda_linear_buffer_device::const_ptr>& persistent_working_data,
			cuda_linear_buffer_device::ptr temporary_working_fixed_buffer,
			cuda_linear_buffer_device::ptr temporary_working_per_entry_buffer,
			cuda_linear_buffer_device::ptr temporary_fixed_buffer,
			cuda_linear_buffer_device::ptr temporary_per_entry_buffer,
			unsigned int entry_count)
		{
			int threadblock_size = get_threadblock_size(input_configuration_specific_list[0].feature_map_count);
			const float * scale_mask = 0;
			if (input_buffers.size() > 2)
				scale_mask = *input_buffers[2];

			int smem_size = ((threadblock_size + 32 - 1) / 32) * sizeof(float);
			if (n_value == 1.0F)
				lerror_upd_kernel<1><<<dim3(input_elem_count_per_feature_map_list[0], entry_count), threadblock_size, smem_size, stream_id>>>(
					*output_buffer,
					*input_buffers[0],
					*input_buffers[1],
					scale_mask,
					input_configuration_specific_list[0].feature_map_count,
					input_elem_count_per_feature_map_list[0],
					n_value,
					scale,
					entry_count);
			else if (n_value == 2.0F)
				lerror_upd_kernel<2><<<dim3(input_elem_count_per_feature_map_list[0], entry_count), threadblock_size, smem_size, stream_id>>>(
					*output_buffer,
					*input_buffers[0],
					*input_buffers[1],
					scale_mask,
					input_configuration_specific_list[0].feature_map_count,
					input_elem_count_per_feature_map_list[0],
					n_value,
					scale,
					entry_count);
			else
				lerror_upd_kernel<-1><<<dim3(input_elem_count_per_feature_map_list[0], entry_count), threadblock_size, smem_size, stream_id>>>(
					*output_buffer,
					*input_buffers[0],
					*input_buffers[1],
					scale_mask,
					input_configuration_specific_list[0].feature_map_count,
					input_elem_count_per_feature_map_list[0],
					n_value,
					scale,
					entry_count);
		}

		void lerror_layer_updater_cuda::enqueue_backward_data_propagation(
			cudaStream_t stream_id,
			unsigned int input_index,
			cuda_linear_buffer_device::ptr input_errors_buffer,
			cuda_linear_buffer_device::const_ptr output_errors_buffer,
			const std::vector<cuda_linear_buffer_device::const_ptr>& schema_data,
			const std::vector<cuda_linear_buffer_device::const_ptr>& data,
			const std::vector<cuda_linear_buffer_device::const_ptr>& data_custom,
			const std::vector<cuda_linear_buffer_device::const_ptr>& input_neurons_buffers,
			cuda_linear_buffer_device::const_ptr output_neurons_buffer,
			const std::vector<cuda_linear_buffer_device::const_ptr>& persistent_working_data,
			cuda_linear_buffer_device::ptr temporary_working_fixed_buffer,
			cuda_linear_buffer_device::ptr temporary_working_per_entry_buffer,
			cuda_linear_buffer_device::const_ptr temporary_fixed_buffer,
			cuda_linear_buffer_device::const_ptr temporary_per_entry_buffer,
			bool add_update_to_destination,
			unsigned int entry_count)
		{
			if (input_neurons_buffers.size() > 2)
			{
				std::pair<dim3, dim3> kernel_dims = cuda_util::get_grid_and_threadblock_sizes_sequential_access(
					*cuda_config,
					input_elem_count_per_feature_map_list[0],
					input_configuration_specific_list[0].feature_map_count,
					entry_count);

				if (add_update_to_destination)
				{
					if (n_value == 1.0F)
						mse_backprop_upd_kernel<true, 1><<<kernel_dims.first, kernel_dims.second, 0, stream_id>>>(
							*input_errors_buffer,
							*input_neurons_buffers[input_index],
							*input_neurons_buffers[1 - input_index],
							*input_neurons_buffers[2],
							n_value,
							n_value - 1.0F,
							scale * n_value,
							input_elem_count_per_feature_map_list[0],
							input_configuration_specific_list[0].feature_map_count,
							entry_count);
					else if (n_value == 2.0F)
						mse_backprop_upd_kernel<true, 2><<<kernel_dims.first, kernel_dims.second, 0, stream_id>>>(
							*input_errors_buffer,
							*input_neurons_buffers[input_index],
							*input_neurons_buffers[1 - input_index],
							*input_neurons_buffers[2],
							n_value,
							n_value - 1.0F,
							scale * n_value,
							input_elem_count_per_feature_map_list[0],
							input_configuration_specific_list[0].feature_map_count,
							entry_count);
					else
						mse_backprop_upd_kernel<true, -1><<<kernel_dims.first, kernel_dims.second, 0, stream_id>>>(
							*input_errors_buffer,
							*input_neurons_buffers[input_index],
							*input_neurons_buffers[1 - input_index],
							*input_neurons_buffers[2],
							n_value,
							n_value - 1.0F,
							scale * n_value,
							input_elem_count_per_feature_map_list[0],
							input_configuration_specific_list[0].feature_map_count,
							entry_count);
				}
				else
				{
					if (n_value == 1.0F)
						mse_backprop_upd_kernel<false, 1><<<kernel_dims.first, kernel_dims.second, 0, stream_id>>>(
							*input_errors_buffer,
							*input_neurons_buffers[input_index],
							*input_neurons_buffers[1 - input_index],
							*input_neurons_buffers[2],
							n_value,
							n_value - 1.0F,
							scale * n_value,
							input_elem_count_per_feature_map_list[0],
							input_configuration_specific_list[0].feature_map_count,
							entry_count);
					else if (n_value == 2.0F)
						mse_backprop_upd_kernel<false, 2><<<kernel_dims.first, kernel_dims.second, 0, stream_id>>>(
							*input_errors_buffer,
							*input_neurons_buffers[input_index],
							*input_neurons_buffers[1 - input_index],
							*input_neurons_buffers[2],
							n_value,
							n_value - 1.0F,
							scale * n_value,
							input_elem_count_per_feature_map_list[0],
							input_configuration_specific_list[0].feature_map_count,
							entry_count);
					else
						mse_backprop_upd_kernel<false, -1><<<kernel_dims.first, kernel_dims.second, 0, stream_id>>>(
							*input_errors_buffer,
							*input_neurons_buffers[input_index],
							*input_neurons_buffers[1 - input_index],
							*input_neurons_buffers[2],
							n_value,
							n_value - 1.0F,
							scale * n_value,
							input_elem_count_per_feature_map_list[0],
							input_configuration_specific_list[0].feature_map_count,
							entry_count);
				}
			}
			else
			{
				int elem_count = entry_count * input_elem_count_per_entry_list[0];
				std::pair<dim3, dim3> kernel_dims = cuda_util::get_grid_and_threadblock_sizes_sequential_access(
					*cuda_config,
					elem_count);

				if (add_update_to_destination)
				{
					if (n_value == 1.0F)
						mse_backprop_upd_kernel<true, 1><<<kernel_dims.first, kernel_dims.second, 0, stream_id>>>(
							*input_errors_buffer,
							*input_neurons_buffers[input_index],
							*input_neurons_buffers[1 - input_index],
							n_value,
							n_value - 1.0F,
							scale * n_value,
							elem_count);
					else if (n_value == 2.0F)
						mse_backprop_upd_kernel<true, 2><<<kernel_dims.first, kernel_dims.second, 0, stream_id>>>(
							*input_errors_buffer,
							*input_neurons_buffers[input_index],
							*input_neurons_buffers[1 - input_index],
							n_value,
							n_value - 1.0F,
							scale * n_value,
							elem_count);
					else
						mse_backprop_upd_kernel<true, -1><<<kernel_dims.first, kernel_dims.second, 0, stream_id>>>(
							*input_errors_buffer,
							*input_neurons_buffers[input_index],
							*input_neurons_buffers[1 - input_index],
							n_value,
							n_value - 1.0F,
							scale * n_value,
							elem_count);
				}
				else
				{
					if (n_value == 1.0F)
						mse_backprop_upd_kernel<false, 1><<<kernel_dims.first, kernel_dims.second, 0, stream_id>>>(
							*input_errors_buffer,
							*input_neurons_buffers[input_index],
							*input_neurons_buffers[1 - input_index],
							n_value,
							n_value - 1.0F,
							scale * n_value,
							elem_count);
					else if (n_value == 2.0F)
						mse_backprop_upd_kernel<false, 2><<<kernel_dims.first, kernel_dims.second, 0, stream_id>>>(
							*input_errors_buffer,
							*input_neurons_buffers[input_index],
							*input_neurons_buffers[1 - input_index],
							n_value,
							n_value - 1.0F,
							scale * n_value,
							elem_count);
					else
						mse_backprop_upd_kernel<false, -1><<<kernel_dims.first, kernel_dims.second, 0, stream_id>>>(
							*input_errors_buffer,
							*input_neurons_buffers[input_index],
							*input_neurons_buffers[1 - input_index],
							n_value,
							n_value - 1.0F,
							scale * n_value,
							elem_count);
				}
			}
		}

		void lerror_layer_updater_cuda::updater_configured()
		{
			if (actions.find(layer_action(layer_action::backward_data, 2)) != actions.end())
				throw neural_network_exception("mse_layer_updater_cuda cannot do backward propagation for scale mask");

			std::shared_ptr<const lerror_layer> layer_derived = std::dynamic_pointer_cast<const lerror_layer>(layer_schema);

			scale = layer_derived->scale;
			n_value = layer_derived->n;
		}

		bool lerror_layer_updater_cuda::is_backward_data_dependent_on_output_buffer(unsigned int action_input_index) const
		{
			return false;
		}

		int lerror_layer_updater_cuda::get_threadblock_size(int input_feature_map_count)
		{
			int threadblock_size;

			if (input_feature_map_count < 256)
			{
				threadblock_size = (input_feature_map_count + 32 - 1) / 32 * 32;
			}
			else
			{
				int threadblock_count = (input_feature_map_count + 256 - 1) / 256;
				threadblock_size = (input_feature_map_count + threadblock_count - 1) / threadblock_count;
				threadblock_size = (threadblock_size + 32 - 1) / 32 * 32;
			}

			return threadblock_size;
		}
	}
}
