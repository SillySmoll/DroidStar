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

#include "tonefile.h"
#include <cmath>
#include <cstring>

namespace tonefile {

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

bool decode_wav(const uint8_t *data, size_t len, std::vector<float> &mono, uint32_t &rate, std::string &err)
{
	mono.clear();
	if(len < 12 || memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0){
		err = "Not a WAV file";
		return false;
	}

	uint16_t fmt = 0, channels = 0, bits = 0;
	const uint8_t *pcm = nullptr;
	size_t pcmlen = 0;
	rate = 0;

	size_t pos = 12;
	while(pos + 8 <= len){
		const uint8_t *ck = data + pos;
		uint32_t cklen = rd32(ck + 4);
		size_t body = pos + 8;
		size_t avail = len - body;
		if(memcmp(ck, "fmt ", 4) == 0){
			if(cklen < 16 || avail < 16){
				err = "Bad fmt chunk";
				return false;
			}
			fmt = rd16(data + body);
			channels = rd16(data + body + 2);
			rate = rd32(data + body + 4);
			bits = rd16(data + body + 14);
			if(fmt == 0xFFFE && cklen >= 26 && avail >= 26){
				fmt = rd16(data + body + 24); // SubFormat GUID starts with the format tag
			}
		}
		else if(memcmp(ck, "data", 4) == 0){
			pcm = data + body;
			pcmlen = (cklen > avail) ? avail : cklen; // tolerate truncated/streamed files
		}
		pos = body + cklen + (cklen & 1);
	}

	if(!rate || !channels || !bits){
		err = "Missing fmt chunk";
		return false;
	}
	if(!pcm){
		err = "Missing data chunk";
		return false;
	}
	if(!(fmt == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) &&
	   !(fmt == 3 && (bits == 32 || bits == 64))){
		err = "Unsupported WAV encoding (use PCM or float WAV)";
		return false;
	}

	const size_t bps = bits / 8;
	const size_t frame = bps * channels;
	const size_t frames = pcmlen / frame;
	mono.resize(frames);

	for(size_t f = 0; f < frames; ++f){
		const uint8_t *p = pcm + f * frame;
		double acc = 0;
		for(uint16_t c = 0; c < channels; ++c, p += bps){
			double v;
			if(fmt == 3){
				if(bits == 32){ float x; memcpy(&x, p, 4); v = x; }
				else { double x; memcpy(&x, p, 8); v = x; }
			}
			else if(bits == 8){ v = ((int)p[0] - 128) / 128.0; }
			else if(bits == 16){ v = (int16_t)rd16(p) / 32768.0; }
			else if(bits == 24){
				int32_t x = (int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24) >> 8;
				v = x / 8388608.0;
			}
			else { v = (int32_t)rd32(p) / 2147483648.0; }
			acc += v;
		}
		mono[f] = (float)(acc / channels);
	}
	return true;
}

void normalize(std::vector<float> &mono, float peak)
{
	float m = 0;
	for(float s : mono){
		float a = std::fabs(s);
		if(a > m) m = a;
	}
	if(m < 1e-6f) return;
	const float g = peak / m;
	for(float &s : mono) s *= g;
}

std::vector<int16_t> to_8k_pcm(const std::vector<float> &mono, uint32_t rate, float gain)
{
	std::vector<int16_t> out;
	if(mono.empty() || !rate) return out;

	const double ratio = (double)rate / 8000.0;              // input samples per output sample
	const size_t outlen = (size_t)((double)mono.size() / ratio);
	out.reserve(outlen);

	// Windowed-sinc low-pass. Cutoff at 3400 Hz (relative to input rate),
	// but never above the input's own Nyquist.
	double fc = 3400.0 / (double)rate;                         // cycles/sample
	if(fc > 0.45) fc = 0.45;
	const int half = (int)std::ceil(16.0 * (ratio > 1.0 ? ratio : 1.0));
	const double PI = 3.14159265358979323846;

	for(size_t n = 0; n < outlen; ++n){
		const double t = n * ratio;                            // position in input
		const long c = (long)std::floor(t);
		double acc = 0, wsum = 0;
		for(long k = c - half + 1; k <= c + half; ++k){
			const double x = (double)k - t;
			const double sinc = (std::fabs(x) < 1e-9) ? 2.0 * fc : std::sin(2.0 * PI * fc * x) / (PI * x);
			const double w = 0.42 + 0.5 * std::cos(PI * x / half) + 0.08 * std::cos(2.0 * PI * x / half); // Blackman
			const double h = sinc * w;
			wsum += h;                                         // full-kernel DC gain (no edge blow-up)
			if(k >= 0 && k < (long)mono.size()) acc += mono[k] * h;
		}
		double v = (wsum != 0) ? acc / wsum : 0;
		v *= gain * 32767.0;
		if(v > 32767.0) v = 32767.0;
		if(v < -32768.0) v = -32768.0;
		out.push_back((int16_t)std::lround(v));
	}
	return out;
}

}
