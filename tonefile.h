/*
	DroidStar Dispatch - tone file loader
	Copyright (C) 2026 DroidStar Dispatch contributors

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef TONEFILE_H
#define TONEFILE_H

#include <cstdint>
#include <string>
#include <vector>

// Plain C++ (no Qt) so it can be unit tested on its own.
namespace tonefile {

// Decode a RIFF/WAVE file held in memory.
// Supports PCM 8/16/24/32-bit integer and 32/64-bit float, any sample rate,
// any channel count (mixed down to mono), WAVE_FORMAT_EXTENSIBLE included.
// Output: mono float samples in [-1, 1] plus the source sample rate.
bool decode_wav(const uint8_t *data, size_t len,
				std::vector<float> &mono, uint32_t &rate, std::string &err);

// Resample mono float audio to 8000 Hz with an anti-alias low-pass
// (windowed sinc, cutoff ~3.4 kHz) and convert to int16 with the given gain.
std::vector<int16_t> to_8k_pcm(const std::vector<float> &mono, uint32_t rate, float gain);

// Normalize so the loudest sample reaches `peak` (0..1). No-op on silence.
void normalize(std::vector<float> &mono, float peak);

}

#endif
