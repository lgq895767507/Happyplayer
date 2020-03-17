//
// Created by Administrator on 2020/3/14/014.
//

#ifndef HAPPYPLAYER_MACRO_H
#define HAPPYPLAYER_MACRO_H

#include <android/log.h>

//if (dataSource){
//delete dataSource;
//dataSource = 0;
//}

//定义释放的宏函数
#define DELETE(object) if(object){delete object; object = 0;}

//定义日志打印宏函数
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "NEFFMPEG",__VA_ARGS__)

//标记线程模式
#define  THREAD_MAIN 1
#define  THREAD_CHILD 2

//错误代码
#define ERROR_CODE_OK 0
#define ERROR_CODE_FFMPEG_PREPARE -1000
#define ERROR_CODE_FFMPEG_PLAY -2000
#define ERROR_CODE_PROGRESS_SETTING -3000

//打不开媒体数据源
#define FFMPEG_CAN_NOT_OPEN_URL (ERROR_CODE_FFMPEG_PREPARE - 1)
//找不到媒体流信息
#define FFMPEG_CAN_NOT_FIND_STREAMS (ERROR_CODE_FFMPEG_PREPARE - 2)
//找不到解码器
#define FFMPEG_FIND_DECODER_FAIL (ERROR_CODE_FFMPEG_PREPARE - 3)
//无法根据解码器创建上下文
#define FFMPEG_ALLOC_CODEC_CONTEXT_FAIL (ERROR_CODE_FFMPEG_PREPARE - 4)
//根据流信息 配置上下文参数失败
#define FFMPEG_CODEC_CONTEXT_PARAMETERS_FAIL (ERROR_CODE_FFMPEG_PREPARE - 5)
//打开解码器失败
#define FFMPEG_OPEN_DECODER_FAIL (ERROR_CODE_FFMPEG_PREPARE - 6)
//没有音视频
#define FFMPEG_NOMEDIA (ERROR_CODE_FFMPEG_PREPARE - 7)

//读取媒体数据包失败
#define FFMPEG_READ_PACKETS_FAIL (ERROR_CODE_FFMPEG_PLAY - 1)

//进度条设置参数有误
#define ERROR_CODE_PROGRESS (ERROR_CODE_PROGRESS_SETTING - 1)
//videoChannel或者audioChannel为空
#define ERROR_CODE_PROGRESS_VIDEO_OR_AUDIO_NULL (ERROR_CODE_PROGRESS_SETTING - 2)
//formatContext为空
#define ERROR_CODE_PROGRESS_FORMAT_CONTEXT_NULL (ERROR_CODE_PROGRESS_SETTING - 3)
//av_seek_frame设置进度条有误
#define ERROR_CODE_AV_SEEK_FRAME (ERROR_CODE_PROGRESS_SETTING - 4)

#endif //HAPPYPLAYER_MACRO_H
