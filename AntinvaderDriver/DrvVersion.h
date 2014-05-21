///////////////////////////////////////////////////////////////////////////////
///
/// 版权所有 (c) 2011 - 2012
///
/// 关于资源版本信息的定义
///
/// (File was in the PUBLIC DOMAIN  - Created by: ddkwizard\.assarbad\.net)
///////////////////////////////////////////////////////////////////////////////

// $Id$

#ifndef __DRVVERSION_H_VERSION__
#define __DRVVERSION_H_VERSION__ 100

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include "buildnumber.h"

// ---------------------------------------------------------------------------
// 一些定义必须在包含此文件时定义
// ---------------------------------------------------------------------------
#define TEXT_AUTHOR            Coxious Hall (Shuxiang Cao)
#define PRD_MAJVER             1 //工程主版本号
#define PRD_MINVER             0 //工程副版本号
#define PRD_BUILD              _FILE_VERSION_BUILD //编译次数
#define FILE_MAJVER            1 //主文件
#define FILE_MINVER            0 // minor file version
#define FILE_BUILD             _FILE_VERSION_BUILD // file build number
#define DRV_YEAR               2010-2012 // 当前年或时间跨度 (e.g. 2003-2009)
#define TEXT_WEBSITE           none // 网页
#define TEXT_PRODUCTNAME       Antinvader 驱动模块 // 工程名称
#define TEXT_FILEDESC          Antinvader 微过滤驱动及内核保护模块 biuld _FILE_VERSION_BUILD// 作用描述
#define TEXT_COMPANY           // 公司
#define TEXT_MODULE            AntinvaderDriver // 模块名称
#define TEXT_COPYRIGHT         版权所有(c)DRV_YEAR TEXT_AUTHOR // 版权信息
// #define TEXT_SPECIALBUILD      // optional comment for special builds
#define TEXT_INTERNALNAME      AntinvaderDriver.sys // 版权信息
// #define TEXT_COMMENTS          // optional comments
// ---------------------------------------------------------------------------
// ... well, that's it. Pretty self-explanatory ;)
// ---------------------------------------------------------------------------

#endif // __DRVVERSION_H_VERSION__
