#include "HttpExtSessions.h"
#include <QTcpSocket>
#include <QUdpSocket>
#include "UrlParse.h"

/////////////////////////////////////////////////////////////////////////////////
//HttpExtSessions
/////////////////////////////////////////////////////////////////////////////////
HttpExtSessions::HttpExtSessions(QObject *p) : QObject(p), m_type(SS_Unknow)
, m_msTimeout(3000), m_msConTimeout(1000), m_bFinish(false), m_tcp(NULL)
, m_rtp(NULL), m_rtcp(NULL)
{
    connect(this, &HttpExtSessions::beginSession, this, &HttpExtSessions::_beginSession);
}

HttpExtSessions::~HttpExtSessions()
{
    delete m_tcp;
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
    addSector("Content-type", type);
    addSector("Content-length", QString::number(content.size()));
    m_curSession.clear();
    m_content = content;
    m_type = SS_HTTP;
    emit beginSession(url, "PUT", "1.1");
}

void HttpExtSessions::PostContent(const QString &url, const QByteArray &content)
{
    addSector("Content-length", QString::number(content.size()));
    m_curSession.clear();
    m_content = content;
    m_type = SS_HTTP;
    emit beginSession(url, "POST", "1.1");
}

void HttpExtSessions::GetHttp(const QString &url)
{
    if (m_tcp)
        return;

    m_curSession.clear();
    m_type = SS_HTTP;
    emit beginSession(url, "GET", "1.1");
}

void HttpExtSessions::BeginRtsp(const QString &url)
{
    if (m_tcp)
        return;
    m_curSession = "DESCRIBE";
    m_type = SS_RTSP;
    emit beginSession(url, m_curSession, "1.0");
}

const QString & HttpExtSessions::MyUserAgent()
{
    static QString sAg = "Qt+HttpExtSessions";
    return sAg;
}

void HttpExtSessions::_beginSession(const QString &url, const QString &sesion, const char *proVer)
{
    if (!m_tcp)
    {
        ensureRtp();
        m_readArray.clear();
        QString host;
        auto port = -1;
        m_curUrl = url;
        QString refUrl;
        QString protocal;
        url_this(url, protocal, host, port, refUrl);
        protocal = protocal.toUpper() + "/" + proVer;
        genSector(sesion.toUpper(), refUrl);
        m_tcp = new QTcpSocket(this);
        connect(m_tcp, &QTcpSocket::connected, this, [=] {
            writeSesion(refUrl, sesion, protocal);
        });
        connect(m_tcp, &QTcpSocket::disconnected, this, [=] {
            m_tcp->deleteLater();
            m_tcp = NULL;
        });
        m_tcp->connectToHost(host, port);
        if (!m_tcp->waitForConnected(m_msConTimeout))
        {
            emit sessionReply(false);
            m_tcp->deleteLater();
            m_tcp = NULL;
            return;
        }
        connect(m_tcp, &QTcpSocket::readyRead, this, &HttpExtSessions::parse);
    }
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
    m_readArray += m_tcp->readAll();
    if (!m_bFinish && m_readArray.indexOf("\r\n\r\n") < 0)
        return;

    QStringList ls;
    if (m_bFinish)
    {
        emit dataReaded(m_readArray);
        m_replyLength -= m_readArray.size();
        m_readArray.clear();
    }
    else
    {
        auto right = 0;
        auto pos = m_readArray.indexOf("\r\n");
        const static QRegExp reg = QRegExp("[=:]");
        m_replyLength = -1;
        while (pos >= 0)
        {
            auto str = m_readArray.mid(right, pos - right);
            right = pos + 2;
            if (str.isEmpty())
            {
                m_bFinish = true;
                emit sessionReply(true);
                m_readArray = m_readArray.mid(right);
                if (right < m_readArray.size())
                {
                    emit dataReaded(m_readArray);
                    m_replyLength -= m_readArray.size();
                    m_readArray.clear();
                }
                break;
            }
            auto tmp = reg.indexIn(str);
            if (tmp > 0 && str.at(tmp) == ':')
            {
                if (0==str.left(tmp).compare("Content-Length", Qt::CaseInsensitive))
                    m_replyLength = str.mid(tmp + 1).toInt();
            }
            else
            {
                ls << str;
            }
            pos = m_readArray.indexOf("\r\n", right);
        }
        if (right > 0)
            m_readArray = m_readArray.mid(right);
    }
   
    if (m_replyLength == 0)
    {
        m_tcp->deleteLater();
        m_tcp = NULL;
        if (!m_curSession.isEmpty() && !ls.isEmpty())
            rtspNext(ls.first());
    }
    for (auto &itr : ls)
    {
        emit sessionConstruct(itr);
    }

    if (m_replyLength == 0 && m_curSession.isEmpty())
        emit sessionFinished();
}

void HttpExtSessions::url_this(const QString &url_in, QString &protocol, QString &host, int &port, QString &url)
{
    QString rest;
    UrlParse::ParseUrl(url_in, protocol, host, port, url, rest);
    auto pos = host.indexOf("@");
    if (pos >= 0)
    {
        QString user;
        QString auth;
        user = host.mid(0, pos);
        host = host.mid(pos + 1);
        if (user.indexOf(":") >= 0)
            addSector("Authorization", "Basic " + user.toUtf8().toBase64());
    }
    if (m_type == SS_RTSP)
        url = "rtsp://"+ host + url;
    if (host.contains(":"))
    {
        UrlParse pa(host, ":");
        pa.getword(host);
        port = pa.getvalue();
    }
}

void HttpExtSessions::genSector(const QString &sc, const QString &host)
{
    clearSector();
    if (sc == "GET")
    {
        addSector("Accept", "*");
        addSector("Accept-Language", "en-us,en;q=0.5");
        addSector("Accept-Encoding", "gzip,deflate");
        addSector("Accept-Charset", "ISO-8859-1,utf-8;q=0.7,*;q=0.7");
        addSector("User-agent", MyUserAgent());
    }
    else if (sc == "PUT")
    {
        addSector("Host", host);
        addSector("User-agent", MyUserAgent());
    }
    else if (sc == "POST")
    {
        addSector("Host", host); // oops - this is actually a request header that we're adding..
        addSector("User-agent", MyUserAgent());
        addSector("Accept", "text/html, text/plain, */*;q=0.01");
        addSector("Connection", "close");
        addSector("Content-type", "application/x-www-form-urlencoded");
    }
    else if (m_type == SS_RTSP)
    {
        static uint32_t sSeq = 1;
        addSector("CSeq", QString::number(sSeq++));
        if (sc == "DESCRIBE")
            addSector("Accept", "application/*");
        else if (sc == "SETUP")
            addSector("Transport", "RTP/AVP;unicast;client_port="+getPort());
        //else if (sc == "PLAY")
    }
}

void HttpExtSessions::writeSesion(const QString &url, const QString &sesion, const QString &protacal)
{
    if (m_tcp && m_tcp->state()==QTcpSocket::ConnectedState)
    {
        m_tcp->write(_get(sesion, url, protacal));
        if (!m_content.isEmpty())
            m_tcp->write(m_content);
        m_sects.clear();
        m_content.clear();
    }
}

void HttpExtSessions::rtspNext(const QString &reply)
{
    if (SS_RTSP==m_type && reply.endsWith("200 OK", Qt::CaseInsensitive))
    {
        m_curSession = nextSesion(m_curSession);
        if (!m_curSession.isEmpty())
            _beginSession(m_curUrl, m_curSession, "1.0");
    }
}

QString HttpExtSessions::nextSesion(const QString &sesion)
{
    if (0 == sesion.compare("DESCRIBE", Qt::CaseInsensitive))
        return "SETUP";
}

QString HttpExtSessions::getPort() const
{
    return QString("%1-%2").arg(m_rtp ? QString::number(m_rtp->localPort()) : QString())
        .arg(m_rtcp ? QString::number(m_rtcp->localPort()) : QString());
}

void HttpExtSessions::addSector(const QString &key, const QString &val)
{
    m_sects[key] = val;
}
void HttpExtSessions::removeSector(const QString &key)
{
    m_sects.remove(key);
}

void HttpExtSessions::clearSector()
{
    m_sects.clear();
}

void HttpExtSessions::ensureRtp()
{
    if (m_type != SS_RTSP)
    {
        delete m_rtcp;
        m_rtcp = NULL;
        delete m_rtp;
        m_rtp = NULL;
        return;
    }
    if (!m_rtp)
    {
        m_rtp = new QUdpSocket(this);
        m_rtcp = new QUdpSocket(this);
        auto port = 4000;
        while (!m_rtp->bind(port) || !m_rtcp->bind(port+1))
        {
            port += 2;
        }
        connect(m_rtp, &QUdpSocket::readyRead, this, [=] {
        });
    }
}
