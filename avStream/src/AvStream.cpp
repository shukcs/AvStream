#include "AvStream.h"
#include "AvThread.h"

#include "ui_AvStream.h"

AvStream::AvStream(QWidget *parent) : QDialog(parent)
, m_ui(new Ui::AvStreamClass), m_tPlay(NULL)
{
    m_ui->setupUi(this);
    connect(m_ui->btm_play, &QPushButton::clicked, this, &AvStream::onPlay);
}

AvStream::~AvStream()
{
    delete m_ui;
    close();
}

void AvStream::onPlay()
{
    if (!m_tPlay)
    {
        auto t = new AvThread(this);
        t->SetUrl(m_ui->lineEdit->text());
        m_tPlay = t;
        m_tPlay->start();
        m_ui->btm_play->setText(tr("Stop"));
        connect(t, &AvThread::readFrame, m_ui->widget, &ImageWidget::ShowImage);
        connect(t, &AvThread::readAudio, m_ui->widget, &ImageWidget::PlaySound);
    }
    else
    {
        close();
        m_ui->btm_play->setText(tr("Play"));
    }
}

void AvStream::close()
{
    if (m_tPlay)
    {
        m_tPlay->terminate();
        while (m_tPlay->isRunning())
        {
            QThread::msleep(1);
        }
        delete m_tPlay;
        m_tPlay = NULL;
    }
}
