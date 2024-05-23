#include "HttpExtSessions.h"
#include <QSettings>
#include <QApplication>
#include <QTcpSocket>
#include "UrlParse.h"

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

void HttpExtSessions::PutContent(const QString &url, const QString &type, const QByteArray &content)
{
    AddSector("Content-type", type);
    AddSector("Content-length", QString::number(content.size()));
    m_content = content;
    emit beginSession(url, "PUT", "1.1");
}

void HttpExtSessions::PostContent(const QString &url, const QByteArray &content)
{
    AddSector("Content-length", QString::number(content.size()));
    m_content = content;
    emit beginSession(url, "POST", "1.1");
}

void HttpExtSessions::BeginSession(const QString &url, const QString &sesion, const char *proVer)
{
    if (m_curSession)
        return;

    emit beginSession(url, sesion, proVer);
}

const QString & HttpExtSessions::MyUserAgent()
{
    static QString sAg = "Qt+HttpExtSessions";
    return sAg;
}

void HttpExtSessions::_beginSession(const QString &url, const QString &sesion /*= "GET"*/, const char *proVer)
{
    if (m_curSession)
        return;

    QString protocal;
    QString host;
    auto port = -1;
    QString refUrl;
    url_this(url, protocal, host, port, refUrl);
    if (port <= 0)
        port = getProtocalDefPort(protocal);
    genSector(sesion.toUpper(), host);

    QString ver = "1.0";
    if (protocal == "http")
        ver = "1.1";

    m_curSession = new QTcpSocket(this);
    connect(m_curSession, &QTcpSocket::connected, this, [=] {
        m_curSession->write(_get(sesion, refUrl, protocal.toUpper()+ "/" + ver));
        if (!m_content.isEmpty())
            m_curSession->write(m_content);
        m_sects.clear();
        m_content.clear();
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

void HttpExtSessions::url_this(const QString &url_in, QString &protocol, QString &host, int &port, QString &url)
{
    UrlParse pa(url_in, "/");
    QString user;
    QString auth;
    if (url_in.indexOf("://") > 0)
        protocol = pa.getword(); // http
    else
        protocol = "http:";

    port = -1;
    if (protocol.toLower() == "https:")
    {
#ifdef HAVE_OPENSSL
        EnableSSL();
#else
        printf("url_this: SSL not available!\n");
#endif
        port = 443;
    }
    else if (protocol.toLower() == "http:")
    {
        port = 80;
    }
    protocol.replace(":", "");
    if (port < 0)
        port = getProtocalDefPort(protocol);

    host = pa.getword();
    auto pos = host.indexOf("@");
    if (pos >= 0)
    {
        user = host.mid(0, pos);
        host = host.mid(pos + 1);
        if (user.indexOf(":") >= 0)
        {
            AddSector("Authorization", "Basic " + user.toUtf8().toBase64());
        }
    }
    if (host.contains(":"))
    {
        UrlParse pa(host, ":");
        pa.getword(host);
        port = pa.getvalue();
    }
}

void HttpExtSessions::genSector(const QString &sc, const QString &host)
{
    if (sc == "HTTP" || sc == "HTTPS")
    {
        if (m_sects.find("Accept") == m_sects.end())
            AddSector("Accept", "*");
        if (m_sects.find("Accept-Language") == m_sects.end())
            AddSector("Accept-Language", "en-us,en;q=0.5");
        if (m_sects.find("Accept-Encoding") == m_sects.end())
            AddSector("Accept-Encoding", "gzip,deflate");
        if (m_sects.find("Accept-Charset") == m_sects.end())
            AddSector("Accept-Charset", "ISO-8859-1,utf-8;q=0.7,*;q=0.7");
        AddSector("User-agent", MyUserAgent());
    }
    else if (sc == "PUT")
    {
        AddSector("Host", host);
        AddSector("User-agent", MyUserAgent());
    }
    else if (sc == "POST")
    {
        AddSector("Host", host); // oops - this is actually a request header that we're adding..
        AddSector("User-agent", MyUserAgent());
        AddSector("Accept", "text/html, text/plain, */*;q=0.01");
        AddSector("Connection", "close");
        AddSector("Content-type", "application/x-www-form-urlencoded");
    }
}
