#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <android/log.h>
#include "emulator_check.h"

#define I386 -386
#define X8664 -8664
#define MALLOC_FAIL -1
#define REAL_DEVICE_STATUS 2
#define VIRTUAL_DEVICE_STATUS 1
#define PROT PROT_EXEC|PROT_WRITE|PROT_READ
#define PAGE_START(addr) (~(getpagesize() - 1) & (addr))
#define LOGI(...) __android_log_print(ANDROID_LOG_ERROR,"emulator_check",__VA_ARGS__)

const int handledSignals[] = {SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS};
const int handledSignalsNum = sizeof(handledSignals) / sizeof(handledSignals[0]);
struct sigaction old_handlers[5];
int (*asmcheck)();

/*
 * Native代码崩溃（即信号异常）捕获的代码片段
 */
void my_sigaction(int signal, siginfo_t *info, void *reserved) {
    LOGI("Crash detect signal %d",signal);
    exit(0);
}

int nativeCrashHandler_onload(JNIEnv *env) {
    struct sigaction handler;
    memset(&handler, 0, sizeof(sigaction));
    handler.sa_sigaction = my_sigaction;
    handler.sa_flags = SA_RESETHAND;
    for (int i = 0; i < handledSignalsNum; ++i) {
        sigaction(handledSignals[i], &handler, &old_handlers[i]);
    }
    return 1;
}

int detect() {
    int mp;
    int status = 0;
    void *exec = malloc(0x1000);
    if (!exec)
        return MALLOC_FAIL;  //申请内存失败，也侧面反映出是真机
    #if defined(__arm__)
//      对应的arm 32位指令集
//      #!cpp
//      __asm __volatile (
//        1    STMFD  SP!,{R4-R7,LR}
//        2    MOV	R6, #0            为r6赋初值
//        3    MOV	R7, PC            PC指向第5行指令所在位置，3~6行用来获得覆盖$address“新指令”的地址
//        4    MOV	R4, #0
//        5    ADD	R6, R6, #1        用来覆盖$address的“新指令”
//        6    LDR	R5, [R7]
//        7    code:
//        8    ADD	R4, R4, #1        这就是$address，是对r4加1
//        9    MOV	R7, PC            9~11行的作用就是把第5行的指令写到第8行
//        10   SUB	R7, R7, #0xC
//        11   STR	R5, [R7]
//        12   CMP	R4, #2            控制循环次数
//        13   BGE	out
//        14   CMP	R6, #2            控制循环次数
//        15   BGE	out               不满足循环次数，则调回去
//        16   B	code
//        17   out:
//        18   MOV	R0, R4            把r4的值作为返回值
//        19   LDMFD SP!,{R4-R7,PC}
//      );
        char code[] = "\xF0\x40\x2D\xE9\x00\x60\xA0\xE3\x0F\x70\xA0\xE1\x00\x40\xA0\xE3"
                      "\x01\x60\x86\xE2\x00\x50\x97\xE5\x01\x40\x84\xE2\x0F\x70\xA0\xE1"
                      "\x0C\x70\x47\xE2\x00\x50\x87\xE5\x02\x00\x54\xE3\x02\x00\x00\xAA"
                      "\x02\x00\x56\xE3\x00\x00\x00\xAA\xF6\xFF\xFF\xEA\x04\x00\xA0\xE1"
                      "\xF0\x80\xBD\xE8";
        void *page_start_addr = (void *)PAGE_START((uint32_t)exec);
        /*
         * 在执行这段机器码后，理应只有1或者2这两种结果，测试发现还有返回0的情况，通过循环将这类噪音过滤
         */
        do{
            memcpy(exec, code, sizeof(code)+1);
            mp = mprotect(page_start_addr, getpagesize(), PROT);
            if (mp < 0) {
                return mp * 100; //代码没有执行权限，侧面反映出host机器是真机
            }
            LOGI("magic_addr = %x", exec);
            asmcheck = exec;
            status = asmcheck();
        }
        while(status != 1 && status != 2);
    #elif defined(__aarch64__)
//      对应的arm 64位指令集
//      #!cpp
//      __asm __volatile (
//        1    SUB SP, SP, #0x30      开辟栈空间
//        2    STR X9, [SP, #8]
//        3    STR X10, [SP, #0x10]
//        4    STR X11, [SP, #0x18]
//        5    STR X12, [SP, #0x20]
//        6    MOV X10, #0
//        7    _start:
//        8    ADR X11, _start        adr伪指令，自动取_start的地址（相对于PC的），并存放到x11寄存器中
//        9    add X11, X11, #12      x11加12，指向第11行指令
//        10   MOV X12, #0            为x12赋初值
//        11   ADD X10, X10, #1       用来覆盖$address的"新指令"
//        12   LDR X9, [X11]
//        13   code:
//        14   ADD X12, X12, #1       这就是$address，是对X12加1
//        15   ADR X11, code          adr伪指令，自动取code的地址（相对于PC的，即第14行指令），并存放到x11寄存器中
//        16   STR X9, [X11]
//        17   CMP X12, #2            控制循环次数
//        18   BGE out                跳出循环
//        19   CMP X10, #2            控制循环次数
//        20   BGE out                跳出循环
//        21   B   code               指定次数内的循环调回去
//        22   out:
//        23   MOV W0, W12            64位寄存器用作32位寄存器时记作:W
//        24   LDR X9, [SP, #8]
//        25   LDR X10, [SP, #0x10]
//        26   LDR X11, [SP, #0x18]
//        27   LDR X12, [SP, #0x20]
//        28   ADD SP, SP, #0x30      出栈，恢复栈空间
//        29   RET
//      );

        char code[] = "\xFF\xC3\x00\xD1\xE9\x07\x00\xF9\xEA\x0B\x00\xF9\xEB\x0F\x00\xF9"
                      "\xEC\x13\x00\xF9\x0A\x00\x80\xD2\x0B\x00\x00\x10\x6B\x31\x00\x91"
                      "\x0C\x00\x80\xD2\x4A\x05\x00\x91\x69\x01\x40\xF9\x8C\x05\x00\x91"
                      "\xEB\xFF\xFF\x10\x69\x01\x00\xF9\x9F\x09\x00\xF1\x8A\x00\x00\x54"
                      "\x5F\x09\x00\xF1\x4A\x00\x00\x54\xF9\xFF\xFF\x17\xE0\x03\x0C\x2A"
                      "\xE9\x07\x40\xF9\xEA\x0B\x40\xF9\xEB\x0F\x40\xF9\xEC\x13\x40\xF9"
                      "\xFF\xC3\x00\x91\xC0\x03\x5F\xD6";
        void *page_start_addr = (void *)PAGE_START((uint64_t)exec);
        do{
            memcpy(exec, code, sizeof(code)+1);
            mp = mprotect(page_start_addr, getpagesize(), PROT);
            if (mp < 0) {
                return mp * 100; //代码没有执行权限，侧面反映出host机器是真机
            }
            LOGI("magic_addr = %x", exec);
            asmcheck = exec;
            status = asmcheck();
        }
        while(status != 1 && status != 2);
    #endif
    LOGI("status = %d", status);
    free(exec);
    return status;
}

JNIEXPORT jint JNICALL Java_cache_faker_DetectService_detect (JNIEnv *env, jobject jobject) {
    nativeCrashHandler_onload(env);
    int statistic_sum = 0;
    #if defined(__i386__)
        LOGI("__i386__  is defined\n");
        return I386;
    #elif defined(__x86_64__)
        LOGI("__x86_64__  is defined\n");
        return X8664;
    #elif defined(__arm__) || defined(__aarch64__)
        for(int j = 0; j < 1000; j++){
            int ret = detect();
            if (ret == REAL_DEVICE_STATUS) {
                statistic_sum += 0;
            }
            else if (ret == VIRTUAL_DEVICE_STATUS) {
                statistic_sum += 1;
            }
            else{
                /*
                 * 等于-1，表示malloc分配内存失败
                 * 等于-100，表示分配的内存空间没有执行权限
                 */
                statistic_sum = ret;
                break;
            }
        }
        LOGI("cache模拟器检测概率分布值 = %d", statistic_sum);
    #endif
    return statistic_sum;
}
