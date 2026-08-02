#pragma once
#ifndef AMXX_NATIVE_SQLX_H
#define AMXX_NATIVE_SQLX_H

#include "amx.h"

extern AMX_NATIVE_INFO sqlx_natives[];

void RegisterSQLxNatives(AMX *amx);

// 清理 SQLx stub 状态 (地图切换时调用)
void ResetSqlxGlobals();

#endif
