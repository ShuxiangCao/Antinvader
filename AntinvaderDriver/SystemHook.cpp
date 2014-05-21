///////////////////////////////////////////////////////////////////////////////
///
/// 版权所有 (c) 2011 - 2012
///
/// 原始文件名称	 : SystemHook.cpp
/// 工程名称         : AntinvaderDriver
/// 创建时间         : 2011-04-2
/// 	         
///
/// 描述	         : HOOK挂钩功能的实现文件
///
/// 更新维护:
///  0000 [2011-04-2]  最初版本.
///
///////////////////////////////////////////////////////////////////////////////

#include "SystemHook.h"

/*---------------------------------------------------------
函数名称:	WriteProtectionOn
函数描述:	开启写保护和中断
输入参数:
输出参数:
返回值:		
其他:		
更新维护:	2011.4.3     最初版本
---------------------------------------------------------*/
inline
VOID WriteProtectionOn()
{
	__asm{
		MOV EAX,CR0
		OR EAX,10000H//关闭写保护
		MOV CR0,EAX
		STI//允许中断
	}
}

/*---------------------------------------------------------
函数名称:	WriteProtectionOff
函数描述:	关闭写保护和中断
输入参数:
输出参数:
返回值:		
其他:		
更新维护:	2011.4.3     最初版本
---------------------------------------------------------*/
inline
VOID WriteProtectionOff()
{
	__asm{
		CLI//不允许中断
		MOV EAX,CR0
		AND EAX,NOT 10000H//取消写保护
		MOV CR0,EAX
	}
}

VOID HookInitializeFunctionAddress()
{
	//保存原始地址
	ZwCreateProcessOriginal 
		= (ZW_CREATE_PROCESS)SSDT_ADDRESS_OF_FUNCTION(ZwCreateProcess);
}

VOID HookOnSSDT()
{
	//关闭写保护 不允许中断
	WriteProtectionOff();

	//修改新地址
	SSDT_ADDRESS_OF_FUNCTION(ZwCreateProcess) 
				= (UCHAR)AntinvaderNewCreateProcess;

	//关闭写保护 不允许中断
	WriteProtectionOn();
}

VOID HookOffSSDT()
{
	//原始地址一定为空
	ASSERT( ZwCreateProcessOriginal );

	//关闭写保护 不允许中断
	WriteProtectionOff();

	//修改回原来的地址
	SSDT_ADDRESS_OF_FUNCTION(ZwCreateProcess) 
				= (UCHAR)ZwCreateProcessOriginal;

	//关闭写保护 不允许中断
	WriteProtectionOn();
}

/*---------------------------------------------------------
函数名称:	AntinvaderNewCreateProcess
函数描述:	用于替换ZwCreateProcess的挂钩函数
输入参数:	同ZwCreateProcess
输出参数:	同ZwCreateProcess
返回值:		同ZwCreateProcess
其他:		
更新维护:	2011.4.5     最初版本
---------------------------------------------------------*/

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
	)
{
/*	//返回值
	NTSTATUS status;

	//将要打开进程的目录的文件对象
	PFILE_OBJECT pFileRootObject;

	//将由打开进程的文件对象
	PFILE_OBJECT pFileObject;

	//保存文件路径的空间 先猜一个大小.
	WCHAR  wPathString[ HOOK_NORMAL_PROCESS_PATH ];
	
	//保存文件路径空间的地址
	PWSTR pPathString = wPathString;

	//文件路径
	PUNICODE_STRING usFilePath = {0};

	//新申请的文件路径 临时使用
	PUNICODE_STRING usFilePathNewAllocated = {0};

	//文件路径长度
	ULONG ulPathLength;
	USHORT ustPathLength;

	//
	//先调用原版的ZwCreateProcess创建进程
	//

	status = ZwCreateProcessOriginal(
			ProcessHandle,
			DesiredAccess,
			ObjectAttributes,
			InheritFromProcessHandle,
			InheritHandles,
			SectionHandle ,
			DebugPort ,
			ExceptionPort
			);

	//
	//如果创建失败就不需要判断了
	//

	if( !NT_SUCCESS(status) )
	{
		return status;
	}

	//
	//先判断进程名称是否匹配 不匹配就返回
	//

	if( IsProcessConfidential( ObjectAttributes -> ObjectName ,NULL , NULL ))
	{
		return STATUS_SUCCESS;
	}

	//
	//初始化字符串
	//
	RtlInitEmptyUnicodeString(
		usFilePath,
		pPathString,
		HOOK_NORMAL_PROCESS_PATH
		);

	//
	//获取进程所在路径的文件对象,失败就直接返回
	//

	status = ObReferenceObjectByHandle(
		ObjectAttributes -> RootDirectory,//路径句柄
		GENERIC_READ,//只读
		*IoFileObjectType,//文件对象
		KernelMode,//内核模式
		(PVOID *)&pFileRootObject,//保存对象地址的空间
		NULL//驱动程序使用NULL
		);

	if( !NT_SUCCESS(status) )
	{
		return STATUS_SUCCESS;
	}

	//
	//使用文件对象获取路径
	//

	ulPathLength = FctGetFilePath(
		pFileRootObject,
		usFilePath,
		CONFIDENTIAL_FILE_NAME_FILE_OBJECT
		);

	//
	//判断猜测的内存是否过小,如果查询失败则ulPathLength = 0,不会执行下列语句
	//
	if( ulPathLength > HOOK_NORMAL_PROCESS_PATH )
	{
		//
		//申请内存,连着进程名称也申请了
		//
		pPathString = (PWSTR)ExAllocatePoolWithTag(
						NonPagedPool,
						ulPathLength + ObjectAttributes -> ObjectName -> Length + 1,
						MEM_HOOK_TAG
						); 

		//
		//重新初始化字符串
		//
		RtlInitEmptyUnicodeString(
				usFilePath,
				pPathString,
				(USHORT)(ulPathLength + ObjectAttributes -> ObjectName -> Length +1 )
				//路径长度 + 一个斜杠长度 + 文件名长度
				);

		//
		//重新查询
		//
		ulPathLength = FctGetFilePath(
				pFileRootObject,
				usFilePath,
				CONFIDENTIAL_FILE_NAME_FILE_OBJECT
				);
	}

	if( !ulPathLength )
	{
		//
		//如果获取失败使用QUERY_NAME_STRING查询
		//

		ulPathLength = FctGetFilePath(
			pFileRootObject,
			usFilePath,
			CONFIDENTIAL_FILE_NAME_QUERY_NAME_STRING
			);

		if( ulPathLength > HOOK_NORMAL_PROCESS_PATH )
		{
			//
			//申请内存,连着进程名称也申请了
			//
			pPathString = (PWSTR)ExAllocatePoolWithTag(
							NonPagedPool,
							ulPathLength + ObjectAttributes -> ObjectName -> Length + 1 ,
							MEM_HOOK_TAG
							); 

			//
			//重新初始化字符串
			//
			RtlInitEmptyUnicodeString(
					usFilePath,
					pPathString,
					(USHORT)(ulPathLength + ObjectAttributes -> ObjectName -> Length +1 )
					//路径长度 + 一个斜杠长度 + 文件名长度
					);

			//
			//重新查询
			//
			ulPathLength = FctGetFilePath(
					pFileRootObject,
					usFilePath,
					CONFIDENTIAL_FILE_NAME_QUERY_NAME_STRING
					);
		}

		if( !ulPathLength )
		{
			//
			//还是失败,没招了,直接返回
			//

			return STATUS_SUCCESS;
		}
	}

	if( ulPathLength + 1 > usFilePath -> MaximumLength )
	{
		//
		//申请内存,连着进程名称也申请了
		//
		pPathString = (PWSTR)ExAllocatePoolWithTag(
						NonPagedPool,
						ulPathLength + ObjectAttributes -> ObjectName -> Length + 1 ,
						MEM_HOOK_TAG
						); 

		//
		//重新初始化字符串
		//
		RtlInitEmptyUnicodeString(
				usFilePathNewAllocated,
				pPathString,
				(USHORT)(ulPathLength + ObjectAttributes -> ObjectName -> Length +1 )
				//路径长度 + 一个斜杠长度 + 文件名长度
				);

		//
		//拷贝字符串
		//
		RtlCopyUnicodeString( usFilePathNewAllocated , usFilePath );

		usFilePath = usFilePathNewAllocated;
	}

	//
	//判断路径后面是否是"\",如果不是就加上
	//

	if( usFilePath->Buffer[ulPathLength-1] != L'\\')
	{
		RtlAppendUnicodeToString(usFilePath , L"\\");
	}

	//
	//如果加上进程名称大小又不够了
	//
	if( ulPathLength + 1 +  ObjectAttributes -> ObjectName -> Length > 
												usFilePath -> MaximumLength )
	{
		//
		//申请内存
		//
		pPathString = (PWSTR)ExAllocatePoolWithTag(
						NonPagedPool,
						ulPathLength + ObjectAttributes -> ObjectName -> Length + 1 ,
						MEM_HOOK_TAG
						); 

		//
		//重新初始化字符串
		//
		RtlInitEmptyUnicodeString(
				usFilePathNewAllocated,
				pPathString,
				(USHORT)(ulPathLength + ObjectAttributes -> ObjectName -> Length +1 )
				//路径长度 + 一个斜杠长度 + 文件名长度
				);

		//
		//拷贝字符串
		//
		RtlCopyUnicodeString( usFilePathNewAllocated , usFilePath );

		usFilePath = usFilePathNewAllocated;
	}

	//
	//把文件名增加上去
	//
	RtlAppendUnicodeStringToString(usFilePath,ObjectAttributes -> ObjectName);
	
	//
	//现在判断进程路径是否匹配 不匹配就返回
	//

	if( IsProcessConfidential( NULL ,usFilePath , NULL ))
	{
		return STATUS_SUCCESS;
	}
*/
	return STATUS_SUCCESS;
}
