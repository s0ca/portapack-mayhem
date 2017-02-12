/*
 * Copyright (C) 2014 Jared Boone, ShareBrained Technology, Inc.
 * Copyright (C) 2016 Furrtek
 *
 * This file is part of PortaPack.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include "proc_fsk.hpp"
#include "portapack_shared_memory.hpp"
#include "sine_table_int8.hpp"
#include "event_m4.hpp"

#include <cstdint>

void FSKProcessor::execute(const buffer_c8_t& buffer) {
	int8_t re, im;
	
	// This is called at 2.28M/2048 = 1113Hz
	
	if (!configured) return;
	
	for (size_t i = 0; i < buffer.count; i++) {

		if (sample_count >= samples_per_bit) {
			if (bit_pos > length) {
				// End of data
				cur_bit = 0;
				txdone_message.done = true;
				shared_memory.application_queue.push(txdone_message);
				configured = false;
			} else {
				cur_bit = (shared_memory.bb_data.data[bit_pos >> 3] << (bit_pos & 7)) & 0x80;
				bit_pos++;
				if (progress_count >= progress_notice) {
					progress_count = 0;
					txdone_message.progress++;
					shared_memory.application_queue.push(txdone_message);
				} else {
					progress_count++;
				}
			}
			sample_count = 0;
		} else {
			sample_count++;
		}
		
		if (configured) {
			if (cur_bit)
				phase += shift_one;
			else
				phase += shift_zero;
			
			sphase = phase + (64 << 24);

			re = (sine_table_i8[(sphase & 0xFF000000) >> 24]);
			im = (sine_table_i8[(phase & 0xFF000000) >> 24]);
		} else {
			re = 0;
			im = 0;
		}
	
		buffer.p[i] = {re, im};
	}
}

void FSKProcessor::on_message(const Message* const p) {
	const auto message = *reinterpret_cast<const FSKConfigureMessage*>(p);
	
	if (message.id == Message::ID::FSKConfigure) {
		samples_per_bit = message.samples_per_bit;
		length = message.stream_length + 4;			// Why 4 ?!
		
		shift_zero = message.shift * (0xFFFFFFFFULL / 2280000);
		shift_one = -shift_zero;
		
		progress_notice = message.progress_notice;
		
		sample_count = samples_per_bit;
		progress_count = 0;
		bit_pos = 0;
		cur_bit = 0;
		
		txdone_message.progress = 0;
		txdone_message.done = false;
		configured = true;
	}
}

int main() {
	EventDispatcher event_dispatcher { std::make_unique<FSKProcessor>() };
	event_dispatcher.run();
	return 0;
}
