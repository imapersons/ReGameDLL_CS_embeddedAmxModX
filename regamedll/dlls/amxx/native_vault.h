#pragma once
#ifndef AMXX_NATIVE_VAULT_H
#define AMXX_NATIVE_VAULT_H

#include "amx.h"

extern AMX_NATIVE_INFO vault_natives[];

// P1: Vault 增强
cell AMX_NATIVE_CALL amxx_vault_get_array(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_vault_set_array(AMX *amx, cell *params);

// P2-4: nVault persistence
cell AMX_NATIVE_CALL amxx_nvault_open(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_nvault_close(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_nvault_get(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_nvault_set(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_nvault_remove(AMX *amx, cell *params);

// P2-3: nvault 高级 API
cell AMX_NATIVE_CALL amxx_nvault_pset(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_nvault_touch(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_nvault_prune(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_nvault_lookup(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_remove_vaultdata(AMX *amx, cell *params);

void RegisterVaultNatives(AMX *amx);

#endif
