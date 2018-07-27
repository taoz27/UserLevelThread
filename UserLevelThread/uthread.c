#include <stdlib.h>
#include "uthread.h"


/*
函数相关


调用函数时  call func
push eip
jump func

调用函数  有参数 func（int i） 
push i;
call func;


func(){
	esp



函数执行完毕的时候  ret
pop eip


}

*/

uThd current;//保存当前纤程的指针，以便yield中对其进行保存操作

uThd uThreadCreated(void *stack, int stackSize, function func) {
	uThd u = (struct uThread *) malloc(sizeof(struct uThread));
	u->stack = stack;
	u->stackSize = stackSize;
	u->regs[1] = (int)stack + stackSize - 4;//esp要指向堆栈的末端，因为堆栈每次push是在减小
	u->hasRun = 0;
	u->status = UtStatusNormal;//就绪
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

//开始或恢复运行纤程
void NAKED utResume(uThd u) {
	/*
	
	u->hasrun 
	没有运行过 call func 第一次调用
	
	已经运行过  

		是否结束  直接退出
		未执行完  从u-》regs恢复上下文 继续运行
	
	
	*/



	_asm {
		/*时刻确保ecx是参数u*/
		mov ecx, dword ptr[esp + 4];//获取参数u

		//保存调用resume时的外部信息
		mov dword ptr[pregs + 0h], ebp;
		mov dword ptr[pregs + 8h], ebx;
		mov dword ptr[pregs + 0ch], edi;
		mov dword ptr[pregs + 10h], esi;
		pop eax;//获取eip
		mov dword ptr[pregs + 14h], eax;//保存eip
		mov dword ptr[pregs + 4h], esp;//保存esp

		//current=u
		mov dword ptr[current], ecx;

		//判断是否运行过
		mov eax, dword ptr[ecx + 30h];//获取u->hasRun
		cmp eax, 1;
		jz runned;//运行过
				 //未运行过 更换堆栈，并call func
		mov dword ptr[ecx + 30h], 1;//置u->hasRun=1;

		mov eax, dword ptr[ecx + 4h];//获取堆栈的底端，这个值保存在uthd->regs[1]---->esp中
		mov esp, eax;//更换堆栈
		//mov esi, dword ptr[ecx + 10h];//这里必须要修改esi，否则会报错 ---ummm好像也没必要切换
									  //-1073741788,   "{错误类型}/n请求的操作所请求的对象类型与在请求中指定的对象类型不符合。"
		mov eax, dword ptr[ecx + 18h];//获取func
		push ecx;//要时刻保证ecx=参数u，将ecx压栈，确保其不会丢失
		call eax;
		pop ecx;//弹栈，恢复ecx
		mov dword ptr[ecx + 34h], 2;//能执行到这里，必定是函数已执行完毕，ret回来了，所以置u->status=finish;

	runned://如果运行过，分两种情况
		mov eax, dword ptr[ecx + 34h];//获取status
		cmp eax, 2;
		jz myexit;//已运行过，且已经执行完毕

		//已运行过，但未执行完毕
		//恢复寄存器，继续运行

		//TODO 此处可以封装成一个函数
		mov ebp, dword ptr[ecx];
		mov esp, dword ptr[ecx + 4h];
		mov ebx, dword ptr[ecx + 08h];
		mov edi, dword ptr[ecx + 0ch];
		mov esi, dword ptr[ecx + 10h];
		mov eax, dword ptr[ecx + 14h];//eip  不能直接对eip赋值  用jmp
		jmp eax;

	myexit://切换为调度器的寄存器，继续调度		标号不能起特殊的名字，比如说exit
		mov ebp, dword ptr[pregs + 0h];
		mov esp, dword ptr[pregs + 4h];
		mov ebx, dword ptr[pregs + 8h];
		mov edi, dword ptr[pregs + 0ch];
		mov esi, dword ptr[pregs + 10h];
		mov eax, dword ptr[pregs + 14h];
		jmp eax;

	}

	/**
	mov ecx, dword ptr[u]
	pop eax
	mov dword ptr[ecx + 24h], eax;//edi->pRegs[3]
	pop eax
	mov dword ptr[ecx + 28h], eax;//esi->pRegs[4]
	pop eax
	mov dword ptr[ecx + 20h], eax;//ebx->pRegs[2]
	mov esp, ebp
	pop eax
	mov dword ptr[ecx + 18h], eax;//ebp->pRegs[0]
	pop eax
	mov dword ptr[ecx + 2ch], eax;//save eip

	; pop ecx;//弹出参数压栈
	mov dword ptr[ecx + 1ch], esp;//esp->pRegs[1]
	}
	current = u;
	//准备运行纤程
	//未运行过
	if (!u->hasRun) {
	u->hasRun = 1;
	_asm {
	mov ecx, dword ptr[u]
	mov eax, dword ptr[ecx + 4h];//get stack end
	mov esp, eax;//change stack
	mov esi, dword ptr[ecx + 10h]//这里必须要修改esi，否则会报错
	//-1073741788,   "{错误类型}/n请求的操作所请求的对象类型与在请求中指定的对象类型不符合。"
	mov eax, dword ptr[ecx + 30h];//get func
	call eax
	//pop ebp
	}
	//TODO:恢复esp，stack
	u->exit = 1;
	}
	//已运行过
	//已运行结束 跳出到外部去继续运行其他纤程
	if (u->exit) {
	//TODO: swap();
	_asm {
	mov ecx, dword ptr[u]
	mov ebp, dword ptr[ecx + 18h]
	mov esp, dword ptr[ecx + 1ch]
	mov ebx, dword ptr[ecx + 20h]
	mov edi, dword ptr[ecx + 24h]
	mov esi, dword ptr[ecx + 28h]
	mov eax, dword ptr[ecx + 2ch]
	jmp eax
	}
	}

	//已开始执行，但还未结束
	//恢复寄存器，继续执行
	_asm {
	mov ecx, dword ptr[u]
	mov ebp, dword ptr[ecx]
	mov esp, dword ptr[ecx + 4h]
	mov ebx, dword ptr[ecx + 08h]
	mov edi, dword ptr[ecx + 0ch]
	mov esi, dword ptr[ecx + 10h]
	mov eax, dword ptr[ecx + 14h]
	jmp eax
	}
	/**/
}

void NAKED utYield() {
	_asm {
		//保存线程上下文
		mov ecx, dword ptr[current];//获取当前运行的线程
		mov dword ptr[ecx], ebp;
		mov dword ptr[ecx + 8h], ebx;
		mov dword ptr[ecx + 0ch], edi;
		mov dword ptr[ecx + 10h], esi;
		pop eax;//eip
		mov dword ptr[ecx + 14h], eax;//eip
		mov dword ptr[ecx + 4h], esp;//esp

		//恢复到调度器，继续运行其他纤程
		mov ebp, dword ptr[pregs + 0h];
		mov esp, dword ptr[pregs + 4h];
		mov ebx, dword ptr[pregs + 8h];
		mov edi, dword ptr[pregs + 0ch];
		mov esi, dword ptr[pregs + 10h];
		mov eax, dword ptr[pregs + 14h];//eip
		jmp eax;
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
		mov ecx, dword ptr[current];
		mov dword ptr[ecx], ebp;
		mov dword ptr[ecx + 8h], ebx;
		mov dword ptr[ecx + 0ch], edi;
		mov dword ptr[ecx + 10h], esi;
		pop eax;
		mov dword ptr[ecx + 14h], eax;//eip
		mov dword ptr[ecx + 4h], esp;

		//保存睡眠相关数据
		//current->status = UtStatusSleep;
		//current->sleepDuri = duri;
		//DWORD curMils = GetTickCount();
		//current->sleepTime = curMils;
		mov dword ptr[ecx + 34h], 1;//status=sleep
		mov eax, dword ptr[esp];//参数duri
		mov dword ptr[ecx + 28h], eax;//sleepduri=duri;
		call GetTickCount;//获取当前时间，返回值在eax中
		mov dword ptr[ecx + 24h], eax;//sleeptime=time;

		//类似yield，恢复到调度器，继续运行其他纤程
		mov ebp, dword ptr[pregs + 0h];
		mov esp, dword ptr[pregs + 4h];
		mov ebx, dword ptr[pregs + 8h];
		mov edi, dword ptr[pregs + 0ch];
		mov esi, dword ptr[pregs + 10h];
		mov eax, dword ptr[pregs + 14h];
		jmp eax;
	}
}