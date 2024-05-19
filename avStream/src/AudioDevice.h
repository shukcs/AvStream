#ifndef __IPC_MANAGER_H__
#define __IPC_MANAGER_H__

#include <QEvent>
#include <QIODevice>
#include <QAudioOutput>

class AudioDevice : public QIODevice
{
	Q_OBJECT
public:
	AudioDevice(QObject *obj);
	~AudioDevice();
	void start(int samRate);
	void stop();
	qint64 readData(char *data, qint64 len);
	qint64 bytesAvailable() const;
	qint64 writeData(const char *data, qint64 len);
	void SetBuff(const char *data, qint64 len);
protected slots:
	void onStart();
	void onStatechange(QAudio::State s);
private:
	QByteArray		m_array;
	QAudioOutput	*m_out;
};

#endif //__IPC_MANAGER_H__