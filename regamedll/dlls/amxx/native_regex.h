#pragma once
#ifndef AMXX_NATIVE_REGEX_H
#define AMXX_NATIVE_REGEX_H

#include "amx.h"

extern AMX_NATIVE_INFO regex_natives[];

void RegisterRegexNatives(AMX *amx);
void ResetRegexHandles();

#endif
