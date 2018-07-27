#include "utexcept.h"

EXCEPTION_DISPOSITION __cdecl _except_handler(
	_In_ struct _EXCEPTION_RECORD* _ExceptionRecord,
	_In_ void*                     _EstablisherFrame,
	_Inout_ struct _CONTEXT*       _ContextRecord,
	_Inout_ void*                  _DispatcherContext
) {
	printf("--------------------------Error Occurred--------------------------\n");
	_ContextRecord->Ebp = exregs[0];
	_ContextRecord->Esp = exregs[1];
	_ContextRecord->Ebx = exregs[2];
	_ContextRecord->Edi = exregs[3];
	_ContextRecord->Esi = exregs[4];
	_ContextRecord->Eip = exregs[5];
	return ExceptionContinueExecution;
}