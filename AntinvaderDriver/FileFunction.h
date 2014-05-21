///////////////////////////////////////////////////////////////////////////////
///
/// 版权所有 (c) 2011 - 2012
///
/// 原始文件名称	 : FileFunction.h
/// 工程名称         : AntinvaderDriver
/// 创建时间         : 2011-03-26
/// 	         
///
/// 描述	         : 关于文件信息的功能声明
///
/// 更新维护:
///  0000 [2011-03-26] 最初版本.
///
///////////////////////////////////////////////////////////////////////////////

#pragma once

////////////////////////
//		宏定义
///////////////////////

/*
//机密文件判断条件类型 满足指定条件即视为机密文件
#define CONFIDENTIAL_FILE_CONDITION_NAME				0x00000001	//文件名全字匹配
#define CONFIDENTIAL_FILE_CONDITION_NAME_CONTAIN		0x00000002	//文件名包含指定字符
#define CONFIDENTIAL_FILE_CONDITION_PATH_CONTAIN		0x00000003	//路径包含指定字符
#define CONFIDENTIAL_FILE_CONDITION_BOTH_PATH_NAME		0x00000004	//绝对路径匹配
#define CONFIDENTIAL_FILE_CONDITION_FOLDER				0x00000005	//所在路径中包含该文件夹(文件夹使用绝对路径)
#define CONFIDENTIAL_FILE_CONDITION_FOLDER_CONTAIN		0x00000006	//所在路径中包含该文件夹(该文件夹只要名称确定即可)
*/

//FileIsEncrypted Flag标志				
#define FILE_IS_ENCRYPTED_DO_NOT_CHANGE_OPEN_WAY		0x00000001
#define FILE_IS_ENCRYPTED_DO_NOT_WRITE_LOGO				0x00000002

//申请内存标志
#define MEM_FILE_TAG									'file'

//加密标识 注意修该加密标识后要将ENCRYPTION_HEAD_LOGO_SIZE修改为相应的数值
#define ENCRYPTION_HEADER								{L'A',L'N',L'T',L'I',L'N',L'V',L'A',L'D',L'E',L'R',\
															L'_',L'E',L'N',L'C',L'R',L'Y',L'P',L'T',L'E',L'D'}
////////////////////////
//		全局变量
////////////////////////

//用于存放加密头内存的旁视链表
NPAGED_LOOKASIDE_LIST	nliNewFileHeaderLookasideList;

////////////////////////
//		常量定义
////////////////////////

//一般的文件路径长度 用于猜测打开进程的路径长度,如果调整的较为准确
//可以提高效率,设置的越大时间复杂度越小,空间复杂度越高
#define NORMAL_FILE_PATH_LENGTH							NORMAL_PATH_LENGTH

////////////////////////
//		结构定义
////////////////////////

//在非重入读写时判断是读还是写
typedef enum _READ_WRITE_TYPE
{
	RwRead = 1,
	RwWrite
}READ_WRITE_TYPE;

////////////////////////
//		函数申明
////////////////////////
NTSTATUS 
FileCreateByObjectNotCreated(
	__in PFLT_INSTANCE pfiInstance,
	__in PFLT_FILE_NAME_INFORMATION pfniFileNameInformation,
	__in PFLT_PARAMETERS pfpParameters,
	__in_opt ULONG ulDesiredAccess,
	__out HANDLE * phFileHandle
	);

NTSTATUS
FileSetSize(
	__in PFLT_INSTANCE pfiInstance,
	__in PFILE_OBJECT pfoFileObject,
	__in PLARGE_INTEGER pnFileSize
	);

NTSTATUS
FileGetStandardInformation(
	__in PFLT_INSTANCE pfiInstance,
	__in PFILE_OBJECT pfoFileObject,
	__inout_opt PLARGE_INTEGER pnAllocateSize,
	__inout_opt PLARGE_INTEGER pnFileSize,
	__inout_opt BOOLEAN *pbDirectory
	);

static
VOID 
FileCompleteCallback(
    __in PFLT_CALLBACK_DATA CallbackData,
    __in PFLT_CONTEXT Context
    );

NTSTATUS 
FileWriteEncryptionHeader( 
	__in PFLT_INSTANCE pfiInstance,
	__in PFILE_OBJECT  pfoFileObject,
	__in PVOLUME_CONTEXT pvcVolumeContext,
	__in PFILE_STREAM_CONTEXT  pscFileStreamContext
	);

NTSTATUS 
FileIsEncrypted(
	__in PFLT_INSTANCE pfiInstance,
	__in PFILE_OBJECT pfoFileObjectOpened,
	__in PFLT_CALLBACK_DATA pfcdCBD,
	__in PVOLUME_CONTEXT pvcVolumeContext,
	__in PFILE_STREAM_CONTEXT  pscFileStreamContext,
	__in ULONG	ulFlags 
	);

void 
FileClearCache(
	PFILE_OBJECT pFileObject
	);

NTSTATUS 
FileCreateForHeaderWriting(
	__in PFLT_INSTANCE pfiInstance,
	__in PUNICODE_STRING puniFileName,
	__out HANDLE * phFileHandle
	);

USHORT 
FileGetFilePostfixName( 
	__in PUNICODE_STRING  puniFileName,
	__inout_opt PUNICODE_STRING puniPostfixName
	);