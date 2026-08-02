#pragma once
#ifndef AMXX_NATIVE_FUN_H
#define AMXX_NATIVE_FUN_H

#include "amx.h"

extern AMX_NATIVE_INFO fun_natives[];

void RegisterFunNatives(AMX *amx);

cell AMX_NATIVE_CALL fun_set_user_gravity(AMX *amx, cell *params);
cell AMX_NATIVE_CALL fun_get_user_gravity(AMX *amx, cell *params);
cell AMX_NATIVE_CALL fun_set_user_maxspeed(AMX *amx, cell *params);
cell AMX_NATIVE_CALL fun_get_user_maxspeed(AMX *amx, cell *params);
cell AMX_NATIVE_CALL fun_user_has_weapon(AMX *amx, cell *params);
cell AMX_NATIVE_CALL fun_get_weapon_name(AMX *amx, cell *params);
cell AMX_NATIVE_CALL fun_set_rendering(AMX *amx, cell *params);
cell AMX_NATIVE_CALL fun_set_user_rendering(AMX *amx, cell *params);
cell AMX_NATIVE_CALL fun_set_user_godmode(AMX *amx, cell *params);
cell AMX_NATIVE_CALL fun_get_user_godmode(AMX *amx, cell *params);
cell AMX_NATIVE_CALL fun_set_user_noclip(AMX *amx, cell *params);
cell AMX_NATIVE_CALL fun_get_user_noclip(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_user_footsteps(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_user_blood(AMX *amx, cell *params);

// P2: 静默模式每帧维持 flTimeStepSound=999 (由 AMXXRuntime::Frame() 调用)
void UpdateSilentFootsteps();
cell AMX_NATIVE_CALL amxx_user_kill(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_user_slap(AMX *amx, cell *params);

// P1-1 补全
cell AMX_NATIVE_CALL fun_get_client_listen(AMX *amx, cell *params);
cell AMX_NATIVE_CALL fun_set_client_listen(AMX *amx, cell *params);
cell AMX_NATIVE_CALL fun_set_user_armor(AMX *amx, cell *params);
cell AMX_NATIVE_CALL fun_set_user_health(AMX *amx, cell *params);
cell AMX_NATIVE_CALL fun_set_user_origin(AMX *amx, cell *params);
cell AMX_NATIVE_CALL fun_get_user_rendering(AMX *amx, cell *params);
cell AMX_NATIVE_CALL fun_set_user_hitzones(AMX *amx, cell *params);
cell AMX_NATIVE_CALL fun_get_user_hitzones(AMX *amx, cell *params);
cell AMX_NATIVE_CALL fun_spawn_entity(AMX *amx, cell *params);
cell AMX_NATIVE_CALL fun_get_user_footsteps(AMX *amx, cell *params);
cell AMX_NATIVE_CALL fun_set_user_frags(AMX *amx, cell *params);
cell AMX_NATIVE_CALL fun_strip_user_weapons(AMX *amx, cell *params);
cell AMX_NATIVE_CALL fun_give_item(AMX *amx, cell *params);

#endif
