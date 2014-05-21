///////////////////////////////////////////////////////////////////////////////
///
/// 版权所有 (c) 2011 - 2012
///
/// 原始文件名称	 : AntinvaderDriver.h
/// 工程名称         : AntinvaderDriver
/// 创建时间         : 2011-03-20
/// 	         
///
/// 描述	         : Antinvader驱动程序主要头文件,包括函数和静态类型定义
///
/// 更新维护:
///  0000 [2011-03-20] 最初版本.
///
///////////////////////////////////////////////////////////////////////////////

// $Id$

#ifndef __ANTINVADERDRIVER_H_VERSION__
#define __ANTINVADERDRIVER_H_VERSION__ 100

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif


#include "drvCommon.h"
#include "drvVersion.h"

//命令约定
#include "..\Common.h"

// COMMUNICATE_PORT_NAME	 \\AntinvaderPort	管道名称 
PRESET_UNICODE_STRING(usCommunicatePortName, COMMUNICATE_PORT_NAME);

#ifndef FILE_DEVICE_ANTINVADERDRIVER
#define FILE_DEVICE_ANTINVADERDRIVER 0x8000
#endif



//调试支持
//#if DBG
		
#define FILE_OBJECT_NAME_BUFFER(_file_object)	(_file_object)->FileName.Buffer
#define CURRENT_PROCESS_NAME_BUFFER ((PCHAR)PsGetCurrentProcess() + stGlobalProcessNameOffset)

//追踪方式
#define DEBUG_TRACE_ERROR                           0x00000001  // Errors - whenever we return a failure code
#define DEBUG_TRACE_LOAD_UNLOAD                     0x00000002  // Loading/unloading of the filter
#define DEBUG_TRACE_INSTANCES                       0x00000004  // Attach / detatch of instances
#define DEBUG_TRACE_DATA_OPERATIONS		            0x00000008  // Operation to access / modify in memory metadata
#define DEBUG_TRACE_ALL_IO                          0x00000010  // All IO operations tracked by this filter
#define DEBUG_TRACE_NORMAL_INFO                     0x00000020  // Misc. information
#define DEBUG_TRACE_IMPORTANT_INFO                  0x00000040  // Important information
#define DEBUG_TRACE_CONFIDENTIAL					0x00000080
#define DEBUG_TRACE_TEMPORARY						0x00000100
#define DEBUG_TRACE_ALL                             0xFFFFFFFF  // All flags

//当前方式
#define DEBUG_TRACE_NOW								DEBUG_TRACE_TEMPORARY|DEBUG_TRACE_ERROR|DEBUG_TRACE_CONFIDENTIAL//DEBUG_TRACE_ALL

#define DebugTrace(_Level,_ProcedureName, _Data)	\
    if ((_Level) & (DEBUG_TRACE_NOW)) {               \
		DbgPrint("[Antinvader:"_ProcedureName"]\n\t\t\t");					\
		DbgPrint _Data;                              \
		DbgPrint("\n");								\
    }

#define DebugTraceFile(_Level,_ProcedureName,_FileName, _Data)		\
	if ((_Level) & (DEBUG_TRACE_NOW)) {								\
		DbgPrint("[Antinvader:"_ProcedureName"]%s\n\t\t\t",_FileName);	\
		DbgPrint _Data;												\
		DbgPrint("\n");												\
    }

#define DebugTraceFileAndProcess(_Level,_ProcedureName,_FileName, _Data)	\
    if ((_Level) & (DEBUG_TRACE_NOW)) {										\
		DbgPrint("[Antinvader:"_ProcedureName"]%ws:%s\n\t\t\t",_FileName,CURRENT_PROCESS_NAME_BUFFER); \
		DbgPrint _Data;														\
		DbgPrint("\n");														\
    }

		//
		//测试
		//
/*		KdPrint(("DebugTestRead:!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"	
				 "\t\tIRP_BUFFERED_IO: 			%d\n"	
				 "\t\tIRP_CLOSE_OPERATION:		%d\n"		
				 "\t\tIRP_DEALLOCATE_BUFFER:	%d\n" 		
				 "\t\tIRP_INPUT_OPERATION:		%d\n"		
				 "\t\tIRP_NOCACHE: 				%d\n"	
				 "\t\tIRP_PAGING_IO: 			%d\n"	
				 "\t\tIRP_SYNCHRONOUS_API:		%d\n" 		
				 "\t\tIRP_SYNCHRONOUS_PAGING_IO:%d\n"
				 "\t\tLength:                   %d\n"
				 "\t\tByteOffset                %d\n",	
					pIoParameterBlock -> IrpFlags&(IRP_BUFFERED_IO),
					pIoParameterBlock -> IrpFlags&(IRP_CLOSE_OPERATION),	
					pIoParameterBlock -> IrpFlags&(IRP_DEALLOCATE_BUFFER),	
					pIoParameterBlock -> IrpFlags&(IRP_INPUT_OPERATION),	
					pIoParameterBlock -> IrpFlags&(IRP_NOCACHE),
					pIoParameterBlock -> IrpFlags&(IRP_PAGING_IO),
					pIoParameterBlock -> IrpFlags&(IRP_SYNCHRONOUS_API),	
					pIoParameterBlock -> IrpFlags&(IRP_SYNCHRONOUS_PAGING_IO),
					pIoParameterBlock->Parameters.Read.Length,
					pIoParameterBlock->Parameters.Read.ByteOffset.QuadPart));*/

//其他宏


#define DebugPrintFileObject(_header,_object,_cached)	KdPrint(("[Antinvader]%s.\n"		\
															"\t\tName:%ws\n"				\
															"\t\tCached %d\n"				\
															"\t\tFcb:0x%X\n",				\
															_header,						\
															_object->FileName.Buffer,		\
															_cached,						\
															_object->FsContext				\
															));								

#define DebugPrintFileStreamContext(_header,_object)		KdPrint(("[Antinvader]%s.\n"			\
															"\t\tName:%ws\n"				\
															"\t\tCached %d\n"				\
															"\t\tCachedFcb:0x%X\n"			\
															"\t\tNoneCachedFcb0x%X\n",		\
															_header,						\
															_object->usName.Buffer,			\
															_object->bCached,				\
															_object->pfcbCachedFCB,			\
															_object->pfcbNoneCachedFCB		\
															));										

//#else

//#define DebugTrace(_Level,_ProcedureName, _Data)             {NOTHING;}
//#define DebugPrintFileObject(_header,_object,_cached)  {NOTHING;}
//#define DebugPrintFileStreamContext(_header,_object)	{NOTHING;}

//#endif

// Values defined for "Method"
// METHOD_BUFFERED
// METHOD_IN_DIRECT
// METHOD_OUT_DIRECT
// METHOD_NEITHER
// 
// Values defined for "Access"
// FILE_ANY_ACCESS
// FILE_READ_ACCESS
// FILE_WRITE_ACCESS

const ULONG IOCTL_ANTINVADERDRIVER_OPERATION = CTL_CODE(FILE_DEVICE_ANTINVADERDRIVER, 0x01, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA);

///////////////////////////
//	函数声明头文件引用
//////////////////////////

//基本算法
#include "BasicAlgorithm.h"

//过滤请求回调
#include "CallbackRoutine.h"

//进程信息处理
#include "ProcessFunction.h"

//机密文件数据
#include "ConfidentialFile.h"

//机密文件数据
#include "FileFunction.h"

//机密进程数据
#include "ConfidentialProcess.h"

//微过滤驱动通用函数
#include "MiniFilterFunction.h"

//挂钩
#include "SystemHook.h"

//驱动卸载
NTSTATUS
Antinvader_Unload (
    __in FLT_FILTER_UNLOAD_FLAGS Flags
    );

//包含了需要过滤的IRP请求
const FLT_OPERATION_REGISTRATION Callbacks[] = {
	//创建
	{ IRP_MJ_CREATE,0,Antinvader_PreCreate,Antinvader_PostCreate },
	//关闭
	{ IRP_MJ_CLOSE,0,Antinvader_PreClose,Antinvader_PostClose},
	//清理
	{ IRP_MJ_CLEANUP,0,Antinvader_PreCleanUp,Antinvader_PostCleanUp},
	//读文件
	{ IRP_MJ_READ,0,Antinvader_PreRead,Antinvader_PostRead},
	//写文件 跳过缓存
	{ IRP_MJ_WRITE,0,Antinvader_PreWrite,Antinvader_PostWrite},
	//设置文件信息,用来保护加密头
	{ IRP_MJ_SET_INFORMATION,0,Antinvader_PreSetInformation,Antinvader_PostSetInformation},
	//目录
	{ IRP_MJ_DIRECTORY_CONTROL,0,Antinvader_PreDirectoryControl,Antinvader_PostDirectoryControl},
	//读取文件信息
	{ IRP_MJ_QUERY_INFORMATION,0,Antinvader_PreQueryInformation,Antinvader_PostQueryInformation},

    { IRP_MJ_OPERATION_END }
};

CONST FLT_CONTEXT_REGISTRATION Contexts[] = {

	{ FLT_VOLUME_CONTEXT, 0, Antinvader_CleanupContext, sizeof(VOLUME_CONTEXT), MEM_CALLBACK_TAG },

    { FLT_STREAM_CONTEXT, 0, Antinvader_CleanupContext, FILE_STREAM_CONTEXT_SIZE, MEM_TAG_FILE_TABLE },

	{ FLT_CONTEXT_END }
};


const FLT_REGISTRATION FilterRegistration = {//驱动注册对象

    sizeof( FLT_REGISTRATION ),         //  大小
    FLT_REGISTRATION_VERSION,           //  版本
    0,                                  //  标志

    Contexts,                               //  上下文
    Callbacks,                          //  操作回调设置 类型FLT_OPERATION_REGISTRATION

    Antinvader_Unload,                  //  卸载调用

    Antinvader_InstanceSetup,           //  启动过滤实例
    NULL,//Antinvader_InstanceQueryTeardown,   //  销毁查询实例
    NULL,//Antinvader_InstanceTeardownStart,   //  开始实例销毁回调
    NULL,//Antinvader_InstanceTeardownComplete,//  完成实例销毁回调

    NULL,                               //  生存文件名称回调 (未使用 NULL)
    NULL,                               //  生成目标文件名称回调(未使用 NULL)
    NULL                                //  标准化过滤器名称缓存(未使用 NULL)
};


#endif // __ANTINVADERDRIVER_H_VERSION__
