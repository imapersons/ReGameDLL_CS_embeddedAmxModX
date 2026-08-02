#pragma once
#ifndef AMXX_NATIVE_DBI_H
#define AMXX_NATIVE_DBI_H

#include "amx.h"

extern AMX_NATIVE_INFO dbi_natives[];

// Formats a query string from varargs (dbi_query/dbi_query2/SQL_PrepareQuery any:... parameters)
// Reused by native_sqlx.cpp
size_t FormatAmxVarargs(AMX *amx, cell *params, int fixedParams, char *out, size_t outLen);

void RegisterDbiNatives(AMX *amx);

// Cleans up DBI state (called on map change)
void ResetDbiGlobals();

// ===========================================
// DBI natives (13)
// ===========================================
cell AMX_NATIVE_CALL amxx_dbi_connect(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_dbi_query(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_dbi_query2(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_dbi_nextrow(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_dbi_field(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_dbi_result(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_dbi_num_rows(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_dbi_free_result(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_dbi_close(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_dbi_error(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_dbi_type(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_dbi_num_fields(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_dbi_field_name(AMX *amx, cell *params);

#endif
