#pragma once
#ifndef AMXX_NATIVE_SOCKETS_H
#define AMXX_NATIVE_SOCKETS_H

#include "amx.h"

extern AMX_NATIVE_INFO sockets_natives[];

// 原版 AMX Mod X sockets.inc 签名的 8 个 native
cell AMX_NATIVE_CALL amxx_socket_open(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_socket_close(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_socket_recv(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_socket_send(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_socket_send2(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_socket_change(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_socket_is_readable(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_socket_is_writable(AMX *amx, cell *params);

void RegisterSocketsNatives(AMX *amx);

// 清理 sockets 状态 (地图切换时调用, 关闭所有未关闭的 socket)
void ResetSocketsGlobals();

#endif
