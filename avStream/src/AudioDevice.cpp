#include "AudioDevice.h"
#include <QTimer>

//////////////////////////////////////////////////////////////////////////////
//AudioDevice 声音输出缓冲
//////////////////////////////////////////////////////////////////////////////
AudioDevice::AudioDevice(QObject *obj)
	:QIODevice(obj), m_out(NULL)
{
}

AudioDevice::~AudioDevice()
{
	delete m_out;
}

void AudioDevice::start(int samRate)
{
	open(QIODevice::ReadOnly);
	if (!m_out)
	{
		QAudioFormat format;
		format.setSampleRate(samRate);
		format.setChannelCount(2);
		format.setSampleSize(16);
		format.setCodec("audio/pcm");
		format.setByteOrder(QAudioFormat::LittleEndian);
		format.setSampleType(QAudioFormat::SignedInt);

		QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
		m_out = info.isFormatSupported(format) ? new QAudioOutput(format, this) : NULL;
		if (m_out)
		{
			m_out->setBufferSize(3200);
			connect(m_out, SIGNAL(stateChanged(QAudio::State)), SLOT(onStatechange(QAudio::State)));
		}
	}
	//要延时一下播放
	if (m_out)
		QTimer::singleShot(10, this, SLOT(onStart()));
}

void AudioDevice::stop()
{
	if (m_out)
		m_out->stop();
	close();
	m_array.clear();
}

qint64 AudioDevice::readData(char *data, qint64 len)
{
	if (!data || len <=0)
		return 0;

	const qint64 chunk = qMin<qint64>(m_array.size(), len);
	memcpy(data, m_array.constData(), chunk);
	m_array.remove(0, chunk);

	return chunk;
}

qint64 AudioDevice::bytesAvailable() const
{
	return m_array.size() + QIODevice::bytesAvailable();
}

qint64 AudioDevice::writeData(const char *data, qint64 len)
{
	Q_UNUSED(data);
	Q_UNUSED(len);

	return 0;
}

void AudioDevice::SetBuff(const char *data, qint64 len)
{
	if (isOpen() && data && len>0)
	{
		m_array.append(data, len);
		
		if (m_out)
		{
			QAudio::State s = m_out->state();
			if (QAudio::SuspendedState == s)
				m_out->resume();
		}
	}
}

void AudioDevice::onStart()
{
	if (m_out)
		m_out->start(this);
}

void AudioDevice::onStatechange(QAudio::State s)
{
	if (QAudio::IdleState == s)
	{
        stop();
        m_out->start(this);
	}
}