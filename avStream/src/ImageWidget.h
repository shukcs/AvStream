#pragma once

#include <QtWidgets/QWidget>
#include <QtGui/QImage>

class QThread;
class AudioDevice;
class ImageWidget : public QWidget
{
    Q_OBJECT
public:
    ImageWidget(QWidget *parent = Q_NULLPTR);

public slots:
    void ShowImage(const QImage &img);
    void PlaySound(const QByteArray &, int samRate);
protected:
    void paintEvent(QPaintEvent *event)override;
private:
    QImage m_img;
    AudioDevice	*m_audio;
};
