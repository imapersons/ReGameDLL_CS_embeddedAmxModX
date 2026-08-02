#pragma once
#ifndef AMXX_NATIVE_GAMECONFIG_H
#define AMXX_NATIVE_GAMECONFIG_H

#include "amx.h"

extern AMX_NATIVE_INFO gameconfig_natives[];

void RegisterGameConfigNatives(AMX *amx);

// 清理 GameConfig 句柄状态 (地图切换时调用)
void ResetGameConfigGlobals();

#endif
