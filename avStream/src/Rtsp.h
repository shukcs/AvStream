#ifndef __RTSP_H__
#define __RTSP_H__

#include <QString>
#include <QObject>
#include <QMap>
#include <QByteArray>
#include <QThread>

struct AVCodec;
struct AVCodecContext;
struct AVFrame;
struct SwsContext;
struct SwrContext;
struct AVChannelLayout;
class QImage;
class Rtsp : public QThread
{
    Q_OBJECT
public:
    enum Session {
        S_None,
        S_Option,
        S_Describe,
        S_Setup,
        S_Play,
        S_GetParam,
        S_Finshed,
    };
    struct StreamFormat
    {
        StreamFormat(uint32_t c=0, uint32_t i=0, uint32_t header=0);
        uint32_t m_typeCodec;
        uint32_t index;
        uint32_t header;
        struct
        {
            uint32_t smpRate; //音频采样率
            uint32_t channel; //音频通道
        };
        const AVCodec *codec = NULL;
        AVCodecContext *ctx = NULL;
        QString m_control;
    };
public:
    Rtsp(QObject *p = NULL, bool useTcp=false);
    ~Rtsp();

    const QMap<QString, QString> &AllSectorsReply()const;
    void BeginRtsp(const QString &url);
    static const QString &MyUserAgent();
protected:
    void run()override;
protected:
    ///会话开始
    bool _recv();
    QByteArray _get(const QString &sesion, const QString &url, const QString &prot);
    bool parse();
    void url_this(const QString &url_in, QString &host, int &port, QString &url);
    void genSector(const QString &sesion);
    void addSector(const QString &key, const QString &val);
    void removeSector(const QString &key);
    void clearSector();
signals:
    void sessionConstruct(const QString &key);
    void sessionReply(bool);
    void sessionFinished();
    ///长连接
    void readAudio(const QByteArray &bts, int sr);
    void readFrame(const QImage &);
private:
    void writeSesion(int sock, const QString &url, const QString &session);
    void rtspNext(const QString &sesion);
    QString curSession()const;
    QString getTransport()const;
    bool parseContent();
    void parseFrame(const char *buff, int len);
    void InitUdp(const QString &host);
    void recvUdp();
    void readFrAudio(AVFrame *fr);
    void readFrVideo(AVFrame *fr);
    void initCodec();
    void ininitCodec();
    StreamFormat *getStream(int index);
    QString sessionName(Session)const;
private:
    int m_sockRtp;
    int m_sockRtcp;
    int m_udpPort;
    int m_sock;
    int m_replyLength;
    Session m_session;
    bool m_bPlay;
    bool m_bUseTcp;
    int m_nSetup;
    int64_t m_msLastHb;

    AVFrame *m_frame = NULL;
    SwsContext *m_swsCtx = NULL;
    SwrContext *m_swrCtx = NULL;
    AVChannelLayout *m_ch = NULL;
    QString m_curUrl;
    QByteArray m_readArray;
    QByteArray m_readUdp;
    QString m_takon;
    QMap<QString, QString>  m_sects;
    QList<StreamFormat>     m_streams;
};

#endif

