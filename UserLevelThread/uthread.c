#include <stdlib.h>
#include "uthread.h"

uThd current;//保存当前纤程的指针，以便yield中对其进行保存操作

uThd uThreadCreated(void *stack, int stackSize, function func) {
	uThd u = (struct uThread *) malloc(sizeof(struct uThread));
	u->stack = stack;
	u->stackSize = stackSize;
	u->regs[1] = (int)stack + stackSize - 4;//esp要指向堆栈的末端，因为堆栈每次push是在减小
	u->status = UtStatusUnstart;//就绪
	u->func = func;//代码段 函数入口
	u->next = NULL;
	return u;
}

uThd uThreadCreate(function func) {
	int stackSize = 4 * 1024 * 1024;//默认分配4MB的堆栈空间
	char *stack = NULL;
	stack = (char*)malloc(sizeof(char)*stackSize);
	return uThreadCreated(stack, stackSize, func);
}

void uThreadDestory(uThd u) {
	free(u->stack);
	free(u);
}

//保存调度器的寄存器数据
void NAKED pushPregs() {
	_asm {
		pop edx;

		//保存调用resume时的外部信息
		mov dword ptr[pregs + 0h], ebp;
		mov dword ptr[pregs + 8h], ebx;
		mov dword ptr[pregs + 0ch], edi;
		mov dword ptr[pregs + 10h], esi;
		pop eax;//获取eip
		mov dword ptr[pregs + 14h], eax;//保存eip
		mov dword ptr[pregs + 4h], esp;//保存esp

		jmp edx;
	}
}
//从ecx中恢复线程数据，并让线程开始运行
void NAKED popEcx() {
	_asm {
		pop edx;

		mov ebp, dword ptr[ecx];
		mov esp, dword ptr[ecx + 4h];
		mov ebx, dword ptr[ecx + 08h];
		mov edi, dword ptr[ecx + 0ch];
		mov esi, dword ptr[ecx + 10h];
		mov eax, dword ptr[ecx + 14h];//eip  不能直接对eip赋值  用jmp
		jmp eax;
	}
}
//从pregs中恢复调度器寄存器，并让调度器开始调度
void NAKED popPregs() {
	_asm {
		pop edx;

		mov ebp, dword ptr[pregs + 0h];
		mov esp, dword ptr[pregs + 4h];
		mov ebx, dword ptr[pregs + 8h];
		mov edi, dword ptr[pregs + 0ch];
		mov esi, dword ptr[pregs + 10h];
		mov eax, dword ptr[pregs + 14h];
		jmp eax;
	}
}
//将寄存器保存到ecx中
void NAKED pushEcx() {
	_asm {
		pop edx;

		mov ecx, dword ptr[current];//获取当前运行的线程
		mov dword ptr[ecx], ebp;
		mov dword ptr[ecx + 8h], ebx;
		mov dword ptr[ecx + 0ch], edi;
		mov dword ptr[ecx + 10h], esi;
		pop eax;//eip
		mov dword ptr[ecx + 14h], eax;//eip
		mov dword ptr[ecx + 4h], esp;//esp

		jmp edx;
	}
}

//开始或恢复运行纤程
void NAKED utResume(uThd u) {
	_asm {
		/*时刻确保ecx是参数u*/
		mov ecx, dword ptr[esp + 4];//获取参数u

		call pushPregs;

		//current=u
		mov dword ptr[current], ecx;

		//判断是否运行过
		mov eax, dword ptr[ecx + 30h];//获取u->hasRun
		cmp eax, UtStatusUnstart;
		ja runned;//运行过
				 //未运行过 更换堆栈，并call func
		mov dword ptr[ecx + 30h], UtStatusNormal;//置u->hasRun=1;status=normal

		mov eax, dword ptr[ecx + 4h];//获取堆栈的底端，这个值保存在uthd->regs[1]---->esp中
		mov esp, eax;//更换堆栈
		mov eax, dword ptr[ecx + 18h];//获取func
		push ecx;//要时刻保证ecx=参数u，将ecx压栈，确保其不会丢失
		call eax;
		pop ecx;//弹栈，恢复ecx
		mov dword ptr[ecx + 30h], UtStatusFinish;//能执行到这里，必定是函数已执行完毕，ret回来了，所以置u->status=finish;

	runned://如果运行过，分两种情况
		mov eax, dword ptr[ecx + 30h];//获取status
		cmp eax, UtStatusFinish;
		jz myexit;//已运行过，且已经执行完毕

		//已运行过，但未执行完毕
		//恢复寄存器，继续运行
		call popEcx;

	myexit://切换为调度器的寄存器，继续调度		标号不能起特殊的名字，比如说exit
		call popPregs;

	}
}

void NAKED utYield() {
	_asm {
		////保存线程上下文
		call pushEcx;

		//恢复到调度器，继续运行其他纤程
		call popPregs;
	}
}

/*
睡眠函数
其实大致相当于yield，只是多保存了睡眠开始时间和持续时间
随后在调度器中会对所有的睡眠线程进行检测，判断其是否要恢复运行
*/
void NAKED utSleep(DWORD duri) {
	_asm {
		//类似yield，保存上下文
		call pushEcx;

		//保存睡眠相关数据
		//current->status = UtStatusSleep;
		//current->sleepDuri = duri;
		//DWORD curMils = GetTickCount();
		//current->sleepTime = curMils;
		mov dword ptr[ecx + 30h], UtStatusSleep;//status=sleep
		mov eax, dword ptr[esp];//参数duri
		mov dword ptr[ecx + 28h], eax;//sleepduri=duri;
		call GetTickCount;//获取当前时间，返回值在eax中
		mov dword ptr[ecx + 24h], eax;//sleeptime=time;

		//类似yield，恢复到调度器，继续运行其他纤程
		call popPregs;
	}
}