#include <stdio.h>
#include <Windows.h>
#include "uthread.h"
#include "utscheduler.h"

#define CYCLES 100//控制测试函数循环次数
#define PRINT 1//控制是否打印输出
int recursion = CYCLES / 2;//递归次数

/*
普通测试函数1
*/
void normalFunc1() {
	int i = 0;
	while (i<CYCLES){
		if (PRINT)
			printf("Normal Func1: Cycle%d.\n", i);
		utYield();
		i++;
	}
}

/*
普通测试函数2
暂不支持传参
*/
void normalFunc2() {
	int i = 0;
	while (i<CYCLES){
		float f = 1.8*3.6;
		if (PRINT)
			printf("Normal Func2: Cycle%d,%f.\n", i,f);
		utYield();
		i++;
	}
}

/*
递归测试函数
*/
void recurFunc() {
	int i = recursion;

	if (PRINT)
		printf("Recursion Func: Recursion%d.\n", i);
	recursion--;
	utYield();

	if (recursion == 0)return;
	recurFunc();

	utYield();
	if (PRINT)
		printf("Normal Func2: Recursion%d.\n", i);
}

/*
睡眠测试函数
*/
void sleepFunc() {
	//获取当前时间，并打印
	DWORD mils = GetTickCount();
	SYSTEMTIME time;
	GetSystemTime(&time);
	if(PRINT)
		printf("Sleep Func: Current Time: %d: Min-%d, Sec-%d\n", mils, time.wMinute, time.wSecond);

	//睡眠
	utSleep(5000);
	
	mils = GetTickCount();
	GetSystemTime(&time);
	if (PRINT)
		printf("Sleep Func: Current Time: %d: Min-%d, Sec-%d\n", mils, time.wMinute, time.wSecond);
}

/*
异常测试函数
*/
void exceptFunc() {
	if (PRINT)
		printf("Exception Func: Make Error.\n");
	_asm{
		xor eax, eax;
		mov [eax], 0;//向地址为0处赋值，产生异常
	}
	if (PRINT)
		printf("Exception Func: Error Clear.\n");
}

/*
异常测试函数
*/
void exceptFunc2() {
	if (PRINT)
		printf("Exception Func: Make Error Divid 0.\n");
	int zero = 0;
	int a = 5 / zero;
	if (PRINT)
		printf("Exception Func: Error Clear.\n");
}

utSch sch;//线程调度器

void init() {
	sch = utSchedulerCreate();
	utSchAddUthd(sch, uThreadCreate(normalFunc1));
	utSchAddUthd(sch, uThreadCreate(normalFunc2));
	utSchAddUthd(sch, uThreadCreate(recurFunc));
	utSchAddUthd(sch, uThreadCreate(sleepFunc));
	utSchAddUthd(sch, uThreadCreate(exceptFunc));
	utSchAddUthd(sch, uThreadCreate(exceptFunc2));
}

int add(int op1, int op2) {
	int res = op1 + op2;
	return res;
}

int main() {
	//int res = add(1, 2);

	/**/
	init();
	printf("-------------------------Test Start-------------------------\n");
	utSchStart(sch);//开始调度
	utSchedulerDestory(sch);//销毁调度器，释放内存
	printf("-------------------------Test End-------------------------\n");
	/**/

	system("pause");
	return 0;
}