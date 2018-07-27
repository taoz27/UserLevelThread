#pragma once
#include <Windows.h>
#include "register.h"

typedef void(*function)();

typedef struct uThread{
	registers regs;//ebp 0h和局部变量有关,esp 4h始终指向堆栈顶,ebx 8h,edi ch,esi 10h,eip 14h指向下一个指令的地址
	function func;//18h 函数
	void *stack;//1ch 为该线程单独分配的堆栈
	int stackSize;//20hh 堆栈大小
	DWORD sleepTime;//24h 睡眠开始时间---只在status为UtStatusSleep时起作用
	DWORD sleepDuri;//28h 睡眠持续时间---只在status为UtStatusSleep时起作用
	struct uThread * next;//2ch 指向调度器中的下一个线程，这里设计的有缺陷
	int status;//30h 线程状态
}*uThd;

registers pregs;//线程外部环境的上下文，即调度器

//创建一个线程
uThd uThreadCreated(void *stack, int stackSize, function func);
uThd uThreadCreate(function func);
//销毁一个线程，不负责销毁next
void uThreadDestory(uThd u);
//开始运行或恢复运行
void utResume(uThd u);
//交出运行权限
void utYield();
//睡眠
void utSleep(DWORD duri);