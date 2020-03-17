//
// Created by Administrator on 2020/3/14/014.
//


#include "VideoChannel.h"


void *task_video_decode(void *args) {
    VideoChannel *videoChannel = static_cast<VideoChannel *>(args);
    //2 dataSource
    videoChannel->video_decode();
    return 0;//一定一定一定要返回0！！！
}

void *task_video_play(void *args) {
    VideoChannel *videoChannel = static_cast<VideoChannel *>(args);
    //2 dataSource
    videoChannel->video_play();
    return 0;//一定一定一定要返回0！！！
}

/**
 * 丢包（AVPacket）
 * @param q
 */
void dropAVPacket(queue<AVPacket *> &q) {
    while (!q.empty()) {
        AVPacket *avPacket = q.front();
        // I 帧、B 帧、 P 帧
        //不能丢 I 帧,AV_PKT_FLAG_KEY: I帧（关键帧）
        if (avPacket->flags != AV_PKT_FLAG_KEY) {
            //丢弃非 I 帧
            BaseChannel::releaseAVPacket(&avPacket);
            q.pop();
        } else {
            break;
        }
    }
}

/**
 * 丢包（AVFrame）
 * @param q
 */
void dropAVFrame(queue<AVFrame *> &q) {
    if (!q.empty()) {
        AVFrame *avFrame = q.front();
        BaseChannel::releaseAVFrame(&avFrame);
        q.pop();
    }
}

VideoChannel::VideoChannel(int id, AVCodecContext *codecContext, double fps, AVRational time_base,
                           JavaCallHelper *javaCallHelper) : BaseChannel(id,
                                                                         codecContext,
                                                                         time_base,
                                                                         javaCallHelper) {
    this->fps = fps;

    packets.setSyncHandle(dropAVPacket);
    frames.setSyncHandle(dropAVFrame);
}

VideoChannel::~VideoChannel() {

}

void VideoChannel::start() {

    isPlaying = 1;
    //设置队列状态为工作状态
    packets.setWork(1);
    frames.setWork(1);

    //解码
    pthread_create(&pid_video_decode, 0, task_video_decode, this);
    //播放
    pthread_create(&pid_video_play, 0, task_video_play, this);
}

void VideoChannel::stop() {
    isPlaying = 0;
    javaCallHelper = 0;
    packets.setWork(0);
    frames.setWork(0);
    pthread_join(pid_video_decode, 0);
    pthread_join(pid_video_play, 0);

}

void VideoChannel::video_decode() {
    AVPacket *packet = 0;
    while (isPlaying) {

        if (isPause) {
            //如果暂停了就不执行下面的内容
            continue;
        }

        int ret = packets.pop(packet);

        if (!isPlaying) {
            //如果停止播放了，跳出循环 释放packet
            break;
        }

        if (!ret) {
            //取数据包失败
            continue;
        }

        //拿到了视频数据包（编码压缩了的），需要把数据包给解码器进行解码
        ret = avcodec_send_packet(codecContext, packet);

        if (ret) {
            //往解码器发送数据包失败，跳出循环
            break;
        }

        releaseAVPacket(&packet);//释放packet，后面不需要了

        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(codecContext, frame);

        if (ret == AVERROR(EAGAIN)) {
            //重来
            continue;
        } else if (ret != 0) {
            break;
        }

        //ret == 0 数据收发正常,成功获取到了解码后的视频原始数据包 AVFrame ，格式是 yuv
        //需要对frame队列做一个限制，否则会导致内存泄露
        while (frames.size() > 100) {
            av_usleep(10 * 1000); //休眠10ms
            continue;
        }

        frames.push(frame);
    }

    releaseAVPacket(&packet);
}

void VideoChannel::video_play() {
    AVFrame *frame = 0;
    //要注意对原始数据进行格式转换：yuv > rgba
    // yuv: 400x800 > rgba: 400x800
    uint8_t *dst_data[4];
    int dst_linesize[4];

    SwsContext *sws_ctx = sws_getContext(codecContext->width, codecContext->height,
                                         codecContext->pix_fmt,
                                         codecContext->width, codecContext->height, AV_PIX_FMT_RGBA,
                                         SWS_BILINEAR, NULL, NULL, NULL);

    //给 dst_data dst_linesize 申请内存
    av_image_alloc(dst_data, dst_linesize,
                   codecContext->width, codecContext->height, AV_PIX_FMT_RGBA, 1);

    double fps_delay = 1.0 / fps; //单位是秒

    while (isPlaying) {

        if (isPause) {
            if (frame) {
                releaseAVFrame(&frame);
            }
            continue;
        }


        int ret = frames.pop(frame);

        if (!isPlaying) {
            //如果停止播放了，跳出循环 释放packet
            break;
        }

        if (!ret) {
            //取数据包失败
            continue;
        }

        //取到了yuv原始数据，下面要进行格式转换
        sws_scale(sws_ctx, frame->data,
                  frame->linesize, 0, codecContext->height, dst_data, dst_linesize);

        //在渲染之前处理每帧停留的时间
        double extra_frame_time = frame->repeat_pict / (2 * fps);//单位秒
        double real_delay = (extra_frame_time + fps_delay);
        //av_usleep(real_delay); //microseconds
        //获取视频的播放时间
        double video_time = frame->best_effort_timestamp * av_q2d(time_base);

        if (!audioChannel) {
            //没有音频。直接按照视频解码的方式播放
            av_usleep(real_delay * 1000000);
            if (javaCallHelper) {
                javaCallHelper->onProgress(THREAD_CHILD, video_time);
            }
        } else {
            double audioTime = audioChannel->audio_time;
            //获取音视频播放的时间差
            double time_diff = video_time - audioTime;

            if (time_diff > 0) {
                //LOGE("视频比音频快：%lf", time_diff);
                if (time_diff > 1) {
                    //等音频慢慢赶上来
                    av_usleep((real_delay * 2) * 1000000);
                } else {
                    av_usleep((real_delay + time_diff) * 1000000);
                }
            } else if (time_diff < 0) {
                //LOGE("音频比视频快: %lf", fabs(time_diff));
                if (fabs(time_diff) >= 0.05) {
                    //时间差如果大于0.05，有明显的延迟感
                    frames.sync();
                    //packets.sync()
                    continue;
                }
            }
        }

        //dst_data: AV_PIX_FMT_RGBA格式的数据
        //渲染，回调出去> native-lib里
        // 渲染一副图像 需要什么信息？
        // 宽高！> 图像的尺寸！
        // 图像的内容！(数据)>图像怎么画
        //需要：1，data;2，linesize；3，width; 4， height
        renderCallback(dst_data[0], dst_linesize[0], codecContext->width, codecContext->height);
        releaseAVFrame(&frame);
    }

    releaseAVFrame(&frame);
    isPlaying = 0;
    av_freep(&dst_data[0]);
    sws_freeContext(sws_ctx);
}

void VideoChannel::setRenderCallback(RenderCallback callback) {
    this->renderCallback = callback;
}

void VideoChannel::setAudioChannel(AudioChannel *audioChannel) {
    this->audioChannel = audioChannel;
}

