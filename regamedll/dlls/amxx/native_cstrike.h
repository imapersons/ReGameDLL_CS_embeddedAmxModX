#pragma once
#ifndef AMXX_NATIVE_CSTRIKE_H
#define AMXX_NATIVE_CSTRIKE_H

#include "amx.h"

extern AMX_NATIVE_INFO cstrike_natives[];

void RegisterCstrikeNatives(AMX *amx);

// Clears CStrike global state between maps (called by runtime OnServerDeactivatePost)
void ResetCstrikeGlobals();

cell AMX_NATIVE_CALL cs_get_user_team(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_user_team(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_money(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_user_money(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_deaths(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_user_deaths(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_armor(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_user_armor(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_defuse(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_user_defuse(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_plant(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_user_plant(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_primweapon(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_secweapon(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_weapon(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_hasprim(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_kevlar(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_user_kevlar(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_helmet(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_user_helmet(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_nvg(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_user_nvg(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_defuser(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_user_defuser(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_user_model(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_reset_user_model(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_user_bpammo(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_bpammo(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_zoom(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_user_zoom(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_submodel(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_user_submodel(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_lastactivity(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_user_lastactivity(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_tked(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_user_tked(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_vip(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_hostagekills(AMX *amx, cell *params);

// CStrike enhancements
cell AMX_NATIVE_CALL cs_get_user_armortype(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_user_armortype(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_no_knives(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_no_knives(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_weapon_info(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_cs_get_weapon_ammo(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_weapon_ammo(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_weapon_clip(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_weapon_clip(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_weap_ammo(AMX *amx, cell *params);

// CStrike weapon enhancements
cell AMX_NATIVE_CALL cs_get_weapon_burst(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_weapon_burst(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_weapon_silen(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_weapon_silen(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_armoury_type(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_armoury_type(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_mapzones(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_buyzone(AMX *amx, cell *params);

// CStrike extensions
cell AMX_NATIVE_CALL cs_get_user_team_id(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_stationary(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_hostage_follow(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_hostage_follow(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_hostage_id(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_shield(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_user_shield(AMX *amx, cell *params);

// CStrike extensions
cell AMX_NATIVE_CALL cs_get_user_plant_zone(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_plant_target(AMX *amx, cell *params);

// ===== Hostage lastuse/nextuse + C4 explode/defusing =====
cell AMX_NATIVE_CALL cs_get_hostage_lastuse(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_hostage_lastuse(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_hostage_nextuse(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_hostage_nextuse(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_c4_explode_time(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_c4_explode_time(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_c4_defusing(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_c4_defusing(AMX *amx, cell *params);

cell AMX_NATIVE_CALL amxx_cs_get_user_model(AMX *amx, cell *params);

// Missing AMXX cstrike natives
cell AMX_NATIVE_CALL cs_get_user_driving(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_user_vip(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_user_hostagekills(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_user_spawn(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_weaponbox_item(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_create_entity(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_find_ent_by_class(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_find_ent_by_owner(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_set_ent_class(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_item_id(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_item_alias(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_translated_item_alias(AMX *amx, cell *params);
cell AMX_NATIVE_CALL cs_get_user_weapon_entity(AMX *amx, cell *params);

#endif
