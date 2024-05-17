#include "AvThread.h"
#include <QImage>
#include <qlogging.h>

extern "C" {
#include "libavutil/avutil.h"
#include "libavutil/timestamp.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
}

AvThread::AvThread(QObject *p) : QThread(p)
{
    avformat_network_init();// ��ʼ�������
}

AvThread::~AvThread()
{
    avformat_network_deinit();
}

void AvThread::SetUrl(const QString &url)
{
    m_url = url;
}

void AvThread::run()
{
    AVFrame *pFrameRGB;
    
    //����AVFormatcontext������FFMPEG���װ(flv, mp4// ffmpegȡrtsp��ʱav_read frame�����Ľ���취 ���ò����Ż�
    AVDictionary* avdic = NULL;
 /*   av_dict_set(&avdic, "buffer size", "1024000", 0);   ///���û����С��1080p�ɽ�ֵ����
    av_dict_set(&avdic, "rtmp transport", "tcp", 0);    ///��udp��ʽ�򿪣������tcp��ʽ�򿪽�udp��
    av_dict_set(&avdic, "stimeout", "2000000", 0);      ///���ó�ʱ�Ͽ�����ʱ�䣬��λ΢��"300000"��8);
    av_dict_set(&avdic, "max delay", "300000", 8);      ///�������ʱ��*/
    //qDebug() << "url:" << m_url;
    AVFormatContext *pFormatctx = avformat_alloc_context();
    if (avformat_open_input(&pFormatctx, m_url.toLocal8Bit().data(), NULL, &avdic) != 0)
    {
        // �����������ļ���qDebug("can't open the file. \n");
        return;
    }
    if (avformat_find_stream_info(pFormatctx, NULL) < 0)
    {
        // ��ȡ�����ݰ�����ȡ���������Ϣ
        //qDebug() << "not find stream info!";
        return;
    }
    int videoStream = -1;
    int audioStream = -1;
    for (int i = 0; i < pFormatctx->nb_streams; ++i)
    {
        switch (pFormatctx->streams[i]->codecpar->codec_type)
        {
        case AVMEDIA_TYPE_VIDEO:
            videoStream = i; break;
        case AVMEDIA_TYPE_AUDIO:
            audioStream = i; break;
        default:
            break;
        }
    }
    if (videoStream < 0)
    {
//        qDebug() << "No video stream!";
        return;
    }
    /// ��ȡ��Ƶ���ı������������
    auto pParam = pFormatctx->streams[videoStream]->codecpar;
    auto pCodec = avcodec_find_decoder(pParam->codec_id);
    if (pCodec == NULL)
    { // �޷��ҵ�������
        return;
    }
    auto codec = avcodec_alloc_context3(pCodec);
    if (!codec)
        return;

    // �򿪱������
    if (avcodec_open2(codec, pCodec, NULL) < 0)
    {
        avcodec_free_context(&codec);
        return; // �޷��򿪱������
    }

    // ��ȡ���ݰ�
    AVPacket packet;
    AVFrame frame;
    SwsContext *swsCtx = NULL;
    while (QThread::isRunning())
    {
        while (av_read_frame(pFormatctx, &packet) >= 0)
        {
            if (packet.stream_index == videoStream)
            {
                if (0 != avcodec_send_packet(codec, &packet))
                    break;
                // ������Ƶ��
                if (0 == avcodec_receive_frame(codec, &frame))
                {
                    swsCtx = readRgb(&frame, swsCtx);
                    break;
                }
            }
            av_packet_unref(&packet);
        }
        sleep(10);
    }

    avcodec_free_context(&codec);
    avformat_close_input(&pFormatctx);
}

SwsContext *AvThread::readRgb(AVFrame *fr, SwsContext *ctx)
{
    // ����һ��AVCodecContext��AVFrame������������
    ctx = sws_getCachedContext(ctx, fr->width, fr->height, (AVPixelFormat)fr->format, fr->width, fr->height, AV_PIX_FMT_RGB24, 0, NULL, NULL, NULL);
    // ִ��Ӳ�����ٵ�YUVתRGB
    QImage img(fr->width, fr->height, QImage::Format_RGB888);
    int lineSz[] = { 3 * fr->width };
    uint8_t *buf[] = { img.bits(), img.bits() + fr->width* fr->height, img.bits() + 2 * fr->width* fr->height };
    sws_scale(ctx, fr->data, fr->linesize, 0, fr->height, buf, lineSz);
    emit readFrame(img);
    return ctx;
}
