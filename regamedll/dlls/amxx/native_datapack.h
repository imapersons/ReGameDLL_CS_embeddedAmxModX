#pragma once
#ifndef AMXX_NATIVE_DATAPACK_H
#define AMXX_NATIVE_DATAPACK_H

#include "amx.h"

extern AMX_NATIVE_INFO datapack_natives[];

void RegisterDataPackNatives(AMX *amx);
void ResetDataPackHandles();

#endif
