#pragma once
#include <QThread>

class QImage;
struct SwsContext;
struct AVFrame;
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
signals:
    void readFrame(const QImage &img);
private:
    QString m_url;
};

