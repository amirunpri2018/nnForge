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

#include "layer_data_custom.h"
#include "layer.h"

#include <map>
#include <string>
#include <boost/filesystem.hpp>
#include <boost/uuid/uuid.hpp>

namespace nnforge
{
	class layer_data_custom_list
	{
	public:
		typedef std::shared_ptr<layer_data_custom_list> ptr;
		typedef std::shared_ptr<const layer_data_custom_list> const_ptr;

		layer_data_custom_list() = default;

		layer_data_custom_list(const std::vector<layer::const_ptr>& layer_list);

		// Narrow other data to the layers from layer_list
		layer_data_custom_list(
			const std::vector<layer::const_ptr>& layer_list,
			const layer_data_custom_list& other);

		void check_consistency(const std::vector<layer::const_ptr>& layer_list) const;

		// Returns empty smart pointer in case no custom data is associated with instance_name
		layer_data_custom::ptr find(const std::string& instance_name) const;

		layer_data_custom::ptr get(const std::string& instance_name) const;

		void add(
			const std::string& instance_name,
			layer_data_custom::ptr data_custom);

		void write(const boost::filesystem::path& folder_path) const;

		void read(const boost::filesystem::path& folder_path);

		std::vector<std::string> get_data_custom_layer_name_list() const;

	private:
		std::map<std::string, layer_data_custom::ptr> instance_name_to_data_custom_map;

		static const boost::uuids::uuid data_custom_guid;
		static const char * data_custom_extractor_pattern;
	};
}
