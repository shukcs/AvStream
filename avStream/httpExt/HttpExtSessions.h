/*
HttpExtSession
Qt QNetworkAccessManager使用http不是很方便，尤其对扩展的
现在有很多扩展的http协议，比如rtsp
*/
#ifndef __HttpExtSessions_H__
#define __HttpExtSessions_H__
#include <QObject>
#include <QMap>

class QTcpSocket;
class QUdpSocket;
class HttpExtSessions : public QObject
{
    Q_OBJECT
public:
    enum SessionType {
        SS_Unknow,
        SS_HTTP,
        SS_RTSP,
    };
public:
    HttpExtSessions(QObject *p=NULL);
    ~HttpExtSessions();

    void SetConnectTimeout(int ms = 1000);
    void SetCommTimeout(int ms=3000);
    const QMap<QString, QString> &AllSectorsReply()const;
    void PutContent(const QString &url, const QString &type, const QByteArray &content);
    void PostContent(const QString &url, const QByteArray &content);
    void GetHttp(const QString &url);
    void BeginRtsp(const QString &url);
    static const QString &MyUserAgent();
protected:
    ///会话开始
    void _beginSession(const QString &url, const QString &sesion, const char *proVer);
    QByteArray _get(const QString &sesion, const QString &url, const QString &prot);
    void parse();
    void url_this(const QString &url_in, QString &protocol, QString &host, int &port, QString &url);
    void genSector(const QString &sesion, const QString &host);
    void addSector(const QString &key, const QString &val);
    void removeSector(const QString &key);
    void clearSector();
    void ensureRtp();
signals:
    void sessionConstruct(const QString &key);
    void sessionReply(bool);
    void sessionFinished();
    ///长连接
    void dataReaded(const QByteArray &bts);
private:
    void writeSesion(const QString &url, const QString &sec, const QString &protocal);
    void rtspNext(const QString &sesion);
    QString nextSesion(const QString &sesion);
    QString getPort()const;
signals:
    void beginSession(const QString &url, const QString &sesion, const char *proVer);
private:
    SessionType m_type;
    int m_msTimeout;
    int m_msConTimeout;
    bool m_bFinish;
    QTcpSocket *m_tcp;
    QUdpSocket *m_rtp;
    QUdpSocket *m_rtcp;
    int m_replyLength;
    QByteArray m_readArray;
    QByteArray m_content;
    QString m_curUrl;
    QString m_curSession;
    QMap<QString, QString>  m_sects;
};

#endif //__HttpExtSession_H__