//
// Created by Administrator on 2020/3/14/014.
//

#include "AudioChannel.h"


void *task_audio_decode(void *args) {
    AudioChannel *audioChannel = static_cast<AudioChannel *>(args);
    audioChannel->audio_decode();
    return 0;//一定一定一定要返回0！！！
}

void *task_audio_play(void *args) {
    AudioChannel *audioChannel = static_cast<AudioChannel *>(args);
    audioChannel->audio_play();
    return 0;//一定一定一定要返回0！！！
}

void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    AudioChannel *audioChannel = static_cast<AudioChannel *>(context);
    int pcm_size = audioChannel->getPCM();
    if (pcm_size > 0) {
        (*bq)->Enqueue(bq, audioChannel->out_buffers, pcm_size);
    }
}

AudioChannel::AudioChannel(int id, AVCodecContext *codecContext, AVRational time_base,
                           JavaCallHelper *javaCallHelper)
        : BaseChannel(id, codecContext, time_base, javaCallHelper) {

    //缓冲区大小设置
    out_channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    out_sampleSize = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    out_sampleRate = 44100;
    // 2(通道数) * 2（16bit=2字节）*44100（采样率）
    out_buffers_size = out_channels * out_sampleSize * out_sampleRate;
    out_buffers = static_cast<uint8_t *>(malloc(out_buffers_size));
    memset(out_buffers, 0, out_buffers_size);

    swrContext = swr_alloc_set_opts(0, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16,
                                    out_sampleRate, codecContext->channel_layout,
                                    codecContext->sample_fmt, codecContext->sample_rate,
                                    0, 0);
    //初始化重采样上下文
    swr_init(swrContext);
}

AudioChannel::~AudioChannel() {
    if (swrContext) {
        swr_free(&swrContext);
        swrContext = 0;
    }

    DELETE(out_buffers)
}

void AudioChannel::start() {
    isPlaying = 1;
    //设置队列状态为工作状态
    packets.setWork(1);
    frames.setWork(1);

    //解码
    pthread_create(&pid_audio_decode, 0, task_audio_decode, this);
    //播放
    pthread_create(&pid_audio_play, 0, task_audio_play, this);
}

void AudioChannel::stop() {
    isPlaying = 0;
    javaCallHelper = 0;
    packets.setWork(0);
    frames.setWork(0);
    pthread_join(pid_audio_decode, 0);
    pthread_join(pid_audio_play, 0);
    if (swrContext) {
        swr_free(&swrContext);
        swrContext = 0;
    }
    //释放播放器相关代码
    if (bqPlayerPlay) {
        (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_STOPPED);
    }
    //销毁播放器对象
    if (bqPlayerObject){
        (*bqPlayerObject)->Destroy(bqPlayerObject);
        bqPlayerObject = 0;
        bqPlayerBufferQueue = 0;
    }
    //销毁混音器对象
    if (outputMixObject) {
        (*outputMixObject)->Destroy(outputMixObject);
        outputMixObject = 0;
    }
    //销毁引擎对象
    if (engineObject) {
        (*engineObject)->Destroy(engineObject);
        engineObject = 0;
        engineInterface = 0;
    }
}

void AudioChannel::audio_decode() {
    AVPacket *packet = 0;
    while (isPlaying) {

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

void AudioChannel::audio_play() {

    //创建引擎并获取引擎接口
    SLresult result;
    //创建引擎对象：SLObjectItf
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);

    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    //初始化引擎
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);

    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    //获取引擎接口
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineInterface);

    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    //设置混音器
    result = (*engineInterface)->CreateOutputMix(engineInterface, &outputMixObject, 0, 0, 0);

    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    //初始化混音器
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);

    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    //创建播放器
    // 配置输入声音信息
    //创建buffer缓冲类型的队列 2个队列
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    //pcm数据格式
    //SL_DATAFORMAT_PCM：数据格式为pcm格式
    //2：双声道
    //SL_SAMPLINGRATE_44_1：采样率为44100
    //SL_PCMSAMPLEFORMAT_FIXED_16：采样格式为16bit
    //SL_PCMSAMPLEFORMAT_FIXED_16：数据大小为16bit
    //SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT：左右声道（双声道）
    //SL_BYTEORDER_LITTLEENDIAN：小端模式
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, 2, SL_SAMPLINGRATE_44_1,
                                   SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
                                   SL_BYTEORDER_LITTLEENDIAN};
    //数据源 将上述配置信息放到这个数据源中
    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    // 配置音轨（输出）,设置混音器
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&loc_outmix, NULL};
    //需要的接口 操作队列的接口
    const SLInterfaceID ids[1] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};
    //3.3 创建播放器
    result = (*engineInterface)->CreateAudioPlayer(engineInterface, &bqPlayerObject, &audioSrc,
                                                   &audioSnk, 1, ids, req);

    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    //初始化播放器：SLObjectItf bqPlayerObject
    result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);

    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    //获取播放器接口：SLPlayItf bqPlayerPlay
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);

    if (SL_RESULT_SUCCESS != result) {
        return;
    }

    //设置播放回调函数,获取播放器队列接口：SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue
    (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE, &bqPlayerBufferQueue);
    //设置回调 void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
    //传入函数指针
    (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback, this);

    //设置播放器状态为播放状态
    (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);

    //手动激活回调函数
    bqPlayerCallback(bqPlayerBufferQueue, this);

}

/**
 * 获取pcm数据大小
 * @return
 */
int AudioChannel::getPCM() {

    int pcm_data_size = 0;

    AVFrame *frame = 0;
    /*  SwrContext *swrContext = swr_alloc_set_opts(0, AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16,
                                                  out_sampleRate, codecContext->channel_layout,
                                                  codecContext->sample_fmt, codecContext->sample_rate,
                                                  0, 0);
      //初始化重采样上下文
      swr_init(swrContext);*/

    while (isPlaying) {
        int ret = frames.pop(frame);

        if (!isPlaying) {
            //如果停止播放了，跳出循环 释放packet
            break;
        }

        if (!ret) {
            //取数据包失败
            continue;
        }

        //LOGE("音频播放中");

        //swr_get_delay: 下一个输入数据与下下个输入数据之间的时间间隔
        int64_t delay = swr_get_delay(swrContext, frame->sample_rate);
        //AV_ROUND_UP：向上取整
        int64_t out_max_samples = av_rescale_rnd(frame->nb_samples + delay, frame->sample_rate,
                                                 out_sampleRate,
                                                 AV_ROUND_UP);
        //上下文
        //输出缓冲区
        //输出缓冲区能容纳的最大数据量
        //输入数据
        //输入数据量
        int out_samples = swr_convert(swrContext, &out_buffers, out_max_samples,
                                      (const uint8_t **) (frame->data), frame->nb_samples);
        // 获取swr_convert转换后 out_samples个 *2 （16位）*2（双声道）
        pcm_data_size = out_samples * out_sampleSize * out_channels;

        //获取音频时间 audio_time需要被VideoChannel获取，frame->best_effort_timestamp*时间单位
        audio_time = frame->best_effort_timestamp * av_q2d(time_base);
        //通过回调出去
        if (javaCallHelper) {
            javaCallHelper->onProgress(THREAD_CHILD, audio_time);
        }

        break;

    }//end while

    releaseAVFrame(&frame);

    return pcm_data_size;
}


