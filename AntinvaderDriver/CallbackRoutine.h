///////////////////////////////////////////////////////////////////////////////
///
/// 版权所有 (c) 2011 - 2012
///
/// 原始文件名称	 : CallbackRoutine.h
/// 工程名称         : AntinvaderDriver
/// 创建时间         : 2011-03-20
/// 	         
///
/// 描述	         : Antinvader 回调函数声明
///
/// 更新维护:
///  0000 [2011-03-20] 最初版本.
///
///////////////////////////////////////////////////////////////////////////////

#pragma once
////////////////////////
//	   结构定义
////////////////////////

//后回调参数
typedef enum _POST_CALLBACK_FLAG
{
	//什么也不做
	DoNothing = 0,
	
	//创建时使用,增加文件到机密文件表
	CreateAddToTable,

	//补充非缓存方式打开的数据
	CreateAddNoneCached,

	//创建时组织访问
	CreateDenied

}POST_CALLBACK_FLAG,*PPOST_CALLBACK_FLAG;

//后回调上下文结构
typedef struct _POST_CALLBACK_CONTEXT
{
	//标志
	POST_CALLBACK_FLAG pcFlags;

	//数据
	PVOID pData;

}POST_CALLBACK_CONTEXT,*PPOST_CALLBACK_CONTEXT;

//卷上下文结构
typedef struct _VOLUME_CONTEXT {

    //保存卷名称
    UNICODE_STRING uniName;

    //  Holds the name of file system mounted on volume(old)
	//  Now it is useless
 //   UNICODE_STRING FsName;

    //
    //  Holds the sector size for this volume.
    //

	ULONG ulSectorSize;

	//
	//读取文件判断是否加密时使用的旁视链表
	//

	PNPAGED_LOOKASIDE_LIST pnliReadEncryptedSignLookasideList;

	// Holds encryption/decryption context
	///COUNTER_MODE_CONTEXT* aes_ctr_ctx ;

	// key. used to encrypt/decrypt files in the volume
//	UCHAR szKey[MAX_KEY_LENGTH] ;
	// key digest. used to verify whether specified file 
	// can be decrypted/encrypted by this key
//	UCHAR szKeyHash[HASH_SIZE] ;

	//spinlock to synchornize encryption/decryption process
//	KSPIN_LOCK FsCryptSpinLock ;

	// Table to hold file context defined in file.h(old)
	// Now it is useless
//	RTL_GENERIC_TABLE FsCtxTable ;
	
	// mutex to synchronize file context table(old)
	// Now it is used to synchronize encryption/decryption process
//	FAST_MUTEX FsCtxTableMutex ;

} VOLUME_CONTEXT, *PVOLUME_CONTEXT;

/*
//文件流上下文
typedef struct _STREAM_CONTEXT {

    //申请这个上下文的文件名称
    UNICODE_STRING FileName;

	//卷名称
	WCHAR wszVolumeName[64] ;

	//file key hash
//	UCHAR szKeyHash[HASH_SIZE] ;

    //Number of times we saw a create on this stream
	//used to verify whether a file flag can be written
	//into end of file and file data can be flush back.
//    LONG RefCount;

	//文件有效大小
	LARGE_INTEGER FileValidLength ;

	//文件实际大小 包括了加密头等
	LARGE_INTEGER FileSize ;

	//Trail Length
//	ULONG uTrailLength ;

	//desired access
//	ULONG uAccess ;

	//Flags
	BOOLEAN bIsFileCrypt ;    //(init false)set after file flag is written into end of file
	BOOLEAN bEncryptOnWrite ; //(init true)set when file is to be supervised, or set when file is already encrypted.
	BOOLEAN bDecryptOnRead ;  //(init false)set when non-encrypted file is first paging written, or set when file is already encrypted.
	BOOLEAN bHasWriteData ;    //(init false)If data is written into file during the life cycle of the stream context. This flag is used to judge whether to write tail flag when file is closed.
	BOOLEAN bFirstWriteNotFromBeg ; //(useless now.)if file is not encrypted yet, whether the first write offset is 0
	BOOLEAN bHasPPTWriteData ;  //(init false)If user click save button in un-encrypts ppt file, this flag is set to TRUE and this file will be encrypted in THE LAST IRP_MJ_CLOSE 

	// Holds encryption/decryption context specified to this file
	///COUNTER_MODE_CONTEXT* aes_ctr_ctx ;
   
	//Lock used to protect this context.
    PERESOURCE Resource;

	//Spin lock used to protect this context when irql is too high
	KSPIN_LOCK Resource1 ;

} STREAM_CONTEXT, *PSTREAM_CONTEXT;*/


////////////////////////
//	   宏定义
////////////////////////
#define MEM_CALLBACK_TAG				'calb'
#define CALLBACK_IS_CACHED(_piopb)		(!(_piopb -> IrpFlags&(IRP_NOCACHE)))?TRUE:FALSE

////////////////////////
//	   全局变量
////////////////////////

//后回调上下文旁视链表
NPAGED_LOOKASIDE_LIST	nliCallbackContextLookasideList;

////////////////////////
//		一般函数
////////////////////////
BOOLEAN
InitializePostCallBackContext(
	__in POST_CALLBACK_FLAG pcFlags,
	__in_opt PVOID pData,
	__inout PPOST_CALLBACK_CONTEXT * dpccContext
	);

VOID
UninitializePostCallBackContext(
	__in PPOST_CALLBACK_CONTEXT pccContext
	);

////////////////////////
//	   一般回调
////////////////////////

NTSTATUS
Antinvader_InstanceSetup (
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in FLT_INSTANCE_SETUP_FLAGS Flags,
    __in DEVICE_TYPE VolumeDeviceType,
    __in FLT_FILESYSTEM_TYPE VolumeFilesystemType
    );

NTSTATUS
Antinvader_InstanceQueryTeardown (
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    );

VOID
Antinvader_InstanceTeardownStart (
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

VOID
Antinvader_InstanceTeardownComplete (
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in FLT_INSTANCE_TEARDOWN_FLAGS Flags
    );

VOID
Antinvader_CleanupContext(
    __in PFLT_CONTEXT pcContext,
    __in FLT_CONTEXT_TYPE pctContextType
    );
////////////////////////
//	过滤回调
////////////////////////

//IRP_MJ_CREATE
FLT_PREOP_CALLBACK_STATUS
Antinvader_PreCreate (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __deref_out_opt PVOID *lpCompletionContext
    );

FLT_POSTOP_CALLBACK_STATUS
Antinvader_PostCreate (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in_opt PVOID lpCompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    );

//IRP_MJ_CLOSE
FLT_PREOP_CALLBACK_STATUS
Antinvader_PreClose (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __deref_out_opt PVOID *lpCompletionContext
    );

FLT_POSTOP_CALLBACK_STATUS
Antinvader_PostClose (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in_opt PVOID lpCompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    );

//IRP_MJ_READ
FLT_PREOP_CALLBACK_STATUS
Antinvader_PreRead (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __deref_out_opt PVOID *lpCompletionContext
    );

FLT_POSTOP_CALLBACK_STATUS
Antinvader_PostRead (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in_opt PVOID lpCompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    );

FLT_POSTOP_CALLBACK_STATUS
Antinvader_PostReadWhenSafe (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in PVOID lpContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    );

//IRP_MJ_WRITE
FLT_PREOP_CALLBACK_STATUS
Antinvader_PreWrite (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __deref_out_opt PVOID *lpCompletionContext
    );

FLT_POSTOP_CALLBACK_STATUS
Antinvader_PostWrite (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in_opt PVOID lpCompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    );

//IRP_MJ_CLEANUP
FLT_PREOP_CALLBACK_STATUS
Antinvader_PreCleanUp (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __deref_out_opt PVOID *lpCompletionContext
    );

FLT_POSTOP_CALLBACK_STATUS
Antinvader_PostCleanUp (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in_opt PVOID lpCompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    );

//IRP_MJ_SET_INFORMATION 
FLT_PREOP_CALLBACK_STATUS
Antinvader_PreSetInformation (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __deref_out_opt PVOID *lpCompletionContext
    );

FLT_POSTOP_CALLBACK_STATUS
Antinvader_PostSetInformation (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in_opt PVOID lpCompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    );

//IRP_MJ_DIRECTORY_CONTROL 
FLT_PREOP_CALLBACK_STATUS
Antinvader_PreDirectoryControl (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __deref_out_opt PVOID *lpCompletionContext
    );

FLT_POSTOP_CALLBACK_STATUS
Antinvader_PostDirectoryControl (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in_opt PVOID lpCompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    );

FLT_POSTOP_CALLBACK_STATUS
Antinvader_PostDirectoryControlWhenSafe (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in_opt PVOID lpCompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    );

//IRP_MJ_QUERY_INFORMATION
FLT_PREOP_CALLBACK_STATUS
Antinvader_PreQueryInformation (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __deref_out_opt PVOID *lpCompletionContext
    );

FLT_POSTOP_CALLBACK_STATUS
Antinvader_PostQueryInformation (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in_opt PVOID lpCompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    );

/////////////////////////
//		通讯回调
/////////////////////////
NTSTATUS
Antinvader_Connect(
    __in PFLT_PORT ClientPort,
    __in PVOID ServerPortCookie,
    __in_bcount(SizeOfContext) PVOID ConnectionContext,
    __in ULONG SizeOfContext,
    __deref_out_opt PVOID *ConnectionCookie
    );

VOID
Antinvader_Disconnect(
    __in_opt PVOID ConnectionCookie
   );


NTSTATUS
Antinvader_Message (
    __in PVOID ConnectionCookie,
    __in_bcount_opt(InputBufferSize) PVOID InputBuffer,
    __in ULONG InputBufferSize,
    __out_bcount_part_opt(OutputBufferSize,*ReturnOutputBufferLength) PVOID OutputBuffer,
    __in ULONG OutputBufferSize,
    __out PULONG ReturnOutputBufferLength
    );