#include "ImageWidget.h"
#include <QtGui/QPainter>

ImageWidget::ImageWidget(QWidget *p) : QWidget(p)
{
}

void ImageWidget::ShowImage(const QImage &img)
{
    m_img = img;
    update();
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
