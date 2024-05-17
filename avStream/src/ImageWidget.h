#pragma once

#include <QtWidgets/QWidget>
#include <QtGui/QImage>

class QThread;
class ImageWidget : public QWidget
{
    Q_OBJECT
public:
    ImageWidget(QWidget *parent = Q_NULLPTR);

public slots:
    void ShowImage(const QImage &img);
protected:
    void paintEvent(QPaintEvent *event)override;
private:
    QImage m_img;
};
