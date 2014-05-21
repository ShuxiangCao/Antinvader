///////////////////////////////////////////////////////////////////////////////
///
/// 版权所有 (c) 2011 - 2012
///
/// 原始文件名称	 : ProcessFunction.h
/// 工程名称         : AntinvaderDriver
/// 创建时间         : 2011-03-20
/// 	         
///
/// 描述	         : 关于进程信息的功能声明
///
/// 更新维护:
///  0000 [2011-03-20] 最初版本.
///
///////////////////////////////////////////////////////////////////////////////

#pragma once

//////////////////////
//	常量定义
//////////////////////

//EPROCESS的最大大小取为12K
#define MAX_EPROCESS_SIZE	3 * 4 * 1024	

//内存标志
#define MEM_PROCESS_FUNCTION_TAG		'pfun'

//////////////////////
//	偏移量定义
//////////////////////

//
//此处定义PEB(Process Environment Block)
//地址存放位置相对于EPROCESS起始位置偏移
//由Windbg分析得到
//nt!_EPROCESS
//...
//   +0x1b0 Peb              : Ptr32 _PEB
//...
//这里直接硬编码将PEB偏移量设置为0x1b0
//
#define PEB_STRUCTURE_OFFSET 0x1b0

//
//此处定义PEB结构中Parameter成员偏移
//由Windbg分析得
//nt!_PEB
//...
//   +0x010 ProcessParameters : Ptr32 _RTL_USER_PROCESS_PARAMETERS
//...
//这里直接硬编码将Parameter成员偏移设置为0x010
//
#define PARAMETERS_STRUCTURE_OFFSET 0x010

//
//此处定义进程镜像所在NT路径名相对偏移
//由Windbg分析得
//nt!_RTL_USER_PROCESS_PARAMETERS
//...
//   +0x038 ImagePathName    : _UNICODE_STRING
//...
//这里直接硬编码将ImagePathName偏移设置为0x038
//
#define IMAGE_PATH_STRUCTURE_OFFSET 0x038
//
//此处定义进程镜像所在SectionObject相对偏移
//由Windbg分析得
//nt!_EPROCESS
//...
//   +0x138 SectionObject    : Ptr32 Void
//...
//这里直接硬编码将SectionObject偏移设置为0x038
//
#define IMAGE_SECTION_OBJECT_STRUCTURE_OFFSET 0x038

//
//此处定义进程镜像所在Segment相对偏移
//由Windbg分析得
//nt!_SECTION_OBJECT
//...
//   +0x014 Segment          : Ptr32 _SEGMENT
//...
//这里直接硬编码将Segment偏移设置为0x014
//
#define IMAGE_SEGMENT_STRUCTURE_OFFSET 0x014

//
//此处定义进程镜像所在ControlArea相对偏移
//由Windbg分析得
//nt!_SEGMENT
//...
//   +0x000 ControlArea      : Ptr32 _CONTROL_AREA
//...
//这里直接硬编码将ControlArea偏移设置为0x000
//
#define IMAGE_CONTROL_AREA_STRUCTURE_OFFSET 0x000

//
//此处定义进程镜像所在FilePointer相对偏移
//由Windbg分析得
//nt!_CONTROL_AREA
//...
//   +0x024 FilePointer      : Ptr32 _FILE_OBJECT
//...
//这里直接硬编码将FilePointer偏移设置为0x024 
//
#define IMAGE_FILE_POINTER_STRUCTURE_OFFSET +0x024 

/////////////////////
//	变量定义
/////////////////////

//保存进程名称地址关于EPROCESS的偏移
static size_t stGlobalProcessNameOffset = 0;

//保存System进程的EPROCESS地址 用于判断
static PEPROCESS peGlobalProcessSystem = 0;

/////////////////////
//	宏定义
/////////////////////

//判断是否匹配时的返回值
#define PROCESS_NAME_NOT_CONFIDENTIAL	0x00000001
#define PROCESS_PATH_NOT_CONFIDENTIAL	0x00000002
#define PROCESS_MD5_NOT_CONFIDENTIAL	0x00000004

/////////////////////
//	函数定义
/////////////////////

void InitProcessNameOffset();

ULONG GetCurrentProcessName(
	PUNICODE_STRING usCurrentProcessName
	);