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

#pragma once

#include "layer_updater_cuda.h"

#include <cudnn.h>

namespace nnforge
{
	namespace cuda
	{
		class sparse_1x1_layer_updater_cuda : public layer_updater_cuda
		{
		public:
			sparse_1x1_layer_updater_cuda();

			virtual ~sparse_1x1_layer_updater_cuda();

			virtual void enqueue_forward_propagation(
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
				unsigned int entry_count);

			virtual void enqueue_backward_data_propagation(
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
				unsigned int entry_count);

			virtual void enqueue_backward_weights_propagation(
				cudaStream_t stream_id,
				const std::vector<cuda_linear_buffer_device::const_ptr>& schema_data,
				const std::vector<cuda_linear_buffer_device::ptr>& gradient,
				const std::vector<cuda_linear_buffer_device::const_ptr>& data_custom,
				const std::vector<cuda_linear_buffer_device::const_ptr>& input_neurons_buffers,
				cuda_linear_buffer_device::const_ptr output_errors_buffer,
				const std::vector<cuda_linear_buffer_device::const_ptr>& persistent_working_data,
				cuda_linear_buffer_device::ptr temporary_working_fixed_buffer,
				cuda_linear_buffer_device::ptr temporary_working_per_entry_buffer,
				cuda_linear_buffer_device::const_ptr temporary_fixed_buffer,
				cuda_linear_buffer_device::const_ptr temporary_per_entry_buffer,
				unsigned int entry_count);

			virtual size_t get_temporary_working_per_entry_buffer_size(const layer_action& action) const;

			virtual bool is_backward_data_dependent_on_input_buffer(unsigned int action_input_index, unsigned int data_input_index) const;

			virtual bool is_backward_data_dependent_on_output_buffer(unsigned int action_input_index) const;

			virtual bool is_backward_weights_dependent_on_input_buffer(unsigned int data_input_index) const;

		protected:
			virtual void updater_configured();

			virtual void notify_data_custom(layer_data_custom::const_ptr host_data_custom);

		private:
			std::pair<int, int> get_entry32_update_block_size_and_count(unsigned int entry_count) const;

			std::pair<int, int> get_entry32_backprop_block_size_and_count(unsigned int entry_count) const;

		private:
			int feature_map_connection_count;
			int max_column_index_count_per_row;
			int max_entry32_update_block_size;
			int max_entry32_backprop_block_size;
			bool bias;
			bool unit_stride;

			layer_configuration_specific input_strided_config;
			std::vector<unsigned int> input_strides;
			std::vector<unsigned int> input_converted_NHWC_strides;
			std::vector<unsigned int> input_converted_CNHW_strides_base;

			cudnnTensorDescriptor_t input_strided_data_desc;
			cudnnTensorDescriptor_t input_converted_NHWC_data_desc;
			cudnnTensorDescriptor_t input_converted_CNHW_data_desc;
			cudnnTensorDescriptor_t output_data_desc;
			cudnnTensorDescriptor_t bias_desc;

			int input_converted_elem_count_per_entry_aligned;
			int output_elem_count_per_entry_aligned;
		};
	}
}
