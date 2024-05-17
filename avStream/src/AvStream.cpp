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
}

void AvStream::onPlay()
{
    if (!m_tPlay)
    {
        auto t = new AvThread(this);
        t->SetUrl(m_ui->lineEdit->text());
        m_tPlay = t;
        m_tPlay->start();
    }
}
