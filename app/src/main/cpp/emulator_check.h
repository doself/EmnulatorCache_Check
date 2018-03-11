//
// Created by wireghost on 2018/3/3.
//
#include <jni.h>

#ifndef EMNULATORCACHE_CHECK_EMULATOR_CHECK_H
#define EMNULATORCACHE_CHECK_EMULATOR_CHECK_H
#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jboolean JNICALL Java_com_snail_antifake_jni_EmulatorDetectUtil_detect(JNIEnv *, jobject);

#ifdef __cplusplus
}
#endif
#endif //EMNULATORCACHE_CHECK_EMULATOR_CHECK_H
