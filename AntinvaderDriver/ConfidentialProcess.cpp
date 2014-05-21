///////////////////////////////////////////////////////////////////////////////
///
/// 版权所有 (c) 2011 - 2012
///
/// 原始文件名称	 : ConfidentialProcess.cpp
/// 工程名称         : AntinvaderDriver
/// 创建时间         : 2011-03-20
/// 	         
///
/// 描述	         : 机密进程数据维护实现文件 包括:
///					   机密进程Hash表
///					   机密进程相关数据
///
/// 更新维护:
///  0000 [2011-03-20] 最初版本.
///
///////////////////////////////////////////////////////////////////////////////

#include "ConfidentialProcess.h"

/*---------------------------------------------------------
函数名称:	PctInitializeHashTable
函数描述:	初始化Hash表
输入参数:
输出参数:
返回值:		
			Hash表是否初始化成功 TRUE为成功 否则为FALSE
其他:
更新维护:	2011.3.23    最初版本
            2011.4.5     重构使用通用算法
---------------------------------------------------------*/
BOOLEAN	 PctInitializeHashTable()
{
	//
	//直接调用HashInitialize初始化
	//
	return HashInitialize(
		&phtProcessHashTableDescriptor,
		CONFIDENTIAL_PROCESS_TABLE_POINT_NUMBER
		);
	return TRUE;
}

/*---------------------------------------------------------
函数名称:	PctGetProcessHash
函数描述:	计算指定进程数据的Hash值
输入参数:	ppdProcessData	进程信息
输出参数:
返回值:		Hash值
其他:		Hash使用路径计算
更新维护:	2011.3.25    最初版本 
---------------------------------------------------------*/
ULONG PctGetProcessHash(
	__in PCONFIDENTIAL_PROCESS_DATA ppdProcessData 
	)
{
	//
	//应该使用路径计算Hash 测试目的使用镜像名计算 
	//

	return ELFhashUnicode( &ppdProcessData -> usName,
									CONFIDENTIAL_PROCESS_TABLE_POINT_NUMBER);
}

/*---------------------------------------------------------
函数名称:	PctConvertProcessDataToStaticAddress
函数描述:	初始化一个将要加入Hash表中的数据节点
输入参数:	ppdProcessData		进程信息
输出参数:	dppdNewProcessData	新节点地址
返回值:		是否初始化成功 TRUE为成功 FALSE为失败
其他:		
更新维护:	2011.3.26    最初版本 
---------------------------------------------------------*/
BOOLEAN PctConvertProcessDataToStaticAddress(
	__in PCONFIDENTIAL_PROCESS_DATA ppdProcessData,
	__inout  PCONFIDENTIAL_PROCESS_DATA * dppdNewProcessData
	)
{
	//分配新结构内存
	PCONFIDENTIAL_PROCESS_DATA ppdNewProcessData
		= (PCONFIDENTIAL_PROCESS_DATA)ExAllocatePoolWithTag(
					NonPagedPool,//非分页内存
					sizeof(CONFIDENTIAL_PROCESS_DATA),//一个数据结构大小
					MEM_TAG_PROCESS_TABLE
					);
	//名称
	PWCHAR pwName = 
		(PWCHAR)ExAllocatePoolWithTag(
					NonPagedPool,//非分页内存
					ppdProcessData->usName.MaximumLength,//一个数据结构大小
					MEM_TAG_PROCESS_TABLE
					);

	//路径
	PWCHAR pwPath = 
		(PWCHAR)ExAllocatePoolWithTag(
					NonPagedPool,//非分页内存
					ppdProcessData->usPath.MaximumLength,//一个数据结构大小
					MEM_TAG_PROCESS_TABLE
					);
	
	//Md5摘要
	PWCHAR pwMd5Digest = 
		(PWCHAR)ExAllocatePoolWithTag(
					NonPagedPool,//非分页内存
					ppdProcessData->usMd5Digest.MaximumLength,//一个数据结构大小
					MEM_TAG_PROCESS_TABLE
					);

	//
	//如果资源不足 分配失败 理论上是不大可能的
	//

	if(!((ULONG)ppdNewProcessData&
		 (ULONG)pwName&
		 (ULONG)pwPath&
		 (ULONG)pwMd5Digest)){
		return FALSE;
	}

	//
	//初始化字符串
	//

	RtlInitEmptyUnicodeString(
		&ppdNewProcessData->usMd5Digest,
		pwMd5Digest,
		ppdProcessData->usMd5Digest.MaximumLength
		);

	RtlInitEmptyUnicodeString(
		&ppdNewProcessData->usPath,
		pwPath,
		ppdProcessData->usName.MaximumLength
		);
	RtlInitEmptyUnicodeString(
		&ppdNewProcessData->usName,
		pwName,
		ppdProcessData->usName.MaximumLength
		);

	//
	//拷贝字符串
	//

	RtlCopyUnicodeString(&ppdNewProcessData->usMd5Digest,&ppdProcessData->usMd5Digest);
	RtlCopyUnicodeString(&ppdNewProcessData->usPath,&ppdProcessData->usPath);
	RtlCopyUnicodeString(&ppdNewProcessData->usName,&ppdProcessData->usName);

	//
	//返回
	//
	*dppdNewProcessData =  ppdNewProcessData;

	return TRUE;
}

/*---------------------------------------------------------
函数名称:	PctAddProcess
函数描述:	向机密进程表中加入一个进程
输入参数:	ppdProcessData	进程信息
输出参数:	
返回值:		
			TRUE为成功 否则为FALSE
其他:		ppdProcessData会自动被转换为静态地址,调用者
			只需要提供数据即可
更新维护:	2011.3.23  最初版本
			2011.4.26  ppdProcessData不必初始化
---------------------------------------------------------*/
BOOLEAN PctAddProcess( 
	__in PCONFIDENTIAL_PROCESS_DATA ppdProcessData 
	)
{
	//
	//增加数据前地址已经被初始化过
	//
	BOOLEAN bReturn;

	PCONFIDENTIAL_PROCESS_DATA ppdNewProcessData;

	//
	//转换成静态地址,把栈中的数据全部置换出来
	//
	bReturn = PctConvertProcessDataToStaticAddress(
		ppdProcessData,
		&ppdNewProcessData
		);

	//
	//如果初始化失败
	//
	if(!bReturn){
		return FALSE;
	}
	
	//
	//计算Hash
	//

	ULONG ulHash = PctGetProcessHash(ppdProcessData);

	bReturn = HashInsertByHash(phtProcessHashTableDescriptor,
		ulHash,
		ppdNewProcessData,
		sizeof(CONFIDENTIAL_PROCESS_DATA)
		);

	//
	//如果失败了 释放掉刚才转换成静态地址申请的内存
	//
	if(!bReturn){
		PctFreeProcessDataStatic(ppdNewProcessData,TRUE);
	}

	return bReturn;
}

/*---------------------------------------------------------
函数名称:	PctIsProcessDataAccordance
函数描述:	判断两进程数据是否批评
输入参数:	ppdProcessDataOne		一个进程数据
			ppdProcessDataAnother	另一个进程数据
			dwFlags					标志 可选值如下 可用或连接

			CONFIDENTIAL_PROCESS_COMPARISON_NAME 比较名称
			CONFIDENTIAL_PROCESS_COMPARISON_PATH 比较路径
			CONFIDENTIAL_PROCESS_COMPARISON_MD5	 比较Md5

输出参数:
返回值:		
			如果相等则为0,不等则返回值为以下值的或结果

			CONFIDENTIAL_PROCESS_COMPARISON_NAME 名称不同
			CONFIDENTIAL_PROCESS_COMPARISON_PATH 路径不同
			CONFIDENTIAL_PROCESS_COMPARISON_MD5	 Md5不同

其他:		如果不需要校验Md5 PCONFIDENTIAL_PROCESS_DATA->
			usMd5Digest可不填

更新维护:	2011.3.25    最初版本
---------------------------------------------------------*/
ULONG  PctIsProcessDataAccordance(
	__in PCONFIDENTIAL_PROCESS_DATA ppdProcessDataOne ,
	__in PCONFIDENTIAL_PROCESS_DATA ppdProcessDataAnother,
	__in ULONG ulFlags
	)
{
	LONG lRet;	 //本函数调用的参数返回记录
	ULONG ulRet; //本函数的返回记录
	
	//
	//输入的地址一定非零
	//

	ASSERT( ppdProcessDataOne );
	ASSERT( ppdProcessDataAnother );

	//赋初值
	ulRet = 0;

	if(ulFlags & CONFIDENTIAL_PROCESS_COMPARISON_MD5){
		//
		//如果需要校验Md5值,则进行比较,大小写敏感
		//
		lRet = RtlCompareUnicodeString( &ppdProcessDataOne -> usMd5Digest ,//第一个md5
										&ppdProcessDataAnother -> usMd5Digest ,//第二个md5
										TRUE);//大小写敏感

		if(lRet){//如果不相等则返回
			ulRet |= CONFIDENTIAL_PROCESS_COMPARISON_MD5;
		}

	}
	
	if(ulFlags & CONFIDENTIAL_PROCESS_COMPARISON_NAME){
		//
		//如果需要校验名称,则进行比较,大小写敏感
		//
		lRet = RtlCompareUnicodeString( &ppdProcessDataOne->usName ,//第一个名称
										&ppdProcessDataAnother->usName ,//第二个名称
										FALSE );//大小写不敏感
		if(lRet){//如果不相等则返回
			ulRet |= CONFIDENTIAL_PROCESS_COMPARISON_NAME;
		}
	}
	
	if(ulFlags & CONFIDENTIAL_PROCESS_COMPARISON_PATH){
		//
		//如果需要校验路径名称,则进行比较,大小写不敏感
		//
		lRet = RtlCompareUnicodeString( &ppdProcessDataOne->usPath ,//第一个路径
										&ppdProcessDataAnother->usPath ,//第二个路径
										FALSE );//大小写不敏感

		if(lRet){//如果不相等则返回
		
			ulRet |= CONFIDENTIAL_PROCESS_COMPARISON_PATH;
		}
	}

	return ulRet;
}

/*---------------------------------------------------------
函数名称:	PctIsDataMachedCallback
函数描述:	判断记录是否想通,作为HashSearchByHash回调
输入参数:	lpContext		作为对照的文件数据
输出参数:	lpNoteData		Hash表中的数据(文件数据)
返回值:		
			两数据是否相等 TRUE为相等 否则为FALSE
其他:		

更新维护:	2011.4.5     最初版本
			2011.7.16    修改了判断方式
---------------------------------------------------------*/
static
BOOLEAN PctIsDataMachedCallback(
	__in PVOID lpContext,
	__in PVOID lpNoteData
	)
{
	//
	//使用PctIsProcessDataAccordance判断,MD5和路径暂时不启用
	//
	return !PctIsProcessDataAccordance(
		(PCONFIDENTIAL_PROCESS_DATA)lpContext,
		(PCONFIDENTIAL_PROCESS_DATA)lpNoteData,
		CONFIDENTIAL_PROCESS_COMPARISON_NAME//校验名称
		);
}

/*---------------------------------------------------------
函数名称:	PctGetSpecifiedProcessDataAddress
函数描述:	获取指定的进程在Hash表中地址
输入参数:	ppdProcessDataSource	用于对照的数据
			dppdProcessDataInTable	保存Hash地址的缓冲区
输出参数:	dppdProcessDataInTable	返回Hash表地址
返回值:		
			两数据是否相等 TRUE为相等 否则为FALSE
其他:		
			若没有找到,dppdProcessDataInTable将返回0

更新维护:	2011.3.25    最初版本
---------------------------------------------------------*/
BOOLEAN PctGetSpecifiedProcessDataAddress(
	__in  PCONFIDENTIAL_PROCESS_DATA ppdProcessDataSource ,
	__inout_opt PCONFIDENTIAL_PROCESS_DATA * dppdProcessDataInTable 
	)
{
	//Hash
	ULONG ulHash;

	//返回值
	BOOLEAN bReturn;

	//找到的地址
	PHASH_NOTE_DESCRIPTOR pndNoteDescriptor;

	//
	//获取进程Hash
	//
	ulHash = PctGetProcessHash( ppdProcessDataSource );

	//
	//直接用Hash搜索
	//

	bReturn = HashSearchByHash(
		phtProcessHashTableDescriptor,
		ulHash,
		PctIsDataMachedCallback,
		ppdProcessDataSource,
		&pndNoteDescriptor
		);

	//
	//判断是否成功,和是否需要保存数据
	//
	if( bReturn ){
		if(dppdProcessDataInTable){
			//
			//成功了就保存数据
			//
			*dppdProcessDataInTable = 
				(PCONFIDENTIAL_PROCESS_DATA)(pndNoteDescriptor -> lpData);
		}
		return TRUE;
	}
	return FALSE;
}

/*---------------------------------------------------------
函数名称:	PctUpdateProcessMd5
函数描述:	更新指定进程的数据
输入参数:	ppdProcessDataInTable	Hash表中地址
			ppdProcessDataSource	有Md5的数据
输出参数:
返回值:		
其他:		ppdProcessDataSource中
			PCONFIDENTIAL_PROCESS_DATA->usName和usPath可不填

更新维护:	2011.3.25    最初版本
---------------------------------------------------------*//*
VOID PctUpdateProcessMd5(
		__in  PCONFIDENTIAL_PROCESS_DATA ppdProcessDataInTable ,
		__in  PCONFIDENTIAL_PROCESS_DATA ppdProcessDataSource
	)
{
	//
	//输入的地址一定正确
	//
	ASSERT( ppdProcessDataSource );
	ASSERT( ppdProcessDataInTable );
	PROCESS_TABLE_LOCK_ON;

	//
	//更新Md5值 直接拷贝
	//
	RtlCopyMemory(
		&ppdProcessDataInTable -> usMd5Digest,
		&ppdProcessDataSource -> usMd5Digest,
		sizeof(UNICODE_STRING)
		);

	PROCESS_TABLE_LOCK_OFF;
}*/

/*---------------------------------------------------------
函数名称:	PctFreeProcessDataStatic
函数描述:	释放进程表中节点内存
输入参数:	ppdProcessData	要释放数据Hash表地址
			bFreeDataBase   是否需要把ppdProcessData本身释放了
输出参数:
返回值:		
其他:		ppdProcessData为Hash表中对应的地址,而非构造
更新维护:	2011.3.26    最初版本
---------------------------------------------------------*/
VOID PctFreeProcessDataStatic(
	__in  PCONFIDENTIAL_PROCESS_DATA ppdProcessData ,
	__in  BOOLEAN bFreeDataBase 
	)
{
	if(ppdProcessData -> usName . Buffer){
		ExFreePool( ppdProcessData -> usName . Buffer );
	}

	if(ppdProcessData -> usPath . Buffer){
		ExFreePool( ppdProcessData -> usPath . Buffer );
	}

	if(ppdProcessData-> usMd5Digest . Buffer){
		ExFreePool( ppdProcessData ->usMd5Digest . Buffer );
	}

	if(bFreeDataBase){
		ExFreePool( ppdProcessData );
	}
}
/*---------------------------------------------------------
函数名称:	PctFreeHashMemoryCallback
函数描述:	HashDelete回调
输入参数:	lpNoteData	要删除文件数据节点地址
输出参数:
返回值:		
其他:		
更新维护:	2011.4.5     最初版本
---------------------------------------------------------*/
static
VOID 
PctFreeHashMemoryCallback (
	__in PVOID lpNoteData
	)
{
	PctFreeProcessDataStatic((PCONFIDENTIAL_PROCESS_DATA)lpNoteData,FALSE);
}

/*---------------------------------------------------------
函数名称:	PctDeleteProcess
函数描述:	删除进程的数据
输入参数:	ppdProcessData	要删除进程数据Hash表地址
输出参数:
返回值:		TRUE 为成功 FALSE为失败
其他:		
更新维护:	2011.3.25    最初版本
			2011.4.5     修改为使用通用库
---------------------------------------------------------*/
BOOLEAN PctDeleteProcess(
	__in  PCONFIDENTIAL_PROCESS_DATA ppdProcessData 
	)
{
	//地址无误
	ASSERT( ppdProcessData );

	//待删除进程的Hash值
	ULONG ulHash;

	//找到的地址
	PHASH_NOTE_DESCRIPTOR pndNoteDescriptor;

	//计算Hash值
	ulHash = PctGetProcessHash( ppdProcessData );

	//返回值
	BOOLEAN bReturn;
	
	//
	//直接用Hash搜索
	//

	bReturn = HashSearchByHash(
		phtProcessHashTableDescriptor,
		ulHash,
		PctIsDataMachedCallback,
		ppdProcessData,
		&pndNoteDescriptor
		);

	if( !bReturn ){
		return FALSE;
	}

	//
	//删除节点
	//
	return HashDelete(
		phtProcessHashTableDescriptor,
		pndNoteDescriptor,
		PctFreeHashMemoryCallback,
		TRUE
		);
}
/*---------------------------------------------------------
函数名称:	PctIsPostfixMonitored
函数描述:	判断当前后缀是否被监视
输入参数:	puniPostfix	后缀名
输出参数:
返回值:		
			TRUE表示监视,FALSE表示不监视
其他:
更新维护:	2011.8.23    最初版本 用于测试
---------------------------------------------------------*/
BOOLEAN	 PctIsPostfixMonitored(PUNICODE_STRING puniPostfix)
{
	UNICODE_STRING	uniPostFix1;
	UNICODE_STRING	uniPostFix2;

	RtlInitUnicodeString(&uniPostFix1,L".txt");
	RtlInitUnicodeString(&uniPostFix2,L".doc");

	if(!RtlCompareUnicodeString(puniPostfix,&uniPostFix1,TRUE)){
		return TRUE;
	}

	if(!RtlCompareUnicodeString(puniPostfix,&uniPostFix2,TRUE)){
		return TRUE;
	}

	return FALSE;
}

/*---------------------------------------------------------
函数名称:	PctFreeTable
函数描述:	释放整个Hash表
输入参数:	
输出参数:
返回值:		TRUE 为成功 FALSE为失败
其他:		驱动卸载时使用
更新维护:	2011.3.25    最初版本
			2011.4.5     修改为使用通用库
---------------------------------------------------------*/
BOOLEAN PctFreeTable()
{
	//
	//直接释放
	//
	HashFree(
		phtProcessHashTableDescriptor,
		PctFreeHashMemoryCallback
		);
	return TRUE;
}