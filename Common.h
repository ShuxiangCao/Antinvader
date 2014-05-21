///////////////////////////////////////////////////////////////////////////////
///
/// 版权所有 (c) 2011 - 2012
///
/// 原始文件名称	 : Common.h
/// 工程名称         : AntinvaderDriver
/// 创建时间         : 2011-03-20
/// 	         
///
/// 描述	         : 声明Ring0和Ring3之间的通信约定
///					   定义一些常量						
///
/// 更新维护:
///  0000 [2011-03-20] 最初版本.
///
///////////////////////////////////////////////////////////////////////////////

#pragma once

/////////////////////////////////////////
//		通信约定
/////////////////////////////////////////

//管道名称	
#define COMMUNICATE_PORT_NAME	"\\AntinvaderPort"

//声明Ring3和Ring0通信的命令
typedef enum _ANTINVADER_COMMAND {
	ENUM_UNDEFINED = 0,
    ENUM_COMMAND_PASS,
    ENUM_BLOCK,
	ENUM_ADD_PROCESS,
	ENUM_DELETE_PROCESS,

	ENUM_OPERATION_SUCCESSFUL,
	ENUM_OPERATION_FAILED
} ANTINVADER_COMMAND;	

//声明Ring3和Ring0通信的命令结构
//
//数据结构为 
//
//消息头   lSize      整个消息大小
//		   acCommand  命令
//消息内容 紧跟在后面的数据 这里没有写出
//         在拆包时将进行处理
//
//其中 ENUM_COMMAND_PASS 无后缀数据
//	   ENUM_BLOCK		 无后缀数据
//
//	   ENUM_ADD_PROCESS  后缀为进程数据
//			三个字符串,以\0结尾
//	   进程名称 进程路径 文件MD5
//		
//	   ENUM_DELETE_PROCESS后缀为进程数据
//			三个字符串,以\0结尾
//	   进程名称 进程路径 文件MD5
//
//	   ENUM_OPERATION_SUCCESSFUL 无后缀数据
//	   ENUM_OPERATION_FAILED	 无后缀数据
//
typedef struct _ANTINVADER_MESSAGE {
	LONG lSize;
    ANTINVADER_COMMAND 	acCommand;
} COMMAND_MESSAGE, *PCOMMAND_MESSAGE;


////////////////////////////////////////
//		宏定义
////////////////////////////////////////

//一般的文件路径长度 用于猜测打开路径长度,如果调整的较为准确
//可以提高效率,设置的越大时间复杂度越小,空间复杂度越高
#define NORMAL_PATH_LENGTH	128

////////////////////////////////////////
//		常量定义
////////////////////////////////////////
