#pragma once
#ifndef AMXX_NATIVE_JSON_H
#define AMXX_NATIVE_JSON_H

#include "amx.h"

extern AMX_NATIVE_INFO json_natives[];

void RegisterJSONNatives(AMX *amx);

// 清理全部 JSON 句柄 (地图切换时调用, 防止内存泄漏)
void ResetJsonHandles();

#endif
