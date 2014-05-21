///////////////////////////////////////////////////////////////////////////////
///
/// 版权所有 (c) 2011 - 2012
///
/// 原始文件名称	 : SystemHook.h
/// 工程名称         : AntinvaderDriver
/// 创建时间         : 2011-04-2
/// 	         
///
/// 描述	         : HOOK挂钩功能的头文件
///
/// 更新维护:
///  0000 [2011-04-2]  最初版本.
///
///////////////////////////////////////////////////////////////////////////////

#pragma once

////////////////////////////
//		数据结构
////////////////////////////

//SSDT
typedef struct _SERVICE_DESCRIPTOR_ENTRY{
        unsigned int *ServiceTableBase;
        unsigned int *ServiceCounterTableBase; 
        unsigned int NumberOfServices;
        unsigned char *ParamTableBase;
} SERVICE_DESCRIPTOR_ENTRY, *PSERVICE_DESCRIPTOR_ENTRY;

//ZwCreateProcess函数指针
typedef NTSTATUS \
( * ZW_CREATE_PROCESS )(
	__out PHANDLE ProcessHandle,
	__in ACCESS_MASK DesiredAccess,
	__in POBJECT_ATTRIBUTES ObjectAttributes,
	__in HANDLE InheritFromProcessHandle,
	__in BOOLEAN InheritHandles,
	__in_opt HANDLE SectionHandle ,
	__in_opt HANDLE DebugPort ,
	__in_opt HANDLE ExceptionPort
	);

////////////////////////////
//		数据导出
////////////////////////////

//原始的ZwCreateProcess
extern "C"
NTSYSAPI
NTSTATUS
NTAPI
ZwCreateProcess(
	__out PHANDLE ProcessHandle,
	__in ACCESS_MASK DesiredAccess,
	__in POBJECT_ATTRIBUTES ObjectAttributes,
	__in HANDLE InheritFromProcessHandle,
	__in BOOLEAN InheritHandles,
	__in_opt HANDLE SectionHandle ,
	__in_opt HANDLE DebugPort ,
	__in_opt HANDLE ExceptionPort
	);

//导出SSDT表
extern "C" __declspec(dllimport) SERVICE_DESCRIPTOR_ENTRY KeServiceDescriptorTable;

////////////////////////////
//		宏定义
////////////////////////////

//一般的文件路径长度 用于猜测打开进程的路径长度,如果调整的较为准确
//可以提高效率,设置的越大时间复杂度越小,空间复杂度越高
#define HOOK_NORMAL_PROCESS_PATH	NORMAL_PATH_LENGTH

//SSDT地址
#define SSDT_ADDRESS_OF_FUNCTION(_function) \
			KeServiceDescriptorTable.ServiceTableBase[ *(PULONG)((PUCHAR)_function+1)]

//内存标志
#define MEM_HOOK_TAG	'hook'

////////////////////////////
//		变量定义
////////////////////////////

//保存原始的ZwCreateOprocess地址
ZW_CREATE_PROCESS ZwCreateProcessOriginal;

////////////////////////////
//		函数申明
////////////////////////////

//准备挂钩代替的函数
extern "C"
NTSTATUS
AntinvaderNewCreateProcess(
	__out PHANDLE ProcessHandle,
	__in ACCESS_MASK DesiredAccess,
	__in POBJECT_ATTRIBUTES ObjectAttributes,
	__in HANDLE InheritFromProcessHandle,
	__in BOOLEAN InheritHandles,
	__in_opt HANDLE SectionHandle ,
	__in_opt HANDLE DebugPort ,
	__in_opt HANDLE ExceptionPort
	);

VOID 
WriteProtectionOn();

VOID 
WriteProtectionOff();
