///////////////////////////////////////////////////////////////////////////////
///
/// 版权所有 (c) 2011 - 2012
///
/// 原始文件名称	 : BasicAlgorithm.h
/// 工程名称         : AntinvaderDriver
/// 创建时间         : 2011-03-20
/// 	         
///
/// 描述	         : 一些基本的算法函数声明,用于生成Hash,遍历链表等
///
/// 更新维护:
///  0000 [2011-03-20] 最初版本.
///
///////////////////////////////////////////////////////////////////////////////

#pragma once

////////////////////////
//		宏定义
////////////////////////

//Hash表内存标志
#define MEM_HASH_TAG		'hash'

//一个指针的长度
#define HASH_POINT_SIZE		sizeof(int *)

//Hash对应的地址
#define HASH_NOTE_POINT_ADDRESS(_table,_hash)	\
			(_table->ulHashTableBaseAddress+_hash * HASH_POINT_SIZE)

//锁保护
#define HASH_LOCK_ON(_table)		KeWaitForSingleObject( &_table -> irmMutex , Executive , KernelMode , FALSE ,NULL )//KeAcquireSpinLock( &_table -> kslLock , &_table -> irqlLockIRQL )
#define HASH_LOCK_OFF(_table)		KeReleaseMutex( &_table -> irmMutex , FALSE)//KeReleaseSpinLock( &_table -> kslLock , _table -> irqlLockIRQL )

////////////////////////
//	常量定义
////////////////////////

//Hash表数据
typedef struct _HASH_TABLE_DESCRIPTOR
{

	ULONG ulHashTableBaseAddress;

	ULONG ulMaximumPointNumber;
	
//	KSPIN_LOCK kslLock;//自旋锁

//	KIRQL irqlLockIRQL;	//取锁中断

	KMUTEX  irmMutex;//互斥体

}HASH_TABLE_DESCRIPTOR,*PHASH_TABLE_DESCRIPTOR;

//Hash节点数据
typedef struct _HASH_NOTE_DESCRIPTOR
{
	LIST_ENTRY lListEntry;

	ULONG ulHash;

	PVOID lpData;

}HASH_NOTE_DESCRIPTOR,*PHASH_NOTE_DESCRIPTOR;

////////////////////////
//	函数定义
////////////////////////

/*---------------------------------------------------------
函数类型:	HASH_IS_NOTE_MACHED_CALLBACK
函数描述:	回调函数,用于判断是否找到了匹配的Hash表节点

输入参数:	lpContext	 上下文,在搜索(Search)函数中指定
			lpNoteData	 当前节点的卫星数据
输出参数:	
返回值:		TRUE为完全匹配 FALSE为不匹配

其他:		该函数要求使用者填写,用于确定Hash表处理函数找
			到的节点是否完全匹配,如果直接返回TRUE则凡是满
			足Hash相同的第一个节点会被返回.

更新维护:	2011.4.4    最初版本
---------------------------------------------------------*/
typedef
BOOLEAN ( * HASH_IS_NOTE_MACHED_CALLBACK) (
	__in PVOID lpContext,
	__in PVOID lpNoteData
	);
/*---------------------------------------------------------
函数类型:	HASH_DELETE_CALLBACK
函数描述:	回调函数,释放卫星数据中内存

输入参数:	lpNoteData	 当前节点的卫星数据
输出参数:	
返回值:		

其他:		该函数要求使用者填写,在解除锁之前释放在构建
			节点内容时申请的内存等

更新维护:	2011.4.5    最初版本
---------------------------------------------------------*/
typedef
VOID ( * HASH_DELETE_CALLBACK) (
	__in PVOID lpNoteData
	);

ULONG ELFhash( 
	__in PANSI_STRING pansiKey,
	__in ULONG ulMod 
	);

BOOLEAN
HashInitialize(
	__in PHASH_TABLE_DESCRIPTOR * dpHashTable,
	__in ULONG ulMaximumPointNumber
	);

BOOLEAN
HashInsertByHash( 
	__in PHASH_TABLE_DESCRIPTOR pHashTable,
	__in ULONG ulHash,
	__in PVOID lpData,
	__in ULONG ulLength
	);

BOOLEAN
HashInsertByNumber( 
	__in PHASH_TABLE_DESCRIPTOR pHashTable,
	__in ULONG ulNumber,
	__in PVOID lpData
	);

BOOLEAN
HashInsertByUnicodeString(
	__in PHASH_TABLE_DESCRIPTOR pHashTable,
	__in PUNICODE_STRING pusString,
	__in PVOID lpData
	);

BOOLEAN
HashSearchByHash( 
	__in PHASH_TABLE_DESCRIPTOR pHashTable,
	__in ULONG ulHash,
	__in HASH_IS_NOTE_MACHED_CALLBACK CallBack,
	__in PVOID lpContext,
	__inout PHASH_NOTE_DESCRIPTOR * dpData
	);

BOOLEAN
HashSearchByNumber(
	__in PHASH_TABLE_DESCRIPTOR pHashTable,
	__in ULONG ulNumber,
	__in HASH_IS_NOTE_MACHED_CALLBACK CallBack,
	__in PVOID lpContext,
	__inout PHASH_NOTE_DESCRIPTOR * dpData
	);

BOOLEAN
HashSearchByString(
	__in PHASH_TABLE_DESCRIPTOR pHashTable,
	__in PUNICODE_STRING pusString,
	__in HASH_IS_NOTE_MACHED_CALLBACK CallBack,
	__in PVOID lpContext,
	__inout PHASH_NOTE_DESCRIPTOR * dpData
	);

BOOLEAN
HashDelete(
	__in PHASH_TABLE_DESCRIPTOR pHashTable,
	__in PHASH_NOTE_DESCRIPTOR pHashNote,
	__in_opt HASH_DELETE_CALLBACK CallBack,
	__in BOOLEAN bLock
	);

VOID
HashFree(
	__in PHASH_TABLE_DESCRIPTOR pHashTable,
	__in_opt HASH_DELETE_CALLBACK CallBack
	);


////////////////////////
//	Debug
////////////////////////
/*
#ifdef DBG

VOID DbgCheckEntireHashTable(
	__in PHASH_TABLE_DESCRIPTOR pHashTable
	);

#define DebugCheckEntireHashTable(_x) DbgCheckEntireHashTable(_x) 

#else

#define DebugCheckEntireHashTable(_x) 

#endif*/