#include "HttpExtSessions.h"
#include <QSettings>
#include <QApplication>
#include <QTcpSocket>

int getProtocalDefPort(const QString &prot)
{
    QSettings settings(QApplication::applicationDirPath()+"/config.ini", QSettings::IniFormat);
    auto protocal = prot.toLower();
    auto ret = -1;
    settings.beginGroup("protocalPorts");
    if (protocal == "http" || protocal == "https")
        ret = settings.value(protocal, 80).toInt();
    else if (protocal == "rtsp")
        ret = settings.value(protocal, 554).toInt();
    else if (protocal == "rtmp")
        ret = settings.value(protocal, 1935).toInt();
    else ///其他默认端口在config.ini【protocalPorts】填好
        ret = settings.value(protocal, -1).toInt();
    settings.endGroup();
    return ret;
}
/////////////////////////////////////////////////////////////////////////////////
//HttpExtSessions
/////////////////////////////////////////////////////////////////////////////////
HttpExtSessions::HttpExtSessions(QObject *p) : QObject(p), m_msTimeout(3000)
, m_msConTimeout(1000), m_bFinish(false), m_curSession(NULL)
{
    connect(this, &HttpExtSessions::beginSession, this, &HttpExtSessions::_beginSession);
}

HttpExtSessions::~HttpExtSessions()
{
    delete m_curSession;
}

void HttpExtSessions::AddSector(const QString &key, const QString &val)
{
    m_sects[key] = val;
}

void HttpExtSessions::RemoveSector(const QString &key)
{
    m_sects.remove(key);
}

void HttpExtSessions::ClearSector()
{
    m_sects.clear();
}

void HttpExtSessions::SetConnectTimeout(int ms /*= 1000*/)
{
    m_msConTimeout = ms;
}

void HttpExtSessions::SetCommTimeout(int ms/*=3000*/)
{
    m_msTimeout = ms;
}

const QMap<QString, QString> & HttpExtSessions::AllSectorsReply() const
{
    return m_sects;
}

void HttpExtSessions::BeginSession(const QString &url, const QString &sesion, const char *proVer)
{
    if (m_curSession)
        return;

    emit beginSession(url, sesion, proVer);
}

void HttpExtSessions::_beginSession(const QString &url, const QString &sesion /*= "GET"*/, const char *proVer)
{
    if (m_curSession)
        return;
    m_bFinish = false;
    m_readArray.clear();
    auto index = url.lastIndexOf("://");
    auto protocal = index > 0 ? url.left(index) : "http";///默认协议http

    auto tmp = index > 0 ? url.mid(index + 3) : url;
    index = tmp.indexOf("/");
    auto host = index > 0 ? tmp.left(index) : tmp;
    AddSector("Host", host);
    auto ls = host.split(":", QString::SkipEmptyParts);
    auto port = -1;
    if (ls.size() == 2)
    {
        host = ls.first();
        port = ls.back().toInt();
    }
    tmp = index > 0 ? tmp.mid(index) : "/";
    if (port <= 0)
        port = getProtocalDefPort(protocal);
    QString ver = proVer ? proVer : (protocal == "http" ? "1.1" : "1.0");
    m_curSession = new QTcpSocket(this);
    connect(m_curSession, &QTcpSocket::connected, this, [=] {
        m_curSession->write(_get(sesion, tmp, protocal.toUpper()+ "/" + ver));
        m_sects.clear();
    });
    connect(m_curSession, &QTcpSocket::disconnected, this, [=] {
        m_curSession->deleteLater();
        m_curSession = NULL;
        emit sessionFinished();
    });
    m_curSession->connectToHost(host, port);
    if (!m_curSession->waitForConnected(m_msConTimeout))
    {
        emit sessionReply(false);
        m_curSession->deleteLater();
        m_curSession = NULL;
        return;
    }

    connect(m_curSession, &QTcpSocket::readyRead, this, &HttpExtSessions::parse);
}

QByteArray HttpExtSessions::_get(const QString &sesion, const QString &url, const QString &prot)
{
    auto tmp = sesion + " " + url + " " + prot + "\r\n";
    for (auto itr = m_sects.begin(); itr != m_sects.end(); ++itr)
    {
        tmp+= itr.key()+":"+itr.value()+"\r\n";
    }
    tmp += "\r\n";
    return tmp.toUtf8();
}

void HttpExtSessions::parse()
{
    m_readArray += m_curSession->readAll();
    if (m_bFinish)
    {
        emit dataReaded(m_readArray);
        return;
    }

    auto right = 0;
    auto pos = m_readArray.indexOf("\r\n");
    QRegExp reg("[=:]");
    while (pos >= 0)
    {
        auto str = m_readArray.mid(right, pos-right);
        right = pos + 2;
        if (str.isEmpty())
        {
            m_bFinish = true;
            if (right < m_readArray.size())
                emit dataReaded(m_readArray.mid(right));
            break;
        }
        auto tmp = reg.indexIn(str);
        if (tmp > 0 && str.at(tmp)==':')
            m_sects[str.left(tmp)] = str.mid(tmp + 1);
        else
            emit sessionConstruct(str);
        pos = m_readArray.indexOf("\r\n", right);
    }
    if (right > 0)
        m_readArray = m_readArray.mid(right);
}
