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

#pragma once

#include "layer.h"

namespace nnforge
{
	// f(x) = x, if x > 0, exp(x)-1 otherwise
	// Derivative:
	// f'(x) = 1 if x > 0, f(x)+1 otherwise
	class exponential_linear_layer : public layer
	{
	public:
		exponential_linear_layer() = default;

		virtual layer::ptr clone() const;

		virtual float get_flops_per_entry(
			const std::vector<layer_configuration_specific>& input_configuration_specific_list,
			const layer_action& action) const;

		virtual std::string get_type_name() const;

		static const std::string layer_type_name;
	};
}
