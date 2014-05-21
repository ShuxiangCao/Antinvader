///////////////////////////////////////////////////////////////////////////////
///
/// 版权所有 (c) 2011 - 2012
///
/// 原始文件名称	 : ConfidentialFile.h
/// 工程名称         : AntinvaderDriver
/// 创建时间         : 2011-07-28
/// 	         
///
/// 描述	         : 关于文件上下文等头文件
///
/// 更新维护:
///  0001 [2011-07-28] 最初版本.废弃上一版本全部内容
///
///////////////////////////////////////////////////////////////////////////////

#pragma once


////////////////////////
//		宏定义
////////////////////////

//文件内存标志
#define MEM_TAG_FILE_TABLE	'cftm'

//文件流上下文大小
#define FILE_STREAM_CONTEXT_SIZE sizeof(_FILE_STREAM_CONTEXT)

//锁保护
#define FILE_STREAM_CONTEXT_LOCK_ON(_file_data)		KeEnterCriticalRegion();ExAcquireResourceExclusiveLite( _file_data -> prResource , TRUE );KeLeaveCriticalRegion()
#define FILE_STREAM_CONTEXT_LOCK_OFF(_file_data)		KeEnterCriticalRegion();ExReleaseResourceLite( _file_data -> prResource );KeLeaveCriticalRegion()

////////////////////////
//		常量定义
////////////////////////

//机密文件头大小 为分页方便初始设置为4k
#define	CONFIDENTIAL_FILE_HEAD_SIZE						1024*1

//加密标识长度
#define ENCRYPTION_HEAD_LOGO_SIZE						40
//sizeof(ENCRYPTION_HEADER)

////////////////////////
//		结构定义
////////////////////////

//声明当前文件缓存中是密文还是明文  机密进程还是非机密进程正在访问
typedef enum _FILE_OPEN_STATUS {
    OPEN_STATUS_FREE = 0,//可以切换,需要刷掉缓存
    OPEN_STATUS_CONFIDENTIAL,//当前是机密进程正在访问
	OPEN_STATUS_NOT_CONFIDENTIAL//当前是非机密进程正在访问
} FILE_OPEN_STATUS;	

//声明当前文件是机密文件还是非机密文件.
typedef enum _FILE_ENCRYPTED_TYPE {
    ENCRYPTED_TYPE_UNKNOWN = 0,//不知道到底如何
    ENCRYPTED_TYPE_CONFIDENTIAL,//文件是机密的
	ENCRYPTED_TYPE_NOT_CONFIDENTIAL//文件是非机密的
} FILE_ENCRYPTED_TYPE;	

//文件流上下文结构体  
typedef struct _FILE_STREAM_CONTEXT
{
//	KSPIN_LOCK kslLock;						//自旋锁

//	KIRQL irqlLockIRQL;						//取锁中断

	PERESOURCE prResource;					//取锁资源

	FILE_ENCRYPTED_TYPE fctEncrypted;		//是否被加密

	ULONG ulReferenceTimes;					//引用计数

	BOOLEAN bUpdateWhenClose;				//是否需要在关闭文件时重写加密头

	BOOLEAN bCached;						//是否缓冲

	LARGE_INTEGER nFileValidLength ;		//文件有效大小

	LARGE_INTEGER nFileSize ;				//文件实际大小 包括了加密头等

	FILE_OPEN_STATUS fosOpenStatus;			//当前缓存是明文(TRUE)还是密文(FALSE)
	
	PVOID pfcbFCB;							//缓冲FCB地址

//	PVOID pfcbNoneCachedFCB;				//非缓冲FCB地址

	UNICODE_STRING usName;					//文件名称

	UNICODE_STRING usPostFix;				//文件后缀名

//	UNICODE_STRING usPath;					//文件路径

	WCHAR wszVolumeName[64] ;				//卷名称

}FILE_STREAM_CONTEXT, * PFILE_STREAM_CONTEXT;

//加密头结构体
typedef struct _FILE_ENCRYPTION_HEAD
{
	WCHAR wEncryptionLogo[ENCRYPTION_HEAD_LOGO_SIZE];//40

	WCHAR wSeperate0[4];//8

	ULONG ulVersion;

	WCHAR wSeperate1[4];//8

	LONGLONG nFileValidLength;//8

	WCHAR wSeperate2[4];//8

	LONGLONG nFileRealSize;//8

	WCHAR wSeperate3[4];//8

	WCHAR wMD5Check[32];

	WCHAR wSeperate4[4];//8
	
	WCHAR wCRC32Check[32];

	WCHAR wSeperate5[4];//8

	WCHAR wKeyEncrypted[32];

}FILE_ENCRYPTION_HEAD,*PFILE_ENCRYPTION_HEAD;

///////////////////////
//		函数声明
///////////////////////

NTSTATUS
FctCreateContextForSpecifiedFileStream(
	__in PFLT_INSTANCE pfiInstance,
	__in PFILE_OBJECT pfoFileObject,
	__inout PFILE_STREAM_CONTEXT * dpscFileStreamContext
   );

NTSTATUS
FctUpdateStreamContextFileName(
    __in PUNICODE_STRING pusName,
    __inout PFILE_STREAM_CONTEXT  pscFileStreamContext
    );

NTSTATUS
FctFreeStreamContext(
    __inout PFILE_STREAM_CONTEXT  pscFileStreamContext
    );

NTSTATUS
FctInitializeContext(
	__inout PFILE_STREAM_CONTEXT pscFileStreamContext,
	__in PFLT_CALLBACK_DATA pfcdCBD,
	__in PFLT_FILE_NAME_INFORMATION	pfniFileNameInformation
   );

NTSTATUS
FctGetSpecifiedFileStreamContext(
	__in PFLT_INSTANCE pfiInstance,
	__in PFILE_OBJECT pfoFileObject,
	__inout PFILE_STREAM_CONTEXT * dpscFileStreamContext
   );