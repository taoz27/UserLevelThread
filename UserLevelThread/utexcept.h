#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "register.h"

registers exregs;//Òì³£´íÎó»Ö¸´¼Ä´æÆ÷

EXCEPTION_DISPOSITION __cdecl _except_handler(
	_In_ struct _EXCEPTION_RECORD* _ExceptionRecord,
	_In_ void*                     _EstablisherFrame,
	_Inout_ struct _CONTEXT*       _ContextRecord,
	_Inout_ void*                  _DispatcherContext
);