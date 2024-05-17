#pragma once

#include <QtWidgets/QDialog>

namespace Ui {
    class AvStreamClass;
}
class QThread;
class AvStream : public QDialog
{
    Q_OBJECT
public:
    AvStream(QWidget *parent = Q_NULLPTR);
    ~AvStream();
protected:
    void onPlay();
    void close();
private:
    Ui::AvStreamClass *m_ui;
    QThread *m_tPlay;
};
