#include "Rtsp.h"
#include <QImage>
#include <QDateTime>
#include "comm_func.h"
#include "httpExt/UrlParse.h"

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/timestamp.h>
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#define RTP_VERSION 2 // RTP version field must equal 2 (p66)

enum {
    RTSP_RET_SUCCESS = 0,
    RTSP_RET_ERR_PARAM = -10,
    RTSP_RET_ERR_DUP_UUID = -11,
    RTSP_RET_ERR_URI = -12,
    RTSP_RET_ERR_RTSP_MSG = -13,
    RTSP_RET_ERR_REVC_RESP = -14,
    RTSP_RET_ERR_AUTH_FAIL = -15,
    RTSP_RET_ERR_NOT_FOUND = -16,
    RTSP_RET_ERR_REVC_TIMEOUT = -17,
    RTSP_RET_ERR_CONNECT = -18,
    RTSP_RET_ERR_NO_RTP = -19,
};

typedef struct
{
    /* byte 0 */
    unsigned short cc : 4;   /* CSRC count */
    unsigned short x : 1;   /* header extension flag */
    unsigned short p : 1;   /* padding flag */
    unsigned short version : 2;   /* protocol version */
                                  /* byte 1 */
    unsigned short pt : 7;   /* payload type */ //pt说明： 96=>H.264, 97=>G.726, 8=>G.711a, 100=>报警数据
    unsigned short marker : 1;   /* marker bit */
                                 /* bytes 2, 3 */
    unsigned short seqno : 16;   /* sequence number */
                                 /* bytes 4-7 */
    unsigned int ts;            /* timestamp in ms */
                                /* bytes 8-11 */
    unsigned int ssrc;          /* synchronization source */
} RTP_HDR_S;  // sizeof: 12

static uint32_t rtp_read_uint32(const uint8_t* ptr)
{
    return (((uint32_t)ptr[0]) << 24) | (((uint32_t)ptr[1]) << 16) | (((uint32_t)ptr[2]) << 8) | ptr[3];
}

static int getIframe(const char *host)
{
    struct sockaddr_in ser_addr;
    unsigned char net_visca_get_key_frame[] = { 0x81, 0x0B, 0x01, 0x04, 0x0D, 0x00, 0xFF };
    auto fd = _parseAddressAndCreateUdp(&ser_addr, host, 1259);
    if (fd < 0)
        return -1;

    auto len = sendto(fd, (char *)&net_visca_get_key_frame, 7, 0, (struct sockaddr*)&ser_addr, sizeof(struct sockaddr_in));
    if (len != 7)
        return -1;

    socket_close(fd);
    return len;
}

static AVCodecID getCodecId(const QString str)
{
    static QMap<QString, AVCodecID> sCodecs = { {"h264", AV_CODEC_ID_H264 }
        , { "h263", AV_CODEC_ID_H263 }
        , { "mpeg4", AV_CODEC_ID_MPEG4 }
        , { "mp3", AV_CODEC_ID_MP3 }
        , { "aac-hbr", AV_CODEC_ID_AAC }
        , { "aac", AV_CODEC_ID_AAC }
        , { "g726", AV_CODEC_ID_ADPCM_G726 }
    };

    auto itr = sCodecs.find(str.toLower());
    return itr != sCodecs.end() ? itr.value() : AV_CODEC_ID_NONE;
}

bool packetAV(AVPacket *pkt, const void* data, int bytes, const QList<Rtsp::StreamFormat> &streams)
{
    RTP_HDR_S headerNal;
    static const int sizeHead = sizeof(RTP_HDR_S);
    if (!data || bytes < sizeHead)
        return false;
    memcpy(&headerNal, data, sizeof(headerNal));
    headerNal.ts = ntohl(headerNal.ts);
    headerNal.ssrc = ntohl(headerNal.ssrc);
    if (bytes < sizeHead) // RFC3550 5.1 RTP Fixed Header Fields(p12)
        return false;
    // pkt header
    const uint8_t *ptr = NULL;
    for (auto &itr : streams)
    {
        if (headerNal.pt == itr.header)
        {
            pkt->stream_index = itr.index;
            ptr = (const uint8_t *)data;
            break;
        }
    }
    if (!ptr)
        return false;

    auto hdrlen = sizeHead + headerNal.cc * 4;
    if (RTP_VERSION != headerNal.version || bytes < hdrlen + (headerNal.x ? 4 : 0) + (headerNal.p ? 1 : 0))
        return false;

    pkt->flags = ptr[hdrlen] & 0x1f;
    assert(bytes >= hdrlen);
    pkt->data = (uint8_t*)ptr + hdrlen;
    pkt->size = bytes - hdrlen;
    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////
//Rtsp::StreamFormat
//////////////////////////////////////////////////////////////////////////////////////////////
Rtsp::StreamFormat::StreamFormat(uint32_t c, uint32_t i, uint32_t h)
: m_typeCodec(c), index(i), header(h), codec(NULL), ctx(NULL)
{
}
//////////////////////////////////////////////////////////////////////////////////////////////
//Rtsp
//////////////////////////////////////////////////////////////////////////////////////////////
Rtsp::Rtsp(QObject *p, bool bUseTcp) : QThread(p), m_sockRtp(-1), m_sockRtcp(-1)
, m_udpPort(13000), m_sock(-1), m_replyLength(0), m_session(S_None), m_bPlay(false)
, m_bUseTcp(false), m_msLastHb(0)
{
    connect(this, &QThread::finished, this, [=] {
        delete this;
    });
}

Rtsp::~Rtsp()
{
    ininitCodec();
}

const QMap<QString, QString> & Rtsp::AllSectorsReply() const
{
    return m_sects;
}

void Rtsp::BeginRtsp(const QString &url)
{
    if (!m_curUrl.isEmpty() || isRunning())
        return;

    m_curUrl = url;
    m_session = S_Option;
    m_nSetup = 0;
    start();
}

const QString & Rtsp::MyUserAgent()
{
    static QString sAg = "Qt+Rtsp";
    return sAg;
}

void Rtsp::run()
{
    QString host;
    int port;
    QString url;

    while (isRunning())
    {
        if (S_None == m_session)
            continue;

        if (S_Setup == m_session)
            InitUdp("0.0.0.0");

        if (m_bPlay)
        {
            recvUdp();
            auto ms = QDateTime::currentMSecsSinceEpoch();
            if (ms - m_msLastHb > 1000 * 10)///心跳
            {
                writeSesion(m_sock, url, sessionName(S_GetParam));
                QByteArray arr(1024 * 8, 0);
                tcp_recv_msg(m_sock, arr.data(), arr.size());
                m_msLastHb = ms;
            }
            continue;
        }
        url_this(m_curUrl, host, port, url);
        if (m_sock == -1 && !m_bPlay)
        {
            if (_create_tcp_socket(&m_sock, host.toUtf8().data(), port) < 0)
            {
                m_session = S_None;
                m_curUrl.clear();
                break;
            }
        }
        writeSesion(m_sock, url, curSession());
        if (!_recv() || !parse())
            break;
    }
}

bool Rtsp::_recv()
{
    QByteArray arr(1024 * 8, 0);
    auto ret = tcp_recv_msg(m_sock, arr.data(), arr.size());
    if (ret == RTSP_RET_ERR_REVC_TIMEOUT)
    {
        socket_close(m_sock);
        m_sock = -1;
        m_session = S_None;
        m_curUrl.clear();
        return false;
    }
    m_readArray += arr.left(ret);
    return true;
}

QByteArray Rtsp::_get(const QString &sesion, const QString &url, const QString &prot)
{
    if (m_sects.isEmpty())
        return QByteArray();

    auto tmp = sesion + " " + url + " " + prot + "\r\n";
    for (auto itr = m_sects.begin(); itr != m_sects.end(); ++itr)
    {
        tmp += itr.key() + ":" + itr.value() + "\r\n";
    }
    tmp += "\r\n";
    return tmp.toUtf8();
}

bool Rtsp::parse()
{
    if (m_readArray.indexOf("\r\n\r\n") < 0)
        return false;

    QStringList ls;
    auto right = 0;
    auto pos = m_readArray.indexOf("\r\n");
    m_replyLength = 0;
    m_takon.clear();
    while (pos >= 0)
    {
        auto str = m_readArray.mid(right, pos - right);
        right = pos + 2;
        if (str.isEmpty())
        {
            m_readArray = m_readArray.mid(right);
            break;
        }
        auto tmp = str.indexOf(":");
        if (tmp > 0)
        {
            if (0 == str.left(tmp).compare("Content-Length", Qt::CaseInsensitive))
                m_replyLength = str.mid(tmp + 1).toInt();
            else if (S_Setup == m_session && !str.left(tmp).compare("Session", Qt::CaseInsensitive))
                m_takon = str.mid(tmp + 1);
        }
        else
        {
            ls << str;
        }
        pos = m_readArray.indexOf("\r\n", right);
    }

    if (!ls.isEmpty())
        rtspNext(ls.first());

    return true;
}

void Rtsp::url_this(const QString &url_in, QString &host, int &port, QString &url)
{
    QString rest;
    UrlParse::ParseUrl(url_in, QString(), host, port, url, rest);
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
    url = "rtsp://" + host + url;
    if (m_session == S_Setup)
        url += "/" + m_streams.at(m_nSetup).m_control;
    
    if (host.contains(":"))
    {
        UrlParse pa(host, ":");
        pa.getword(host);
        port = pa.getvalue();
    }
}

void Rtsp::genSector(const QString &sc)
{
    clearSector();
    if (sc.isEmpty())
        return;

    static uint32_t sSeq = 1;
    addSector("CSeq", QString::number(sSeq++));
    if (!m_takon.isEmpty())
        addSector("Session", m_takon);
    addSector("User-agent", MyUserAgent());
    if (sc == "DESCRIBE")
        addSector("Accept", "application/sdp");
    else if (sc == "SETUP")
        addSector("Transport", getTransport());
    else if (sc == "PLAY")
        addSector("Range", "npt=0.000-");
}

void Rtsp::writeSesion(int sock, const QString &url, const QString &session)
{
    if (m_sock > 0)
    {
        genSector(session);
        auto arr = _get(session, url, "RTSP/1.0");
        if (!arr.isEmpty())
            tcp_write_msg(sock, arr.data(), arr.size());
    }
}

void Rtsp::rtspNext(const QString &reply)
{
    if (reply.endsWith("200 OK", Qt::CaseInsensitive))
    {
        if (parseContent())
        {
            switch (m_session)
            {
            case Rtsp::S_None:
                break;
            case Rtsp::S_Option:
                m_session = S_Describe; break;
            case Rtsp::S_Describe:
                m_session = S_Setup; break;
            case Rtsp::S_Setup:
                m_session = m_nSetup<m_streams.size() ? m_session : S_Play;
                break;
            case Rtsp::S_Play:
                m_bPlay = true;
                m_msLastHb = QDateTime::currentMSecsSinceEpoch();
                break;
            case Rtsp::S_Finshed:
                break;
            default:
                break;
            }
        }
    }
    else
    {
        socket_close(m_sock);
        m_sock = -1;
    }
}

QString Rtsp::curSession() const
{
    return sessionName(m_session);
}

QString Rtsp::getTransport() const
{
    if (m_bUseTcp)
        return "RTP/AVP/TCP;unicast;interleaved=0-1";

    return QString("RTP/AVP;unicast;client_port=%1-%2").arg(m_udpPort).arg(m_udpPort + 1);
}

bool Rtsp::parseContent()
{
    if (S_Setup == m_session)
    {
        m_nSetup++;
    }
    else if (S_Describe == m_session)
    {
        auto right = 0;
        auto pos = m_readArray.indexOf("\r\n");
        m_replyLength = 0;
        m_takon.clear();
        int index = -1;
        while (pos >= 0)
        {
            QString str = m_readArray.mid(right, pos - right);
            right = pos + 2;
            auto tmp = str.indexOf("=");
            if (tmp > 0)
            {
                if (str.left(tmp) == "m")
                {
                    for (auto &itr : str.mid(tmp + 1).split(" ", QString::SkipEmptyParts))
                    {
                        if (0 == itr.compare("audio", Qt::CaseInsensitive))
                            index = AVMEDIA_TYPE_AUDIO;
                        else if (0 == itr.compare("video", Qt::CaseInsensitive))
                            index = AVMEDIA_TYPE_VIDEO;
                    }
                }
                bool bParse = str.left(tmp) == "a" && index >= 0;
                if (auto item = bParse ? getStream(index) : NULL)
                {
                    if (str.mid(tmp + 1, 6) == "rtpmap")
                    {
                        auto &ls = str.mid(tmp + 1).split(" ", QString::SkipEmptyParts);
                        if (ls.size() > 1)
                        {
                            auto lsTmp = ls.at(0).split(":", QString::SkipEmptyParts);
                            if (lsTmp.size() > 1)
                                item->header = lsTmp.at(1).toInt();
                            lsTmp = ls.at(1).split("/");
                            if (lsTmp.size() > 1)
                            {
                                if (item->index == AVMEDIA_TYPE_AUDIO && lsTmp.size()>2)
                                {
                                    item->smpRate = lsTmp.at(1).toInt();
                                    item->channel = lsTmp.at(2).toInt();
                                }
                                else
                                {
                                    item->m_typeCodec = getCodecId(lsTmp.at(0));
                                }
                            }
                        }
                    }
                    else if (item->index == AVMEDIA_TYPE_AUDIO && str.mid(tmp + 1, 4)=="fmtp")
                    {
                        auto &ls = str.mid(tmp + 1).split(" ", QString::SkipEmptyParts);
                        if (ls.size() > 1)
                        {
                            for (auto &itr : ls.at(1).split(";"))
                            {
                                auto lsTmp2 = itr.split("=", QString::SkipEmptyParts);
                                if (lsTmp2.size() == 2 && lsTmp2.first() == "mode")
                                    item->m_typeCodec = getCodecId(lsTmp2.back().toLower());
                            }
                        }
                    }
                    else if (str.mid(tmp + 1, 7) == "control")
                    {
                        auto &ls = str.mid(tmp + 1).split(":", QString::SkipEmptyParts);
                        if (ls.size() > 1)
                            item->m_control = ls.last();
                    }
                }
            }

            pos = m_readArray.indexOf("\r\n", right);
        }
    }
    m_readArray.clear();
    return true;
}

void Rtsp::parseFrame(const char *buff, int len)
{
    initCodec();
    AVPacket packet = { 0 };
    if (packetAV(&packet, buff, len, m_streams))
    {
        const StreamFormat *f = NULL;
        for (auto &itr : m_streams)
        {
            if (itr.index == packet.stream_index)
            {
                f = &itr;
                break;
            }
        }
        if (!f || !f->codec)
            return;
        if (packet.stream_index==AVMEDIA_TYPE_VIDEO)
        {
            if (0 != avcodec_send_packet(f->ctx, &packet))
                return;

            //// 处理视频包
            if (0 == avcodec_receive_frame(f->ctx, m_frame))
                readFrVideo(m_frame);

        }
        else if (packet.stream_index==AVMEDIA_TYPE_AUDIO)
        {
            if (0 != avcodec_send_packet(f->ctx, &packet))
                return;
            // 处理视频包
            if (0 == avcodec_receive_frame(f->ctx, m_frame))
                readFrAudio(m_frame);
        }
    }
}

void Rtsp::InitUdp(const QString &host)
{
    if (m_bUseTcp)
        return;

    while (m_sockRtp < 0)
    {
        m_sockRtp = _create_udp_socket(host.toUtf8().data(), m_udpPort);
        m_sockRtcp = _create_udp_socket(host.toUtf8().data(), m_udpPort+1);
        if (m_sockRtp > 0 && m_sockRtcp>0)
            break;
        if (m_sockRtp > 0)
            socket_close(m_sockRtp);
        if (m_sockRtcp > 0)
            socket_close(m_sockRtcp);
        m_sockRtp = -1;
        m_sockRtcp = -1;
        m_udpPort += 2;
    }
}

void Rtsp::recvUdp()
{
    if (m_sockRtp < 0 || m_bUseTcp)
        return;
    if (m_readUdp.size() < 1024 * 640)
        m_readUdp.resize(1024 * 640);

    fd_set rfds = {};
    FD_SET(m_sockRtp, &rfds);
    FD_SET(m_sockRtcp, &rfds);
    struct timeval tv = { 1, 0 };

    auto ret = select(m_sockRtcp + 1, &rfds, NULL, NULL, &tv);
    if (ret <= 0)
    {
        m_bPlay = false;
        m_session = S_Describe;
        m_takon.clear();
        m_nSetup = 0;
        socket_close(m_sock);
        m_sock = -1;
        return;
    }

    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    if (FD_ISSET(m_sockRtp, &rfds))
    {
        auto len = recvfrom(m_sockRtp, m_readUdp.data(), m_readUdp.size(), 0, (struct sockaddr *)&addr, &addrlen);
        parseFrame(m_readUdp.data(), len);
    }
    if (FD_ISSET(m_sockRtcp, &rfds))
    {
        auto len = recvfrom(m_sockRtcp, m_readUdp.data(), m_readUdp.size(), 0, (struct sockaddr *)&addr, &addrlen);
        parseFrame(m_readUdp.data(), len);
    }
}

void Rtsp::initCodec()
{
    if (m_frame)
        return;

    for (auto itr = m_streams.begin(); itr != m_streams.end(); ++itr)
    {
        if (!itr->codec)
        {
            itr->codec = avcodec_find_decoder((AVCodecID)itr->m_typeCodec);
            itr->ctx = avcodec_alloc_context3(itr->codec);
            if (avcodec_open2(itr->ctx, itr->codec, NULL) < 0)
            {
                avcodec_free_context(&itr->ctx);
            }
        }
    }

    m_frame = av_frame_alloc();
    m_ch = new AVChannelLayout;
}

void Rtsp::ininitCodec()
{
    if (m_ch)
    {
        av_channel_layout_uninit(m_ch);
        m_ch = NULL;
    }
    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = NULL;
    }
    swr_free(&m_swrCtx);
    av_frame_free(&m_frame);

    for (auto itr = m_streams.begin(); itr != m_streams.end(); ++itr)
    {
        avcodec_free_context(&itr->ctx);
    }
    m_streams.clear();
}

Rtsp::StreamFormat * Rtsp::getStream(int index)
{
    for (auto &itr : m_streams)
    {
        if (itr.index == index)
            return &itr;
    }
    m_streams << StreamFormat(AV_CODEC_ID_NONE, index);
    return &m_streams.back();
}

void Rtsp::readFrAudio(AVFrame *fr)
{
    if (!m_swrCtx)
    {
        av_channel_layout_default(m_ch, 2);
        swr_alloc_set_opts2(&m_swrCtx, m_ch, AV_SAMPLE_FMT_S16, fr->sample_rate, &fr->ch_layout
            , (AVSampleFormat)fr->format, fr->sample_rate, 0, NULL);
        swr_init(m_swrCtx);
    }
    uint8_t *data[1] = { 0 };
    QByteArray arr(fr->nb_samples * 4, 0); //两通道16bit
    data[0] = (uint8_t *)arr.data();
    auto len = swr_convert(m_swrCtx, data, fr->nb_samples, fr->data, fr->nb_samples);
    if (len > 0)
        emit readAudio(arr, fr->sample_rate);
}

void Rtsp::readFrVideo(AVFrame *fr)
{
    // 创建一个AVCodecContext和AVFrame用于输入和输出
    if (!m_swsCtx)
    {
        m_swsCtx = sws_getContext(fr->width, fr->height, (AVPixelFormat)fr->format, fr->width, fr->height, AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
        ///ffmpeg Rec.601
        av_opt_set_int(m_swsCtx, "src_range", 0, AV_OPT_SEARCH_CHILDREN);
    }
    // 执行硬件加速的YUV转RGB
    QImage img(fr->width, fr->height, QImage::Format_RGB888);
    int lineSz[] = { 3 * fr->width };
    uint8_t *buf[] = { img.bits(), img.bits() + fr->width* fr->height, img.bits() + 2 * fr->width* fr->height };
    sws_scale(m_swsCtx, fr->data, fr->linesize, 0, fr->height, buf, lineSz);
    emit readFrame(img);
}

void Rtsp::addSector(const QString &key, const QString &val)
{
    m_sects[key] = val;
}
void Rtsp::removeSector(const QString &key)
{
    m_sects.remove(key);
}

void Rtsp::clearSector()
{
    m_sects.clear();
}

QString Rtsp::sessionName(Session sc) const
{
    static QMap<Session, QString> sSessionMap = {
        { S_Option, "OPTIONS" },
        { S_Describe, "DESCRIBE" },
        { S_Setup, "SETUP" },
        { S_Play, "PLAY" },
        { S_GetParam, "GET_PARAMETER" }
    };
    auto itr = sSessionMap.find(sc);
    return itr != sSessionMap.end() ? itr.value() : QString();
}