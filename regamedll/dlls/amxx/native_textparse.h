#pragma once
#ifndef AMXX_NATIVE_TEXTPARSE_H
#define AMXX_NATIVE_TEXTPARSE_H

#include "amx.h"

extern AMX_NATIVE_INFO textparse_natives[];

// ===== INI Parser (textparse_ini.inc) =====
cell AMX_NATIVE_CALL amxx_ini_create_parser(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_ini_destroy_parser(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_ini_parse_file(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_ini_set_parse_start(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_ini_set_parse_end(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_ini_set_readers(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_ini_set_raw_line(AMX *amx, cell *params);

// ===== SMC Parser (textparse_smc.inc) =====
cell AMX_NATIVE_CALL amxx_smc_create_parser(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_smc_destroy_parser(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_smc_parse_file(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_smc_set_parse_start(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_smc_set_parse_end(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_smc_set_readers(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_smc_set_raw_line(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_smc_get_error_string(AMX *amx, cell *params);

// 清理 INI/SMC 句柄状态 (地图切换时调用, 与 ResetGameConfigGlobals 一致)
void ResetTextparseGlobals();

#endif
