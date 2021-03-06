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

#include "add_layer.h"

#include "neural_network_exception.h"
#include "proto/nnforge.pb.h"

#include <boost/format.hpp>
#include <sstream>

namespace nnforge
{
	const std::string add_layer::layer_type_name = "Add";

	add_layer::add_layer(float alpha)
		: alpha(alpha)
	{
	}

	std::string add_layer::get_type_name() const
	{
		return layer_type_name;
	}

	layer::ptr add_layer::clone() const
	{
		return layer::ptr(new add_layer(*this));
	}

	layer_configuration_specific add_layer::get_output_layer_configuration_specific(const std::vector<layer_configuration_specific>& input_configuration_specific_list) const
	{
		unsigned int neuron_count_per_entry = input_configuration_specific_list[0].get_neuron_count();
		for(std::vector<layer_configuration_specific>::const_iterator it = input_configuration_specific_list.begin() + 1; it != input_configuration_specific_list.end(); ++it)
		{
			unsigned int new_neuron_count_per_entry = it->get_neuron_count();
			if (new_neuron_count_per_entry != neuron_count_per_entry)
				throw neural_network_exception("Neuron count per entry mismatch in 2 input layers for add_layer");
		}

		return input_configuration_specific_list[0];
	}

	void add_layer::write_proto(void * layer_proto) const
	{
		protobuf::Layer * layer_proto_typed = reinterpret_cast<nnforge::protobuf::Layer *>(layer_proto);

		if (alpha != 1.0F)
		{
			nnforge::protobuf::AddParam * param = layer_proto_typed->mutable_add_param();
			param->set_alpha(alpha);
		}
	}

	void add_layer::read_proto(const void * layer_proto)
	{
		const protobuf::Layer * layer_proto_typed = reinterpret_cast<const nnforge::protobuf::Layer *>(layer_proto);
		if (layer_proto_typed->has_add_param())
		{
			const protobuf::AddParam& param = layer_proto_typed->add_param();
			alpha = param.alpha();
		}
		else
		{
			alpha = 1.0F;
		}
	}

	float add_layer::get_flops_per_entry(
		const std::vector<layer_configuration_specific>& input_configuration_specific_list,
		const layer_action& action) const
	{
		switch (action.get_action_type())
		{
		case layer_action::forward:
			{
				unsigned int neuron_count = get_output_layer_configuration_specific(input_configuration_specific_list).get_neuron_count();
				unsigned int per_item_flops = static_cast<unsigned int>(input_configuration_specific_list.size());
				return static_cast<float>(neuron_count) * static_cast<float>(per_item_flops);
			}
		case layer_action::backward_data:
			{
				unsigned int neuron_count = get_output_layer_configuration_specific(input_configuration_specific_list).get_neuron_count();
				unsigned int per_item_flops = static_cast<unsigned int>(input_configuration_specific_list.size());
				return static_cast<float>(neuron_count) * static_cast<float>(per_item_flops);
			}
		default:
			return 0.0F;
		}
	}

	std::vector<std::string> add_layer::get_parameter_strings() const
	{
		std::vector<std::string> res;

		std::stringstream ss;
		if (alpha != 1.0F)
		{
			ss << "alpha " << alpha;
		}

		if (!ss.str().empty())
			res.push_back(ss.str());

		return res;
	}

	bool add_layer::is_backward_data_identity(int backprop_index) const
	{
		return (alpha == 1.0F);
	}
}
