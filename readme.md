# 用户级线程库

首先创建一个线程调度器，然后创建多个线程（如不做特殊说明，本文中以下的&quot;线程&quot;均指&quot;用户级线程&quot;），并将其添加到线程调度器中。随后调度器开始执行调度，找到适合运行的线程，将执行权交给他。线程执行过程中可能调用yield()交权，也可能调用sleep()将自己睡眠，也可能出现异常。

所以大致可以分为四个模块：调度器、线程、睡眠函数、异常处理。

##线程调度器的实现

调度器的实现可以分为创建、销毁、添加线程、开始调度四部分，如下：
	
	utSch utSchedulerCreate();//线程调度器的创建
	void utSchedulerDestory(utSchsch);//线程调度器的销毁
	void utSchAddUthd(utSchsch, uThdthd);//添加一个线程
	void utSchStart(utSchsch);//开始调度

而调度器的结构体定义可以简略的概括为三个线程链表：可执行（就绪）线程表、睡眠线程表、结束线程表

	typedefstructuThreadScheduler {

        struct uThread * uThdNormal;//可执行线程
        struct uThread * uThdSleep;//睡眠中线程
        struct uThread * uThdFinish;//执行完毕线程

	}*utSch;

线程调度器的创建、销毁及添加线程都比较简单，在此不做赘述。只写一下调度的实现过程。

#图片

首先从就绪队列中取出一个线程，然后将执行权转交给该线程，线程在运行中调用了yield()或sleep()或执行完毕之后，会再次将执行权回交给调度器，调度器继续运行，检查刚刚执行的线程的状态，如果线程处于睡眠状态或结束状态，则将其移动到相应的睡眠或结束链表中。然后检查所有的睡眠线程，判断当前时间是否大于睡眠线程的开始睡眠时间加上睡眠线程的睡眠持续时间，如有，则将其移动到就绪链表中以唤醒该线程。然后重复循环，直到所有的线程均执行完毕，即就绪链表与睡眠链表均为空。

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
			...
			//开始或恢复线程
			utResume(current);
			//相当于cache
			...
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

检查当前线程与检查睡眠线程都比较简单，不再赘述。

##线程的实现

线程的实现可以分为线程创建、销毁、继续运行、交权四个部分，如下：

	//创建一个自定义栈大小的线程
	uThd uThreadCreated(void \*stack, intstackSize, functionfunc);
	
	//创建一个默认栈大小的线程
	uThd uThreadCreate(functionfunc);
	
	//销毁一个线程，不负责销毁next
	void uThreadDestory(uThdu);
	
	//开始运行或恢复运行
	void utResume(uThdu);
	
	//交出运行权限
	void utYield();

而线程的结构体由执行函数func、执行过程中的上下文寄存器regs及各自的栈和一些状态组成。

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

寄存器中ebp与局部变量有关，esp与栈有关，eip指向即将被执行的指令地址。结构体中的regs变量主要用于在切换线程时，将当前线程的相应寄存器保存起来，在下次继续运行时再恢复这些寄存器，线程便可从上次中断处继续运行。

每个线程都分配一块内存用作栈，方便对栈的管理，相互之间不会冲突。

线程的创建比较简单，但需要注意，初始化时esp指向栈的底端。减4是因为运行在32位系统上。
	
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

线程被调度运行的过程大致如下图：

由于我们的调度器与所有的用户级线程是在同一个进程（内核级线程）中执行的，所以首先要保存调度器的上下文寄存器，以便在随后再恢复调度器的上下文，转权给调度器，让其去调度其他线程。

随后判断当前线程是否被调度过，因为如果没有调度过，便没有上下文可以来恢复。如果没有，那么就调用线程函数，开始调度；如果曾调度过，并且未曾执行完毕，那么就不应重新执行，而应当从上下文寄存器中恢复寄存器，从断点处继续运行。

#图片

而在运行过程中，函数可能会调用yield()，此时保存线程寄存器到regs中，并将调度器的上下文寄存器恢复，让权给调度器；如果调用sleep()，则保存线程寄存器到regs中，保存睡眠变量，改变状态为statusSleep，并将调度器的上下文寄存器恢复，让权给调度器；如果函数执行完毕，则会自动返回到调用函数的地方，从那里继续运行，修改状态为statusFinish，并将调度器上下文寄存器恢复，让权给调度器。


##线程的完善

正如上一章所看到的，一般函数的汇编代码中添加了一些优化代码，用于控制堆栈平衡及预留局部变量空间，而我们在写线程函数时，这些优化代码会对我们的代码产生极大的影响，这个时候我们就要使用裸函数。

裸函数就是在代码中申明函数后，编译时，除了入口参数压栈，然后call这个函数外，编译器不会为函数体生成任何代码。

使用**__declspec(naked)**可以声明一个裸函数。

	#defineNAKED\_declspec(naked)//裸函数

接下来开始完善我们的线程。

线程被调度运行的过程如下（见第四章的图）

首先要保存调度器的上下文寄存器，以便在随后再恢复调度器的上下文，转权给调度器，让其去调度其他线程。

随后判断当前线程是否被调度过。如果没有，那么就调用线程函数，开始调度；如果曾调度过，并且未曾执行完毕，那么就不应重新执行，而应当从上下文寄存器中恢复寄存器，从断点处继续运行。

而在运行过程中，函数可能会调用yield()，此时保存线程寄存器到regs中，并将调度器的上下文寄存器恢复，让权给调度器；如果调用sleep()，则保存线程寄存器到regs中，保存睡眠变量，改变状态为statusSleep，并将调度器的上下文寄存器恢复，让权给调度器；如果函数执行完毕，则会自动返回到调用函数的地方，从那里继续运行，修改状态为statusFinish，并将调度器上下文寄存器恢复，让权给调度器。

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

在一开始获取参数u，我们使用的是：

	mov ecx, dword ptr[esp + 4];//获取参数u

这是因为当前esp指向的是calluThdResume()时压栈的eip，再之前压栈的便是参数u了，而push时栈顶esp是在减小，所以可知[esp+4]便是参数u。

需要注意的是：

	mov eax, dword ptr[ecx + 18h];//获取func入口
	call eax;

在执行call时，会自动将call之后的下一条指令地址（即返回地址）push到线程自己的栈当中，那么在线程运行结束之后，ret时会自动返回到此处calleax后的下一条指令。当程序运行到这里时，我们就可以确定线程必定已执行完毕，那么就可以置状态位为finish了。

第7章YIELD与SLEEP

函数在调用yield()之后，表明其执行时间段已经完毕，希望将控制权交给调度器，去调度其他线程。

在这个过程中，需要先保存当前线程的寄存器，然后再恢复调度器的寄存器上下文，将控制权交给调度器。

	void NAKED utYield() {
		_asm {
			////保存线程上下文
			call pushEcx;
	
			//恢复到调度器，继续运行其他纤程
			call popPregs;
		}
	}

将sleep与yield放到同一章是因为二者十分相似，都是放弃自身的控制权，保存寄存器，恢复调度器寄存器，将控制权交给调度器。

唯一不同的是sleep需要多保存一个睡眠的变量。

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

看中间的一段代码。汇编代码的作用相当于注释中的c代码。

在这里，获取参数duri是直接使用的[esp]，是因为之前再保存线程寄存器时，已经将栈中的eip给pop出来了，所以esp所指向的就是参数duri了。

随后在获取当前时间时，直接callGetTickCount，由第5章可知，其会将返回值保存在eax寄存器中，所以从eax中可以获取当前时间。

##异常处理

在线程发生异常时，操作系统会设法调用用户定义的回调函数，这个函数可以做任何事情，但最终他必须返回一个值，来告诉操作系统接下来去做什么。这个回调函数原型如下：

	EXCEPTION_DISPOSITION __cdecl _except_handler(
	_In_ struct _EXCEPTION_RECORD* _ExceptionRecord,
	_In_ void*                     _EstablisherFrame,
	_Inout_ struct _CONTEXT*       _ContextRecord,
	_Inout_ void*                  _DispatcherContext
	);
返回值EXCEPTION_DISPOSITION用于表明该异常回调函数是否已经正确处理了异常。枚举如下：
	
	typedef enum _EXCEPTION_DISPOSITION
	{
         ExceptionContinueExecution = 0,//异常被忽略或已处理，回到异常指令，尝试重新执行异常指令。
         ExceptionContinueSearch = 1,//异常未被识别、处理，继续寻找下一个异常回调函数
         ExceptionNestedException = 2,//后两个不常用
         ExceptionCollidedUnwind = 3
	} EXCEPTION\_DISPOSITION;

回掉函数_except_handler的第一个参数ExceptionRecord中存储这操作系统分配给该异常的编号，例如STATUS_ACCESS_VIOLATION的编号是0xC0000005。

第二个参数EstablisherFrame暂时不用理解。

第三个参数ContextRecord是一个指向CONTEXT的指针，CONTEXT中定义并存储了线程中用到的寄存器，例如EAX，EBX，ESP，EIP等。

第四个参数DispatcherContext暂时也忽略。

所以当程序发生异常时，操作系统会试图调用回调函数，并把相应的参数传入，然后函数返回一个值，告诉操作系统是否已正确处理，如果处理了，就回到程序中继续运行，如果没有，则去寻找下一异常回调函数。

那么问题是，系统如何能够找到用户定义的异常回调函数呢？

这里就要用到一个称为EXCEPTION\_REGISTRATION的结构，结构体如下：

	typedef struct _EXCEPTION_REGISTRATION_RECORD
	{
	     PEXCEPTION_REGISTRATION_RECORD Next;//下一个异常EXCEPTION_REGISTRATION
	     PEXCEPTION_DISPOSITION Handler;//异常回调函数
	} EXCEPTION_REGISTRATION_RECORD, *PEXCEPTION_REGISTRATION_RECORD;

通过这样一个链表可以将所有的异常回调函数串在一起。

那么如何寻找第一个EXCEPTION_REGISTRATION结构？

每个线程都有自己的异常处理程序回调函数，每个线程都有一个很重要的数据结构，即线程信息块（TIB或TEB）。TIB中的第一个DWORD是指向该线程的EXCEPTION\_REGISTRATION结构的指针。在Intel Win32平台上，FS寄存器始终指向当前的TIB。因此，通过FS：[0]可以找到一个指向EXCEPTION\_REGISTRATION结构的指针。

 ##图片

所以，将所有的都串起来，并结合下图，当程序发生异常时，操作系统首先通过线程信息块TIB（即FS:[0]）找到第一个异常处理表EXCEPTION\_REGISTRATION，每个EXCEPTION\_REGISTRATION包含两个变量，异常回调指针handler和next指针（这里有的说是next，有的说是prev），在回调函数执行完毕后，通过返回值确定是恢复线程运行还是继续查找回调函数。

 ##图片

所以简单实现以下异常处理try…cache。

	//相当于try
	_asm {
		//保存异常发生前的环境
		mov dword ptr[exregs + 0h], ebp;
		mov dword ptr[exregs + 8h], ebx;
		mov dword ptr[exregs + 0ch], edi;
		mov dword ptr[exregs + 10h], esi;
		mov dword ptr[exregs + 14h], offset cachec;//eip
		//压入异常处理函数
		push _except_handler;
		push fs : [0];//构造EXCEPTION_REGISTRATION
		mov fs : [0], esp;
		mov dword ptr[exregs + 4h], esp;//esp
	}
	//开始或恢复线程
	utResume(current);
	//相当于cache
	_asm {
		jmp rightc;//正常情况下，没发生异常时，会执行这条语句，直接跳出该块
	cachec://发生异常时，进入这里
		mov eax, dword ptr[current];
		mov dword ptr[eax + 34h], 2;//将当前线程结束
	rightc:
		mov eax, [esp];
		mov fs : [0], eax;
		add esp, 8;
	}


在rusume之前，保存可能出错之前的寄存器到exregs中，在保存eip时，为了让发生异常时能够回到cache中，直接mov dword ptr[exregs + 14h], offset cachec;//eip，将cache的指令地址赋给exregs中的eip。然后在栈中构造一个EXCEPTION\_REGISTRATION，即push \_except\_handler; push fs : [0];，然后让fs : [0]指向esp，即mov fs : [0],esp;这样就相当于构造了一个try块，

然后在cache中的处理是，由于该线程发生异常，我们也不必对其进行修改，直接将它kill掉就可以了，即把它的status改为finish即可。然后恢复fs : [0]，平衡堆栈。

	EXCEPTION_DISPOSITION __cdecl _except_handler(
		_In_ struct _EXCEPTION_RECORD* _ExceptionRecord,
		_In_ void*                     _EstablisherFrame,
		_Inout_ struct _CONTEXT*       _ContextRecord,
		_Inout_ void*                  _DispatcherContext
	) {
		printf("--------------------------Error Occurred--------------------------\n");
	//恢复异常发生前的寄存器
		_ContextRecord->Ebp = exregs[0];
		_ContextRecord->Esp = exregs[1];
		_ContextRecord->Ebx = exregs[2];
		_ContextRecord->Edi = exregs[3];
		_ContextRecord->Esi = exregs[4];
		_ContextRecord->Eip = exregs[5];
		return ExceptionContinueExecution;
	}

在异常回调函数中的处理是，直接恢复发生异常前的寄存器，然后返回到cache中，即可。

##实现效果


最终运行效果如下：
##图片

由于是后添加的异常线程，所以先执行异常线程，执行时打印&quot;MakeError&quot;，然后开始产生除0错误和越界错误，当检测到错误后回去执行异常处理函数，打印&quot;ErrorOccurred&quot;表明检测到异常，并开始恢复异常前的寄存器，恢复后，会杀掉出错线程，所以异常函数中的打印&quot;ErrorClear&quot;并没有执行。

之后开始调度睡眠线程执行，执行后处于睡眠状态，五秒内不再调度他。随后递归线程、普通线程2、普通线程1开始交替执行。

 ##图片

我设置了循环次数为100次，可以看到，所有的其他线程都执行完了，过了一会，睡眠线程又开始执行，并结束了。睡眠线程开始时间为6s的时候（图1中sec-6），结束时间为11s的时候（图2中sec-11），中间刚好相差5秒。
##参考
1. [**https://en.wikipedia.org/wiki/Win32\_Thread\_Information\_Block**](https://en.wikipedia.org/wiki/Win32_Thread_Information_Block)
**Win32 Thread Information Block-wikipedia**
2. [**http://bytepointer.com/resources/pietrek\_crash\_course\_depths\_of\_win32\_seh.htm**](http://bytepointer.com/resources/pietrek_crash_course_depths_of_win32_seh.htm)
**A Crash Course on the Depths of Win32**** ™ **** Structured Exception Handling**
3. [**http://www.nirsoft.net/kernel\_struct/vista/index.html**](http://www.nirsoft.net/kernel_struct/vista/index.html)
**Windows Vista Kernel Structures**
4. [**http://www.cnblogs.com/sniperHW/archive/2012/06/19/2554574.html#3583865**](http://www.cnblogs.com/sniperHW/archive/2012/06/19/2554574.html#3583865)
**实现c协程**
5. [**https://github.com/Tencent/libco**](https://github.com/Tencent/libco)
**腾讯libco开源库**