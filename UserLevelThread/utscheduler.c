#include <stdlib.h>
#include "utscheduler.h"
#include "utexcept.h"

/*
在每次调度了某个线程一次后，都会来检测一下当前线程的状态，以便及时将睡眠、执行完毕线程切换到相应链表 <?貌似用队列更好一些吧?> 中
*/
void checkCurrent(uThd* prev,uThd* current,utSch sch) {
	if ((*current)->status == UtStatusNormal) {
		//由于其他两种情况（sleep、finish）下都会导致寻找下一个可执行线程必须用prev->next，所以这里这么做可以统一
		//切换为队列会更好
		(*prev) = (*prev)->next;
		return;
	}
	if ((*current)->status == UtStatusSleep) {
		(*prev)->next = (*current)->next;
		(*current)->next = sch->uThdSleep->next;
		sch->uThdSleep->next = (*current);
		return;
	}
	if ((*current)->status == UtStatusFinish){
		(*prev)->next = (*current)->next;
		(*current)->next = sch->uThdFinish->next;
		sch->uThdFinish->next = (*current);
		return;
	}
}

/*
检测睡眠线程中有没有该醒来的
*/
void checkSleepUthd(utSch sch) {
	uThd utSleepPrev = sch->uThdSleep;
	uThd utSleep = sch->uThdSleep->next;
	while (utSleep != NULL) {
		DWORD curMils = GetTickCount();
		if (curMils > (utSleep->sleepTime + utSleep->sleepDuri)) {
			utSleepPrev->next = utSleep->next;
			utSleep->next = sch->uThdNormal->next;
			sch->uThdNormal->next = utSleep;
			utSleep = utSleepPrev->next;
		}
		else {
			utSleep = utSleep->next;
			utSleepPrev = utSleepPrev->next;
		}
	}
}

utSch utSchedulerCreate() {
	utSch sch;
	sch = (utSch)malloc(sizeof(struct uThreadScheduler));
	sch->uThdNormal = (uThd)malloc(sizeof(struct uThread));
	sch->uThdNormal->next = NULL;
	sch->uThdSleep = (uThd)malloc(sizeof(struct uThread));
	sch->uThdSleep->next = NULL;
	sch->uThdFinish = (uThd)malloc(sizeof(struct uThread));
	sch->uThdFinish->next = NULL;
	return sch;
}

void utSchedulerDestory(utSch sch) {
	uThd temp1 = sch->uThdNormal->next, temp2;
	while (temp1){
		temp2 = temp1->next;
		uThreadDestory(temp1);
		temp1 = temp2;
	}
	temp1 = sch->uThdSleep->next;
	while (temp1) {
		temp2 = temp1->next;
		uThreadDestory(temp1);
		temp1 = temp2;
	}
	temp1 = sch->uThdFinish->next;
	while (temp1) {
		temp2 = temp1->next;
		uThreadDestory(temp1);
		temp1 = temp2;
	}
	free(sch->uThdNormal);
	free(sch->uThdSleep);
	free(sch->uThdFinish);
	free(sch);
}

void utSchAddUthd(utSch sch, uThd thd) {
	thd->next = sch->uThdNormal->next;
	sch->uThdNormal->next = thd;
}

void utSchStart(utSch sch) {
	uThd prev = sch->uThdNormal;//指向current的前一个，用于移动线程
	uThd current = sch->uThdNormal->next;//取出就绪链表中的第一个线程
	if (current==NULL){//如果为空，则表明没有线程
		printf("-------------------------无用户级线程-------------------------\n");
		return;
	}

	while (sch->uThdNormal->next!=NULL||sch->uThdSleep->next!=NULL){//当就绪链表和睡眠链表均为空时，结束循环

		if (current != NULL) {//可能存在normal表为空，sleep表不为空
			//相当于try
			_asm {
				//保存异常发生前的环境
				mov dword ptr[exregs + 0h], ebp;
				mov dword ptr[exregs + 8h], ebx;
				mov dword ptr[exregs + 0ch], edi;
				mov dword ptr[exregs + 10h], esi;
				mov dword ptr[exregs + 14h], offset cachec;//eip
				//异常处理函数
				push _except_handler;
				push fs : [0];
				mov fs : [0], esp;
				mov dword ptr[exregs + 4h], esp;//esp
			}
			//开始或恢复线程
			utResume(current);
			//相当于cache
			_asm {
				jmp rightc;//正常情况下，直接跳出该块
			cachec:
				mov eax, dword ptr[current];
				mov dword ptr[eax + 34h], 2;//将当前线程结束
			rightc:
				mov eax, [esp];
				mov fs : [0], eax;
				add esp, 8;
			}
			//检测当前线程状态
			checkCurrent(&prev, &current, sch);
		}

		//检测睡眠线程
		checkSleepUthd(sch);

		//从就绪队列中取出一个线程
		if (prev == NULL)
			prev = sch->uThdNormal;
		current = prev->next;//执行checkcurrent之后，prev已经变成了此处正确的prev
		if (current == NULL) {
			prev = sch->uThdNormal;
			current = sch->uThdNormal->next;
		}

	}
}