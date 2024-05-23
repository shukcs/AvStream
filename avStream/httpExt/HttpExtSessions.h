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
class HttpExtSessions : public QObject
{
    Q_OBJECT
public:
    HttpExtSessions(QObject *p=NULL);
    ~HttpExtSessions();

    void AddSector(const QString &key, const QString &val);
    void RemoveSector(const QString &key);
    void ClearSector();
    void SetConnectTimeout(int ms = 1000);
    void SetCommTimeout(int ms=3000);
    const QMap<QString, QString> &AllSectorsReply()const;
    void BeginSession(const QString &url, const QString &sesion = "GET", const char *proVer = NULL);
protected:
    void _beginSession(const QString &url, const QString &sesion, const char *proVer);
    QByteArray _get(const QString &sesion, const QString &url, const QString &prot);
    void parse();
signals:
    void sessionConstruct(const QString &key);
    void sessionReply(bool);
    void sessionFinished();
    ///长连接
    void dataReaded(const QByteArray &bts);
private:
signals:
    void beginSession(const QString &url, const QString &sesion, const char *proVer);
private:
    int m_msTimeout;
    int m_msConTimeout;
    bool m_bFinish;
    QTcpSocket *m_curSession;
    QByteArray m_readArray;
    QMap<QString, QString>  m_sects;
};

#endif //__HttpExtSession_H__