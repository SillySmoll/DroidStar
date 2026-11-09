/*
	DroidStar Dispatch - shared "this audio frame is a dispatch tone" flag
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

#ifndef DISPATCHFLAG_H
#define DISPATCHFLAG_H

#include <atomic>

// True when the most recent TX audio frame came from the dispatch player
// rather than the microphone. Set in DispatchPlayer::fill(), read by the
// vocoder so it can turn off its speech pause gate for steady tones.
inline std::atomic<bool> g_dispatch_tone_frame{false};

#endif
