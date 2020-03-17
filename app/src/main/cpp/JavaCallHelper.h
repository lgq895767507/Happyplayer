//
// Created by Administrator on 2020/3/14/014.
//

#ifndef HAPPYPLAYER_JAVACALLHELPER_H
#define HAPPYPLAYER_JAVACALLHELPER_H


#include <jni.h>
#include "macro.h"

class JavaCallHelper {
public:
    JavaCallHelper(JavaVM *javaVM_, JNIEnv *env_, jobject instance_);

    ~JavaCallHelper();

    void onPrepared(int threadMode);

    void onError(int threadMode, int errorCode);

    void onProgress(int threadMode, int progress);

private:
    JavaVM *javaVM;
    JNIEnv *env;
    jobject instance;
    jmethodID jmd_prepared;
    jmethodID jmd_onError;
    jmethodID jmd_onProgress;
};


#endif //HAPPYPLAYER_JAVACALLHELPER_H
