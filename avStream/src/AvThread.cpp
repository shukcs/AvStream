#include "AvThread.h"
#include <QImage>
#include <qlogging.h>

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
    av_dict_set(&avdic, "stimeout", "3000000", 0);
    AVFormatContext *pFormatctx = avformat_alloc_context();
    if (avformat_open_input(&pFormatctx, m_url.toLocal8Bit().data(), NULL, &avdic) != 0)
    {
        // �����������ļ���qDebug("can't open the file. \n");
        return;
    }
    if (avformat_find_stream_info(pFormatctx, &avdic) < 0)
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
        avformat_close_input(&pFormatctx);
        return;
    }

    auto codec = avcodec_alloc_context3(pCodec);
    if (!codec)
    {
        avformat_close_input(&pFormatctx);
        return;
    }

    // �򿪱������
    if (avcodec_open2(codec, pCodec, NULL) < 0)
    {
        avcodec_free_context(&codec);
        avformat_close_input(&pFormatctx);
        return; // �޷��򿪱������
    }

    auto pParamA = pFormatctx->streams[audioStream]->codecpar;
    auto pCodecA = avcodec_find_decoder(pParamA->codec_id);
    auto codecACtx = avcodec_alloc_context3(pCodecA);
    if (avcodec_open2(codecACtx, pCodecA, NULL) < 0)
        avcodec_free_context(&codecACtx);

    // ��ȡ���ݰ�
    AVPacket packet;
    av_init_packet(&packet);
    auto frame = av_frame_alloc();
    SwsContext *swsCtx = NULL;
    SwrContext *swrCtx = NULL;
    AVChannelLayout ch;
    while (QThread::isRunning())
    {
        while (av_read_frame(pFormatctx, &packet) >= 0)
        {
            if (packet.stream_index == videoStream)
            {
                if (0 != avcodec_send_packet(codec, &packet))
                    break;
                // ������Ƶ��
                if (0 == avcodec_receive_frame(codec, frame))
                    swsCtx = readRgb(frame, swsCtx);

                av_packet_unref(&packet);
            }
            else if (codecACtx && packet.stream_index == audioStream)
            {
                if (0 != avcodec_send_packet(codecACtx, &packet))
                    break;
                // ������Ƶ��
                if (0 == avcodec_receive_frame(codecACtx, frame))
                    swrCtx = readFrAudio(frame, swrCtx, &ch);

                av_packet_unref(&packet);
            }
        }
        msleep(10);
    }
    av_channel_layout_uninit(&ch);
    sws_freeContext(swsCtx);
    swr_free(&swrCtx);
    av_frame_free(&frame);
    avcodec_free_context(&codec);
    avcodec_free_context(&codecACtx);
    avformat_close_input(&pFormatctx);
}

SwsContext *AvThread::readRgb(AVFrame *fr, SwsContext *ctx)
{
    // ����һ��AVCodecContext��AVFrame������������
    if (!ctx)
    {
        ctx = sws_getContext(fr->width, fr->height, (AVPixelFormat)fr->format, fr->width, fr->height, AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
        ///ffmpeg Rec.601
        av_opt_set_int(ctx, "src_range", 0, AV_OPT_SEARCH_CHILDREN);
    }
    // ִ��Ӳ�����ٵ�YUVתRGB
    QImage img(fr->width, fr->height, QImage::Format_RGB888);
    int lineSz[] = { 3 * fr->width };
    uint8_t *buf[] = { img.bits(), img.bits() + fr->width* fr->height, img.bits() + 2 * fr->width* fr->height };
    sws_scale(ctx, fr->data, fr->linesize, 0, fr->height, buf, lineSz);
    emit readFrame(img);
    return ctx;
}

SwrContext * AvThread::readFrAudio(AVFrame *fr, SwrContext *ctx, AVChannelLayout *ch)
{
    if (!ctx)
    {
        av_channel_layout_default(ch, 2);
        swr_alloc_set_opts2(&ctx, ch, AV_SAMPLE_FMT_S16, fr->sample_rate, &fr->ch_layout
            , (AVSampleFormat)fr->format, fr->sample_rate, 0, NULL);
        swr_init(ctx);
    }
    uint8_t *data[1] = { 0 };
    QByteArray arr(fr->nb_samples * 4, 0); //��ͨ��16bit
    data[0] = (uint8_t *)arr.data();
    auto len = swr_convert(ctx, data, fr->nb_samples, fr->data, fr->nb_samples);
    if (len > 0)
    {
        emit readAudio(arr, fr->nb_samples);
    }
    return ctx;
}

