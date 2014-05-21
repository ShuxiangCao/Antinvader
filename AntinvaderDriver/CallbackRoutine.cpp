///////////////////////////////////////////////////////////////////////////////
///
/// 版权所有 (c) 2011 - 2012
///
/// 原始文件名称	 : CallbackRoutine.cpp
/// 工程名称         : AntinvaderDriver
/// 创建时间         : 2011-03-20
/// 	         
///
/// 描述	         : Antinvader 回调实现
///
/// 更新维护:
///  0000 [2011-03-20] 最初版本.
///
///////////////////////////////////////////////////////////////////////////////

#include "CallbackRoutine.h"

////////////////////////////////////
//    一般函数
////////////////////////////////////
/*---------------------------------------------------------
函数名称:	InitializePostCallBackContext
函数描述:	初始化回调上下文
输入参数:
			pcFlags			回调标志,详见
							POST_CALLBACK_FLAG定义
			pData			需要的数据,可为NULL
			dpccContext		保存初始化好的上下文地址的空间
输出参数:	
			dpccContext		初始化好的上下文
返回值:	
			TRUE 成功 否则为FALSE
其他:		
			后回调中需要使用UninitializePostCallBackContext
			释放这里申请的上下文空间.

更新维护:	2011.5.1    最初版本
---------------------------------------------------------*/
BOOLEAN
InitializePostCallBackContext(
	__in POST_CALLBACK_FLAG pcFlags,
	__in_opt PVOID pData,
	__inout PPOST_CALLBACK_CONTEXT * dpccContext
	)
{
	//分配的上下文地址空间
	PPOST_CALLBACK_CONTEXT pccContext;

	//
	//分配空间
	//
	pccContext = (PPOST_CALLBACK_CONTEXT)ExAllocatePoolWithTag(
		NonPagedPool,
		sizeof(POST_CALLBACK_CONTEXT),
		MEM_CALLBACK_TAG
		);
	
	//
	//分配空间失败
	//
	if( !pccContext ){
		return FALSE;
	}

	//
	//填充数据
	//
	pccContext -> pcFlags = pcFlags;
	pccContext -> pData = pData;

	//
	//返回
	//
	*dpccContext = pccContext;

	return TRUE;
}

/*---------------------------------------------------------
函数名称:	UninitializePostCallBackContext
函数描述:	初始化回调上下文
输入参数:	dpccContext		初始化好的上下文地址
输出参数:	
返回值:	
其他:		配合InitializePostCallBackContext使用
更新维护:	2011.5.1    最初版本
---------------------------------------------------------*/
VOID
UninitializePostCallBackContext(
	__in PPOST_CALLBACK_CONTEXT pccContext
	)
{
	ExFreePoolWithTag( pccContext , MEM_CALLBACK_TAG );
}

////////////////////////////////////
//    过滤回调例程
////////////////////////////////////

/*---------------------------------------------------------
函数名称:	Antinvader_PreCreate
函数描述:	预请求回调IRP_MJ_CREATE
输入参数:
			pfcdCBD				回调数据
			pFltObjects			文件对象
			lpCompletionContext	完成请求的上下文
输出参数:
返回值:		
			FLT_PREOP_SUCCESS_WITH_CALLBACK 使用后回调
			FLT_PREOP_SUCCESS_NO_CALLBACK   无后回调
其他:		
更新维护:	2011.3.20    最初版本
			2011.3.23    关闭FastIo
			2011.4.9     增加了机密进程判断,启用机密
							   文件表
			2011.4.13    增加了创建新缓冲,判断加密表
							   中文件是否存在
		    2011.4.30    Bug:无文件名时将阻止文件打开,
							       导致出错,已修正.
		    2011.5.1     修改了回调上下文,优化了执行
							   顺序.
			2011.7.8     Bug:FileIsEncrypted中修改了
								   FLT_PARAMETER却没有通知
								   FLTMGR.已修正.
			2011.7.10   再次关闭FastIo
			2011.7.13   为了兼容新打开的文件FCB情况
							  将加入文件加密表时机推后到Post
			2011.7.20   修改了FileIsEncrypted配套
			2011.7.27   添加了上下文支持 
			2011.7.29   修改了创建上下文位置
---------------------------------------------------------*/
FLT_PREOP_CALLBACK_STATUS 
Antinvader_PreCreate (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __deref_out_opt PVOID *lpCompletionContext
    )
{

	//
	//没有文件对象
	//
	if(!pFltObjects->FileObject){

		DebugTrace(DEBUG_TRACE_NORMAL_INFO,"PreCreate",
			("No file object was found.pass now."));

		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}
	
	//
	//如果只是打开目录 直接放过
	//
	if( pfcdCBD -> Iopb ->Parameters.Create.Options & 
		FILE_DIRECTORY_FILE ){

		DebugTraceFileAndProcess(
			DEBUG_TRACE_NORMAL_INFO,
			"PreCreate",
			FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
			("Just open directory.Pass now.")
			);

		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

/*---------------------------------------------------------
函数名称:	Antinvader_PostCreate
函数描述:	后请求回调IRP_MJ_CREATE
输入参数:
			pfcdCBD				回调数据
			pFltObjects			文件对象
			lpCompletionContext	完成请求的上下文
			Flags				回调发生的标志(原因)
输出参数:
返回值:		
			FLT_POSTOP_FINISHED_PROCESSING 成功
其他:		
更新维护:	2011.3.20    最初版本
			2011.5.1     修改了上下文结构,增加了添加
							   到机密文件表的功能.
			2011.7.13    为了兼容新打开的文件FCB情况
							   将加入文件加密表时机推后到这里
			2011.7.17    增加了缓存释放
			2011.7.29    大幅度修改 将大部分工作转移到
							   这里
		    2012.1.2     增加了释放缓存操作
			2012.1.3     取消了释放缓存,禁止机密和非机密
			                   进程同时打开机密文件
			2012.1.3     Bug:修改后导致重命名等问题困难.
								   已修正.
		    2012.1.3     Bug:无法阻止同时打开相同文件..修正ing
			2012.1.4     放弃阻止打开相同文件,专注刷缓存
---------------------------------------------------------*/
FLT_POSTOP_CALLBACK_STATUS
Antinvader_PostCreate (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in_opt PVOID lpCompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    )
{
	//Io操作参数块
	PFLT_IO_PARAMETER_BLOCK pIoParameterBlock;

	//返回值
	NTSTATUS status;

	//文件对象
	PFILE_OBJECT pfoFileObject;

	//实例
	PFLT_INSTANCE pfiInstance;

	//是否是机密文件
	BOOLEAN bIsEncrypted;

	//加密头补齐
//	BOOLEAN bHeaderAuto = TRUE;
	
	//新的流文件对象
//	PFILE_OBJECT pfoStreamFileObject;

	//打开的文件对象
	PFILE_OBJECT pfoFileObjectOpened = NULL;

	//文件句柄
	HANDLE hFile = NULL;

	//渴望的权限
	ULONG ulDesiredAccess;
	
	//获取到的文件对象
	PFILE_STREAM_CONTEXT pscFileStreamContext = NULL;

	//当前是否是缓存方式
	BOOLEAN bCached;

	//是否清除缓存
	BOOLEAN bClearCache = FALSE;

	//卷上下文
	PVOLUME_CONTEXT pvcVolumeContext = 0;

	//文件名称信息
	PFLT_FILE_NAME_INFORMATION pfniFileNameInformation = 0;

	//
	//确保IRQL<=APC_LEVEL
	//
	PAGED_CODE();

	//
	//获取需要的参数
	//
	pIoParameterBlock = pfcdCBD -> Iopb;
	pfiInstance = pFltObjects->Instance;
	pfoFileObject = pFltObjects -> FileObject;

	FltDecodeParameters(
		pfcdCBD,
		NULL,
		NULL,
		NULL,
		(LOCK_OPERATION *)&ulDesiredAccess
		);

	DebugTraceFileAndProcess(
		DEBUG_TRACE_ALL_IO,
		"PostCreate",
		FILE_OBJECT_NAME_BUFFER(pfoFileObject),
		("PostCreate entered.FltIsOperationSynchronous %d",FltIsOperationSynchronous(pfcdCBD))
		);

//	if(!IsCurrentProcessConfidential()){
//		return FLT_POSTOP_FINISHED_PROCESSING;
//	}

	do{
		
		if(!NT_SUCCESS(pfcdCBD->IoStatus.Status)){
			//
			//如果打开失败了 那么不用管了
			//
			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO,
				"PostCreate",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("File failed to open , pass now.")
				);

			break;
		}

		//
		//获取卷上下文
		//
		status = FltGetVolumeContext(
			pFltObjects->Filter,
			pFltObjects->Volume,
			(PFLT_CONTEXT *)&pvcVolumeContext
			);

		//
		//没有能够获取卷上下文 返回
		//
		if( !NT_SUCCESS(status)|| pvcVolumeContext == NULL ){
			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO,
				"PostCreate",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("No volume context was found, pass now.")
				);

			break;
		}

		//
		//获取文件名称信息
		//
		status = FltGetFileNameInformation(
			pfcdCBD, 
			//FLT_FILE_NAME_NORMALIZED|FLT_FILE_NAME_QUERY_DEFAULT, 
			FLT_FILE_NAME_OPENED|FLT_FILE_NAME_QUERY_DEFAULT,
			&pfniFileNameInformation
			);
		
		if( !NT_SUCCESS(status)){
			//
			//没拿到文件信息
			//
			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO,
				"PostCreate",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("Cannot get file information, pass now.")
				);

			pfniFileNameInformation = NULL;
			break;
		}

		if(!pfniFileNameInformation->Name.Length){
			//
			//文件名长度为0,返回 并释放pfniFileNameInformation
			//
			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO,
				"PostCreate",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("Zero name length, pass now.")
				);

			break;
		}

		//
		//比较卷名称和打开名称如果相同说明是在打开卷 就不过滤了
		//
		if (RtlCompareUnicodeString(
				&pfniFileNameInformation->Name,
				&pfniFileNameInformation->Volume,
				TRUE
				) == 0){

			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO,
				"PostCreate",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("Openning volume, pass now.")
				);

			break;
		}

		//
		//创建文件流上下文 如果已经存在 引用计数将自动加1
		//
		status = FctCreateContextForSpecifiedFileStream(
			pfiInstance,
			pfoFileObject,
			&pscFileStreamContext
			);

		if(status != STATUS_FLT_CONTEXT_ALREADY_DEFINED){
			if(status == STATUS_NOT_SUPPORTED){

			DebugTraceFileAndProcess(
				DEBUG_TRACE_ERROR,
				"PostCreate",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("Error:File does not supported.")
				);

				ASSERT(FALSE);

				break;
			}

			//
			//如果没有上下文 那么新初始化上下文
			//
			status = FctInitializeContext(
				pscFileStreamContext,
				pfcdCBD,
				pfniFileNameInformation
				);

			if (!NT_SUCCESS(status)){

				DebugTraceFileAndProcess(
					DEBUG_TRACE_ERROR,
					"PostCreate",
					FILE_OBJECT_NAME_BUFFER(pfoFileObject),
					("Error:Cannot initialize fct context.")
					);

				break ;
			}

			//
			//如果不是需要监控的文件,设置为不需要监控
			//
	/*		if(!PctIsPostfixMonitored(&pscFileStreamContext->usPostFix)){
				DebugTraceFileAndProcess(
					DEBUG_TRACE_NORMAL_INFO,
					"PostCreate",
					FILE_OBJECT_NAME_BUFFER(pfoFileObject),
					("Current file postfix not monitored.PostFix %ws.Length %d",pscFileStreamContext->usPostFix.Buffer,pscFileStreamContext->usPostFix.Length)
					);
				FctUpdateFileConfidentialCondition(
					pscFileStreamContext,ENCRYPTED_TYPE_NOT_CONFIDENTIAL);
				break;
			}*/

		}//if(status != STATUS_FLT_CONTEXT_ALREADY_DEFINED)

		
		//
		//现在都有上下文了,如果已经设定是机密文件 那么释放缓存 退出
		//
		if( FctGetFileConfidentialCondition(pscFileStreamContext) == ENCRYPTED_TYPE_CONFIDENTIAL){

			DebugTraceFileAndProcess(
				DEBUG_TRACE_IMPORTANT_INFO|DEBUG_TRACE_CONFIDENTIAL,
				"PostCreate",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("File encripted.Now clear file cache.FCB:0x%X",pfoFileObject->FsContext)
				);

			FileClearCache(pfoFileObject);
			break;
		}

		//
		//不知道是不是新的机密文件(没有机密进程打开过),而且是非机密进程访问,那么直接放过
		//
		if((FctGetFileConfidentialCondition(pscFileStreamContext) == ENCRYPTED_TYPE_NOT_CONFIDENTIAL)&&
		(!IsCurrentProcessConfidential())){

			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO,
				"PostCreate",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("None confiditial process create file not encripted.Pass now.")
				);
			break;

		//}else if(pscFileStreamContext->fctEncrypted != ENCRYPTED_TYPE_CONFIDENTIAL){
		}else{
			//
			//现在的情况是 一定不是之前打开过的机密文件 一定不是非机密文件和非机密进程的组合
			//可能是机密进程 - 非机密文件/新打开的文件 或 非机密进程 - 新打开的文件
			//
			//如果不知道到底是不是机密文件 那么打开文件看一看 现在一定是机密进程
			//此处可能的情况是,之前某个程序复制了一个机密文件,上下文还存在
			//当时由于是非机密进程所以设置了ENCRYPTED_TYPE_NOT_CONFIDENTIAL
			//现在重新检查.或者是ENCRYPTED_TYPE_UNKNOWN 本来就应该检查
			//

			status = FileIsEncrypted(
				pfiInstance,
				pfoFileObject,
				pfcdCBD,
				pvcVolumeContext,
				pscFileStreamContext,
				NULL
				);

			if(status == STATUS_FILE_NOT_ENCRYPTED){

				DebugTraceFileAndProcess(
					DEBUG_TRACE_NORMAL_INFO,
					"PostCreate",
					FILE_OBJECT_NAME_BUFFER(pfoFileObject),
					("File tested and not encripted.Set now.")
					);

				FctUpdateFileConfidentialCondition(
					pscFileStreamContext,ENCRYPTED_TYPE_NOT_CONFIDENTIAL);

				break;
			}

			//
			//新文件,如果自动补齐过加密头说明也修改过Parameter,使用Dirty
			//

			if(status == STATUS_REPARSE_OBJECT){

				DebugTraceFileAndProcess(
					DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
					"PostCreate",
					FILE_OBJECT_NAME_BUFFER(pfoFileObject),
					("New file.Head has been wrriten.Set drity now.FCB:0x%X",pfoFileObject->FsContext)
					);

				FltSetCallbackDataDirty(pfcdCBD);
				status = STATUS_SUCCESS;

				FctUpdateFileConfidentialCondition(
					pscFileStreamContext,ENCRYPTED_TYPE_CONFIDENTIAL);

				break;

			}else if(!NT_SUCCESS(status)){

				DebugTraceFileAndProcess(
					DEBUG_TRACE_IMPORTANT_INFO,
					"PostCreate",
					FILE_OBJECT_NAME_BUFFER(pfoFileObject),
					("File cannot be tested.Process access denied.")
					);

				//
				//失败了,阻止访问
				//
				FltCancelFileOpen(
					pFltObjects->Instance,
					pfoFileObject);

				//
				//设置返回信息
				//
				pfcdCBD->IoStatus.Status = STATUS_ACCESS_DENIED;
				pfcdCBD->IoStatus.Information = 0;

				FctUpdateFileConfidentialCondition(
					pscFileStreamContext,ENCRYPTED_TYPE_NOT_CONFIDENTIAL);

				ASSERT(FALSE/*Access shouldn't be denied.*/);
				break;
			}

			//
			//如果不能成功读取并解包加密头 认为是非机密文件
			//
			if(!NT_SUCCESS(FileReadEncryptionHeaderAndDeconstruct(
				pfiInstance,
				pfoFileObject,
				pvcVolumeContext,
				pscFileStreamContext))){
				DebugTraceFileAndProcess(
					DEBUG_TRACE_ERROR|DEBUG_TRACE_CONFIDENTIAL, 
					"PostCreate",
					FILE_OBJECT_NAME_BUFFER(pfoFileObject),
					("Error:Cannot read entire file encryption header.")
					);

				FctUpdateFileConfidentialCondition(
					pscFileStreamContext,ENCRYPTED_TYPE_NOT_CONFIDENTIAL);

				break;
			}else{
				DebugTraceFileAndProcess(
					DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL, 
					"PostCreate",
					FILE_OBJECT_NAME_BUFFER(pfoFileObject),
					("Confidential file head read.Valid length %d",pscFileStreamContext->nFileValidLength.QuadPart)
					);

				
				FctUpdateFileConfidentialCondition(
					pscFileStreamContext,ENCRYPTED_TYPE_CONFIDENTIAL);

				FileClearCache(pfoFileObject);

				break;
			}
		}
		
	}while(0);

	//
	//善后工作
	//

	if (NULL != pscFileStreamContext){
		FctReleaseStreamContext(pscFileStreamContext) ;
	}

	if( pfoFileObjectOpened ){
		ObDereferenceObject( pfoFileObjectOpened );
	}

	if( hFile != NULL ){
		FltClose( hFile );
	}

	if(pfniFileNameInformation){
		FltReleaseFileNameInformation(pfniFileNameInformation);
	}

	if(pvcVolumeContext){
		FltReleaseContext(pvcVolumeContext);
	}

	if( pfoFileObjectOpened ){
		ObDereferenceObject( pfoFileObjectOpened );
	}

	DebugTraceFileAndProcess(
		DEBUG_TRACE_NORMAL_INFO,
		"PostCreate",
		FILE_OBJECT_NAME_BUFFER(pfoFileObject),
		("All finished.ioStatus 0x%X",pfcdCBD->IoStatus.Status)
		);

	return FLT_POSTOP_FINISHED_PROCESSING;
}

/*---------------------------------------------------------
函数名称:	Antinvader_PreClose
函数描述:	预请求回调IRP_MJ_CLOSE
输入参数:
			pfcdCBD				回调数据
			pFltObjects			文件对象
			lpCompletionContext	完成请求的上下文
输出参数:
返回值:		
其他:		
更新维护:	2011.3.20    最初版本
			2011.7.10    添加取消引用计数
			2011.7.12    修改了Pre和Post的功能把取消
							   引用计数放到了Post里
---------------------------------------------------------*/
FLT_PREOP_CALLBACK_STATUS
Antinvader_PreClose (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __deref_out_opt PVOID *lpCompletionContext
    )
{
	//Io操作参数块
	PFLT_IO_PARAMETER_BLOCK pIoParameterBlock;

	//实例
	PFLT_INSTANCE pfiInstance;

	//打开的文件对象
	PFILE_OBJECT pfoFileObject;

	//返回值
	BOOLEAN bReturn;
	
	//是否是文件夹
	BOOLEAN bDirectory;

	//返回值
	NTSTATUS status;
	
	//文件流上下文
	PFILE_STREAM_CONTEXT pscFileStreamContext = NULL;

	//卷上下文
	PVOLUME_CONTEXT pvcVolumeContext = NULL;

	//申请的大小
	LARGE_INTEGER nAllocateSize;

	//文件大小
	LARGE_INTEGER nFileSize;

	//打开的文件句柄
	HANDLE hFile = NULL;

	//重新打开的文件对象
	PFILE_OBJECT pfoFileObjectOpened = NULL;
	
	//
	//确保IRQL<=APC_LEVEL
	//
	PAGED_CODE();

	//
	//获取需要的参数
	//
	pIoParameterBlock = pfcdCBD -> Iopb;
	pfiInstance = pIoParameterBlock -> TargetInstance;
	pfoFileObject = pIoParameterBlock->TargetFileObject;//pFltObjects -> FileObject;

	DebugTraceFileAndProcess(
		DEBUG_TRACE_ALL_IO,
		"PreClose",
		FILE_OBJECT_NAME_BUFFER(pfoFileObject),
		("PreClose entered.")
		);

	//
	//获取文件基本信息
	//
	status = FileGetStandardInformation(
		pfiInstance,
		pfoFileObject,
		&nAllocateSize,
		&nFileSize,
		&bDirectory
		); 
	//
	//失败或者是目录都直接返回
	//
	if(!NT_SUCCESS(status)){

		DebugTraceFileAndProcess(
			DEBUG_TRACE_NORMAL_INFO,
			"PreClose",
			FILE_OBJECT_NAME_BUFFER(pfoFileObject),
			("Cannot get file imformation.")
			);

		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	if(bDirectory){

		DebugTraceFileAndProcess(
			DEBUG_TRACE_NORMAL_INFO,
			"PreClose",
			FILE_OBJECT_NAME_BUFFER(pfoFileObject),
			("Closing a dictory. Pass now.")
			);

		return FLT_PREOP_SUCCESS_NO_CALLBACK;
	}

	do{
		//
		//获取卷上下文
		//
		status = FltGetVolumeContext(
			pFltObjects->Filter,
			pFltObjects->Volume,
			(PFLT_CONTEXT *)&pvcVolumeContext
			);

		if(!NT_SUCCESS(status)){

			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO,
				"PreClose",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("Cannot get volume context. Pass now.")
				);

			pvcVolumeContext = NULL;
			break;
		}

		//
		//获取文件流上下文
		//
		status = FctGetSpecifiedFileStreamContext(
			pfiInstance,
			pfoFileObject,
			&pscFileStreamContext
			);

		if(!NT_SUCCESS(status)){

			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO,
				"PreClose",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("Cannot get file context. Pass now.")
				);

			pscFileStreamContext = NULL;
			break;
		}

		//
		//如果打开的是机密文件 //刷掉缓存 但是会出现bug ntfs文件系统错误
		//
		if(FctGetFileConfidentialCondition(pscFileStreamContext) == ENCRYPTED_TYPE_CONFIDENTIAL){
			//
			//如果是流文件的话说明是系统内部打开的,我们不动它 其他情况引用计数减1
 			//
			if ((pFltObjects->FileObject->Flags & FO_STREAM_FILE) != FO_STREAM_FILE){

					DebugTraceFileAndProcess(
						DEBUG_TRACE_NORMAL_INFO,
						"PreClose",
						FILE_OBJECT_NAME_BUFFER(pfoFileObject),
						("Not a stream file.Dereference now.")
						);

					FctDereferenceFileContext(pscFileStreamContext);
				}

			DebugTraceFileAndProcess(
				DEBUG_TRACE_IMPORTANT_INFO|DEBUG_TRACE_CONFIDENTIAL,
				"PreClose",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("Closeing an encrypted file.Valid size %d",pscFileStreamContext->nFileValidLength)
				);

			//
			//所有引用全部释放 如果需要则更新加密头
			//
			if((FctIsReferenceCountZero(pscFileStreamContext))&&(FctIsUpdateWhenCloseFlag(pscFileStreamContext))){
				//
				//手动打开文件
				//
				DebugTraceFileAndProcess(
					DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
					"PreClose",
					FILE_OBJECT_NAME_BUFFER(pfoFileObject),
					("All closed now refresh file encryption header.")
					);

				status = FileCreateForHeaderWriting(pfiInstance,&pscFileStreamContext->usName,&hFile);

				if(!NT_SUCCESS(status)){
					
					DebugTraceFileAndProcess(
						DEBUG_TRACE_ERROR,
						"PreClose",
						FILE_OBJECT_NAME_BUFFER(pfoFileObject),
						("Error:Cannot open file.Status 0x%X",status)
						);
					ASSERT(FALSE);
					break;
				}

				status = ObReferenceObjectByHandle(
					hFile,
					STANDARD_RIGHTS_ALL,
					*IoFileObjectType,
					KernelMode,
					(PVOID *)&pfoFileObjectOpened,
					NULL
					);

				if(!NT_SUCCESS(status)){

					DebugTraceFileAndProcess(
						DEBUG_TRACE_ERROR,
						"PreClose",
						FILE_OBJECT_NAME_BUFFER(pfoFileObject),
						("Error:Cannot reference object. 0x%X",status)
						);
					ASSERT(FALSE);
					break;
				}
				
				//
				//重写加密头
				//
				status = FileWriteEncryptionHeader(
					pfiInstance,
					pfoFileObjectOpened,
					pvcVolumeContext,
					pscFileStreamContext
					);

				if(!NT_SUCCESS(status)){
				
					DebugTraceFileAndProcess(
						DEBUG_TRACE_ERROR,
						"PreClose",
						FILE_OBJECT_NAME_BUFFER(pfoFileObject),
						("Error:Cannot update file header. Status 0x%X",status)
						);
					ASSERT(FALSE);
					break;
				}

				FctSetUpdateWhenCloseFlag(pscFileStreamContext,FALSE);

				//
				//写完了释放缓存
				//
				FileClearCache(pfoFileObject);

				
				FctReleaseStreamContext(pscFileStreamContext) ;
				//FctFreeStreamContext(pscFileStreamContext);

				FltDeleteContext(pscFileStreamContext);

				pscFileStreamContext = NULL;
			}

			break;
		}


	}while(0);
	
	//
	//清理
	//
	if( pfoFileObjectOpened ){
		ObDereferenceObject( pfoFileObjectOpened );
	}

	if( hFile != NULL ){
		FltClose( hFile );
	}

	if (pscFileStreamContext != NULL){
		FctReleaseStreamContext(pscFileStreamContext);
	}

	if (pvcVolumeContext != NULL){
		FltReleaseContext(pvcVolumeContext) ;
	}
	
	return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

/*---------------------------------------------------------
函数名称:	Antinvader_PostClose
函数描述:	后请求回调IRP_MJ_CLOSE
输入参数:
			pfcdCBD				回调数据
			pFltObjects			文件对象
			lpCompletionContext	完成请求的上下文
			Flags				回调发生的标志(原因)
输出参数:
返回值:		
其他:		
更新维护:	2011.3.20    最初版本
---------------------------------------------------------*/
FLT_POSTOP_CALLBACK_STATUS
Antinvader_PostClose (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in_opt PVOID lpCompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    )
{	
	//
	//确保IRQL<=APC_LEVEL
	//
	PAGED_CODE();

//	DebugPrintFileStreamContext("Deleteing",((PFILE_STREAM_CONTEXT)lpCompletionContext));

//	FctDereferenceFileStreamContextObject( (PFILE_STREAM_CONTEXT)lpCompletionContext );

	return FLT_POSTOP_FINISHED_PROCESSING;//STATUS_SUCCESS;
}

/*---------------------------------------------------------
函数名称:	Antinvader_PreRead
函数描述:	预请求回调IRP_MJ_READ
输入参数:
			pfcdCBD				回调数据
			pFltObjects			文件对象
			lpCompletionContext	完成请求的上下文
输出参数:
返回值:		
			FLT_PREOP_SUCCESS_WITH_CALLBACK 结束
其他:		
更新维护:	2011.3.20    最初版本
			2011.4.2     添加了初步过滤代码
			2011.7.13    增加了偏移量修改,暂时忽略
							   Offset为空即按照当前偏移读
							   取的情况.
		    2011.7.17    修增加了释放缓存 修改了结构
			2011.7.29    大幅度重新修改
---------------------------------------------------------*/
FLT_PREOP_CALLBACK_STATUS
Antinvader_PreRead (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __deref_out_opt PVOID *lpCompletionContext
    )
{
	UNREFERENCED_PARAMETER(pFltObjects); 

	//I/O参数块,包含IRP相关信息
	PFLT_IO_PARAMETER_BLOCK  pIoParameterBlock;

	//文件对象
	PFILE_OBJECT pfoFileObject;
	
	//偏移量
	PLARGE_INTEGER pliOffset; 

	//返回值
	BOOLEAN bReturn;

	//当前是否是缓存读取方式
	BOOLEAN bCachedNow;

	//返回值
	NTSTATUS status;

	//文件流上下文
	PFILE_STREAM_CONTEXT pscFileStreamContext = NULL;

	//卷上下文
	PVOLUME_CONTEXT pvcVolumeContext = NULL;

	//本函数返回值
	FLT_PREOP_CALLBACK_STATUS fcsStatus = FLT_PREOP_SUCCESS_NO_CALLBACK ;
	
	//文件有效长度
	LARGE_INTEGER nFileValidSize;	
	
	//缓冲区地址
	ULONG ulBuffer;

	//旧的缓冲区地址
	ULONG ulOriginalBuffer;

	//缓冲区长度
	ULONG ulDataLength;

	//新的Mdl
	PMDL pMemoryDescribeList;

	//文件对象
	
	//
	//确保IRQL<=APC_LEVEL
	//
	PAGED_CODE();

	//
	//获取需要的数据
	//
	pIoParameterBlock = pfcdCBD -> Iopb;

	pfoFileObject = pFltObjects -> FileObject;

	DebugTraceFileAndProcess(
		DEBUG_TRACE_ALL_IO,
		"PreRead",
		FILE_OBJECT_NAME_BUFFER(pfoFileObject),
		("PreRead entered.")
		);

	//
	//如果没有交换过缓冲,那么上下文传入NULL
	//
	*lpCompletionContext = NULL;

	do{
		//
		//不是机密进程 直接返回
		//
		if(!IsCurrentProcessConfidential())
		{
			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO,
				"PreRead",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("Not confidential process. Pass now.")
				);

			break;
		}

		//
		//获取文件流上下文
		//
		status = FctGetSpecifiedFileStreamContext(
			pFltObjects->Instance,
			pfoFileObject,
			&pscFileStreamContext
			);

		if(!NT_SUCCESS(status)){

			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO,
				"PreRead",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("No file context find.Reguarded as not confidential file.")
				);

			pscFileStreamContext = NULL;
			break;
		}

		//
		//获取卷上下文
		//
		status = FltGetVolumeContext(
			pFltObjects->Filter,
			pFltObjects->Volume,
			(PFLT_CONTEXT *)&pvcVolumeContext
			);

		if(!NT_SUCCESS(status)){

			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO,
				"PreRead",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("No volum context find.Reguarded as not confidential file.")
				);

			pvcVolumeContext = NULL;
			break;
		}
		//
		//如果未加密 直接返回
		//
		if(FctGetFileConfidentialCondition(pscFileStreamContext) != ENCRYPTED_TYPE_CONFIDENTIAL){		

		//	DebugPrintFileObject("Antinvader_PreRead not confidential.",pfoFileObject,bCachedNow);

			DebugTraceFileAndProcess(
				DEBUG_TRACE_IMPORTANT_INFO,
				"PreRead",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("Confidential proccess read an not confidential file.File enctype %d",FctGetFileConfidentialCondition(pscFileStreamContext))
				);
		
			break;
		}

		//
		//如果根本不读
		//
		if(pIoParameterBlock->Parameters.Read.Length == 0){

			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO,
				"PreRead",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("Zero byte to read.Pass now.")
				);

			break;
		}

		//
		//我们只处理IRP_NOCACHE,IRP_PAGING_IO或IRP_SYNCHRONOUS_PAGING_IO 其他全部放过
		//
		if (!(pIoParameterBlock->IrpFlags &
			(IRP_NOCACHE | 
			IRP_PAGING_IO | 
			IRP_SYNCHRONOUS_PAGING_IO))){	
				
			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO,
				"PreRead",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("Not the operation we intrested.Pass now.")
				);

			break ;
		}

		//
		// 如果读取的长度超过了文件有效长度,返回EOF
		//
		FctGetFileValidSize(pscFileStreamContext,&nFileValidSize);

		if (pIoParameterBlock->Parameters.Read.ByteOffset.QuadPart >= nFileValidSize.QuadPart){

			pfcdCBD->IoStatus.Status =  STATUS_END_OF_FILE;
			pfcdCBD->IoStatus.Information = 0 ;
			fcsStatus = FLT_PREOP_COMPLETE ;
			break;
		}

		//
		//如果是非缓存读写,那么读写长度必须对齐(对其修正被放到AllocateAndSwapToNewMdlBuffer中)
		//
		
//        if (pIoParameterBlock->IrpFlags & IRP_NOCACHE){
//
//           pIoParameterBlock->Parameters.Read.Length = 
//				(ULONG)ROUND_TO_SIZE(pIoParameterBlock->Parameters.Read.Length,pvcVolumeContext->ulSectorSize);
//        }

		//
		//禁止FastIo
		//
		if (FLT_IS_FASTIO_OPERATION(pfcdCBD)){
			fcsStatus = FLT_PREOP_DISALLOW_FASTIO;

			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
				"PreRead",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("Disallow fast io.")
				);

			break ;
		} 

		DebugTraceFileAndProcess(
			DEBUG_TRACE_ALL_IO|DEBUG_TRACE_CONFIDENTIAL,
			"PreRead",
			FILE_OBJECT_NAME_BUFFER(pfoFileObject),
			("A confidential file and a confidential process encounterd.Enc:%d,",pscFileStreamContext->fctEncrypted)
			);

		//
		//保存偏移量地址
		//
		pliOffset = &pIoParameterBlock -> Parameters.Read.ByteOffset;

		if((pliOffset -> LowPart == FILE_USE_FILE_POINTER_POSITION )&&
			(pliOffset -> HighPart == -1)){
			//
			//暂时忽略按照当前偏移量的情况
			//
			DebugTraceFileAndProcess(
				DEBUG_TRACE_ALL_IO|DEBUG_TRACE_CONFIDENTIAL,
				"PreRead",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("Ignore %s tries to read by current postion.",CURRENT_PROCESS_NAME_BUFFER)
				);
			ASSERT(FALSE);
		}

		//
		//IRP操作的话交换缓冲
		//
        if( pfcdCBD->Flags & FLTFL_CALLBACK_DATA_IRP_OPERATION) {
		
			bReturn = AllocateAndSwapToNewMdlBuffer(
				pIoParameterBlock,
				pvcVolumeContext,
				(PULONG)lpCompletionContext,
				&pMemoryDescribeList,
				&ulBuffer,
				&ulDataLength,
				Allocate_BufferRead
				);

			if(!bReturn){

				DebugTraceFileAndProcess(
					DEBUG_TRACE_ERROR|DEBUG_TRACE_CONFIDENTIAL,
					"PreRead",
					FILE_OBJECT_NAME_BUFFER(pfoFileObject),
					("Error:Cannot swap buffer.")
					);			
			}

			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
				"PreRead",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("Swap buffer finished.")
				);
        }
		//
		//修改偏移
		//
		DebugTraceFileAndProcess(
			DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
			"PreRead",
			FILE_OBJECT_NAME_BUFFER(pfoFileObject),
			("PreRead modifing offset.Original offset %d ",pliOffset -> QuadPart)
			);

		pliOffset -> QuadPart += CONFIDENTIAL_FILE_HEAD_SIZE;

		fcsStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;

	}while(0);

	//
	//设置修改
	//
	FltSetCallbackDataDirty(pfcdCBD);
	
	if (pscFileStreamContext != NULL){
		FctReleaseStreamContext(pscFileStreamContext) ;
	}

	if (pvcVolumeContext != NULL){
		FltReleaseContext(pvcVolumeContext) ;
	}	

	return 	fcsStatus;
}

/*---------------------------------------------------------
函数名称:	Antinvader_PostRead
函数描述:	后请求回调IRP_MJ_READ
输入参数:
			pfcdCBD				回调数据
			pFltObjects			文件对象
			lpCompletionContext	完成请求的上下文
			Flags				回调发生的标志(原因)
输出参数:
返回值:		
			FLT_POSTOP_FINISHED_PROCESSING 成功
其他:		
更新维护:	2011.3.20    最初版本
			2011.4.2     添加了初步过滤代码
			2011.4.3     添加了是否需要释放内存的判断
			2011.7.10    添加了缓存和非缓存方式处理
			2011.7.30    修改增加了Do when safe
---------------------------------------------------------*/
FLT_POSTOP_CALLBACK_STATUS
Antinvader_PostRead (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in_opt PVOID lpCompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    )
{
	//返回值
	NTSTATUS status;

	//返回值
	BOOLEAN	 bReturn;

	//读出的Mdl地址
	PMDL *dpMdlAddressPointer;

	//数据缓冲地址
    PVOID  *dpBuffer;

	//缓冲长度
    PULONG pulLength;

	//可以使用的访问缓冲的方式
    LOCK_OPERATION loDesiredAcces;

	//I/O参数块,包含IRP相关信息
	PFLT_IO_PARAMETER_BLOCK  pIoParameterBlock;

	//
	//
	//一些微过滤器为了某些操作必须交换缓冲.考虑一个微过滤器
	//实现加密算法,对一个非缓冲(non-cached)IRP_MJ_READ,它一
	//般会希望把缓冲中的数据解密.同样的在写的时候,它希望把内
	//容加密.考虑以下情况:内容无法在这个空间中加密.因为对于
	//IRP_MJ_WRITE,这个微过滤器可能只有IoreadAccess权限.因此
	//微过滤器必须以他自己的有读写权限的缓冲区取代原的缓冲区.
	//加密了原缓冲区中的内容后写入新缓冲区后,再继续传递I/O请求.
	//
	//

	//缓冲区地址
	ULONG ulBuffer;

	//缓冲区长度
	ULONG ulDataLength;

	//新的Mdl
	PMDL pMemoryDescribeList;

	//当前是否是缓冲状态
	BOOLEAN bCached;

	//文件对象
	PFILE_OBJECT pfoFileObject;

	//本函数返回值
	FLT_POSTOP_CALLBACK_STATUS fcsStatus = FLT_POSTOP_FINISHED_PROCESSING;

	//新的缓冲
	ULONG ulSwappedBuffer;

	//文件流上下文
	PFILE_STREAM_CONTEXT pscFileStreamContext = NULL;

	//文件有效大小
	LARGE_INTEGER nFileValidLength;
	//
	//接下来读取分装微过滤回调数据,如果失败就返回.
	//

	status = FltDecodeParameters(
			pfcdCBD,
			&dpMdlAddressPointer,
			&dpBuffer,
			&pulLength,
			&loDesiredAcces
			);

	if(!NT_SUCCESS(status)){
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	//
	//获取文件对象,iopb,过滤器实例,回调上下文等
	//
	pIoParameterBlock = pfcdCBD -> Iopb;
	pfoFileObject = pFltObjects->FileObject;

	ulSwappedBuffer = (ULONG)lpCompletionContext;

	DebugTraceFileAndProcess(
		DEBUG_TRACE_ALL_IO|DEBUG_TRACE_CONFIDENTIAL,
		"PostRead",
		FILE_OBJECT_NAME_BUFFER(pfoFileObject),
		("PostRead entered.")
		);
	//
	//由于预操作中已经检查了是否是机密进程,这里不必再次检查.
	//

	//
	//当一个实例被卸除的时候,过滤管理器可能调用候后操作回调,
	//但是此时操作还未真的完成.这时,标志FLTFL_POST_OPERATION_DRAINING
	//会设置.此时提供了尽量少的信息.所以微过滤器应该清理所
	//有的从预操作中传来的操作上下文,并返回FLT_POSTOP_FINISHED_PROCESSING.
	//

	if( Flags & FLTFL_POST_OPERATION_DRAINING ){
		ASSERT(FALSE);
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	//
	//如果操作失败了 直接返回
	//
	if(!NT_SUCCESS(pfcdCBD->IoStatus.Status) 
		|| (pfcdCBD->IoStatus.Information == 0)){

		DebugTraceFileAndProcess(
			DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
			"PostRead",
			FILE_OBJECT_NAME_BUFFER(pfoFileObject),
			("Read faild.Pass now")
			);

		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	//
	//获取文件流上下文 检查读取的大小
	//
	
	status = FctGetSpecifiedFileStreamContext(
		pFltObjects->Instance,
		pfoFileObject,
		&pscFileStreamContext
		);

	if(!NT_SUCCESS(status)){

		DebugTraceFileAndProcess(
			DEBUG_TRACE_ERROR,
			"PostRead",
			FILE_OBJECT_NAME_BUFFER(pfoFileObject),
			("Error:Cannot get file context in post operation.")
			);
			ASSERT(FALSE);
			pscFileStreamContext = NULL;
			return FLT_POSTOP_FINISHED_PROCESSING;
		}
	
	//
	//获取文件有效大小 如果读出来的长度超过有效大小,那么修正
	//这里传入的offset是本来的offset非我们修改过的,所以用
	//本来的偏移量加上读出的长度和valid size正好比较,不用修正
	//
	FctGetFileValidSize(pscFileStreamContext,&nFileValidLength);

	if (nFileValidLength.QuadPart < 
		(pIoParameterBlock->Parameters.Read.ByteOffset.QuadPart + pfcdCBD->IoStatus.Information)){
				        pfcdCBD->IoStatus.Information = 
							(ULONG)(nFileValidLength.QuadPart - pIoParameterBlock->Parameters.Read.ByteOffset.QuadPart);
				    }
	//
	//我们需要把读出来的数据拷贝回用户缓冲里面.注意
	//传入的数据pfcdCBD是原始的(没交换缓冲前)的用户
	//缓冲,一直理解错了坑死我..所以最后记得拷贝
	//

	//
	//获取原始地址
	//

	if(pIoParameterBlock -> Parameters.Read.MdlAddress)	{

		DebugTraceFileAndProcess(
					DEBUG_TRACE_DATA_OPERATIONS|DEBUG_TRACE_CONFIDENTIAL,
					"PostRead",
					FILE_OBJECT_NAME_BUFFER(pfoFileObject),
					("Using mdl buffer.")
					);

		ulBuffer = (ULONG)MmGetSystemAddressForMdlSafe(
				pIoParameterBlock -> Parameters.Read.MdlAddress,
				NormalPagePriority
			);

			//
			//如果拿不到Mdl 返回错误
			//
			if(!ulBuffer){

				DebugTraceFileAndProcess(
					DEBUG_TRACE_ERROR|DEBUG_TRACE_CONFIDENTIAL,
					"PostRead",
					FILE_OBJECT_NAME_BUFFER(pfoFileObject),
					("Error:Could not get MDL address.")
					);

				pfcdCBD->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
				pfcdCBD->IoStatus.Information = 0;
				return FLT_POSTOP_FINISHED_PROCESSING;
			}
	}

	//
	//如果是FastIo或者是使用System Buffer传递数据 那么Buffer位置在
	//Parameters.Read.ReadBuffer里面
	//

	else if((pfcdCBD->Flags & FLTFL_CALLBACK_DATA_SYSTEM_BUFFER) ||
		    (pfcdCBD->Flags & FLTFL_CALLBACK_DATA_FAST_IO_OPERATION)){
		ulBuffer = 
			(ULONG)pIoParameterBlock -> Parameters.Read.ReadBuffer;

		DebugTraceFileAndProcess(
					DEBUG_TRACE_DATA_OPERATIONS|DEBUG_TRACE_CONFIDENTIAL,
					"PostRead",
					FILE_OBJECT_NAME_BUFFER(pfoFileObject),
					("Using system buffer.")
					);
	}else{
	//
	//其他情况 可能是一些任意的用户数据 很可能在Paged Pool中故
	//不能在DPC下进行修改
	//          
		DebugTraceFileAndProcess(
					DEBUG_TRACE_IMPORTANT_INFO|DEBUG_TRACE_CONFIDENTIAL,
					"PostRead",
					FILE_OBJECT_NAME_BUFFER(pfoFileObject),
					("Completing processing when safe.")
					);

		if (!FltDoCompletionProcessingWhenSafe( 
					pfcdCBD,
					pFltObjects,
					lpCompletionContext,
					Flags,
					Antinvader_PostReadWhenSafe,
					&fcsStatus)) {
				//
				//没法转到安全的IRQL 返回错误
				//
                pfcdCBD->IoStatus.Status = STATUS_UNSUCCESSFUL;
                pfcdCBD->IoStatus.Information = 0;
				return FLT_POSTOP_FINISHED_PROCESSING;
            }
			return fcsStatus;
	}

	//
	//获取长度
	//
	ulDataLength = pfcdCBD->IoStatus.Information;

	//
	//执行解解密操作
	//

	DebugTraceFileAndProcess(
			DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
			"PostRead",
			FILE_OBJECT_NAME_BUFFER(pfoFileObject),
			("Start deconde. Length:%d",ulDataLength)
			);

	//
	//测试使用,直接异或0x77
	//
	__try{

		for(ULONG i=0;i<ulDataLength ;i++){
			*((char *)(ulSwappedBuffer+i)) = *((char *)(ulSwappedBuffer+i))^0x77;
		}

	}__finally{
		//
		//数据已经被修改 设置标志位 并返回
		//
//		FltSetCallbackDataDirty(pfcdCBD);
	}

	//
	//把数据拷贝回原来的缓冲
	//
	if(ulSwappedBuffer){
		RtlCopyMemory((PVOID)ulBuffer,(PVOID)ulSwappedBuffer,pfcdCBD->IoStatus.Information );
	}
	//
	//取消引用
	//
//	FctDereferenceFileStreamContextObject((PFILE_STREAM_CONTEXT)lpCompletionContext);
	
	//FltSetCallbackDataDirty(pfcdCBD);

	FltSetCallbackDataDirty(pfcdCBD);

	if(ulSwappedBuffer){
		FreeAllocatedMdlBuffer(ulSwappedBuffer,Allocate_BufferRead);
	}

	if (pscFileStreamContext != NULL){
		FctReleaseStreamContext(pscFileStreamContext) ;
	}

	return FLT_POSTOP_FINISHED_PROCESSING;//STATUS_SUCCESS;
}

/*---------------------------------------------------------
函数名称:	Antinvader_PostReadWhenSafe
函数描述:	安全时Read后回调
输入参数:
			pfcdCBD				回调数据
			pFltObjects			文件对象
			lpCompletionContext	完成请求的上下文
			Flags				标志位
输出参数:
返回值:		
			
其他:		
更新维护:	2011.7.30    最初版本
---------------------------------------------------------*/
FLT_POSTOP_CALLBACK_STATUS
Antinvader_PostReadWhenSafe (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in PVOID lpCompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    )
{
	//缓冲区
	ULONG ulBuffer;

	//返回值
	NTSTATUS status;

	//数据长度
	ULONG ulDataLength;

	//新的缓冲
	ULONG ulSwappedBuffer;

	DebugTraceFileAndProcess(
		DEBUG_TRACE_ALL_IO|DEBUG_TRACE_CONFIDENTIAL,
		"PostReadWhenSafe",
		FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
		("PostReadWhenSafe entered.")
		);


	//
	//执行到这里说明是不带MDL的用户数据 锁住(也就是创建一个MDL)
	//

	status = FltLockUserBuffer( pfcdCBD );

    if (!NT_SUCCESS(status)){
		//
		//出错了 返回错误信息
		//
		DebugTraceFileAndProcess(
			DEBUG_TRACE_ERROR|DEBUG_TRACE_CONFIDENTIAL,
			"PostReadWhenSafe",
			FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
			("Error:Could not lock MDL address.")
			);
    
		pfcdCBD->IoStatus.Status = status;
        pfcdCBD->IoStatus.Information = 0;

		return FLT_POSTOP_FINISHED_PROCESSING;
    } 

	//
	//获取地址
	//
	ulBuffer = (ULONG)MmGetSystemAddressForMdlSafe(
		pfcdCBD -> Iopb -> Parameters.Read.MdlAddress,
		NormalPagePriority
	);

	if (!ulBuffer) {	
		//
		//出错了 返回错误信息
		//
		DebugTraceFileAndProcess(
			DEBUG_TRACE_ERROR|DEBUG_TRACE_CONFIDENTIAL,
			"PostReadWhenSafe",
			FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
			("Error:Could not get MDL address.")
			);
		pfcdCBD->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
		pfcdCBD->IoStatus.Information = 0;

		return FLT_POSTOP_FINISHED_PROCESSING;
	} 

	//
	//获取长度 新的缓冲
	//

	ulDataLength = pfcdCBD->IoStatus.Information;

	ulSwappedBuffer = (ULONG)lpCompletionContext;

	//
	//执行解解密操作
	//
	DebugTraceFileAndProcess(
			DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
			"PostReadWhenSafe",
			FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
			("Start deconde. Length:%d",ulDataLength)
			);
	/*
	__try{

		for(ULONG i=0;i<ulDataLength ;i++){
			*((char *)(ulBuffer+i)) = *((char *)(ulBuffer+i))^0x77;
		}

	}__finally{
		//
		//数据已经被修改 设置标志位 并返回
		//
		FltSetCallbackDataDirty(pfcdCBD);
	}*/

	//
	//把数据拷贝回原来的缓冲
	//

	RtlCopyMemory((PVOID) ulBuffer,(PVOID)ulSwappedBuffer,pfcdCBD->IoStatus.Information );

	FltSetCallbackDataDirty(pfcdCBD);

	FreeAllocatedMdlBuffer(ulSwappedBuffer,Allocate_BufferRead);
	
	return FLT_POSTOP_FINISHED_PROCESSING;
}


/*---------------------------------------------------------
函数名称:	Antinvader_PreWrite
函数描述:	预请求回调IRP_MJ_WRITE
输入参数:
			pfcdCBD				回调数据
			pFltObjects			文件对象
			lpCompletionContext	完成请求的上下文
输出参数:
返回值:		
			
其他:		
更新维护:	2011.3.20    最初版本
			2011.4.3     优化了回调 添加了初步过滤
			2012.1.2     对所有IRP进行了过滤 存在问题 
---------------------------------------------------------*/
FLT_PREOP_CALLBACK_STATUS
Antinvader_PreWrite (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __deref_out_opt PVOID *lpCompletionContext
    )
{
	UNREFERENCED_PARAMETER(pFltObjects); 

	//返回值
	NTSTATUS status;

	//返回值
	BOOLEAN	 bReturn;

	//当前是否缓存
	BOOLEAN bCachedNow;

	//本函数返回值 默认不需要回调
	FLT_PREOP_CALLBACK_STATUS pcStatus 
			= FLT_PREOP_SUCCESS_NO_CALLBACK;

	//读出的Mdl地址
	PMDL *dpMdlAddressPointer;

	//数据缓冲地址
    PVOID  *dpBuffer;

	//缓冲长度
    PULONG pulLength;

	//可以使用的访问缓冲的方式
    LOCK_OPERATION loDesiredAcces;

	//I/O参数块,包含IRP相关信息
	PFLT_IO_PARAMETER_BLOCK  pIoParameterBlock;

	//
	//
	//一些微过滤器为了某些操作必须交换缓冲.考虑一个微过滤器
	//实现加密算法,对一个非缓冲(non-cached)IRP_MJ_READ,它一
	//般会希望把缓冲中的数据解密.同样的在写的时候,它希望把内
	//容加密.考虑以下情况:内容无法在这个空间中加密.因为对于
	//IRP_MJ_WRITE,这个微过滤器可能只有IoreadAccess权限.因此
	//微过滤器必须以他自己的有读写权限的缓冲区取代原的缓冲区.
	//加密了原缓冲区中的内容后写入新缓冲区后,再继续传递I/O请求.
	//
	//

	//缓冲区地址
	ULONG ulBuffer;

	//旧的缓冲区地址
	ULONG ulOriginalBuffer;

	//缓冲区长度
	ULONG ulDataLength;

	//新的Mdl
	PMDL pMemoryDescribeList;

	//文件对象
	PFILE_OBJECT pfoFileObject;

	//偏移量
	PLARGE_INTEGER	pliOffset;

	//文件大小
	LARGE_INTEGER	nFileSize;
	
	//文件流上下文
	PFILE_STREAM_CONTEXT pscFileStreamContext = NULL;

	//卷上下文
	PVOLUME_CONTEXT pvcVolumeContext = NULL;

	//
	//确保IRQL<=APC_LEVEL
	//
	PAGED_CODE();

	//
	//保存数据
	//
	pIoParameterBlock = pfcdCBD -> Iopb;

	pfoFileObject = pFltObjects -> FileObject;

	*lpCompletionContext = NULL;

	DebugTraceFileAndProcess(
		DEBUG_TRACE_ALL_IO,
		"PreWrite",
		FILE_OBJECT_NAME_BUFFER(pfoFileObject),
		("PreWrite entered.")
		);

	//
	//检查是否是机密进程
	//
	do{

		//
		//获取卷上下文
		//
		status = FltGetVolumeContext(
			pFltObjects->Filter,
			pFltObjects->Volume,
			(PFLT_CONTEXT *)&pvcVolumeContext
			);

		if(!NT_SUCCESS(status))	{
			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO,
				"PreWrite",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("No volum context find.Reguarded as not confidential file.")
				);
			pvcVolumeContext = NULL;
			break;
		}

		//
		//获取文件流上下文
		//
		status = FctGetSpecifiedFileStreamContext(
			pFltObjects->Instance,
			pfoFileObject,
			&pscFileStreamContext
			);

		if(!NT_SUCCESS(status))	{
			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO,
				"PreWrite",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("No file context find.Reguarded as not confidential file.")
				);
			pscFileStreamContext = NULL;
			break;
		}

		if(!IsCurrentProcessConfidential())	{

			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO,
				"PreWrite",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("Not confidential process.Pass now.")
				);
			break;
		}

		//
		//这里存在特殊情况,有些只读文件对象会发送
		//分页/不缓存写请求,来写入部分文件(我至今
		//也不理解这是怎么回事,是不是写分页文件?)
		//在这种情况下,加密文件会造成一部分内容明
		//文而一部分密文的情况从而损坏文件.所以判
		//断是否需要进行加密操作时对写如权限进行了
		//判断,以保证这样的情况不会发生.
		//
		if(FctGetFileConfidentialCondition(pscFileStreamContext) != ENCRYPTED_TYPE_CONFIDENTIAL ){
			//|| !pfoFileObject->WriteAccess){		

			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO,//|DEBUG_TRACE_CONFIDENTIAL,
				"PreWrite",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("The file type we are not interested in.")
				);

			break;
		}


		//
		//读取分装微过滤回调数据 如果失败就返回
		//

		status = FltDecodeParameters(
				pfcdCBD,
				&dpMdlAddressPointer,
				&dpBuffer,
				&pulLength,
				&loDesiredAcces
				);

		if(!NT_SUCCESS(status)){
			pcStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
			break;
		}


		//
		//禁止FastIo
		//
		if (FLT_IS_FASTIO_OPERATION(pfcdCBD)){

			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
				"PreWrite",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("Disallow fast io.")
				);

			pcStatus = FLT_PREOP_DISALLOW_FASTIO ;
			break ;
		}  

		//
		//保存偏移量地址
		//
		pliOffset = &pIoParameterBlock -> Parameters.Write.ByteOffset;


		//
		//只过滤IRP_NOCACHE IRP_PAGING_IO 或者 IRP_SYNCHRONOUS_PAGING_IO
		//
		if (!(pIoParameterBlock->IrpFlags & 
			(IRP_NOCACHE | 
			IRP_PAGING_IO | 
			IRP_SYNCHRONOUS_PAGING_IO))){

			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO,
				"PreWrite",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("The operation we are not interested in.")
				);
			//
			//有时文件可以通过缓存缓存io扩展,所以这里要记录文件大小
			//

			nFileSize.QuadPart = pliOffset->QuadPart + pIoParameterBlock -> Parameters.Write.Length;

			if(FctUpdateFileValidSizeIfLonger(pscFileStreamContext,&nFileSize,TRUE)){

				nFileSize.QuadPart += CONFIDENTIAL_FILE_HEAD_SIZE;

				if(!NT_SUCCESS(FileSetSize(pFltObjects->Instance,pfoFileObject,&nFileSize))){
					DebugTraceFileAndProcess(
						DEBUG_TRACE_ERROR|DEBUG_TRACE_CONFIDENTIAL,
						"PreWrite",
						FILE_OBJECT_NAME_BUFFER(pfoFileObject),
						("Error:Cannot set file size.")
						);
				}else{
						DebugTraceFileAndProcess(
						DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
						"PreWrite",
						FILE_OBJECT_NAME_BUFFER(pfoFileObject),
						("Set size to %d.",nFileSize.QuadPart)
						);
				}
			}

			pcStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
			break;
		}

		if((pliOffset -> LowPart == FILE_USE_FILE_POINTER_POSITION )&&
			(pliOffset -> HighPart == -1)){
			//
			//暂时忽略按照当前偏移量的情况
			//
			DebugTraceFileAndProcess(
				DEBUG_TRACE_ALL_IO|DEBUG_TRACE_CONFIDENTIAL,
				"PreWrite",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("Ignore %s tries to read by current postion.",CURRENT_PROCESS_NAME_BUFFER)
				);
			ASSERT(FALSE);
		}

		DebugTraceFileAndProcess(
			DEBUG_TRACE_ALL_IO|DEBUG_TRACE_CONFIDENTIAL,
			"PreWrite",
			FILE_OBJECT_NAME_BUFFER(pfoFileObject),
			("Confidential enter:Length %d offset %d",pIoParameterBlock -> Parameters.Write.Length,pIoParameterBlock -> Parameters.Write.ByteOffset)
			);

		//
		//修改偏移由于读的时候已经修改过偏移了 这里也要修改
		//
		pliOffset -> QuadPart += CONFIDENTIAL_FILE_HEAD_SIZE;

		//
		//如果需要进行缓冲分配
		//

		if( loDesiredAcces != IoModifyAccess ){
			//
			//申请新缓冲 如果失败就返回
			//

			bReturn = AllocateAndSwapToNewMdlBuffer(
				pIoParameterBlock,
				pvcVolumeContext,
				(PULONG)lpCompletionContext,
				&pMemoryDescribeList,
				NULL,//&ulBuffer,
				&ulDataLength,
				Allocate_BufferWrite
				);
			
			ulBuffer = *(PULONG)lpCompletionContext;
			//
			//不能在后操作回调中替换掉旧的缓冲和MDL.
			//过滤管理器自动执行这些操作。实际上微过
			//滤器在后操作回调中的Iopb中见到缓冲空间
			//和MDL是替换前的。微过滤
			//器必须自己在上下文中记录新的缓冲区。
			//

			if(!bReturn){

				DebugTraceFileAndProcess(
					DEBUG_TRACE_ERROR|DEBUG_TRACE_CONFIDENTIAL,
					"PreWrite",
					FILE_OBJECT_NAME_BUFFER(pfoFileObject),
					("Error:Cannot allocate new mdl buffer.")
				);

				pcStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
				break;
			}
		
			pcStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;

		}else{
			//
			//获取原始地址
			//

			if(pIoParameterBlock -> Parameters.Write.MdlAddress){
				ulOriginalBuffer = (ULONG)MmGetSystemAddressForMdlSafe(
						pIoParameterBlock -> Parameters.Write.MdlAddress,
						NormalPagePriority
					);

			}else{
				ulOriginalBuffer = 
					(ULONG)pIoParameterBlock -> Parameters.Write.WriteBuffer;
			}

			ulDataLength = pIoParameterBlock -> Parameters.Write.Length;

			//
			//直接使用原始地址
			//
			ulBuffer = ulOriginalBuffer;
		}

		//
		//执行加加密操作
		//
		DebugTraceFileAndProcess(
			DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
			"PreWrite",
			FILE_OBJECT_NAME_BUFFER(pfoFileObject),
			("Start encrypt. Length:%d",ulDataLength)
			);

		pcStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;

		//
		//测试使用 直接异或0x77
		//
		__try{

			for(ULONG i=0;i<ulDataLength;i++){
				*((char *)(ulBuffer+i)) = *((char *)(ulBuffer+i))^0x77;
			}

		}__finally{
			//
			//数据已经被修改 设置标志位 并返回
			//
			//FltSetCallbackDataDirty(pfcdCBD);
		}

		DebugTraceFileAndProcess(
			DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
			"PreWrite",
			FILE_OBJECT_NAME_BUFFER(pfoFileObject),
			("Filt finished.Length: %d offset %d",pIoParameterBlock -> Parameters.Write.Length,pIoParameterBlock -> Parameters.Write.ByteOffset)
			);
	}while(0);

	//
	//数据已经被修改 设置标志位 并返回
	//

	FltSetCallbackDataDirty(pfcdCBD);

	//
	//释放上下文
	//
	if (pscFileStreamContext != NULL){
		FctReleaseStreamContext(pscFileStreamContext) ;
	}

	if (pvcVolumeContext != NULL){
		FltReleaseContext(pvcVolumeContext) ;
	}

	return pcStatus;
}

/*---------------------------------------------------------
函数名称:	Antinvader_PostWrite
函数描述:	后请求回调IRP_MJ_WRITE
输入参数:
			pfcdCBD				回调数据
			pFltObjects			文件对象
			lpCompletionContext	完成请求的上下文
			Flags				回调发生的标志(原因)
输出参数:
返回值:		
			
其他:		
更新维护:	2011.3.20    最初版本
			2011.4.3     添加了初步过滤,暂未恢复Mdl地址
			2011.7.10    修改上下文传送内容,恢复了Mdl
			2011.7.15    重新阅读了文档 去掉了mdl操作
			2011.7.22    增加了引用计数操作
---------------------------------------------------------*/
FLT_POSTOP_CALLBACK_STATUS
Antinvader_PostWrite (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in_opt PVOID lpCompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    )
{
	//I/O参数块,包含IRP相关信息
	PFLT_IO_PARAMETER_BLOCK  pIoParameterBlock;

	//文件流上下文
	PFILE_STREAM_CONTEXT pscFileStreamContext;

	//起始写入点偏移量
	LARGE_INTEGER nByteOffset;

	//新文件大小
	LARGE_INTEGER nFileNewSize;

	//成功写入的长度
	ULONG ulWrittenBytes;

	//增加的长度
	LONGLONG llAddedBytes;

	//新的缓冲
	ULONG ulSwappedBuffer;

	//返回状态
	NTSTATUS status ;
	//
	//确保IRQL<=APC_LEVEL
	//
	PAGED_CODE();
	
	//
	//获取文件流上下文
	//
	status = FctGetSpecifiedFileStreamContext(
		pFltObjects->Instance,
		pFltObjects->FileObject,
		&pscFileStreamContext
		);

	if(!NT_SUCCESS(status))	{
		DebugTraceFileAndProcess(
			DEBUG_TRACE_NORMAL_INFO,
			"PostWrite",
			FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
			("No file context find.Reguarded as not confidential file.")
			);
		pscFileStreamContext = NULL;
		ASSERT(FALSE);
		return FLT_POSTOP_FINISHED_PROCESSING;
	}

	//
	//如果lpCompletionContext为0表示没有申请过内存,不需释放
	//回调已经被优化,仅申请过内存后才会有此回调.当一个实例被
	//卸除的时候,过滤管理器可能调用候后操作回调,但是此时操作
	//还未真的完成.这时,标志FLTFL_POST_OPERATION_DRAINING
	//会设置.此时提供了尽量少的信息.所以微过滤器应该清理所
	//有的从预操作中传来的操作上下文,并返回FLT_POSTOP_FINISHED_PROCESSING.
	//
	
	if( Flags & FLTFL_POST_OPERATION_DRAINING ){
		if(lpCompletionContext){
			FctReleaseStreamContext(pscFileStreamContext) ;
		}
		return FLT_POSTOP_FINISHED_PROCESSING;
	}


	//
	//保存Iopb 偏移量 写入长度
	//

	pIoParameterBlock	= pfcdCBD -> Iopb;

	nByteOffset			= pIoParameterBlock->Parameters.Write.ByteOffset ;
	
	ulWrittenBytes		= pfcdCBD->IoStatus.Information ;
	
	ulSwappedBuffer = (ULONG)lpCompletionContext;

	DebugTraceFileAndProcess(
			DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
			"PostWrite",
			FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
			("Confidential enterd.status: 0x%X Original valid :%d Offset %d , Bytes wrriten :%d",
			pfcdCBD->IoStatus.Status,(int)pscFileStreamContext->nFileValidLength.QuadPart,(int)nByteOffset.QuadPart,ulWrittenBytes)
			);

//	nFileNewSize.QuadPart = pscFileStreamContext->nFileValidLength.QuadPart + CONFIDENTIAL_FILE_HEAD_SIZE;

/*	if(!NT_SUCCESS(FileSetSize(pFltObjects->Instance,pFltObjects->FileObject,&nFileNewSize))){
		DebugTraceFileAndProcess(
			DEBUG_TRACE_ERROR|DEBUG_TRACE_CONFIDENTIAL,
			"PostWrite",
			FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
			("Cannot update file length to %d.",nFileNewSize.QuadPart)
			);
	}*/
	//
	//如果写入的长度超过了文件原本的ValidLength 那么说明长度加长了
	//重新修改文件有效长度
	//
/*
	FILE_STREAM_CONTEXT_LOCK_ON(pscFileStreamContext);

	if((llAddedBytes = 
		(nByteOffset.QuadPart + (LONGLONG)ulWrittenBytes) - 
		pscFileStreamContext->nFileValidLength.QuadPart) > 0){
			
			pscFileStreamContext->nFileValidLength.QuadPart = 
				nByteOffset.QuadPart + (LONGLONG)ulWrittenBytes;
			
			pscFileStreamContext->nFileSize.QuadPart += llAddedBytes;

			pscFileStreamContext->bUpdateWhenClose = TRUE;

	//		nFileNewSize.QuadPart = pscFileStreamContext->nFileValidLength.QuadPart + CONFIDENTIAL_FILE_HEAD_SIZE;

		DebugTraceFileAndProcess(
			DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
			"PostWrite",
			FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
			("Update file valid size to %d.",pscFileStreamContext->nFileValidLength.QuadPart)
			);
	}
	
	FILE_STREAM_CONTEXT_LOCK_OFF(pscFileStreamContext);
*//*
	if(pscFileStreamContext->bUpdateWhenClose){
		//
		//需要重刷缓存说明文件大小变动.这里重新设置.
		//
		if(!NT_SUCCESS(FileSetSize(pFltObjects->Instance,pFltObjects->FileObject,&nFileNewSize))){
			DebugTraceFileAndProcess(
				DEBUG_TRACE_ERROR|DEBUG_TRACE_CONFIDENTIAL,
				"PostWrite",
				FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
				("Cannot update file length to %d.",nFileNewSize.QuadPart)
				);	
		}
	}
*/
	if(ulSwappedBuffer){
		FreeAllocatedMdlBuffer(ulSwappedBuffer,Allocate_BufferWrite);
	}

	//查看是否需要更新缓存

	if(lpCompletionContext){
		FctReleaseStreamContext(pscFileStreamContext) ;
	}

	return FLT_POSTOP_FINISHED_PROCESSING;//STATUS_SUCCESS;
}

/*---------------------------------------------------------
函数名称:	Antinvader_PreSetInformation
函数描述:	预请求回调IRP_MJ_SET_INFORMATION
输入参数:
			pfcdCBD				回调数据
			pFltObjects			文件对象
			lpCompletionContext	完成请求的上下文
输出参数:
返回值:		
			
其他:		
更新维护:	2011.3.20    最初版本
			2011.7.12    修改了大小 保证了加密头存在
---------------------------------------------------------*/
FLT_PREOP_CALLBACK_STATUS
Antinvader_PreSetInformation (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __deref_out_opt PVOID *lpCompletionContext
    )
{
	//文件对象
	PFILE_OBJECT pfoFileObject;

	//I/O参数块,包含IRP相关信息
	PFLT_IO_PARAMETER_BLOCK  pIoParameterBlock;

	//文件信息类别
	FILE_INFORMATION_CLASS	ficFileInformation;
	
	//文件信息
	PVOID	pFileInformation;

	//数据大小
	ULONG ulLength;
	
	//返回值
	BOOLEAN bReturn;

	//返回值
	NTSTATUS status;

	//文件流上下文
	PFILE_STREAM_CONTEXT pscFileStreamContext = NULL;

	//卷上下文
	PVOLUME_CONTEXT pvcVolumeContext = NULL;

	//是否修改需要重新写入加密头
	BOOLEAN bUpdateFileEncryptionHeader = TRUE;

	//返回值
	FLT_PREOP_CALLBACK_STATUS pcsStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
	
	//
	//确保IRQL<=APC_LEVEL
	//
	PAGED_CODE();

	//
	//保存Iopb和文件信息指针
	//
	pIoParameterBlock = pfcdCBD -> Iopb;

	ficFileInformation = pIoParameterBlock->
		Parameters.SetFileInformation.FileInformationClass;

	pFileInformation = pIoParameterBlock->
		Parameters.SetFileInformation.InfoBuffer;

	ulLength = pIoParameterBlock->
		Parameters.SetFileInformation.Length;

	pfoFileObject = pFltObjects->FileObject;

	//
	//检查是否是机密进程
	//

	do{

//		if(!IsCurrentProcessConfidential()){
//			return FLT_PREOP_SUCCESS_NO_CALLBACK;
//		}
		//
		//获取文件流上下文
		//
		status = FctGetSpecifiedFileStreamContext(
			pFltObjects->Instance,
			pfoFileObject,
			&pscFileStreamContext
			);

		if(!NT_SUCCESS(status)){
			pscFileStreamContext = NULL;
			bUpdateFileEncryptionHeader = FALSE;
			break;
		}

		//
		//获取卷上下文
		//
		status = FltGetVolumeContext(
			pFltObjects->Filter,
			pFltObjects->Volume,
			(PFLT_CONTEXT *)&pvcVolumeContext
			);

		if(!NT_SUCCESS(status)){
			pvcVolumeContext = NULL;
			bUpdateFileEncryptionHeader = FALSE;
			break;
		}

		//
		//如果未加密 直接返回
		//
		if(FctGetFileConfidentialCondition(pscFileStreamContext) != ENCRYPTED_TYPE_CONFIDENTIAL){		
			bUpdateFileEncryptionHeader = FALSE;
			break;
		}

		if(!IsCurrentProcessConfidential()){

			//if(!IsCurrentProcessSystem()){
	/*			
				bUpdateFileEncryptionHeader = FALSE;
				break;
	*/	//}
			DebugTraceFileAndProcess(
					DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
					"PreSetInformation",
					FILE_OBJECT_NAME_BUFFER(pfoFileObject),
					("An non confidential process operate on a confidential file.FCB:0x%X",pfoFileObject->FsContext)
					);

	/*		DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
				"PreSetInformation",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("System operate on confidential file.Access granted.FCB:0x%X",pfoFileObject->FsContext)
				);*/
			break;
		}

		//
		//禁止 FAST IO
		//
		if (FLT_IS_FASTIO_OPERATION(pfcdCBD))
		{
			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
				"PreSetInformation",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("Disallow fast io.")
				);
			pcsStatus = FLT_PREOP_DISALLOW_FASTIO ;
			break;
		}


		DebugTraceFileAndProcess(
			DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
			"PreSetInformation",
			FILE_OBJECT_NAME_BUFFER(pfoFileObject),
			("Confidential entered,Valid file size %d,file size %d",
				pscFileStreamContext->nFileValidLength,pscFileStreamContext->nFileSize)
			);

		//
		//获取文件对象
		//
		pfoFileObject = pFltObjects -> FileObject;

		//
		//打印调试信息
		//

		//DebugPrintFileObject("Antinvader_PreSetInformation",pfoFileObject,CALLBACK_IS_CACHED(pIoParameterBlock));
		//
		//具体类型,具体分析
		//
		switch(ficFileInformation){

		case FileAllInformation:
				//
				//
				// FileAllInformation,是由以下结构组成.即使长度不够,
				// 依然可以返回前面的字节.
				//typedef struct _FILE_ALL_INFORMATION {
				//    FILE_BASIC_INFORMATION BasicInformation;
				//    FILE_STANDARD_INFORMATION StandardInformation;
				//    FILE_INTERNAL_INFORMATION InternalInformation;
				//    FILE_EA_INFORMATION EaInformation;
				//    FILE_ACCESS_INFORMATION AccessInformation;
				//    FILE_POSITION_INFORMATION PositionInformation;
				//    FILE_MODE_INFORMATION ModeInformation;
				//    FILE_ALIGNMENT_INFORMATION AlignmentInformation;
				//    FILE_NAME_INFORMATION NameInformation;
				//} FILE_ALL_INFORMATION, *PFILE_ALL_INFORMATION;
				// 我们需要注意的是返回的字节里是否包含了StandardInformation
				// 这个可能影响文件的大小的信息.
				//
				//
			{
				PFILE_ALL_INFORMATION paiFileInformation 
					= (PFILE_ALL_INFORMATION) pFileInformation;

			
				//
				//如果包含了StandardInformation那么进行修改
				//
				if( ulLength>= 
					sizeof(FILE_BASIC_INFORMATION) + 
					sizeof(FILE_STANDARD_INFORMATION)){

					DebugTraceFileAndProcess(
						DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
						"PreSetInformation",
						FILE_OBJECT_NAME_BUFFER(pfoFileObject),
						("FileAllInformation Entered EndOfFile original %d",
						paiFileInformation->StandardInformation.EndOfFile.QuadPart)
						);		
					//
					//更新有效长度 实际长度
					//

					FctUpdateFileValidSize(pscFileStreamContext,&paiFileInformation->StandardInformation.EndOfFile,TRUE);

					//
					//修改长度
					//

					paiFileInformation->StandardInformation.AllocationSize.QuadPart = 
						ROUND_TO_SIZE(paiFileInformation->StandardInformation.AllocationSize.QuadPart + CONFIDENTIAL_FILE_HEAD_SIZE,
						pvcVolumeContext->ulSectorSize);

					paiFileInformation->StandardInformation.EndOfFile.QuadPart+= CONFIDENTIAL_FILE_HEAD_SIZE;

					//
					//如果包括PositionInformation那么修改
					//
					if( ulLength >= 
						sizeof(FILE_BASIC_INFORMATION) + 
						sizeof(FILE_STANDARD_INFORMATION) +
						sizeof(FILE_INTERNAL_INFORMATION) +
						sizeof(FILE_EA_INFORMATION) +
						sizeof(FILE_ACCESS_INFORMATION) +
						sizeof(FILE_POSITION_INFORMATION)){
							paiFileInformation->PositionInformation.CurrentByteOffset.QuadPart
								+= CONFIDENTIAL_FILE_HEAD_SIZE;
						
					}

				}
				break;
			}

		case FileAllocationInformation:
			{
				PFILE_ALLOCATION_INFORMATION palloiFileInformation
					=(PFILE_ALLOCATION_INFORMATION)pFileInformation;

				DebugTraceFileAndProcess(
					DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
					"PreSetInformation",
					FILE_OBJECT_NAME_BUFFER(pfoFileObject),
					("FileAllocationInformation Entered,AllocationSize original %d",
					palloiFileInformation->AllocationSize.QuadPart)
					);
				//
				//修改长度
				//
				palloiFileInformation->AllocationSize.QuadPart = 
					ROUND_TO_SIZE(palloiFileInformation->AllocationSize.QuadPart+ CONFIDENTIAL_FILE_HEAD_SIZE,
					pvcVolumeContext->ulSectorSize);

				break;
			}
			//
			//其他同理 修改之
			//
		case FileValidDataLengthInformation:
			{
				PFILE_VALID_DATA_LENGTH_INFORMATION pvliInformation = 
					(PFILE_VALID_DATA_LENGTH_INFORMATION)pFileInformation;

				DebugTraceFileAndProcess(
					DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
					"PreSetInformation",
					FILE_OBJECT_NAME_BUFFER(pfoFileObject),
					("FileValidDataLengthInformation Entered,AllocationSize original %d",
					pvliInformation->ValidDataLength.QuadPart)
					);
				//
				//更新有效长度 实际长度
				//

				FctUpdateFileValidSize(pscFileStreamContext,&pvliInformation->ValidDataLength,TRUE);

				pvliInformation->ValidDataLength.QuadPart += CONFIDENTIAL_FILE_HEAD_SIZE;

				break;
			}
		case FileStandardInformation:
			{
				PFILE_STANDARD_INFORMATION psiFileInformation 
					= (PFILE_STANDARD_INFORMATION)pFileInformation;

				DebugTraceFileAndProcess(
					DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
					"PreSetInformation",
					FILE_OBJECT_NAME_BUFFER(pfoFileObject),
					("FileStandardInformation Entered,AllocationSize EndOfFile original %d",
					psiFileInformation->EndOfFile.QuadPart)
					);
				//
				//更新有效长度 实际长度
				//

				FctUpdateFileValidSize(pscFileStreamContext,&psiFileInformation->EndOfFile,TRUE);
				
				//
				//修改长度
				//

				psiFileInformation->AllocationSize.QuadPart = 
					ROUND_TO_SIZE(psiFileInformation->AllocationSize.QuadPart+ CONFIDENTIAL_FILE_HEAD_SIZE,
					pvcVolumeContext->ulSectorSize);
     
				psiFileInformation->EndOfFile.QuadPart += CONFIDENTIAL_FILE_HEAD_SIZE;

			
				break;
			}
		case FileEndOfFileInformation:
			{
				PFILE_END_OF_FILE_INFORMATION peofInformation = 
					(PFILE_END_OF_FILE_INFORMATION)pFileInformation;

				DebugTraceFileAndProcess(
					DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
					"PreSetInformation",
					FILE_OBJECT_NAME_BUFFER(pfoFileObject),
					("FileEndOfFileInformation Entered, EndOfFile original %d",
					peofInformation->EndOfFile.QuadPart)
					);

				//
				//更新有效长度 实际长度
				//

				FctUpdateFileValidSize(pscFileStreamContext,&peofInformation->EndOfFile,TRUE);

				//
				//修改长度
				//
				peofInformation->EndOfFile.QuadPart += CONFIDENTIAL_FILE_HEAD_SIZE;

				break;
			}
		case FilePositionInformation:
			{
				PFILE_POSITION_INFORMATION ppiInformation =
					(PFILE_POSITION_INFORMATION)pFileInformation; 

				ppiInformation->CurrentByteOffset.QuadPart += CONFIDENTIAL_FILE_HEAD_SIZE;

				DebugTraceFileAndProcess(
					DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
					"PreSetInformation",
					FILE_OBJECT_NAME_BUFFER(pfoFileObject),
					("FilePositionInformation Entered, EndOfFile original %d",
					ppiInformation->CurrentByteOffset.QuadPart)
					);

				break;
			}

		case FileRenameInformation:
		case FileNameInformation:
		case FileNamesInformation:
			{
				//
				//使用后回调重新获得文件名
				//
				DebugTraceFileAndProcess(
					DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
					"PreSetInformation",
					FILE_OBJECT_NAME_BUFFER(pfoFileObject),
					("Name operation entered, using post callback to update name.")
					);

				pcsStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;
				*lpCompletionContext = pscFileStreamContext;
				pscFileStreamContext = NULL;

				bUpdateFileEncryptionHeader = FALSE;
	
				break;
			}
	   default:
			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
				"PreSetInformation",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("default Entered, encouterd something we do not concern.Type code %d",ficFileInformation)
				);
			
			bUpdateFileEncryptionHeader = FALSE;
			//
			//不知道是什么
			//
		    //ASSERT(FALSE);
		}

	}while(0);

	if(	bUpdateFileEncryptionHeader){
		FctSetUpdateWhenCloseFlag(pscFileStreamContext,TRUE);
	}

	if (pscFileStreamContext != NULL){
		FctReleaseStreamContext(pscFileStreamContext) ;
	}

	if (pvcVolumeContext != NULL){
		FltReleaseContext(pvcVolumeContext) ;
	}	
	
	return pcsStatus;
}

/*---------------------------------------------------------
函数名称:	Antinvader_PostSetInformation
函数描述:	后请求回调IRP_MJ_SET_INFORMATION
输入参数:
			pfcdCBD				回调数据
			pFltObjects			文件对象
			lpCompletionContext	完成请求的上下文
			Flags				回调发生的标志(原因)
输出参数:
返回值:		
			
其他:		
更新维护:	2011.3.20    最初版本
			2011.7.21    增加了FctDereferenceFileStreamContextObject
			2011.8.
---------------------------------------------------------*/
FLT_POSTOP_CALLBACK_STATUS
Antinvader_PostSetInformation (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in_opt PVOID lpCompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    )
{	
	//
	//确保IRQL<=APC_LEVEL
	//
	PAGED_CODE();

	//文件流上下文
	PFILE_STREAM_CONTEXT pscFileStreamContext = (PFILE_STREAM_CONTEXT)lpCompletionContext;
	
	//文件名信息
	PFLT_FILE_NAME_INFORMATION pfniFileNameInformation = NULL;

	//状态
	NTSTATUS status;
	
	do{
		//
		//获取文件名称信息
		//
		status = FltGetFileNameInformation(
			pfcdCBD, 
			FLT_FILE_NAME_OPENED|FLT_FILE_NAME_QUERY_DEFAULT,
			&pfniFileNameInformation
			);
			
		if( !NT_SUCCESS(status)){
			//
			//没拿到文件信息
			//
			DebugTraceFileAndProcess(
				DEBUG_TRACE_ERROR|DEBUG_TRACE_CONFIDENTIAL,
				"PostSetInformation",
				FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
				("Error:Cannot get file information, pass now.")
				);

			pfniFileNameInformation = NULL;
			break;
		}
		
		DebugTraceFileAndProcess(
			DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
			"PostSetInformation",
			FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
			("Successfully get new name.Updating now.")
			);

		FctUpdateStreamContextFileName(&pfniFileNameInformation->Name,pscFileStreamContext);

	}while(0);

	if (pfniFileNameInformation != NULL){
		FltReleaseFileNameInformation(pfniFileNameInformation);
	}	
	
	if(pscFileStreamContext != NULL){
		FctReleaseStreamContext(pscFileStreamContext) ;
	}

	return FLT_POSTOP_FINISHED_PROCESSING;//STATUS_SUCCESS;
}

/*---------------------------------------------------------
函数名称:	Antinvader_PreQueryInformation
函数描述:	预请求回调IRP_MJ_QUERY_INFORMATION
输入参数:
			pfcdCBD				回调数据
			pFltObjects			文件对象
			lpCompletionContext	完成请求的上下文
输出参数:
返回值:		
			
其他:		
更新维护:	2011.3.20    最初版本
			2011.7.12    添加了过滤 判断是否修改大小
---------------------------------------------------------*/
FLT_PREOP_CALLBACK_STATUS
Antinvader_PreQueryInformation (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __deref_out_opt PVOID *lpCompletionContext
    )
{
	//返回值
	NTSTATUS status;

	//文件流上下文
	PFILE_STREAM_CONTEXT pscFileStreamContext = NULL;

	//卷上下文
	PVOLUME_CONTEXT pvcVolumeContext = NULL;

	//本函数的
	FLT_PREOP_CALLBACK_STATUS pcStatus = FLT_PREOP_SUCCESS_WITH_CALLBACK;

	//
	//确保IRQL<=APC_LEVEL
	//
	PAGED_CODE();

	//
	//非同步操作 放过
	//
//	if(FltIsOperationSynchronous(pfcdCBD)){
//		return FLT_PREOP_SUCCESS_WITH_CALLBACK;
//	}

	do{
		//
		//检查是否是机密进程
		//

		if(!IsCurrentProcessConfidential())	{
			pcStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
			break;
		}
		//
		//获取文件流上下文
		//
		status = FctGetSpecifiedFileStreamContext(
			pFltObjects->Instance,
			pFltObjects->FileObject,
			&pscFileStreamContext
			);

		if(!NT_SUCCESS(status))	{
			pcStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
			pscFileStreamContext = NULL;
			break;
		}

		//
		//获取卷上下文
		//
		status = FltGetVolumeContext(
			pFltObjects->Filter,
			pFltObjects->Volume,
			(PFLT_CONTEXT *)&pvcVolumeContext
			);

		if(!NT_SUCCESS(status)){
			pcStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
			pvcVolumeContext = NULL;
			break;
		}

		//
		//如果未加密 直接返回
		//
		if(FctGetFileConfidentialCondition(pscFileStreamContext) != ENCRYPTED_TYPE_CONFIDENTIAL){		
			pcStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO,
				"PreSetInformation",
				FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
				("not confidential file, pass now.")
				);
			break;
		}

		//
		//禁止 FAST IO
		//
		if (FLT_IS_FASTIO_OPERATION(pfcdCBD))
		{
			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
				"PreQueryInformation",
				FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
				("Disallow fast io.")
				);
			pcStatus = FLT_PREOP_DISALLOW_FASTIO ;
			break;
		}

		//
		//把文件流上下文传过去 设为NULL表示不用释放
		//

		*lpCompletionContext = pscFileStreamContext;
		pscFileStreamContext = NULL;

	}while(0);

	if (pscFileStreamContext != NULL){
		FctReleaseStreamContext(pscFileStreamContext) ;
	}

	if (pvcVolumeContext != NULL){
		FltReleaseContext(pvcVolumeContext) ;
	}

	return pcStatus;
}

/*---------------------------------------------------------
函数名称:	Antinvader_PostQueryInformation
函数描述:	后请求回调IRP_MJ_QUERY_INFORMATION
输入参数:
			pfcdCBD				回调数据
			pFltObjects			文件对象
			lpCompletionContext	完成请求的上下文
			Flags				回调发生的标志(原因)
输出参数:
返回值:		
			
其他:		
更新维护:	2011.3.20    最初版本
			2011.7.10    添加了修改大小信息
---------------------------------------------------------*/
FLT_POSTOP_CALLBACK_STATUS
Antinvader_PostQueryInformation (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in_opt PVOID lpCompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    )
{
	//I/O参数块,包含IRP相关信息
	PFLT_IO_PARAMETER_BLOCK  pIoParameterBlock;

	//文件信息类别
	FILE_INFORMATION_CLASS	ficFileInformation;
	
	//文件信息
	PVOID	pFileInformation;

	//数据长度
	ULONG ulLength;
	
	//流文件上下文
	PFILE_STREAM_CONTEXT pscFileStreamContext;

	//文件对象
	PFILE_OBJECT	pfoFileObject;
	
	//
	//确保IRQL<=APC_LEVEL
	//
	PAGED_CODE();

	//
	//保存Iopb和文件信息指针 上下文等
	//
	pIoParameterBlock = pfcdCBD -> Iopb;

	ficFileInformation = pIoParameterBlock->
		Parameters.QueryFileInformation.FileInformationClass;

	pFileInformation = pIoParameterBlock->
		Parameters.QueryFileInformation.InfoBuffer;

	ulLength = pIoParameterBlock->
		Parameters.SetFileInformation.Length;

	pscFileStreamContext = (PFILE_STREAM_CONTEXT)lpCompletionContext;

	pfoFileObject = pFltObjects->FileObject;

	DebugTraceFileAndProcess(
		DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
		"PostQueryInformation",
		FILE_OBJECT_NAME_BUFFER(pfoFileObject),
		("Confidential entered,Valid file size %d",
		pscFileStreamContext->nFileValidLength)
		);

	//
	//具体类型,具体分析
	//
	switch(ficFileInformation)
	{
	case FileAllInformation:
			//
			//
		    // FileAllInformation,是由以下结构组成.即使长度不够,
            // 依然可以返回前面的字节.
            //typedef struct _FILE_ALL_INFORMATION {
            //    FILE_BASIC_INFORMATION BasicInformation;
            //    FILE_STANDARD_INFORMATION StandardInformation;
            //    FILE_INTERNAL_INFORMATION InternalInformation;
            //    FILE_EA_INFORMATION EaInformation;
            //    FILE_ACCESS_INFORMATION AccessInformation;
            //    FILE_POSITION_INFORMATION PositionInformation;
            //    FILE_MODE_INFORMATION ModeInformation;
            //    FILE_ALIGNMENT_INFORMATION AlignmentInformation;
            //    FILE_NAME_INFORMATION NameInformation;
            //} FILE_ALL_INFORMATION, *PFILE_ALL_INFORMATION;
            // 我们需要注意的是返回的字节里是否包含了StandardInformation
            // 这个可能影响文件的大小的信息.
			//
			//
		{
			PFILE_ALL_INFORMATION paiFileInformation 
				= (PFILE_ALL_INFORMATION) pFileInformation;
			
			//
			//如果包含了StandardInformation那么进行修改
			//
			if( ulLength >= 
                sizeof(FILE_BASIC_INFORMATION) + 
                sizeof(FILE_STANDARD_INFORMATION)){
				
					DebugTraceFileAndProcess(
					DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
					"PostQueryInformation",
					FILE_OBJECT_NAME_BUFFER(pfoFileObject),
					("FileAllInformation Entered, EndOfFile original %d valid length %d",
					paiFileInformation->StandardInformation.EndOfFile.QuadPart,pscFileStreamContext->nFileValidLength.QuadPart)
					);		

				//
				//断言大小一定会超过或等于一个加密头
				//
				ASSERT(paiFileInformation->StandardInformation.EndOfFile.QuadPart>= CONFIDENTIAL_FILE_HEAD_SIZE ||
					paiFileInformation->StandardInformation.EndOfFile.QuadPart==0);

#ifdef DBG
				//
				//断言我们记录的文件信息没问题 
				//
				//FILE_STREAM_CONTEXT_LOCK_ON(pscFileStreamContext);
				//ASSERT(pscFileStreamContext->nFileValidLength.QuadPart == paiFileInformation->StandardInformation.EndOfFile.QuadPart);
				//FILE_STREAM_CONTEXT_LOCK_OFF(pscFileStreamContext);
#endif

//				paiFileInformation->StandardInformation.AllocationSize.QuadPart-= CONFIDENTIAL_FILE_HEAD_SIZE;
				
				FctGetFileValidSize(pscFileStreamContext,&paiFileInformation->StandardInformation.EndOfFile);
				//paiFileInformation->StandardInformation.EndOfFile.QuadPart = pscFileStreamContext->nFileValidLength.QuadPart;
				//paiFileInformation->StandardInformation.EndOfFile.QuadPart-= CONFIDENTIAL_FILE_HEAD_SIZE;

				//
				//如果包括PositionInformation那么修改
				//
				if( ulLength  >= 
                    sizeof(FILE_BASIC_INFORMATION) + 
                    sizeof(FILE_STANDARD_INFORMATION) +
                    sizeof(FILE_INTERNAL_INFORMATION) +
                    sizeof(FILE_EA_INFORMATION) +
                    sizeof(FILE_ACCESS_INFORMATION) +
                    sizeof(FILE_POSITION_INFORMATION)){
					//
					//如果当前位置大于一个加密头头那么修改
					//
					if(paiFileInformation->PositionInformation.CurrentByteOffset.
						QuadPart >= CONFIDENTIAL_FILE_HEAD_SIZE){

						paiFileInformation->PositionInformation.CurrentByteOffset.QuadPart
							-= CONFIDENTIAL_FILE_HEAD_SIZE;
					}
				}
			}
			break;
		}

	case FileAllocationInformation:
		{
			PFILE_ALLOCATION_INFORMATION palloiFileInformation
				=(PFILE_ALLOCATION_INFORMATION)pFileInformation;
			
			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
				"PostQueryInformation",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("FileAllocationInformation Entered,AllocationSize original %d",
				palloiFileInformation->AllocationSize.QuadPart)
				);
			//
			//断言大小一定会超过或等于一个加密头
			//
			ASSERT(palloiFileInformation->AllocationSize.QuadPart>=CONFIDENTIAL_FILE_HEAD_SIZE);

			//palloiFileInformation->AllocationSize.QuadPart-= CONFIDENTIAL_FILE_HEAD_SIZE;

			break;
		}
		//
		//其他同理 修改之
		//
    case FileValidDataLengthInformation:
        {
		    PFILE_VALID_DATA_LENGTH_INFORMATION pvliInformation = 
                (PFILE_VALID_DATA_LENGTH_INFORMATION)pFileInformation;

			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
				"PostQueryInformation",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("FileValidDataLengthInformation Entered,ValidDataLength original %d",
				pvliInformation->ValidDataLength.QuadPart)
				);

            ASSERT(pvliInformation->ValidDataLength.QuadPart >= CONFIDENTIAL_FILE_HEAD_SIZE);
#ifdef DBG
				//
				//断言我们记录的文件信息没问题 
				//
				//FILE_STREAM_CONTEXT_LOCK_ON(pscFileStreamContext);
				//ASSERT(pscFileStreamContext->nFileValidLength.QuadPart == pvliInformation->ValidDataLength.QuadPart);
				//FILE_STREAM_CONTEXT_LOCK_OFF(pscFileStreamContext);
#endif
		    //pvliInformation->ValidDataLength.QuadPart -= CONFIDENTIAL_FILE_HEAD_SIZE;
			//pvliInformation->ValidDataLength.QuadPart = pscFileStreamContext->nFileValidLength.QuadPart;
			
			FctGetFileValidSize(pscFileStreamContext,&pvliInformation->ValidDataLength);

            break;
        }
    case FileStandardInformation:
        {
            PFILE_STANDARD_INFORMATION psiFileInformation 
				= (PFILE_STANDARD_INFORMATION)pFileInformation;
			
			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
				"PostQueryInformation",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("FileStandardInformation Entered,EndOfFile original %d",
				psiFileInformation->EndOfFile.QuadPart)
				);

            ASSERT(psiFileInformation->AllocationSize.QuadPart >= CONFIDENTIAL_FILE_HEAD_SIZE);
            ASSERT((psiFileInformation->EndOfFile.QuadPart >= CONFIDENTIAL_FILE_HEAD_SIZE) ||
				psiFileInformation->EndOfFile.QuadPart == 0);
#ifdef DBG
				//
				//断言我们记录的文件信息没问题 
				//
				//FILE_STREAM_CONTEXT_LOCK_ON(pscFileStreamContext);
				//ASSERT(pscFileStreamContext->nFileValidLength.QuadPart == psiFileInformation->EndOfFile.QuadPart);
				//FILE_STREAM_CONTEXT_LOCK_OFF(pscFileStreamContext);
#endif
//            psiFileInformation->AllocationSize.QuadPart -= CONFIDENTIAL_FILE_HEAD_SIZE;    

            //psiFileInformation->EndOfFile.QuadPart -= CONFIDENTIAL_FILE_HEAD_SIZE;
            //psiFileInformation->EndOfFile.QuadPart = pscFileStreamContext->nFileValidLength.QuadPart;

			FctGetFileValidSize(pscFileStreamContext,&psiFileInformation->EndOfFile);

			break;
        }
    case FileEndOfFileInformation:
        {
		    PFILE_END_OF_FILE_INFORMATION peofInformation = 
                (PFILE_END_OF_FILE_INFORMATION)pFileInformation;

			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
				"PostQueryInformation",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("FileEndOfFileInformation Entered, EndOfFile original %d",
				peofInformation->EndOfFile.QuadPart)
				);


            ASSERT(peofInformation->EndOfFile.QuadPart >= CONFIDENTIAL_FILE_HEAD_SIZE);
#ifdef DBG
				//
				//断言我们记录的文件信息没问题 
				//
				//FILE_STREAM_CONTEXT_LOCK_ON(pscFileStreamContext);
				//ASSERT(pscFileStreamContext->nFileValidLength.QuadPart == peofInformation->EndOfFile.QuadPart);
				//FILE_STREAM_CONTEXT_LOCK_OFF(pscFileStreamContext);
#endif
		    //peofInformation->EndOfFile.QuadPart -= CONFIDENTIAL_FILE_HEAD_SIZE;
			//peofInformation->EndOfFile.QuadPart = pscFileStreamContext->nFileValidLength.QuadPart;

			FctGetFileValidSize(pscFileStreamContext,&peofInformation->EndOfFile);

            break;
        }
	case FilePositionInformation:
		{
			PFILE_POSITION_INFORMATION ppiInformation =
				(PFILE_POSITION_INFORMATION)pFileInformation; 

			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
				"PostQueryInformation",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("FilePositionInformation Entered, EndOfFile original %d",
				ppiInformation->CurrentByteOffset.QuadPart)
				);

            if(ppiInformation->CurrentByteOffset.QuadPart > CONFIDENTIAL_FILE_HEAD_SIZE)

			    ppiInformation->CurrentByteOffset.QuadPart -= CONFIDENTIAL_FILE_HEAD_SIZE;
			break;
		}

    default:
		DebugTraceFileAndProcess(
			DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
			"PostQueryInformation",
			FILE_OBJECT_NAME_BUFFER(pfoFileObject),
			("default Entered, encouterd something we do not concern.Type code %d",ficFileInformation)
			);
	}

	//
	//取消引用
	//
/*	if(lpCompletionContext)
	{
//		FctDereferenceFileStreamContextObject((PFILE_STREAM_CONTEXT)lpCompletionContext);
	}*/

	//
	//设置修改
	//
	FltSetCallbackDataDirty(pfcdCBD);

	//
	//释放上下文
	//
	FctReleaseStreamContext(pscFileStreamContext) ;

	return FLT_POSTOP_FINISHED_PROCESSING;
}

/*---------------------------------------------------------
函数名称:	Antinvader_PreDirectoryControl
函数描述:	预请求回调IRP_MJ_DIRECTORY_CONTROL
输入参数:
			pfcdCBD				回调数据
			pFltObjects			文件对象
			lpCompletionContext	完成请求的上下文
输出参数:
返回值:		
			
其他:		
更新维护:	2011.3.20    最初版本
---------------------------------------------------------*/
FLT_PREOP_CALLBACK_STATUS
Antinvader_PreDirectoryControl (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __deref_out_opt PVOID *lpCompletionContext
    )
{	
	//Io参数块
    PFLT_IO_PARAMETER_BLOCK pIoParameterBlock;

	//本函数的返回值
    FLT_PREOP_CALLBACK_STATUS fcsReturn = FLT_PREOP_SUCCESS_NO_CALLBACK;

	//新数据的缓冲区
    PVOID pNewBuffer = NULL;

	//内存描述列表
    PMDL pmMemoryDiscriptionList = NULL;

	//卷上下文
    PVOLUME_CONTEXT pvcVolumeContext = NULL;

	//返回值
    NTSTATUS status;
	
	//返回值
	BOOLEAN bReturn;

	//流文件上下文
	PFILE_STREAM_CONTEXT pscFileStreamContext = NULL;

	//文件对象
	PFILE_OBJECT pfoFileObject;
	
	//
	//确保IRQL<=APC_LEVEL
	//

	PAGED_CODE();
    
	//
	//保存一些指针
	//
	pIoParameterBlock = pfcdCBD->Iopb;

	pfoFileObject = pFltObjects->FileObject;

	//
	//把完成上下文设为NULL 如果后来又被设为新的数据,说明交换过缓冲
	//
	*lpCompletionContext = NULL;

	do {

		//如果不是IRP_MN_QUERY_DIRECTORY 那么就不管了
		if ((pIoParameterBlock->MinorFunction != IRP_MN_QUERY_DIRECTORY) ||
			(pIoParameterBlock->Parameters.DirectoryControl.QueryDirectory.Length == 0)){
			break;
		}

		//
		//检查是否是机密进程
		//
		if(!IsCurrentProcessConfidential())	{
			
			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO,
				"PreDirectoryControl",
				FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
				("Not confidential process.Pass now.")
				);
			break;
		}
/*
		//
		//获取文件流上下文
		//
		status = FctGetSpecifiedFileStreamContext(
			pFltObjects->Instance,
			pFltObjects->FileObject,
			&pscFileStreamContext
			);

		if(!NT_SUCCESS(status)){

			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO,
				"PreDirectoryControl",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("No file context find.Reguarded as not confidential file.")
				);
			break;
		}
*/
/*		//
		//获取卷上下文
		//
		status = FltGetVolumeContext(
			pFltObjects->Filter,
			pFltObjects->Volume,
			(PFLT_CONTEXT *)&pvcVolumeContext
			);

		if(!NT_SUCCESS(status))
		{
			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO,
				"PreDirectoryControl",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("No volume context find.Reguarded as not confidential file.")
				);
			break;
		}

		//
		//如果未加密 直接返回
		//
		if(FctGetFileConfidentialCondition(pscFileStreamContext) != ENCRYPTED_TYPE_CONFIDENTIAL){		

			DebugTraceFileAndProcess(
				DEBUG_TRACE_IMPORTANT_INFO,
				"PreDirectoryControl",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("Confidential proccess read an not confidential file.File enctype %d",
					FctGetFileConfidentialCondition(pscFileStreamContext))
				);

			break;
		}*/
/*
		//
		//禁止 FAST IO
		//
		if (FLT_IS_FASTIO_OPERATION(pfcdCBD)){

			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO,
				"PreDirectoryControl",
				FILE_OBJECT_NAME_BUFFER(pfoFileObject),
				("Disallow fast io.")
				);
			break;
		}

		//
		//如果是IRP操作 那么交换缓冲区
		//
        if (FlagOn(pfcdCBD->Flags,FLTFL_CALLBACK_DATA_IRP_OPERATION)){

			bReturn = AllocateAndSwapToNewMdlBuffer(
				pIoParameterBlock,
				pvcVolumeContext,
				(PULONG)lpCompletionContext,
				NULL,
				NULL,
				NULL,
				Allocate_BufferDirectoryControl
				);

			if(!bReturn){
				DebugTraceFileAndProcess(
					DEBUG_TRACE_ERROR|DEBUG_TRACE_CONFIDENTIAL,
					"PreDirectoryControl",
					FILE_OBJECT_NAME_BUFFER(pfoFileObject),
					("Error:Cannot swap buffer.")
					);
				break;
			}*/
		//}
		DebugTraceFileAndProcess(
			DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
			"PreDirectoryControl",
			FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
			("An directory control has been lunched.")
			);
        fcsReturn = FLT_PREOP_SUCCESS_NO_CALLBACK;
    } while(0);

	if (pscFileStreamContext != NULL){
		FctReleaseStreamContext(pscFileStreamContext) ;
	}

	if (pvcVolumeContext != NULL){
		FltReleaseContext(pvcVolumeContext) ;
	}

    return fcsReturn ;


	return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

/*---------------------------------------------------------
函数名称:	Antinvader_PostDirectoryControl
函数描述:	后请求回调IRP_MJ_DIRECTORY_CONTROL
输入参数:
			pfcdCBD				回调数据
			pFltObjects			文件对象
			lpCompletionContext	完成请求的上下文
			Flags				回调发生的标志(原因)
输出参数:
返回值:		
			
其他:		
更新维护:	2011.3.20    最初版本
---------------------------------------------------------*/
FLT_POSTOP_CALLBACK_STATUS
Antinvader_PostDirectoryControl (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in_opt PVOID lpCompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    )
{	
	//返回状态
	NTSTATUS status;

	//I/O参数块,包含IRP相关信息
	PFLT_IO_PARAMETER_BLOCK  pIoParameterBlock;

	//文件信息类别
	FILE_INFORMATION_CLASS	ficFileInformation;
	
	//文件信息
	PVOID	pFileInformation;

	//数据长度
	ULONG ulLength;
	
	//流文件上下文
	PFILE_STREAM_CONTEXT pscFileStreamContext = NULL;

	//文件对象
	PFILE_OBJECT	pfoFileObject;
	
	//交换了的缓存
	ULONG ulSwappedBuffer = NULL;

	//原来的缓冲(需要我们把数据考进去的)
	ULONG ulBuffer;

	//用于保存WhenSafe的状态
	FLT_POSTOP_CALLBACK_STATUS fcsStatus = FLT_POSTOP_FINISHED_PROCESSING;

	//
	//保存Iopb和文件信息指针 上下文等
	//
	pIoParameterBlock = pfcdCBD -> Iopb;

	ficFileInformation = pIoParameterBlock->
		Parameters.DirectoryControl.QueryDirectory.FileInformationClass;

	pFileInformation = pIoParameterBlock->
		Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer;

	ulLength = pIoParameterBlock->
		Parameters.DirectoryControl.QueryDirectory.Length;

	ulSwappedBuffer = (ULONG)lpCompletionContext;

	pfoFileObject = pFltObjects->FileObject;

	DebugTraceFileAndProcess(
		DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
		"PostDirectoryControl",
		FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
		("PostDirectoryControl enterd.FileInformation %d,Swapped buffer 0x%X",ficFileInformation,lpCompletionContext)
		);
	//
	//获取文件流上下文
	//
	/*status = FctGetSpecifiedFileStreamContext(
		pFltObjects->Instance,
		pFltObjects->FileObject,
		&pscFileStreamContext
		);

	if(!NT_SUCCESS(status))	{
		DebugTraceFileAndProcess(
			DEBUG_TRACE_ERROR|DEBUG_TRACE_CONFIDENTIAL,
			"PostDirectoryControl",
			FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
			("Error:Cannot get file context in post opration.")
			);
		pscFileStreamContext = NULL;
		ASSERT(FALSE);
		return FLT_POSTOP_FINISHED_PROCESSING;
	}
*/
	do{

        if (!NT_SUCCESS(pfcdCBD->IoStatus.Status) || 
			(pfcdCBD->IoStatus.Information == 0)) {
			break;
		}

		//
        // 跟Read的时候一样,我们需要把数据拷贝回原来的缓冲
		// 参数里面的缓冲地址是原来的缓冲
		//
        if (pIoParameterBlock->Parameters.DirectoryControl.QueryDirectory.MdlAddress != NULL){
		
			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
				"PostDirectoryControl",
				FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
				("Using mdl buffer.")
				);

            ulBuffer = (ULONG)MmGetSystemAddressForMdlSafe( 
				pIoParameterBlock->Parameters.DirectoryControl.QueryDirectory.MdlAddress,
				NormalPagePriority 
				);

            if (!ulBuffer){
				DebugTraceFileAndProcess(
					DEBUG_TRACE_ERROR|DEBUG_TRACE_CONFIDENTIAL,
					"PostDirectoryControl",
					FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
					("Error:Cannot get mdl buffer.")
					);

                pfcdCBD->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
                pfcdCBD->IoStatus.Information = 0;
                break;
            }
        }
		else if (FlagOn(pfcdCBD->Flags,FLTFL_CALLBACK_DATA_SYSTEM_BUFFER) || 
			FlagOn(pfcdCBD->Flags,FLTFL_CALLBACK_DATA_FAST_IO_OPERATION)){
			
			DebugTraceFileAndProcess(
				DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
				"PostDirectoryControl",
				FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
				("Using system buffer.")
				);

            ulBuffer = (ULONG)pIoParameterBlock->
				Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer;
        } 
		else 
		{
			//
			//由于WhenSafe里面会再次获取文件流上下文
			//这里先释放掉防止死锁
			//
//			FctReleaseStreamContext(pscFileStreamContext) ;
//			pscFileStreamContext = NULL;

            if (FltDoCompletionProcessingWhenSafe( 
				pfcdCBD,
				pFltObjects,
				lpCompletionContext,Flags,
				Antinvader_PostDirectoryControlWhenSafe,
				&fcsStatus )){
				//
				//不用在这里释放SwappedBuffer
				//
                ulSwappedBuffer = NULL;
            }else{
                pfcdCBD->IoStatus.Status = STATUS_UNSUCCESSFUL;
                pfcdCBD->IoStatus.Information = 0;
            }
            break;
        }

        //
        //  我们这里是一个系统缓冲或者是FastIo(已经禁止了),现在考数据并且处理异常
		//
		//注意:由于一个FASTFAT的Bug,会返回一个错误的长度,所以我们用
		//Parameters.DirectoryControl.QueryDirectory.Length里面的长度
		//
		if(ulSwappedBuffer){

			__try{
			
				RtlCopyMemory( (PVOID)ulBuffer,
							  (PVOID)ulSwappedBuffer,
							   /*Data->IoStatus.Information*/
							   pIoParameterBlock->Parameters.DirectoryControl.QueryDirectory.Length );

			} __except (EXCEPTION_EXECUTE_HANDLER) {
				pfcdCBD->IoStatus.Status = GetExceptionCode();
				pfcdCBD->IoStatus.Information = 0;
				DebugTraceFileAndProcess(
					DEBUG_TRACE_ERROR|DEBUG_TRACE_CONFIDENTIAL,
					"PostDirectoryControl",
					FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
					("Error:Error occurred when copy data back.0x%X.",pfcdCBD->IoStatus.Status)
					);
				break;
			}
			

		}
	/*
		DebugTraceFileAndProcess(
			DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
			"PostDirectoryControl",
			FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
			("Swap finished FileInformationClass %d.",ficFileInformation)
			);*/
	}while(0);

			
	//
	//收尾工作
	//

	if(ulSwappedBuffer){
		FreeAllocatedMdlBuffer(ulSwappedBuffer,Allocate_BufferDirectoryControl);
	}

	if (pscFileStreamContext){
		FctReleaseStreamContext(pscFileStreamContext) ;
	}
    
	return fcsStatus;//STATUS_SUCCESS;
}

/*---------------------------------------------------------
函数名称:	Antinvader_PostDirectoryControl
函数描述:	安全时DirectoryControl后回调
输入参数:
			pfcdCBD				回调数据
			pFltObjects			文件对象
			lpCompletionContext	完成请求的上下文
			Flags				回调发生的标志(原因)
输出参数:
返回值:		
			
其他:		
更新维护:	2011.7.20    最初版本
---------------------------------------------------------*/
FLT_POSTOP_CALLBACK_STATUS
Antinvader_PostDirectoryControlWhenSafe (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in_opt PVOID lpCompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    )
{
	//返回值
	NTSTATUS status;

	//I/O参数块,包含IRP相关信息
	PFLT_IO_PARAMETER_BLOCK  pIoParameterBlock;

	//文件信息类别
	FILE_INFORMATION_CLASS	ficFileInformation;
	
	//文件信息
	PVOID	pFileInformation;

	//数据长度
	ULONG ulLength;
	
	//流文件上下文
	PFILE_STREAM_CONTEXT pscFileStreamContext = NULL;

	//文件对象
	PFILE_OBJECT	pfoFileObject;
	
	//交换了的缓存
	ULONG ulSwappedBuffer = NULL;

	//原来的缓冲(需要我们把数据考进去的)
	ULONG ulBuffer;

	//
	//锁定(创建)MDL,这样我们才能访问
	//
	status = FltLockUserBuffer( pfcdCBD );


	//
	//保存Iopb和文件信息指针 上下文等
	//
	pIoParameterBlock = pfcdCBD -> Iopb;

	ficFileInformation = pIoParameterBlock->
		Parameters.DirectoryControl.QueryDirectory.FileInformationClass;

	pFileInformation = pIoParameterBlock->
		Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer;

	ulLength = pIoParameterBlock->
		Parameters.DirectoryControl.QueryDirectory.Length;

	ulSwappedBuffer = (ULONG)lpCompletionContext;

	pfoFileObject = pFltObjects->FileObject;

	DebugTraceFileAndProcess(
		DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
		"PostDirectoryControlWhenSafe",
		FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
		("PostDirectoryControlWhenSafe enterd.FileInformation %d Swapped buffer 0x%X",ficFileInformation,lpCompletionContext)
		);

	//
	//获取文件流上下文
	//
/*	status = FctGetSpecifiedFileStreamContext(
		pFltObjects->Instance,
		pFltObjects->FileObject,
		&pscFileStreamContext
		);

	if(!NT_SUCCESS(status))	{
		DebugTraceFileAndProcess(
			DEBUG_TRACE_ERROR|DEBUG_TRACE_CONFIDENTIAL,
			"PostDirectoryControlWhenSafe",
			FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
			("Error:Cannot get file context in post opration.")
			);
		pscFileStreamContext = NULL;
		ASSERT(FALSE);
		return FLT_POSTOP_FINISHED_PROCESSING;
	}*/

	do{
        if (!NT_SUCCESS(pfcdCBD->IoStatus.Status) || 
			(pfcdCBD->IoStatus.Information == 0)) {
			break;
		}
	
		//
		//获取原始数据地址
		//
		ulBuffer = (ULONG)MmGetSystemAddressForMdlSafe( 
			pIoParameterBlock->Parameters.DirectoryControl.QueryDirectory.MdlAddress,
			NormalPagePriority 
			);

		if (!ulBuffer){
		DebugTraceFileAndProcess(
			DEBUG_TRACE_ERROR|DEBUG_TRACE_CONFIDENTIAL,
			"PostDirectoryControlWhenSafe",
			FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
			("Error:Cannot get mdl buffer.")
			);

			pfcdCBD->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
			pfcdCBD->IoStatus.Information = 0;
			break;
		}

		if(ulSwappedBuffer){

			__try{
			
				//
				//同PostDirectoryControl 拷贝数据长度用
				//Parameters.DirectoryControl.QueryDirectory.Length 
				//
				RtlCopyMemory( (PVOID)ulBuffer,
							  (PVOID)ulSwappedBuffer,
							   /*Data->IoStatus.Information*/
							   pIoParameterBlock->Parameters.DirectoryControl.QueryDirectory.Length );

			} __except (EXCEPTION_EXECUTE_HANDLER) {

				pfcdCBD->IoStatus.Status = GetExceptionCode();
				pfcdCBD->IoStatus.Information = 0;
				DebugTraceFileAndProcess(
					DEBUG_TRACE_ERROR|DEBUG_TRACE_CONFIDENTIAL,
					"PostDirectoryControl",
					FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
					("Error:Error occurred when copy data back.0x%X.",pfcdCBD->IoStatus.Status)
					);
				break;
			}

		}
	/*	DebugTraceFileAndProcess(
			DEBUG_TRACE_NORMAL_INFO|DEBUG_TRACE_CONFIDENTIAL,
			"PostDirectoryControlWhenSafe",
			FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
			("Swap finished FileInformationClass %d.",ficFileInformation)
			);*/
	}while(0);
			

	//
	//收尾工作
	//

	if(ulSwappedBuffer){
		FreeAllocatedMdlBuffer(ulSwappedBuffer,Allocate_BufferDirectoryControl);
	}

	if (pscFileStreamContext){
		FctReleaseStreamContext(pscFileStreamContext) ;
	}
    
	return FLT_POSTOP_FINISHED_PROCESSING;
}


/*---------------------------------------------------------
函数名称:	Antinvader_PreCleanUp
函数描述:	预请求回调IRP_MJ_CLEANUP
输入参数:
			pfcdCBD				回调数据
			pFltObjects			文件对象
			lpCompletionContext	完成请求的上下文
输出参数:
返回值:		
			
其他:		
更新维护:	2011.3.20    最初版本
			2012.1.4     增加了在这里刷缓存的操作 很重要
							   不刷Write的东西就不能被正常加密了
---------------------------------------------------------*/
FLT_PREOP_CALLBACK_STATUS
Antinvader_PreCleanUp (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __deref_out_opt PVOID *lpCompletionContext
    )
{	
	//
	//确保IRQL<=APC_LEVEL
	//
	PAGED_CODE();

	//返回值
	NTSTATUS status; 

	//卷上下文
	PVOLUME_CONTEXT	pvcVolumeContext;

	//文件名信息
	PFLT_FILE_NAME_INFORMATION pfniFileNameInformation = NULL;

	//是否是文件夹
	BOOLEAN bDirectory;

	//文件流上下文
	PFILE_STREAM_CONTEXT pscFileStreamContext = NULL;

	//
	//检查是否是机密进程
	//
	

	do{
	
		//
		//获取卷上下文
		//
		status = FltGetVolumeContext(
			pFltObjects->Filter,
			pFltObjects->Volume,
			(PFLT_CONTEXT *)&pvcVolumeContext
			);

		if (!NT_SUCCESS(status) 
			|| (NULL == pvcVolumeContext)){

			pscFileStreamContext = NULL;

			DebugTraceFileAndProcess(
					DEBUG_TRACE_NORMAL_INFO,
					"PreCleanUp",
					FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
					("No volume context was found.")
					);

			break ;
		}
/*
		//
		//获取文件流上下文
		//
		status = FctGetSpecifiedFileStreamContext(
			pFltObjects->Instance,
			pFltObjects->FileObject,
			&pscFileStreamContext
			);

		if (!NT_SUCCESS(status) ){
			pscFileStreamContext = NULL;
			__leave ;
		}

		if(pscFileStreamContext->fctEncrypted == ENCRYPTED_TYPE_NOT_CONFIDENTIAL){		
			__leave;
		}
*/
		//
		//获取文件名称信息
		//
		status = FltGetFileNameInformation(
			pfcdCBD, 
			FLT_FILE_NAME_NORMALIZED|FLT_FILE_NAME_QUERY_DEFAULT, 
			&pfniFileNameInformation
			);

		if (!NT_SUCCESS(status)){
			DebugTraceFileAndProcess(
					DEBUG_TRACE_ERROR,
					"PreCleanUp",
					FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
					("Error:Cannot get file name information.")
					);
			pfniFileNameInformation = NULL;
			break ;
		}

		if (pfniFileNameInformation->Name.Length != 0){
			//
			//获取基本信息 看看是不是文件夹
			//
			status = FileGetStandardInformation(
				pFltObjects->Instance,
				pFltObjects->FileObject,
				NULL,
				NULL,
				&bDirectory
				) ;

			if (!NT_SUCCESS(status)){

				DebugTraceFileAndProcess(
					DEBUG_TRACE_NORMAL_INFO,
					"PreCleanUp",
					FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
					("Cannot get file information.")
					);

				break ;
			}

			if (bDirectory){

				DebugTraceFileAndProcess(
					DEBUG_TRACE_NORMAL_INFO,
					"PreCleanUp",
					FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
					("Dictory just pass.")
					);

				break ;
			}

		//
		//由于是CleanUp,都要刷缓存
		//
		FileClearCache(pFltObjects->FileObject);

		}
	}while(0);

	if (pvcVolumeContext != NULL){
		FltReleaseContext(pvcVolumeContext) ;
	}

	if (pfniFileNameInformation != NULL){
		FltReleaseFileNameInformation(pfniFileNameInformation);
	}	
	 
	return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

/*---------------------------------------------------------
函数名称:	Antinvader_PostCleanUp
函数描述:	后请求回调IRP_MJ_CLEANUP
输入参数:
			pfcdCBD				回调数据
			pFltObjects			文件对象
			lpCompletionContext	完成请求的上下文
			Flags				回调发生的标志(原因)
输出参数:
返回值:		
			
其他:		
更新维护:	2011.3.20    最初版本
			2011.7.17    增加了缓存释放
---------------------------------------------------------*/
FLT_POSTOP_CALLBACK_STATUS
Antinvader_PostCleanUp (
    __inout PFLT_CALLBACK_DATA pfcdCBD,
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in_opt PVOID lpCompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    )
{	
    KdPrint(("[Antinvader]Antinvader_PostCleanUp: Entered\n") );

	//
	//确保IRQL<=APC_LEVEL
	//
	PAGED_CODE();

	//Io操作参数块
	PFLT_IO_PARAMETER_BLOCK pIoParameterBlock;

	//打开的文件对象
	PFILE_OBJECT pfoFileObject;

	//Hash表中地址
	PHASH_NOTE_DESCRIPTOR pndFileNoteDescriptor;

	//返回值
	BOOLEAN bReturn;
	
	//
	//确保IRQL<=APC_LEVEL
	//
	PAGED_CODE();
/*
	//
	//获取需要的参数
	//
	pIoParameterBlock = pfcdCBD -> Iopb;
	pfoFileObject = pFltObjects -> FileObject;
	

	//返回值
	NTSTATUS status; 

	//卷上下文
	PVOLUME_CONTEXT	pvcVolumeContext;

	//文件名信息
	PFLT_FILE_NAME_INFORMATION pfniFileNameInformation = NULL;

	//是否是文件夹
	BOOLEAN bDirectory;

	//文件流上下文
	PFILE_STREAM_CONTEXT pscFileStreamContext = NULL;

	//
	//检查是否是机密进程
	//
	

	do{
	
		//
		//获取卷上下文
		//
		status = FltGetVolumeContext(
			pFltObjects->Filter,
			pFltObjects->Volume,
			(PFLT_CONTEXT *)&pvcVolumeContext
			);

		if (!NT_SUCCESS(status) 
			|| (NULL == pvcVolumeContext)){

			pscFileStreamContext = NULL;

			DebugTraceFileAndProcess(
					DEBUG_TRACE_NORMAL_INFO,
					"PreCleanUp",
					FILE_OBJECT_NAME_BUFFER(pFltObjects->FileObject),
					("No volume context was found.")
					);

			break ;
		}

		//
		//获取文件流上下文
		//
		status = FctGetSpecifiedFileStreamContext(
			pFltObjects->Instance,
			pFltObjects->FileObject,
			&pscFileStreamContext
			);

		if (!NT_SUCCESS(status) ){
			pscFileStreamContext = NULL;
			break ;
		}

		if(pscFileStreamContext->fctEncrypted == ENCRYPTED_TYPE_NOT_CONFIDENTIAL){		
			break;
		}

	}while(0);

	if (pvcVolumeContext != NULL){
		FltReleaseContext(pvcVolumeContext) ;
	}
	
	if (pscFileStreamContext != NULL){
		FctReleaseStreamContext(pscFileStreamContext) ;
	}
	 */
	return FLT_POSTOP_FINISHED_PROCESSING;//STATUS_SUCCESS;
}

/////////////////////////////////////
//	   一般驱动回调例程
////////////////////////////////////
/*---------------------------------------------------------
函数名称:	Antinvader_InstanceSetup
函数描述:	启动过滤实例
输入参数:
			pFltObjects				过滤驱动对象
			Flags					启动标志
			VolumeDeviceType		磁盘卷类型
			VolumeFilesystemType	卷文件系统类型
输出参数:
返回值:		
			STATUS_SUCCESS 成功
其他:		
更新维护:	2011.3.20    最初版本
			2011.7.27    增加了卷上下文设置
---------------------------------------------------------*/
NTSTATUS
Antinvader_InstanceSetup (
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in FLT_INSTANCE_SETUP_FLAGS Flags,
    __in DEVICE_TYPE VolumeDeviceType,
    __in FLT_FILESYSTEM_TYPE VolumeFilesystemType
    )
{
//    UNREFERENCED_PARAMETER( pFltObjects );
//    UNREFERENCED_PARAMETER( Flags );
//    UNREFERENCED_PARAMETER( VolumeDeviceType );
//    UNREFERENCED_PARAMETER( VolumeFilesystemType );

    PAGED_CODE();

    KdPrint(("[Antinvader]Antinvader_InstanceSetup: Entered\n") );

	//用于存储卷信息的缓冲区
    UCHAR szVolumePropertiesBuffer[sizeof(FLT_VOLUME_PROPERTIES)+512];

	//卷信息指针
    PFLT_VOLUME_PROPERTIES pvpVolumeProperties = (PFLT_VOLUME_PROPERTIES)szVolumePropertiesBuffer;

	//返回值
	NTSTATUS status;

	//卷上下文
	PVOLUME_CONTEXT pvcVolumeContext = NULL;

	//设备对象
	PDEVICE_OBJECT pdoDeviceObject = NULL;

	//返回的长度
	ULONG ulReturn;

	//能用的名称指针
	PUNICODE_STRING pusWorkingName;

	do{
		//
		//将要Attach到一个卷上了,分配一个上下文
		//
		status = FltAllocateContext(
			pFltObjects->Filter,
			FLT_VOLUME_CONTEXT,
			sizeof(VOLUME_CONTEXT),
			NonPagedPool,
			(PFLT_CONTEXT *)&pvcVolumeContext
			);

		if(!NT_SUCCESS(status))	{
			break;
		}

		//
		//获取卷属性 找到需要的数据
		//
		status = FltGetVolumeProperties(
			pFltObjects->Volume,
			pvpVolumeProperties,
			sizeof(szVolumePropertiesBuffer),
			&ulReturn
			);

		if(!NT_SUCCESS(status)){
			break;
		}

		//
		//保存扇区大小
		//
		pvcVolumeContext->ulSectorSize 
			= pvpVolumeProperties -> SectorSize;

		pvcVolumeContext->uniName.Buffer = NULL;

		pvcVolumeContext->pnliReadEncryptedSignLookasideList = NULL;

		//
		//获取设备对象
		//
		status = FltGetDiskDeviceObject(
			pFltObjects->Volume,
			&pdoDeviceObject
			);

		//
		//如果成功了 尝试获取Dos名称
		//
		if (NT_SUCCESS(status))
		{
			status = RtlVolumeDeviceToDosName(
				pdoDeviceObject,
				&pvcVolumeContext->uniName
				); 
		}

		//
		//拿不到Dos名称 那就拿NT名称吧
		//
		if (!NT_SUCCESS(status)){
			//
            //看看哪个名字能用
            //
			if (pvpVolumeProperties->RealDeviceName.Length > 0) {
                pusWorkingName = &pvpVolumeProperties->RealDeviceName;
            }
			else if (pvpVolumeProperties->FileSystemDeviceName.Length > 0) 	{
                pusWorkingName = &pvpVolumeProperties->FileSystemDeviceName;
            } else {
				//
				//没有可用名称 这种设备就不挂接了
				//
                status = STATUS_FLT_DO_NOT_ATTACH;  
                break;
            }
		
            pvcVolumeContext->uniName.Buffer 
				= (PWCH)ExAllocatePoolWithTag(
					NonPagedPool,
					pusWorkingName->Length + sizeof(WCHAR),//加上一个连接符":"的长度
					MEM_CALLBACK_TAG
					);

			//
			//失败了.....
			//
            if (pvcVolumeContext->uniName.Buffer == NULL) {
                status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }

            pvcVolumeContext->uniName.Length = 0;
            pvcVolumeContext->uniName.MaximumLength = pusWorkingName->Length + sizeof(WCHAR);

			//
			//把名字复制过来 并且加上":"
			//
            RtlCopyUnicodeString( &pvcVolumeContext->uniName, pusWorkingName);
            RtlAppendUnicodeToString( &pvcVolumeContext->uniName, L":");
		}
	
		//
		//在读取文件判断是否是机密文件时需要使用NonCache读取,至少是一个SectorSize
		//由于开销较大还是决定用旁视链表
		//
		pvcVolumeContext->pnliReadEncryptedSignLookasideList	= 
			(PNPAGED_LOOKASIDE_LIST)ExAllocatePoolWithTag(
					NonPagedPool,
					sizeof(NPAGED_LOOKASIDE_LIST),
					MEM_FILE_TAG);

		ExInitializeNPagedLookasideList(
			pvcVolumeContext->pnliReadEncryptedSignLookasideList,
			NULL,
			NULL,
			0,
			sizeof(pvcVolumeContext->ulSectorSize),
			MEM_FILE_TAG,
			0
			);

		//
		//这里如果有需要可以初始化一些其他东西
		//
		

		//
		//设置上下文
		//
		status = FltSetVolumeContext(
			pFltObjects->Volume,
			FLT_SET_CONTEXT_KEEP_IF_EXISTS,
			pvcVolumeContext,
			NULL
			);

		if (status == STATUS_FLT_CONTEXT_ALREADY_DEFINED) {
			//
			//如果已经设置过上下文了 没关系
			//
			status = STATUS_SUCCESS;
		}

	}while(0);

	if (pvcVolumeContext) {
		//
		//释放上下文 如果不释放系统会挂起
		//
		FltReleaseContext( pvcVolumeContext );
	}

	if (pdoDeviceObject) {
		//
		//释放设备对象
		//
		ObDereferenceObject(pdoDeviceObject);	
	}
	

    return status;
}

/*---------------------------------------------------------
函数名称:	Antinvader_InstanceQueryTeardown
函数描述:	销毁查询实例
输入参数:
			pFltObjects				过滤驱动对象
			Flags					查询实例标志
输出参数:
返回值:		
			STATUS_SUCCESS 成功
其他:		暂时未添加任何功能
更新维护:	2011.3.20    最初版本
---------------------------------------------------------*/
NTSTATUS
Antinvader_InstanceQueryTeardown (
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in FLT_INSTANCE_QUERY_TEARDOWN_FLAGS Flags
    )
{
    UNREFERENCED_PARAMETER( pFltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    KdPrint(("[Antinvader]Antinvader_InstanceQueryTeardown: Entered\n"));

    return STATUS_SUCCESS;
}

/*---------------------------------------------------------
函数名称:	Antinvader_InstanceTeardownStart
函数描述:	开始销毁过滤实例回调
输入参数:
			pFltObjects				过滤驱动对象
			Flags					查询实例标志
输出参数:
返回值:		
			
其他:		暂时未添加任何功能
更新维护:	2011.3.20    最初版本
---------------------------------------------------------*/
VOID
Antinvader_InstanceTeardownStart (
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in FLT_INSTANCE_TEARDOWN_FLAGS Flags
    )
{
    UNREFERENCED_PARAMETER( pFltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    KdPrint(("[Antinvader]Antinvader_InstanceTeardownStart: Entered\n") );
}

/*---------------------------------------------------------
函数名称:	Antinvader_InstanceTeardownComplete
函数描述:	完成销毁过滤实例回调
输入参数:
			pFltObjects	过滤驱动对象
			Flags		实例销毁标志
输出参数:
返回值:		
			
其他:		暂时未添加任何功能
更新维护:	2011.3.20    最初版本
---------------------------------------------------------*/
VOID
Antinvader_InstanceTeardownComplete (
    __in PCFLT_RELATED_OBJECTS pFltObjects,
    __in FLT_INSTANCE_TEARDOWN_FLAGS Flags
    )
{
    UNREFERENCED_PARAMETER( pFltObjects );
    UNREFERENCED_PARAMETER( Flags );

    PAGED_CODE();

    KdPrint(("[Antinvader]Antinvader_InstanceTeardownComplete: Entered\n") );
}

/*---------------------------------------------------------
函数名称:	Antinvader_CleanupContext
函数描述:	清理上下文
输入参数:
			pcContext		上下文内容
			pctContextType	上下文类型
输出参数:
返回值:		
			
其他:		
更新维护:	2011.7.27    最初版本
			2011.7.29    Bug:指针传错 已经修复
---------------------------------------------------------*/
VOID
Antinvader_CleanupContext(
    __in PFLT_CONTEXT pcContext,
    __in FLT_CONTEXT_TYPE pctContextType
    )
{
    PVOLUME_CONTEXT pvcVolumeContext = NULL ;
	PFILE_STREAM_CONTEXT pscFileStreamContext = NULL ;

    PAGED_CODE();

	switch(pctContextType)
	{
	case FLT_VOLUME_CONTEXT:
		{
			pvcVolumeContext = (PVOLUME_CONTEXT)pcContext ;

			if (pvcVolumeContext->uniName.Buffer != NULL) 	{
				ExFreePool(pvcVolumeContext->uniName.Buffer);
				pvcVolumeContext->uniName.Buffer = NULL;
			}

			if(pvcVolumeContext->pnliReadEncryptedSignLookasideList){

				ExDeleteNPagedLookasideList(pvcVolumeContext->pnliReadEncryptedSignLookasideList);

				ExFreePool(pvcVolumeContext->pnliReadEncryptedSignLookasideList);

				pvcVolumeContext->pnliReadEncryptedSignLookasideList = NULL;

			}
		}
	    break ;
	case FLT_STREAM_CONTEXT:
		{
			KIRQL OldIrql ;

			pscFileStreamContext = (PFILE_STREAM_CONTEXT)pcContext ;

			//
			//释放上下文
			//

			FctFreeStreamContext(pscFileStreamContext);
		}
		break ;	
	}
}

///////////////////////////////
//		通信回调例程
///////////////////////////////

/*---------------------------------------------------------
函数名称:	Antinvader_Connect
函数描述:	Ring3连接时回调该函数
输入参数:
			ClientPort			客户端端口
			ServerPortCookie	服务端口上下文
			ConnectionContext	连接端上下文
			SizeOfContext		连接端上下文大小
			ConnectionCookie	连接Cookie
输出参数:
返回值:		
			STATUS_SUCCESS 成功
其他:		
更新维护:	2011.3.20    最初版本
---------------------------------------------------------*/

NTSTATUS
Antinvader_Connect(
    __in PFLT_PORT ClientPort,
    __in PVOID ServerPortCookie,
    __in_bcount(SizeOfContext) PVOID ConnectionContext,
    __in ULONG SizeOfContext,
    __deref_out_opt PVOID *ConnectionCookie
    )
{
	KdPrint(("[Antinvader]Antinvader_Connect: Entered.\n"));
    PAGED_CODE();

    UNREFERENCED_PARAMETER( ServerPortCookie );
    UNREFERENCED_PARAMETER( ConnectionContext );
    UNREFERENCED_PARAMETER( SizeOfContext );
    UNREFERENCED_PARAMETER( ConnectionCookie );

//  ASSERT( gClientPort == NULL );

    pfpGlobalClientPort = ClientPort;

    return STATUS_SUCCESS;
}

/*---------------------------------------------------------
函数名称:	Antinvader_Disconnect
函数描述:	Ring3断开连接时回调该函数
输入参数:
			ConnectionCookie	连接Cookie
输出参数:
返回值:		
			
其他:		
更新维护:	2011.3.20    最初版本
---------------------------------------------------------*/
VOID
Antinvader_Disconnect(
    __in_opt PVOID ConnectionCookie
   )
{		
    PAGED_CODE();
    UNREFERENCED_PARAMETER( ConnectionCookie );
	KdPrint(("[Antinvader]Antinvader_Disconnect: Entered\n"));
    
    // 关闭句柄
    FltCloseClientPort( pfltGlobalFilterHandle, &pfpGlobalClientPort );

	pfpGlobalClientPort = NULL;

}

/*---------------------------------------------------------
函数名称:	Antinvader_Disconnect
函数描述:	Ring3消息处理函数
输入参数:
			ConnectionCookie	连接Cookie
			InputBuffer			传入的数据
			InputBufferSize		传入数据大小
			OutputBufferSize	传出数据缓冲大小

输出参数:
			OutputBuffer				传出的数据
			ReturnOutputBufferLength	实际传出数据大小
返回值:		
			STATUS_SUCCESS 成功
其他:		
更新维护:	2011.3.20    最初版本
---------------------------------------------------------*/
NTSTATUS
Antinvader_Message (
    __in PVOID ConnectionCookie,
    __in_bcount_opt(InputBufferSize) PVOID InputBuffer,
    __in ULONG InputBufferSize,
    __out_bcount_part_opt(OutputBufferSize,*ReturnOutputBufferLength) PVOID OutputBuffer,
    __in ULONG OutputBufferSize,
    __out PULONG ReturnOutputBufferLength
    )
{
	//命令
    ANTINVADER_COMMAND acCommand;

	//返回值
    NTSTATUS status;

	//进程信息
	CONFIDENTIAL_PROCESS_DATA cpdProcessData;

	//拆包用临时指针
	PCWSTR  pcwString;

	//回复的数据
	COMMAND_MESSAGE replyMessage;

	//返回值
	BOOLEAN bReturn;

	replyMessage.lSize = sizeof(COMMAND_MESSAGE);
	replyMessage.acCommand = ENUM_OPERATION_FAILED;

    PAGED_CODE();

    UNREFERENCED_PARAMETER( ConnectionCookie );

	//
	//	输入和输出的缓冲区是用户模式的原始地址.
	//	过滤管理器已经做了ProbedForRead(InputBuffer)
	//	和ProbedForWrite(OutputBuffer),从而保证它们
	//	基于地址访问是有效的(用户模式与内核模式),微
	//	过滤驱动并不用检查.但过滤管理器没有做任何调
	//	整的指针检查.如果要使用指针操作的话,微过滤器
	//	必须继续使用结构化异常处理保护访问缓冲区.
	//

	//检查输入数据是否正确
 /*   if ((InputBuffer != NULL) &&
        (InputBufferSize >= (FIELD_OFFSET(COMMAND_MESSAGE,Command) +
        sizeof(ANTINVADER_COMMAND)))) {*/
	 if (((InputBuffer != NULL) &&
		  (InputBufferSize == ((PCOMMAND_MESSAGE) InputBuffer)->lSize))&&
		  (OutputBufferSize>=sizeof(COMMAND_MESSAGE)))
	 {
        __try  {
            //消息是用户模式缓冲区,所以需要结构化异常处理保护
			
			//保存命令
            acCommand = ((PCOMMAND_MESSAGE) InputBuffer)->acCommand;

			//按照命令执行
			switch (acCommand) {
				case ENUM_COMMAND_PASS:
					break;
				case ENUM_BLOCK:
					break;
				case ENUM_ADD_PROCESS:
				case ENUM_DELETE_PROCESS:
					//
					//先拆包 length已经是字节长度,不需要乘sizeof修正
					//
					pcwString = (PCWSTR)((ULONG)InputBuffer+sizeof(COMMAND_MESSAGE));
					RtlInitUnicodeString(&cpdProcessData.usName,pcwString);

					pcwString = (PCWSTR)((ULONG)pcwString+(cpdProcessData.usName.Length+sizeof(WCHAR)) );
					RtlInitUnicodeString(&cpdProcessData.usPath,pcwString);

					pcwString = (PCWSTR)((ULONG)pcwString+(cpdProcessData.usPath.Length+sizeof(WCHAR)));
					RtlInitUnicodeString(&cpdProcessData.usMd5Digest,pcwString);

					KdPrint(("[Antinvader]Process Name:%ws\n\t\tProcess Path %ws\n\t\tProcess MD5 %ws\n",
						cpdProcessData.usName.Buffer,cpdProcessData.usPath.Buffer,cpdProcessData.usMd5Digest.Buffer));
					
					//
					//判断是删除还是别的 执行操作
					//
					bReturn = ((acCommand==ENUM_ADD_PROCESS)?
						PctAddProcess(&cpdProcessData):PctDeleteProcess(&cpdProcessData));

					if(bReturn){
						status = STATUS_SUCCESS;
						replyMessage.acCommand = ENUM_OPERATION_SUCCESSFUL;
					}else{
						status = STATUS_UNSUCCESSFUL;
					}
					break;
				default:
					KdPrint(("[Antinvader]Antinvader_Message: default\n"));
					status = STATUS_INVALID_PARAMETER;
					break;
			}
        }__except( EXCEPTION_EXECUTE_HANDLER ){
			//如果出错了 返回错误值
            return GetExceptionCode();
        }
    } else {
		//如果数据长度不对
        status = STATUS_INVALID_PARAMETER;
    }

	//
	//返回数据
	//
	RtlCopyMemory(OutputBuffer,&replyMessage,sizeof(replyMessage.lSize));
	*ReturnOutputBufferLength = replyMessage.lSize;

    return status;
}