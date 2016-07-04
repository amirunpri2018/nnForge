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

#include "layer_tester_cuda.h"

#include <cudnn.h>

namespace nnforge
{
	namespace cuda
	{
		class sparse_fully_connected_layer_tester_cuda : public layer_tester_cuda
		{
		public:
			sparse_fully_connected_layer_tester_cuda();

			virtual ~sparse_fully_connected_layer_tester_cuda();

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
				unsigned int entry_count);

		protected:
			virtual void tester_configured();

			virtual void notify_data_custom(layer_data_custom::const_ptr host_data_custom);

		private:
			std::pair<int, int> get_input_feature_map_block_size_and_count() const;

			int feature_map_connection_count;
			int max_column_index_count_per_row;
			int window_size;
			float bias;

			static const int max_input_feature_map_block_size;

			cudnnTensorDescriptor_t output_data_desc;
			cudnnTensorDescriptor_t bias_desc;
		};
	}
}
