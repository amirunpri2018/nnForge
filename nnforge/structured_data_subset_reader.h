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

#include "structured_data_reader.h"

namespace nnforge
{
	class structured_data_subset_reader : public structured_data_reader
	{
	public:
		typedef std::shared_ptr<structured_data_subset_reader> ptr;

		structured_data_subset_reader(
			structured_data_reader::ptr original_reader,
			const std::vector<int>& entry_subset);

		structured_data_subset_reader(
			structured_data_reader::ptr original_reader,
			int fold_count,
			int fold_id,
			bool is_training);

		~structured_data_subset_reader() = default;

		virtual bool read(
			unsigned int entry_id,
			float * data);

		virtual bool raw_read(
			unsigned int entry_id,
			std::vector<unsigned char>& all_elems);

		virtual layer_configuration_specific get_configuration() const;

		virtual int get_entry_count() const;

		virtual raw_data_writer::ptr get_writer(std::shared_ptr<std::ostream> out) const;

	protected:
		structured_data_reader::ptr original_reader;
		std::vector<int> entry_subset;
	};
}
