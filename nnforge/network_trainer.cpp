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

#include "network_trainer.h"

#include <vector>
#include <iostream>

#include "neural_network_exception.h"
#include "exponential_learning_rate_decay_policy.h"

namespace nnforge
{
	network_trainer::network_trainer(
		network_schema::ptr schema,
		const std::vector<std::string>& output_layer_names,
		const std::vector<std::string>& error_source_layer_names,
		const std::vector<std::string>& exclude_data_update_layer_names)
		: schema(schema)
		, output_layer_names(output_layer_names)
		, error_source_layer_names(error_source_layer_names)
		, exclude_data_update_layer_names(exclude_data_update_layer_names)
		, epoch_count(50)
		, learning_rate(0.02F)
		, lr_policy(new exponential_learning_rate_decay_policy())
		, batch_size(1)
		, max_chunk_size(0)
	{
	}

	void network_trainer::train(
		structured_data_bunch_reader& reader,
		network_data_peeker& peeker,
		network_data_pusher& progress_pusher,
		network_data_pusher& pusher)
	{
		initialize_train(reader);

		while(true)
		{
			network_data_peek_entry entry_peeked = peeker.peek(schema);
			if (entry_peeked.data == 0)
				break;

			training_task_state new_task;
			new_task.index_peeked = entry_peeked.index;
			new_task.data = entry_peeked.data;
			new_task.initial_epoch = entry_peeked.start_epoch;

			bool empty_momentum1 = false;
			if (momentum.is_momentum_data())
			{
				if (entry_peeked.momentum_data)
					new_task.momentum_data = entry_peeked.momentum_data;
				else
				{
					new_task.momentum_data = network_data::ptr(new network_data(schema->get_layers()));
					if (new_task.initial_epoch > 0)
						empty_momentum1 = true;
				}
			}
			else
				new_task.momentum_data = network_data::ptr();

			bool empty_momentum2 = false;
			if (momentum.is_momentum_data2())
			{
				if (entry_peeked.momentum_data2)
					new_task.momentum_data2 = entry_peeked.momentum_data2;
				else
				{
					new_task.momentum_data2 = network_data::ptr(new network_data(schema->get_layers()));
					if (new_task.initial_epoch > 0)
						empty_momentum2 = true;
				}
			}
			else
				new_task.momentum_data2 = network_data::ptr();

			
			if (is_last_epoch(new_task))
			{
				std::cout << "Warning: Task is allocated which is already complete. Index " << new_task.index_peeked << ", Base epoch " << new_task.initial_epoch << std::endl;
				continue;
			}

			std::cout << "New task allocated: Index " << new_task.index_peeked << ", Base epoch " << new_task.initial_epoch;
			if (empty_momentum1)
				std::cout << ", Starting with the 1st empty momentum";
			if (empty_momentum2)
				std::cout << ", Starting with the 2nd empty momentum";
			std::cout << std::endl;

			unsigned int reader_epoch_id = new_task.initial_epoch;

			while(true)
			{
				std::cout << "---------- NN # " << new_task.index_peeked << ", Epoch " << new_task.get_current_epoch() + 1 << " ----------" << std::endl;

				reader.set_epoch(reader_epoch_id);

				train_step(
					reader,
					new_task);

				++reader_epoch_id;

				progress_pusher.push(new_task, *schema);

				if (is_broken(new_task))
				{
					std::cout << "# " << new_task.index_peeked << " - broken weights while training, discarding it." << std::endl;
					break;
				}

				if (is_last_epoch(new_task))
				{
					pusher.push(new_task, *schema);
					break;
				}
			}
		}
	}

	bool network_trainer::is_last_epoch(const training_task_state& state) const
	{
		return (state.get_current_epoch() >= epoch_count);
	}

	bool network_trainer::is_broken(const training_task_state& state) const
	{
		for(std::map<std::string, std::pair<layer_configuration_specific, std::shared_ptr<std::vector<double> > > >::const_iterator it = state.history.back().second.begin(); it != state.history.back().second.end(); ++it)
		{
			for(std::vector<double>::const_iterator it2 = it->second.second->begin(); it2 != it->second.second->end(); ++it2)
			{
				double error = *it2;
				bool sanity_check = (error < 1.0e+10) && (-error > -1.0E+10) && !(-error < -1.0E+10);
				if (!sanity_check)
					return true;
			}
		}
		return false;
	}

	float network_trainer::get_global_learning_rate(unsigned int epoch) const
	{
		return lr_policy->get_learning_rate_decay(epoch) * learning_rate;
	}
}
