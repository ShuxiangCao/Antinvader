///////////////////////////////////////////////////////////////////////////////
///
/// 版权所有 (c) 2011 - 2012
///
/// 原始文件名称	 : ConfidentialProcess.h
/// 工程名称         : AntinvaderDriver
/// 创建时间         : 2011-03-20
/// 	         
///
/// 描述	         : 机密进程数据维护头文件 包括:
///					   机密进程Hash表
///					   机密进程相关数据
///
/// 更新维护:
///  0000 [2011-03-23] 最初版本.
///
///////////////////////////////////////////////////////////////////////////////

#pragma once

////////////////////////
//		常量定义
////////////////////////

//机密进程Hash表能储存的指针数 暂定为256 
#define CONFIDENTIAL_PROCESS_TABLE_POINT_NUMBER	256

//指针大小
#define CONFIDENTIAL_PROCESS_POINT_SIZE	sizeof(int *)

//机密进程Hash表实际大小暂定为256个指针大小 方便32位64位机兼容
#define CONFIDENTIAL_PROCESS_TABLE_SIZE CONFIDENTIAL_PROCESS_TABLE_POINT_NUMBER * \
											sizeof(int *)	

//关于PctIsProcessDataAccordance中Flags和返回值取值 需要比较用或运算
#define CONFIDENTIAL_PROCESS_COMPARISON_MATCHED 0X00000000	//全部相等
#define CONFIDENTIAL_PROCESS_COMPARISON_NAME	0x00000001	//比较名称(名称不同)
#define CONFIDENTIAL_PROCESS_COMPARISON_PATH	0x00000002	//比较路径(路径不同)
#define CONFIDENTIAL_PROCESS_COMPARISON_MD5		0x00000004	//比较Md5摘要(Md5摘要不同)


////////////////////////
//		变量定义
////////////////////////

//机密进程Hash表
PHASH_TABLE_DESCRIPTOR phtProcessHashTableDescriptor;

//进程记录旁视链表
NPAGED_LOOKASIDE_LIST  nliProcessContextLookasideList;

////////////////////////
//		宏定义
////////////////////////

//内存标志 分配内存时使用
#define MEM_TAG_PROCESS_TABLE	'cptd'

/*
//断言地址一定为Hash表中内容 暂时不启用
#define ASSERT_HASH_TABLE_ADDRESS(addr)				ASSERT(	(addr>=ulGlobalProcessDataTableAddress)&&	\
																(addr < CONFIDENTIAL_PROCESS_TABLE_SIZE + \
																ulGlobalProcessDataTableAddress))*/
////////////////////////
//		结构定义
////////////////////////

//机密进程信息结构体

typedef struct _CONFIDENTIAL_PROCESS_DATA
{
	UNICODE_STRING usName;						//进程名称

	UNICODE_STRING usPath;						//进程路径
	
	UNICODE_STRING usMd5Digest;					//进程MD5校验值

}CONFIDENTIAL_PROCESS_DATA, * PCONFIDENTIAL_PROCESS_DATA;

/////////////////////////
//		函数声明
/////////////////////////

BOOLEAN	 PctInitializeHashTable();

ULONG PctGetProcessHash(
	__in PCONFIDENTIAL_PROCESS_DATA ppdProcessData 
	);

BOOLEAN PctConvertProcessDataToStaticAddress(
	__in PCONFIDENTIAL_PROCESS_DATA ppdProcessData,
	__inout  PCONFIDENTIAL_PROCESS_DATA * dppdNewProcessData
	);

BOOLEAN PctAddProcess( 
	__in PCONFIDENTIAL_PROCESS_DATA ppdProcessData 
	);

ULONG  PctIsProcessDataAccordance(
	__in PCONFIDENTIAL_PROCESS_DATA ppdProcessDataOne ,
	__in PCONFIDENTIAL_PROCESS_DATA ppdProcessDataAnother,
	__in ULONG ulFlags
	);

BOOLEAN PctGetSpecifiedProcessDataAddress(
	__in  PCONFIDENTIAL_PROCESS_DATA ppdProcessDataSource ,
	__inout PCONFIDENTIAL_PROCESS_DATA * dppdProcessDataInTable 
	);
/*
VOID PctUpdateProcessMd5(
	__in  PCONFIDENTIAL_PROCESS_DATA ppdProcessDataInTable ,
	__in  PCONFIDENTIAL_PROCESS_DATA ppdProcessDataSource
	);
*/
VOID PctFreeProcessDataStatic(
	__in  PCONFIDENTIAL_PROCESS_DATA ppdProcessData ,
	__in  BOOLEAN bFreeDataBase 
	);

BOOLEAN PctFreeTable();

BOOLEAN PctDeleteProcess(
	__in  PCONFIDENTIAL_PROCESS_DATA ppdProcessData 
	);

static
BOOLEAN PctIsDataMachedCallback(
	__in PVOID lpContext,
	__in PVOID lpNoteData
	);
static
VOID 
PctFreeHashMemoryCallback (
	__in PVOID lpNoteData
	);