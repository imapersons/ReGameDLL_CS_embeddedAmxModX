#pragma once
#ifndef AMXX_NATIVE_ENTITIES_H
#define AMXX_NATIVE_ENTITIES_H

#include "amx.h"

extern AMX_NATIVE_INFO entity_natives[];

cell AMX_NATIVE_CALL amxx_pev(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_pev(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_engfunc(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_global_get(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_global_set(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_register_think(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_register_touch(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_pev_valid(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_str_pev(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_pev_string(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_dispatch_spawn(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_dispatch_keyvalue(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_find_ent_by_owner(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_find_ent_by_target(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_entity_range(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_register_ham(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_execute_ham(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_execute_ham_b(AMX *amx, cell *params);

// P0: Ham 数据读写
cell AMX_NATIVE_CALL amxx_get_ham_data_int(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_ham_data_int(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_ham_data_float(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_ham_data_float(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_register_ham_from_entity(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_register_ham_from_player(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_register_forwardex(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_unregister_forwardex(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_create_forwardex(AMX *amx, cell *params);

// P1: pdata 私有数据读写
cell AMX_NATIVE_CALL amxx_get_pdata_int(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_pdata_int(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_pdata_float(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_pdata_float(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_pdata_string(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_pdata_string(AMX *amx, cell *params);
// P1: 引擎 Forward 注册
cell AMX_NATIVE_CALL amxx_register_forward(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_unregister_forward(AMX *amx, cell *params);

// P2: 实体查找扩展
cell AMX_NATIVE_CALL amxx_find_ent_by_class(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_find_ent_by_model(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_find_ent_in_sphere(AMX *amx, cell *params);

// P3: trace 扩展
cell AMX_NATIVE_CALL amxx_trace_texture(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_trace_model(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_trace_normal(AMX *amx, cell *params);
// P3: 固体实体
cell AMX_NATIVE_CALL amxx_get_brush_entity(AMX *amx, cell *params);
// P3: 向量运算
cell AMX_NATIVE_CALL amxx_vec_add(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_vec_sub(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_vec_length(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_vec_distance(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_vec_normalize(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_vec_angle(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_vec_dot_product(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_vec_cross(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_vec_reflect(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_vec_mul(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_vec_div(AMX *amx, cell *params);

// dllfunc — 完整实现在 native_entities.cpp，fakemeta_natives 表也引用此声明
cell AMX_NATIVE_CALL amxx_dllfunc(AMX *amx, cell *params);

// ===== P1-3: Ham 参数/返回值 natives =====
cell AMX_NATIVE_CALL amxx_SetHamParamFloat(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_SetHamParamInt(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_SetHamParamEntity(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_SetHamParamVector(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_SetHamParamString(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_GetHamReturnStatus(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_GetHamReturnInt(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_GetHamReturnFloat(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_GetHamReturnVector(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_GetHamReturnEntity(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_GetHamReturnString(AMX *amx, cell *params);

// ===== P1-4: Fakemeta KeyValue 句柄 API =====
cell AMX_NATIVE_CALL amxx_create_kvd(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_free_kvd(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_kvd(AMX *amx, cell *params);

// ===== P1-2: Engine 模块补全 natives =====
cell AMX_NATIVE_CALL amxx_get_global_float(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_global_int(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_global_string(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_global_vector(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_global_edict(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_global_edict2(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_find_ent_by_tname(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_attach_view(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_view(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_playback_event(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_usercmd(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_usercmd(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_fake_touch(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_force_use(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_lights(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_create_entity(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_remove_entity(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_is_valid_ent(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_entity_count(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_entity_set_size(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_entity_set_origin(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_entity_set_model(AMX *amx, cell *params);
// P1-5: Engine module entity_get_*/entity_set_* natives
cell AMX_NATIVE_CALL amxx_entity_get_int(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_entity_set_int(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_entity_get_float(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_entity_set_float(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_entity_get_vector(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_entity_set_vector(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_entity_get_string(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_entity_set_string(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_entity_get_edict(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_entity_set_edict(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_entity_get_byte(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_entity_set_byte(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_ent_rendering(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_drop_to_floor(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_speak(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_speak(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_halflife_time(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_radius_damage(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_trace_line(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_entity_intersects(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_is_visible(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_trace_hull(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_decal_index(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_info_keybuffer(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_call_think(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_keyvalue(AMX *amx, cell *params);

// ===== P1-6: Engine 模块补全 natives =====
cell AMX_NATIVE_CALL amxx_get_distance(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_vector_to_angle(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_traceresult(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_point_contents(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_is_in_viewcone(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_copy_keyvalue(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_find_sphere_class(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_grenade_id(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_eng_get_string(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_register_impulse(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_unregister_impulse(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_unregister_think(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_unregister_touch(AMX *amx, cell *params);

// Fakemeta forward 类型常量（与 amxmodx fakemeta_const.inc 中的 FM_* 枚举完全对齐）
// 标准枚举从 FM_PrecacheModel=1 开始，FM_ServerPrint=131 结束。
// 注意：FM_Spawn=91（不是 0）。原版 fakemeta 的 DLL 类 forward 排在 EngFunc 类 forward 之后。
#define FM_PrecacheModel              1
#define FM_PrecacheSound              2
#define FM_SetModel                   3
#define FM_ModelIndex                 4
#define FM_ModelFrames                5
#define FM_SetSize                    6
#define FM_ChangeLevel                7
#define FM_VecToYaw                   8
#define FM_VecToAngles                9
#define FM_MoveToOrigin               10
#define FM_ChangeYaw                  11
#define FM_ChangePitch                12
#define FM_FindEntityByString         13
#define FM_GetEntityIllum             14
#define FM_FindEntityInSphere         15
#define FM_FindClientInPVS            16
#define FM_EntitiesInPVS              17
#define FM_MakeVectors                18
#define FM_AngleVectors               19
#define FM_CreateEntity               20
#define FM_RemoveEntity               21
#define FM_CreateNamedEntity          22
#define FM_MakeStatic                 23
#define FM_EntIsOnFloor               24
#define FM_DropToFloor                25
#define FM_WalkMove                   26
#define FM_SetOrigin                  27
#define FM_EmitSound                  28
#define FM_EmitAmbientSound           29
#define FM_TraceLine                  30
#define FM_TraceToss                  31
#define FM_TraceMonsterHull           32
#define FM_TraceHull                  33
#define FM_TraceModel                 34
#define FM_TraceTexture               35
#define FM_TraceSphere                36
#define FM_GetAimVector               37
#define FM_ParticleEffect             38
#define FM_LightStyle                 39
#define FM_DecalIndex                 40
#define FM_PointContents              41
#define FM_MessageBegin               42
#define FM_MessageEnd                 43
#define FM_WriteByte                  44
#define FM_WriteChar                  45
#define FM_WriteShort                 46
#define FM_WriteLong                  47
#define FM_WriteAngle                 48
#define FM_WriteCoord                 49
#define FM_WriteString                50
#define FM_WriteEntity                51
#define FM_CVarGetFloat               52
#define FM_CVarGetString              53
#define FM_CVarSetFloat               54
#define FM_CVarSetString              55
#define FM_FreeEntPrivateData         56
#define FM_SzFromIndex                57
#define FM_AllocString                58
#define FM_RegUserMsg                 59
#define FM_AnimationAutomove          60
#define FM_GetBonePosition            61
#define FM_GetAttachment              62
#define FM_SetView                    63
#define FM_Time                       64
#define FM_CrosshairAngle             65
#define FM_FadeClientVolume           66
#define FM_SetClientMaxspeed          67
#define FM_CreateFakeClient           68
#define FM_RunPlayerMove              69
#define FM_NumberOfEntities           70
#define FM_StaticDecal                71
#define FM_PrecacheGeneric            72
#define FM_BuildSoundMsg              73
#define FM_GetPhysicsKeyValue         74
#define FM_SetPhysicsKeyValue         75
#define FM_GetPhysicsInfoString       76
#define FM_PrecacheEvent              77
#define FM_PlaybackEvent              78
#define FM_CheckVisibility            79
#define FM_GetCurrentPlayer           80
#define FM_CanSkipPlayer              81
#define FM_SetGroupMask               82
#define FM_Voice_GetClientListening   83
#define FM_Voice_SetClientListening   84
#define FM_InfoKeyValue               85
#define FM_SetKeyValue                86
#define FM_SetClientKeyValue          87
#define FM_GetPlayerAuthId            88
#define FM_GetPlayerWONId             89
#define FM_IsMapValid                 90
#define FM_Spawn                      91
#define FM_Think                      92
#define FM_Use                        93
#define FM_Touch                      94
#define FM_Blocked                    95
#define FM_KeyValue                   96
#define FM_SetAbsBox                  97
#define FM_ClientConnect              98
#define FM_ClientDisconnect           99
#define FM_ClientKill                 100
#define FM_ClientPutInServer          101
#define FM_ClientCommand              102
#define FM_ServerDeactivate           103
#define FM_PlayerPreThink             104
#define FM_PlayerPostThink            105
#define FM_StartFrame                 106
#define FM_ParmsNewLevel              107
#define FM_ParmsChangeLevel           108
#define FM_GetGameDescription         109
#define FM_SpectatorConnect           110
#define FM_SpectatorDisconnect        111
#define FM_SpectatorThink             112
#define FM_Sys_Error                  113
#define FM_PM_FindTextureType         114
#define FM_RegisterEncoders           115
#define FM_CreateInstBaselines        116
#define FM_AllowLagCompensation       117
#define FM_AlertMessage               118
#define FM_OnFreeEntPrivateData       119
#define FM_GameShutdown               120
#define FM_ShouldCollide              121
#define FM_ClientUserInfoChanged      122
#define FM_UpdateClientData           123
#define FM_AddToFullPack               124
#define FM_CmdStart                   125
#define FM_CmdEnd                     126
#define FM_CreateInstBaseline         127
#define FM_CreateBaseline             128
#define FM_GetInfoKeyBuffer           129
#define FM_ClientPrintf               130
#define FM_ServerPrint                131
// ===== EMBEDDED 扩展 forward（非标准 fakemeta，排在标准范围之后避免冲突）=====
// 供 amxx_hooks.cpp（FM_PlayerSpawn/FM_PlayerKilled）与 register_forwardex
// （FM_TakeDamage/FM_TraceAttack）使用。
#define FM_PlayerSpawn                132
#define FM_PlayerKilled               133
#define FM_TakeDamage                 134
#define FM_TraceAttack                135
#define FM_MAX                        136

// 触发 Fakemeta forward（供 amxx_hooks.cpp 调用）
void FireFMForward(int fmType, int playerIdx);

// P1-9: 触发常见引擎事件 Fakemeta forward（供 thinktouch.cpp 调用）
// 返回非 0 表示有插件返回 PLUGIN_HANDLED+ ，调用方应跳过引擎原始函数。
int FireFMForwardSpawn(int entIdx);
int FireFMForwardThink(int entIdx);
int FireFMForwardTouch(int touchedIdx, int otherIdx);
int FireFMForwardSetModel(int entIdx, const char *model);

// CD/ES/UC Forward 触发（供 client.cpp 的 UpdateClientData/AddToFullPack/CmdStart 调用）
// 使用 struct tag 前向声明（实际类型为 clientdata_t/entity_state_t/usercmd_t typedef）
struct clientdata_s;
struct entity_state_s;
struct usercmd_s;
int FireFMForwardUpdateClientData(int entIdx, int sendweapons, struct clientdata_s *cd);
int FireFMForwardAddToFullPack(struct entity_state_s *state, int e, int entIdx, int hostIdx, int hostflags, int player, unsigned char *pSet);
int FireFMForwardCmdStart(int playerIdx, struct usercmd_s *cmd, unsigned int random_seed);

void RegisterEntityNatives(AMX *amx);

// 跨地图清理 native_entities 全局状态（KVD 池等）
void ResetEntityGlobals();

// ===== Ham Sandwich 补充 natives =====
// SetHamParamEntity2 - 同 SetHamParamEntity，但修改传递到 post forward
cell AMX_NATIVE_CALL amxx_SetHamParamEntity2(AMX *amx, cell *params);
// SetHamParamTraceResult - 设置 TraceResult 参数
cell AMX_NATIVE_CALL amxx_SetHamParamTraceResult(AMX *amx, cell *params);
// SetHamParamItemInfo - 设置 ItemInfo 参数
cell AMX_NATIVE_CALL amxx_SetHamParamItemInfo(AMX *amx, cell *params);
// Get/Set Ham ItemInfo
cell AMX_NATIVE_CALL amxx_GetHamItemInfo(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_SetHamItemInfo(AMX *amx, cell *params);
// Create/Free Ham ItemInfo
cell AMX_NATIVE_CALL amxx_CreateHamItemInfo(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_FreeHamItemInfo(AMX *amx, cell *params);
// Get Original Ham Return 系列
cell AMX_NATIVE_CALL amxx_GetOrigHamReturnInteger(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_GetOrigHamReturnFloat(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_GetOrigHamReturnVector(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_GetOrigHamReturnEntity(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_GetOrigHamReturnString(AMX *amx, cell *params);
// Set Ham Return 系列
cell AMX_NATIVE_CALL amxx_SetHamReturnInteger(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_SetHamReturnFloat(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_SetHamReturnVector(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_SetHamReturnEntity(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_SetHamReturnString(AMX *amx, cell *params);
// GetHamReturnInteger (别名)
cell AMX_NATIVE_CALL amxx_GetHamReturnInteger(AMX *amx, cell *params);
// set_pdata_cbase
cell AMX_NATIVE_CALL amxx_set_pdata_cbase(AMX *amx, cell *params);
// get_pdata_cbase_safe
cell AMX_NATIVE_CALL amxx_get_pdata_cbase_safe(AMX *amx, cell *params);

#endif