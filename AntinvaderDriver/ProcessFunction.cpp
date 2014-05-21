///////////////////////////////////////////////////////////////////////////////
///
/// 版权所有 (c) 2011 - 2012
///
/// 原始文件名称	 : ProcessFunction.cpp
/// 工程名称         : AntinvaderDriver
/// 创建时间         : 2011-03-20
/// 	         
///
/// 描述	         : 关于进程信息的功能实现
///
/// 更新维护:
///  0000 [2011-03-20] 最初版本.
///
///////////////////////////////////////////////////////////////////////////////

#include "ProcessFunction.h"

/*---------------------------------------------------------
函数名称:	InitProcessNameOffset
函数描述:	初始化EPROCESS结构中进程名的地址
输入参数:
输出参数:
返回值:		
其他:		参考了楚狂人(谭文)的思路
更新维护:	2011.3.20    最初版本
---------------------------------------------------------*/
void InitProcessNameOffset()
{
	ULONG i;
	PEPROCESS  peCurrentProcess;
	peGlobalProcessSystem = PsGetCurrentProcess();
	
	peCurrentProcess = peGlobalProcessSystem;

	//
	//搜索EPROCESS结构,在其中找到System字符串地址,
	//则该地址为EPROCESS中进程名称地址
	//

	for( i=0 ; i < MAX_EPROCESS_SIZE ; i++ ){
		if(!strncmp("System",
					(PCHAR)peCurrentProcess+i,
					strlen("System"))) {
			stGlobalProcessNameOffset = i;
			break;
		}
	}
}

/*---------------------------------------------------------
函数名称:	GetCurrentProcessName
函数描述:	初始化EPROCESS结构中进程名的地址
输入参数:	usCurrentProcessName	保存进程名的缓冲区
输出参数:	usCurrentProcessName	保存的进程名地址
返回值:		进程名长度
其他:		参考了楚狂人(谭文)的思路
更新维护:	2011.3.20    最初版本
---------------------------------------------------------*/
ULONG GetCurrentProcessName( 
	__in PUNICODE_STRING usCurrentProcessName
	)
{
	PEPROCESS  peCurrentProcess;
	ULONG	i,ulLenth;
    ANSI_STRING ansiCurrentProcessName;
	if(stGlobalProcessNameOffset == 0)
		return 0;

	//
    // 获得当前进程EPROCESS,然后移动一个偏移得到进程名所在位置
	//

	peCurrentProcess = PsGetCurrentProcess();

	//
	//直接将这个字符串填到ansiCurrentProcessName里面
	//

    RtlInitAnsiString(&ansiCurrentProcessName,
						((PCHAR)peCurrentProcess + stGlobalProcessNameOffset));

	//
    // 这个名字是ansi字符串,现在转化为unicode字符串
	//

	//
	//获取需要的长度
	//

    ulLenth = RtlAnsiStringToUnicodeSize(&ansiCurrentProcessName);

    if(ulLenth > usCurrentProcessName->MaximumLength)  {
		//
		//如果长度不够则返回需要的长度
		//
        return ulLenth;
    }

	//
	//转换为Unicode
	//

    RtlAnsiStringToUnicodeString(usCurrentProcessName,
									&ansiCurrentProcessName,FALSE);
	return ulLenth;
}

/*---------------------------------------------------------
函数名称:	IsProcessConfidential
函数描述:	判断进程是否是机密进程
输入参数:	
			usProcessName	进程名称
			usProcessPath	进程路径
			usProcessMd5	进程MD5校验值
输出参数:	
返回值:		完全匹配为0,否则为以下值的或结果

			PROCESS_NAME_NOT_CONFIDENTIAL	名称不匹配
			PROCESS_PATH_NOT_CONFIDENTIAL	路径不匹配
			PROCESS_MD5_NOT_CONFIDENTIAL	MD5校验不匹配

其他:		不需要检查的信息直接填为NULL即可

更新维护:	2011.4.5     最初版本  仅测试notepad.exe
			2011.7.25    接入Pct模块
---------------------------------------------------------
ULONG IsProcessConfidential( 
	PUNICODE_STRING usProcessName,
	PUNICODE_STRING usProcessPath,
	PUNICODE_STRING usProcessMd5
	)
{
	//返回值 记录不匹配信息
	ULONG ulRet;

	//传入数据
	CONFIDENTIAL_PROCESS_DATA cpdProcessData;

	if(!)

	
    UNICODE_STRING usProcessConfidential = { 0 };
	if( usProcessName ){
		RtlInitUnicodeString(&usProcessConfidential,L"notepad.exe");
		if(RtlCompareUnicodeString(usProcessName,&usProcessConfidential,TRUE) == 0)
			return 0;
		return PROCESS_NAME_NOT_CONFIDENTIAL;
	}

	
	return 0;
}*/
/*---------------------------------------------------------
函数名称:	IsCurrentProcessSystem
函数描述:	判断当前进程是否是机密进程
输入参数:	
输出参数:	
返回值:		当前进程是否是System
其他:		测试对WPS加密时发现有时会有System的线程对机密
			文件进行操作.故如果是机密文件且是System进程
			同意加密.
更新维护:	2011.7.27    最初版本
---------------------------------------------------------*/
inline BOOLEAN IsCurrentProcessSystem(){
	return peGlobalProcessSystem == PsGetCurrentProcess();
}

/*---------------------------------------------------------
函数名称:	IsCurrentProcessConfidential
函数描述:	判断当前进程是否是机密进程
输入参数:	
输出参数:	
返回值:		当前进程是否是机密进程
其他:		
更新维护:	2011.4.3     最初版本 仅测试notepad
			2011.7.25    接入机密进程表模块 仅测试
---------------------------------------------------------*/

BOOLEAN IsCurrentProcessConfidential()
{
    WCHAR wProcessNameBuffer[32] = { 0 };

	CONFIDENTIAL_PROCESS_DATA cpdCurrentProcessData = { 0 };

//    UNICODE_STRING usProcessConfidential = { 0 };

    ULONG ulLength;

    RtlInitEmptyUnicodeString(
		&cpdCurrentProcessData.usName,
		wProcessNameBuffer,
		32*sizeof(WCHAR)
		);

	ulLength = GetCurrentProcessName(&cpdCurrentProcessData.usName);
	//KdPrint(("[Antinvader]IsCurrentProcessConfidential Name:%ws\n",cpdCurrentProcessData.usName.Buffer));

	__try{
	return PctGetSpecifiedProcessDataAddress(&cpdCurrentProcessData,NULL);
	}__except(EXCEPTION_EXECUTE_HANDLER){
		ASSERT(FALSE);
	}
	//   RtlInitUnicodeString(&usProcessConfidential,L"notepad.exe");

//    if(RtlCompareUnicodeString(&usProcessName,&usProcessConfidential,TRUE) == 0)
//      return TRUE;
    return FALSE;
}

/*---------------------------------------------------------
函数名称:	GetCurrentProcessPath
函数描述:	获取当前进程路径信息

输入参数:	puniFilePath	指向有效内存的字符串指针

输出参数:	puniFilePath	包含镜像所在路径的字符串

返回值:		TRUE 如果成功找到 
			FALSE 失败

其他:		不必初始化传入时的Buffer地址
			
			原理就是去进程的PEB中把数据刨出来
			
			一定注意这个函数会申请内存用于存放路径
			记得用完了释放

更新维护:	2011.4.3     最初版本
---------------------------------------------------------*/
BOOLEAN 
GetCurrentProcessPath(
	__inout PUNICODE_STRING puniFilePath
	)
{
	//PEB结构地址
	ULONG ulPeb;

	//Parameter结构地址
	ULONG ulParameters;

	//当前进程
	PEPROCESS  peCurrentProcess;

	//找到的地址,等待拷贝
	PUNICODE_STRING puniFilePathLocated;
	
	//
    // 获得当前进程EPROCESS
	//

	peCurrentProcess = PsGetCurrentProcess();

	//
	//对不确定的内存进行访问,经常出错,结构化异常处理套上
	//
	__try
	{
		//
		//获取当前进程PEB地址
		//
		ulPeb = *(ULONG*)((ULONG)peCurrentProcess+PEB_STRUCTURE_OFFSET);

		//
		//空指针说明是内核进程,肯定没有PEB结构
		//
	    if(!ulPeb){
			return FALSE;
		}

		//
		//检测地址是否有效,无效肯定也不行
		//
		if(!MmIsAddressValid((PVOID)ulPeb)){
			return -1;
		}

		//
		//计算Parameter地址 由于不存在指针而
		//直接是将结构体本身放在了这里,故不需
		//要再次进行地址有效性检测
		//
		ulParameters = *(PULONG)((ULONG)ulPeb+PARAMETERS_STRUCTURE_OFFSET);

		//
		//计算Path地址
		//
		puniFilePathLocated = (PUNICODE_STRING)(ulParameters+IMAGE_PATH_STRUCTURE_OFFSET);

		//
		//申请内存
		//
		puniFilePath->Buffer = (PWCH)ExAllocatePoolWithTag(
			NonPagedPool,
			puniFilePathLocated->MaximumLength+2,
			MEM_PROCESS_FUNCTION_TAG
			);

		//
		//拷贝数据
		//
		RtlCopyUnicodeString(
			puniFilePath,
			puniFilePathLocated
			);
 
		return TRUE;

	}__except(EXCEPTION_EXECUTE_HANDLER){
			KdPrint(("[Antinvader]Severe error occured when getting current process path.\r\n"));
#ifdef DBG
			__asm int 3
#endif
	} 
	
	return FALSE;
}

