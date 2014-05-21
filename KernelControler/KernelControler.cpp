// KernelControler.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include "KernelControler.h"

CFilterDriverObject::CFilterDriverObject()
{
	//
	//构造时自动连接驱动程序
	//
	DWORD dwResult = FilterConnectCommunicationPort(
		_T(COMMUNICATE_PORT_NAME),
		0,
		NULL,
		0,
		NULL,
		&hConnectionPort
		);

	if(dwResult != S_OK){
		throw _T("Cannot establish driver connection.");
		hConnectionPort = NULL;
	}
}

CFilterDriverObject::~CFilterDriverObject()
{
	//
	//析构时关闭链接
	//
	if(hConnectionPort){
		CloseHandle(hConnectionPort);
	}
	//MessageBox(NULL,_T("Free successful"),_T("FREE"),MB_OK);
}

BOOL CFilterDriverObject::AddConfidentialProcess(
	LPCWSTR wProcessName, 
	LPCWSTR wProcessPath,
	LPCWSTR wProcessMD5
	)
{

	DWORD dwBytesReturned;//返回的数据大小
	DWORD dwPackSize;//包大小
	PCOMMAND_MESSAGE pCommondMessage;//数据包

	PCOMMAND_MESSAGE pReplyMessage = //返回数据
		(PCOMMAND_MESSAGE)malloc(sizeof(COMMAND_MESSAGE));
	
	if(!PackProcessData(
			wProcessName, 
			wProcessPath,
			wProcessMD5,
			ENUM_ADD_PROCESS,
			&pCommondMessage,
			&dwPackSize
			)){
		return FALSE;
	}

	DWORD dwResult = FilterSendMessage(
		hConnectionPort,
		pCommondMessage,
		dwPackSize,
		pReplyMessage,
		sizeof(COMMAND_MESSAGE),
		&dwBytesReturned
		);
/*
	if(dwResult != S_OK ||
		dwBytesReturned != sizeof(COMMAND_MESSAGE)||
		pReplyMessage->acCommand != ENUM_OPERATION_SUCCESSFUL){
		throw _T("Add process failed.");
		return FALSE;		
	}
	if(dwResult != S_OK){
		throw _T("Add process failed indicated by return value.");
		return FALSE;
	}

	if(dwBytesReturned != sizeof(COMMAND_MESSAGE)){
		throw _T("Add process failed indicated by bytes returned.");
		return FALSE;
	}

	if(pReplyMessage->acCommand != ENUM_OPERATION_SUCCESSFUL){
		throw _T("Add process failed indicated reply message.");
		return FALSE;
	}*/
	//
	//收尾工作
	//
	free(pCommondMessage);
	free(pReplyMessage);
	return TRUE;
}

BOOL CFilterDriverObject::DeleteConfidentialProcess(
	LPCWSTR wProcessName, 
	LPCWSTR wProcessPath,
	LPCWSTR wProcessMD5
	)
{

	DWORD dwBytesReturned;//返回的数据大小
	DWORD dwPackSize;//包大小
	PCOMMAND_MESSAGE pCommondMessage;//数据包
	
	PCOMMAND_MESSAGE pReplyMessage = //返回数据
		(PCOMMAND_MESSAGE)malloc(sizeof(COMMAND_MESSAGE));

	if(!PackProcessData(
			wProcessName, 
			wProcessPath,
			wProcessMD5,
			ENUM_DELETE_PROCESS,
			&pCommondMessage,
			&dwPackSize
			)){
		return FALSE;
	}

	DWORD dwResult = FilterSendMessage(
		hConnectionPort,
		pCommondMessage,
		dwPackSize,
		pReplyMessage,
		sizeof(COMMAND_MESSAGE),
		&dwBytesReturned
		);
/*
	if(dwResult != S_OK ||
		dwBytesReturned != sizeof(COMMAND_MESSAGE)||
		pReplyMessage->acCommand != ENUM_OPERATION_SUCCESSFUL){
		throw _T("Add process failed.");
		return FALSE;		
	}
	if(dwResult != S_OK){
		throw _T("Add process failed indicated by return value.");
		return FALSE;
	}

	if(dwBytesReturned != sizeof(COMMAND_MESSAGE)){
		throw _T("Add process failed indicated by bytes returned.");
		return FALSE;
	}

	if(pReplyMessage->acCommand != ENUM_OPERATION_SUCCESSFUL){
		throw _T("Add process failed indicated reply message.");
		return FALSE;
	}*/
	//
	//收尾工作
	//
	free(pCommondMessage);
	free(pReplyMessage);
	return TRUE;
}

//用完了记得释放内存
BOOL CFilterDriverObject::PackProcessData(
		LPCWSTR wProcessName, //进程名
		LPCWSTR wProcessPath,//路径
		LPCWSTR wProcessMD5,//md5
		ANTINVADER_COMMAND acCommond,//命令
		PCOMMAND_MESSAGE * dpMessage,//输出打好包的数据地址
		DWORD * ddwMessageSize//输出包大小
		)
{
	LPVOID pAddress;//拷贝地址 用于计算拷贝数据所在位置
	//
	//计算数据结构大小
	//
	size_t stProcessName = (_tcslen(wProcessName) + 1)*sizeof(TCHAR);
	size_t stProcessPath = (_tcslen(wProcessPath) + 1)*sizeof(TCHAR);
	size_t stProcessMD5 = (_tcslen(wProcessMD5) + 1)*sizeof(TCHAR);

	size_t stSize = stProcessName  + stProcessPath + stProcessMD5 +	sizeof(COMMAND_MESSAGE);

	//
	//申请内存
	//					
	PCOMMAND_MESSAGE pCommondMessage = 
		(PCOMMAND_MESSAGE)malloc(stSize);
	
	if(!pCommondMessage){
		throw _T("Insufficient memory");
		return FALSE;
	}

	//
	//填充数据
	//
	pCommondMessage->lSize = stSize;
	pCommondMessage->acCommand = acCommond;

	pAddress = (LPVOID)((DWORD)pCommondMessage+sizeof(COMMAND_MESSAGE));
	memcpy(pAddress,wProcessName,stProcessName);

	pAddress = (LPVOID)((DWORD)pAddress + stProcessName);
	memcpy(pAddress,wProcessPath,stProcessPath);

	pAddress = (LPVOID)((DWORD)pAddress + stProcessPath);
	memcpy(pAddress,wProcessMD5,stProcessMD5);

	*dpMessage = pCommondMessage;
	*ddwMessageSize = stSize;
	return TRUE;
}

