#pragma once
#ifndef AMXX_NATIVE_LANG_H
#define AMXX_NATIVE_LANG_H

#include "amx.h"

extern AMX_NATIVE_INFO lang_natives[];

cell AMX_NATIVE_CALL amxx_get_user_langid(AMX *amx, cell *params);

void RegisterLangNatives(AMX *amx);

#endif
