// 下列 ifdef 块是创建使从 DLL 导出更简单的
// 宏的标准方法。此 DLL 中的所有文件都是用命令行上定义的 KERNELCONTROLER_EXPORTS
// 符号编译的。在使用此 DLL 的
// 任何其他项目上不应定义此符号。这样，源文件中包含此文件的任何其他项目都会将
// KERNELCONTROLER_API 函数视为是从 DLL 导入的，而此 DLL 则将用此宏定义的
// 符号视为是被导出的。
#ifdef KERNELCONTROLER_EXPORTS
#define KERNELCONTROLER_API __declspec(dllexport)
#else
#define KERNELCONTROLER_API __declspec(dllimport)
#endif

#include "..\\Common.h"

// 此类是从 KernelControler.dll 导出的
class KERNELCONTROLER_API CFilterDriverObject {
public:
	CFilterDriverObject();
	virtual ~CFilterDriverObject();

	BOOL Start();//开始过滤 驱动加载后会自动过滤
	BOOL Stop();//停止过滤
	BOOL AddConfidentialProcess(
		LPCWSTR wProcessName, //进程名
		LPCWSTR wProcessPath,//路径
		LPCWSTR wProcessMD5//md5
		);
	BOOL DeleteConfidentialProcess(
		LPCWSTR wProcessName, //进程名
		LPCWSTR wProcessPath,//路径
		LPCWSTR wProcessMD5//md5
		);
private:
	BOOL PackProcessData(
		LPCWSTR wProcessName, //进程名
		LPCWSTR wProcessPath,//路径
		LPCWSTR wProcessMD5,//md5
		ANTINVADER_COMMAND acCommond,//命令
		PCOMMAND_MESSAGE * dpMessage,//输出打好包的数据地址
		DWORD * ddwMessageSize//输出包大小
		);
	HANDLE hConnectionPort;
};

