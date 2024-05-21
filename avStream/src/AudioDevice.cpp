#include "AudioDevice.h"
#include <QTimer>

//////////////////////////////////////////////////////////////////////////////
//AudioDevice �����������
//////////////////////////////////////////////////////////////////////////////
AudioDevice::AudioDevice(QObject *obj)
	:QIODevice(obj), m_out(NULL)
{
}

AudioDevice::~AudioDevice()
{
	delete m_out;
}

void AudioDevice::Init(int samRate)
{
	open(QIODevice::ReadOnly);
	if (!m_out)
	{
        QAudioFormat format;
        format.setSampleRate(samRate);                //���ò�����
        format.setChannelCount(2);                  //����ͨ����
        format.setSampleSize(16);                   //��������16λ
        format.setCodec("audio/pcm");               //������ʽΪpcm��ʽ
        format.setByteOrder(QAudioFormat::LittleEndian);  //Ĭ��С��ģʽ
        format.setSampleType(QAudioFormat::UnSignedInt);    //�޷���������
		QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
		m_out = info.isFormatSupported(format) ? new QAudioOutput(format, this) : NULL;
		if (m_out)
            m_out->setBufferSize(512*1024);
	}
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
    if (m_array.isEmpty())
    {
        m_out->stop();
        return 0;
    }

	const qint64 chunk = qMin<qint64>(m_array.size(), len);
	memcpy(data, m_array.constData(), chunk);
	m_array.remove(0, chunk);
    if (m_array.isEmpty())
        m_out->stop();

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
	if (m_out && data && len>0)
	{
        m_array.append(data, len);
        QAudio::State s = m_out->state();
        if (QAudio::SuspendedState == s)
            m_out->resume();
        else if (QAudio::StoppedState==s || QAudio::IdleState==s)
            m_out->start(this);
	}
}