///////////////////////////////////////////////////////////////////////////////
///
/// 版权所有 (c) 2011 - 2012
///
/// 原始文件名称	 : BasicAlgorithm.cpp
/// 工程名称         : AntinvaderDriver
/// 创建时间         : 2011-03-20
/// 	         
///
/// 描述	         : 一些基本的算法函数实现,用于生成Hash,遍历链表等
///
/// 更新维护:
///  0000 [2011-03-20] 最初版本.
///
///////////////////////////////////////////////////////////////////////////////

#include "BasicAlgorithm.h"

/*---------------------------------------------------------
函数名称:	ELFhashAnsi
函数描述:	ELF算法计算字符串Hash Ansi版本
输入参数:	pansiKey	需要Hash的字符串
			ulMod		最大Hash值范围
输出参数:	
返回值:		Hash值
其他:		
		ELF hash是对字符串进行hash操作时的常用函数将一个字
		符串的数组中的每个元素依次按前四位与上一个元素的
		低四位相与,组成一个长整形,如果长整的高四位大于零,
		那么就将它折回再与长整的低四位相异或,这样最后得到
		的长整对HASH表长取余,得到在HASH中的位置.
更新维护:	2011.3.20    最初版本
---------------------------------------------------------*/
ULONG ELFhashAnsi( PANSI_STRING pansiKey,ULONG ulMod )
{
    ULONG ulHash = 0;
    ULONG ulTemp;
	
	PCHAR pchBuffer;

	pchBuffer = pansiKey->Buffer;

    while( * pchBuffer ){

        ulHash =( ulHash<< 4) + *pchBuffer++;
        ulTemp = ulHash & 0xf0000000L;
        if( ulTemp ) ulHash ^= ulTemp >> 24;
        ulHash &= ~ulTemp;
    }

    return ulHash % ulMod;
}

/*---------------------------------------------------------
函数名称:	ELFhashUnicode
函数描述:	ELF算法计算字符串Hash Unicode版本 转换后使用了
			Ansi版本
输入参数:	pansiKey	需要Hash的字符串
			ulMod		最大Hash值范围
输出参数:	
返回值:		Hash值
其他:		

更新维护:	2011.3.20    最初版本
			2011.7.16    添加了Ansi申请内存
---------------------------------------------------------*/
ULONG ELFhashUnicode( PUNICODE_STRING pusKey,ULONG ulMod )
{	
	//转换ansi字符串
	ANSI_STRING ansiKey;

	//获取的Hash值
	ULONG ulHash;

	//
 	//转换成Ansi
    //
	RtlUnicodeStringToAnsiString(
		&ansiKey,
		pusKey,
		TRUE//让RtlUnicodeStringToAnsiString申请内存
		);

	//
	//直接用ansi版本
    //

	ulHash = ELFhashAnsi(&ansiKey,ulMod);

	//
	//释放内存
	//
	RtlFreeAnsiString(&ansiKey);

	return ulHash;
}

/*---------------------------------------------------------
函数名称:	HashInitialize
函数描述:	初始化新的Hash表

输入参数:	dpHashTable					保存Hash表数据
			ulMaximumPointNumber		最大Hash值范围

输出参数:	dpHashTable					Hash表数据

返回值:		TRUE为成功 FALSE失败
其他:		
更新维护:	2011.4.2     最初版本
			2011.7.16    将自旋锁改为了互斥体
---------------------------------------------------------*/
BOOLEAN
HashInitialize(
	PHASH_TABLE_DESCRIPTOR * dpHashTable,
	ULONG ulMaximumPointNumber
	)
{
	//传入参数一定正确
	ASSERT( dpHashTable );
	ASSERT( ulMaximumPointNumber );

	//
	//分别申请两块内存,分别是表内存和表描述结构,
	//如果失败则返回,否则填充结构,数据置零然后返回
	//

	PHASH_TABLE_DESCRIPTOR pHashTable;

	pHashTable = (PHASH_TABLE_DESCRIPTOR)ExAllocatePoolWithTag(
						NonPagedPool ,
						sizeof(HASH_TABLE_DESCRIPTOR),
						MEM_HASH_TAG
						);

	if( !pHashTable ){
		return FALSE;
	}

	pHashTable -> ulHashTableBaseAddress
		= (ULONG)ExAllocatePoolWithTag(
						NonPagedPool ,
						HASH_POINT_SIZE * ulMaximumPointNumber,
						MEM_HASH_TAG
						);

	if( !pHashTable ){
		ExFreePool( pHashTable );
		return FALSE;
	}

	RtlZeroMemory(
		(PVOID)pHashTable -> ulHashTableBaseAddress ,
		HASH_POINT_SIZE * ulMaximumPointNumber
		);

	//
	//初始化互斥体 保存信息
	//

	KeInitializeMutex( &pHashTable -> irmMutex , 0 );

	pHashTable -> ulMaximumPointNumber = ulMaximumPointNumber;

	//
	//返回
	//
	*dpHashTable = pHashTable;

	return TRUE;
}

/*---------------------------------------------------------
函数名称:	HashInsertByHash
函数描述:	通过提交Hash值插入Hash表节点

输入参数:	pHashTable			Hash表叙述子
			ulHash				Hash值
			lpData				保存的卫星数据
			ulLength			卫星数据长度

输出参数:
返回值:		TRUE为成功 FALSE失败
其他:		卫星数据将会被拷贝,并在节点删除时被释放
更新维护:	2011.4.4     最初版本
---------------------------------------------------------*/
BOOLEAN
HashInsertByHash( 
	__in PHASH_TABLE_DESCRIPTOR pHashTable,
	__in ULONG ulHash,
	__in PVOID lpData,
	__in ULONG ulLength
	)
{
	//Hash对应的第一个地址
	ULONG ulPointAddress;
		
	//新的节点地址
	PHASH_NOTE_DESCRIPTOR pHashNote;

	//第一个保存的节点地址 用于确定冲突
	PHASH_NOTE_DESCRIPTOR pFirstNote;

	//卫星数据空间地址
	PVOID lpBuffer;

	//
	//获取Hash对应的地址,初始化一个新的节点 失败就返回
	//

	pHashNote = (PHASH_NOTE_DESCRIPTOR)ExAllocatePoolWithTag(
						NonPagedPool ,
						sizeof(HASH_NOTE_DESCRIPTOR),
						MEM_HASH_TAG
						);

	if( !pHashNote ){
		return FALSE;
	}

	ulPointAddress = HASH_NOTE_POINT_ADDRESS( pHashTable , ulHash );

	//
	//申请卫星数据内存并拷贝
	//

	lpBuffer = ExAllocatePoolWithTag(
					NonPagedPool,
					ulLength,
					MEM_HASH_TAG
					);

	if( !lpBuffer ){
		ExFreePool( pHashNote );
		return FALSE;
	}

	RtlCopyMemory(lpBuffer,lpData,ulLength);

	//
	//涉及到链表操作,从这里开始上锁
	//

	HASH_LOCK_ON( pHashTable );


	pFirstNote = *(PHASH_NOTE_DESCRIPTOR *)ulPointAddress;

	if( !pFirstNote ){
		//
		//如果还没有存放过数据,初始化链表,直接写入 并初始化链表头
		//

		*(PULONG)ulPointAddress = (ULONG)pHashNote;

		InitializeListHead( (PLIST_ENTRY) pHashNote);

	}else{
		//
		//如果存在冲突 则插入链表
		//

		InsertTailList( (PLIST_ENTRY)pFirstNote , (PLIST_ENTRY)pHashNote);
	}

	//
	//保存Hash 删除时使用
	//
	pHashNote -> ulHash = ulHash;

	//
	//保存卫星数据地址
	//
	pHashNote ->lpData = lpBuffer;

	HASH_LOCK_OFF( pHashTable );

	return TRUE;
}

/*---------------------------------------------------------
函数名称:	HashInsertByNumber
函数描述:	通过提交数字插入Hash表节点

输入参数:	pHashTable			Hash表叙述子
			ulNumber			用于计算Hash的数字
			lpData				保存的卫星数据
			ulLength			卫星数据长度

输出参数:
返回值:		TRUE为成功 FALSE失败
其他:		卫星数据将会被拷贝,并在节点删除时被释放
更新维护:	2011.4.4     最初版本
---------------------------------------------------------*/
BOOLEAN
HashInsertByNumber( 
	__in PHASH_TABLE_DESCRIPTOR pHashTable,
	__in ULONG ulNumber,
	__in PVOID lpData,
	__in ULONG ulLength
	)
{
	return HashInsertByHash(
				pHashTable,
				ulNumber%(pHashTable->ulMaximumPointNumber),
				lpData,
				ulLength
				);
}

/*---------------------------------------------------------
函数名称:	HashInsertByUnicodeString
函数描述:	通过提交Unicode字符串插入Hash表节点

输入参数:	pHashTable			Hash表叙述子
			pusString			用于计算Hash的Unicode字符串
			lpData				保存的卫星数据
			ulLength			卫星数据长度

输出参数:
返回值:		TRUE为成功 FALSE失败
其他:		卫星数据将会被拷贝,并在节点删除时被释放
更新维护:	2011.4.4     最初版本
---------------------------------------------------------*/
BOOLEAN
HashInsertByUnicodeString(
	__in PHASH_TABLE_DESCRIPTOR pHashTable,
	__in PUNICODE_STRING pusString,
	__in PVOID lpData,
	__in ULONG ulLength
	)
{
	return HashInsertByHash(
				pHashTable,
				ELFhashUnicode(
						pusString,
						pHashTable->ulMaximumPointNumber
						),
				lpData,
				ulLength
				);
}

/*---------------------------------------------------------
函数名称:	HashSearchByHash
函数描述:	通过提交Hash值的方式搜索Hash表节点

输入参数:	pHashTable			Hash表叙述子

			ulHash				Hash值

			CallBack			回调函数,用于判断是否找到,
								详细信息请参见BasicAlgorithm.h

			lpContext			回调函数的上下文,将作为参数传递
								给回调函数

			dpData				保存找到节点地址的空间

输出参数:	dpData				找到的节点地址

返回值:		TRUE为找到 FALSE失败

其他:		如果只是想查询是否在表中 可讲dpData置为NULL		

更新维护:	2011.4.4     最初版本
			2011.7.17    增加了dpData判断   
---------------------------------------------------------*/
BOOLEAN
HashSearchByHash( 
	__in PHASH_TABLE_DESCRIPTOR pHashTable,
	__in ULONG ulHash,
	__in HASH_IS_NOTE_MACHED_CALLBACK CallBack,
	__in PVOID lpContext,
	__inout PHASH_NOTE_DESCRIPTOR * dpData
	)
{
	//当前节点
	PHASH_NOTE_DESCRIPTOR pCurrentHashNote;

	//第一个节点
	PHASH_NOTE_DESCRIPTOR pFirstHashNote;

	//是否已经找到
	BOOLEAN bFind = FALSE;

	//
	//访问了分页内存 一定要在PASSIVE_LEVEL中
	//
	//ASSERT( KeGetCurrentIrql() <= APC_LEVEL );
	PAGED_CODE();

	//
	//直接返回地址
	//涉及到链表操作 使用自旋锁
	//

	HASH_LOCK_ON( pHashTable );

	//
	//获取第一个节点,如果没有数据就返回
	//

	 pFirstHashNote = 
		*(PHASH_NOTE_DESCRIPTOR *)HASH_NOTE_POINT_ADDRESS(pHashTable,ulHash);

	if( !pFirstHashNote ){
		HASH_LOCK_OFF( pHashTable );
		return FALSE;
	}

	//
	//遍历节点,调用回调判断数据是否正确数据
	//

	pCurrentHashNote = pFirstHashNote;

	do{

		bFind = CallBack( lpContext , pCurrentHashNote->lpData );

		if( bFind ){
			break;
		}

		pCurrentHashNote = 
			(PHASH_NOTE_DESCRIPTOR) pCurrentHashNote -> lListEntry .Flink;

	}while( pCurrentHashNote != pFirstHashNote);

	HASH_LOCK_OFF( pHashTable );

	if(dpData){
		*dpData =  pCurrentHashNote;
	}

	return bFind;
}

/*---------------------------------------------------------
函数名称:	HashSearchByNumber
函数描述:	通过提交Hash值的方式插入Hash表节点

输入参数:	pHashTable			Hash表叙述子

			ulNumber			用于计算Hash的数字

			CallBack			回调函数,用于判断是否找到,
								详细信息请参见BasicAlgorithm.h

			lpContext			回调函数的上下文,将作为参数传递
								给回调函数

			dpData				保存找到节点地址的空间

输出参数:	dpData				找到的节点地址

返回值:		TRUE为找到 FALSE失败
其他:		
更新维护:	2011.4.4     最初版本
---------------------------------------------------------*/
BOOLEAN
HashSearchByNumber(
	__in PHASH_TABLE_DESCRIPTOR pHashTable,
	__in ULONG ulNumber,
	__in HASH_IS_NOTE_MACHED_CALLBACK CallBack,
	__in PVOID lpContext,
	__inout PHASH_NOTE_DESCRIPTOR * dpData
	)
{
	return HashSearchByHash(
				pHashTable,
				ulNumber%(pHashTable->ulMaximumPointNumber),
				CallBack,
				lpContext,
				dpData
				);
}

/*---------------------------------------------------------
函数名称:	HashSearchByString
函数描述:	通过提交Hash值的方式插入Hash表节点

输入参数:	pHashTable			Hash表叙述子

			pusString			用于计算Hash的Unicode字符串

			CallBack			回调函数,用于判断是否找到,
								详细信息请参见BasicAlgorithm.h

			lpContext			回调函数的上下文,将作为参数传递
								给回调函数

			dpData				保存找到节点地址的空间

输出参数:	dpData				找到的节点地址

返回值:		TRUE为找到 FALSE失败
其他:		
更新维护:	2011.4.4     最初版本
---------------------------------------------------------*/
BOOLEAN
HashSearchByString(
	__in PHASH_TABLE_DESCRIPTOR pHashTable,
	__in PUNICODE_STRING pusString,
	__in HASH_IS_NOTE_MACHED_CALLBACK CallBack,
	__in PVOID lpContext,
	__inout PHASH_NOTE_DESCRIPTOR * dpData
	)
{
	return HashSearchByHash(
				pHashTable,
				ELFhashUnicode(
						pusString,
						pHashTable->ulMaximumPointNumber
						),
				CallBack,
				lpContext,
				dpData
				);
}

/*---------------------------------------------------------
函数名称:	HashDelete
函数描述:	通过提交Hash值的方式插入Hash表节点

输入参数:	pHashTable	Hash表叙述子
			pHashNote	Hash节点叙述子
			CallBack	回调,不需要可以填NULL
			bLock		是否加锁
输出参数:	

返回值:		TRUE删除成功 否则为FALSE
其他:		回调中不需要释放卫星数据本身,只需要释放其下级空间
更新维护:	2011.4.4     最初版本
---------------------------------------------------------*/
BOOLEAN
HashDelete(
	__in PHASH_TABLE_DESCRIPTOR pHashTable,
	__in PHASH_NOTE_DESCRIPTOR pHashNote,
	__in_opt HASH_DELETE_CALLBACK CallBack,
	__in BOOLEAN bLock
	)
{
	ASSERT( pHashNote );
	ASSERT( pHashTable );

	PHASH_NOTE_DESCRIPTOR * dpNote;

	if( bLock ){
		HASH_LOCK_ON( pHashTable );
	}

	//
	//先计算出Hash表中指向note的指针地址
	//
	dpNote = ( PHASH_NOTE_DESCRIPTOR * )
				HASH_NOTE_POINT_ADDRESS( pHashTable , pHashNote->ulHash);

	if(IsListEmpty( (PLIST_ENTRY) pHashNote )){
		//
		//如果只有一个节点了,那么表中的数据也应该清理
		//
		*dpNote = NULL;

	}else{
		//
		//如果不止一个节点而且节点是第一个,那么要对Hash表中的地址修改
		//
		if(*dpNote == pHashNote){
			*dpNote = (PHASH_NOTE_DESCRIPTOR)pHashNote->lListEntry.Flink;
		}

		//
		//然后安全移除节点
		//
		RemoveEntryList( (PLIST_ENTRY) pHashNote );
	}

	if( bLock ){
		HASH_LOCK_OFF( pHashTable );
	}

	//
	//释放内存 先调用回调
	//
	if( CallBack ){
		CallBack( pHashNote->lpData );
	}

	ExFreePool( pHashNote->lpData );
	ExFreePool( pHashNote );

	return TRUE;
}

/*---------------------------------------------------------
函数名称:	HashFree
函数描述:	释放整个Hash表
输入参数:	pHashTable	Hash表叙述子
			CallBack	删除回调
输出参数:	
返回值:		
其他:		
更新维护:	2011.4.4     最初版本
---------------------------------------------------------*/
VOID
HashFree(
	__in PHASH_TABLE_DESCRIPTOR pHashTable,
	__in_opt HASH_DELETE_CALLBACK CallBack
	)
{
	ASSERT( pHashTable );

	//第一个Hash节点地址的地址
	PHASH_NOTE_DESCRIPTOR * dpCurrentFirstHashNote;

	HASH_LOCK_ON( pHashTable );

	//
	//遍历所有可能的Hash表中地址
	//
	for( ULONG ulCurrentHash = 0 ;
		ulCurrentHash < pHashTable-> ulMaximumPointNumber ;
		ulCurrentHash ++ ){
		//
		//一直删除第一个节点,直到整个链表被清空
		//
		dpCurrentFirstHashNote = 
				(PHASH_NOTE_DESCRIPTOR *)HASH_NOTE_POINT_ADDRESS( pHashTable , ulCurrentHash);

		while( * dpCurrentFirstHashNote != NULL ){

			DebugTrace(DEBUG_TRACE_NORMAL_INFO,"HashFree",("Release table note 0x%X",*dpCurrentFirstHashNote));

			HashDelete(
				pHashTable ,
				*dpCurrentFirstHashNote ,
				CallBack,
				FALSE //由于这里已经加过锁了,Delete时不必加锁
				);
		}
	}

	HASH_LOCK_OFF( pHashTable );

	//
	//所有节点已经删除 现在释放表内存和叙述子内存
	//

	ExFreePool( (PVOID)pHashTable->ulHashTableBaseAddress );
	ExFreePool( (PVOID)pHashTable );
}

///////////////////Debug///////////////////////////
/*---------------------------------------------------------
函数名称:	DbgCheckEntireHashTable
函数描述:	检查整个Hash表
输入参数:	pHashTable	Hash表叙述子
输出参数:	
返回值:		
其他:		使用时请用宏DebugCheckEntireHashTable
更新维护:	2012.12.22     最初版本
---------------------------------------------------------*//*
VOID DbgCheckEntireHashTable(
	__in PHASH_TABLE_DESCRIPTOR pHashTable
	)
{
	KdPrint(("[Antinvader]DbgCheckEntireHashTable entered.\n"));

	PHASH_NOTE_DESCRIPTOR pCurrentHashNote;
	PHASH_NOTE_DESCRIPTOR pHeadNote;

	ASSERT(pHashTable);

	KdPrint(("[Antinvader]pHashTable passed.\n\
			\t\tDiscriptor address:0x%X\n\
			\t\tBase address:0x%X\n\
			\t\tMaximum Point Number:0x%X\n\
			\t\tMutex address:0x%x\n",
		pHashTablepHashTable,
		pHashTablepHashTable->ulHashTableBaseAddress,
		pHashTablepHashTable->ulMaximumPointNumber,
		pHashTablepHashTable->irmMutex));

	//
	//直接返回地址
	//涉及到链表操作 使用自旋锁
	//

	HASH_LOCK_ON( pHashTable );

	//
	//遍历整个表
	//

	for(ULONG ulHash = 0;ulHash < pHashTable -> ulMaximumPointNumber;ulHash++){

		pCurrentHashNote = 
			*(PHASH_NOTE_DESCRIPTOR *)HASH_NOTE_POINT_ADDRESS(pHashTable,ulHash);

		if(pCurrentHashNote){
			//
			//存在有效节点
			//
			KdPrint(("[Antinvader]Node %d found,head address:0x%X\n",ulHash,pCurrentHashNote));
			
			if(IsListEmpty( (PLIST_ENTRY) pHeadNote )){

				KdPrint(("[Antinvader]empty:0x%X\n",ulHash,pCurrentHashNote));

			}else{
				//
				//如果不止一个节点,那么简单移除即可
				//
				RemoveEntryList( (PLIST_ENTRY) pHashNote );
			}
			pHeadNote = pCurrentHashNote;

			while(pCurrentHashNote->lListEntry.Flink)


			
		}
	}

	 KdPrint(("[Antinvader]First node address:0x%X\n",pFirstHashNote));

	if( !pFirstHashNote ){
		KdPrint(("[Antinvader]First node address:0x%X\n",pFirstHashNote));
		HASH_LOCK_OFF( pHashTable );
		return FALSE;
	}

	//
	//遍历节点,调用回调判断数据是否正确数据
	//

	pCurrentHashNote = pFirstHashNote;

	do{

		bFind = CallBack( lpContext , pCurrentHashNote->lpData );

		if( bFind ){
			break;
		}

		pCurrentHashNote = 
			(PHASH_NOTE_DESCRIPTOR) pCurrentHashNote -> lListEntry .Flink;

	}while( pCurrentHashNote != pFirstHashNote);

	HASH_LOCK_OFF( pHashTable );

	if(dpData){
		*dpData =  pCurrentHashNote;
	}

}*/