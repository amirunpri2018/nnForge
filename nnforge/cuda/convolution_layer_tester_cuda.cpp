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

#include "convolution_layer_tester_cuda.h"

#include "neural_network_cudnn_exception.h"
#include "cudnn_util.h"
#include "../convolution_layer.h"

namespace nnforge
{
	namespace cuda
	{
		convolution_layer_tester_cuda::convolution_layer_tester_cuda()
			: input_data_desc(0)
			, output_data_desc(0)
			, weights_desc(0)
			, convolution_desc(0)
			, bias_desc(0)
		{
			cudnn_safe_call(cudnnCreateTensorDescriptor(&input_data_desc));
			cudnn_safe_call(cudnnCreateTensorDescriptor(&output_data_desc));
			cudnn_safe_call(cudnnCreateFilterDescriptor(&weights_desc));
			cudnn_safe_call(cudnnCreateConvolutionDescriptor(&convolution_desc));
			cudnn_safe_call(cudnnCreateTensorDescriptor(&bias_desc));
		}

		convolution_layer_tester_cuda::~convolution_layer_tester_cuda()
		{
			cudnnDestroyTensorDescriptor(input_data_desc);
			cudnnDestroyTensorDescriptor(output_data_desc);
			cudnnDestroyFilterDescriptor(weights_desc);
			cudnnDestroyConvolutionDescriptor(convolution_desc);
			cudnnDestroyTensorDescriptor(bias_desc);
		}

		void convolution_layer_tester_cuda::enqueue_forward_propagation(
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
			cudnn_safe_call(cudnnSetStream(cuda_config->get_cudnn_handle(), stream_id));

			cudnn_util::set_tensor_descriptor(
				input_data_desc,
				input_configuration_specific_list[0],
				entry_count);
			cudnn_util::set_tensor_descriptor(
				output_data_desc,
				output_configuration_specific,
				entry_count);

			{
				void * workspace = 0;
				size_t workspace_size = 0;
				if (temporary_working_fixed_buffer)
				{
					workspace = *temporary_working_fixed_buffer;
					workspace_size = temporary_working_fixed_buffer->get_size();
				}

				cudnnConvolutionFwdAlgo_t algo = cuda_config->cudnn_find_convolution_forward_algo(
					input_data_desc,
					weights_desc,
					convolution_desc,
					output_data_desc,
					*input_buffers[0],
					*data[0],
					*output_buffer,
					workspace,
					workspace_size);

				float alpha = 1.0F;
				float beta = 0.0F;
				cudnn_safe_call(cudnnConvolutionForward(
					cuda_config->get_cudnn_handle(),
					&alpha,
					input_data_desc,
					*input_buffers[0],
					weights_desc,
					*data[0],
					convolution_desc,
					algo,
					workspace,
					workspace_size,
					&beta,
					output_data_desc,
					*output_buffer));
			}

			if (bias)
			{
				float alpha = 1.0F;
				float beta = 1.0F;
				cudnn_safe_call(cudnnAddTensor(
					cuda_config->get_cudnn_handle(),
					&alpha,
					bias_desc,
					*data[1],
					&beta,
					output_data_desc,
					*output_buffer));
			}
		}

		void convolution_layer_tester_cuda::tester_configured()
		{
			std::shared_ptr<const convolution_layer> layer_derived = std::dynamic_pointer_cast<const convolution_layer>(layer_schema);

			window_sizes = layer_derived->window_sizes;
			strides = layer_derived->strides;
			dilation = layer_derived->dilation;
			bias = layer_derived->bias;

			std::vector<unsigned int> zero_padding = layer_derived->left_zero_padding;
			for(int i = 0; i < window_sizes.size(); ++i)
			{
				if (zero_padding[i] != layer_derived->right_zero_padding[i])
					throw neural_network_exception("cuDNN is not able to run convolution when left and right padding sizes don't match");
			}

			cudnn_util::set_filter_descriptor(
				weights_desc,
				output_configuration_specific.feature_map_count,
				input_configuration_specific_list[0].feature_map_count,
				window_sizes);

			cudnn_util::set_tensor_bias_descriptor(
				bias_desc,
				output_configuration_specific.feature_map_count,
				static_cast<unsigned int>(output_configuration_specific.dimension_sizes.size()));

			cudnn_util::set_convolution_descriptor(
				convolution_desc,
				zero_padding,
				strides,
				dilation);
		}

		std::pair<size_t, bool> convolution_layer_tester_cuda::get_temporary_working_fixed_buffer_size() const
		{
			bool is_over_sol_algos_available = cudnn_util::is_over_sol_algos_available(window_sizes, strides, dilation);
			unsigned int working_buffer_elem_count = std::max(input_configuration_specific_list[0].feature_map_count, output_configuration_specific.feature_map_count);
			for(int i = 0; i < window_sizes.size(); ++i)
				working_buffer_elem_count *= window_sizes[i];
			return std::make_pair(std::max(working_buffer_elem_count * sizeof(int), (size_t)(1024*1024)), is_over_sol_algos_available);
		}
	}
}
