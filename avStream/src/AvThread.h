#pragma once
#include <QThread>

class QImage;
struct SwsContext;
struct SwrContext;
struct AVFrame;
struct AVChannelLayout;
class AvThread : public QThread
{
    Q_OBJECT
public:
    AvThread(QObject *p=NULL);
    ~AvThread();

    void SetUrl(const QString &url);
    void run()override;
protected:
    SwsContext *readRgb(AVFrame *fr, SwsContext *ctx);
    SwrContext *readFrAudio(AVFrame *fr, SwrContext *ctx, AVChannelLayout *);
signals:
    void readFrame(const QImage &img);
    void readAudio(const QByteArray &, int samRate);
private:
    QString m_url;
};

