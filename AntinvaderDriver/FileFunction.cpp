///////////////////////////////////////////////////////////////////////////////
///
/// 版权所有 (c) 2011 - 2012
///
/// 原始文件名称	 : FileFunction.cpp
/// 工程名称         : AntinvaderDriver
/// 创建时间         : 2011-03-26
/// 	         
///
/// 描述	         : 关于文件信息的功能实现
///
/// 更新维护:
///  0000 [2011-03-26] 最初版本.
///
///////////////////////////////////////////////////////////////////////////////

#include "FileFunction.h"

/*---------------------------------------------------------
函数名称:	FileSetSize
函数描述:	非重入设置文件大小
输入参数:
			pfiInstance			过滤器实例
			pfoFileObject		文件对象
			pnFileSize			文件大小
输出参数:
返回值:		
			STATUS_SUCCESS 成功	否则返回相应状态

其他:		

更新维护:	2011.4.3    最初版本 使用IRP
			2011.4.9 	  修改为使用FltXXX版本
---------------------------------------------------------*/
NTSTATUS
FileSetSize(
	__in PFLT_INSTANCE pfiInstance,
	__in PFILE_OBJECT pfoFileObject,
	__in PLARGE_INTEGER pnFileSize
	)
{
	//
	//直接设置文件信息
	//
	FILE_END_OF_FILE_INFORMATION eofInformation;
	eofInformation.EndOfFile.QuadPart = pnFileSize->QuadPart;

	return FltSetInformationFile(
						pfiInstance,
						pfoFileObject,
						(PVOID)&eofInformation,
						sizeof(FILE_END_OF_FILE_INFORMATION),
						FileEndOfFileInformation
						);
}

/*---------------------------------------------------------
函数名称:	FileSetOffset
函数描述:	重新设置文件偏移 用于读取数据后修复 
输入参数:	
			pfiInstance			过滤器实例
			pfoFileObject		文件对象
			pnFileOffset		文件偏移
输出参数:
返回值:		
其他:	
更新维护:	2011.7.17     最初版本
---------------------------------------------------------*/
NTSTATUS FileSetOffset(
	__in PFLT_INSTANCE pfiInstance,
	__in PFILE_OBJECT pfoFileObject,
	__in PLARGE_INTEGER pnFileOffset
	)
{
	FILE_POSITION_INFORMATION fpiPosition;

	LARGE_INTEGER nOffset;

	//
	//先把偏移量拷贝出来 再设置
	//

	nOffset.QuadPart = pnFileOffset->QuadPart;
	nOffset.LowPart = pnFileOffset->LowPart;

	fpiPosition.CurrentByteOffset = nOffset;

	return FltSetInformationFile(pfiInstance,
								   pfoFileObject,
								   &fpiPosition,
								   sizeof(FILE_POSITION_INFORMATION),
								   FilePositionInformation
								   ) ;
}
/*---------------------------------------------------------
函数名称:	FileGetStandardInformation
函数描述:	非重入获取文件基本信息
输入参数:
			pfiInstance			过滤器实例
			pfoFileObject		文件对象
			pnAllocateSize		申请大小
			pnFileSize			文件大小
			pbDirectory			是否是目录
输出参数:
返回值:		
			STATUS_SUCCESS 成功	否则返回相应状态

其他:		后三项参数为可选,不需要查询置为NULL即可

更新维护:	2011.4.3    最初版本
			2011.4.9    修改为使用FltXXX版本
---------------------------------------------------------*/
NTSTATUS
FileGetStandardInformation(
	__in PFLT_INSTANCE pfiInstance,
	__in PFILE_OBJECT pfoFileObject,
	__inout_opt PLARGE_INTEGER pnAllocateSize,
	__inout_opt PLARGE_INTEGER pnFileSize,
	__inout_opt BOOLEAN *pbDirectory
	)
{
	//返回值
	NTSTATUS status;

	PFILE_STANDARD_INFORMATION psiFileStandardInformation;

	//
	//首先分配内存 准备查询 如果失败 返回资源不足
	//
	psiFileStandardInformation = (PFILE_STANDARD_INFORMATION)
				ExAllocatePoolWithTag(NonPagedPool,sizeof(FILE_STANDARD_INFORMATION),MEM_FILE_TAG);

	if( !psiFileStandardInformation ){
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	//
	//查询信息 如果成功就筛选信息,否则直接返回
	//
	status = FltQueryInformationFile(
		pfiInstance,//实例 防止重入
		pfoFileObject,
		(PVOID)psiFileStandardInformation,
		sizeof(FILE_STANDARD_INFORMATION),
		FileStandardInformation,
		NULL//不需要了解返回了多少数据
		);

	if(NT_SUCCESS(status)){
		if(pnAllocateSize){
			*pnAllocateSize 
				= psiFileStandardInformation ->AllocationSize;
		}
		if(pnFileSize){
			*pnFileSize
				= psiFileStandardInformation -> EndOfFile;
		}
		if(pbDirectory != NULL){
			*pbDirectory = psiFileStandardInformation -> Directory;
		}
	}

	ExFreePool(psiFileStandardInformation);

	return status;
}

/*---------------------------------------------------------
函数名称:	FileCompleteCallback
函数描述:	完成回调,激活事件
输入参数:
			CallbackData		返回数据
			Context				上下文
输出参数:
返回值:		
其他:	
更新维护:	2011.4.9    最初版本
---------------------------------------------------------*/
static
VOID 
FileCompleteCallback(
    __in PFLT_CALLBACK_DATA CallbackData,
    __in PFLT_CONTEXT Context
    )
{
	//
	//设置完成标志
	//
	KeSetEvent((PRKEVENT)Context, 0, FALSE);
}

/*---------------------------------------------------------
函数名称:	FileWriteEncryptionHeader
函数描述:	非重入写加密头信息
输入参数:
			pfiInstance			过滤器实例
			pfoFileObject		文件对象
			pvcVolumeContext	卷上下文
			pscFileStreamContext文件流上下文
输出参数:
返回值:		
			STATUS_SUCCESS 成功	否则返回相应状态

其他:		加密头数据定义请参考FileFunction.h ENCRYPTION_HEADER

更新维护:	2011.4.9    修改为使用FltXXX版本
---------------------------------------------------------*/
NTSTATUS 
FileWriteEncryptionHeader( 
	__in PFLT_INSTANCE pfiInstance,
	__in PFILE_OBJECT  pfoFileObject,
	__in PVOLUME_CONTEXT pvcVolumeContext,
	__in PFILE_STREAM_CONTEXT  pscFileStreamContext
	)
{
	//文件头数据
//    static WCHAR wHeader[CONFIDENTIAL_FILE_HEAD_SIZE/sizeof(WCHAR)] = ENCRYPTION_HEADER;

	//完成事件
	KEVENT keEventComplete;

	//文件大小
    LARGE_INTEGER nFileSize;

	//偏移量
	LARGE_INTEGER nOffset;

	//长度 设置为标准加密头长度
    ULONG ulLength = CONFIDENTIAL_FILE_HEAD_SIZE;

	//返回值
    NTSTATUS status;

	//加密头地址
	PVOID pHeader;

	//是否需要设置文件大小
	BOOLEAN bSetSize = FALSE;

	//
	//开始前先初始化事件对象
	//

    KeInitializeEvent(
		&keEventComplete,
		SynchronizationEvent,//同步事件
		FALSE//事件初始标志为FALSE
		);

    //
	//设置文件大小
	//
	FILE_STREAM_CONTEXT_LOCK_ON(pscFileStreamContext);

	//
	//新的文件 把加密头大小加到FileLength上面
	//
	if(pscFileStreamContext->nFileValidLength.QuadPart == 0){
		pscFileStreamContext->nFileSize.QuadPart = CONFIDENTIAL_FILE_HEAD_SIZE;

		bSetSize = TRUE;

	}else{
		ASSERT(pscFileStreamContext->nFileSize.QuadPart >= CONFIDENTIAL_FILE_HEAD_SIZE);
	}

	
	FILE_STREAM_CONTEXT_LOCK_OFF(pscFileStreamContext);

	if(bSetSize){

		status = FileSetSize(
			pfiInstance,
			pfoFileObject,
			&nFileSize
			);

		if(!NT_SUCCESS(status))
			return status;
	}
	//
	//如果不是新文件 而又要写加密头 一定是更新加密头 长度必大于一个加密头
	//
/*	

	nFileSize.QuadPart = pscFileStreamContext->nFileValidLength.QuadPart + CONFIDENTIAL_FILE_HEAD_SIZE;

*/
	//
    //写入加密标识头
	//
    nOffset.QuadPart = 0;

//	ulLength = ROUND_TO_SIZE(ulLength,pvcVolumeContext->ulSectorSize);

	pHeader = ExAllocateFromNPagedLookasideList(&nliNewFileHeaderLookasideList);

	FctConstructFileHead(pscFileStreamContext,pHeader);

	status =  FltWriteFile(
			   pfiInstance,//起始实例,用于防止重入
			   pfoFileObject,//文件对象
			   &nOffset,//偏移量 从头写起
			   CONFIDENTIAL_FILE_HEAD_SIZE,//ulLength,//一个头的大小
			   pHeader,//头数据
			   FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET|FLTFL_IO_OPERATION_NON_CACHED,//非缓存写入
			   NULL,//不需要返回写入的字节数
			   FileCompleteCallback,//回调,确认执行完毕
			   &keEventComplete//回调上下文,传递完成事件
			   );
	//
	//等待完成
	//
	KeWaitForSingleObject(&keEventComplete, Executive, KernelMode, TRUE, 0);

	ExFreeToNPagedLookasideList(&nliNewFileHeaderLookasideList,pHeader);

	return status;
}

/*---------------------------------------------------------
函数名称:	FileReadEncryptionHeaderAndDeconstruct
函数描述:	非重入读取整个加密头 同时解包
输入参数:
			pfiInstance			过滤器实例
			pfoFileObject		文件对象
			pvcVolumeContext	卷上下文
			pscFileStreamContext文件流上下文
输出参数:
返回值:		
			STATUS_SUCCESS 成功	否则返回相应状态

其他:		

更新维护:	2011.4.9    修改为使用FltXXX版本
---------------------------------------------------------*/
NTSTATUS 
FileReadEncryptionHeaderAndDeconstruct( 
	__in PFLT_INSTANCE pfiInstance,
	__in PFILE_OBJECT  pfoFileObject,
	__in PVOLUME_CONTEXT pvcVolumeContext,
	__in PFILE_STREAM_CONTEXT  pscFileStreamContext
	)
{

	//完成事件
	KEVENT keEventComplete;

	//文件大小
    LARGE_INTEGER nFileSize;

	//偏移量
	LARGE_INTEGER nOffset;

	//长度 设置为标准加密头长度
    ULONG ulLength = CONFIDENTIAL_FILE_HEAD_SIZE;

	//各函数返回状态
    NTSTATUS status;

	//本函数要返回的状态
	NTSTATUS statusRet;

	//加密头地址
	PVOID pHeader;

	//
	//开始前先初始化事件对象
	//

    KeInitializeEvent(
		&keEventComplete,
		SynchronizationEvent,//同步事件
		FALSE//事件初始标志为FALSE
		);

	//
    //读取加密标识头
	//
    nOffset.QuadPart = 0;

//	ulLength = ROUND_TO_SIZE(ulLength,pvcVolumeContext->ulSectorSize);

	pHeader = ExAllocateFromNPagedLookasideList(&nliNewFileHeaderLookasideList);


	statusRet = FltReadFile(
				pfiInstance,//起始实例,用于防止重入
				pfoFileObject,//文件对象
				&nOffset,//偏移量 从头写起
				CONFIDENTIAL_FILE_HEAD_SIZE,//ulLength,//一个头的大小
				pHeader,
				FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET|FLTFL_IO_OPERATION_NON_CACHED,//非缓存写入
				NULL,//不需要返回读入的字节数
				FileCompleteCallback,//回调,确认执行完毕
				&keEventComplete//回调上下文,传递完成事件
				);

	//
	//等待完成
	//
	KeWaitForSingleObject(&keEventComplete, Executive, KernelMode, TRUE, 0);

	//
	//解包
	//
	FctDeconstructFileHead(pscFileStreamContext,pHeader);

	ExFreeToNPagedLookasideList(&nliNewFileHeaderLookasideList,pHeader);

	//
	//恢复偏移量到0
	//
	status = FileSetOffset(pfiInstance,pfoFileObject,&nOffset);

	if( !NT_SUCCESS(status) ){
		statusRet = status;
	}

	return statusRet;
}

/*---------------------------------------------------------
函数名称:	FileIsEncrypted
函数描述:	判断文件是否是机密文件
输入参数:
			pfiInstance					过滤器实例
			pfoFileObject				文件对象
			pfcdCBD						回调参数
			ulFlags						标志位

输出参数:
			pbIsFileEncrypted 文件是否被加密
返回值:		
			STATUS_SUCCESS					成功(未写加密头)	
			STATUS_FILE_NOT_ENCRYPTED		不是机密文件
			STATUS_REPARSE_OBJECT			在这个新建的文件 在本函数
											中被写入了加密头

其他:		这个函数设计的不是很好,把写文件头和修改IRP参数
			也包含在里面了.暂时不修改了.

			如果这个文件已经存在在机密表中,函数将返回
			STATUS_IMAGE_ALREADY_LOADED

			如果查询名称失败(根本无名称)将返回
			STATUS_OBJECT_NAME_NOT_FOUND

			pbAutoWriteEncryptedHeader传入状态为TRUE时将自动
			补齐加密头,同时将修改FLT_PARAMETER部分数据,若输出
			TRUE说明已经进行,需要外部调用FltSetCallbackDataDirty

更新维护:	2011.4.9     最初版本
			2011.4.13    增加了判断是否已经存在在表中
			2011.4.19    Bug:未执行FltCreateFile也会
								   运行FltClose,已修正.
		    2011.4.30    修改查询不到名称时的返回值.
			2011.5.1     Bug:未赋pbIsFileEncrypted
								   初值会造成判断错误.已修正.
		    2011.5.2     将打开文件的操作独立出去.
			2011.7.8     Bug:修改了FLT_PARAMETER却没有
								   通知FLTMGR.通知外部是否需
								   要Dirty.已修正.
			2011.7.20    修改了数据结构增加了定义
			2012.1.1     Bug:发现此函数导致一些图标加载
			                       异常,FltReadFile造成,使用
								   自己重新打开的文件对象即可.
								   已修正.
---------------------------------------------------------*/
NTSTATUS 
FileIsEncrypted(
	__in PFLT_INSTANCE pfiInstance,
	__in PFILE_OBJECT pfoFileObjectOpened,
	__in PFLT_CALLBACK_DATA pfcdCBD,
	__in PVOLUME_CONTEXT pvcVolumeContext,
	__in PFILE_STREAM_CONTEXT  pscFileStreamContext,
	__in ULONG	ulFlags 
	)
{
	//是否是目录
	BOOLEAN bDirectory;

	//文件长度
	LARGE_INTEGER nFileSize;

	//状态
	NTSTATUS status;

	//完成事件
	KEVENT keEventComplete;

	//偏移量
	LARGE_INTEGER nOffset;

	//加密标识
	WCHAR wEncryptedLogo[ENCRYPTION_HEAD_LOGO_SIZE]	= ENCRYPTION_HEADER;

	//读取的数据内容 用于同加密标识比较 多申请了一个存放\0的空间
	WCHAR wBufferRead[ENCRYPTION_HEAD_LOGO_SIZE+2] = {0};

	//文件句柄
	HANDLE hFile = NULL;

	//对象属性,用于打开文件时使用
	OBJECT_ATTRIBUTES oaObjectAttributes;

	//文件路径
	UNICODE_STRING usPath;

	//文件路径存放数据地址
	WCHAR wPath[NORMAL_FILE_PATH_LENGTH];

	//文件路径指针
	PWSTR pwPath = wPath;

	//文件路径长度
	ULONG ulPathLength;

	//需要的权限
	ULONG ulDesiredAccess;

	//准备返回的返回值
	NTSTATUS statusRet = STATUS_FILE_NOT_ENCRYPTED;

	//函数返回值
	BOOLEAN bReturn;

	//过滤器IO参数
	PFLT_PARAMETERS  pfpParameters;

	//用于存放读取的文件开头部分的内存
	PWCHAR pwFileHead;

	//
	//保存访问权限的值 参数等
	//
	FltDecodeParameters(
		pfcdCBD,
		NULL,
		NULL,
		NULL,
		(LOCK_OPERATION *)&ulDesiredAccess
		);

	pfpParameters = &pfcdCBD -> Iopb -> Parameters;

	do{

		//
		//查询文件基本信息
		//
		status = FileGetStandardInformation(
				pfiInstance,
				pfoFileObjectOpened,
				NULL,
				&nFileSize,
				&bDirectory
				);

		if( !NT_SUCCESS(status) ){
			statusRet = status;
			break;
		}

		//
		//如果是目录,直接返回不需要加密
		//
		if( bDirectory ){
			statusRet = STATUS_FILE_NOT_ENCRYPTED;
			
			break;
		}

		//
		//如果文件大小为0(新建的文件),并且准备写操作,而且是机密进程,那么加密
		//
		if( ( nFileSize.QuadPart == 0 ) &&//文件大小为0
			(ulDesiredAccess & (FILE_WRITE_DATA | FILE_APPEND_DATA) )&&//准备写文件 
			IsCurrentProcessConfidential()){
			//
			//如果要求自动补齐加密头,那么现在写入 写入前重新设置文件大小
			//
			if(!(ulFlags&FILE_IS_ENCRYPTED_DO_NOT_WRITE_LOGO)){

				statusRet = FileWriteEncryptionHeader(
					pfiInstance,
					pfoFileObjectOpened,
					pvcVolumeContext,
					pscFileStreamContext
					);
				
				if(NT_SUCCESS(statusRet)){
					FileClearCache(pfoFileObjectOpened);
					//
					//恢复偏移量到0
					//

					DebugTraceFileAndProcess(
						DEBUG_TRACE_IMPORTANT_INFO|DEBUG_TRACE_CONFIDENTIAL,
						"FileIsEncrypted",
						FILE_OBJECT_NAME_BUFFER(pfoFileObjectOpened),
						("Header has been written.Set offset. High:%d,Low:%d,Quard:%d",
							nOffset.HighPart,nOffset.LowPart,nOffset.QuadPart)
						);

					nOffset.QuadPart = 0;

					status = FileSetOffset(pfiInstance,pfoFileObjectOpened,&nOffset);

					if( !NT_SUCCESS(status) )
					{
						statusRet = status;
						break;
					}
					statusRet = STATUS_REPARSE_OBJECT;
				}else{

					DebugTraceFileAndProcess(
						DEBUG_TRACE_ERROR,
						"FileIsEncrypted",
						FILE_OBJECT_NAME_BUFFER(pfoFileObjectOpened),
						("Cannot write header.")
						);
				}
				
				break;
			}

			statusRet =  STATUS_SUCCESS;

			break;
		}

		//
		//如果文件大小小于加密头,那么肯定不是加密文件
		//
		if( nFileSize.QuadPart < CONFIDENTIAL_FILE_HEAD_SIZE ){
			statusRet = STATUS_FILE_NOT_ENCRYPTED;

			break;
		}

		//
		//现在读出前几个字符判断是否是加密文件,字符数
		//取决于FileFunction.h中ENCRYPTION_HEAD_LOGO_SIZE
		//

		//
		//开始前先初始化事件对象
		//

		KeInitializeEvent(
			&keEventComplete,
			SynchronizationEvent,//同步事件
			FALSE//事件初始标志为FALSE
			);

		nOffset.QuadPart = 0;

		//
		//获取一个内存块用于存放
		//
//		pwFileHead = (PWCHAR)ExAllocateFromNPagedLookasideList(
//			pvcVolumeContext->pnliReadEncryptedSignLookasideList);

		__try{

			status = FltReadFile(
				pfiInstance,
				pfoFileObjectOpened,
				&nOffset,
				ENCRYPTION_HEAD_LOGO_SIZE,//pvcVolumeContext->ulSectorSize,//由于非缓存必须一次性读一个读一个SectorSize,所以这里就读一个ENCRYPTION_HEAD_LOGO_SIZE,//,ulLengthToRead,//读出一个标识长度的数据
				wBufferRead,//pwFileHead,//保存在pwFileHead
				FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET,//FLTFL_IO_OPERATION_NON_CACHED|FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET,
				NULL,
				FileCompleteCallback,
				(PVOID)&keEventComplete
				);

			KeWaitForSingleObject(&keEventComplete, Executive, KernelMode, TRUE, 0);

		}__except(EXCEPTION_EXECUTE_HANDLER){

//			ExFreeToNPagedLookasideList(
//				pvcVolumeContext->pnliReadEncryptedSignLookasideList,pwFileHead);

			return STATUS_UNSUCCESSFUL;
		}

		if( !NT_SUCCESS(status) ){
			statusRet = status;
			break;
		}
		
		//
		//恢复偏移量到0
		//
		status = FileSetOffset(pfiInstance,pfoFileObjectOpened,&nOffset);
		if( !NT_SUCCESS(status) )
		{
			statusRet = status;
			break;
		}

		//
		//比较标志是否相等
		//
		//DebugPrintFileObject("Read file check",pfoFileObjectOpened,FALSE);
		//KdPrint(("\t\tRead file %ws\n",wBufferRead));

		if( RtlCompareMemory(
				wBufferRead,
				wEncryptedLogo,
				ENCRYPTION_HEAD_LOGO_SIZE)
					== ENCRYPTION_HEAD_LOGO_SIZE){
			statusRet = STATUS_SUCCESS;

			DebugTraceFileAndProcess(
				DEBUG_TRACE_IMPORTANT_INFO|DEBUG_TRACE_CONFIDENTIAL,
				"FileIsEncrypted",
				FILE_OBJECT_NAME_BUFFER(pfoFileObjectOpened),
				("Confidential file detected.")
				);
//			ExFreeToNPagedLookasideList(
//				pvcVolumeContext->pnliReadEncryptedSignLookasideList,pwFileHead);
			break;
		}

//		ExFreeToNPagedLookasideList(
//			pvcVolumeContext->pnliReadEncryptedSignLookasideList,pwFileHead);

	}while(0);

	//
	//善后工作
	//
/*	
	//
	//要注意:
    //1.文件的CREATE改为OPEN.
    //2.文件的OVERWRITE去掉.不管是不是要加密的文件,
    //都必须这样做.否则的话,本来是试图生成文件的,
    //结果发现文件已经存在了.本来试图覆盖文件的,再
    //覆盖一次会去掉加密头.
	//

	//
	//如果连名字都没有 什么都没做 就不用修改了
	//
	if( !(ulFlags&FILE_IS_ENCRYPTED_DO_NOT_CHANGE_OPEN_WAY) )
	{
		ULONG ulDisp = FILE_OPEN;
		pfpParameters->Create.Options &= 0x00ffffff;
		pfpParameters->Create.Options |= (ulDisp << 24);
	}*/

	return	statusRet;
}

/*---------------------------------------------------------
函数名称:	FileCreateByObjectNotCreated
函数描述:	在文件被打开前先通过该对象打开文件
输入参数:
			pfiInstance					过滤器实例
			pfoFileObject				文件对象
			pfpParameters				IRP参数
			phFileHandle				保存文件句柄的空间
			ulDesiredAccess				期望的权限

输出参数:
			phFileHandle 文件句柄
返回值:		
			STATUS_SUCCESS					成功	
			STATUS_OBJECT_NAME_NOT_FOUND	未查询到名称

其他:		如果查询名称失败(根本无名称)将返回
			STATUS_OBJECT_NAME_NOT_FOUND

			如果要同传入的文件对象权限一致,ulDesiredAccess
			可以置为NULL

			打开后需要使用FltClose关闭文件

更新维护:	2011.5.2     最初版本
			2011.7.28    修改了部分参数
			2012.1.2     Bug:返回值为成功打开,但句柄无效
---------------------------------------------------------*/
NTSTATUS 
FileCreateByObjectNotCreated(
	__in PFLT_INSTANCE pfiInstance,
	__in PFLT_FILE_NAME_INFORMATION pfniFileNameInformation,
	__in PFLT_PARAMETERS pfpParameters,
	__in_opt ULONG ulDesiredAccess,
	__out HANDLE * phFileHandle
	)
{
	//读取的数据内容 用于同加密标识比较
	WCHAR wBufferRead[ENCRYPTION_HEAD_LOGO_SIZE] = {0};

	//文件属性
	ULONG  FileAttributes;

	//共享权限
	ULONG  ShareAccess;

	//打开处理
	ULONG  CreateDisposition;
	
	//打开选项	
	ULONG  CreateOptions;

	//对象属性,用于打开文件时使用
	OBJECT_ATTRIBUTES oaObjectAttributes;

	//文件路径长度
	ULONG ulPathLength;

	//返回的Io状态
	IO_STATUS_BLOCK ioStatusBlock;

	//返回的状态
	NTSTATUS statusRet;

	//
	//填充数据
	//
	
	InitializeObjectAttributes(
		&oaObjectAttributes,
		&pfniFileNameInformation->Name,
		OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,
		NULL,
		NULL
		);

	CreateDisposition	= pfpParameters->Create.Options>>24;
	CreateOptions		= pfpParameters->Create.Options & 0x00ffffff;
	ShareAccess			= pfpParameters->Create.ShareAccess;
	FileAttributes		= pfpParameters->Create.FileAttributes;

	//
	//保存访问权限的值 如果没有设置
	//
	if(!ulDesiredAccess){
		ulDesiredAccess = 
			pfpParameters -> Create.SecurityContext -> DesiredAccess;
	}

	//
	//为了兼容Windows Xp,使用FltCreateFile
	//

	return FltCreateFile(
		pfltGlobalFilterHandle,
		pfiInstance,
		phFileHandle,
		ulDesiredAccess,
		&oaObjectAttributes,
		&ioStatusBlock,
		NULL,
		FileAttributes,
		ShareAccess,
		CreateDisposition,
		CreateOptions,
		NULL,
		NULL,
		IO_IGNORE_SHARE_ACCESS_CHECK
		);
}


/*---------------------------------------------------------
函数名称:	FileCreateForHeaderWriting
函数描述:	在文件被打开前先通过该对象打开文件
输入参数:
			pfiInstance					过滤器实例
			pfniFileNameInformation		文件名信息
			phFileHandle				保存文件句柄的空间

输出参数:
			phFileHandle 文件句柄
返回值:		
			STATUS_SUCCESS					成功	
			STATUS_OBJECT_NAME_NOT_FOUND	未查询到名称

其他:		如果查询名称失败(根本无名称)将返回
			STATUS_OBJECT_NAME_NOT_FOUND


			打开后需要使用FltClose关闭文件

更新维护:	2011.5.2     最初版本
---------------------------------------------------------*/
NTSTATUS 
FileCreateForHeaderWriting(
	__in PFLT_INSTANCE pfiInstance,
	__in PUNICODE_STRING puniFileName,
	__out HANDLE * phFileHandle
	)
{
	//对象属性,用于打开文件时使用
	OBJECT_ATTRIBUTES oaObjectAttributes;

	//文件路径长度
	ULONG ulPathLength;

	//返回的Io状态
	IO_STATUS_BLOCK ioStatusBlock;

	//返回的状态
	NTSTATUS statusRet;

	//
	//填充数据
	//
	
	InitializeObjectAttributes(
		&oaObjectAttributes,
		puniFileName,
		OBJ_KERNEL_HANDLE|OBJ_CASE_INSENSITIVE,
		NULL,
		NULL
		);


	//
	//为了兼容Windows Xp,使用FltCreateFile
	//

	return FltCreateFile(
		pfltGlobalFilterHandle,
		pfiInstance,
		phFileHandle,
		FILE_READ_DATA|FILE_WRITE_DATA,
		&oaObjectAttributes,
		&ioStatusBlock,
		NULL,
		FILE_ATTRIBUTE_NORMAL,
		FILE_SHARE_READ,
		FILE_OPEN,
		FILE_NON_DIRECTORY_FILE,
		NULL,
		0,
		IO_IGNORE_SHARE_ACCESS_CHECK
		);
}


/*---------------------------------------------------------
函数名称:	FileClearCache
函数描述:	释放缓冲 用于机密和非机密进程读取间调换数据
输入参数:	pFileObject					文件对象
输出参数:
返回值:		
其他:	
更新维护:	2011.7.17     最初版本
---------------------------------------------------------*/
// 清理缓冲
void 
FileClearCache(
	PFILE_OBJECT pFileObject
	)
{
	//FCB
	PFSRTL_COMMON_FCB_HEADER pFcb;
	
	//睡眠时间 用于KeDelayExecutionThread
	LARGE_INTEGER liInterval;
	
	//是否需要释放资源
	BOOLEAN bNeedReleaseResource = FALSE;

	//是否需要释放分页资源
	BOOLEAN bNeedReleasePagingIoResource = FALSE;

	//IRQL
	KIRQL irql;

	//循环时是否跳出
	BOOLEAN bBreak = TRUE;

	//是否资源被锁定
	BOOLEAN bLockedResource = FALSE;

	//是否是分页资源被锁定
	BOOLEAN bLockedPagingIoResource = FALSE;
	
	//
	//获取FCB
	//
	pFcb = (PFSRTL_COMMON_FCB_HEADER)pFileObject->FsContext;

	//
	//如果没有FCB 直接返回
	//
	if(pFcb == NULL){
/*
#ifdef DBG
		__asm int 3
#endif*/
		return;
	}

	//
	//保证当前IRQL<=APC_LEVEL
	//
	if ( irql = KeGetCurrentIrql() > APC_LEVEL ){
#ifdef DBG
		__asm int 3
#endif
		return;
	}

	//
	//设置睡眠时间
	//
	liInterval.QuadPart = -1 * (LONGLONG)50;

	//
	//进入文件系统
	//

	FsRtlEnterFileSystem();

	//
	//循环拿锁 一定要拿锁 否则可能损坏数据
	//
	for(;;){
		//
		//初始化参数
		//
		bBreak							= TRUE;
		bNeedReleaseResource			= FALSE;
		bNeedReleasePagingIoResource	= FALSE;
		bLockedResource					= FALSE;
		bLockedPagingIoResource			= FALSE;

		//
		//进入文件系统
		//

//		FsRtlEnterFileSystem();

		//
		//从FCB中拿锁
		//
		if (pFcb->PagingIoResource){
			bLockedPagingIoResource = ExIsResourceAcquiredExclusiveLite(pFcb->PagingIoResource);
		}

		//
		//使劲拿 必须拿 一定拿.....
		//
		if (pFcb->Resource){
			bLockedResource = TRUE;
			//
			//先尝试拿一下资源
			//
			if (ExIsResourceAcquiredExclusiveLite(pFcb->Resource) == FALSE){
				//
				//没拿到资源 再来一次
				//
				bNeedReleaseResource = TRUE;
				if (bLockedPagingIoResource){
					if (ExAcquireResourceExclusiveLite(pFcb->Resource, FALSE) == FALSE){
						bBreak = FALSE;
						bNeedReleaseResource = FALSE;
						bLockedResource = FALSE;
					}
				}else{
					ExAcquireResourceExclusiveLite(pFcb->Resource, TRUE);
				}
			}
		}
   
		if (bLockedPagingIoResource == FALSE){
			if (pFcb->PagingIoResource){

				bLockedPagingIoResource = TRUE;
				bNeedReleasePagingIoResource = TRUE;

				if (bLockedResource){

					if (ExAcquireResourceExclusiveLite(pFcb->PagingIoResource, FALSE) == FALSE)	{

						bBreak = FALSE;
						bLockedPagingIoResource = FALSE;
						bNeedReleasePagingIoResource = FALSE;
					}
				}else{
					ExAcquireResourceExclusiveLite(pFcb->PagingIoResource, TRUE);
				}
			}
		}

		if (bBreak){
			break;
		}
       
		if (bNeedReleasePagingIoResource){
			ExReleaseResourceLite(pFcb->PagingIoResource);
		}
		if (bNeedReleaseResource){
			ExReleaseResourceLite(pFcb->Resource);
		}

/*		if (irql == PASSIVE_LEVEL)
		{
//			FsRtlExitFileSystem();
			KeDelayExecutionThread(KernelMode, FALSE, &liInterval);
		}
		else
		{
			KEVENT waitEvent;
			KeInitializeEvent(&waitEvent, NotificationEvent, FALSE);
			KeWaitForSingleObject(&waitEvent, Executive, KernelMode, FALSE, &liInterval);
		}*/
	}

	//
	//终于拿到锁了
	//
	if (pFileObject->SectionObjectPointer){

		IO_STATUS_BLOCK ioStatus;

		IoSetTopLevelIrp( (PIRP)FSRTL_FSP_TOP_LEVEL_IRP );

		CcFlushCache(pFileObject->SectionObjectPointer, NULL, 0, &ioStatus);

		if (pFileObject->SectionObjectPointer->ImageSectionObject){
			MmFlushImageSection(pFileObject->SectionObjectPointer,MmFlushForWrite); // MmFlushForDelete
		}

		CcPurgeCacheSection(pFileObject->SectionObjectPointer, NULL, 0, FALSE);  

		IoSetTopLevelIrp(NULL);   
	}

	if (bNeedReleasePagingIoResource){
		ExReleaseResourceLite(pFcb->PagingIoResource);
	}
	if (bNeedReleaseResource){
		ExReleaseResourceLite(pFcb->Resource);
	}

	FsRtlExitFileSystem();
/*
Acquire:
	FsRtlEnterFileSystem() ;

	if (Fcb->Resource)
		ResourceAcquired = ExAcquireResourceExclusiveLite(Fcb->Resource, TRUE) ;
	if (Fcb->PagingIoResource)
		PagingIoResourceAcquired = ExAcquireResourceExclusive(Fcb->PagingIoResource,FALSE);
	else
		PagingIoResourceAcquired = TRUE ;
	if (!PagingIoResourceAcquired)
	{
		if (Fcb->Resource)  ExReleaseResource(Fcb->Resource);
		FsRtlExitFileSystem();
		KeDelayExecutionThread(KernelMode,FALSE,&Delay50Milliseconds);	
		goto Acquire;	
	}

	if(FileObject->SectionObjectPointer)
	{
		IoSetTopLevelIrp( (PIRP)FSRTL_FSP_TOP_LEVEL_IRP );

		if (bIsFlushCache)
		{
			CcFlushCache( FileObject->SectionObjectPointer, FileOffset, Length, &IoStatus );
		}

		if(FileObject->SectionObjectPointer->ImageSectionObject)
		{
			MmFlushImageSection(
				FileObject->SectionObjectPointer,
				MmFlushForWrite
				) ;
		}

		if(FileObject->SectionObjectPointer->DataSectionObject)
		{ 
			PurgeRes = CcPurgeCacheSection( FileObject->SectionObjectPointer,
				NULL,
				0,
				FALSE );                                                    
		}
                                      
		IoSetTopLevelIrp(NULL);
	}

	if (Fcb->PagingIoResource)
		ExReleaseResourceLite(Fcb->PagingIoResource );                                       
	if (Fcb->Resource)
		ExReleaseResourceLite(Fcb->Resource );                     

	FsRtlExitFileSystem() ;*/
}
/*---------------------------------------------------------
函数名称:	FileGetFilePostfixName
函数描述:	获取文件后缀名
输入参数:
			puniFileName		文件名称
			puniPostfixName		保存后缀名的UNICODE_STRING结构
								需申请好内存 若为NULL则返回需要
								申请的长度
输出参数:
			puniPostfixName		后缀名字符串

返回值:		文件后缀名长度

			如果返回0说明获取失败

其他:		
更新维护:	2011.8.9    最初版本
---------------------------------------------------------*/
USHORT 
FileGetFilePostfixName( 
	__in PUNICODE_STRING  puniFileName,
	__inout_opt PUNICODE_STRING puniPostfixName
	)
{
	UNICODE_STRING uniPostFix;
	USHORT usLength;

	for(usLength = puniFileName->Length/sizeof(WCHAR);usLength > 0; --usLength){

		if(puniFileName->Buffer[usLength] == L'\\'){
			return 0;
		}

		if(puniFileName->Buffer[usLength] == L'.'){
			RtlInitUnicodeString(&uniPostFix,&puniFileName->Buffer[usLength]);

			if(puniPostfixName){
				if(puniPostfixName->MaximumLength < uniPostFix.Length){
					return uniPostFix.Length;
				}

				RtlCopyUnicodeString(puniPostfixName,&uniPostFix);

				puniPostfixName->Length = puniFileName->Length - usLength*sizeof(WCHAR);

				return puniPostfixName->Length;
			}else{
				return uniPostFix.MaximumLength;
			}
		}
	}

	return 0;
}
