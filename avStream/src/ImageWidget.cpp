#include "ImageWidget.h"
#include <QtGui/QPainter>
#include "AudioDevice.h"

ImageWidget::ImageWidget(QWidget *p) : QWidget(p)
, m_audio(NULL)
{
}

void ImageWidget::ShowImage(const QImage &img)
{
    m_img = img;
    update();
}

void ImageWidget::PlaySound(const QByteArray &b, int samRate)
{
    if (!m_audio)
    {
        m_audio = new AudioDevice(this);
        m_audio->start(samRate);
    }
    m_audio->SetBuff(b.data(), b.size());
}

void ImageWidget::paintEvent(QPaintEvent *e)
{
    if (m_img.isNull())
        return;

    QPainter p(this);
    auto sz = m_img.size().scaled(size(), Qt::KeepAspectRatio);
    QRect rc(QPoint(0, 0), sz);
    rc.moveCenter(rect().center());
    p.drawImage(rc, m_img);
}
