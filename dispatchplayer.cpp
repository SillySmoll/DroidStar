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

#include "dispatchplayer.h"
#include "tonefile.h"
#include <QFile>
#include <QUrl>
#include <QAudioDecoder>
#include <QAudioBuffer>
#include <QEventLoop>
#include <QTimer>
#include <QMutexLocker>
#include <cstring>
#include <algorithm>

DispatchPlayer *DispatchPlayer::instance()
{
	static DispatchPlayer *p = new DispatchPlayer();
	return p;
}

DispatchPlayer::DispatchPlayer(QObject *parent) : QObject(parent)
{
}

// MP3 / M4A / OGG etc. via Qt Multimedia's decoder (FFmpeg backend on Windows).
bool DispatchPlayer::decode_other(const QString &path, std::vector<float> &mono, uint32_t &rate, QString &err)
{
	QAudioDecoder dec;
	QEventLoop loop;
	bool failed = false;
	rate = 0;

	QObject::connect(&dec, &QAudioDecoder::bufferReady, &loop, [&]() {
		QAudioBuffer b = dec.read();
		if(!b.isValid()) return;
		const QAudioFormat f = b.format();
		const int ch = f.channelCount();
		const qsizetype frames = b.frameCount();
		if(ch <= 0) return;
		rate = f.sampleRate();
		for(qsizetype i = 0; i < frames; ++i){
			float acc = 0;
			for(int c = 0; c < ch; ++c){
				const qsizetype idx = i * ch + c;
				switch(f.sampleFormat()){
				case QAudioFormat::UInt8: acc += (b.constData<uint8_t>()[idx] - 128) / 128.0f; break;
				case QAudioFormat::Int16: acc += b.constData<int16_t>()[idx] / 32768.0f; break;
				case QAudioFormat::Int32: acc += b.constData<int32_t>()[idx] / 2147483648.0f; break;
				case QAudioFormat::Float: acc += b.constData<float>()[idx]; break;
				default: break;
				}
			}
			mono.push_back(acc / ch);
		}
	});
	QObject::connect(&dec, &QAudioDecoder::finished, &loop, &QEventLoop::quit);
	QObject::connect(&dec, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error), &loop, [&](QAudioDecoder::Error) {
		failed = true;
		err = dec.errorString();
		loop.quit();
	});

	dec.setSource(QUrl::fromLocalFile(path));
	QTimer::singleShot(15000, &loop, [&]() { failed = true; err = "Timed out decoding file"; loop.quit(); });
	dec.start();
	loop.exec();
	dec.stop();

	if(failed && mono.empty()) return false;
	if(mono.empty() || !rate){
		err = "Could not decode audio (try a WAV file)";
		return false;
	}
	return true;
}

bool DispatchPlayer::load(const QString &path, float volume, int leadms, QString &err)
{
	QFile f(path);
	if(!f.open(QIODevice::ReadOnly)){
		err = "Cannot open " + path;
		return false;
	}
	const QByteArray data = f.readAll();
	f.close();

	std::vector<float> mono;
	uint32_t rate = 0;
	if(data.startsWith("RIFF")){
		std::string e;
		if(!tonefile::decode_wav((const uint8_t *)data.constData(), (size_t)data.size(), mono, rate, e)){
			err = QString::fromStdString(e);
			return false;
		}
	}
	else if(!decode_other(path, mono, rate, err)){
		return false;
	}

	if(volume < 0.0f) volume = 0.0f;
	if(volume > 2.0f) volume = 2.0f;
	std::vector<int16_t> pcm = tonefile::to_8k_pcm(mono, rate, volume);
	if(pcm.empty()){
		err = "File contains no audio";
		return false;
	}

	// 5 ms fades so the tone doesn't start/end with a click.
	const size_t fade = std::min<size_t>(40, pcm.size() / 2);
	for(size_t i = 0; i < fade; ++i){
		const float g = (float)i / (float)fade;
		pcm[i] = (int16_t)(pcm[i] * g);
		pcm[pcm.size() - 1 - i] = (int16_t)(pcm[pcm.size() - 1 - i] * g);
	}

	if(leadms < 0) leadms = 0;
	if(leadms > 3000) leadms = 3000;
	std::vector<int16_t> buf((size_t)leadms * 8, 0);
	buf.insert(buf.end(), pcm.begin(), pcm.end());

	QMutexLocker l(&m_mutex);
	m_buf.swap(buf);
	m_pos = 0;
	m_armed = false;
	return true;
}

void DispatchPlayer::start()
{
	QMutexLocker l(&m_mutex);
	m_pos = 0;
	m_armed = !m_buf.empty();
}

void DispatchPlayer::stop()
{
	QMutexLocker l(&m_mutex);
	m_armed = false;
}

bool DispatchPlayer::active()
{
	QMutexLocker l(&m_mutex);
	return m_armed;
}

double DispatchPlayer::duration_sec()
{
	QMutexLocker l(&m_mutex);
	return m_buf.size() / 8000.0;
}

bool DispatchPlayer::fill(int16_t *pcm, int n)
{
	bool done = false;
	{
		QMutexLocker l(&m_mutex);
		if(!m_armed) return false;
		const size_t left = m_buf.size() - m_pos;
		const size_t take = std::min<size_t>(left, (size_t)n);
		memcpy(pcm, m_buf.data() + m_pos, take * sizeof(int16_t));
		if(take < (size_t)n){
			memset(pcm + take, 0, ((size_t)n - take) * sizeof(int16_t));
		}
		m_pos += take;
		if(m_pos >= m_buf.size()){
			m_armed = false;
			done = true;
		}
	}
	if(done){
		emit finished(); // queued to the main thread (receiver lives there)
	}
	return true;
}
