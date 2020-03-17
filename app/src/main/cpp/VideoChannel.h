//
// Created by Administrator on 2020/3/14/014.
//

#ifndef HAPPYPLAYER_VIDEOCHANNEL_H
#define HAPPYPLAYER_VIDEOCHANNEL_H

#include "BaseChannel.h"

extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
};

#include "AudioChannel.h"

typedef void (*RenderCallback)(uint8_t *, int, int, int);

class VideoChannel : public BaseChannel {
public:
    VideoChannel(int id, AVCodecContext *codecContext, double fps, AVRational time_base,
                 JavaCallHelper *javaCallHelper);

    ~VideoChannel();

    void start();

    void stop();

    void video_decode();

    void video_play();

    void setRenderCallback(RenderCallback renderCallback);

    void setAudioChannel(AudioChannel *audioChannel);

private:
    pthread_t pid_video_decode;
    pthread_t pid_video_play;
    RenderCallback renderCallback;
    double fps;
    AudioChannel *audioChannel = 0;
};

#endif //HAPPYPLAYER_VIDEOCHANNEL_H
