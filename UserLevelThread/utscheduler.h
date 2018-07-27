#pragma once
#include "uthread.h"

//调度器
typedef struct uThreadScheduler {
	struct uThread * uThdNormal;//可执行线程
	struct uThread * uThdSleep;//睡眠中线程
	struct uThread * uThdFinish;//执行完毕线程
}*utSch;

utSch utSchedulerCreate();
void utSchedulerDestory(utSch sch);
void utSchAddUthd(utSch sch, uThd thd);
void utSchStart(utSch sch);