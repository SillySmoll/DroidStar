/*
	DroidStar Dispatch - tone injection into the TX audio path
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

#ifndef DISPATCHPLAYER_H
#define DISPATCHPLAYER_H

#include <QObject>
#include <QMutex>
#include <QString>
#include <vector>
#include <cstdint>

// While armed, AudioEngine::read() pulls 8 kHz PCM from here instead of the
// microphone, so the tone goes through whichever vocoder the current mode uses
// (AMBE for DMR/BrandMeister). When the buffer runs out the mic takes over
// again and finished() is emitted.
class DispatchPlayer : public QObject
{
	Q_OBJECT
public:
	static DispatchPlayer *instance();

	// Main thread. volume: 1.0 = file as recorded, up to 2.0. leadms: silence
	// before the tone so the network has the call set up before audio starts.
	bool load(const QString &path, float volume, int leadms, QString &err);
	void start();
	void stop();
	bool active();
	double duration_sec();

	// Audio/mode thread. Returns false when not armed (caller uses mic).
	bool fill(int16_t *pcm, int n);

signals:
	void finished();

private:
	explicit DispatchPlayer(QObject *parent = nullptr);
	bool decode_other(const QString &path, std::vector<float> &mono, uint32_t &rate, QString &err);

	QMutex m_mutex;
	std::vector<int16_t> m_buf;
	size_t m_pos = 0;
	bool m_armed = false;
};

#endif
