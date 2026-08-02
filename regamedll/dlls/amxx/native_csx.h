#pragma once
#ifndef AMXX_NATIVE_CSX_H
#define AMXX_NATIVE_CSX_H

#include "amx.h"

extern AMX_NATIVE_INFO csx_natives[];

void RegisterCsxNatives(AMX *amx);

// 跨地图清理 CSX 全局状态（由 runtime OnServerDeactivatePost 调用）
void ResetCsxGlobals();

cell AMX_NATIVE_CALL amxx_custom_weapon_add(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_custom_weapon_dmg(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_custom_weapon_shot(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_xmod_is_melee_wpn(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_xmod_get_wpnname(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_xmod_get_wpnlogname(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_xmod_get_maxweapons(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_xmod_get_stats_size(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_map_objectives(AMX *amx, cell *params);

// ===== csstats.inc natives =====
cell AMX_NATIVE_CALL amxx_get_user_wstats(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_wrstats(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_rstats(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_vstats(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_astats(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_reset_user_wstats(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_stats(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_statsnum(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_stats2(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_stats2(AMX *amx, cell *params);

// ===== csstats recording API (called from amxx_hooks.cpp game-event hooks) =====
void CsstatsRecordDamage(int attacker, int victim, int weapon, int damage, int hitgroup);
void CsstatsRecordDeath(int killer, int victim, int weapon, bool headshot, bool isTK);
void CsstatsRecordShot(int attacker, int weapon);
void CsstatsRecordBombPlant(int planter);
void CsstatsRecordBombDefuse(int defuser, bool success);
void CsstatsRecordBombExplode(int planter);
void CsstatsResetRound();

#endif
