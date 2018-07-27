#pragma once
#include <Windows.h>
#include "register.h"

typedef void(*function)();

//�߳�״̬
enum UtStatus{
	UtStatusNormal=0, 
	UtStatusSleep=1, 
	UtStatusFinish=2
};

typedef struct uThread{
	registers regs;//ebp 0h�;ֲ������й�,esp 4hʼ��ָ���ջ��,ebx 8h,edi ch,esi 10h,eip 14hָ����һ��ָ��ĵ�ַ
	function func;//18h ����
	void *stack;//1ch Ϊ���̵߳�������Ķ�ջ
	int stackSize;//20hh ��ջ��С
	DWORD sleepTime;//24h ˯�߿�ʼʱ��---ֻ��statusΪUtStatusSleepʱ������
	DWORD sleepDuri;//28h ˯�߳���ʱ��---ֻ��statusΪUtStatusSleepʱ������
	struct uThread * next;//2ch ָ��������е���һ���̣߳�������Ƶ���ȱ��
	int hasRun;//30h �߳��Ƿ��ѿ�ʼִ�У���ƵĲ�������Ӧ�ϲ���status��
	enum UtStatus status;//34h �߳�״̬
}*uThd;

registers pregs;//�߳��ⲿ�����������ģ���������

//����һ���߳�
uThd uThreadCreated(void *stack, int stackSize, function func);
uThd uThreadCreate(function func);
//����һ���̣߳�����������next
void uThreadDestory(uThd u);
//��ʼ���л�ָ�����
void utResume(uThd u);
//��������Ȩ��
void utYield();
//˯��
void utSleep(DWORD duri);