//
// Created by Administrator on 2020/3/14/014.
//

#ifndef HAPPYPLAYER_BASECHANNEL_H
#define HAPPYPLAYER_BASECHANNEL_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
};

#include "safe_queue.h"

/**
 * VideoChannel和AudioChannel的父类
 */

extern "C" {
#include <libavutil/time.h>
};

#include "macro.h"
#include "JavaCallHelper.h"

class BaseChannel {
public:
    BaseChannel(int id, AVCodecContext *codecContext, AVRational time_base,
                JavaCallHelper *javaCallHelper) : id(id),
                                                  codecContext(codecContext),
                                                  time_base(time_base),
                                                  javaCallHelper(javaCallHelper) {
        packets.setReleaseCallback(releaseAVPacket);
        frames.setReleaseCallback(releaseAVFrame);
    }

    virtual ~BaseChannel() {
        packets.clear();
        frames.clear();
    }

    /**
     * 释放 AVPacket
     * @param packet
     */
    static void releaseAVPacket(AVPacket **packet) {
        if (packet) {
            av_packet_free(packet);
            *packet = 0;
        }
    }

    /**
     * 释放 AVFrame
     * @param frame
     */
    static void releaseAVFrame(AVFrame **frame) {
        if (frame) {
            av_frame_free(frame);
            *frame = 0;
        }
    }

    //纯虚函数（抽象方法）
    virtual void start() = 0;

    virtual void stop() = 0;


    SafeQueue<AVPacket *> packets;
    SafeQueue<AVFrame *> frames;
    int id;
    bool isPlaying = 0;
    bool isPause = 0;
    AVCodecContext *codecContext;
    AVRational time_base;
    double audio_time;
    JavaCallHelper *javaCallHelper = 0;
};


#endif //HAPPYPLAYER_BASECHANNEL_H
