#pragma once

// Windows 网络初始化：winsock2.h 必须在 windows.h 之前包含
// 所有使用 ASIO/网络 的 .cpp 应首先包含此头文件
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
