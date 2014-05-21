///////////////////////////////////////////////////////////////////////////////
///
/// 版权所有 (c) 2011 - 2012
///
/// 原始文件名称	 : ConfidentialFile.cpp
/// 工程名称         : AntinvaderDriver
/// 创建时间         : 2011-03-26
/// 	         
///
/// 描述	         : 关于文件上下文等实现
///
/// 更新维护:
///  0001 [2011-07-28] 最初版本.废弃上一版本全部内容
///
///////////////////////////////////////////////////////////////////////////////

#include "ConfidentialFile.h"

/*---------------------------------------------------------
函数名称:	FctCreateContextForSpecifiedFileStream
函数描述:	为指定的文件流创建上下文
输入参数:	pfiInstance				过滤器实例
			pfoFileObject			文件对象
			dpscFileStreamContext	保存上下文指针的空间
输出参数:
返回值:		STATUS_SUCCESS 为成功 

其他:		当打开一个已经存在的上下文时,引用计数将+1
			增加了同步
更新维护:	2011.7.28    最初版本
			2012.1.3     增加了对引用计数的修改
---------------------------------------------------------*/
NTSTATUS
FctCreateContextForSpecifiedFileStream(
	__in PFLT_INSTANCE pfiInstance,
	__in PFILE_OBJECT pfoFileObject,
	__inout PFILE_STREAM_CONTEXT * dpscFileStreamContext
   )
{
	//返回值
    NTSTATUS status;

	//申请到的流上下文
    PFILE_STREAM_CONTEXT pscFileStreamContext;	

	//保存旧的上下文
	PFILE_STREAM_CONTEXT pscOldStreamContext;

	//
	//先把返回值置NULL
	//
	*dpscFileStreamContext = NULL;

	//
	//IRQL必须 <= APC Level
	//
	PAGED_CODE();

	//
	//先尝试获取上下文
	//
	status = FctGetSpecifiedFileStreamContext( 
		pfiInstance,
		pfoFileObject,
		&pscFileStreamContext 
		);

	if(status == STATUS_NOT_SUPPORTED){
		return status;
	}

	if(status != STATUS_NOT_FOUND){
		//
		//已经有了 此处增加了引用计数
		//
		*dpscFileStreamContext = pscFileStreamContext;
		
		//FILE_STREAM_CONTEXT_LOCK_ON(pscFileStreamContext);
		pscFileStreamContext->ulReferenceTimes ++;
		//FILE_STREAM_CONTEXT_LOCK_OFF(pscFileStreamContext);

		return STATUS_FLT_CONTEXT_ALREADY_DEFINED;
	}

	//
	//申请上下文
	//
	status = FltAllocateContext( pfltGlobalFilterHandle,
                                 FLT_STREAM_CONTEXT,
                                 FILE_STREAM_CONTEXT_SIZE,
                                 NonPagedPool,
                                 (PFLT_CONTEXT *)&pscFileStreamContext
								 );
	if (!NT_SUCCESS( status )){
		return status;
	}

	//
	//初始化上下文
	//
	RtlZeroMemory( pscFileStreamContext, FILE_STREAM_CONTEXT_SIZE );

	//
	//分配内存
	//
	pscFileStreamContext -> prResource = (PERESOURCE)ExAllocatePoolWithTag(
		NonPagedPool,
		sizeof(ERESOURCE),
		MEM_TAG_FILE_TABLE
		);

	if(!pscFileStreamContext -> prResource){
		//
		//没有内存了
		//
        FltReleaseContext( pscFileStreamContext );
        return STATUS_INSUFFICIENT_RESOURCES;
	}

	ExInitializeResourceLite(pscFileStreamContext->prResource);	

//	KeInitializeSpinLock(&pscFileStreamContext->kslLock) ; 

	//
	//设置上下文
	//
	status = FltSetStreamContext( 
				pfiInstance,
				pfoFileObject,
				FLT_SET_CONTEXT_KEEP_IF_EXISTS,
				pscFileStreamContext,
				(PFLT_CONTEXT *)&pscOldStreamContext 
				);

	if (!NT_SUCCESS( status )){
		FltReleaseContext(pscFileStreamContext);
		return status;
	} 

	//
	//同步
	//
	FILE_STREAM_CONTEXT_LOCK_ON(pscFileStreamContext);

	//
	//保存返回值
	//
	*dpscFileStreamContext = pscFileStreamContext;

	return STATUS_SUCCESS;
}

/*---------------------------------------------------------
函数名称:	FctGetSpecifiedFileStreamContext
函数描述:	获取文件流上下文 加入了同步
输入参数:	pfiInstance				过滤器实例
			pfoFileObject			文件对象
			dpscFileStreamContext	保存文件流指针的空间
输出参数:
返回值:		STATUS_SUCCESS 为成功 
其他:		
更新维护:	2011.7.28    最初版本
---------------------------------------------------------*/
NTSTATUS
FctGetSpecifiedFileStreamContext(
	__in PFLT_INSTANCE pfiInstance,
	__in PFILE_OBJECT pfoFileObject,
	__inout PFILE_STREAM_CONTEXT * dpscFileStreamContext
   )
{
	NTSTATUS status;

	status =  FltGetStreamContext(
		pfiInstance,
		pfoFileObject,
		(PFLT_CONTEXT *)dpscFileStreamContext
		);

	if(!NT_SUCCESS(status)){
		return status;
	}

	FILE_STREAM_CONTEXT_LOCK_ON((*dpscFileStreamContext));

	return status;
}

/*---------------------------------------------------------
函数名称:	FctInitializeContext
函数描述:	为指定的文件流创建上下文 增加了同步
输入参数:	pscFileStreamContext	保存文件流指针的空间
输出参数:
返回值:		STATUS_SUCCESS 为成功 
其他:		
更新维护:	2011.7.28    最初版本
---------------------------------------------------------*/
NTSTATUS
FctInitializeContext(
	__inout PFILE_STREAM_CONTEXT pscFileStreamContext,
	__in PFLT_CALLBACK_DATA pfcdCBD,
	__in PFLT_FILE_NAME_INFORMATION	pfniFileNameInformation
   )
{
	//返回值 
	NTSTATUS status;

	//
	//上锁
	//
	//FILE_STREAM_CONTEXT_LOCK_ON(pscFileStreamContext);

	status = FctUpdateStreamContextFileName(
		&pfniFileNameInformation->Name,
		pscFileStreamContext
		);

	if(!NT_SUCCESS(status)){		
		//FILE_STREAM_CONTEXT_LOCK_OFF(pscFileStreamContext);
		return status;
	}

	//
	//保存卷名称 等 初始化其他数据
	//
	RtlCopyMemory(
		pscFileStreamContext->wszVolumeName,
		pfniFileNameInformation->Volume.Buffer,
		pfniFileNameInformation->Volume.Length
		) ;
	
	pscFileStreamContext -> ulReferenceTimes = 1;

	pscFileStreamContext ->bCached = CALLBACK_IS_CACHED(pfcdCBD->Iopb);

	pscFileStreamContext ->fctEncrypted = ENCRYPTED_TYPE_UNKNOWN;

	pscFileStreamContext->fosOpenStatus = OPEN_STATUS_FREE;

	pscFileStreamContext->bUpdateWhenClose = FALSE;

	//
	//这里还不知道到底有没有加密 所以先认为是未加密文件
	//
	status = FileGetStandardInformation(
		pfcdCBD->Iopb->TargetInstance,
		pfcdCBD->Iopb->TargetFileObject,
		NULL,
		&pscFileStreamContext->nFileValidLength,
		NULL
		);

	if(!NT_SUCCESS(status)){
		//FILE_STREAM_CONTEXT_LOCK_OFF(pscFileStreamContext);
		return status;
	}

	//
	//解锁
	//
	//FILE_STREAM_CONTEXT_LOCK_OFF(pscFileStreamContext);

	return STATUS_SUCCESS;
}

/*---------------------------------------------------------
函数名称:	FctUpdateStreamContextFileName
函数描述:	更新流文件上下文中的名称
输入参数:	pusName					新的名称
			pscFileStreamContext	文件流上下文指针
输出参数:
返回值:		STATUS_SUCCESS 为成功 
其他:		
更新维护:	2011.7.28    最初版本
---------------------------------------------------------*/
NTSTATUS
FctUpdateStreamContextFileName(
    __in PUNICODE_STRING pusName,
    __inout PFILE_STREAM_CONTEXT  pscFileStreamContext
    )
{
	//返回值
    NTSTATUS status = STATUS_SUCCESS ;

	//后缀名长度
	USHORT usPostFixLength;


	//
    //如果已经有名字了 先释放掉
	//
	if (pscFileStreamContext->usName.Buffer != NULL) {

		ExFreePoolWithTag( pscFileStreamContext->usName.Buffer,MEM_TAG_FILE_TABLE );
		pscFileStreamContext->usName.Length = 0;
		pscFileStreamContext->usName.MaximumLength = 0;
		pscFileStreamContext->usName.Buffer = NULL;
	}

	//
    //申请并拷贝新名称
	//
    pscFileStreamContext->usName.MaximumLength = pusName->Length;
    pscFileStreamContext->usName.Buffer = (PWCH)ExAllocatePoolWithTag(
											NonPagedPool,
                                            pscFileStreamContext->usName.MaximumLength,
                                            MEM_TAG_FILE_TABLE
											);
	


    if (pscFileStreamContext->usName.Buffer == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyUnicodeString(&pscFileStreamContext->usName, pusName);

	usPostFixLength = FileGetFilePostfixName(&pscFileStreamContext->usName,NULL);

	if(usPostFixLength){
		pscFileStreamContext->usPostFix.Buffer = (PWCH)ExAllocatePoolWithTag(
											NonPagedPool,
                                            usPostFixLength,
                                            MEM_TAG_FILE_TABLE
											);

		if (pscFileStreamContext->usPostFix.Buffer == NULL) {
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		pscFileStreamContext->usPostFix.Length = 0;
		pscFileStreamContext->usPostFix.MaximumLength = usPostFixLength;

		FileGetFilePostfixName(&pscFileStreamContext->usName,&pscFileStreamContext->usPostFix);

	}else{
		pscFileStreamContext->usPostFix.Length = 0;
		pscFileStreamContext->usPostFix.MaximumLength = 0;
		pscFileStreamContext->usPostFix.Buffer = NULL;
	}

    return status;
}

/*---------------------------------------------------------
函数名称:	FctFreeStreamContext
函数描述:	释放文件流上下文
			pscFileStreamContext	文件流上下文指针
输出参数:
返回值:		STATUS_SUCCESS 为成功 
其他:		
更新维护:	2011.7.28    最初版本
---------------------------------------------------------*/
NTSTATUS
FctFreeStreamContext(
    __inout PFILE_STREAM_CONTEXT  pscFileStreamContext
    )
{
	NTSTATUS status;

	if (pscFileStreamContext == NULL){
		return STATUS_SUCCESS;
	}

	if (pscFileStreamContext->usName.Buffer != NULL) {
		ExFreePoolWithTag(
			pscFileStreamContext->usName.Buffer,
			MEM_TAG_FILE_TABLE
			);

		pscFileStreamContext->usName.Length = 0;
		pscFileStreamContext->usName.MaximumLength = 0;
		pscFileStreamContext->usName.Buffer = NULL;
	}

	if(pscFileStreamContext->usPostFix.Buffer != NULL){
		ExFreePoolWithTag(
			pscFileStreamContext->usPostFix.Buffer,
			MEM_TAG_FILE_TABLE
			);
		pscFileStreamContext->usPostFix.Length = 0;
		pscFileStreamContext->usPostFix.MaximumLength = 0;
		pscFileStreamContext->usPostFix.Buffer = NULL;
	}


	if(pscFileStreamContext -> prResource != NULL){
		status = ExDeleteResourceLite(
			pscFileStreamContext -> prResource
			);

		if(!NT_SUCCESS(status))	{
			return status;
		}

		ExFreePoolWithTag(
			pscFileStreamContext -> prResource,
			MEM_TAG_FILE_TABLE
			);
	}	
	return STATUS_SUCCESS;
}

/*---------------------------------------------------------
函数名称:	FctConstructFileHead
函数描述:	释放文件流上下文
			pscFileStreamContext	文件流上下文指针
			pFileHead				保存头的地址
输出参数:	pFileHead				填写好的头

返回值:		STATUS_SUCCESS 为成功 
其他:		需要调用者申请内存
更新维护:	2011.7.28    最初版本
---------------------------------------------------------*/
NTSTATUS
FctConstructFileHead(
    __in	PFILE_STREAM_CONTEXT  pscFileStreamContext,
	__inout PVOID	pFileHead
    )
{
	WCHAR wHeaderLogo[20] = ENCRYPTION_HEADER;

	PFILE_ENCRYPTION_HEAD pfehFileEncryptionHead 
		= (PFILE_ENCRYPTION_HEAD)pFileHead;

	RtlZeroMemory(pfehFileEncryptionHead,CONFIDENTIAL_FILE_HEAD_SIZE);

	RtlCopyMemory(pfehFileEncryptionHead,wHeaderLogo,ENCRYPTION_HEAD_LOGO_SIZE);

	pfehFileEncryptionHead->nFileValidLength	= pscFileStreamContext->nFileValidLength.QuadPart;
	pfehFileEncryptionHead->nFileRealSize		= pscFileStreamContext->nFileSize.QuadPart;

	return STATUS_SUCCESS;
}

/*---------------------------------------------------------
函数名称:	FctDeconstructFileHead
函数描述:	释放文件流上下文
			pscFileStreamContext	文件流上下文指针
			pFileHead				保存头的地址
输出参数:	pscFileStreamContext	解包好的上下文

返回值:		STATUS_SUCCESS 为成功 
其他:		需要调用者读取好文件头
更新维护:	2011.7.28    最初版本
---------------------------------------------------------*/
NTSTATUS
FctDeconstructFileHead(
    __inout	PFILE_STREAM_CONTEXT  pscFileStreamContext,
	__in PVOID	pFileHead
    )
{
	PFILE_ENCRYPTION_HEAD pfehFileEncryptionHead 
		= (PFILE_ENCRYPTION_HEAD)pFileHead;

	//FILE_STREAM_CONTEXT_LOCK_ON(pscFileStreamContext);

	pscFileStreamContext->nFileValidLength.QuadPart = pfehFileEncryptionHead->nFileValidLength;
	pscFileStreamContext->nFileSize.QuadPart		= pfehFileEncryptionHead->nFileRealSize;
	pscFileStreamContext->bUpdateWhenClose			= FALSE;

	//FILE_STREAM_CONTEXT_LOCK_OFF(pscFileStreamContext);

	return STATUS_SUCCESS;
}

/*---------------------------------------------------------
函数名称:	FctUpdateFileValidSize
函数描述:	设置文件有效大小
			pscFileStreamContext	文件流上下文指针
			pnFileValidSize			文件有效大小
			bSetUpdateWhenClose		是否设置上UpdateWhenClose
									标志
输出参数:	pscFileStreamContext	设置好的上下文

返回值:		无 
其他:		内联函数

			如果bSetUpdateWhenClose为TRUE 则将自动设置
			UpdateWhenClose标志

更新维护:	2011.7.28    最初版本
---------------------------------------------------------*/
//FORCEINLINE 
VOID 
FctUpdateFileValidSize(
    __inout	PFILE_STREAM_CONTEXT  pscFileStreamContext,
	__in	PLARGE_INTEGER		  pnFileValidSize,
	__in	BOOLEAN				  bSetUpdateWhenClose
    )
{
	//FILE_STREAM_CONTEXT_LOCK_ON(pscFileStreamContext);

	pscFileStreamContext->nFileValidLength.QuadPart = pnFileValidSize->QuadPart;

	if(bSetUpdateWhenClose){

		pscFileStreamContext->bUpdateWhenClose		= TRUE;
	}

	//FILE_STREAM_CONTEXT_LOCK_OFF(pscFileStreamContext);
}

/*---------------------------------------------------------
函数名称:	FctGetFileValidSize
函数描述:	设置文件有效大小
			pscFileStreamContext	文件流上下文指针
			pnFileValidSize			存放文件有效大小的指针

输出参数:	pnFileValidSize			文件有效大小

返回值:		无 
其他:		内联函数
			pnFileValidSize 需要指向有效地址
更新维护:	2011.7.28    最初版本
---------------------------------------------------------*/
//FORCEINLINE 
VOID 
FctGetFileValidSize(
    __in	PFILE_STREAM_CONTEXT  pscFileStreamContext,
	__inout	PLARGE_INTEGER		  pnFileValidSize
    )
{
	//FILE_STREAM_CONTEXT_LOCK_ON(pscFileStreamContext);

	pnFileValidSize->QuadPart = pscFileStreamContext->nFileValidLength.QuadPart;

	//FILE_STREAM_CONTEXT_LOCK_OFF(pscFileStreamContext);
}


/*---------------------------------------------------------
函数名称:	FctUpdateFileValidSizeIfLonger
函数描述:	如果传入大小更长的话,设置文件有效大小
			pscFileStreamContext	文件流上下文指针
			pnFileValidSize			文件有效大小
			bSetUpdateWhenClose		是否设置上UpdateWhenClose
									标志
输出参数:	pscFileStreamContext	设置好的上下文

返回值:		TRUE 为成功设置,FALSE为没有设置 
其他:		内联函数

			如果bSetUpdateWhenClose为TRUE 则将自动设置
			UpdateWhenClose标志,但如果没有更新文件有效大小
			标志不会被设置.

更新维护:	2011.7.28    最初版本
---------------------------------------------------------*/
//FORCEINLINE 
BOOLEAN 
FctUpdateFileValidSizeIfLonger(
    __inout	PFILE_STREAM_CONTEXT  pscFileStreamContext,
	__in	PLARGE_INTEGER		  pnFileValidSize,
	__in	BOOLEAN				  bSetUpdateWhenClose
    )
{
	BOOLEAN bReturn = FALSE;

	//FILE_STREAM_CONTEXT_LOCK_ON(pscFileStreamContext);
	
	if(pnFileValidSize->QuadPart > pscFileStreamContext->nFileValidLength.QuadPart){
	
		pscFileStreamContext->nFileValidLength.QuadPart = pnFileValidSize->QuadPart;

		if(bSetUpdateWhenClose){

			pscFileStreamContext->bUpdateWhenClose		= TRUE;
		}

		bReturn = TRUE;
	}
	//FILE_STREAM_CONTEXT_LOCK_OFF(pscFileStreamContext);

	return bReturn;
}

/*---------------------------------------------------------
函数名称:	FctUpdateFileConfidentialCondition
函数描述:	设置当前文件是否为机密文件
			pscFileStreamContext	文件流上下文指针
			fetFileEncryptedType	文件类型

输出参数:	pscFileStreamContext	设置好的上下文

返回值:		无 
其他:		内联函数

			fetFileEncryptedType可选值有
		
			ENCRYPTED_TYPE_UNKNOWN			不知道到底如何
			ENCRYPTED_TYPE_CONFIDENTIAL		文件是机密的
			ENCRYPTED_TYPE_NOT_CONFIDENTIAL 文件是非机密的

更新维护:	2011.7.28    最初版本
---------------------------------------------------------*/
//FORCEINLINE 
VOID 
FctUpdateFileConfidentialCondition(
    __inout	PFILE_STREAM_CONTEXT  pscFileStreamContext,
	__in	FILE_ENCRYPTED_TYPE	  fetFileEncryptedType
    )
{
	//FILE_STREAM_CONTEXT_LOCK_ON(pscFileStreamContext);

	pscFileStreamContext->fctEncrypted = fetFileEncryptedType;

	//FILE_STREAM_CONTEXT_LOCK_OFF(pscFileStreamContext);
}

/*---------------------------------------------------------
函数名称:	FctGetFileConfidentialCondition
函数描述:	获取当前文件是否为机密文件
			pscFileStreamContext	文件流上下文指针

输出参数:	无

返回值:		文件当前情况 取值有

			ENCRYPTED_TYPE_UNKNOWN			不知道到底如何
			ENCRYPTED_TYPE_CONFIDENTIAL		文件是机密的
			ENCRYPTED_TYPE_NOT_CONFIDENTIAL 文件是非机密的

其他:		内联函数

更新维护:	2011.7.28    最初版本
---------------------------------------------------------*/
//FORCEINLINE 
FILE_ENCRYPTED_TYPE 
FctGetFileConfidentialCondition(
    __in	PFILE_STREAM_CONTEXT  pscFileStreamContext
    )
{
	FILE_ENCRYPTED_TYPE	  fetFileEncryptedType;

	//FILE_STREAM_CONTEXT_LOCK_ON(pscFileStreamContext);

	fetFileEncryptedType = pscFileStreamContext->fctEncrypted;

	//FILE_STREAM_CONTEXT_LOCK_OFF(pscFileStreamContext);

	return fetFileEncryptedType;
}

/*---------------------------------------------------------
函数名称:	FctDereferenceFileContext
函数描述:	引用计数减一
			pscFileStreamContext	文件流上下文指针

输出参数:	pscFileStreamContext	设置好的上下文

返回值:		无 
其他:		内联函数
			用于确认是否文件被全部关闭,获得重写加密头时机

更新维护:	2011.7.28    最初版本
---------------------------------------------------------*/
//FORCEINLINE 
VOID 
FctDereferenceFileContext(
    __inout	PFILE_STREAM_CONTEXT  pscFileStreamContext
    )
{
	//FILE_STREAM_CONTEXT_LOCK_ON(pscFileStreamContext);

	ASSERT( pscFileStreamContext->ulReferenceTimes > 0);

	--(pscFileStreamContext->ulReferenceTimes);

	//FILE_STREAM_CONTEXT_LOCK_OFF(pscFileStreamContext);
}
/*---------------------------------------------------------
函数名称:	FctReferenceFileContext
函数描述:	引用计数加一
			pscFileStreamContext	文件流上下文指针

输出参数:	pscFileStreamContext	设置好的上下文

返回值:		无 
其他:		内联函数
			用于确认是否文件被全部关闭,获得重写加密头时机

更新维护:	2011.7.28    最初版本
---------------------------------------------------------*/
//FORCEINLINE 
VOID 
FctReferenceFileContext(
    __inout	PFILE_STREAM_CONTEXT  pscFileStreamContext
    )
{

	//FILE_STREAM_CONTEXT_LOCK_ON(pscFileStreamContext);

	ASSERT( pscFileStreamContext->ulReferenceTimes != 0);

	++(pscFileStreamContext->ulReferenceTimes);

	//FILE_STREAM_CONTEXT_LOCK_OFF(pscFileStreamContext);
}

/*---------------------------------------------------------
函数名称:	FctIsReferenceCountZero
函数描述:	引用计数是否降到0
			pscFileStreamContext	文件流上下文指针

输出参数:	无

返回值:		无 
其他:		内联函数
			用于确认是否文件被全部关闭,获得重写加密头时机

更新维护:	2011.7.28    最初版本
---------------------------------------------------------*/
//FORCEINLINE 
BOOLEAN 
FctIsReferenceCountZero(
    __in	PFILE_STREAM_CONTEXT  pscFileStreamContext
    )
{
	BOOLEAN bIsZero;

	//FILE_STREAM_CONTEXT_LOCK_ON(pscFileStreamContext);

	bIsZero = !pscFileStreamContext->ulReferenceTimes;

	//FILE_STREAM_CONTEXT_LOCK_OFF(pscFileStreamContext);

	return bIsZero;
}

/*---------------------------------------------------------
函数名称:	FctSetUpdateWhenCloseFlag
函数描述:	设置UpdateWhenCloseFlag标志
			pscFileStreamContext	文件流上下文指针
			bSet					需要重写还是不需要

输出参数:	pscFileStreamContext	设置好的上下文

返回值:		无 
其他:		内联函数
			
			bSet 为True表示需要重刷 False表示不用

更新维护:	2011.7.28    最初版本
---------------------------------------------------------*/
//FORCEINLINE 
VOID 
FctSetUpdateWhenCloseFlag(
    __inout	PFILE_STREAM_CONTEXT	pscFileStreamContext,
	__in	BOOLEAN					bSet
    )
{
	//FILE_STREAM_CONTEXT_LOCK_ON(pscFileStreamContext);

	pscFileStreamContext->bUpdateWhenClose = bSet;

	//FILE_STREAM_CONTEXT_LOCK_OFF(pscFileStreamContext);
}

/*---------------------------------------------------------
函数名称:	FctIsUpdateWhenCloseFlag
函数描述:	是否设置了UpdateWhenCloseFlag标志

			pscFileStreamContext	文件流上下文指针

输出参数:	无

返回值:		无 
其他:		内联函数
			
			返回值为True表示需要刷 False表示不需要

更新维护:	2011.7.28    最初版本
---------------------------------------------------------*/
//FORCEINLINE 
BOOLEAN 
FctIsUpdateWhenCloseFlag(
    __inout	PFILE_STREAM_CONTEXT	pscFileStreamContext
    )
{
	BOOLEAN bUpdateWhenClose;

	//FILE_STREAM_CONTEXT_LOCK_ON(pscFileStreamContext);

	bUpdateWhenClose = pscFileStreamContext->bUpdateWhenClose;

	//FILE_STREAM_CONTEXT_LOCK_OFF(pscFileStreamContext);

	return bUpdateWhenClose;
}

/*---------------------------------------------------------
函数名称:	FctReleaseStreamContext
函数描述:	释放上下文,其中加入了同步
			用于替换FltReleaseContext

			pscFileStreamContext	文件流上下文指针

输出参数:	无

返回值:		无 
其他:		内联函数
			
更新维护:	2011.7.28    最初版本
---------------------------------------------------------*/
//FORCEINLINE
VOID FctReleaseStreamContext(
    __inout	PFILE_STREAM_CONTEXT	pscFileStreamContext
	)
{
	FILE_STREAM_CONTEXT_LOCK_OFF(pscFileStreamContext);
	FltReleaseContext(pscFileStreamContext);
}