#include "precompiled.h"
#include "native_entities.h"
#include "amx.h"
#include "thinktouch.h"
#include "hamsandwich.h"
#include "runtime.h"
#include "util.h"
#include "../extdll.h"
#include "../enginecallback.h"
#include "../cbase.h"
#include <cstdio>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <vector>

// Forward type enum (maps to engine pfn function pointers)
// FM_* forward type constants are defined in native_entities.h

struct ForwardHook {
    int type;       // ForwardType (use int to avoid enum compatibility issues)
    AMX *amx;
    cell funcidx;
};

static std::vector<ForwardHook> g_forwardHooks;

// Global TraceResult cache (used by trace_* natives)
static TraceResult g_lastTrace;

// ===== Fakemeta CD/ES/UC global cache =====
// Static caches used when a plugin passes handle=0; a non-zero handle is treated as a pointer to the struct.
static clientdata_t g_cd_glb;
static entity_state_t g_es_glb;
static usercmd_t g_uc_glb;

// Context pointers for the currently executing CD/ES/UC forward (set by the
// UpdateClientData/AddToFullPack/CmdStart hooks and passed to plugin callbacks by FireFMForwardCD/ES/UC)
clientdata_t *g_amxx_current_cd = nullptr;
entity_state_t *g_amxx_current_es = nullptr;
usercmd_t *g_amxx_current_uc = nullptr;

// current KeyValueData for copy_keyvalue (set by the pfn_keyvalue hook during dispatch)
static KeyValueData g_currentKvd;
static bool g_inKeyValue = false;

// register_impulse / unregister_impulse / unregister_think / unregister_touch
// Registries used here. The id returned by register_think/register_touch is allocated here;
// unregister_* use it to remove the entry and detach the dispatch hook from AMXXThinkTouch.
struct ImpulseReg { int id; int impulse; AMX *amx; cell funcidx; };
struct ThinkReg   { int id; char classname[64]; AMX *amx; cell funcidx; };
struct TouchReg   { int id; char toucher[64]; char touched[64]; AMX *amx; cell funcidx; };
static std::vector<ImpulseReg> g_impulseRegs;
static std::vector<ThinkReg>   g_thinkRegs;
static std::vector<TouchReg>   g_touchRegs;
static int s_nextHookId = 1;

// ===== pev_* field enum and offset table =====
// The original fakemeta indexes byte offsets via g_offset_table[pev_*] and determines
// the field type from the range the pev_* value falls in (int/float/vec/byte/bytearray/
// string/edict). This logic is replicated so standard AMXX plugins calling pev/set_pev
// with pev_* enum values work directly.
enum {
    pev_string_start = 0,
    pev_classname,
    pev_globalname,
    pev_model,
    pev_target,
    pev_targetname,
    pev_netname,
    pev_message,
    pev_noise,
    pev_noise1,
    pev_noise2,
    pev_noise3,
    pev_string_end,
    pev_edict_start,
    pev_chain,
    pev_dmg_inflictor,
    pev_enemy,
    pev_aiment,
    pev_owner,
    pev_groundentity,
    pev_euser1,
    pev_euser2,
    pev_euser3,
    pev_euser4,
    pev_edict_end,
    pev_float_start,
    pev_impacttime,
    pev_starttime,
    pev_idealpitch,
    pev_ideal_yaw,
    pev_pitch_speed,
    pev_yaw_speed,
    pev_ltime,
    pev_nextthink,
    pev_gravity,
    pev_friction,
    pev_frame,
    pev_animtime,
    pev_framerate,
    pev_scale,
    pev_renderamt,
    pev_health,
    pev_frags,
    pev_takedamage,
    pev_max_health,
    pev_teleport_time,
    pev_armortype,
    pev_armorvalue,
    pev_dmg_take,
    pev_dmg_save,
    pev_dmg,
    pev_dmgtime,
    pev_speed,
    pev_air_finished,
    pev_pain_finished,
    pev_radsuit_finished,
    pev_maxspeed,
    pev_fov,
    pev_flFallVelocity,
    pev_fuser1,
    pev_fuser2,
    pev_fuser3,
    pev_fuser4,
    pev_float_end,
    pev_int_start,
    pev_fixangle,
    pev_modelindex,
    pev_viewmodel,
    pev_weaponmodel,
    pev_movetype,
    pev_solid,
    pev_skin,
    pev_body,
    pev_effects,
    pev_light_level,
    pev_sequence,
    pev_gaitsequence,
    pev_rendermode,
    pev_renderfx,
    pev_weapons,
    pev_deadflag,
    pev_button,
    pev_impulse,
    pev_spawnflags,
    pev_flags,
    pev_colormap,
    pev_team,
    pev_waterlevel,
    pev_watertype,
    pev_playerclass,
    pev_weaponanim,
    pev_pushmsec,
    pev_bInDuck,
    pev_flTimeStepSound,
    pev_flSwimTime,
    pev_flDuckTime,
    pev_iStepLeft,
    pev_gamestate,
    pev_oldbuttons,
    pev_groupinfo,
    pev_iuser1,
    pev_iuser2,
    pev_iuser3,
    pev_iuser4,
    pev_int_end,
    pev_byte_start,
    pev_controller_0,
    pev_controller_1,
    pev_controller_2,
    pev_controller_3,
    pev_blending_0,
    pev_blending_1,
    pev_byte_end,
    pev_bytearray_start,
    pev_controller,
    pev_blending,
    pev_bytearray_end,
    pev_vecarray_start,
    pev_origin,
    pev_oldorigin,
    pev_velocity,
    pev_basevelocity,
    pev_clbasevelocity,
    pev_movedir,
    pev_angles,
    pev_avelocity,
    pev_v_angle,
    pev_endpos,
    pev_startpos,
    pev_absmin,
    pev_absmax,
    pev_mins,
    pev_maxs,
    pev_size,
    pev_rendercolor,
    pev_view_ofs,
    pev_vuser1,
    pev_vuser2,
    pev_vuser3,
    pev_vuser4,
    pev_punchangle,
    pev_vecarray_end,
    pev_string2_begin,
    pev_weaponmodel2,
    pev_viewmodel2,
    pev_string2_end,
    pev_edict2_start,
    pev_pContainingEntity,
    pev_absolute_end
};

static int g_pev_offset_table[pev_absolute_end];
static bool g_pev_offset_init = false;

#define PEV_DO_OFFSET(field) g_pev_offset_table[pev_##field] = (int)offsetof(entvars_t, field)
#define PEV_DO_OFFSET_R(namedEnum, real, sub) g_pev_offset_table[namedEnum] = (int)offsetof(entvars_t, real) + (sub)

static void InitPevOffsets()
{
    if (g_pev_offset_init)
        return;
    for (int i = 0; i < pev_absolute_end; i++)
        g_pev_offset_table[i] = -1;

    PEV_DO_OFFSET(fixangle);
    PEV_DO_OFFSET(modelindex);
    PEV_DO_OFFSET(viewmodel);
    PEV_DO_OFFSET(weaponmodel);
    PEV_DO_OFFSET(movetype);
    PEV_DO_OFFSET(solid);
    PEV_DO_OFFSET(skin);
    PEV_DO_OFFSET(body);
    PEV_DO_OFFSET(effects);
    PEV_DO_OFFSET(light_level);
    PEV_DO_OFFSET(sequence);
    PEV_DO_OFFSET(gaitsequence);
    PEV_DO_OFFSET(rendermode);
    PEV_DO_OFFSET(renderfx);
    PEV_DO_OFFSET(weapons);
    PEV_DO_OFFSET(deadflag);
    PEV_DO_OFFSET(button);
    PEV_DO_OFFSET(impulse);
    PEV_DO_OFFSET(spawnflags);
    PEV_DO_OFFSET(flags);
    PEV_DO_OFFSET(colormap);
    PEV_DO_OFFSET(team);
    PEV_DO_OFFSET(waterlevel);
    PEV_DO_OFFSET(watertype);
    PEV_DO_OFFSET(playerclass);
    PEV_DO_OFFSET(weaponanim);
    PEV_DO_OFFSET(pushmsec);
    PEV_DO_OFFSET(bInDuck);
    PEV_DO_OFFSET(flTimeStepSound);
    PEV_DO_OFFSET(flSwimTime);
    PEV_DO_OFFSET(flDuckTime);
    PEV_DO_OFFSET(iStepLeft);
    PEV_DO_OFFSET(gamestate);
    PEV_DO_OFFSET(oldbuttons);
    PEV_DO_OFFSET(groupinfo);
    PEV_DO_OFFSET(iuser1);
    PEV_DO_OFFSET(iuser2);
    PEV_DO_OFFSET(iuser3);
    PEV_DO_OFFSET(iuser4);
    PEV_DO_OFFSET(impacttime);
    PEV_DO_OFFSET(starttime);
    PEV_DO_OFFSET(idealpitch);
    PEV_DO_OFFSET(ideal_yaw);
    PEV_DO_OFFSET(pitch_speed);
    PEV_DO_OFFSET(yaw_speed);
    PEV_DO_OFFSET(ltime);
    PEV_DO_OFFSET(nextthink);
    PEV_DO_OFFSET(gravity);
    PEV_DO_OFFSET(friction);
    PEV_DO_OFFSET(frame);
    PEV_DO_OFFSET(animtime);
    PEV_DO_OFFSET(framerate);
    PEV_DO_OFFSET(scale);
    PEV_DO_OFFSET(renderamt);
    PEV_DO_OFFSET(health);
    PEV_DO_OFFSET(frags);
    PEV_DO_OFFSET(takedamage);
    PEV_DO_OFFSET(max_health);
    PEV_DO_OFFSET(teleport_time);
    PEV_DO_OFFSET(armortype);
    PEV_DO_OFFSET(armorvalue);
    PEV_DO_OFFSET(dmg_take);
    PEV_DO_OFFSET(dmg_save);
    PEV_DO_OFFSET(dmg);
    PEV_DO_OFFSET(dmgtime);
    PEV_DO_OFFSET(speed);
    PEV_DO_OFFSET(air_finished);
    PEV_DO_OFFSET(pain_finished);
    PEV_DO_OFFSET(radsuit_finished);
    PEV_DO_OFFSET(maxspeed);
    PEV_DO_OFFSET(fov);
    PEV_DO_OFFSET(flFallVelocity);
    PEV_DO_OFFSET(fuser1);
    PEV_DO_OFFSET(fuser2);
    PEV_DO_OFFSET(fuser3);
    PEV_DO_OFFSET(fuser4);
    PEV_DO_OFFSET(classname);
    PEV_DO_OFFSET(globalname);
    PEV_DO_OFFSET(model);
    PEV_DO_OFFSET(target);
    PEV_DO_OFFSET(targetname);
    PEV_DO_OFFSET(netname);
    PEV_DO_OFFSET(message);
    PEV_DO_OFFSET(noise);
    PEV_DO_OFFSET(noise1);
    PEV_DO_OFFSET(noise2);
    PEV_DO_OFFSET(noise3);
    PEV_DO_OFFSET(chain);
    PEV_DO_OFFSET(dmg_inflictor);
    PEV_DO_OFFSET(enemy);
    PEV_DO_OFFSET(aiment);
    PEV_DO_OFFSET(owner);
    PEV_DO_OFFSET(groundentity);
    PEV_DO_OFFSET(euser1);
    PEV_DO_OFFSET(euser2);
    PEV_DO_OFFSET(euser3);
    PEV_DO_OFFSET(euser4);
    PEV_DO_OFFSET(origin);
    PEV_DO_OFFSET(oldorigin);
    PEV_DO_OFFSET(velocity);
    PEV_DO_OFFSET(basevelocity);
    PEV_DO_OFFSET(clbasevelocity);
    PEV_DO_OFFSET(movedir);
    PEV_DO_OFFSET(angles);
    PEV_DO_OFFSET(avelocity);
    PEV_DO_OFFSET(v_angle);
    PEV_DO_OFFSET(endpos);
    PEV_DO_OFFSET(startpos);
    PEV_DO_OFFSET(absmin);
    PEV_DO_OFFSET(absmax);
    PEV_DO_OFFSET(mins);
    PEV_DO_OFFSET(maxs);
    PEV_DO_OFFSET(size);
    PEV_DO_OFFSET(rendercolor);
    PEV_DO_OFFSET(view_ofs);
    PEV_DO_OFFSET(vuser1);
    PEV_DO_OFFSET(vuser2);
    PEV_DO_OFFSET(vuser3);
    PEV_DO_OFFSET(vuser4);
    PEV_DO_OFFSET(punchangle);
    PEV_DO_OFFSET(controller);
    PEV_DO_OFFSET_R(pev_controller_0, controller, 0);
    PEV_DO_OFFSET_R(pev_controller_1, controller, 1);
    PEV_DO_OFFSET_R(pev_controller_2, controller, 2);
    PEV_DO_OFFSET_R(pev_controller_3, controller, 3);
    PEV_DO_OFFSET(blending);
    PEV_DO_OFFSET_R(pev_blending_0, blending, 0);
    PEV_DO_OFFSET_R(pev_blending_1, blending, 1);
    PEV_DO_OFFSET_R(pev_weaponmodel2, weaponmodel, 0);
    PEV_DO_OFFSET_R(pev_viewmodel2, viewmodel, 0);
    PEV_DO_OFFSET(pContainingEntity);

    g_pev_offset_init = true;
}

#define PEV_EDICT_OFFS(v, o) ((char *)(v) + (o))

cell AMX_NATIVE_CALL amxx_pev(AMX *amx, cell *params)
{
    InitPevOffsets();

    int index = params[1];
    edict_t *pEntity = INDEXENT(index);
    if (!pEntity)
        return 0;

    int iSwitch = params[2];
    if (iSwitch <= pev_string_start || iSwitch >= pev_absolute_end)
        return 0;

    int offs = g_pev_offset_table[iSwitch];
    if (offs == -1)
        return 0;

    enum {
        Ret_Int      = (1 << 0),
        Ret_Float    = (1 << 1),
        Ret_Vec      = (1 << 2),
        Ret_ByteArray= (1 << 3),
        Ret_String   = (1 << 4),
        Ret_Edict    = (1 << 5),
        Ret_Bytes2   = (1 << 6),
        Ret_Bytes4   = (1 << 7),
        Ret_Byte     = (1 << 8)
    };

    union {
        int       i;
        float     f;
        unsigned char b;
        unsigned int s;  // string_t is enum class, use unsigned int for union compatibility
        unsigned char ba[4];
    } rets;
    memset(&rets, 0, sizeof(rets));
    Vector vr(0, 0, 0);

    int ValType = 0;
    entvars_t *v = &pEntity->v;

    if (iSwitch > pev_int_start && iSwitch < pev_int_end) {
        rets.i = *(int *)PEV_EDICT_OFFS(v, offs);
        ValType = Ret_Int;
    } else if (iSwitch > pev_float_start && iSwitch < pev_float_end) {
        rets.f = *(float *)PEV_EDICT_OFFS(v, offs);
        ValType = Ret_Float;
    } else if (iSwitch > pev_vecarray_start && iSwitch < pev_vecarray_end) {
        vr = *(vec3_t *)PEV_EDICT_OFFS(v, offs);
        ValType = Ret_Vec;
    } else if (iSwitch > pev_bytearray_start && iSwitch < pev_bytearray_end) {
        if (iSwitch == pev_controller) {
            rets.ba[0] = v->controller[0];
            rets.ba[1] = v->controller[1];
            rets.ba[2] = v->controller[2];
            rets.ba[3] = v->controller[3];
            ValType = Ret_Bytes4;
        } else {
            rets.ba[0] = v->blending[0];
            rets.ba[1] = v->blending[1];
            ValType = Ret_Bytes2;
        }
    } else if (iSwitch > pev_byte_start && iSwitch < pev_byte_end) {
        rets.b = *(unsigned char *)PEV_EDICT_OFFS(v, offs);
        ValType = Ret_Byte;
    } else if ((iSwitch > pev_string_start && iSwitch < pev_string_end) ||
               (iSwitch > pev_string2_begin && iSwitch < pev_string2_end)) {
        rets.s = (unsigned int)*(string_t *)PEV_EDICT_OFFS(v, offs);
        ValType = Ret_String;
    } else if ((iSwitch > pev_edict_start && iSwitch < pev_edict_end) ||
               (iSwitch > pev_edict2_start && iSwitch < pev_absolute_end)) {
        edict_t *e = *(edict_t **)PEV_EDICT_OFFS(v, offs);
        rets.i = e ? ENTINDEX(e) : -1;
        ValType = Ret_Int | Ret_Edict;
    } else {
        return 0;
    }

    size_t count = (size_t)(params[0] / sizeof(cell)) - 2;

    if (count == 0) {
        if (ValType & Ret_Int)
            return rets.i;
        else if (ValType == Ret_Byte)
            return rets.b;
        else if (ValType == Ret_Float)
            return amx_ftoc(rets.f);
        else if (ValType == Ret_String)
            return (cell)rets.s;
        return 0;
    } else if (count == 1) {
        cell *addr;
        amx_GetAddr(amx, params[3], &addr);
        if (!addr)
            return 0;
        if (ValType == Ret_Float) {
            *addr = amx_ftoc(rets.f);
        } else if (ValType == Ret_Int) {
            // when writing an int by-ref, wrap it with amx_ftoc (float bits) instead of writing the raw int
            *addr = amx_ftoc((float)rets.i);
        } else if (ValType == Ret_Byte) {
            // byte fields are written by-ref directly as integer values
            *addr = rets.b;
        } else if (ValType == Ret_Vec) {
            addr[0] = amx_ftoc(vr.x);
            addr[1] = amx_ftoc(vr.y);
            addr[2] = amx_ftoc(vr.z);
        } else if (ValType == Ret_Bytes2) {
            addr[0] = rets.ba[0];
            addr[1] = rets.ba[1];
        } else if (ValType == Ret_Bytes4) {
            addr[0] = rets.ba[0];
            addr[1] = rets.ba[1];
            addr[2] = rets.ba[2];
            addr[3] = rets.ba[3];
        } else {
            return 0;
        }
        return 1;
    } else if (count == 2) {
        cell *sizeAddr;
        amx_GetAddr(amx, params[4], &sizeAddr);
        int size = sizeAddr ? (int)*sizeAddr : 0;
        cell *dest;
        amx_GetAddr(amx, params[3], &dest);
        if (!dest)
            return 0;
        if (ValType == Ret_String) {
            const char *str = STRING((string_t)rets.s);
            if (!str) str = "";
            return amx_SetString(dest, str, 0, 0, size);
        } else if ((ValType & Ret_Int) || ValType == Ret_Byte) {
            char temp[32];
            snprintf(temp, sizeof(temp), "%d", (ValType == Ret_Byte) ? (int)rets.b : rets.i);
            return amx_SetString(dest, temp, 0, 0, size);
        } else if (ValType == Ret_Float) {
            char temp[32];
            snprintf(temp, sizeof(temp), "%f", rets.f);
            return amx_SetString(dest, temp, 0, 0, size);
        } else if (ValType == Ret_Vec) {
            char temp[64];
            snprintf(temp, sizeof(temp), "%f %f %f", vr.x, vr.y, vr.z);
            return amx_SetString(dest, temp, 0, 0, size);
        } else if (ValType == Ret_Bytes2) {
            char temp[32];
            snprintf(temp, sizeof(temp), "%d %d", rets.ba[0], rets.ba[1]);
            return amx_SetString(dest, temp, 0, 0, size);
        } else if (ValType == Ret_Bytes4) {
            char temp[32];
            snprintf(temp, sizeof(temp), "%d %d %d %d", rets.ba[0], rets.ba[1], rets.ba[2], rets.ba[3]);
            return amx_SetString(dest, temp, 0, 0, size);
        }
        return 0;
    } else if (count == 3) {
        cell *sizeAddr;
        amx_GetAddr(amx, params[5], &sizeAddr);
        int size = sizeAddr ? (int)*sizeAddr : 0;
        cell *handleAddr;
        amx_GetAddr(amx, params[3], &handleAddr);
        cell *dest;
        amx_GetAddr(amx, params[4], &dest);
        if (ValType == Ret_String) {
            if (handleAddr) *handleAddr = (cell)rets.s;
            const char *str = STRING((string_t)rets.s);
            if (!str) str = "";
            return dest ? amx_SetString(dest, str, 0, 0, size) : 0;
        }
        return 0;
    }

    return 0;
}

cell AMX_NATIVE_CALL amxx_set_pev(AMX *amx, cell *params)
{
    InitPevOffsets();

    int index = params[1];
    edict_t *pEntity = INDEXENT(index);
    if (!pEntity)
        return 0;

    int iSwitch = params[2];
    if (iSwitch <= pev_string_start || iSwitch >= pev_absolute_end)
        return 0;

    int offs = g_pev_offset_table[iSwitch];
    if (offs == -1)
        return 0;

    cell *blah;
    amx_GetAddr(amx, params[3], &blah);
    if (!blah)
        return 0;

    entvars_t *v = &pEntity->v;

    if (iSwitch > pev_int_start && iSwitch < pev_int_end) {
        *(int *)PEV_EDICT_OFFS(v, offs) = (int)*blah;
    } else if (iSwitch > pev_float_start && iSwitch < pev_float_end) {
        *(float *)PEV_EDICT_OFFS(v, offs) = amx_ctof(blah[0]);
    } else if ((iSwitch > pev_string_start && iSwitch < pev_string_end) ||
               (iSwitch > pev_string2_begin && iSwitch < pev_string2_end)) {
        char strbuf[1024];
        amx_GetString(strbuf, blah, 0, sizeof(strbuf));
        string_t value = ALLOC_STRING(strbuf);
        *(string_t *)PEV_EDICT_OFFS(v, offs) = value;
    } else if ((iSwitch > pev_edict_start && iSwitch < pev_edict_end) ||
               (iSwitch > pev_edict2_start && iSwitch < pev_absolute_end)) {
        int target = (int)*blah;
        edict_t *e = (target > 0) ? INDEXENT(target) : nullptr;
        *(edict_t **)PEV_EDICT_OFFS(v, offs) = e;
    } else if (iSwitch > pev_vecarray_start && iSwitch < pev_vecarray_end) {
        vec3_t vec;
        vec[0] = amx_ctof(blah[0]);
        vec[1] = amx_ctof(blah[1]);
        vec[2] = amx_ctof(blah[2]);
        *(vec3_t *)PEV_EDICT_OFFS(v, offs) = vec;
    } else if (iSwitch > pev_byte_start && iSwitch < pev_byte_end) {
        *(unsigned char *)PEV_EDICT_OFFS(v, offs) = (unsigned char)*blah;
    } else if (iSwitch > pev_bytearray_start && iSwitch < pev_bytearray_end) {
        if (iSwitch == pev_controller) {
            v->controller[0] = blah[0];
            v->controller[1] = blah[1];
            v->controller[2] = blah[2];
            v->controller[3] = blah[3];
        } else {
            v->blending[0] = blah[0];
            v->blending[1] = blah[1];
        }
    } else {
        return 0;
    }

    return 1;
}

// ===== engfunc zero-based numbering (EngFunc_* enum) =====
enum EngFuncIndex
{
    EngFunc_PrecacheModel = 0,
    EngFunc_PrecacheSound,
    EngFunc_SetModel,
    EngFunc_ModelIndex,
    EngFunc_ModelFrames,
    EngFunc_SetSize,
    EngFunc_ChangeLevel,
    EngFunc_VecToYaw,
    EngFunc_VecToAngles,
    EngFunc_MoveToOrigin,
    EngFunc_ChangeYaw,
    EngFunc_ChangePitch,
    EngFunc_FindEntityByString,
    EngFunc_GetEntityIllum,
    EngFunc_FindEntityInSphere,
    EngFunc_FindClientInPVS,
    EngFunc_EntitiesInPVS,
    EngFunc_MakeVectors,
    EngFunc_AngleVectors,
    EngFunc_CreateEntity,
    EngFunc_RemoveEntity,
    EngFunc_CreateNamedEntity,
    EngFunc_MakeStatic,
    EngFunc_EntIsOnFloor,
    EngFunc_DropToFloor,
    EngFunc_WalkMove,
    EngFunc_SetOrigin,
    EngFunc_EmitSound,
    EngFunc_EmitAmbientSound,
    EngFunc_TraceLine,
    EngFunc_TraceToss,
    EngFunc_TraceMonsterHull,
    EngFunc_TraceHull,
    EngFunc_TraceModel,
    EngFunc_TraceTexture,
    EngFunc_TraceSphere,
    EngFunc_GetAimVector,
    EngFunc_ParticleEffect,
    EngFunc_LightStyle,
    EngFunc_DecalIndex,
    EngFunc_PointContents,
    EngFunc_FreeEntPrivateData,
    EngFunc_SzFromIndex,
    EngFunc_AllocString,
    EngFunc_RegUserMsg,
    EngFunc_AnimationAutomove,
    EngFunc_GetBonePosition,
    EngFunc_GetAttachment,
    EngFunc_SetView,
    EngFunc_Time,
    EngFunc_CrosshairAngle,
    EngFunc_FadeClientVolume,
    EngFunc_SetClientMaxspeed,
    EngFunc_CreateFakeClient,
    EngFunc_RunPlayerMove,
    EngFunc_NumberOfEntities,
    EngFunc_StaticDecal,
    EngFunc_PrecacheGeneric,
    EngFunc_BuildSoundMsg,
    EngFunc_GetPhysicsKeyValue,
    EngFunc_SetPhysicsKeyValue,
    EngFunc_GetPhysicsInfoString,
    EngFunc_PrecacheEvent,
    EngFunc_PlaybackEvent,
    EngFunc_CheckVisibility,
    EngFunc_GetCurrentPlayer,
    EngFunc_CanSkipPlayer,
    EngFunc_SetGroupMask,
    EngFunc_GetClientListening,
    EngFunc_SetClientListening,
    EngFunc_MessageBegin,
    EngFunc_WriteCoord,
    EngFunc_WriteAngle,
    EngFunc_InfoKeyValue,
    EngFunc_SetKeyValue,
    EngFunc_SetClientKeyValue,
    EngFunc_CreateInstBaseline,
    EngFunc_GetInfoKeyBuffer,
    EngFunc_AlertMessage,
    EngFunc_ClientPrintf,
    EngFunc_ServerPrint,
    EngFunc_MAX
};

// Provided by native_fakemeta.cpp: tr2 handle -> TraceResult*
extern TraceResult *FM_GetTraceResult(int handle);

// ===== Helpers for Fakemeta variadic engfunc/dllfunc =====
// Fakemeta's engfunc/dllfunc are declared as variadic natives (type, any:...).
// The Small compiler passes all variadic args by reference: params[N] is an AMX-space
// address that must be dereferenced with amx_GetAddr to obtain the actual value.
static int FM_GetCell(AMX *amx, cell *params, int n)
{
    cell *cRet = nullptr;
    if (amx_GetAddr(amx, params[n], &cRet) != AMX_ERR_NONE || !cRet)
        return 0;
    return (int)cRet[0];
}

static float FM_GetFloat(AMX *amx, cell *params, int n)
{
    cell v = FM_GetCell(amx, params, n);
    return amx_ctof(v);
}

static edict_t *FM_GetEntity(AMX *amx, cell *params, int n)
{
    int idx = FM_GetCell(amx, params, n);
    return (idx > 0) ? INDEXENT(idx) : nullptr;
}

static int FM_GetStringSafe(AMX *amx, cell *params, int n, char *buf, int buflen)
{
    cell *addr = nullptr;
    if (amx_GetAddr(amx, params[n], &addr) != AMX_ERR_NONE || !addr) {
        buf[0] = 0;
        return 0;
    }
    amx_GetString(buf, addr, 0, buflen);
    return 1;
}

cell AMX_NATIVE_CALL amxx_engfunc(AMX *amx, cell *params)
{
    int funcIndex = params[1]; // fixed parameter (type), passed by value

    switch (funcIndex) {
        case EngFunc_PrecacheModel: {
            char model[256] = "";
            FM_GetStringSafe(amx, params, 2, model, sizeof(model));
            if (model[0] == 0)
                return 0;
            return PRECACHE_MODEL(model);
        }
        case EngFunc_PrecacheSound: {
            char sound[256] = "";
            FM_GetStringSafe(amx, params, 2, sound, sizeof(sound));
            if (sound[0] == 0)
                return 0;
            return PRECACHE_SOUND(sound);
        }
        case EngFunc_SetModel: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            char model[256] = "";
            FM_GetStringSafe(amx, params, 3, model, sizeof(model));
            SET_MODEL(pEdict, model);
            return 1;
        }
        case EngFunc_ModelIndex: {
            char model[256] = "";
            FM_GetStringSafe(amx, params, 2, model, sizeof(model));
            return MODEL_INDEX(model);
        }
        case EngFunc_ModelFrames: {
            return MODEL_FRAMES(FM_GetCell(amx, params, 2));
        }
        case EngFunc_SetSize: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            cell *minAddr = nullptr, *maxAddr = nullptr;
            amx_GetAddr(amx, params[3], &minAddr);
            amx_GetAddr(amx, params[4], &maxAddr);
            if (!minAddr || !maxAddr) return 0;
            Vector mins(amx_ctof(minAddr[0]), amx_ctof(minAddr[1]), amx_ctof(minAddr[2]));
            Vector maxs(amx_ctof(maxAddr[0]), amx_ctof(maxAddr[1]), amx_ctof(maxAddr[2]));
            SET_SIZE(pEdict, mins, maxs);
            return 1;
        }
        case EngFunc_ChangeLevel: {
            char map[256] = "";
            FM_GetStringSafe(amx, params, 2, map, sizeof(map));
            CHANGE_LEVEL(map, "");
            return 1;
        }
        case EngFunc_VecToYaw: {
            cell *inArr = nullptr, *outArr = nullptr;
            amx_GetAddr(amx, params[2], &inArr);
            amx_GetAddr(amx, params[3], &outArr);
            if (!inArr || !outArr) return 0;
            Vector vec(amx_ctof(inArr[0]), amx_ctof(inArr[1]), amx_ctof(inArr[2]));
            float yaw = VEC_TO_YAW(vec);
            outArr[0] = amx_ftoc(yaw);
            return 1;
        }
        case EngFunc_VecToAngles: {
            cell *inArr = nullptr, *outArr = nullptr;
            amx_GetAddr(amx, params[2], &inArr);
            amx_GetAddr(amx, params[3], &outArr);
            if (!inArr || !outArr) return 0;
            Vector vec(amx_ctof(inArr[0]), amx_ctof(inArr[1]), amx_ctof(inArr[2]));
            Vector angles;
            VEC_TO_ANGLES(vec, angles);
            outArr[0] = amx_ftoc(angles.x);
            outArr[1] = amx_ftoc(angles.y);
            outArr[2] = amx_ftoc(angles.z);
            return 1;
        }
        case EngFunc_MoveToOrigin: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            cell *goalAddr = nullptr;
            amx_GetAddr(amx, params[3], &goalAddr);
            if (!goalAddr) return 0;
            Vector goal(amx_ctof(goalAddr[0]), amx_ctof(goalAddr[1]), amx_ctof(goalAddr[2]));
            float dist = FM_GetFloat(amx, params, 4);
            int moveType = FM_GetCell(amx, params, 5);
            MOVE_TO_ORIGIN(pEdict, goal, dist, moveType);
            return 1;
        }
        case EngFunc_ChangeYaw: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            g_engfuncs.pfnChangeYaw(pEdict);
            return 1;
        }
        case EngFunc_ChangePitch: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            g_engfuncs.pfnChangePitch(pEdict);
            return 1;
        }
        case EngFunc_FindEntityByString: {
            int start = FM_GetCell(amx, params, 2);
            char field[64] = "", value[256] = "";
            FM_GetStringSafe(amx, params, 3, field, sizeof(field));
            FM_GetStringSafe(amx, params, 4, value, sizeof(value));
            edict_t *pStart = (start == -1) ? nullptr : INDEXENT(start);
            edict_t *pEdict = FIND_ENTITY_BY_STRING(pStart, field, value);
            return pEdict ? ENTINDEX(pEdict) : -1;
        }
        case EngFunc_GetEntityIllum: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            return GETENTITYILLUM(pEdict);
        }
        case EngFunc_FindEntityInSphere: {
            int start = FM_GetCell(amx, params, 2);
            cell *posAddr = nullptr;
            amx_GetAddr(amx, params[3], &posAddr);
            if (!posAddr) return -1;
            Vector pos(amx_ctof(posAddr[0]), amx_ctof(posAddr[1]), amx_ctof(posAddr[2]));
            float radius = FM_GetFloat(amx, params, 4);
            edict_t *pStart = (start == -1) ? nullptr : INDEXENT(start);
            edict_t *pEnt = FIND_ENTITY_IN_SPHERE(pStart, pos, radius);
            return pEnt ? ENTINDEX(pEnt) : -1;
        }
        case EngFunc_FindClientInPVS: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            edict_t *pRet = FIND_CLIENT_IN_PVS(pEdict);
            return pRet ? ENTINDEX(pRet) : 0;
        }
        case EngFunc_EntitiesInPVS: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            edict_t *pRet = FIND_ENTITY_IN_PVS(pEdict);
            return pRet ? ENTINDEX(pRet) : 0;
        }
        case EngFunc_MakeVectors: {
            cell *angAddr = nullptr;
            amx_GetAddr(amx, params[2], &angAddr);
            if (!angAddr) return 0;
            float angles[3];
            angles[0] = amx_ctof(angAddr[0]);
            angles[1] = amx_ctof(angAddr[1]);
            angles[2] = amx_ctof(angAddr[2]);
            g_engfuncs.pfnMakeVectors(angles);
            return 1;
        }
        case EngFunc_AngleVectors: {
            cell *angAddr = nullptr;
            amx_GetAddr(amx, params[2], &angAddr);
            if (!angAddr) return 0;
            float angles[3];
            angles[0] = amx_ctof(angAddr[0]);
            angles[1] = amx_ctof(angAddr[1]);
            angles[2] = amx_ctof(angAddr[2]);
            float forward[3], right[3], up[3];
            g_engfuncs.pfnAngleVectors(angles, forward, right, up);
            cell *dest;
            int nparams = (int)(params[0] / sizeof(cell));
            if (nparams >= 3) {
                amx_GetAddr(amx, params[3], &dest);
                if (dest) { dest[0] = amx_ftoc(forward[0]); dest[1] = amx_ftoc(forward[1]); dest[2] = amx_ftoc(forward[2]); }
            }
            if (nparams >= 4) {
                amx_GetAddr(amx, params[4], &dest);
                if (dest) { dest[0] = amx_ftoc(right[0]); dest[1] = amx_ftoc(right[1]); dest[2] = amx_ftoc(right[2]); }
            }
            if (nparams >= 5) {
                amx_GetAddr(amx, params[5], &dest);
                if (dest) { dest[0] = amx_ftoc(up[0]); dest[1] = amx_ftoc(up[1]); dest[2] = amx_ftoc(up[2]); }
            }
            return 1;
        }
        case EngFunc_CreateEntity: {
            edict_t *pEnt = CREATE_ENTITY();
            return pEnt ? ENTINDEX(pEnt) : 0;
        }
        case EngFunc_RemoveEntity: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict || pEdict->free) return 0;
            REMOVE_ENTITY(pEdict);
            return 1;
        }
        case EngFunc_CreateNamedEntity: {
            // variadic args are passed by reference: dereference first to get the string_t value
            int className = FM_GetCell(amx, params, 2);
            edict_t *pEnt = CREATE_NAMED_ENTITY(className);
            return pEnt ? ENTINDEX(pEnt) : 0;
        }
        case EngFunc_MakeStatic: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            MAKE_STATIC(pEdict);
            return 1;
        }
        case EngFunc_EntIsOnFloor: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            return ENT_IS_ON_FLOOR(pEdict) ? 1 : 0;
        }
        case EngFunc_DropToFloor: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            return DROP_TO_FLOOR(pEdict);
        }
        case EngFunc_WalkMove: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            float yaw = FM_GetFloat(amx, params, 3);
            float dist = FM_GetFloat(amx, params, 4);
            int mode = FM_GetCell(amx, params, 5);
            return WALK_MOVE(pEdict, yaw, dist, mode);
        }
        case EngFunc_SetOrigin: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            cell *orgAddr = nullptr;
            amx_GetAddr(amx, params[3], &orgAddr);
            if (!orgAddr) return 0;
            Vector origin(amx_ctof(orgAddr[0]), amx_ctof(orgAddr[1]), amx_ctof(orgAddr[2]));
            g_engfuncs.pfnSetOrigin(pEdict, origin);
            return 1;
        }
        case EngFunc_EmitSound: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            int channel = FM_GetCell(amx, params, 3);
            char sample[256] = "";
            FM_GetStringSafe(amx, params, 4, sample, sizeof(sample));
            float vol = FM_GetFloat(amx, params, 5);
            float atten = FM_GetFloat(amx, params, 6);
            int fFlags = FM_GetCell(amx, params, 7);
            int pitch = FM_GetCell(amx, params, 8);
            g_engfuncs.pfnEmitSound(pEdict, channel, sample, vol, atten, fFlags, pitch);
            return 1;
        }
        case EngFunc_EmitAmbientSound: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            cell *posAddr = nullptr;
            amx_GetAddr(amx, params[3], &posAddr);
            if (!posAddr) return 0;
            float pos[3];
            pos[0] = amx_ctof(posAddr[0]);
            pos[1] = amx_ctof(posAddr[1]);
            pos[2] = amx_ctof(posAddr[2]);
            char samp[256] = "";
            FM_GetStringSafe(amx, params, 4, samp, sizeof(samp));
            float vol = FM_GetFloat(amx, params, 5);
            float atten = FM_GetFloat(amx, params, 6);
            int fFlags = FM_GetCell(amx, params, 7);
            int pitch = FM_GetCell(amx, params, 8);
            g_engfuncs.pfnEmitAmbientSound(pEdict, pos, samp, vol, atten, fFlags, pitch);
            return 1;
        }
        case EngFunc_TraceLine: {
            cell *srcAddr = nullptr, *endAddr = nullptr;
            amx_GetAddr(amx, params[2], &srcAddr);
            amx_GetAddr(amx, params[3], &endAddr);
            if (!srcAddr || !endAddr) return 0;
            float v1[3], v2[3];
            v1[0] = amx_ctof(srcAddr[0]); v1[1] = amx_ctof(srcAddr[1]); v1[2] = amx_ctof(srcAddr[2]);
            v2[0] = amx_ctof(endAddr[0]); v2[1] = amx_ctof(endAddr[1]); v2[2] = amx_ctof(endAddr[2]);
            int fNoMonsters = FM_GetCell(amx, params, 4);
            int skip = FM_GetCell(amx, params, 5);
            TraceResult *tr = &g_lastTrace;
            int nparams = (int)(params[0] / sizeof(cell));
            if (nparams >= 6) {
                TraceResult *h = FM_GetTraceResult(FM_GetCell(amx, params, 6));
                if (h) tr = h;
            }
            g_engfuncs.pfnTraceLine(v1, v2, fNoMonsters, skip == -1 ? nullptr : INDEXENT(skip), tr);
            return 1;
        }
        case EngFunc_TraceToss: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            int ignore = FM_GetCell(amx, params, 3);
            TraceResult *tr = &g_lastTrace;
            int nparams = (int)(params[0] / sizeof(cell));
            if (nparams >= 4) {
                TraceResult *h = FM_GetTraceResult(FM_GetCell(amx, params, 4));
                if (h) tr = h;
            }
            g_engfuncs.pfnTraceToss(pEdict, ignore == -1 ? nullptr : INDEXENT(ignore), tr);
            return 1;
        }
        case EngFunc_TraceMonsterHull: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            cell *srcAddr = nullptr, *endAddr = nullptr;
            amx_GetAddr(amx, params[3], &srcAddr);
            amx_GetAddr(amx, params[4], &endAddr);
            if (!srcAddr || !endAddr) return 0;
            float v1[3], v2[3];
            v1[0] = amx_ctof(srcAddr[0]); v1[1] = amx_ctof(srcAddr[1]); v1[2] = amx_ctof(srcAddr[2]);
            v2[0] = amx_ctof(endAddr[0]); v2[1] = amx_ctof(endAddr[1]); v2[2] = amx_ctof(endAddr[2]);
            int fNoMonsters = FM_GetCell(amx, params, 5);
            int skip = FM_GetCell(amx, params, 6);
            TraceResult *tr = &g_lastTrace;
            int nparams = (int)(params[0] / sizeof(cell));
            if (nparams >= 7) {
                TraceResult *h = FM_GetTraceResult(FM_GetCell(amx, params, 7));
                if (h) tr = h;
            }
            return g_engfuncs.pfnTraceMonsterHull(pEdict, v1, v2, fNoMonsters, skip == 0 ? nullptr : INDEXENT(skip), tr);
        }
        case EngFunc_TraceHull: {
            cell *srcAddr = nullptr, *endAddr = nullptr;
            amx_GetAddr(amx, params[2], &srcAddr);
            amx_GetAddr(amx, params[3], &endAddr);
            if (!srcAddr || !endAddr) return 0;
            float v1[3], v2[3];
            v1[0] = amx_ctof(srcAddr[0]); v1[1] = amx_ctof(srcAddr[1]); v1[2] = amx_ctof(srcAddr[2]);
            v2[0] = amx_ctof(endAddr[0]); v2[1] = amx_ctof(endAddr[1]); v2[2] = amx_ctof(endAddr[2]);
            int fNoMonsters = FM_GetCell(amx, params, 4);
            int hullNumber = FM_GetCell(amx, params, 5);
            int skip = FM_GetCell(amx, params, 6);
            TraceResult *tr = &g_lastTrace;
            int nparams = (int)(params[0] / sizeof(cell));
            if (nparams >= 7) {
                TraceResult *h = FM_GetTraceResult(FM_GetCell(amx, params, 7));
                if (h) tr = h;
            }
            g_engfuncs.pfnTraceHull(v1, v2, fNoMonsters, hullNumber, skip == 0 ? nullptr : INDEXENT(skip), tr);
            return 1;
        }
        case EngFunc_TraceModel: {
            cell *srcAddr = nullptr, *endAddr = nullptr;
            amx_GetAddr(amx, params[2], &srcAddr);
            amx_GetAddr(amx, params[3], &endAddr);
            if (!srcAddr || !endAddr) return 0;
            float v1[3], v2[3];
            v1[0] = amx_ctof(srcAddr[0]); v1[1] = amx_ctof(srcAddr[1]); v1[2] = amx_ctof(srcAddr[2]);
            v2[0] = amx_ctof(endAddr[0]); v2[1] = amx_ctof(endAddr[1]); v2[2] = amx_ctof(endAddr[2]);
            int hullNumber = FM_GetCell(amx, params, 4);
            int pent = FM_GetCell(amx, params, 5);
            TraceResult *tr = &g_lastTrace;
            int nparams = (int)(params[0] / sizeof(cell));
            if (nparams >= 6) {
                TraceResult *h = FM_GetTraceResult(FM_GetCell(amx, params, 6));
                if (h) tr = h;
            }
            g_engfuncs.pfnTraceModel(v1, v2, hullNumber, pent == 0 ? nullptr : INDEXENT(pent), tr);
            return 1;
        }
        case EngFunc_TraceTexture: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            cell *srcAddr = nullptr, *endAddr = nullptr;
            amx_GetAddr(amx, params[3], &srcAddr);
            amx_GetAddr(amx, params[4], &endAddr);
            if (!srcAddr || !endAddr) return 0;
            float v1[3], v2[3];
            v1[0] = amx_ctof(srcAddr[0]); v1[1] = amx_ctof(srcAddr[1]); v1[2] = amx_ctof(srcAddr[2]);
            v2[0] = amx_ctof(endAddr[0]); v2[1] = amx_ctof(endAddr[1]); v2[2] = amx_ctof(endAddr[2]);
            const char *tex = g_engfuncs.pfnTraceTexture(pEdict, v1, v2);
            if (!tex) tex = "";
            cell *dest = nullptr;
            amx_GetAddr(amx, params[5], &dest);
            if (dest) {
                int maxLen = ((int)(params[0] / sizeof(cell)) >= 6) ? FM_GetCell(amx, params, 6) : 255;
                if (maxLen > 0)
                    amx_SetString(dest, tex, 0, 0, maxLen);
            }
            return tex[0] != 0 ? 1 : 0;
        }
        case EngFunc_TraceSphere: {
            cell *srcAddr = nullptr, *endAddr = nullptr;
            amx_GetAddr(amx, params[2], &srcAddr);
            amx_GetAddr(amx, params[3], &endAddr);
            if (!srcAddr || !endAddr) return 0;
            float v1[3], v2[3];
            v1[0] = amx_ctof(srcAddr[0]); v1[1] = amx_ctof(srcAddr[1]); v1[2] = amx_ctof(srcAddr[2]);
            v2[0] = amx_ctof(endAddr[0]); v2[1] = amx_ctof(endAddr[1]); v2[2] = amx_ctof(endAddr[2]);
            int fNoMonsters = FM_GetCell(amx, params, 4);
            float radius = FM_GetFloat(amx, params, 5);
            int skip = FM_GetCell(amx, params, 6);
            TraceResult *tr = &g_lastTrace;
            int nparams = (int)(params[0] / sizeof(cell));
            if (nparams >= 7) {
                TraceResult *h = FM_GetTraceResult(FM_GetCell(amx, params, 7));
                if (h) tr = h;
            }
            g_engfuncs.pfnTraceSphere(v1, v2, fNoMonsters, radius, skip == 0 ? nullptr : INDEXENT(skip), tr);
            return 1;
        }
        case EngFunc_GetAimVector: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            float speed = FM_GetFloat(amx, params, 3);
            float out[3];
            g_engfuncs.pfnGetAimVector(pEdict, speed, out);
            cell *dest = nullptr;
            amx_GetAddr(amx, params[4], &dest);
            if (dest) {
                dest[0] = amx_ftoc(out[0]);
                dest[1] = amx_ftoc(out[1]);
                dest[2] = amx_ftoc(out[2]);
            }
            return 1;
        }
        case EngFunc_ParticleEffect: {
            cell *orgAddr = nullptr, *dirAddr = nullptr;
            amx_GetAddr(amx, params[2], &orgAddr);
            amx_GetAddr(amx, params[3], &dirAddr);
            if (!orgAddr || !dirAddr) return 0;
            float org[3], dir[3];
            org[0] = amx_ctof(orgAddr[0]); org[1] = amx_ctof(orgAddr[1]); org[2] = amx_ctof(orgAddr[2]);
            dir[0] = amx_ctof(dirAddr[0]); dir[1] = amx_ctof(dirAddr[1]); dir[2] = amx_ctof(dirAddr[2]);
            float color = FM_GetFloat(amx, params, 4);
            float count = FM_GetFloat(amx, params, 5);
            g_engfuncs.pfnParticleEffect(org, dir, color, count);
            return 1;
        }
        case EngFunc_LightStyle: {
            int style = FM_GetCell(amx, params, 2);
            char val[64] = "";
            FM_GetStringSafe(amx, params, 3, val, sizeof(val));
            g_engfuncs.pfnLightStyle(style, val);
            return 1;
        }
        case EngFunc_DecalIndex: {
            char name[128] = "";
            FM_GetStringSafe(amx, params, 2, name, sizeof(name));
            return g_engfuncs.pfnDecalIndex(name);
        }
        case EngFunc_PointContents: {
            cell *posAddr = nullptr;
            amx_GetAddr(amx, params[2], &posAddr);
            if (!posAddr) return 0;
            float pos[3];
            pos[0] = amx_ctof(posAddr[0]);
            pos[1] = amx_ctof(posAddr[1]);
            pos[2] = amx_ctof(posAddr[2]);
            return g_engfuncs.pfnPointContents(pos);
        }
        case EngFunc_FreeEntPrivateData: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            g_engfuncs.pfnFreeEntPrivateData(pEdict);
            return 1;
        }
        case EngFunc_SzFromIndex: {
            const char *str = g_engfuncs.pfnSzFromIndex(FM_GetCell(amx, params, 2));
            if (!str) str = "";
            cell *dest = nullptr;
            amx_GetAddr(amx, params[3], &dest);
            if (dest) {
                int maxLen = ((int)(params[0] / sizeof(cell)) >= 4) ? FM_GetCell(amx, params, 4) : 255;
                if (maxLen > 0)
                    amx_SetString(dest, str, 0, 0, maxLen);
            }
            return 1;
        }
        case EngFunc_AllocString: {
            char sz[256] = "";
            FM_GetStringSafe(amx, params, 2, sz, sizeof(sz));
            return g_engfuncs.pfnAllocString(sz);
        }
        case EngFunc_RegUserMsg: {
            char name[128] = "";
            FM_GetStringSafe(amx, params, 2, name, sizeof(name));
            return g_engfuncs.pfnRegUserMsg(name, FM_GetCell(amx, params, 3));
        }
        case EngFunc_AnimationAutomove: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            float flTime = FM_GetFloat(amx, params, 3);
            g_engfuncs.pfnAnimationAutomove(pEdict, flTime);
            return 1;
        }
        case EngFunc_GetBonePosition: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            int bone = FM_GetCell(amx, params, 3);
            float origin[3], angles[3];
            g_engfuncs.pfnGetBonePosition(pEdict, bone, origin, angles);
            cell *dest = nullptr;
            amx_GetAddr(amx, params[4], &dest);
            if (dest) {
                dest[0] = amx_ftoc(origin[0]);
                dest[1] = amx_ftoc(origin[1]);
                dest[2] = amx_ftoc(origin[2]);
            }
            amx_GetAddr(amx, params[5], &dest);
            if (dest) {
                dest[0] = amx_ftoc(angles[0]);
                dest[1] = amx_ftoc(angles[1]);
                dest[2] = amx_ftoc(angles[2]);
            }
            return 1;
        }
        case EngFunc_GetAttachment: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            int attachment = FM_GetCell(amx, params, 3);
            float origin[3], angles[3];
            g_engfuncs.pfnGetAttachment(pEdict, attachment, origin, angles);
            cell *dest = nullptr;
            amx_GetAddr(amx, params[4], &dest);
            if (dest) {
                dest[0] = amx_ftoc(origin[0]);
                dest[1] = amx_ftoc(origin[1]);
                dest[2] = amx_ftoc(origin[2]);
            }
            amx_GetAddr(amx, params[5], &dest);
            if (dest) {
                dest[0] = amx_ftoc(angles[0]);
                dest[1] = amx_ftoc(angles[1]);
                dest[2] = amx_ftoc(angles[2]);
            }
            return 1;
        }
        case EngFunc_SetView: {
            edict_t *pClient = FM_GetEntity(amx, params, 2);
            if (!pClient) return 0;
            edict_t *pViewEnt = FM_GetEntity(amx, params, 3);
            if (!pViewEnt) return 0;
            g_engfuncs.pfnSetView(pClient, pViewEnt);
            return 1;
        }
        case EngFunc_Time: {
            return amx_ftoc(gpGlobals->time);
        }
        case EngFunc_CrosshairAngle: {
            edict_t *pClient = FM_GetEntity(amx, params, 2);
            if (!pClient) return 0;
            float pitch = FM_GetFloat(amx, params, 3);
            float yaw = FM_GetFloat(amx, params, 4);
            g_engfuncs.pfnCrosshairAngle(pClient, pitch, yaw);
            return 1;
        }
        case EngFunc_FadeClientVolume: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            g_engfuncs.pfnFadeClientVolume(pEdict, FM_GetCell(amx, params, 3), FM_GetCell(amx, params, 4), FM_GetCell(amx, params, 5), FM_GetCell(amx, params, 6));
            return 1;
        }
        case EngFunc_SetClientMaxspeed: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            float speed = FM_GetFloat(amx, params, 3);
            g_engfuncs.pfnSetClientMaxspeed(pEdict, speed);
            return 1;
        }
        case EngFunc_CreateFakeClient: {
            char name[128] = "";
            FM_GetStringSafe(amx, params, 2, name, sizeof(name));
            edict_t *pEdict = g_engfuncs.pfnCreateFakeClient(name);
            return pEdict ? ENTINDEX(pEdict) : 0;
        }
        case EngFunc_RunPlayerMove: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            cell *vaAddr = nullptr;
            amx_GetAddr(amx, params[3], &vaAddr);
            if (!vaAddr) return 0;
            float viewangles[3];
            viewangles[0] = amx_ctof(vaAddr[0]);
            viewangles[1] = amx_ctof(vaAddr[1]);
            viewangles[2] = amx_ctof(vaAddr[2]);
            float forwardmove = FM_GetFloat(amx, params, 4);
            float sidemove = FM_GetFloat(amx, params, 5);
            float upmove = FM_GetFloat(amx, params, 6);
            g_engfuncs.pfnRunPlayerMove(pEdict, viewangles, forwardmove, sidemove, upmove,
                (unsigned short)FM_GetCell(amx, params, 7), (byte)FM_GetCell(amx, params, 8), (byte)FM_GetCell(amx, params, 9));
            return 1;
        }
        case EngFunc_NumberOfEntities: {
            return g_engfuncs.pfnNumberOfEntities();
        }
        case EngFunc_StaticDecal: {
            cell *orgAddr = nullptr;
            amx_GetAddr(amx, params[2], &orgAddr);
            if (!orgAddr) return 0;
            float origin[3];
            origin[0] = amx_ctof(orgAddr[0]);
            origin[1] = amx_ctof(orgAddr[1]);
            origin[2] = amx_ctof(orgAddr[2]);
            g_engfuncs.pfnStaticDecal(origin, FM_GetCell(amx, params, 3), FM_GetCell(amx, params, 4), FM_GetCell(amx, params, 5));
            return 1;
        }
        case EngFunc_PrecacheGeneric: {
            char s[256] = "";
            FM_GetStringSafe(amx, params, 2, s, sizeof(s));
            return g_engfuncs.pfnPrecacheGeneric(s);
        }
        case EngFunc_BuildSoundMsg: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            int channel = FM_GetCell(amx, params, 3);
            char sample[256] = "";
            FM_GetStringSafe(amx, params, 4, sample, sizeof(sample));
            float vol = FM_GetFloat(amx, params, 5);
            float atten = FM_GetFloat(amx, params, 6);
            int fFlags = FM_GetCell(amx, params, 7);
            int pitch = FM_GetCell(amx, params, 8);
            int msgDest = FM_GetCell(amx, params, 9);
            int msgType = FM_GetCell(amx, params, 10);
            float pOrigin[3] = {0.0f, 0.0f, 0.0f};
            int nparams = (int)(params[0] / sizeof(cell));
            if (nparams >= 11) {
                cell *orgAddr = nullptr;
                amx_GetAddr(amx, params[11], &orgAddr);
                if (orgAddr) {
                    pOrigin[0] = amx_ctof(orgAddr[0]);
                    pOrigin[1] = amx_ctof(orgAddr[1]);
                    pOrigin[2] = amx_ctof(orgAddr[2]);
                }
            }
            int ed = (nparams >= 12) ? FM_GetCell(amx, params, 12) : 0;
            g_engfuncs.pfnBuildSoundMsg(pEdict, channel, sample, vol, atten, fFlags, pitch,
                msgDest, msgType, pOrigin, ed == 0 ? nullptr : INDEXENT(ed));
            return 1;
        }
        case EngFunc_GetPhysicsKeyValue: {
            edict_t *pClient = FM_GetEntity(amx, params, 2);
            if (!pClient) return 0;
            char key[64] = "";
            FM_GetStringSafe(amx, params, 3, key, sizeof(key));
            const char *val = g_engfuncs.pfnGetPhysicsKeyValue(pClient, key);
            if (!val) val = "";
            cell *dest = nullptr;
            amx_GetAddr(amx, params[4], &dest);
            if (dest) {
                int maxLen = ((int)(params[0] / sizeof(cell)) >= 5) ? FM_GetCell(amx, params, 5) : 255;
                if (maxLen > 0)
                    amx_SetString(dest, val, 0, 0, maxLen);
            }
            return 1;
        }
        case EngFunc_SetPhysicsKeyValue: {
            edict_t *pClient = FM_GetEntity(amx, params, 2);
            if (!pClient) return 0;
            char key[64] = "", value[256] = "";
            FM_GetStringSafe(amx, params, 3, key, sizeof(key));
            FM_GetStringSafe(amx, params, 4, value, sizeof(value));
            g_engfuncs.pfnSetPhysicsKeyValue(pClient, key, value);
            return 1;
        }
        case EngFunc_GetPhysicsInfoString: {
            edict_t *pClient = FM_GetEntity(amx, params, 2);
            if (!pClient) return 0;
            const char *info = g_engfuncs.pfnGetPhysicsInfoString(pClient);
            if (!info) info = "";
            cell *dest = nullptr;
            amx_GetAddr(amx, params[3], &dest);
            if (dest) {
                int maxLen = ((int)(params[0] / sizeof(cell)) >= 4) ? FM_GetCell(amx, params, 4) : 255;
                if (maxLen > 0)
                    amx_SetString(dest, info, 0, 0, maxLen);
            }
            return 1;
        }
        case EngFunc_PrecacheEvent: {
            int type = FM_GetCell(amx, params, 2);
            char name[256] = "";
            FM_GetStringSafe(amx, params, 3, name, sizeof(name));
            return g_engfuncs.pfnPrecacheEvent(type, name);
        }
        case EngFunc_PlaybackEvent: {
            int flags = FM_GetCell(amx, params, 2);
            int invoker = FM_GetCell(amx, params, 3);
            int eventindex = FM_GetCell(amx, params, 4);
            float delay = FM_GetFloat(amx, params, 5);
            cell *orgAddr = nullptr, *angAddr = nullptr;
            amx_GetAddr(amx, params[6], &orgAddr);
            amx_GetAddr(amx, params[7], &angAddr);
            float origin[3] = {0.0f, 0.0f, 0.0f};
            float angles[3] = {0.0f, 0.0f, 0.0f};
            if (orgAddr) {
                origin[0] = amx_ctof(orgAddr[0]);
                origin[1] = amx_ctof(orgAddr[1]);
                origin[2] = amx_ctof(orgAddr[2]);
            }
            if (angAddr) {
                angles[0] = amx_ctof(angAddr[0]);
                angles[1] = amx_ctof(angAddr[1]);
                angles[2] = amx_ctof(angAddr[2]);
            }
            float fparam1 = FM_GetFloat(amx, params, 8);
            float fparam2 = FM_GetFloat(amx, params, 9);
            int iparam1 = FM_GetCell(amx, params, 10);
            int iparam2 = FM_GetCell(amx, params, 11);
            int bparam1 = FM_GetCell(amx, params, 12);
            int bparam2 = FM_GetCell(amx, params, 13);
            g_engfuncs.pfnPlaybackEvent(flags, invoker == -1 ? nullptr : INDEXENT(invoker), eventindex,
                delay, origin, angles, fparam1, fparam2, iparam1, iparam2, bparam1, bparam2);
            return 1;
        }
        case EngFunc_CheckVisibility: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            unsigned char *pset = reinterpret_cast<unsigned char *>((intptr_t)FM_GetCell(amx, params, 3));
            return g_engfuncs.pfnCheckVisibility(pEdict, pset);
        }
        case EngFunc_GetCurrentPlayer: {
            return g_engfuncs.pfnGetCurrentPlayer();
        }
        case EngFunc_CanSkipPlayer: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            return g_engfuncs.pfnCanSkipPlayer(pEdict);
        }
        case EngFunc_SetGroupMask: {
            g_engfuncs.pfnSetGroupMask(FM_GetCell(amx, params, 2), FM_GetCell(amx, params, 3));
            return 1;
        }
        case EngFunc_GetClientListening: {
            return g_engfuncs.pfnVoice_GetClientListening(FM_GetCell(amx, params, 2), FM_GetCell(amx, params, 3));
        }
        case EngFunc_SetClientListening: {
            return g_engfuncs.pfnVoice_SetClientListening(FM_GetCell(amx, params, 2), FM_GetCell(amx, params, 3), FM_GetCell(amx, params, 4));
        }
        case EngFunc_MessageBegin: {
            int msgDest = FM_GetCell(amx, params, 2);
            int msgType = FM_GetCell(amx, params, 3);
            cell *orgAddr = nullptr;
            amx_GetAddr(amx, params[4], &orgAddr);
            float pOrigin[3] = {0.0f, 0.0f, 0.0f};
            if (orgAddr) {
                pOrigin[0] = amx_ctof(orgAddr[0]);
                pOrigin[1] = amx_ctof(orgAddr[1]);
                pOrigin[2] = amx_ctof(orgAddr[2]);
            }
            int ed = FM_GetCell(amx, params, 5);
            g_engfuncs.pfnMessageBegin(msgDest, msgType, pOrigin, ed == 0 ? nullptr : INDEXENT(ed));
            return 1;
        }
        case EngFunc_WriteCoord: {
            g_engfuncs.pfnWriteCoord(FM_GetFloat(amx, params, 2));
            return 1;
        }
        case EngFunc_WriteAngle: {
            g_engfuncs.pfnWriteAngle(FM_GetFloat(amx, params, 2));
            return 1;
        }
        case EngFunc_InfoKeyValue: {
            char *infobuffer = reinterpret_cast<char *>((intptr_t)FM_GetCell(amx, params, 2));
            if (!infobuffer) return 0;
            char key[64] = "";
            FM_GetStringSafe(amx, params, 3, key, sizeof(key));
            const char *val = g_engfuncs.pfnInfoKeyValue(infobuffer, key);
            if (!val) val = "";
            cell *dest = nullptr;
            amx_GetAddr(amx, params[4], &dest);
            if (dest) {
                int maxLen = ((int)(params[0] / sizeof(cell)) >= 5) ? FM_GetCell(amx, params, 5) : 255;
                if (maxLen > 0)
                    amx_SetString(dest, val, 0, 0, maxLen);
            }
            return 1;
        }
        case EngFunc_SetKeyValue: {
            char *infobuffer = reinterpret_cast<char *>((intptr_t)FM_GetCell(amx, params, 2));
            if (!infobuffer) return 0;
            char key[64] = "", value[256] = "";
            FM_GetStringSafe(amx, params, 3, key, sizeof(key));
            FM_GetStringSafe(amx, params, 4, value, sizeof(value));
            g_engfuncs.pfnSetKeyValue(infobuffer, key, value);
            return 1;
        }
        case EngFunc_SetClientKeyValue: {
            int clientIndex = FM_GetCell(amx, params, 2);
            if (clientIndex < 1 || clientIndex > gpGlobals->maxClients) return 0;
            char *infobuffer = reinterpret_cast<char *>((intptr_t)FM_GetCell(amx, params, 3));
            if (!infobuffer) return 0;
            char key[64] = "", value[256] = "";
            FM_GetStringSafe(amx, params, 4, key, sizeof(key));
            FM_GetStringSafe(amx, params, 5, value, sizeof(value));
            g_engfuncs.pfnSetClientKeyValue(clientIndex, infobuffer, key, value);
            return 1;
        }
        case EngFunc_CreateInstBaseline: {
            int classname = FM_GetCell(amx, params, 2);
            entity_state_t *es = reinterpret_cast<entity_state_t *>((intptr_t)FM_GetCell(amx, params, 3));
            if (!es) return 0;
            return g_engfuncs.pfnCreateInstancedBaseline(classname, es);
        }
        case EngFunc_GetInfoKeyBuffer: {
            int index = FM_GetCell(amx, params, 2);
            char *buf = g_engfuncs.pfnGetInfoKeyBuffer(index == -1 ? nullptr : INDEXENT(index));
            return reinterpret_cast<cell>(buf);
        }
        case EngFunc_AlertMessage: {
            int atype = FM_GetCell(amx, params, 2);
            char msg[1024] = "";
            FM_GetStringSafe(amx, params, 3, msg, sizeof(msg));
            g_engfuncs.pfnAlertMessage(static_cast<ALERT_TYPE>(atype), "%s", msg);
            return 1;
        }
        case EngFunc_ClientPrintf: {
            edict_t *pEdict = FM_GetEntity(amx, params, 2);
            if (!pEdict) return 0;
            int ptype = FM_GetCell(amx, params, 3);
            char msg[1024] = "";
            FM_GetStringSafe(amx, params, 4, msg, sizeof(msg));
            g_engfuncs.pfnClientPrintf(pEdict, static_cast<PRINT_TYPE>(ptype), msg);
            return 1;
        }
        case EngFunc_ServerPrint: {
            char msg[1024] = "";
            FM_GetStringSafe(amx, params, 2, msg, sizeof(msg));
            g_engfuncs.pfnServerPrint(msg);
            return 1;
        }
        default:
            return 0;
    }
}

// dllfunc(type, ent, ...) - calls an entity DLL function
// common DLLFunc_* indices: 1=GameInit 2=Spawn 3=Think 4=Use 5=Use 6=Blocked 7=KeyValue
// 8=Save 9=Restore 10=SetAbsBox 11=GetSaveInfo 12=ShouldToggleOnDeath
// 13=TraceAttack 14=TakeDamage 15=TakeHealth 16=Killed 17=BloodColor
// 18=TraceBleed 19=IsTriggered 20=MyMonsterPointer 21=MySquadMonsterPointer
// 22=GetGunPosition 23=GetTogglerState 24=GetToggleState 25=AddPoints
// 26=AddPointsToTeam 27=AddPlayerItem 28=GiveAmmo 29=GetNextBestWeapon
// 30=Intersects 31=PlayerRespawn 32=IsFixedVisibility
cell AMX_NATIVE_CALL amxx_dllfunc(AMX *amx, cell *params)
{
    int type = params[1]; // fixed parameter (type), passed by value
    // in dllfunc(type, ent, ...), ent and the following args are variadic (passed by reference)
    int entIndex = FM_GetCell(amx, params, 2);
    edict_t *pEdict = INDEXENT(entIndex);
    if (!pEdict) return 0;
    CBaseEntity *pEntity = CBaseEntity::Instance(pEdict);
    if (!pEntity && type != 1) return 0;  // GameInit needs no entity

    switch (type) {
        case 1: // DLLFunc_GameInit
            return 1; // already handled at game startup
        case 2: // DLLFunc_Spawn - via DispatchSpawn
            return DispatchSpawn(pEdict) >= 0 ? 1 : 0;
        case 3: { // DLLFunc_Think
            if (pEntity) { pEntity->Think(); return 1; }
            return 0;
        }
        case 4: // DLLFunc_Use(pActivator, pCaller, useType, valueFloat)
        case 5: {
            edict_t *pActivator = FM_GetEntity(amx, params, 3);
            edict_t *pCaller    = FM_GetEntity(amx, params, 4);
            USE_TYPE useType = (USE_TYPE)FM_GetCell(amx, params, 5);
            float value = FM_GetFloat(amx, params, 6);
            if (pEntity) {
                CBaseEntity *pAct = pActivator ? CBaseEntity::Instance(pActivator) : nullptr;
                CBaseEntity *pCll = pCaller    ? CBaseEntity::Instance(pCaller)    : nullptr;
                pEntity->Use(pAct, pCll, useType, value);
                return 1;
            }
            return 0;
        }
        case 6: { // DLLFunc_Blocked(pOther)
            edict_t *pOther = FM_GetEntity(amx, params, 3);
            CBaseEntity *pOtherEnt = pOther ? CBaseEntity::Instance(pOther) : nullptr;
            if (pEntity && pOtherEnt) {
                pEntity->Blocked(pOtherEnt);
                return 1;
            }
            return 0;
        }
        case 7: { // DLLFunc_KeyValue(infobuffer)
            if (pEntity) {
                // simplified: KeyValueData is empty
                KeyValueData kvd;
                memset(&kvd, 0, sizeof(kvd));
                pEntity->KeyValue(&kvd);
                return kvd.fHandled ? 1 : 0;
            }
            return 0;
        }
        case 13: { // DLLFunc_TraceAttack(pAttacker, flDamage, dir, ptr, bitsDamageType)
            edict_t *pAttacker = FM_GetEntity(amx, params, 3);
            float flDamage = FM_GetFloat(amx, params, 4);
            Vector dir;
            dir.x = FM_GetFloat(amx, params, 5); dir.y = FM_GetFloat(amx, params, 6); dir.z = FM_GetFloat(amx, params, 7);
            TraceResult *ptr = reinterpret_cast<TraceResult *>((intptr_t)FM_GetCell(amx, params, 8));
            int bitsDamageType = FM_GetCell(amx, params, 9);
            CBaseEntity *pAtt = pAttacker ? CBaseEntity::Instance(pAttacker) : nullptr;
            if (pEntity) {
                pEntity->TraceAttack(pAtt? pAtt->pev : nullptr, flDamage, dir, ptr, bitsDamageType);
                return 1;
            }
            return 0;
        }
        case 14: { // DLLFunc_TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType)
            entvars_t *pevInf = reinterpret_cast<entvars_t *>((intptr_t)FM_GetCell(amx, params, 3));
            entvars_t *pevAtt = reinterpret_cast<entvars_t *>((intptr_t)FM_GetCell(amx, params, 4));
            float flDamage = FM_GetFloat(amx, params, 5);
            int bits = FM_GetCell(amx, params, 6);
            if (pEntity)
                return pEntity->TakeDamage(pevInf, pevAtt, flDamage, bits) ? 1 : 0;
            return 0;
        }
        case 15: { // DLLFunc_TakeHealth(flHealth, bitsDamageType)
            float flHealth = FM_GetFloat(amx, params, 3);
            int bits = FM_GetCell(amx, params, 4);
            if (pEntity)
                return pEntity->TakeHealth(flHealth, bits) ? 1 : 0;
            return 0;
        }
        case 16: { // DLLFunc_Killed(pevAttacker, iGib)
            entvars_t *pevAtt = reinterpret_cast<entvars_t *>((intptr_t)FM_GetCell(amx, params, 3));
            int iGib = FM_GetCell(amx, params, 4);
            if (pEntity) { pEntity->Killed(pevAtt, iGib); return 1; }
            return 0;
        }
        case 31: { // DLLFunc_PlayerRespawn(CBasePlayer *)
            CBasePlayer *pPlayer = dynamic_cast<CBasePlayer *>(pEntity);
            if (pPlayer) { pPlayer->RoundRespawn(); return 1; }
            return 0;
        }
        default:
            return 0;
    }
}

// global_get/global_set use the GL_* enum values:
//   0=GL_trace_time(float) 1=GL_trace_inopen(int) 2=GL_trace_inwater(int)
//   3=GL_trace_allsolid(int) 4=GL_trace_startsolid(int) 5=GL_trace_fraction(float)
//   6=GL_trace_endpos(vec) 7=GL_trace_plane_normal(vec) 8=GL_trace_plane_dist(float)
//   9=GL_trace_hitgroup(int) 10=GL_trace_ent(edict) 11=GL_msg_entity(edict)
//   12=GL_trace_flags(int)
cell AMX_NATIVE_CALL amxx_global_get(AMX *amx, cell *params)
{
    int gl = (int)params[1];
    cell *dest = nullptr;
    int nparams = (int)(params[0] / sizeof(cell));
    if (nparams >= 2)
        amx_GetAddr(amx, params[2], &dest);

    switch (gl) {
        case 0: { float v = gpGlobals->time;            if (dest) { *dest = amx_ftoc(v); return 1; } return amx_ftoc(v); }
        case 1: { int v = (int)gpGlobals->trace_inopen; if (dest) { *dest = v; return 1; } return v; }
        case 2: { int v = (int)gpGlobals->trace_inwater;if (dest) { *dest = v; return 1; } return v; }
        case 3: { int v = (int)gpGlobals->trace_allsolid; if (dest) { *dest = v; return 1; } return v; }
        case 4: { int v = (int)gpGlobals->trace_startsolid; if (dest) { *dest = v; return 1; } return v; }
        case 5: { float v = gpGlobals->trace_fraction; if (dest) { *dest = amx_ftoc(v); return 1; } return amx_ftoc(v); }
        case 6: { if (dest) { dest[0] = amx_ftoc(gpGlobals->trace_endpos.x); dest[1] = amx_ftoc(gpGlobals->trace_endpos.y); dest[2] = amx_ftoc(gpGlobals->trace_endpos.z); return 1; } return 0; }
        case 7: { if (dest) { dest[0] = amx_ftoc(gpGlobals->trace_plane_normal.x); dest[1] = amx_ftoc(gpGlobals->trace_plane_normal.y); dest[2] = amx_ftoc(gpGlobals->trace_plane_normal.z); return 1; } return 0; }
        case 8: { float v = gpGlobals->trace_plane_dist; if (dest) { *dest = amx_ftoc(v); return 1; } return amx_ftoc(v); }
        case 9: { int v = gpGlobals->trace_hitgroup;    if (dest) { *dest = v; return 1; } return v; }
        case 10: { int v = gpGlobals->trace_ent ? ENTINDEX(gpGlobals->trace_ent) : -1; if (dest) { *dest = v; return 1; } return v; }
        case 11: { int v = gpGlobals->msg_entity;       if (dest) { *dest = v; return 1; } return v; }
        case 12: { int v = gpGlobals->trace_flags;      if (dest) { *dest = v; return 1; } return v; }
        default: return 0;
    }
}

cell AMX_NATIVE_CALL amxx_global_set(AMX *amx, cell *params)
{
    int gl = (int)params[1];
    cell *src = nullptr;
    int nparams = (int)(params[0] / sizeof(cell));
    if (nparams >= 2)
        amx_GetAddr(amx, params[2], &src);

    switch (gl) {
        case 0:  gpGlobals->time             = src ? amx_ctof(*src) : 0.0f; return 1;
        case 1:  gpGlobals->trace_inopen     = (float)(src ? *src : 0); return 1;
        case 2:  gpGlobals->trace_inwater    = (float)(src ? *src : 0); return 1;
        case 3:  gpGlobals->trace_allsolid   = (float)(src ? *src : 0); return 1;
        case 4:  gpGlobals->trace_startsolid = (float)(src ? *src : 0); return 1;
        case 5:  gpGlobals->trace_fraction   = src ? amx_ctof(*src) : 0.0f; return 1;
        case 6:  if (src) { gpGlobals->trace_endpos.x = amx_ctof(src[0]); gpGlobals->trace_endpos.y = amx_ctof(src[1]); gpGlobals->trace_endpos.z = amx_ctof(src[2]); } return 1;
        case 7:  if (src) { gpGlobals->trace_plane_normal.x = amx_ctof(src[0]); gpGlobals->trace_plane_normal.y = amx_ctof(src[1]); gpGlobals->trace_plane_normal.z = amx_ctof(src[2]); } return 1;
        case 8:  gpGlobals->trace_plane_dist = src ? amx_ctof(*src) : 0.0f; return 1;
        case 9:  gpGlobals->trace_hitgroup   = src ? *src : 0; return 1;
        case 10: gpGlobals->trace_ent        = (src && *src > 0) ? INDEXENT(*src) : nullptr; return 1;
        case 11: gpGlobals->msg_entity       = src ? *src : 0; return 1;
        case 12: gpGlobals->trace_flags      = src ? *src : 0; return 1;
        default: return 0;
    }
}

cell AMX_NATIVE_CALL amxx_register_think(AMX *amx, cell *params)
{
    cell *classname_addr;
    amx_GetAddr(amx, params[1], &classname_addr);
    char classname[64];
    amx_GetString(classname, classname_addr, 0, sizeof(classname));

    cell *funcname_addr;
    amx_GetAddr(amx, params[2], &funcname_addr);
    char funcname[64];
    amx_GetString(funcname, funcname_addr, 0, sizeof(funcname));

    int funcidx;
    if (amx_FindPublic(amx, funcname, &funcidx) != AMX_ERR_NONE)
        return 0;

    AMXXThinkTouch::GetInstance().RegisterThink(classname, amx, funcidx);

    ThinkReg r;
    r.id = s_nextHookId++;
    strncpy(r.classname, classname, sizeof(r.classname) - 1);
    r.classname[sizeof(r.classname) - 1] = '\0';
    r.amx = amx;
    r.funcidx = funcidx;
    g_thinkRegs.push_back(r);
    return r.id;
}

cell AMX_NATIVE_CALL amxx_register_touch(AMX *amx, cell *params)
{
    cell *toucher_addr, *touched_addr;
    amx_GetAddr(amx, params[1], &toucher_addr);
    amx_GetAddr(amx, params[2], &touched_addr);
    char toucher[64], touched[64];
    amx_GetString(toucher, toucher_addr, 0, sizeof(toucher));
    amx_GetString(touched, touched_addr, 0, sizeof(touched));

    cell *funcname_addr;
    amx_GetAddr(amx, params[3], &funcname_addr);
    char funcname[64];
    amx_GetString(funcname, funcname_addr, 0, sizeof(funcname));

    int funcidx;
    if (amx_FindPublic(amx, funcname, &funcidx) != AMX_ERR_NONE)
        return 0;

    AMXXThinkTouch::GetInstance().RegisterTouch(toucher, touched, amx, funcidx);

    TouchReg r;
    r.id = s_nextHookId++;
    strncpy(r.toucher, toucher, sizeof(r.toucher) - 1);
    r.toucher[sizeof(r.toucher) - 1] = '\0';
    strncpy(r.touched, touched, sizeof(r.touched) - 1);
    r.touched[sizeof(r.touched) - 1] = '\0';
    r.amx = amx;
    r.funcidx = funcidx;
    g_touchRegs.push_back(r);
    return r.id;
}

cell AMX_NATIVE_CALL amxx_pev_valid(AMX *amx, cell *params)
{
    (void)amx;
    int index = params[1];
    if (index < 0 || index > gpGlobals->maxEntities)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free)
        return 0;
    return 1;
}

cell AMX_NATIVE_CALL amxx_str_pev(AMX *amx, cell *params)
{
    int index = params[1];
    int offset = params[2];
    int maxlen = params[3];

    edict_t *pEntity = INDEXENT(index);
    if (!pEntity)
        return 0;

    cell *dest;
    amx_GetAddr(amx, params[4], &dest);

    // read the string_t from entvars, then resolve it via STRING()
    string_t strVal = *(string_t *)((char *)&pEntity->v + offset);
    const char *value = STRING(strVal);
    if (!value) value = "";
    return amx_SetString(dest, value, 0, 0, maxlen);
}

cell AMX_NATIVE_CALL amxx_set_pev_string(AMX *amx, cell *params)
{
    int index = params[1];
    int offset = params[2];

    edict_t *pEntity = INDEXENT(index);
    if (!pEntity)
        return 0;

    cell *addr;
    amx_GetAddr(amx, params[3], &addr);
    char value[256];
    amx_GetString(value, addr, 0, sizeof(value));

    // allocate a string_t and write it to entvars
    string_t newStr = ALLOC_STRING(value);
    *(string_t *)((char *)&pEntity->v + offset) = newStr;
    return 1;
}

cell AMX_NATIVE_CALL amxx_dispatch_spawn(AMX *amx, cell *params)
{
    int index = params[1];
    edict_t *pEntity = INDEXENT(index);
    if (!pEntity)
        return 0;

    int result = DispatchSpawn(pEntity);
    return result >= 0 ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_dispatch_keyvalue(AMX *amx, cell *params)
{
    int index = params[1];
    edict_t *pEntity = INDEXENT(index);
    if (!pEntity)
        return 0;

    cell *key_addr, *val_addr;
    amx_GetAddr(amx, params[2], &key_addr);
    amx_GetAddr(amx, params[3], &val_addr);
    char key[128], value[512];
    amx_GetString(key, key_addr, 0, sizeof(key));
    amx_GetString(value, val_addr, 0, sizeof(value));

    KeyValueData kvd;
    kvd.szClassName = (char *)STRING(pEntity->v.classname);
    kvd.szKeyName = key;
    kvd.szValue = value;
    kvd.fHandled = 0;

    DispatchKeyValue(pEntity, &kvd);
    return 1;
}

cell AMX_NATIVE_CALL amxx_find_ent_by_owner(AMX *amx, cell *params)
{
    // signature: find_ent_by_owner(start_from_ent, classname[], owner_index[, category])
    // params[1]=start, params[2]=classname, params[3]=ownerIndex, params[4]=category (optional)
    int iEnt = params[1];
    int oEnt = params[3];
    edict_t *pEnt = iEnt >= 0 ? INDEXENT(iEnt) : nullptr;
    edict_t *entOwner = INDEXENT(oEnt);

    // optional 4th param (jghg2 compat): 1=target, 2=targetname, default classname
    const char *sCategory = "classname";
    if (params[0] / sizeof(cell) >= 4) {
        switch (params[4]) {
            case 1: sCategory = "target"; break;
            case 2: sCategory = "targetname"; break;
            default: sCategory = "classname";
        }
    }

    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    char classname[256];
    amx_GetString(classname, addr, 0, sizeof(classname));

    while (!FNullEnt(pEnt = FIND_ENTITY_BY_STRING(pEnt, sCategory, classname))) {
        if (pEnt->v.owner == entOwner)
            return ENTINDEX(pEnt);
    }
    return 0;
}

cell AMX_NATIVE_CALL amxx_find_ent_by_target(AMX *amx, cell *params)
{
    int startEnt = params[1];

    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    char targetname[256];
    amx_GetString(targetname, addr, 0, sizeof(targetname));

    edict_t *pStart = startEnt >= 0 ? INDEXENT(startEnt) : nullptr;
    edict_t *pEnt = FIND_ENTITY_BY_STRING(pStart, "targetname", targetname);
    return pEnt ? ENTINDEX(pEnt) : 0;
}

cell AMX_NATIVE_CALL amxx_entity_range(AMX *amx, cell *params)
{
    int ent1 = params[1];
    int ent2 = params[2];

    edict_t *pEnt1 = INDEXENT(ent1);
    edict_t *pEnt2 = INDEXENT(ent2);
    if (!pEnt1 || !pEnt2)
        return 0;

    Vector delta = pEnt1->v.origin - pEnt2->v.origin;
    float dist = delta.Length();
    return amx_ftoc(dist);
}

// signature: RegisterHam(Ham:function, const EntityClass[], const Callback[], Post=0, bool:specialbot=false)
// params[1]=hookType, params[2]=classname (string), params[3]=callback function name, params[4]=post, params[5]=specialbot
cell AMX_NATIVE_CALL amxx_register_ham(AMX *amx, cell *params)
{
    int hookType = (int)params[1];
    int argCount = (int)(params[0] / sizeof(cell));
    if (argCount < 3)
    {
        AMXX_LOG_ERR("[Ham] RegisterHam: expected at least 3 arguments (function, classname[], callback[])");
        return 0;
    }

    cell *classname_addr;
    amx_GetAddr(amx, params[2], &classname_addr);
    char classname[64];
    amx_GetString(classname, classname_addr, 0, sizeof(classname));

    cell *funcname_addr;
    amx_GetAddr(amx, params[3], &funcname_addr);
    char funcname[64];
    amx_GetString(funcname, funcname_addr, 0, sizeof(funcname));

    int funcidx;
    if (amx_FindPublic(amx, funcname, &funcidx) != AMX_ERR_NONE)
    {
        AMXX_LOG_ERR("[Ham] RegisterHam: function \"%s\" not found", funcname);
        return 0;
    }

    bool post = argCount >= 4 && params[4] != 0;
    bool specialbot = argCount >= 5 && params[5] != 0;

    // returns a forward handle (>0) on success, or 0 on failure
    return (cell)AMXXHamSandwich::GetInstance().RegisterHam((HamHookType)hookType, amx, funcidx, post, specialbot);
}

cell AMX_NATIVE_CALL amxx_execute_ham(AMX *amx, cell *params)
{
    int hookType = params[1];
    int entityId = params[2];
    edict_t *pEdict = INDEXENT(entityId);
    if (!pEdict) return 0;
    CBaseEntity *pEnt = CBaseEntity::Instance(pEdict);

    switch (hookType) {
        case Ham_Spawn:
            return DispatchSpawn(pEdict) >= 0 ? 1 : 0;
        case Ham_Think: {
            if (pEnt) { pEnt->Think(); return 1; }
            return 0;
        }
        case Ham_Touch: {
            // Ham_Touch needs the toucher entity as an extra parameter
            edict_t *pToucher = INDEXENT(params[3]);
            if (!pToucher) return 0;
            CBaseEntity *pEnt2 = CBaseEntity::Instance(pToucher);
            if (pEnt && pEnt2) { pEnt->Touch(pEnt2); return 1; }
            return 0;
        }
        case Ham_Use: {
            // 原版签名: ExecuteHam(Ham_Use, this, idcaller, idactivator, use_type, Float:value)
            if (!pEnt) return 0;
            edict_t *pCaller = INDEXENT(params[3]);
            edict_t *pActivator = INDEXENT(params[4]);
            pEnt->Use(
                pActivator ? CBaseEntity::Instance(pActivator) : nullptr,
                pCaller ? CBaseEntity::Instance(pCaller) : nullptr,
                (USE_TYPE)params[5], amx_ctof(params[6]));
            return 1;
        }
        case Ham_ObjectCaps: {
            // 原版签名: ExecuteHam(Ham_ObjectCaps, this)
            if (pEnt) return pEnt->ObjectCaps();
            return 0;
        }
        case Ham_TraceAttack: {
            // 原版签名: ExecuteHam(Ham_TraceAttack, this, idattacker, Float:damage, Float:direction[3], tracehandle, damagebits)
            if (!pEnt) return 0;
            edict_t *pAttacker = INDEXENT(params[3]);
            Vector vecDir;
            vecDir.x = amx_ctof(params[5]);
            vecDir.y = amx_ctof(params[6]);
            vecDir.z = amx_ctof(params[7]);
            pEnt->TraceAttack(
                pAttacker ? &pAttacker->v : nullptr,
                amx_ctof(params[4]), vecDir,
                (TraceResult *)(intptr_t)params[8], (int)params[9]);
            return 1;
        }
        case Ham_TakeDamage: {
            // 原版签名: ExecuteHam(Ham_TakeDamage, this, idinflictor, idattacker, Float:damage, damagebits)
            CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
            if (!pPlayer) return 0;
            edict_t *pInflictor = INDEXENT(params[3]);
            edict_t *pAttacker = INDEXENT(params[4]);
            float damage = amx_ctof(params[5]);
            int bitsDamage = params[6];
            return pPlayer->TakeDamage(
                pInflictor ? &pInflictor->v : nullptr,
                pAttacker ? &pAttacker->v : nullptr,
                damage, bitsDamage) ? 1 : 0;
        }
        case Ham_Killed: {
            // 原版签名: ExecuteHam(Ham_Killed, this, idattacker, shouldgib)
            CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
            if (!pPlayer) return 0;
            edict_t *pAttacker = INDEXENT(params[3]);
            int iGib = params[4];
            pPlayer->Killed(pAttacker ? &pAttacker->v : nullptr, iGib);
            return 1;
        }
        case Ham_Item_Deploy: {
            // 原版签名: ExecuteHam(Ham_Item_Deploy, this)
            CBasePlayerWeapon *pWeapon = pEnt ? dynamic_cast<CBasePlayerWeapon *>(pEnt) : nullptr;
            if (!pWeapon) return 0;
            return pWeapon->Deploy() ? 1 : 0;
        }
        case Ham_Item_Holster: {
            // 原版签名: ExecuteHam(Ham_Item_Holster, this, skiplocal)
            CBasePlayerWeapon *pWeapon = pEnt ? dynamic_cast<CBasePlayerWeapon *>(pEnt) : nullptr;
            if (!pWeapon) return 0;
            pWeapon->Holster((int)params[3]);
            return 1;
        }
        case Ham_Item_PostFrame: {
            // 原版签名: ExecuteHam(Ham_Item_PostFrame, this)
            CBasePlayerWeapon *pWeapon = pEnt ? dynamic_cast<CBasePlayerWeapon *>(pEnt) : nullptr;
            if (!pWeapon) return 0;
            pWeapon->ItemPostFrame();
            return 1;
        }
        case Ham_Weapon_PrimaryAttack: {
            CBasePlayerWeapon *pWeapon = pEnt ? dynamic_cast<CBasePlayerWeapon *>(pEnt) : nullptr;
            if (!pWeapon) return 0;
            pWeapon->PrimaryAttack();
            return 1;
        }
        case Ham_Weapon_SecondaryAttack: {
            CBasePlayerWeapon *pWeapon = pEnt ? dynamic_cast<CBasePlayerWeapon *>(pEnt) : nullptr;
            if (!pWeapon) return 0;
            pWeapon->SecondaryAttack();
            return 1;
        }
        case Ham_Weapon_Reload: {
            CBasePlayerWeapon *pWeapon = pEnt ? dynamic_cast<CBasePlayerWeapon *>(pEnt) : nullptr;
            if (!pWeapon) return 0;
            pWeapon->Reload();
            return 1;
        }
        case Ham_Weapon_WeaponIdle: {
            CBasePlayerWeapon *pWeapon = pEnt ? dynamic_cast<CBasePlayerWeapon *>(pEnt) : nullptr;
            if (!pWeapon) return 0;
            pWeapon->WeaponIdle();
            return 1;
        }
        default:
            return 0;
    }
}

cell AMX_NATIVE_CALL amxx_execute_ham_b(AMX *amx, cell *params)
{
    (void)amx;
    int hookType = params[1];
    int entityId = params[2];
    edict_t *pEdict = INDEXENT(entityId);
    if (!pEdict) return 0;
    CBaseEntity *pEnt = CBaseEntity::Instance(pEdict);

    // ExecuteHamB 必须先触发已注册的 Ham 钩子：
    // HAM_SUPERCEDE 阻止原函数并返回 SetHamReturn* 值；
    // HAM_OVERRIDE 仍调用原函数，但用 SetHamReturn* 的值覆盖返回。
    HamCtxReset(g_hamCtx);

    switch (hookType) {
        case Ham_Spawn: {
            cell r = AMXXHamSandwich::GetInstance().DispatchHam(Ham_Spawn, 1, (cell)entityId);
            if (r == HAM_SUPERCEDE || r == HAM_OVERRIDE)
                return 0;
            return DispatchSpawn(pEdict) >= 0 ? 1 : 0;
        }
        case Ham_Think: {
            cell r = AMXXHamSandwich::GetInstance().DispatchHam(Ham_Think, 1, (cell)entityId);
            if (r == HAM_SUPERCEDE || r == HAM_OVERRIDE)
                return 0;
            if (pEnt) { pEnt->Think(); return 1; }
            return 0;
        }
        case Ham_Use: {
            cell r = AMXXHamSandwich::GetInstance().DispatchHam(Ham_Use, 5,
                (cell)entityId, (cell)params[3], (cell)params[4], (cell)params[5], (cell)params[6]);
            if (r == HAM_SUPERCEDE || r == HAM_OVERRIDE)
                return 0;
            if (!pEnt) return 0;
            edict_t *pCaller = INDEXENT(params[3]);
            edict_t *pActivator = INDEXENT(params[4]);
            pEnt->Use(
                pActivator ? CBaseEntity::Instance(pActivator) : nullptr,
                pCaller ? CBaseEntity::Instance(pCaller) : nullptr,
                (USE_TYPE)params[5], amx_ctof(params[6]));
            return 1;
        }
        case Ham_ObjectCaps: {
            cell r = AMXXHamSandwich::GetInstance().DispatchHam(Ham_ObjectCaps, 1, (cell)entityId);
            if (r == HAM_SUPERCEDE)
                return HamCtxReturnAsCell();
            if (!pEnt) return 0;
            int ret = pEnt->ObjectCaps();
            g_hamOrigCtx.returnType = 1;
            g_hamOrigCtx.intVal = ret;
            if (r == HAM_OVERRIDE)
                return HamCtxReturnAsCell();
            return ret;
        }
        case Ham_TraceAttack: {
            cell r = AMXXHamSandwich::GetInstance().DispatchHam(Ham_TraceAttack, 8,
                (cell)entityId, (cell)params[3], (cell)params[4],
                (cell)params[5], (cell)params[6], (cell)params[7],
                (cell)params[8], (cell)params[9]);
            if (r == HAM_SUPERCEDE || r == HAM_OVERRIDE)
                return 0;
            if (!pEnt) return 0;
            edict_t *pAttacker = INDEXENT(params[3]);
            Vector vecDir;
            vecDir.x = amx_ctof(params[5]);
            vecDir.y = amx_ctof(params[6]);
            vecDir.z = amx_ctof(params[7]);
            pEnt->TraceAttack(
                pAttacker ? &pAttacker->v : nullptr,
                amx_ctof(params[4]), vecDir,
                (TraceResult *)(intptr_t)params[8], (int)params[9]);
            return 1;
        }
        case Ham_TakeDamage: {
            // 原版签名: ExecuteHamB(Ham_TakeDamage, this, idinflictor, idattacker, Float:damage, damagebits)
            int inflictorId = (int)params[3];
            int attackerId = (int)params[4];
            float damage = amx_ctof(params[5]);
            int bitsDamage = (int)params[6];
            cell r = AMXXHamSandwich::GetInstance().DispatchHam(Ham_TakeDamage, 5,
                (cell)entityId, (cell)inflictorId, (cell)attackerId, amx_ftoc(damage), (cell)bitsDamage);
            if (r == HAM_SUPERCEDE)
                return HamCtxReturnAsCell();
            CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
            if (!pPlayer) return 0;
            edict_t *pInflictor = INDEXENT(inflictorId);
            edict_t *pAttacker = INDEXENT(attackerId);
            BOOL ret = pPlayer->TakeDamage(
                pInflictor ? &pInflictor->v : nullptr,
                pAttacker ? &pAttacker->v : nullptr,
                damage, bitsDamage);
            g_hamOrigCtx.returnType = 1;
            g_hamOrigCtx.intVal = ret;
            if (r == HAM_OVERRIDE)
                return HamCtxReturnAsCell();
            return ret ? 1 : 0;
        }
        case Ham_Killed: {
            edict_t *pAttacker = INDEXENT(params[3]);
            int iGib = params[4];
            cell r = AMXXHamSandwich::GetInstance().DispatchHam(Ham_Killed, 3,
                (cell)entityId, (cell)params[3], (cell)iGib);
            if (r == HAM_SUPERCEDE || r == HAM_OVERRIDE)
                return 0;
            CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
            if (!pPlayer) return 0;
            pPlayer->Killed(pAttacker ? &pAttacker->v : nullptr, iGib);
            return 1;
        }
        case Ham_Item_Deploy: {
            cell r = AMXXHamSandwich::GetInstance().DispatchHam(Ham_Weapon_Deploy, 1, (cell)entityId);
            if (r == HAM_SUPERCEDE)
                return HamCtxReturnAsCell();
            CBasePlayerWeapon *pWeapon = pEnt ? dynamic_cast<CBasePlayerWeapon *>(pEnt) : nullptr;
            if (!pWeapon) return 0;
            BOOL ret = pWeapon->Deploy();
            g_hamOrigCtx.returnType = 1;
            g_hamOrigCtx.intVal = ret;
            if (r == HAM_OVERRIDE)
                return HamCtxReturnAsCell();
            return ret ? 1 : 0;
        }
        case Ham_Item_Holster: {
            cell r = AMXXHamSandwich::GetInstance().DispatchHam(Ham_Weapon_Holster, 2,
                (cell)entityId, (cell)params[3]);
            if (r == HAM_SUPERCEDE || r == HAM_OVERRIDE)
                return 0;
            CBasePlayerWeapon *pWeapon = pEnt ? dynamic_cast<CBasePlayerWeapon *>(pEnt) : nullptr;
            if (!pWeapon) return 0;
            pWeapon->Holster((int)params[3]);
            return 1;
        }
        case Ham_Item_PostFrame: {
            cell r = AMXXHamSandwich::GetInstance().DispatchHam(Ham_Item_PostFrame, 1, (cell)entityId);
            if (r == HAM_SUPERCEDE || r == HAM_OVERRIDE)
                return 0;
            CBasePlayerWeapon *pWeapon = pEnt ? dynamic_cast<CBasePlayerWeapon *>(pEnt) : nullptr;
            if (!pWeapon) return 0;
            pWeapon->ItemPostFrame();
            return 1;
        }
        case Ham_Weapon_PrimaryAttack: {
            cell r = AMXXHamSandwich::GetInstance().DispatchHam(Ham_Weapon_PrimaryAttack, 1, (cell)entityId);
            if (r == HAM_SUPERCEDE || r == HAM_OVERRIDE)
                return 0;
            CBasePlayerWeapon *pWeapon = pEnt ? dynamic_cast<CBasePlayerWeapon *>(pEnt) : nullptr;
            if (!pWeapon) return 0;
            pWeapon->PrimaryAttack();
            return 1;
        }
        case Ham_Weapon_SecondaryAttack: {
            cell r = AMXXHamSandwich::GetInstance().DispatchHam(Ham_Weapon_SecondaryAttack, 1, (cell)entityId);
            if (r == HAM_SUPERCEDE || r == HAM_OVERRIDE)
                return 0;
            CBasePlayerWeapon *pWeapon = pEnt ? dynamic_cast<CBasePlayerWeapon *>(pEnt) : nullptr;
            if (!pWeapon) return 0;
            pWeapon->SecondaryAttack();
            return 1;
        }
        case Ham_Weapon_Reload: {
            cell r = AMXXHamSandwich::GetInstance().DispatchHam(Ham_Weapon_Reload, 1, (cell)entityId);
            if (r == HAM_SUPERCEDE || r == HAM_OVERRIDE)
                return 0;
            CBasePlayerWeapon *pWeapon = pEnt ? dynamic_cast<CBasePlayerWeapon *>(pEnt) : nullptr;
            if (!pWeapon) return 0;
            pWeapon->Reload();
            return 1;
        }
        case Ham_Weapon_WeaponIdle: {
            cell r = AMXXHamSandwich::GetInstance().DispatchHam(Ham_Weapon_WeaponIdle, 1, (cell)entityId);
            if (r == HAM_SUPERCEDE || r == HAM_OVERRIDE)
                return 0;
            CBasePlayerWeapon *pWeapon = pEnt ? dynamic_cast<CBasePlayerWeapon *>(pEnt) : nullptr;
            if (!pWeapon) return 0;
            pWeapon->WeaponIdle();
            return 1;
        }
        default:
            return 0;
    }
}

// ===== P0: Ham 鏁版嵁璇诲啓 =====
// get_ham_data_int(entity, offset, byte=4) 鈥?閫氳繃 CBaseEntity::Instance 璇诲啓
// offset 以 int(4 字节) 为单位（与原版 hamsandwich 一致），第 3 参 Windows 下忽略。
cell AMX_NATIVE_CALL amxx_get_ham_data_int(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    int offset = (int)params[2];

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free)
        return 0;

    void *pEntity = CBaseEntity::Instance(pEdict);
    if (!pEntity)
        return 0;

    int byteOffset = offset * 4;
    // P2: 非 Windows 平台加 linuxdiff 平台补偿 (对齐 pdata 系列默认值 5)
#ifndef _WIN32
    byteOffset += 5;
#endif

    int nparams = params[0] / sizeof(cell);
    int byte = (nparams >= 3) ? (int)params[3] : 4;

    switch (byte) {
        case 1: return *(unsigned char *)((char *)pEntity + byteOffset);
        case 2: return *(short *)((char *)pEntity + byteOffset);
        default: return *(cell *)((char *)pEntity + byteOffset);
    }
}

// set_ham_data_int(entity, offset, value, byte=4)
cell AMX_NATIVE_CALL amxx_set_ham_data_int(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    int offset = (int)params[2];
    cell value = params[3];

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free)
        return 0;

    void *pEntity = CBaseEntity::Instance(pEdict);
    if (!pEntity)
        return 0;

    int byteOffset = offset * 4;
    // P2: 非 Windows 平台加 linuxdiff 平台补偿 (与 get_ham_data_int 对称)
#ifndef _WIN32
    byteOffset += 5;
#endif

    int nparams = params[0] / sizeof(cell);
    int byte = (nparams >= 4) ? (int)params[4] : 4;

    switch (byte) {
        case 1: *(unsigned char *)((char *)pEntity + byteOffset) = (unsigned char)(value & 0xFF); break;
        case 2: *(short *)((char *)pEntity + byteOffset) = (short)value; break;
        default: *(cell *)((char *)pEntity + byteOffset) = value; break;
    }
    return 1;
}

// get_ham_data_float(entity, offset)
cell AMX_NATIVE_CALL amxx_get_ham_data_float(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    int offset = (int)params[2];

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free)
        return 0;

    void *pEntity = CBaseEntity::Instance(pEdict);
    if (!pEntity)
        return 0;

    int byteOffset = offset * 4;
    float val = *(float *)((char *)pEntity + byteOffset);
    return amx_ftoc(val);
}

// set_ham_data_float(entity, offset, value)
cell AMX_NATIVE_CALL amxx_set_ham_data_float(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    int offset = (int)params[2];

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free)
        return 0;

    void *pEntity = CBaseEntity::Instance(pEdict);
    if (!pEntity)
        return 0;

    int byteOffset = offset * 4;
    *(float *)((char *)pEntity + byteOffset) = amx_ctof(params[3]);
    return 1;
}

// RegisterHamFromEntity(hookType, EntityId, funcname[], Post=0) - 按实体 ID 注册 Ham 钩子
// 原版签名: RegisterHamFromEntity(Ham:function, EntityId, const Callback[], Post=0)
// params[1]=hookType, params[2]=EntityId(整数，非字符串), params[3]=callback函数名, params[4]=post
cell AMX_NATIVE_CALL amxx_register_ham_from_entity(AMX *amx, cell *params)
{
    int hookType = (int)params[1];
    int entityId = (int)params[2];
    int argCount = (int)(params[0] / sizeof(cell));
    if (argCount < 3)
    {
        AMXX_LOG_ERR("[Ham] RegisterHamFromEntity: expected at least 3 arguments (function, EntityId, callback[])");
        return 0;
    }

    cell *funcname_addr;
    amx_GetAddr(amx, params[3], &funcname_addr);
    char funcname[64];
    amx_GetString(funcname, funcname_addr, 0, sizeof(funcname));

    int funcidx;
    if (amx_FindPublic(amx, funcname, &funcidx) != AMX_ERR_NONE)
    {
        AMXX_LOG_ERR("[Ham] RegisterHamFromEntity: function \"%s\" not found", funcname);
        return 0;
    }

    // 校验实体有效（原版按实体 ID 解析类名后注册）
    edict_t *pEdict = nullptr;
    if (entityId < 1 || entityId > gpGlobals->maxEntities)
        pEdict = nullptr;
    else
        pEdict = INDEXENT(entityId);
    if (!pEdict || pEdict->free)
    {
        AMXX_LOG_ERR("[Ham] RegisterHamFromEntity: invalid entity id %d", entityId);
        return 0;
    }

    bool post = argCount >= 4 && params[4] != 0;
    return (cell)AMXXHamSandwich::GetInstance().RegisterHam((HamHookType)hookType, amx, funcidx, post, false);
}

// RegisterHamFromPlayer(hookType, funcname[], Post=0) - 注册玩家级别的 Ham 钩子
// 原版签名: RegisterHamPlayer(Ham:function, const Callback[], Post=0) -> RegisterHam(function, "player", Callback, Post, true)
cell AMX_NATIVE_CALL amxx_register_ham_from_player(AMX *amx, cell *params)
{
    int hookType = (int)params[1];
    int argCount = (int)(params[0] / sizeof(cell));
    if (argCount < 2)
    {
        AMXX_LOG_ERR("[Ham] RegisterHamFromPlayer: expected at least 2 arguments (function, callback[])");
        return 0;
    }

    cell *funcname_addr;
    amx_GetAddr(amx, params[2], &funcname_addr);
    char funcname[64];
    amx_GetString(funcname, funcname_addr, 0, sizeof(funcname));

    int funcidx;
    if (amx_FindPublic(amx, funcname, &funcidx) != AMX_ERR_NONE)
    {
        AMXX_LOG_ERR("[Ham] RegisterHamFromPlayer: function \"%s\" not found", funcname);
        return 0;
    }

    bool post = argCount >= 3 && params[3] != 0;

    // 原版 AMXX RegisterHamFromPlayer 返回 forward handle (>0)，失败返回 0
    return (cell)AMXXHamSandwich::GetInstance().RegisterHam((HamHookType)hookType, amx, funcidx, post, true);
}

// get_pdata_cbase(ent, offset, base)
cell AMX_NATIVE_CALL amxx_get_pdata_cbase(AMX *amx, cell *params)
{
    int ent = (int)params[1];
    edict_t *pEdict = INDEXENT(ent);
    if (!pEdict || pEdict->free)
        return 0;
    
    return (cell)(intptr_t)pEdict;
}

// get_pdata_ent(entity, offset, linuxdiff=20, macdiff=20)
// 读取私有数据中的 edict 指针并返回其实体索引
cell AMX_NATIVE_CALL amxx_get_pdata_ent(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    int offset = (int)params[2];
    int linuxdiff = (params[0] / sizeof(cell) >= 4) ? (int)params[3] : 20;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;
    int byteOffset = offset * 4;
#ifdef _WIN32
    (void)linuxdiff;
#else
    byteOffset += linuxdiff;
#endif
    edict_t *pStored = *(edict_t **)((char *)pEdict->pvPrivateData + byteOffset);
    if (!pStored || pStored->free)
        return 0;
    return ENTINDEX(pStored);
}

// set_pdata_ent(entity, offset, value, linuxdiff=20, macdiff=20)
cell AMX_NATIVE_CALL amxx_set_pdata_ent(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    int offset = (int)params[2];
    int value = (int)params[3];
    int linuxdiff = (params[0] / sizeof(cell) >= 4) ? (int)params[4] : 20;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;
    int byteOffset = offset * 4;
#ifdef _WIN32
    (void)linuxdiff;
#else
    byteOffset += linuxdiff;
#endif
    edict_t *pStore = (value > 0) ? INDEXENT(value) : nullptr;
    *(edict_t **)((char *)pEdict->pvPrivateData + byteOffset) = pStore;
    return 1;
}

// get_pdata_bool(entity, offset, linuxdiff=20, macdiff=20)
cell AMX_NATIVE_CALL amxx_get_pdata_bool(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    int offset = (int)params[2];
    int linuxdiff = (params[0] / sizeof(cell) >= 4) ? (int)params[3] : 20;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;
    int byteOffset = offset * 4;
#ifdef _WIN32
    (void)linuxdiff;
#else
    byteOffset += linuxdiff;
#endif
    return *(bool *)((char *)pEdict->pvPrivateData + byteOffset) ? 1 : 0;
}

// set_pdata_bool(entity, offset, value, linuxdiff=20, macdiff=20)
cell AMX_NATIVE_CALL amxx_set_pdata_bool(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    int offset = (int)params[2];
    int value = (int)params[3];
    int linuxdiff = (params[0] / sizeof(cell) >= 4) ? (int)params[4] : 20;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;
    int byteOffset = offset * 4;
#ifdef _WIN32
    (void)linuxdiff;
#else
    byteOffset += linuxdiff;
#endif
    *(bool *)((char *)pEdict->pvPrivateData + byteOffset) = (value != 0);
    return 1;
}

// get_pdata_byte(entity, offset, linuxdiff=20, macdiff=20)
cell AMX_NATIVE_CALL amxx_get_pdata_byte(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    int offset = (int)params[2];
    int linuxdiff = (params[0] / sizeof(cell) >= 4) ? (int)params[3] : 20;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;
    int byteOffset = offset * 4;
#ifdef _WIN32
    (void)linuxdiff;
#else
    byteOffset += linuxdiff;
#endif
    return (cell)*(byte *)((char *)pEdict->pvPrivateData + byteOffset);
}

// set_pdata_byte(entity, offset, value, linuxdiff=20, macdiff=20)
cell AMX_NATIVE_CALL amxx_set_pdata_byte(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    int offset = (int)params[2];
    int value = (int)params[3];
    int linuxdiff = (params[0] / sizeof(cell) >= 4) ? (int)params[4] : 20;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;
    int byteOffset = offset * 4;
#ifdef _WIN32
    (void)linuxdiff;
#else
    byteOffset += linuxdiff;
#endif
    *(byte *)((char *)pEdict->pvPrivateData + byteOffset) = (byte)value;
    return 1;
}

// get_pdata_short(entity, offset, linuxdiff=20, macdiff=20)
cell AMX_NATIVE_CALL amxx_get_pdata_short(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    int offset = (int)params[2];
    int linuxdiff = (params[0] / sizeof(cell) >= 4) ? (int)params[3] : 20;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;
    int byteOffset = offset * 4;
#ifdef _WIN32
    (void)linuxdiff;
#else
    byteOffset += linuxdiff;
#endif
    return (cell)*(short *)((char *)pEdict->pvPrivateData + byteOffset);
}

// set_pdata_short(entity, offset, value, linuxdiff=20, macdiff=20)
cell AMX_NATIVE_CALL amxx_set_pdata_short(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    int offset = (int)params[2];
    int value = (int)params[3];
    int linuxdiff = (params[0] / sizeof(cell) >= 4) ? (int)params[4] : 20;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;
    int byteOffset = offset * 4;
#ifdef _WIN32
    (void)linuxdiff;
#else
    byteOffset += linuxdiff;
#endif
    *(short *)((char *)pEdict->pvPrivateData + byteOffset) = (short)value;
    return 1;
}

// get_pdata_vector(entity, offset, Float:output[3], linuxdiff=20, macdiff=20)
cell AMX_NATIVE_CALL amxx_get_pdata_vector(AMX *amx, cell *params)
{
    int index = (int)params[1];
    int offset = (int)params[2];
    int linuxdiff = (params[0] / sizeof(cell) >= 4) ? (int)params[4] : 20;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;
    cell *out;
    amx_GetAddr(amx, params[3], &out);
    if (!out)
        return 0;
    int byteOffset = offset * 4;
#ifdef _WIN32
    (void)linuxdiff;
#else
    byteOffset += linuxdiff;
#endif
    float *vec = (float *)((char *)pEdict->pvPrivateData + byteOffset);
    out[0] = amx_ftoc(vec[0]);
    out[1] = amx_ftoc(vec[1]);
    out[2] = amx_ftoc(vec[2]);
    return 1;
}

// set_pdata_vector(entity, offset, Float:origin[3], linuxdiff=20, macdiff=20)
cell AMX_NATIVE_CALL amxx_set_pdata_vector(AMX *amx, cell *params)
{
    int index = (int)params[1];
    int offset = (int)params[2];
    int linuxdiff = (params[0] / sizeof(cell) >= 4) ? (int)params[4] : 20;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;
    cell *src;
    amx_GetAddr(amx, params[3], &src);
    if (!src)
        return 0;
    int byteOffset = offset * 4;
#ifdef _WIN32
    (void)linuxdiff;
#else
    byteOffset += linuxdiff;
#endif
    float *vec = (float *)((char *)pEdict->pvPrivateData + byteOffset);
    vec[0] = amx_ctof(src[0]);
    vec[1] = amx_ctof(src[1]);
    vec[2] = amx_ctof(src[2]);
    return 1;
}

// get_pdata_ehandle(entity, offset, linuxdiff=20, macdiff=20)
// EHandle 存储实体索引（4 字节整型），返回 ENTINDEX
cell AMX_NATIVE_CALL amxx_get_pdata_ehandle(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    int offset = (int)params[2];
    int linuxdiff = (params[0] / sizeof(cell) >= 4) ? (int)params[3] : 20;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;
    int byteOffset = offset * 4;
#ifdef _WIN32
    (void)linuxdiff;
#else
    byteOffset += linuxdiff;
#endif
    int ehandle = *(int *)((char *)pEdict->pvPrivateData + byteOffset);
    if (ehandle <= 0 || ehandle > gpGlobals->maxEntities)
        return 0;
    edict_t *pTarget = INDEXENT(ehandle);
    if (!pTarget || pTarget->free)
        return 0;
    return ENTINDEX(pTarget);
}

// set_pdata_ehandle(entity, offset, value, linuxdiff=20, macdiff=20)
cell AMX_NATIVE_CALL amxx_set_pdata_ehandle(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    int offset = (int)params[2];
    int value = (int)params[3];
    int linuxdiff = (params[0] / sizeof(cell) >= 4) ? (int)params[4] : 20;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;
    int byteOffset = offset * 4;
#ifdef _WIN32
    (void)linuxdiff;
#else
    byteOffset += linuxdiff;
#endif
    *(int *)((char *)pEdict->pvPrivateData + byteOffset) = value;
    return 1;
}

// pev_serial(entity) — 返回 edict 的 serialnumber
cell AMX_NATIVE_CALL amxx_pev_serial(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    if (index <= 0 || index > gpGlobals->maxEntities)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free)
        return 0;
    return (cell)pEdict->serialnumber;
}

// forward_return(type, any:...) — 存储当前 forward 的返回值（桩实现）
static cell g_forward_retval = 0;
static int g_forward_rettype = 0;
cell AMX_NATIVE_CALL amxx_forward_return(AMX *amx, cell *params)
{
    (void)amx;
    g_forward_rettype = (int)params[1];
    g_forward_retval = params[2];
    return 1;
}

// get_tr(TR_member, [output]) — 从最近一次 trace_* 的全局 TraceResult 读取
// TR_* 枚举与 amxmodx fakemeta_const.inc 对齐：
//   0=AllSolid 1=StartSolid 2=InOpen 3=InWater 4=flFraction
//   5=vecEndPos 6=flPlaneDist 7=vecPlaneNormal 8=pHit 9=iHitgroup
cell AMX_NATIVE_CALL amxx_get_tr(AMX *amx, cell *params)
{
    int field = (int)params[1];
    TraceResult *tr = &g_lastTrace;
    cell *c = nullptr;
    switch (field) {
        case 0: return tr->fAllSolid;
        case 1: return tr->fStartSolid;
        case 2: return tr->fInOpen;
        case 3: return tr->fInWater;
        case 4:
            amx_GetAddr(amx, params[2], &c);
            if (c) *c = amx_ftoc(tr->flFraction);
            return 1;
        case 5:
            amx_GetAddr(amx, params[2], &c);
            if (c) { c[0] = amx_ftoc(tr->vecEndPos.x); c[1] = amx_ftoc(tr->vecEndPos.y); c[2] = amx_ftoc(tr->vecEndPos.z); }
            return 1;
        case 6:
            amx_GetAddr(amx, params[2], &c);
            if (c) *c = amx_ftoc(tr->flPlaneDist);
            return 1;
        case 7:
            amx_GetAddr(amx, params[2], &c);
            if (c) { c[0] = amx_ftoc(tr->vecPlaneNormal.x); c[1] = amx_ftoc(tr->vecPlaneNormal.y); c[2] = amx_ftoc(tr->vecPlaneNormal.z); }
            return 1;
        case 8:
            if (!tr->pHit || FNullEnt(tr->pHit)) return -1;
            return ENTINDEX(tr->pHit);
        case 9: return tr->iHitgroup;
    }
    return 0;
}

// set_tr(TR_member, value) — 写入最近一次 trace_* 的全局 TraceResult
cell AMX_NATIVE_CALL amxx_set_tr(AMX *amx, cell *params)
{
    int field = (int)params[1];
    TraceResult *tr = &g_lastTrace;
    cell *c = nullptr;
    switch (field) {
        case 0: tr->fAllSolid = params[2]; return 1;
        case 1: tr->fStartSolid = params[2]; return 1;
        case 2: tr->fInOpen = params[2]; return 1;
        case 3: tr->fInWater = params[2]; return 1;
        case 4: tr->flFraction = amx_ctof(params[2]); return 1;
        case 5:
            amx_GetAddr(amx, params[2], &c);
            if (!c) return 0;
            tr->vecEndPos.x = amx_ctof(c[0]);
            tr->vecEndPos.y = amx_ctof(c[1]);
            tr->vecEndPos.z = amx_ctof(c[2]);
            return 1;
        case 6: tr->flPlaneDist = amx_ctof(params[2]); return 1;
        case 7:
            amx_GetAddr(amx, params[2], &c);
            if (!c) return 0;
            tr->vecPlaneNormal.x = amx_ctof(c[0]);
            tr->vecPlaneNormal.y = amx_ctof(c[1]);
            tr->vecPlaneNormal.z = amx_ctof(c[2]);
            return 1;
        case 8: {
            int entIdx = (int)params[2];
            tr->pHit = (entIdx > 0) ? INDEXENT(entIdx) : nullptr;
            return 1;
        }
        case 9: tr->iHitgroup = params[2]; return 1;
    }
    return 0;
}

// get_cd(cd_handle, ClientData:member, ...) — 与原版 fm_tr2.cpp get_cd 完全一致
cell AMX_NATIVE_CALL amxx_get_cd(AMX *amx, cell *params)
{
    clientdata_t *cd;
    if (params[1] == 0)
        cd = &g_cd_glb;
    else
        cd = reinterpret_cast<clientdata_t *>(params[1]);

    cell *ptr = nullptr;
    int len;

    switch (params[2])
    {
    case 0: // CD_Origin
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        ptr[0] = amx_ftoc(cd->origin.x);
        ptr[1] = amx_ftoc(cd->origin.y);
        ptr[2] = amx_ftoc(cd->origin.z);
        return 1;
    case 1: // CD_Velocity
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        ptr[0] = amx_ftoc(cd->velocity.x);
        ptr[1] = amx_ftoc(cd->velocity.y);
        ptr[2] = amx_ftoc(cd->velocity.z);
        return 1;
    case 2: // CD_ViewModel
        return cd->viewmodel;
    case 3: // CD_PunchAngle
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        ptr[0] = amx_ftoc(cd->punchangle.x);
        ptr[1] = amx_ftoc(cd->punchangle.y);
        ptr[2] = amx_ftoc(cd->punchangle.z);
        return 1;
    case 4: // CD_Flags
        return cd->flags;
    case 5: // CD_WaterLevel
        return cd->waterlevel;
    case 6: // CD_WaterType
        return cd->watertype;
    case 7: // CD_ViewOfs
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        ptr[0] = amx_ftoc(cd->view_ofs.x);
        ptr[1] = amx_ftoc(cd->view_ofs.y);
        ptr[2] = amx_ftoc(cd->view_ofs.z);
        return 1;
    case 8: // CD_Health
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(cd->health);
        return 1;
    case 9: // CD_bInDuck
        return cd->bInDuck;
    case 10: // CD_Weapons
        return cd->weapons;
    case 11: // CD_flTimeStepSound
        return cd->flTimeStepSound;
    case 12: // CD_flDuckTime
        return cd->flDuckTime;
    case 13: // CD_flSwimTime
        return cd->flSwimTime;
    case 14: // CD_WaterJumpTime
        return cd->waterjumptime;
    case 15: // CD_MaxSpeed
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(cd->maxspeed);
        return 1;
    case 16: // CD_FOV
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(cd->fov);
        return 1;
    case 17: // CD_WeaponAnim
        return cd->weaponanim;
    case 18: // CD_ID
        return cd->m_iId;
    case 19: // CD_AmmoShells
        return cd->ammo_shells;
    case 20: // CD_AmmoNails
        return cd->ammo_nails;
    case 21: // CD_AmmoCells
        return cd->ammo_cells;
    case 22: // CD_AmmoRockets
        return cd->ammo_rockets;
    case 23: // CD_flNextAttack
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(cd->m_flNextAttack);
        return 1;
    case 24: // CD_tfState
        return cd->tfstate;
    case 25: // CD_PushMsec
        return cd->pushmsec;
    case 26: // CD_DeadFlag
        return cd->deadflag;
    case 27: // CD_PhysInfo
        {
            cell *sizestr;
            amx_GetAddr(amx, params[4], &sizestr);
            if (!sizestr) return 0;
            cell *dest;
            amx_GetAddr(amx, params[3], &dest);
            if (!dest) return 0;
            return amx_SetString(dest, cd->physinfo, 0, 0, (int)*sizestr);
        }
    case 28: // CD_iUser1
        return cd->iuser1;
    case 29: // CD_iUser2
        return cd->iuser2;
    case 30: // CD_iUser3
        return cd->iuser3;
    case 31: // CD_iUser4
        return cd->iuser4;
    case 32: // CD_fUser1
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(cd->fuser1);
        return 1;
    case 33: // CD_fUser2
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(cd->fuser2);
        return 1;
    case 34: // CD_fUser3
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(cd->fuser3);
        return 1;
    case 35: // CD_fUser4
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(cd->fuser4);
        return 1;
    case 36: // CD_vUser1
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        ptr[0] = amx_ftoc(cd->vuser1.x);
        ptr[1] = amx_ftoc(cd->vuser1.y);
        ptr[2] = amx_ftoc(cd->vuser1.z);
        return 1;
    case 37: // CD_vUser2
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        ptr[0] = amx_ftoc(cd->vuser2.x);
        ptr[1] = amx_ftoc(cd->vuser2.y);
        ptr[2] = amx_ftoc(cd->vuser2.z);
        return 1;
    case 38: // CD_vUser3
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        ptr[0] = amx_ftoc(cd->vuser3.x);
        ptr[1] = amx_ftoc(cd->vuser3.y);
        ptr[2] = amx_ftoc(cd->vuser3.z);
        return 1;
    case 39: // CD_vUser4
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        ptr[0] = amx_ftoc(cd->vuser4.x);
        ptr[1] = amx_ftoc(cd->vuser4.y);
        ptr[2] = amx_ftoc(cd->vuser4.z);
        return 1;
    }
    (void)len;
    AMXX_LOG("Invalid ClientData member: %d", params[2]);
    return 0;
}

// get_es(es_handle, EntityState:member, ...) — 与原版 fm_tr2.cpp get_es 完全一致
cell AMX_NATIVE_CALL amxx_get_es(AMX *amx, cell *params)
{
    entity_state_t *es;
    if (params[1] == 0)
        es = &g_es_glb;
    else
        es = reinterpret_cast<entity_state_t *>(params[1]);

    cell *ptr = nullptr;

    switch (params[2])
    {
    case 0: // ES_EntityType
        return es->entityType;
    case 1: // ES_Number
        return es->number;
    case 2: // ES_MsgTime
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(es->msg_time);
        return 1;
    case 3: // ES_MessageNum
        return es->messagenum;
    case 4: // ES_Origin
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        ptr[0] = amx_ftoc(es->origin.x);
        ptr[1] = amx_ftoc(es->origin.y);
        ptr[2] = amx_ftoc(es->origin.z);
        return 1;
    case 5: // ES_Angles
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        ptr[0] = amx_ftoc(es->angles.x);
        ptr[1] = amx_ftoc(es->angles.y);
        ptr[2] = amx_ftoc(es->angles.z);
        return 1;
    case 6: // ES_ModelIndex
        return es->modelindex;
    case 7: // ES_Sequence
        return es->sequence;
    case 8: // ES_Frame
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(es->frame);
        return 1;
    case 9: // ES_ColorMap
        return es->colormap;
    case 10: // ES_Skin
        return es->skin;
    case 11: // ES_Solid
        return es->solid;
    case 12: // ES_Effects
        return es->effects;
    case 13: // ES_Scale
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(es->scale);
        return 1;
    case 14: // ES_eFlags
        return es->eflags;
    case 15: // ES_RenderMode
        return es->rendermode;
    case 16: // ES_RenderAmt
        return es->renderamt;
    case 17: // ES_RenderColor (原版 fm_tr2 有 r,b,g 顺序 bug，此处保持一致)
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        ptr[0] = es->rendercolor.r;
        ptr[1] = es->rendercolor.b;
        ptr[2] = es->rendercolor.g;
        return 1;
    case 18: // ES_RenderFx
        return es->renderfx;
    case 19: // ES_MoveType
        return es->movetype;
    case 20: // ES_AnimTime
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(es->animtime);
        return 1;
    case 21: // ES_FrameRate
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(es->framerate);
        return 1;
    case 22: // ES_Body
        return es->body;
    case 23: // ES_Controller
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        ptr[0] = es->controller[0];
        ptr[1] = es->controller[1];
        ptr[2] = es->controller[2];
        ptr[3] = es->controller[3];
        return 1;
    case 24: // ES_Blending
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        ptr[0] = es->blending[0];
        ptr[1] = es->blending[1];
        ptr[2] = es->blending[2];
        ptr[3] = es->blending[3];
        return 1;
    case 25: // ES_Velocity
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        ptr[0] = amx_ftoc(es->velocity.x);
        ptr[1] = amx_ftoc(es->velocity.y);
        ptr[2] = amx_ftoc(es->velocity.z);
        return 1;
    case 26: // ES_Mins
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        ptr[0] = amx_ftoc(es->mins.x);
        ptr[1] = amx_ftoc(es->mins.y);
        ptr[2] = amx_ftoc(es->mins.z);
        return 1;
    case 27: // ES_Maxs
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        ptr[0] = amx_ftoc(es->maxs.x);
        ptr[1] = amx_ftoc(es->maxs.y);
        ptr[2] = amx_ftoc(es->maxs.z);
        return 1;
    case 28: // ES_AimEnt
        return es->aiment;
    case 29: // ES_Owner
        return es->owner;
    case 30: // ES_Friction
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(es->friction);
        return 1;
    case 31: // ES_Gravity
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(es->gravity);
        return 1;
    case 32: // ES_Team
        return es->team;
    case 33: // ES_PlayerClass
        return es->playerclass;
    case 34: // ES_Health
        return es->health;
    case 35: // ES_Spectator
        return es->spectator;
    case 36: // ES_WeaponModel
        return es->weaponmodel;
    case 37: // ES_GaitSequence
        return es->gaitsequence;
    case 38: // ES_BaseVelocity
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        ptr[0] = amx_ftoc(es->basevelocity.x);
        ptr[1] = amx_ftoc(es->basevelocity.y);
        ptr[2] = amx_ftoc(es->basevelocity.z);
        return 1;
    case 39: // ES_UseHull
        return es->usehull;
    case 40: // ES_OldButtons
        return es->oldbuttons;
    case 41: // ES_OnGround
        return es->onground;
    case 42: // ES_iStepLeft
        return es->iStepLeft;
    case 43: // ES_flFallVelocity
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(es->flFallVelocity);
        return 1;
    case 44: // ES_FOV
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(es->fov);
        return 1;
    case 45: // ES_WeaponAnim
        return es->weaponanim;
    case 46: // ES_StartPos
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        ptr[0] = amx_ftoc(es->startpos.x);
        ptr[1] = amx_ftoc(es->startpos.y);
        ptr[2] = amx_ftoc(es->startpos.z);
        return 1;
    case 47: // ES_EndPos
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        ptr[0] = amx_ftoc(es->endpos.x);
        ptr[1] = amx_ftoc(es->endpos.y);
        ptr[2] = amx_ftoc(es->endpos.z);
        return 1;
    case 48: // ES_ImpactTime
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(es->impacttime);
        return 1;
    case 49: // ES_StartTime
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(es->starttime);
        return 1;
    case 50: // ES_iUser1
        return es->iuser1;
    case 51: // ES_iUser2
        return es->iuser2;
    case 52: // ES_iUser3
        return es->iuser3;
    case 53: // ES_iUser4
        return es->iuser4;
    case 54: // ES_fUser1
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(es->fuser1);
        return 1;
    case 55: // ES_fUser2
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(es->fuser2);
        return 1;
    case 56: // ES_fUser3
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(es->fuser3);
        return 1;
    case 57: // ES_fUser4
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(es->fuser4);
        return 1;
    case 58: // ES_vUser1
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        ptr[0] = amx_ftoc(es->vuser1.x);
        ptr[1] = amx_ftoc(es->vuser1.y);
        ptr[2] = amx_ftoc(es->vuser1.z);
        return 1;
    case 59: // ES_vUser2
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        ptr[0] = amx_ftoc(es->vuser2.x);
        ptr[1] = amx_ftoc(es->vuser2.y);
        ptr[2] = amx_ftoc(es->vuser2.z);
        return 1;
    case 60: // ES_vUser3
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        ptr[0] = amx_ftoc(es->vuser3.x);
        ptr[1] = amx_ftoc(es->vuser3.y);
        ptr[2] = amx_ftoc(es->vuser3.z);
        return 1;
    case 61: // ES_vUser4
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        ptr[0] = amx_ftoc(es->vuser4.x);
        ptr[1] = amx_ftoc(es->vuser4.y);
        ptr[2] = amx_ftoc(es->vuser4.z);
        return 1;
    }
    AMXX_LOG("Invalid EntityState member: %d", params[2]);
    return 0;
}

// set_es(es_handle, EntityState:member, ...) — 与原版 fm_tr2.cpp set_es 完全一致
cell AMX_NATIVE_CALL amxx_set_es(AMX *amx, cell *params)
{
    if (*params / sizeof(cell) < 3)
    {
        AMXX_LOG("set_es: No data passed");
        return 0;
    }

    entity_state_t *es;
    if (params[1] == 0)
        es = &g_es_glb;
    else
        es = reinterpret_cast<entity_state_t *>(params[1]);

    cell *ptr = nullptr;
    amx_GetAddr(amx, params[3], &ptr);
    if (!ptr) return 0;

    switch (params[2])
    {
    case 0: es->entityType = *ptr; return 1;
    case 1: es->number = *ptr; return 1;
    case 2: es->msg_time = amx_ctof(*ptr); return 1;
    case 3: es->messagenum = *ptr; return 1;
    case 4:
        es->origin.x = amx_ctof(ptr[0]);
        es->origin.y = amx_ctof(ptr[1]);
        es->origin.z = amx_ctof(ptr[2]);
        return 1;
    case 5:
        es->angles.x = amx_ctof(ptr[0]);
        es->angles.y = amx_ctof(ptr[1]);
        es->angles.z = amx_ctof(ptr[2]);
        return 1;
    case 6: es->modelindex = *ptr; return 1;
    case 7: es->sequence = *ptr; return 1;
    case 8: es->frame = amx_ctof(*ptr); return 1;
    case 9: es->colormap = *ptr; return 1;
    case 10: es->skin = (short)*ptr; return 1;
    case 11: es->solid = (short)*ptr; return 1;
    case 12: es->effects = *ptr; return 1;
    case 13: es->scale = amx_ctof(*ptr); return 1;
    case 14: es->eflags = (byte)*ptr; return 1;
    case 15: es->rendermode = *ptr; return 1;
    case 16: es->renderamt = *ptr; return 1;
    case 17:
        es->rendercolor.r = (byte)ptr[0];
        es->rendercolor.g = (byte)ptr[1];
        es->rendercolor.b = (byte)ptr[2];
        return 1;
    case 18: es->renderfx = *ptr; return 1;
    case 19: es->movetype = *ptr; return 1;
    case 20: es->animtime = amx_ctof(*ptr); return 1;
    case 21: es->framerate = amx_ctof(*ptr); return 1;
    case 22: es->body = *ptr; return 1;
    case 23:
        es->controller[0] = (byte)ptr[0];
        es->controller[1] = (byte)ptr[1];
        es->controller[2] = (byte)ptr[2];
        es->controller[3] = (byte)ptr[3];
        return 1;
    case 24:
        es->blending[0] = (byte)ptr[0];
        es->blending[1] = (byte)ptr[1];
        es->blending[2] = (byte)ptr[2];
        es->blending[3] = (byte)ptr[3];
        return 1;
    case 25:
        es->velocity.x = amx_ctof(ptr[0]);
        es->velocity.y = amx_ctof(ptr[1]);
        es->velocity.z = amx_ctof(ptr[2]);
        return 1;
    case 26:
        es->mins.x = amx_ctof(ptr[0]);
        es->mins.y = amx_ctof(ptr[1]);
        es->mins.z = amx_ctof(ptr[2]);
        return 1;
    case 27:
        es->maxs.x = amx_ctof(ptr[0]);
        es->maxs.y = amx_ctof(ptr[1]);
        es->maxs.z = amx_ctof(ptr[2]);
        return 1;
    case 28: es->aiment = *ptr; return 1;
    case 29: es->owner = *ptr; return 1;
    case 30: es->friction = amx_ctof(*ptr); return 1;
    case 31: es->gravity = amx_ctof(*ptr); return 1;
    case 32: es->team = *ptr; return 1;
    case 33: es->playerclass = *ptr; return 1;
    case 34: es->health = *ptr; return 1;
    case 35: es->spectator = *ptr; return 1;
    case 36: es->weaponmodel = *ptr; return 1;
    case 37: es->gaitsequence = *ptr; return 1;
    case 38:
        es->basevelocity.x = amx_ctof(ptr[0]);
        es->basevelocity.y = amx_ctof(ptr[1]);
        es->basevelocity.z = amx_ctof(ptr[2]);
        return 1;
    case 39: es->usehull = *ptr; return 1;
    case 40: es->oldbuttons = *ptr; return 1;
    case 41: es->onground = *ptr; return 1;
    case 42: es->iStepLeft = *ptr; return 1;
    case 43: es->flFallVelocity = amx_ctof(*ptr); return 1;
    case 44: es->fov = amx_ctof(*ptr); return 1;
    case 45: es->weaponanim = *ptr; return 1;
    case 46:
        es->startpos.x = amx_ctof(ptr[0]);
        es->startpos.y = amx_ctof(ptr[1]);
        es->startpos.z = amx_ctof(ptr[2]);
        return 1;
    case 47:
        es->endpos.x = amx_ctof(ptr[0]);
        es->endpos.y = amx_ctof(ptr[1]);
        es->endpos.z = amx_ctof(ptr[2]);
        return 1;
    case 48: es->impacttime = amx_ctof(*ptr); return 1;
    case 49: es->starttime = amx_ctof(*ptr); return 1;
    case 50: es->iuser1 = *ptr; return 1;
    case 51: es->iuser2 = *ptr; return 1;
    case 52: es->iuser3 = *ptr; return 1;
    case 53: es->iuser4 = *ptr; return 1;
    case 54: es->fuser1 = amx_ctof(*ptr); return 1;
    case 55: es->fuser2 = amx_ctof(*ptr); return 1;
    case 56: es->fuser3 = amx_ctof(*ptr); return 1;
    case 57: es->fuser4 = amx_ctof(*ptr); return 1;
    case 58:
        es->vuser1.x = amx_ctof(ptr[0]);
        es->vuser1.y = amx_ctof(ptr[1]);
        es->vuser1.z = amx_ctof(ptr[2]);
        return 1;
    case 59:
        es->vuser2.x = amx_ctof(ptr[0]);
        es->vuser2.y = amx_ctof(ptr[1]);
        es->vuser2.z = amx_ctof(ptr[2]);
        return 1;
    case 60:
        es->vuser3.x = amx_ctof(ptr[0]);
        es->vuser3.y = amx_ctof(ptr[1]);
        es->vuser3.z = amx_ctof(ptr[2]);
        return 1;
    case 61:
        es->vuser4.x = amx_ctof(ptr[0]);
        es->vuser4.y = amx_ctof(ptr[1]);
        es->vuser4.z = amx_ctof(ptr[2]);
        return 1;
    }
    AMXX_LOG("Invalid EntityState member: %d", params[2]);
    return 0;
}

// get_uc(uc_handle, UserCmd:member, ...) — 与原版 fm_tr2.cpp get_uc 完全一致
cell AMX_NATIVE_CALL amxx_get_uc(AMX *amx, cell *params)
{
    usercmd_t *uc;
    if (params[1] == 0)
        uc = &g_uc_glb;
    else
        uc = reinterpret_cast<usercmd_t *>(params[1]);

    cell *ptr = nullptr;

    switch (params[2])
    {
    case 0: // UC_LerpMsec
        return uc->lerp_msec;
    case 1: // UC_Msec
        return uc->msec;
    case 2: // UC_ViewAngles
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        ptr[0] = amx_ftoc(uc->viewangles.x);
        ptr[1] = amx_ftoc(uc->viewangles.y);
        ptr[2] = amx_ftoc(uc->viewangles.z);
        return 1;
    case 3: // UC_ForwardMove
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(uc->forwardmove);
        return 1;
    case 4: // UC_SideMove
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(uc->sidemove);
        return 1;
    case 5: // UC_UpMove
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        *ptr = amx_ftoc(uc->upmove);
        return 1;
    case 6: // UC_LightLevel
        return uc->lightlevel;
    case 7: // UC_Buttons
        return uc->buttons;
    case 8: // UC_Impulse
        return uc->impulse;
    case 9: // UC_WeaponSelect
        return uc->weaponselect;
    case 10: // UC_ImpactIndex
        return uc->impact_index;
    case 11: // UC_ImpactPosition
        amx_GetAddr(amx, params[3], &ptr);
        if (!ptr) return 0;
        ptr[0] = amx_ftoc(uc->impact_position.x);
        ptr[1] = amx_ftoc(uc->impact_position.y);
        ptr[2] = amx_ftoc(uc->impact_position.z);
        return 1;
    }
    AMXX_LOG("Invalid UserCmd member: %d", params[2]);
    return 0;
}

// set_uc(uc_handle, UserCmd:member, ...) — 与原版 fm_tr2.cpp set_uc 完全一致
cell AMX_NATIVE_CALL amxx_set_uc(AMX *amx, cell *params)
{
    if (*params / sizeof(cell) < 3)
    {
        AMXX_LOG("set_uc: No data passed");
        return 0;
    }

    usercmd_t *uc;
    if (params[1] == 0)
        uc = &g_uc_glb;
    else
        uc = reinterpret_cast<usercmd_t *>(params[1]);

    cell *ptr = nullptr;
    amx_GetAddr(amx, params[3], &ptr);
    if (!ptr) return 0;

    switch (params[2])
    {
    case 0: uc->lerp_msec = (short)*ptr; return 1;
    case 1: uc->msec = (byte)*ptr; return 1;
    case 2:
        uc->viewangles.x = amx_ctof(ptr[0]);
        uc->viewangles.y = amx_ctof(ptr[1]);
        uc->viewangles.z = amx_ctof(ptr[2]);
        return 1;
    case 3: uc->forwardmove = amx_ctof(*ptr); return 1;
    case 4: uc->sidemove = amx_ctof(*ptr); return 1;
    case 5: uc->upmove = amx_ctof(*ptr); return 1;
    case 6: uc->lightlevel = (byte)*ptr; return 1;
    case 7: uc->buttons = (unsigned short)*ptr; return 1;
    case 8: uc->impulse = (byte)*ptr; return 1;
    case 9: uc->weaponselect = (byte)*ptr; return 1;
    case 10: uc->impact_index = *ptr; return 1;
    case 11:
        uc->impact_position.x = amx_ctof(ptr[0]);
        uc->impact_position.y = amx_ctof(ptr[1]);
        uc->impact_position.z = amx_ctof(ptr[2]);
        return 1;
    }
    AMXX_LOG("Invalid UserCmd member: %d", params[2]);
    return 0;
}

// copy_infokey_buffer(infoBuffer, out[], maxlen)
// infoBuffer 视为实体索引，复制该实体 InfoKeyBuffer 内容到 out
cell AMX_NATIVE_CALL amxx_copy_infokey_buffer(AMX *amx, cell *params)
{
    int id = (int)params[1];
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    int maxlen = (int)params[3];
    const char *info = "";
    if (id >= 1 && id <= gpGlobals->maxClients) {
        edict_t *e = INDEXENT(id);
        if (e && g_engfuncs.pfnGetInfoKeyBuffer)
            info = (const char *)g_engfuncs.pfnGetInfoKeyBuffer(e);
    }
    return amx_SetString(dest, info ? info : "", 0, 0, maxlen);
}

// set_controller(entity, controller, Float:value)
// 简化实现：写 controller[controller] = byte(value * 255 / 360)，返回归一化值
cell AMX_NATIVE_CALL amxx_set_controller(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    int controller = (int)params[2];
    float value = amx_ctof(params[3]);
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free)
        return 0;
    if (controller < 0 || controller > 3)
        return 0;
    byte newVal = (byte)(value * 255.0f / 360.0f);
    pEdict->v.controller[controller] = newVal;
    return amx_ftoc((float)newVal);
}

// GetModelBoundingBox(entity, Float:mins[3], Float:maxs[3], sequence=0)
// 简化实现：读取 entvars 的 mins/maxs 作为回退
cell AMX_NATIVE_CALL amxx_GetModelBoundingBox(AMX *amx, cell *params)
{
    int index = (int)params[1];
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free)
        return 0;
    cell *mins, *maxs;
    amx_GetAddr(amx, params[2], &mins);
    amx_GetAddr(amx, params[3], &maxs);
    if (mins) {
        mins[0] = amx_ftoc(pEdict->v.mins.x);
        mins[1] = amx_ftoc(pEdict->v.mins.y);
        mins[2] = amx_ftoc(pEdict->v.mins.z);
    }
    if (maxs) {
        maxs[0] = amx_ftoc(pEdict->v.maxs.x);
        maxs[1] = amx_ftoc(pEdict->v.maxs.y);
        maxs[2] = amx_ftoc(pEdict->v.maxs.z);
    }
    return 1;
}

// trace_forward(Float:start[3], Float:angle[3], Float:give, ignoreEnt,
//               &Float:hitX, &Float:hitY, &Float:shortestDistance,
//               &Float:shortestDistLow, &Float:shortestDistHigh)
// 沿角度方向以 give 距离做 TRACE_LINE，回填命中信息
cell AMX_NATIVE_CALL amxx_trace_forward(AMX *amx, cell *params)
{
    cell *s, *a;
    amx_GetAddr(amx, params[1], &s);
    amx_GetAddr(amx, params[2], &a);
    if (!s || !a)
        return 0;
    Vector start(amx_ctof(s[0]), amx_ctof(s[1]), amx_ctof(s[2]));
    Vector angle(amx_ctof(a[0]), amx_ctof(a[1]), amx_ctof(a[2]));
    float give = amx_ctof(params[3]);
    int ignoreEnt = (int)params[4];

    float sp = sinf(angle.x * (float)(M_PI / 180.0));
    float cp = cosf(angle.x * (float)(M_PI / 180.0));
    float sy = sinf(angle.y * (float)(M_PI / 180.0));
    float cy = cosf(angle.y * (float)(M_PI / 180.0));
    Vector forward(cp * cy, cp * sy, -sp);
    Vector end = start + forward * give;

    TraceResult tr;
    TRACE_LINE(start, end, dont_ignore_monsters, ignoreEnt > 0 ? INDEXENT(ignoreEnt) : nullptr, &tr);
    g_lastTrace = tr;

    cell *hitX, *hitY, *shortestDist, *shortestDistLow, *shortestDistHigh;
    amx_GetAddr(amx, params[5], &hitX);
    amx_GetAddr(amx, params[6], &hitY);
    amx_GetAddr(amx, params[7], &shortestDist);
    amx_GetAddr(amx, params[8], &shortestDistLow);
    amx_GetAddr(amx, params[9], &shortestDistHigh);
    if (hitX) *hitX = amx_ftoc(tr.vecEndPos.x);
    if (hitY) *hitY = amx_ftoc(tr.vecEndPos.y);
    float dist = (tr.vecEndPos - start).Length();
    if (shortestDist) *shortestDist = amx_ftoc(dist);
    if (shortestDistLow) *shortestDistLow = amx_ftoc(tr.vecEndPos.z);
    float highZ = start.z + give;
    if (shortestDistHigh) *shortestDistHigh = amx_ftoc(highZ);
    return 1;
}

// lookup_sequence(entity, const name[], &Float:framerate=0.0, &bool:loops=false, &Float:groundspeed=0.0)
// 与原版 fakemeta misc.cpp lookup_sequence 完全一致
cell AMX_NATIVE_CALL amxx_lookup_sequence(AMX *amx, cell *params)
{
    int index = (int)params[1];
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free)
        return -1;

    studiohdr_t *pstudiohdr = (studiohdr_t *)GET_MODEL_PTR(pEdict);
    if (!pstudiohdr)
    {
        AMXX_LOG("lookup_sequence: Could not retrieve the model pointer from entity %d", index);
        return -1;
    }

    mstudioseqdesc_t *pseqdesc = reinterpret_cast<mstudioseqdesc_t *>(
        reinterpret_cast<char *>(pstudiohdr) + pstudiohdr->seqindex);

    char label[256];
    {
        cell *addr;
        amx_GetAddr(amx, params[2], &addr);
        if (!addr) return -1;
        amx_GetString(label, addr, 0, sizeof(label));
    }

    int numParams = (int)(*params / sizeof(cell));
    for (int i = 0; i < pstudiohdr->numseq; i++)
    {
        if (stricmp(pseqdesc[i].label, label) == 0)
        {
            // 与 HLSDK animating.cpp 一致的计算方式
            mstudioseqdesc_t *pseq = &pseqdesc[i];

            if (numParams >= 3)
            {
                cell *framerate;
                amx_GetAddr(amx, params[3], &framerate);
                if (framerate)
                {
                    float fps = 256.0f * pseq->fps / (pseq->numframes - 1);
                    *framerate = amx_ftoc(fps);
                }
            }
            if (numParams >= 4)
            {
                cell *loops;
                amx_GetAddr(amx, params[4], &loops);
                if (loops)
                    *loops = (pseq->flags & STUDIO_LOOPING) ? 1 : 0;
            }
            if (numParams >= 5)
            {
                cell *groundspeed;
                amx_GetAddr(amx, params[5], &groundspeed);
                if (groundspeed)
                {
                    float gs = sqrt(pseq->linearmovement[0] * pseq->linearmovement[0]
                                  + pseq->linearmovement[1] * pseq->linearmovement[1]
                                  + pseq->linearmovement[2] * pseq->linearmovement[2]);
                    gs = gs * pseq->fps / (pseq->numframes - 1);
                    *groundspeed = amx_ftoc(gs);
                }
            }
            return i;
        }
    }
    return -1;
}

// SetHamParamFloat(param, Float:value) — 修改当前 Ham 回调的第 param 个参数（float 类型）
cell AMX_NATIVE_CALL amxx_SetHamParamFloat(AMX *amx, cell *params)
{
    (void)amx;
    if (!g_currentHamCtx.active)
        return 0;
    int param = (int)params[1];
    if (param < 1 || param > g_currentHamCtx.paramCount)
        return 0;
    auto &slot = g_currentHamCtx.params[param - 1];
    if (slot.type != HamParamSlot::PT_FLOAT || !slot.ptr)
        return 0;
    *(float *)slot.ptr = amx_ctof(params[2]);
    return 1;
}

// SetHamParamInt(param, value) — 修改当前 Ham 回调的第 param 个参数（int 类型）
cell AMX_NATIVE_CALL amxx_SetHamParamInt(AMX *amx, cell *params)
{
    (void)amx;
    if (!g_currentHamCtx.active)
        return 0;
    int param = (int)params[1];
    if (param < 1 || param > g_currentHamCtx.paramCount)
        return 0;
    auto &slot = g_currentHamCtx.params[param - 1];
    if (slot.type != HamParamSlot::PT_INT || !slot.ptr)
        return 0;
    *(int *)slot.ptr = (int)params[2];
    return 1;
}

// SetHamParamEntity(param, entity) — 修改当前 Ham 回调的第 param 个参数（entity index 类型）
cell AMX_NATIVE_CALL amxx_SetHamParamEntity(AMX *amx, cell *params)
{
    (void)amx;
    if (!g_currentHamCtx.active)
        return 0;
    int param = (int)params[1];
    if (param < 1 || param > g_currentHamCtx.paramCount)
        return 0;
    auto &slot = g_currentHamCtx.params[param - 1];
    if (slot.type != HamParamSlot::PT_ENTITY || !slot.ptr)
        return 0;
    *(int *)slot.ptr = (int)params[2];
    return 1;
}

// SetHamParamVector(param, Float:value[3]) — 修改当前 Ham 回调的第 param 个参数（vector 类型）
cell AMX_NATIVE_CALL amxx_SetHamParamVector(AMX *amx, cell *params)
{
    (void)amx;
    if (!g_currentHamCtx.active)
        return 0;
    int param = (int)params[1];
    if (param < 1 || param > g_currentHamCtx.paramCount)
        return 0;
    auto &slot = g_currentHamCtx.params[param - 1];
    if (slot.type != HamParamSlot::PT_VECTOR || !slot.ptr)
        return 0;
    cell *vec;
    amx_GetAddr(amx, params[2], &vec);
    if (!vec) return 0;
    Vector *v = (Vector *)slot.ptr;
    v->x = amx_ctof(vec[0]);
    v->y = amx_ctof(vec[1]);
    v->z = amx_ctof(vec[2]);
    return 1;
}

// SetHamParamString(param, const value[]) — 修改当前 Ham 回调的第 param 个参数（string 类型）
// 注意：字符串参数通过 const char** 间接修改，新字符串必须由调用方保证生命周期
cell AMX_NATIVE_CALL amxx_SetHamParamString(AMX *amx, cell *params)
{
    (void)amx;
    if (!g_currentHamCtx.active)
        return 0;
    int param = (int)params[1];
    if (param < 1 || param > g_currentHamCtx.paramCount)
        return 0;
    auto &slot = g_currentHamCtx.params[param - 1];
    if (slot.type != HamParamSlot::PT_STRING || !slot.ptr)
        return 0;
    // 暂存到静态缓冲区（单次回调内有效）
    static char s_hamStringBuf[256];
    cell *src;
    amx_GetAddr(amx, params[2], &src);
    if (!src) return 0;
    amx_GetString(s_hamStringBuf, src, 0, sizeof(s_hamStringBuf));
    *(const char **)slot.ptr = s_hamStringBuf;
    return 1;
}

// set_cd(cd_handle, ClientData:member, ...) — 与原版 fm_tr2.cpp set_cd 完全一致
cell AMX_NATIVE_CALL amxx_set_cd(AMX *amx, cell *params)
{
    if (*params / sizeof(cell) < 3)
    {
        AMXX_LOG("set_cd: No data passed");
        return 0;
    }

    clientdata_t *cd;
    if (params[1] == 0)
        cd = &g_cd_glb;
    else
        cd = reinterpret_cast<clientdata_t *>(params[1]);

    cell *ptr = nullptr;
    amx_GetAddr(amx, params[3], &ptr);
    if (!ptr) return 0;

    switch (params[2])
    {
    case 0: // CD_Origin
        cd->origin.x = amx_ctof(ptr[0]);
        cd->origin.y = amx_ctof(ptr[1]);
        cd->origin.z = amx_ctof(ptr[2]);
        return 1;
    case 1: // CD_Velocity
        cd->velocity.x = amx_ctof(ptr[0]);
        cd->velocity.y = amx_ctof(ptr[1]);
        cd->velocity.z = amx_ctof(ptr[2]);
        return 1;
    case 2: // CD_ViewModel
        cd->viewmodel = *ptr;
        return 1;
    case 3: // CD_PunchAngle
        cd->punchangle.x = amx_ctof(ptr[0]);
        cd->punchangle.y = amx_ctof(ptr[1]);
        cd->punchangle.z = amx_ctof(ptr[2]);
        return 1;
    case 4: cd->flags = *ptr; return 1;
    case 5: cd->waterlevel = *ptr; return 1;
    case 6: cd->watertype = *ptr; return 1;
    case 7: // CD_ViewOfs
        cd->view_ofs.x = amx_ctof(ptr[0]);
        cd->view_ofs.y = amx_ctof(ptr[1]);
        cd->view_ofs.z = amx_ctof(ptr[2]);
        return 1;
    case 8: cd->health = amx_ctof(*ptr); return 1;
    case 9: cd->bInDuck = *ptr; return 1;
    case 10: cd->weapons = *ptr; return 1;
    case 11: cd->flTimeStepSound = *ptr; return 1;
    case 12: cd->flDuckTime = *ptr; return 1;
    case 13: cd->flSwimTime = *ptr; return 1;
    case 14: cd->waterjumptime = *ptr; return 1;
    case 15: cd->maxspeed = amx_ctof(*ptr); return 1;
    case 16: cd->fov = amx_ctof(*ptr); return 1;
    case 17: cd->weaponanim = *ptr; return 1;
    case 18: cd->m_iId = *ptr; return 1;
    case 19: cd->ammo_shells = *ptr; return 1;
    case 20: cd->ammo_nails = *ptr; return 1;
    case 21: cd->ammo_cells = *ptr; return 1;
    case 22: cd->ammo_rockets = *ptr; return 1;
    case 23: cd->m_flNextAttack = amx_ctof(*ptr); return 1;
    case 24: cd->tfstate = *ptr; return 1;
    case 25: cd->pushmsec = *ptr; return 1;
    case 26: cd->deadflag = *ptr; return 1;
    case 27: // CD_PhysInfo
        {
            char phys[MAX_PHYSINFO_STRING];
            amx_GetString(phys, ptr, 0, sizeof(phys));
            strncpy(cd->physinfo, phys, MAX_PHYSINFO_STRING - 1);
            cd->physinfo[MAX_PHYSINFO_STRING - 1] = 0;
            return 1;
        }
    case 28: cd->iuser1 = *ptr; return 1;
    case 29: cd->iuser2 = *ptr; return 1;
    case 30: cd->iuser3 = *ptr; return 1;
    case 31: cd->iuser4 = *ptr; return 1;
    case 32: cd->fuser1 = amx_ctof(*ptr); return 1;
    case 33: cd->fuser2 = amx_ctof(*ptr); return 1;
    case 34: cd->fuser3 = amx_ctof(*ptr); return 1;
    case 35: cd->fuser4 = amx_ctof(*ptr); return 1;
    case 36:
        cd->vuser1.x = amx_ctof(ptr[0]);
        cd->vuser1.y = amx_ctof(ptr[1]);
        cd->vuser1.z = amx_ctof(ptr[2]);
        return 1;
    case 37:
        cd->vuser2.x = amx_ctof(ptr[0]);
        cd->vuser2.y = amx_ctof(ptr[1]);
        cd->vuser2.z = amx_ctof(ptr[2]);
        return 1;
    case 38:
        cd->vuser3.x = amx_ctof(ptr[0]);
        cd->vuser3.y = amx_ctof(ptr[1]);
        cd->vuser3.z = amx_ctof(ptr[2]);
        return 1;
    case 39:
        cd->vuser4.x = amx_ctof(ptr[0]);
        cd->vuser4.y = amx_ctof(ptr[1]);
        cd->vuser4.z = amx_ctof(ptr[2]);
        return 1;
    }
    AMXX_LOG("Invalid ClientData member: %d", params[2]);
    return 0;
}

// EnableHamForward(forward)
cell AMX_NATIVE_CALL amxx_EnableHamForward(AMX *amx, cell *params)
{
    int forwardId = (int)params[1];
    if (!AMXXHamSandwich::GetInstance().EnableForward(forwardId))
    {
        // 原版行为: 无效句柄记错误并返回 -1
        AMXX_LOG_ERR("[Ham] EnableHamForward: Invalid HamHook handle %d", forwardId);
        return -1;
    }
    return 0;
}

// DisableHamForward(forward)
cell AMX_NATIVE_CALL amxx_DisableHamForward(AMX *amx, cell *params)
{
    int forwardId = (int)params[1];
    if (!AMXXHamSandwich::GetInstance().DisableForward(forwardId))
    {
        // 原版行为: 无效句柄记错误并返回 -1
        AMXX_LOG_ERR("[Ham] DisableHamForward: Invalid HamHook handle %d", forwardId);
        return -1;
    }
    return 0;
}

// IsHamValid(Ham:function) — 校验 Ham 函数编号（非 forward 句柄）
cell AMX_NATIVE_CALL amxx_IsHamValid(AMX *amx, cell *params)
{
    (void)amx;
    int func = (int)params[1];
    return AMXXHamSandwich::GetInstance().IsHamFunctionValid(func) ? 1 : 0;
}

// register_forwardex(type[], funcname[]) - 注册带参数的扩展 forward
cell AMX_NATIVE_CALL amxx_register_forwardex(AMX *amx, cell *params)
{
    cell *type_addr, *funcname_addr;
    amx_GetAddr(amx, params[1], &type_addr);
    amx_GetAddr(amx, params[2], &funcname_addr);
    char type[64], funcname[256];
    amx_GetString(type, type_addr, 0, sizeof(type));
    amx_GetString(funcname, funcname_addr, 0, sizeof(funcname));

    int funcidx;
    if (amx_FindPublic(amx, funcname, &funcidx) != AMX_ERR_NONE)
        return 0;

    // Map extended forward type name to ForwardHook type
    int forwardType = -1;
    if (strcmp(type, "Spawn") == 0) forwardType = FM_Spawn;
    else if (strcmp(type, "Think") == 0) forwardType = FM_Think;
    else if (strcmp(type, "Touch") == 0) forwardType = FM_Touch;
    else if (strcmp(type, "KeyValue") == 0) forwardType = FM_KeyValue;
    else if (strcmp(type, "PlayerPreThink") == 0) forwardType = FM_PlayerPreThink;
    else if (strcmp(type, "PlayerPostThink") == 0) forwardType = FM_PlayerPostThink;
    else if (strcmp(type, "TakeDamage") == 0) forwardType = FM_TakeDamage;
    else if (strcmp(type, "TraceAttack") == 0) forwardType = FM_TraceAttack;
    else if (strcmp(type, "SetModel") == 0) forwardType = FM_SetModel;
    else if (strcmp(type, "EmitSound") == 0) forwardType = FM_EmitSound;

    if (forwardType < 0)
        return 0;

    ForwardHook hook;
    hook.type = forwardType;
    hook.amx = amx;
    hook.funcidx = funcidx;
    g_forwardHooks.push_back(hook);
    return 1;
}

// unregister_forwardex(type[], funcname[]) - 注销扩展 forward
cell AMX_NATIVE_CALL amxx_unregister_forwardex(AMX *amx, cell *params)
{
    cell *type_addr, *funcname_addr;
    amx_GetAddr(amx, params[1], &type_addr);
    amx_GetAddr(amx, params[2], &funcname_addr);
    char type[64], funcname[256];
    amx_GetString(type, type_addr, 0, sizeof(type));
    amx_GetString(funcname, funcname_addr, 0, sizeof(funcname));

    int funcidx;
    if (amx_FindPublic(amx, funcname, &funcidx) != AMX_ERR_NONE)
        return 0;

    int forwardType = -1;
    if (strcmp(type, "Spawn") == 0) forwardType = FM_Spawn;
    else if (strcmp(type, "Think") == 0) forwardType = FM_Think;
    else if (strcmp(type, "Touch") == 0) forwardType = FM_Touch;
    else if (strcmp(type, "KeyValue") == 0) forwardType = FM_KeyValue;
    else if (strcmp(type, "PlayerPreThink") == 0) forwardType = FM_PlayerPreThink;
    else if (strcmp(type, "PlayerPostThink") == 0) forwardType = FM_PlayerPostThink;
    else if (strcmp(type, "TakeDamage") == 0) forwardType = FM_TakeDamage;
    else if (strcmp(type, "TraceAttack") == 0) forwardType = FM_TraceAttack;
    else if (strcmp(type, "SetModel") == 0) forwardType = FM_SetModel;
    else if (strcmp(type, "EmitSound") == 0) forwardType = FM_EmitSound;

    if (forwardType < 0)
        return 0;

    for (size_t fi = 0; fi < g_forwardHooks.size(); fi++) {
        if (g_forwardHooks[fi].type == forwardType &&
            g_forwardHooks[fi].amx == amx &&
            g_forwardHooks[fi].funcidx == funcidx) {
            g_forwardHooks.erase(g_forwardHooks.begin() + fi);
            return 1;
        }
    }
    return 0;
}

// ===== P1: pdata 私有数据读写 =====
// 原版 pdata.cpp 约定：offset 以 int(4 字节) 为单位，读写前先乘 4 得到字节偏移；
// 第 3 参（byte）在 Windows 下被原版忽略（仅 Linux/Apple 基址补偿用），不做读取宽度。
// get_pdata_int(entity, offset, linuxdiff=5)
cell AMX_NATIVE_CALL amxx_get_pdata_int(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    int offset = (int)params[2];
    int linuxdiff = (params[0] / sizeof(cell) >= 4) ? (int)params[3] : 5;

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;

    int byteOffset = offset * 4;
#ifdef _WIN32
    (void)linuxdiff;
#else
    byteOffset += linuxdiff;
#endif
    return *(cell *)((char *)pEdict->pvPrivateData + byteOffset);
}

// set_pdata_int(entity, offset, value, linuxdiff=5)
cell AMX_NATIVE_CALL amxx_set_pdata_int(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    int offset = (int)params[2];
    cell value = params[3];
    int linuxdiff = (params[0] / sizeof(cell) >= 4) ? (int)params[4] : 5;

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;

    int byteOffset = offset * 4;
#ifdef _WIN32
    (void)linuxdiff;
#else
    byteOffset += linuxdiff;
#endif
    *(cell *)((char *)pEdict->pvPrivateData + byteOffset) = value;
    return 1;
}

// get_pdata_float(entity, offset, linuxdiff=5)
cell AMX_NATIVE_CALL amxx_get_pdata_float(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    int offset = (int)params[2];
    int linuxdiff = (params[0] / sizeof(cell) >= 4) ? (int)params[3] : 5;

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;

    int byteOffset = offset * 4;
#ifdef _WIN32
    (void)linuxdiff;
#else
    byteOffset += linuxdiff;
#endif
    float val = *(float *)((char *)pEdict->pvPrivateData + byteOffset);
    return amx_ftoc(val);
}

// set_pdata_float(entity, offset, value, linuxdiff=5)
cell AMX_NATIVE_CALL amxx_set_pdata_float(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    int offset = (int)params[2];
    int linuxdiff = (params[0] / sizeof(cell) >= 4) ? (int)params[4] : 5;

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;

    int byteOffset = offset * 4;
#ifdef _WIN32
    (void)linuxdiff;
#else
    byteOffset += linuxdiff;
#endif
    *(float *)((char *)pEdict->pvPrivateData + byteOffset) = amx_ctof(params[3]);
    return 1;
}

// get_pdata_string(entity, offset, dest[], maxlength, byref=1, linux, mac)
// (fakemeta.inc: params[3]=dest, params[4]=maxlength)
cell AMX_NATIVE_CALL amxx_get_pdata_string(AMX *amx, cell *params)
{
    int index = (int)params[1];
    int offset = (int)params[2];

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;

    cell *dest;
    amx_GetAddr(amx, params[3], &dest);
    int byteOffset = offset * 4;
    const char *str = (const char *)((char *)pEdict->pvPrivateData + byteOffset);
    if (!str) str = "";
    return amx_SetString(dest, str, 0, 0, (int)params[4]);
}

// set_pdata_string(entity, offset, source[], realloc=2, linux, mac)
// (fakemeta.inc: params[3]=source; 移植版简化实现, params[4] 作为拷贝长度上限, 不做 realloc 语义)
cell AMX_NATIVE_CALL amxx_set_pdata_string(AMX *amx, cell *params)
{
    int index = (int)params[1];
    int offset = (int)params[2];

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;

    cell *src;
    amx_GetAddr(amx, params[3], &src);
    char value[1024];
    amx_GetString(value, src, 0, sizeof(value));

    int byteOffset = offset * 4;
    strncpy((char *)pEdict->pvPrivateData + byteOffset, value, (int)params[4]);
    return 1;
}

// ===== P1: 寮曟搸 Forward 娉ㄥ唽绯荤粺 =====

cell AMX_NATIVE_CALL amxx_register_forward(AMX *amx, cell *params)
{
    int forwardType = (int)params[1];

    cell *funcname_addr;
    amx_GetAddr(amx, params[2], &funcname_addr);
    char funcname[256];
    amx_GetString(funcname, funcname_addr, 0, sizeof(funcname));

    int funcidx;
    if (amx_FindPublic(amx, funcname, &funcidx) != AMX_ERR_NONE)
        return 0;

    if (forwardType < 0 || forwardType >= FM_MAX)
        return 0;

    // 妫€鏌ユ槸鍚﹀凡娉ㄥ唽
    for (size_t fi = 0; fi < g_forwardHooks.size(); fi++) {
        if (g_forwardHooks[fi].type == forwardType && g_forwardHooks[fi].amx == amx && g_forwardHooks[fi].funcidx == funcidx)
            return 1;
    }

    {
        ForwardHook hook;
        hook.type = forwardType;
        hook.amx = amx;
        hook.funcidx = funcidx;
        g_forwardHooks.push_back(hook);
    }

    {
        char amxx_log_buf[1024];
        snprintf(amxx_log_buf, sizeof(amxx_log_buf), "[AMXX] [Forward] Registered forward type=%d funcidx=%d", forwardType, funcidx);
        printf("%s\n", amxx_log_buf);
        SERVER_PRINT(amxx_log_buf);
        SERVER_PRINT("\n");
    }
    return 1;
}

cell AMX_NATIVE_CALL amxx_unregister_forward(AMX *amx, cell *params)
{
    int forwardType = (int)params[1];

    cell *funcname_addr;
    amx_GetAddr(amx, params[2], &funcname_addr);
    char funcname[256];
    amx_GetString(funcname, funcname_addr, 0, sizeof(funcname));

    int funcidx;
    if (amx_FindPublic(amx, funcname, &funcidx) != AMX_ERR_NONE)
        return 0;

    for (size_t fi = 0; fi < g_forwardHooks.size(); fi++) {
        if (g_forwardHooks[fi].type == forwardType && g_forwardHooks[fi].amx == amx && g_forwardHooks[fi].funcidx == funcidx) {
            g_forwardHooks.erase(g_forwardHooks.begin() + fi);
            return 1;
        }
    }
    return 0;
}

// 渚涘紩鎿庤皟鐢ㄧ殑 Forward 鎵ц鎺ュ彛
void ExecuteForwardHooks(int type, cell *params, int numParams)
{
    for (size_t fi = 0; fi < g_forwardHooks.size(); fi++) {
        if (g_forwardHooks[fi].type == type) {
            cell retval;
            amx_Exec(g_forwardHooks[fi].amx, &retval, g_forwardHooks[fi].funcidx);
        }
    }
}

// ===== P2: 瀹炰綋鏌ユ壘鎵╁睍 =====

cell AMX_NATIVE_CALL amxx_find_ent_by_class(AMX *amx, cell *params)
{
    int startEnt = (int)params[1];

    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    char classname[256];
    amx_GetString(classname, addr, 0, sizeof(classname));

    edict_t *pStart = (startEnt >= 0) ? INDEXENT(startEnt) : nullptr;
    edict_t *pEdict = FIND_ENTITY_BY_STRING(pStart, "classname", classname);
    return pEdict ? ENTINDEX(pEdict) : 0;
}

cell AMX_NATIVE_CALL amxx_find_ent_by_model(AMX *amx, cell *params)
{
    // 原版签名: find_ent_by_model(start, classname[], model[])
    int iStart = params[1];

    cell *class_addr;
    amx_GetAddr(amx, params[2], &class_addr);
    char classname[256];
    amx_GetString(classname, class_addr, 0, sizeof(classname));

    cell *model_addr;
    amx_GetAddr(amx, params[3], &model_addr);
    char model[256];
    amx_GetString(model, model_addr, 0, sizeof(model));

    edict_t *pEdict = (iStart >= 0) ? INDEXENT(iStart) : nullptr;

    if (classname[0]) {
        while (!FNullEnt(pEdict = FIND_ENTITY_BY_CLASSNAME(pEdict, classname))) {
            if (pEdict->v.model > 0 && !strcmp(STRING(pEdict->v.model), model))
                return ENTINDEX(pEdict);
        }
    } else {
        // classname 为空串则匹配所有实体
        for (int i = (iStart >= 0 ? iStart + 1 : 0); i <= gpGlobals->maxEntities; i++) {
            edict_t *pEnt = INDEXENT(i);
            if (!pEnt || pEnt->free)
                continue;
            if (pEnt->v.model > 0 && !strcmp(STRING(pEnt->v.model), model))
                return i;
        }
    }
    return 0;
}

cell AMX_NATIVE_CALL amxx_find_ent_in_sphere(AMX *amx, cell *params)
{
    int startEnt = (int)params[1];

    Vector pos;
    pos.x = amx_ctof(params[2]);
    pos.y = amx_ctof(params[3]);
    pos.z = amx_ctof(params[4]);

    float radius = amx_ctof(params[5]);

    edict_t *pStart = (startEnt >= 0) ? INDEXENT(startEnt) : nullptr;
    edict_t *pEnt = FIND_ENTITY_IN_SPHERE(pStart, pos, radius);
    return pEnt ? ENTINDEX(pEnt) : 0;
}

// ===== P3: trace 鎵╁睍 =====

cell AMX_NATIVE_CALL amxx_trace_texture(AMX *amx, cell *params)
{
    // P2: original signature trace_texture(entity, output[], len)
    // fetch texture around the given entity's own origin (engine module original behavior)
    int index = (int)params[1];
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict)
        return 0;
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    if (!dest) return 0;
    const char *texName = TRACE_TEXTURE(pEdict, pEdict->v.origin, pEdict->v.origin);
    if (!texName) texName = "";
    return amx_SetString(dest, texName, 0, 0, (int)params[3]);
}

cell AMX_NATIVE_CALL amxx_trace_model(AMX *amx, cell *params)
{
    // P2: original signature trace_model(entity, output[], len)
    // return the given entity's model name (engine module original behavior)
    int index = (int)params[1];
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict)
        return 0;
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    if (!dest) return 0;
    const char *model = STRING(pEdict->v.model);
    if (!model) model = "";
    return amx_SetString(dest, model, 0, 0, (int)params[3]);
}
cell AMX_NATIVE_CALL amxx_trace_normal(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    dest[0] = amx_ftoc(g_lastTrace.vecPlaneNormal.x);
    dest[1] = amx_ftoc(g_lastTrace.vecPlaneNormal.y);
    dest[2] = amx_ftoc(g_lastTrace.vecPlaneNormal.z);
    return 1;
}

// ===== P3: 鍥轰綋瀹炰綋鏌ヨ =====

cell AMX_NATIVE_CALL amxx_get_brush_entity(AMX *amx, cell *params)
{
    int index = (int)params[1];
    (void)amx;
    if (index < 0 || index > gpGlobals->maxEntities)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict) return 0;
    // 鍥轰綋妯″瀷瀹炰綋: model 绱㈠紩涓哄鏁版椂鏄?BSP/brush 妯″瀷
    if (pEdict->v.modelindex & 1)
        return index;
    return 0;
}

// ===== P3: 鍚戦噺杩愮畻 =====

cell AMX_NATIVE_CALL amxx_vec_add(AMX *amx, cell *params)
{
    cell *v1, *v2, *dest;
    amx_GetAddr(amx, params[1], &v1);
    amx_GetAddr(amx, params[2], &v2);
    amx_GetAddr(amx, params[3], &dest);

    Vector vec1(amx_ctof(v1[0]), amx_ctof(v1[1]), amx_ctof(v1[2]));
    Vector vec2(amx_ctof(v2[0]), amx_ctof(v2[1]), amx_ctof(v2[2]));
    Vector result = vec1 + vec2;

    dest[0] = amx_ftoc(result.x);
    dest[1] = amx_ftoc(result.y);
    dest[2] = amx_ftoc(result.z);
    return 1;
}

cell AMX_NATIVE_CALL amxx_vec_sub(AMX *amx, cell *params)
{
    cell *v1, *v2, *dest;
    amx_GetAddr(amx, params[1], &v1);
    amx_GetAddr(amx, params[2], &v2);
    amx_GetAddr(amx, params[3], &dest);

    Vector vec1(amx_ctof(v1[0]), amx_ctof(v1[1]), amx_ctof(v1[2]));
    Vector vec2(amx_ctof(v2[0]), amx_ctof(v2[1]), amx_ctof(v2[2]));
    Vector result = vec1 - vec2;

    dest[0] = amx_ftoc(result.x);
    dest[1] = amx_ftoc(result.y);
    dest[2] = amx_ftoc(result.z);
    return 1;
}

cell AMX_NATIVE_CALL amxx_vec_length(AMX *amx, cell *params)
{
    cell *v;
    amx_GetAddr(amx, params[1], &v);
    Vector vec(amx_ctof(v[0]), amx_ctof(v[1]), amx_ctof(v[2]));
    float length = vec.Length();
    return amx_ftoc(length);
}

cell AMX_NATIVE_CALL amxx_vec_distance(AMX *amx, cell *params)
{
    cell *v1, *v2;
    amx_GetAddr(amx, params[1], &v1);
    amx_GetAddr(amx, params[2], &v2);
    Vector vec1(amx_ctof(v1[0]), amx_ctof(v1[1]), amx_ctof(v1[2]));
    Vector vec2(amx_ctof(v2[0]), amx_ctof(v2[1]), amx_ctof(v2[2]));
    float dist = (vec1 - vec2).Length();
    return amx_ftoc(dist);
}

cell AMX_NATIVE_CALL amxx_vec_normalize(AMX *amx, cell *params)
{
    cell *v, *dest;
    amx_GetAddr(amx, params[1], &v);
    amx_GetAddr(amx, params[2], &dest);
    Vector vec(amx_ctof(v[0]), amx_ctof(v[1]), amx_ctof(v[2]));
    vec = vec.Normalize();
    dest[0] = amx_ftoc(vec.x);
    dest[1] = amx_ftoc(vec.y);
    dest[2] = amx_ftoc(vec.z);
    return 1;
}

cell AMX_NATIVE_CALL amxx_vec_angle(AMX *amx, cell *params)
{
    cell *v1, *v2;
    amx_GetAddr(amx, params[1], &v1);
    amx_GetAddr(amx, params[2], &v2);
    Vector vec1(amx_ctof(v1[0]), amx_ctof(v1[1]), amx_ctof(v1[2]));
    Vector vec2(amx_ctof(v2[0]), amx_ctof(v2[1]), amx_ctof(v2[2]));
    Vector angles;
    VEC_TO_ANGLES(vec2 - vec1, angles);
    cell *dest;
    amx_GetAddr(amx, params[3], &dest);
    dest[0] = amx_ftoc(angles.x);
    dest[1] = amx_ftoc(angles.y);
    dest[2] = amx_ftoc(angles.z);
    return 1;
}

cell AMX_NATIVE_CALL amxx_vec_dot_product(AMX *amx, cell *params)
{
    cell *v1, *v2;
    amx_GetAddr(amx, params[1], &v1);
    amx_GetAddr(amx, params[2], &v2);
    Vector vec1(amx_ctof(v1[0]), amx_ctof(v1[1]), amx_ctof(v1[2]));
    Vector vec2(amx_ctof(v2[0]), amx_ctof(v2[1]), amx_ctof(v2[2]));
    float result = DotProduct(vec1, vec2);
    return amx_ftoc(result);
}

cell AMX_NATIVE_CALL amxx_vec_cross(AMX *amx, cell *params)
{
    cell *v1, *v2, *dest;
    amx_GetAddr(amx, params[1], &v1);
    amx_GetAddr(amx, params[2], &v2);
    amx_GetAddr(amx, params[3], &dest);

    Vector vec1(amx_ctof(v1[0]), amx_ctof(v1[1]), amx_ctof(v1[2]));
    Vector vec2(amx_ctof(v2[0]), amx_ctof(v2[1]), amx_ctof(v2[2]));
    Vector result = CrossProduct(vec1, vec2);

    dest[0] = amx_ftoc(result.x);
    dest[1] = amx_ftoc(result.y);
    dest[2] = amx_ftoc(result.z);
    return 1;
}

cell AMX_NATIVE_CALL amxx_vec_reflect(AMX *amx, cell *params)
{
    cell *v, *normal, *dest;
    amx_GetAddr(amx, params[1], &v);
    amx_GetAddr(amx, params[2], &normal);
    amx_GetAddr(amx, params[3], &dest);

    Vector vec(amx_ctof(v[0]), amx_ctof(v[1]), amx_ctof(v[2]));
    Vector norm(amx_ctof(normal[0]), amx_ctof(normal[1]), amx_ctof(normal[2]));
    float dot = DotProduct(vec, norm);
    Vector result = vec - 2.0f * dot * norm;

    dest[0] = amx_ftoc(result.x);
    dest[1] = amx_ftoc(result.y);
    dest[2] = amx_ftoc(result.z);
    return 1;
}

cell AMX_NATIVE_CALL amxx_vec_mul(AMX *amx, cell *params)
{
    cell *v, *dest;
    amx_GetAddr(amx, params[1], &v);
    amx_GetAddr(amx, params[3], &dest);
    float scalar = amx_ctof(params[2]);

    Vector vec(amx_ctof(v[0]), amx_ctof(v[1]), amx_ctof(v[2]));
    Vector result = vec * scalar;

    dest[0] = amx_ftoc(result.x);
    dest[1] = amx_ftoc(result.y);
    dest[2] = amx_ftoc(result.z);
    return 1;
}

cell AMX_NATIVE_CALL amxx_vec_div(AMX *amx, cell *params)
{
    cell *v, *dest;
    amx_GetAddr(amx, params[1], &v);
    amx_GetAddr(amx, params[3], &dest);
    float scalar = amx_ctof(params[2]);
    if (scalar == 0.0f) return 0;

    Vector vec(amx_ctof(v[0]), amx_ctof(v[1]), amx_ctof(v[2]));
    Vector result = vec / scalar;

    dest[0] = amx_ftoc(result.x);
    dest[1] = amx_ftoc(result.y);
    dest[2] = amx_ftoc(result.z);
    return 1;
}

// ============================================================
// P1-2: Engine 模块补全 natives
// ============================================================

// Engine globals 偏移量 (对应 engine.inc 中的 GL_* 枚举)
enum {
    GL_trace_allhits = 0,
    GL_trace_allsurface,
    GL_trace_aleaf,
    GL_trace_patched,
    GL_trace_inopen,
    GL_trace_inwater,
    GL_trace_pHit,
    GL_trace_pHitEndpos,
    GL_trace_pHitNormal,
    GL_trace_pEndposNormal,
    GL_trace_flFraction,
    GL_trace_flPlaneDist,
    GL_trace_planeNormal,
    GL_trace_iHitgroup,
    GL_trace_iszTextureName,
    GL_trace_flu,
    GL_trace_flv,
    GL_srCover,
    GL_srCurrentspec,
    GL_srTimeinspec,
    GL_srLastattack,
    GL_tmEndTime,
    GL_pGlobalParameters,
    GL_time,
    GL_frametime,
    GL_force_retouch,
    GL_found_secrets,
    GL_serverflags,
    GL_total_secrets,
    GL_v_forward,
    GL_v_right,
    GL_v_up,
    GL_trace_pent,
    GL_trace_pplayer,
    GL_vecPlaneNormal,
    GL_flPlaneDist,
    GL_trace_pCdecals,
    GL_maxClients,
    GL_trace_pHitBuff,
    GL_maxEntities,
    GL_pStringBase,
    GL_pModelStrings,
    GL_pSoundStrings,
    GL_iStringBaseSize,
    GL_pEdicts,
    GL_areanodes,
    GL_headnode,
    GL_num_texinfo,
    GL_materials,
    GL_lightstylevalue,
    GL_pMoveVars,
    GL_pMoveGlobVars,
    GL_trace_hullnum,
    GL_trace_contents,
    GL_trace_startpos,
    GL_trace_endpos,
    GL_trace_startcontents,
    GL_vs_52
};

static cell *GlOffset2Ptr(int offset)
{
    switch (offset) {
        case GL_time:             return (cell *)&gpGlobals->time;
        case GL_frametime:        return (cell *)&gpGlobals->frametime;
        case GL_force_retouch:    return (cell *)&gpGlobals->force_retouch;
        case GL_found_secrets:    return (cell *)&gpGlobals->found_secrets;
        // GL_total_secrets / GL_trace_allhits / GL_trace_contents 在 Xash3D 的 globalvars_t 中不存在 → 返回 nullptr
        case GL_maxClients:       return (cell *)&gpGlobals->maxClients;
        case GL_maxEntities:      return (cell *)&gpGlobals->maxEntities;
        case GL_pStringBase:      return (cell *)&gpGlobals->pStringBase;
        case GL_trace_flFraction: return (cell *)&gpGlobals->trace_fraction;
        case GL_trace_inopen:     return (cell *)&gpGlobals->trace_inopen;
        case GL_trace_inwater:    return (cell *)&gpGlobals->trace_inwater;
        case GL_trace_iHitgroup:  return (cell *)&gpGlobals->trace_hitgroup;
        default: return nullptr;
    }
}

static cell *GlVecOffset2Ptr(int offset)
{
    switch (offset) {
        case GL_v_forward:  return (cell *)&gpGlobals->v_forward.x;
        case GL_v_right:    return (cell *)&gpGlobals->v_right.x;
        case GL_v_up:       return (cell *)&gpGlobals->v_up.x;
        // GL_trace_startpos 在 Xash3D 的 globalvars_t 中不存在 → 返回 nullptr
        case GL_trace_endpos:     return (cell *)&gpGlobals->trace_endpos.x;
        case GL_vecPlaneNormal:   return (cell *)&gpGlobals->trace_plane_normal.x;
        default: return nullptr;
    }
}

cell AMX_NATIVE_CALL amxx_get_global_float(AMX *amx, cell *params)
{
    (void)amx;
    cell *p = GlOffset2Ptr((int)params[1]);
    return p ? *p : 0;
}
cell AMX_NATIVE_CALL amxx_get_global_int(AMX *amx, cell *params)
{
    (void)amx;
    cell *p = GlOffset2Ptr((int)params[1]);
    return p ? *p : 0;
}
cell AMX_NATIVE_CALL amxx_get_global_string(AMX *amx, cell *params)
{
    int var = (int)params[1];
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    int maxlen = (int)params[3];
    const char *str = "";
    // GL_iszTextureName (Xash3D 下无对应 globalvars 字段): 默认保持空串即可
    (void)var;
    return amx_SetString(dest, str, 0, 0, maxlen);
}
cell AMX_NATIVE_CALL amxx_get_global_vector(AMX *amx, cell *params)
{
    cell *p = GlVecOffset2Ptr((int)params[1]);
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    if (!p || !dest) return 0;
    dest[0] = p[0]; dest[1] = p[1]; dest[2] = p[2];
    return 1;
}
cell AMX_NATIVE_CALL amxx_get_global_edict(AMX *amx, cell *params)
{
    (void)amx;
    int var = (int)params[1];
    // Xash3D 的 globalvars_t 里 trace_ent 直接是命中的 edict_t*（没有 trace_pHit 中间结构体）
    if (var == GL_trace_pent && gpGlobals->trace_ent)
        return ENTINDEX(gpGlobals->trace_ent);
    return 0;
}
cell AMX_NATIVE_CALL amxx_get_global_edict2(AMX *amx, cell *params)
{
    return amxx_get_global_edict(amx, params);
}

cell AMX_NATIVE_CALL amxx_find_ent_by_tname(AMX *amx, cell *params)
{
    int start = (int)params[1];
    cell *tname_addr;
    amx_GetAddr(amx, params[2], &tname_addr);
    char tname[128];
    amx_GetString(tname, tname_addr, 0, sizeof(tname));
    for (int i = start + 1; i <= gpGlobals->maxEntities; i++) {
        edict_t *e = INDEXENT(i);
        if (!e || e->free) continue;
        const char *tn = STRING(e->v.targetname);
        if (tn && tn[0] && strcmp(tn, tname) == 0) return i;
    }
    return 0;
}

cell AMX_NATIVE_CALL amxx_attach_view(AMX *amx, cell *params)
{
    int index = (int)params[1], target = (int)params[2];
    if (index < 1 || index > gpGlobals->maxClients) return 0;
    edict_t *pPlayer = INDEXENT(index);
    if (!pPlayer) return 0;
    if (target > 0 && target <= gpGlobals->maxEntities) {
        edict_t *pTarget = INDEXENT(target);
        if (pTarget) {
            // Xash3D 的 enginefuncs_t 没有 pfnSetClientViewEntity，改用 SET_VIEW 宏
            // SET_VIEW = (*g_engfuncs.pfnSetView)，可在 enginecallback.h:149 找到
            SET_VIEW(pPlayer, pTarget);
        }
    }
    return 1;
}
cell AMX_NATIVE_CALL amxx_set_view(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return 1;
}

cell AMX_NATIVE_CALL amxx_playback_event(AMX *amx, cell *params)
{
    int flags = (int)params[1], invoker = (int)params[2], eventidx = (int)params[3];
    float delay = amx_ctof(params[4]);
    cell *org_addr, *ang_addr;
    amx_GetAddr(amx, params[5], &org_addr);
    amx_GetAddr(amx, params[6], &ang_addr);
    Vector origin(org_addr ? amx_ctof(org_addr[0]) : 0, org_addr ? amx_ctof(org_addr[1]) : 0, org_addr ? amx_ctof(org_addr[2]) : 0);
    Vector angles(ang_addr ? amx_ctof(ang_addr[0]) : 0, ang_addr ? amx_ctof(ang_addr[1]) : 0, ang_addr ? amx_ctof(ang_addr[2]) : 0);
    float fp1 = amx_ctof(params[7]), fp2 = amx_ctof(params[8]);
    int ip1 = (int)params[9], ip2 = (int)params[10], bp1 = (int)params[11], bp2 = (int)params[12];
    edict_t *pInvoker = (invoker > 0) ? INDEXENT(invoker) : nullptr;
    if (g_engfuncs.pfnPlaybackEvent) {
        g_engfuncs.pfnPlaybackEvent(flags, pInvoker, eventidx, delay, origin, angles, fp1, fp2, ip1, ip2, bp1, bp2);
        return 1;
    }
    return 0;
}
cell AMX_NATIVE_CALL amxx_get_usercmd(AMX *amx, cell *params) { (void)amx; (void)params; return 0; }
cell AMX_NATIVE_CALL amxx_set_usercmd(AMX *amx, cell *params) { (void)amx; (void)params; return 0; }

cell AMX_NATIVE_CALL amxx_fake_touch(AMX *amx, cell *params)
{
    (void)amx;
    int a = (int)params[1], b = (int)params[2];
    edict_t *e1 = (a > 0 && a <= gpGlobals->maxEntities) ? INDEXENT(a) : nullptr;
    edict_t *e2 = (b > 0 && b <= gpGlobals->maxEntities) ? INDEXENT(b) : nullptr;
    if (!e1 || !e2) return 0;
    // MDLL_Touch 在 ReGameDLL 内嵌版未定义，等价于 cbase.h 声明的 DispatchTouch
    DispatchTouch(e1, e2);
    return 1;
}
cell AMX_NATIVE_CALL amxx_force_use(AMX *amx, cell *params)
{
    (void)amx;
    int used = (int)params[1], user = (int)params[2];
    if (used <= 0 || used > gpGlobals->maxEntities || user < 1 || user > gpGlobals->maxClients) return 0;
    edict_t *eu = INDEXENT(used), *eU = INDEXENT(user);
    if (!eu || !eU || !eU->pvPrivateData) return 0;
    CBaseEntity *pEnt = GET_PRIVATE<CBaseEntity>(eu);
    CBasePlayer *pPly = GET_PRIVATE<CBasePlayer>(eU);
    if (!pEnt || !pPly) return 0;
    pEnt->Use(pPly, pPly, USE_TOGGLE, 0);
    return 1;
}
cell AMX_NATIVE_CALL amxx_set_lights(AMX *amx, cell *params)
{
    cell *addr; amx_GetAddr(amx, params[1], &addr);
    char light[64]; amx_GetString(light, addr, 0, sizeof(light));
    // Xash3D enginefuncs_t 没有 pfnSetLightStyle，改用引擎命令 "lightstyle <style> <value>"
    // style 0 = 全局环境光 (AMXX set_lights 默认就是 0)
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "lightstyle 0 %s\n", light);
    SERVER_COMMAND(cmd);
    return 1;
}
cell AMX_NATIVE_CALL amxx_create_entity(AMX *amx, cell *params)
{
    cell *addr; amx_GetAddr(amx, params[1], &addr);
    char classname[128]; amx_GetString(classname, addr, 0, sizeof(classname));
    edict_t *e = CREATE_NAMED_ENTITY(MAKE_STRING(classname));
    return e ? ENTINDEX(e) : 0;
}
cell AMX_NATIVE_CALL amxx_remove_entity(AMX *amx, cell *params)
{
    (void)amx;
    int i = (int)params[1];
    if (i <= 0 || i > gpGlobals->maxEntities) return 0;
    edict_t *e = INDEXENT(i);
    if (!e) return 0;
    REMOVE_ENTITY(e);
    return 1;
}
cell AMX_NATIVE_CALL amxx_is_valid_ent(AMX *amx, cell *params)
{
    (void)amx;
    int i = (int)params[1];
    if (i < 0 || i > gpGlobals->maxEntities) return 0;
    if (i == 0) return 1;
    edict_t *e = INDEXENT(i);
    if (!e || e->free) return 0;
    return 1;
}
cell AMX_NATIVE_CALL amxx_entity_count(AMX *amx, cell *params) { (void)amx; (void)params; return gpGlobals->maxEntities; }

cell AMX_NATIVE_CALL amxx_entity_set_size(AMX *amx, cell *params)
{
    int i = (int)params[1];
    if (i <= 0 || i > gpGlobals->maxEntities) return 0;
    edict_t *e = INDEXENT(i); if (!e) return 0;
    cell *mn, *mx; amx_GetAddr(amx, params[2], &mn); amx_GetAddr(amx, params[3], &mx);
    Vector mins(amx_ctof(mn[0]), amx_ctof(mn[1]), amx_ctof(mn[2]));
    Vector maxs(amx_ctof(mx[0]), amx_ctof(mx[1]), amx_ctof(mx[2]));
    SET_SIZE(e, mins, maxs);
    return 1;
}
cell AMX_NATIVE_CALL amxx_entity_set_origin(AMX *amx, cell *params)
{
    int i = (int)params[1];
    if (i <= 0 || i > gpGlobals->maxEntities) return 0;
    edict_t *e = INDEXENT(i); if (!e) return 0;
    cell *org; amx_GetAddr(amx, params[2], &org);
    SET_ORIGIN(e, Vector(amx_ctof(org[0]), amx_ctof(org[1]), amx_ctof(org[2])));
    return 1;
}
cell AMX_NATIVE_CALL amxx_entity_set_model(AMX *amx, cell *params)
{
    int i = (int)params[1];
    if (i <= 0 || i > gpGlobals->maxEntities) return 0;
    edict_t *e = INDEXENT(i); if (!e) return 0;
    cell *addr; amx_GetAddr(amx, params[2], &addr);
    char model[256]; amx_GetString(model, addr, 0, sizeof(model));
    if (model[0]) { SET_MODEL(e, model); return 1; }
    return 0;
}
cell AMX_NATIVE_CALL amxx_set_ent_rendering(AMX *amx, cell *params)
{
    int i = (int)params[1];
    if (i <= 0 || i > gpGlobals->maxEntities) return 0;
    edict_t *e = INDEXENT(i); if (!e) return 0;
    e->v.renderfx = params[2];
    e->v.rendercolor.x = (float)params[3];
    e->v.rendercolor.y = (float)params[4];
    e->v.rendercolor.z = (float)params[5];
    e->v.rendermode = params[6];
    e->v.renderamt = (float)params[7];
    return 1;
}
cell AMX_NATIVE_CALL amxx_drop_to_floor(AMX *amx, cell *params)
{
    (void)amx;
    int i = (int)params[1];
    if (i <= 0 || i > gpGlobals->maxEntities) return 0;
    edict_t *e = INDEXENT(i); if (!e) return 0;
    return DROP_TO_FLOOR(e);
}
cell AMX_NATIVE_CALL amxx_set_speak(AMX *amx, cell *params)
{
    (void)amx;
    int i = (int)params[1];
    if (i < 1 || i > gpGlobals->maxClients) return 0;
    edict_t *e = INDEXENT(i); if (!e) return 0;
    e->v.spawnflags = params[2];
    return 1;
}
cell AMX_NATIVE_CALL amxx_get_speak(AMX *amx, cell *params)
{
    (void)amx;
    int i = (int)params[1];
    if (i < 1 || i > gpGlobals->maxClients) return 0;
    edict_t *e = INDEXENT(i); if (!e) return 0;
    return (cell)e->v.spawnflags;
}
cell AMX_NATIVE_CALL amxx_halflife_time(AMX *amx, cell *params) { (void)amx; (void)params; return amx_ftoc(gpGlobals->time); }

cell AMX_NATIVE_CALL amxx_radius_damage(AMX *amx, cell *params)
{
    cell *a; amx_GetAddr(amx, params[1], &a);
    Vector org(amx_ctof(a[0]), amx_ctof(a[1]), amx_ctof(a[2]));
    float dmgM = amx_ctof(params[2]), rM = amx_ctof(params[3]);
    TraceResult tr;
    UTIL_TraceHull(org, org, dont_ignore_monsters, head_hull, nullptr, &tr);
    // Xash3D RadiusDamage(combat.h) 签名: (vecSrc, inflictor, attacker, damage, radius, ignoreClass, dmgType) = 7 参数
    entvars_t *inflictor = (tr.pHit && !tr.pHit->free) ? &tr.pHit->v : nullptr;
    ::RadiusDamage(org, inflictor, nullptr, dmgM, rM, CLASS_NONE, DMG_BLAST);
    return 1;
}
cell AMX_NATIVE_CALL amxx_trace_line(AMX *amx, cell *params)
{
    int ig = (int)params[1];
    cell *s, *e, *r;
    amx_GetAddr(amx, params[2], &s); amx_GetAddr(amx, params[3], &e); amx_GetAddr(amx, params[4], &r);
    Vector st(amx_ctof(s[0]), amx_ctof(s[1]), amx_ctof(s[2]));
    Vector en(amx_ctof(e[0]), amx_ctof(e[1]), amx_ctof(e[2]));
    TraceResult tr;
    TRACE_LINE(st, en, dont_ignore_monsters, ig > 0 ? INDEXENT(ig) : nullptr, &tr);
    g_lastTrace = tr;  // 缓存到全局，供 traceresult() 读取
    if (r) { r[0] = amx_ftoc(tr.vecEndPos.x); r[1] = amx_ftoc(tr.vecEndPos.y); r[2] = amx_ftoc(tr.vecEndPos.z); }
    return tr.pHit ? ENTINDEX(tr.pHit) : 0;
}
cell AMX_NATIVE_CALL amxx_entity_intersects(AMX *amx, cell *params)
{
    int a = (int)params[1], b = (int)params[2];
    if (a <= 0 || a > gpGlobals->maxEntities || b <= 0 || b > gpGlobals->maxEntities) return 0;
    edict_t *ea = INDEXENT(a), *eb = INDEXENT(b); if (!ea || !eb) return 0;
    if (ea->v.absmax.x < eb->v.absmin.x || ea->v.absmin.x > eb->v.absmax.x) return 0;
    if (ea->v.absmax.y < eb->v.absmin.y || ea->v.absmin.y > eb->v.absmax.y) return 0;
    if (ea->v.absmax.z < eb->v.absmin.z || ea->v.absmin.z > eb->v.absmax.z) return 0;
    return 1;
}
cell AMX_NATIVE_CALL amxx_is_visible(AMX *amx, cell *params)
{
    int a = (int)params[1], b = (int)params[2];
    if (a <= 0 || a > gpGlobals->maxEntities || b <= 0 || b > gpGlobals->maxEntities) return 0;
    edict_t *ea = INDEXENT(a), *eb = INDEXENT(b); if (!ea || !eb) return 0;
    TraceResult tr;
    Vector s = ea->v.origin + ea->v.view_ofs;
    Vector t = eb->v.origin + eb->v.view_ofs;
    TRACE_LINE(s, t, dont_ignore_monsters, ea, &tr);
    return tr.flFraction >= 1.0f ? 1 : 0;
}
cell AMX_NATIVE_CALL amxx_trace_hull(AMX *amx, cell *params)
{
    cell *o; amx_GetAddr(amx, params[1], &o);
    int hull = (int)params[2], ig = (int)params[3], ignoreMons = (int)params[4];
    int nump = (int)params[0] / (int)sizeof(cell);
    Vector org(amx_ctof(o[0]), amx_ctof(o[1]), amx_ctof(o[2]));
    Vector end = org;
    if (nump >= 5) {
        cell *e; amx_GetAddr(amx, params[5], &e);
        if (e) end = Vector(amx_ctof(e[0]), amx_ctof(e[1]), amx_ctof(e[2]));
    }
    TraceResult tr;
    // 与原版 engine 模块一致：直接传递 hull 编号（HULL_POINT/HULL_HUMAN/HULL_LARGE/HULL_HEAD
    // 与引擎 point/human/large/head_hull 枚举值一一对应），返回 StartSolid(1)|AllSolid(2)|!InOpen(4) 位掩码
    TRACE_HULL(org, end, ignoreMons, hull, ig > 0 ? INDEXENT(ig) : nullptr, &tr);
    g_lastTrace = tr;
    int iResult = 0;
    if (tr.fStartSolid) iResult += 1;
    if (tr.fAllSolid)   iResult += 2;
    if (!tr.fInOpen)    iResult += 4;
    return iResult;
}
cell AMX_NATIVE_CALL amxx_get_decal_index(AMX *amx, cell *params)
{
    cell *a; amx_GetAddr(amx, params[1], &a);
    char name[64]; amx_GetString(name, a, 0, sizeof(name));
    if (g_engfuncs.pfnDecalIndex) return (cell)g_engfuncs.pfnDecalIndex(name);
    return 0;
}
cell AMX_NATIVE_CALL amxx_get_info_keybuffer(AMX *amx, cell *params)
{
    int id = (int)params[1];
    cell *dest; amx_GetAddr(amx, params[2], &dest);
    int maxlen = (int)params[3];
    const char *info = "";
    if (id >= 1 && id <= gpGlobals->maxClients) {
        edict_t *e = INDEXENT(id);
        if (e && g_engfuncs.pfnGetInfoKeyBuffer) info = (const char *)g_engfuncs.pfnGetInfoKeyBuffer(e);
    }
    return amx_SetString(dest, info ? info : "", 0, 0, maxlen);
}
cell AMX_NATIVE_CALL amxx_call_think(AMX *amx, cell *params)
{
    (void)amx;
    int i = (int)params[1];
    if (i <= 0 || i > gpGlobals->maxEntities) return 0;
    edict_t *e = INDEXENT(i); if (!e) return 0;
    // MDLL_Think 在 ReGameDLL 内嵌版未定义，等价于 cbase.h 声明的 DispatchThink
    DispatchThink(e);
    return 1;
}
cell AMX_NATIVE_CALL amxx_get_keyvalue(AMX *amx, cell *params)
{
    int idx = (int)params[1];
    cell *ka; amx_GetAddr(amx, params[2], &ka);
    char key[64]; amx_GetString(key, ka, 0, sizeof(key));
    cell *va; amx_GetAddr(amx, params[3], &va);
    int maxlen = (int)params[4];
    if (idx <= 0 || idx > gpGlobals->maxEntities) return amx_SetString(va, "", 0, 0, maxlen);
    edict_t *e = INDEXENT(idx); if (!e) return amx_SetString(va, "", 0, 0, maxlen);
    const char *r = "";
    if (stricmp(key, "classname") == 0) r = STRING(e->v.classname);
    else if (stricmp(key, "targetname") == 0) r = STRING(e->v.targetname);
    else if (stricmp(key, "target") == 0) r = STRING(e->v.target);
    else if (stricmp(key, "globalname") == 0) r = STRING(e->v.globalname);
    else if (stricmp(key, "model") == 0) r = STRING(e->v.model);
    else if (stricmp(key, "message") == 0) r = STRING(e->v.message);
    else if (stricmp(key, "noise") == 0) r = STRING(e->v.noise);
    else if (stricmp(key, "noise1") == 0) r = STRING(e->v.noise1);
    else if (stricmp(key, "noise2") == 0) r = STRING(e->v.noise2);
    else if (stricmp(key, "noise3") == 0) r = STRING(e->v.noise3);
    else if (stricmp(key, "netname") == 0) r = STRING(e->v.netname);
    else if (stricmp(key, "origin") == 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%g %g %g", e->v.origin.x, e->v.origin.y, e->v.origin.z);
        return amx_SetString(va, buf, 0, 0, maxlen);
    }
    if (!r) r = "";
    return amx_SetString(va, r, 0, 0, maxlen);
}

// ================ P1-3: Ham GetHamReturn_* 系列 ================
// HamCtx / g_hamCtx / g_hamOrigCtx 定义在 hamsandwich.h/.cpp（与桥接函数共享）
cell AMX_NATIVE_CALL amxx_GetHamReturnStatus(AMX *amx, cell *params) { (void)amx; (void)params; return 0; }
cell AMX_NATIVE_CALL amxx_GetHamReturnInt(AMX *amx, cell *params) { (void)amx; (void)params; return g_hamCtx.intVal; }
cell AMX_NATIVE_CALL amxx_GetHamReturnFloat(AMX *amx, cell *params) { (void)amx; (void)params; return amx_ftoc(g_hamCtx.floatVal); }
cell AMX_NATIVE_CALL amxx_GetHamReturnVector(AMX *amx, cell *params)
{
    cell *a; amx_GetAddr(amx, params[1], &a);
    if (a) { a[0] = amx_ftoc(g_hamCtx.vecVal[0]); a[1] = amx_ftoc(g_hamCtx.vecVal[1]); a[2] = amx_ftoc(g_hamCtx.vecVal[2]); }
    return 1;
}
cell AMX_NATIVE_CALL amxx_GetHamReturnEntity(AMX *amx, cell *params) { (void)amx; (void)params; return g_hamCtx.entVal; }
cell AMX_NATIVE_CALL amxx_GetHamReturnString(AMX *amx, cell *params)
{
    cell *a; amx_GetAddr(amx, params[1], &a);
    return amx_SetString(a, g_hamCtx.stringVal, 0, 0, (int)params[2]);
}

// GetHamReturnInteger - 别名版，与 GetHamReturnInt 相同
cell AMX_NATIVE_CALL amxx_GetHamReturnInteger(AMX *amx, cell *params) {
    cell *a; amx_GetAddr(amx, params[1], &a);
    if (a) *a = g_hamCtx.intVal;
    return 1;
}

// SetHamReturnInteger - 设置 Hook 返回整数
cell AMX_NATIVE_CALL amxx_SetHamReturnInteger(AMX *amx, cell *params) {
    (void)amx;
    g_hamCtx.intVal = (int)params[1];
    g_hamCtx.returnType = 1; // int
    return 1;
}

// SetHamReturnFloat - 设置 Hook 返回浮点
cell AMX_NATIVE_CALL amxx_SetHamReturnFloat(AMX *amx, cell *params) {
    (void)amx;
    g_hamCtx.floatVal = amx_ctof(params[1]);
    g_hamCtx.returnType = 2; // float
    return 1;
}

// SetHamReturnVector - 设置 Hook 返回向量
cell AMX_NATIVE_CALL amxx_SetHamReturnVector(AMX *amx, cell *params) {
    cell *a; amx_GetAddr(amx, params[1], &a);
    if (a) {
        g_hamCtx.vecVal[0] = amx_ctof(a[0]);
        g_hamCtx.vecVal[1] = amx_ctof(a[1]);
        g_hamCtx.vecVal[2] = amx_ctof(a[2]);
    }
    g_hamCtx.returnType = 3; // vector
    return 1;
}

// SetHamReturnEntity - 设置 Hook 返回实体索引
cell AMX_NATIVE_CALL amxx_SetHamReturnEntity(AMX *amx, cell *params) {
    (void)amx;
    g_hamCtx.entVal = (int)params[1];
    g_hamCtx.returnType = 4; // entity
    return 1;
}

// SetHamReturnString - 设置 Hook 返回字符串
cell AMX_NATIVE_CALL amxx_SetHamReturnString(AMX *amx, cell *params) {
    cell *a; amx_GetAddr(amx, params[1], &a);
    if (a)
        amx_GetString(g_hamCtx.stringVal, a, 0, sizeof(g_hamCtx.stringVal));
    else
        g_hamCtx.stringVal[0] = '\0';
    g_hamCtx.returnType = 5; // string
    return 1;
}

// GetOrigHamReturnInteger - 获取原始函数返回整数
cell AMX_NATIVE_CALL amxx_GetOrigHamReturnInteger(AMX *amx, cell *params) {
    cell *a; amx_GetAddr(amx, params[1], &a);
    if (a) *a = g_hamOrigCtx.intVal;
    return 1;
}

// GetOrigHamReturnFloat - 获取原始返回浮点
cell AMX_NATIVE_CALL amxx_GetOrigHamReturnFloat(AMX *amx, cell *params) {
    cell *a; amx_GetAddr(amx, params[1], &a);
    if (a) *a = amx_ftoc(g_hamOrigCtx.floatVal);
    return 1;
}

// GetOrigHamReturnVector - 获取原始返回向量
cell AMX_NATIVE_CALL amxx_GetOrigHamReturnVector(AMX *amx, cell *params) {
    cell *a; amx_GetAddr(amx, params[1], &a);
    if (a) {
        a[0] = amx_ftoc(g_hamOrigCtx.vecVal[0]);
        a[1] = amx_ftoc(g_hamOrigCtx.vecVal[1]);
        a[2] = amx_ftoc(g_hamOrigCtx.vecVal[2]);
    }
    return 1;
}

// GetOrigHamReturnEntity - 获取原始返回实体
cell AMX_NATIVE_CALL amxx_GetOrigHamReturnEntity(AMX *amx, cell *params) {
    cell *a; amx_GetAddr(amx, params[1], &a);
    if (a) *a = g_hamOrigCtx.entVal;
    return 1;
}

// GetOrigHamReturnString - 获取原始返回字符串
cell AMX_NATIVE_CALL amxx_GetOrigHamReturnString(AMX *amx, cell *params) {
    cell *a; amx_GetAddr(amx, params[1], &a);
    if (a)
        amx_SetString(a, g_hamOrigCtx.stringVal, 0, 0, params[2]);
    return 1;
}

// SetHamParamEntity2 - 与 SetHamParamEntity 相同语义（修改当前回调参数）
cell AMX_NATIVE_CALL amxx_SetHamParamEntity2(AMX *amx, cell *params) {
    (void)amx;
    if (!g_currentHamCtx.active)
        return 0;
    int param = (int)params[1];
    if (param < 1 || param > g_currentHamCtx.paramCount)
        return 0;
    auto &slot = g_currentHamCtx.params[param - 1];
    if (slot.type != HamParamSlot::PT_ENTITY || !slot.ptr)
        return 0;
    *(int *)slot.ptr = (int)params[2];
    return 1;
}

// SetHamParamTraceResult - 未实现（返回 0 并记错误，避免插件误以为生效）
cell AMX_NATIVE_CALL amxx_SetHamParamTraceResult(AMX *amx, cell *params) {
    (void)amx; (void)params;
    AMXX_LOG_ERR("[Ham] SetHamParamTraceResult: not implemented in this build");
    return 0;
}

// SetHamParamItemInfo - 未实现（返回 0 并记错误，避免插件误以为生效）
cell AMX_NATIVE_CALL amxx_SetHamParamItemInfo(AMX *amx, cell *params) {
    (void)amx; (void)params;
    AMXX_LOG_ERR("[Ham] SetHamParamItemInfo: not implemented in this build");
    return 0;
}

// ItemInfo 池（简化版）
struct ItemInfoEntry { bool used; int dummy; };
static ItemInfoEntry s_itemInfoPool[64];
static int s_itemInfoNextHandle = 1;

// CreateHamItemInfo
cell AMX_NATIVE_CALL amxx_CreateHamItemInfo(AMX *amx, cell *params) {
    (void)amx; (void)params;
    for (int i = 0; i < 64; i++) {
        if (!s_itemInfoPool[i].used) {
            s_itemInfoPool[i].used = true;
            return s_itemInfoNextHandle++;
        }
    }
    return 0;
}

// FreeHamItemInfo
cell AMX_NATIVE_CALL amxx_FreeHamItemInfo(AMX *amx, cell *params) {
    (void)amx;
    int handle = (int)params[1] - 1;
    if (handle >= 0 && handle < 64) {
        s_itemInfoPool[handle].used = false;
    }
    return 1;
}

// GetHamItemInfo - 未实现（返回 0 并记错误，避免插件误以为生效）
cell AMX_NATIVE_CALL amxx_GetHamItemInfo(AMX *amx, cell *params) {
    (void)amx; (void)params;
    AMXX_LOG_ERR("[Ham] GetHamItemInfo: not implemented in this build");
    return 0;
}

// SetHamItemInfo - 未实现（返回 0 并记错误，避免插件误以为生效）
cell AMX_NATIVE_CALL amxx_SetHamItemInfo(AMX *amx, cell *params) {
    (void)amx; (void)params;
    AMXX_LOG_ERR("[Ham] SetHamItemInfo: not implemented in this build");
    return 0;
}

// set_pdata_cbase(id, offset, value, linuxdiff=5, macdiff=5)
// 与 get_pdata_cbase 成对，写入 CBase 指针字段
cell AMX_NATIVE_CALL amxx_set_pdata_cbase(AMX *amx, cell *params) {
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxEntities) return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free || !pEdict->pvPrivateData) return 0;

    int offset = (int)params[2];
    int value = (int)params[3];
    int linuxdiff = (params[0] / sizeof(cell) >= 4) ? (int)params[4] : 5;

#ifdef _WIN32
    (void)linuxdiff;
#else
    offset += linuxdiff;
#endif

    unsigned char *pvData = (unsigned char *)pEdict->pvPrivateData + offset;
    // 写入 CBase* (ENTINDEX 转 edict_t*：-1 写 NULL)
    if (value <= 0) {
        *(void **)pvData = nullptr;
    } else {
        edict_t *pTarget = INDEXENT(value);
        if (pTarget && !pTarget->free && pTarget->pvPrivateData) {
            *(void **)pvData = pTarget->pvPrivateData;
        } else {
            *(void **)pvData = nullptr;
        }
    }
    return 1;
}

// get_pdata_cbase_safe(id, offset, linuxdiff=5, macdiff=5)
// 同 get_pdata_cbase，但额外验证：如果 edict 无效但非空，返回 -2 而非 -1
cell AMX_NATIVE_CALL amxx_get_pdata_cbase_safe(AMX *amx, cell *params) {
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxEntities) return -1;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free || !pEdict->pvPrivateData) return -1;

    int offset = (int)params[2];
    int linuxdiff = (params[0] / sizeof(cell) >= 3) ? (int)params[3] : 5;

#ifdef _WIN32
    (void)linuxdiff;
#else
    offset += linuxdiff;
#endif

    unsigned char *pvData = (unsigned char *)pEdict->pvPrivateData + offset;
    void *pCBase = *(void **)pvData;

    if (!pCBase) return -1;

    // 从 CBase* 反查 entvars_t* 再转 edict_t*：CBase 首个字段为 pev 指针
    entvars_t *pVars = *(entvars_t **)pCBase;
    if (!pVars) return -2;
    edict_t *pOwner = ENT(pVars);
    if (!pOwner) return -2;
    int ownerIdx = ENTINDEX(pOwner);
    if (ownerIdx <= 0) return -2;
    if (pOwner->free) return -2;
    return ownerIdx;
}

// ================ P1-4: Fakemeta KeyValue 句柄 API ================
struct KvdEntry {
    int handle;
    char key[256];
    char value[1024];
};
static std::vector<KvdEntry> s_kvdPool;
static int s_kvdNextHandle = 1;

// create_kvd() — 创建 KeyValueData 句柄，返回 handle (>0)，失败返回 0
cell AMX_NATIVE_CALL amxx_create_kvd(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    KvdEntry kv;
    kv.handle = s_kvdNextHandle++;
    kv.key[0] = '\0';
    kv.value[0] = '\0';
    s_kvdPool.push_back(kv);
    return (cell)kv.handle;
}

// free_kvd(handle) — 释放 KeyValueData 句柄
cell AMX_NATIVE_CALL amxx_free_kvd(AMX *amx, cell *params)
{
    (void)amx;
    int handle = (int)params[1];
    for (auto it = s_kvdPool.begin(); it != s_kvdPool.end(); ++it) {
        if (it->handle == handle) {
            s_kvdPool.erase(it);
            return 1;
        }
    }
    return 0;
}

// get_kvd(handle, &Key[], &Value[], KeyMax, ValueMax) — 读取 KVD
cell AMX_NATIVE_CALL amxx_get_kvd(AMX *amx, cell *params)
{
    int handle = (int)params[1];
    KvdEntry *pKv = nullptr;
    for (auto &kv : s_kvdPool) {
        if (kv.handle == handle) { pKv = &kv; break; }
    }
    if (!pKv) return 0;
    cell *keyAddr, *valAddr;
    amx_GetAddr(amx, params[2], &keyAddr);
    amx_GetAddr(amx, params[3], &valAddr);
    amx_SetString(keyAddr, pKv->key, 0, 0, (int)params[4]);
    amx_SetString(valAddr, pKv->value, 0, 0, (int)params[5]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_create_forwardex(AMX *amx, cell *params)
{
    (void)amx;
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char name[256];
    amx_GetString(name, name_addr, 0, sizeof(name));

    AMXXRuntime::GetInstance().CreateForward(name);
    return 1;
}

// ============================================================
// P1-5: Engine module entity_get_*/entity_set_* natives
// EV_* 枚举值与 engine_const.inc 完全对齐（每类从 0 开始）
// 实现方式与原版 modules/engine/entity.cpp 的 switch-case 完全一致。
// ============================================================

enum {
    EV_INT_gamestate = 0, EV_INT_oldbuttons, EV_INT_groupinfo, EV_INT_iuser1,
    EV_INT_iuser2, EV_INT_iuser3, EV_INT_iuser4, EV_INT_weaponanim,
    EV_INT_pushmsec, EV_INT_bInDuck, EV_INT_flTimeStepSound, EV_INT_flSwimTime,
    EV_INT_flDuckTime, EV_INT_iStepLeft, EV_INT_movetype, EV_INT_solid,
    EV_INT_skin, EV_INT_body, EV_INT_effects, EV_INT_light_level,
    EV_INT_sequence, EV_INT_gaitsequence, EV_INT_modelindex, EV_INT_playerclass,
    EV_INT_waterlevel, EV_INT_watertype, EV_INT_spawnflags, EV_INT_flags,
    EV_INT_colormap, EV_INT_team, EV_INT_fixangle, EV_INT_weapons,
    EV_INT_rendermode, EV_INT_renderfx, EV_INT_button, EV_INT_impulse,
    EV_INT_deadflag
};

enum {
    EV_FL_impacttime = 0, EV_FL_starttime, EV_FL_idealpitch, EV_FL_pitch_speed,
    EV_FL_ideal_yaw, EV_FL_yaw_speed, EV_FL_ltime, EV_FL_nextthink,
    EV_FL_gravity, EV_FL_friction, EV_FL_frame, EV_FL_animtime,
    EV_FL_framerate, EV_FL_health, EV_FL_frags, EV_FL_takedamage,
    EV_FL_max_health, EV_FL_teleport_time, EV_FL_armortype, EV_FL_armorvalue,
    EV_FL_dmg_take, EV_FL_dmg_save, EV_FL_dmg, EV_FL_dmgtime,
    EV_FL_speed, EV_FL_air_finished, EV_FL_pain_finished, EV_FL_radsuit_finished,
    EV_FL_scale, EV_FL_renderamt, EV_FL_maxspeed, EV_FL_fov,
    EV_FL_flFallVelocity, EV_FL_fuser1, EV_FL_fuser2, EV_FL_fuser3, EV_FL_fuser4
};

enum {
    EV_VEC_origin = 0, EV_VEC_oldorigin, EV_VEC_velocity, EV_VEC_basevelocity,
    EV_VEC_clbasevelocity, EV_VEC_movedir, EV_VEC_angles, EV_VEC_avelocity,
    EV_VEC_punchangle, EV_VEC_v_angle, EV_VEC_endpos, EV_VEC_startpos,
    EV_VEC_absmin, EV_VEC_absmax, EV_VEC_mins, EV_VEC_maxs,
    EV_VEC_size, EV_VEC_rendercolor, EV_VEC_view_ofs, EV_VEC_vuser1,
    EV_VEC_vuser2, EV_VEC_vuser3, EV_VEC_vuser4
};

enum {
    EV_ENT_chain = 0, EV_ENT_dmg_inflictor, EV_ENT_enemy, EV_ENT_aiment,
    EV_ENT_owner, EV_ENT_groundentity, EV_ENT_pContainingEntity, EV_ENT_euser1,
    EV_ENT_euser2, EV_ENT_euser3, EV_ENT_euser4
};

enum {
    EV_SZ_classname = 0, EV_SZ_globalname, EV_SZ_model, EV_SZ_target,
    EV_SZ_targetname, EV_SZ_netname, EV_SZ_message, EV_SZ_noise,
    EV_SZ_noise1, EV_SZ_noise2, EV_SZ_noise3, EV_SZ_viewmodel, EV_SZ_weaponmodel
};

enum {
    EV_BYTE_controller1 = 0, EV_BYTE_controller2, EV_BYTE_controller3,
    EV_BYTE_controller4, EV_BYTE_blending1, EV_BYTE_blending2
};

cell AMX_NATIVE_CALL amxx_entity_get_int(AMX *amx, cell *params)
{
    (void)amx;
    int i = (int)params[1];
    if (i <= 0 || i > gpGlobals->maxEntities) return 0;
    edict_t *pEnt = INDEXENT(i);
    if (!pEnt) return 0;

    int idx = (int)params[2];
    int iRetValue = 0;
    switch (idx) {
        case EV_INT_gamestate:       iRetValue = pEnt->v.gamestate; break;
        case EV_INT_oldbuttons:      iRetValue = pEnt->v.oldbuttons; break;
        case EV_INT_groupinfo:       iRetValue = pEnt->v.groupinfo; break;
        case EV_INT_iuser1:          iRetValue = pEnt->v.iuser1; break;
        case EV_INT_iuser2:          iRetValue = pEnt->v.iuser2; break;
        case EV_INT_iuser3:          iRetValue = pEnt->v.iuser3; break;
        case EV_INT_iuser4:          iRetValue = pEnt->v.iuser4; break;
        case EV_INT_weaponanim:      iRetValue = pEnt->v.weaponanim; break;
        case EV_INT_pushmsec:        iRetValue = pEnt->v.pushmsec; break;
        case EV_INT_bInDuck:         iRetValue = pEnt->v.bInDuck; break;
        case EV_INT_flTimeStepSound: iRetValue = pEnt->v.flTimeStepSound; break;
        case EV_INT_flSwimTime:      iRetValue = pEnt->v.flSwimTime; break;
        case EV_INT_flDuckTime:      iRetValue = pEnt->v.flDuckTime; break;
        case EV_INT_iStepLeft:       iRetValue = pEnt->v.iStepLeft; break;
        case EV_INT_movetype:        iRetValue = pEnt->v.movetype; break;
        case EV_INT_solid:           iRetValue = pEnt->v.solid; break;
        case EV_INT_skin:            iRetValue = pEnt->v.skin; break;
        case EV_INT_body:            iRetValue = pEnt->v.body; break;
        case EV_INT_effects:         iRetValue = pEnt->v.effects; break;
        case EV_INT_light_level:     iRetValue = pEnt->v.light_level; break;
        case EV_INT_sequence:        iRetValue = pEnt->v.sequence; break;
        case EV_INT_gaitsequence:    iRetValue = pEnt->v.gaitsequence; break;
        case EV_INT_modelindex:      iRetValue = pEnt->v.modelindex; break;
        case EV_INT_playerclass:     iRetValue = pEnt->v.playerclass; break;
        case EV_INT_waterlevel:      iRetValue = pEnt->v.waterlevel; break;
        case EV_INT_watertype:       iRetValue = pEnt->v.watertype; break;
        case EV_INT_spawnflags:      iRetValue = pEnt->v.spawnflags; break;
        case EV_INT_flags:           iRetValue = pEnt->v.flags; break;
        case EV_INT_colormap:        iRetValue = pEnt->v.colormap; break;
        case EV_INT_team:            iRetValue = pEnt->v.team; break;
        case EV_INT_fixangle:        iRetValue = pEnt->v.fixangle; break;
        case EV_INT_weapons:         iRetValue = pEnt->v.weapons; break;
        case EV_INT_rendermode:      iRetValue = pEnt->v.rendermode; break;
        case EV_INT_renderfx:        iRetValue = pEnt->v.renderfx; break;
        case EV_INT_button:          iRetValue = pEnt->v.button; break;
        case EV_INT_impulse:         iRetValue = pEnt->v.impulse; break;
        case EV_INT_deadflag:        iRetValue = pEnt->v.deadflag; break;
        default: return 0;
    }
    return iRetValue;
}

cell AMX_NATIVE_CALL amxx_entity_set_int(AMX *amx, cell *params)
{
    (void)amx;
    int i = (int)params[1];
    if (i <= 0 || i > gpGlobals->maxEntities) return 0;
    edict_t *pEnt = INDEXENT(i);
    if (!pEnt) return 0;

    int idx = (int)params[2];
    int iNewValue = (int)params[3];
    switch (idx) {
        case EV_INT_gamestate:       pEnt->v.gamestate = iNewValue; break;
        case EV_INT_oldbuttons:      pEnt->v.oldbuttons = iNewValue; break;
        case EV_INT_groupinfo:       pEnt->v.groupinfo = iNewValue; break;
        case EV_INT_iuser1:          pEnt->v.iuser1 = iNewValue; break;
        case EV_INT_iuser2:          pEnt->v.iuser2 = iNewValue; break;
        case EV_INT_iuser3:          pEnt->v.iuser3 = iNewValue; break;
        case EV_INT_iuser4:          pEnt->v.iuser4 = iNewValue; break;
        case EV_INT_weaponanim:      pEnt->v.weaponanim = iNewValue; break;
        case EV_INT_pushmsec:        pEnt->v.pushmsec = iNewValue; break;
        case EV_INT_bInDuck:         pEnt->v.bInDuck = iNewValue; break;
        case EV_INT_flTimeStepSound: pEnt->v.flTimeStepSound = iNewValue; break;
        case EV_INT_flSwimTime:      pEnt->v.flSwimTime = iNewValue; break;
        case EV_INT_flDuckTime:      pEnt->v.flDuckTime = iNewValue; break;
        case EV_INT_iStepLeft:       pEnt->v.iStepLeft = iNewValue; break;
        case EV_INT_movetype:        pEnt->v.movetype = iNewValue; break;
        case EV_INT_solid:           pEnt->v.solid = iNewValue; break;
        case EV_INT_skin:            pEnt->v.skin = iNewValue; break;
        case EV_INT_body:            pEnt->v.body = iNewValue; break;
        case EV_INT_effects:         pEnt->v.effects = iNewValue; break;
        case EV_INT_light_level:     pEnt->v.light_level = iNewValue; break;
        case EV_INT_sequence:        pEnt->v.sequence = iNewValue; break;
        case EV_INT_gaitsequence:    pEnt->v.gaitsequence = iNewValue; break;
        case EV_INT_modelindex:      pEnt->v.modelindex = iNewValue; break;
        case EV_INT_playerclass:     pEnt->v.playerclass = iNewValue; break;
        case EV_INT_waterlevel:      pEnt->v.waterlevel = iNewValue; break;
        case EV_INT_watertype:       pEnt->v.watertype = iNewValue; break;
        case EV_INT_spawnflags:      pEnt->v.spawnflags = iNewValue; break;
        case EV_INT_flags:           pEnt->v.flags = iNewValue; break;
        case EV_INT_colormap:        pEnt->v.colormap = iNewValue; break;
        case EV_INT_team:            pEnt->v.team = iNewValue; break;
        case EV_INT_fixangle:        pEnt->v.fixangle = iNewValue; break;
        case EV_INT_weapons:         pEnt->v.weapons = iNewValue; break;
        case EV_INT_rendermode:      pEnt->v.rendermode = iNewValue; break;
        case EV_INT_renderfx:        pEnt->v.renderfx = iNewValue; break;
        case EV_INT_button:          pEnt->v.button = iNewValue; break;
        case EV_INT_impulse:         pEnt->v.impulse = iNewValue; break;
        case EV_INT_deadflag:        pEnt->v.deadflag = iNewValue; break;
        default: return 0;
    }
    return 1;
}

cell AMX_NATIVE_CALL amxx_entity_get_float(AMX *amx, cell *params)
{
    (void)amx;
    int i = (int)params[1];
    if (i <= 0 || i > gpGlobals->maxEntities) return 0;
    edict_t *pEnt = INDEXENT(i);
    if (!pEnt) return 0;

    int idx = (int)params[2];
    float fVal = 0;
    switch (idx) {
        case EV_FL_impacttime:       fVal = pEnt->v.impacttime; break;
        case EV_FL_starttime:        fVal = pEnt->v.starttime; break;
        case EV_FL_idealpitch:       fVal = pEnt->v.idealpitch; break;
        case EV_FL_pitch_speed:      fVal = pEnt->v.pitch_speed; break;
        case EV_FL_ideal_yaw:        fVal = pEnt->v.ideal_yaw; break;
        case EV_FL_yaw_speed:        fVal = pEnt->v.yaw_speed; break;
        case EV_FL_ltime:            fVal = pEnt->v.ltime; break;
        case EV_FL_nextthink:        fVal = pEnt->v.nextthink; break;
        case EV_FL_gravity:          fVal = pEnt->v.gravity; break;
        case EV_FL_friction:         fVal = pEnt->v.friction; break;
        case EV_FL_frame:            fVal = pEnt->v.frame; break;
        case EV_FL_animtime:         fVal = pEnt->v.animtime; break;
        case EV_FL_framerate:        fVal = pEnt->v.framerate; break;
        case EV_FL_health:           fVal = pEnt->v.health; break;
        case EV_FL_frags:            fVal = pEnt->v.frags; break;
        case EV_FL_takedamage:       fVal = pEnt->v.takedamage; break;
        case EV_FL_max_health:       fVal = pEnt->v.max_health; break;
        case EV_FL_teleport_time:    fVal = pEnt->v.teleport_time; break;
        case EV_FL_armortype:        fVal = pEnt->v.armortype; break;
        case EV_FL_armorvalue:       fVal = pEnt->v.armorvalue; break;
        case EV_FL_dmg_take:         fVal = pEnt->v.dmg_take; break;
        case EV_FL_dmg_save:         fVal = pEnt->v.dmg_save; break;
        case EV_FL_dmg:              fVal = pEnt->v.dmg; break;
        case EV_FL_dmgtime:          fVal = pEnt->v.dmgtime; break;
        case EV_FL_speed:            fVal = pEnt->v.speed; break;
        case EV_FL_air_finished:     fVal = pEnt->v.air_finished; break;
        case EV_FL_pain_finished:    fVal = pEnt->v.pain_finished; break;
        case EV_FL_radsuit_finished: fVal = pEnt->v.radsuit_finished; break;
        case EV_FL_scale:            fVal = pEnt->v.scale; break;
        case EV_FL_renderamt:        fVal = pEnt->v.renderamt; break;
        case EV_FL_maxspeed:         fVal = pEnt->v.maxspeed; break;
        case EV_FL_fov:              fVal = pEnt->v.fov; break;
        case EV_FL_flFallVelocity:   fVal = pEnt->v.flFallVelocity; break;
        case EV_FL_fuser1:           fVal = pEnt->v.fuser1; break;
        case EV_FL_fuser2:           fVal = pEnt->v.fuser2; break;
        case EV_FL_fuser3:           fVal = pEnt->v.fuser3; break;
        case EV_FL_fuser4:           fVal = pEnt->v.fuser4; break;
        default: return 0;
    }
    return amx_ftoc(fVal);
}

cell AMX_NATIVE_CALL amxx_entity_set_float(AMX *amx, cell *params)
{
    (void)amx;
    int i = (int)params[1];
    if (i <= 0 || i > gpGlobals->maxEntities) return 0;
    edict_t *pEnt = INDEXENT(i);
    if (!pEnt) return 0;

    int idx = (int)params[2];
    float fVal = amx_ctof(params[3]);
    switch (idx) {
        case EV_FL_impacttime:       pEnt->v.impacttime = fVal; break;
        case EV_FL_starttime:        pEnt->v.starttime = fVal; break;
        case EV_FL_idealpitch:       pEnt->v.idealpitch = fVal; break;
        case EV_FL_pitch_speed:      pEnt->v.pitch_speed = fVal; break;
        case EV_FL_ideal_yaw:        pEnt->v.ideal_yaw = fVal; break;
        case EV_FL_yaw_speed:        pEnt->v.yaw_speed = fVal; break;
        case EV_FL_ltime:            pEnt->v.ltime = fVal; break;
        case EV_FL_nextthink:        pEnt->v.nextthink = fVal; break;
        case EV_FL_gravity:          pEnt->v.gravity = fVal; break;
        case EV_FL_friction:         pEnt->v.friction = fVal; break;
        case EV_FL_frame:            pEnt->v.frame = fVal; break;
        case EV_FL_animtime:         pEnt->v.animtime = fVal; break;
        case EV_FL_framerate:        pEnt->v.framerate = fVal; break;
        case EV_FL_health:           pEnt->v.health = fVal; break;
        case EV_FL_frags:            pEnt->v.frags = fVal; break;
        case EV_FL_takedamage:       pEnt->v.takedamage = fVal; break;
        case EV_FL_max_health:       pEnt->v.max_health = fVal; break;
        case EV_FL_teleport_time:    pEnt->v.teleport_time = fVal; break;
        case EV_FL_armortype:        pEnt->v.armortype = fVal; break;
        case EV_FL_armorvalue:       pEnt->v.armorvalue = fVal; break;
        case EV_FL_dmg_take:         pEnt->v.dmg_take = fVal; break;
        case EV_FL_dmg_save:         pEnt->v.dmg_save = fVal; break;
        case EV_FL_dmg:              pEnt->v.dmg = fVal; break;
        case EV_FL_dmgtime:          pEnt->v.dmgtime = fVal; break;
        case EV_FL_speed:            pEnt->v.speed = fVal; break;
        case EV_FL_air_finished:     pEnt->v.air_finished = fVal; break;
        case EV_FL_pain_finished:    pEnt->v.pain_finished = fVal; break;
        case EV_FL_radsuit_finished: pEnt->v.radsuit_finished = fVal; break;
        case EV_FL_scale:            pEnt->v.scale = fVal; break;
        case EV_FL_renderamt:        pEnt->v.renderamt = fVal; break;
        case EV_FL_maxspeed:         pEnt->v.maxspeed = fVal; break;
        case EV_FL_fov:              pEnt->v.fov = fVal; break;
        case EV_FL_flFallVelocity:   pEnt->v.flFallVelocity = fVal; break;
        case EV_FL_fuser1:           pEnt->v.fuser1 = fVal; break;
        case EV_FL_fuser2:           pEnt->v.fuser2 = fVal; break;
        case EV_FL_fuser3:           pEnt->v.fuser3 = fVal; break;
        case EV_FL_fuser4:           pEnt->v.fuser4 = fVal; break;
        default: return 0;
    }
    return 1;
}

cell AMX_NATIVE_CALL amxx_entity_get_vector(AMX *amx, cell *params)
{
    int i = (int)params[1];
    if (i <= 0 || i > gpGlobals->maxEntities) return 0;
    edict_t *pEnt = INDEXENT(i);
    if (!pEnt) return 0;

    int idx = (int)params[2];
    cell *vRet;
    amx_GetAddr(amx, params[3], &vRet);
    if (!vRet) return 0;

    Vector v(0, 0, 0);
    switch (idx) {
        case EV_VEC_origin:         v = pEnt->v.origin; break;
        case EV_VEC_oldorigin:      v = pEnt->v.oldorigin; break;
        case EV_VEC_velocity:       v = pEnt->v.velocity; break;
        case EV_VEC_basevelocity:   v = pEnt->v.basevelocity; break;
        case EV_VEC_clbasevelocity: v = pEnt->v.clbasevelocity; break;
        case EV_VEC_movedir:        v = pEnt->v.movedir; break;
        case EV_VEC_angles:         v = pEnt->v.angles; break;
        case EV_VEC_avelocity:      v = pEnt->v.avelocity; break;
        case EV_VEC_punchangle:     v = pEnt->v.punchangle; break;
        case EV_VEC_v_angle:        v = pEnt->v.v_angle; break;
        case EV_VEC_endpos:         v = pEnt->v.endpos; break;
        case EV_VEC_startpos:       v = pEnt->v.startpos; break;
        case EV_VEC_absmin:         v = pEnt->v.absmin; break;
        case EV_VEC_absmax:         v = pEnt->v.absmax; break;
        case EV_VEC_mins:           v = pEnt->v.mins; break;
        case EV_VEC_maxs:           v = pEnt->v.maxs; break;
        case EV_VEC_size:           v = pEnt->v.size; break;
        case EV_VEC_rendercolor:    v = pEnt->v.rendercolor; break;
        case EV_VEC_view_ofs:       v = pEnt->v.view_ofs; break;
        case EV_VEC_vuser1:         v = pEnt->v.vuser1; break;
        case EV_VEC_vuser2:         v = pEnt->v.vuser2; break;
        case EV_VEC_vuser3:         v = pEnt->v.vuser3; break;
        case EV_VEC_vuser4:         v = pEnt->v.vuser4; break;
        default: return 0;
    }
    vRet[0] = amx_ftoc(v.x);
    vRet[1] = amx_ftoc(v.y);
    vRet[2] = amx_ftoc(v.z);
    return 1;
}

cell AMX_NATIVE_CALL amxx_entity_set_vector(AMX *amx, cell *params)
{
    int i = (int)params[1];
    if (i <= 0 || i > gpGlobals->maxEntities) return 0;
    edict_t *pEnt = INDEXENT(i);
    if (!pEnt) return 0;

    int idx = (int)params[2];
    cell *vAmx;
    amx_GetAddr(amx, params[3], &vAmx);
    if (!vAmx) return 0;

    Vector v(amx_ctof(vAmx[0]), amx_ctof(vAmx[1]), amx_ctof(vAmx[2]));
    switch (idx) {
        case EV_VEC_origin:         pEnt->v.origin = v; break;
        case EV_VEC_oldorigin:      pEnt->v.oldorigin = v; break;
        case EV_VEC_velocity:       pEnt->v.velocity = v; break;
        case EV_VEC_basevelocity:   pEnt->v.basevelocity = v; break;
        case EV_VEC_clbasevelocity: pEnt->v.clbasevelocity = v; break;
        case EV_VEC_movedir:        pEnt->v.movedir = v; break;
        case EV_VEC_angles:         pEnt->v.angles = v; break;
        case EV_VEC_avelocity:      pEnt->v.avelocity = v; break;
        case EV_VEC_punchangle:     pEnt->v.punchangle = v; break;
        case EV_VEC_v_angle:        pEnt->v.v_angle = v; break;
        case EV_VEC_endpos:         pEnt->v.endpos = v; break;
        case EV_VEC_startpos:       pEnt->v.startpos = v; break;
        case EV_VEC_absmin:         pEnt->v.absmin = v; break;
        case EV_VEC_absmax:         pEnt->v.absmax = v; break;
        case EV_VEC_mins:           pEnt->v.mins = v; break;
        case EV_VEC_maxs:           pEnt->v.maxs = v; break;
        case EV_VEC_size:           pEnt->v.size = v; break;
        case EV_VEC_rendercolor:    pEnt->v.rendercolor = v; break;
        case EV_VEC_view_ofs:       pEnt->v.view_ofs = v; break;
        case EV_VEC_vuser1:         pEnt->v.vuser1 = v; break;
        case EV_VEC_vuser2:         pEnt->v.vuser2 = v; break;
        case EV_VEC_vuser3:         pEnt->v.vuser3 = v; break;
        case EV_VEC_vuser4:         pEnt->v.vuser4 = v; break;
        default: return 0;
    }
    return 1;
}

cell AMX_NATIVE_CALL amxx_entity_get_string(AMX *amx, cell *params)
{
    int i = (int)params[1];
    if (i <= 0 || i > gpGlobals->maxEntities) return 0;
    edict_t *pEnt = INDEXENT(i);
    if (!pEnt) return 0;

    int idx = (int)params[2];
    string_t iszString = 0;
    switch (idx) {
        case EV_SZ_classname:   iszString = pEnt->v.classname; break;
        case EV_SZ_globalname:  iszString = pEnt->v.globalname; break;
        case EV_SZ_model:       iszString = pEnt->v.model; break;
        case EV_SZ_target:      iszString = pEnt->v.target; break;
        case EV_SZ_targetname:  iszString = pEnt->v.targetname; break;
        case EV_SZ_netname:     iszString = pEnt->v.netname; break;
        case EV_SZ_message:     iszString = pEnt->v.message; break;
        case EV_SZ_noise:       iszString = pEnt->v.noise; break;
        case EV_SZ_noise1:      iszString = pEnt->v.noise1; break;
        case EV_SZ_noise2:      iszString = pEnt->v.noise2; break;
        case EV_SZ_noise3:      iszString = pEnt->v.noise3; break;
        case EV_SZ_viewmodel:   iszString = pEnt->v.viewmodel; break;
        case EV_SZ_weaponmodel: iszString = pEnt->v.weaponmodel; break;
        default: return 0;
    }

    const char *szRet = STRING(iszString);
    if (!szRet) szRet = "";
    cell *dest;
    amx_GetAddr(amx, params[3], &dest);
    if (!dest) return 0;
    return amx_SetString(dest, szRet, 0, 0, (int)params[4]);
}

cell AMX_NATIVE_CALL amxx_entity_set_string(AMX *amx, cell *params)
{
    int i = (int)params[1];
    if (i <= 0 || i > gpGlobals->maxEntities) return 0;
    edict_t *pEnt = INDEXENT(i);
    if (!pEnt) return 0;

    int idx = (int)params[2];
    cell *src;
    amx_GetAddr(amx, params[3], &src);
    if (!src) return 0;
    char strbuf[1024];
    amx_GetString(strbuf, src, 0, sizeof(strbuf));
    string_t iszString = ALLOC_STRING(strbuf);

    switch (idx) {
        case EV_SZ_classname:   pEnt->v.classname = iszString; break;
        case EV_SZ_globalname:  pEnt->v.globalname = iszString; break;
        case EV_SZ_model:       pEnt->v.model = iszString; break;
        case EV_SZ_target:      pEnt->v.target = iszString; break;
        case EV_SZ_targetname:  pEnt->v.targetname = iszString; break;
        case EV_SZ_netname:     pEnt->v.netname = iszString; break;
        case EV_SZ_message:     pEnt->v.message = iszString; break;
        case EV_SZ_noise:       pEnt->v.noise = iszString; break;
        case EV_SZ_noise1:      pEnt->v.noise1 = iszString; break;
        case EV_SZ_noise2:      pEnt->v.noise2 = iszString; break;
        case EV_SZ_noise3:      pEnt->v.noise3 = iszString; break;
        case EV_SZ_viewmodel:   pEnt->v.viewmodel = iszString; break;
        case EV_SZ_weaponmodel: pEnt->v.weaponmodel = iszString; break;
        default: return 0;
    }
    return 1;
}

cell AMX_NATIVE_CALL amxx_entity_get_edict(AMX *amx, cell *params)
{
    (void)amx;
    int i = (int)params[1];
    if (i <= 0 || i > gpGlobals->maxEntities) return 0;
    edict_t *pEnt = INDEXENT(i);
    if (!pEnt) return 0;

    int idx = (int)params[2];
    edict_t *pRet = nullptr;
    switch (idx) {
        case EV_ENT_chain:             pRet = pEnt->v.chain; break;
        case EV_ENT_dmg_inflictor:     pRet = pEnt->v.dmg_inflictor; break;
        case EV_ENT_enemy:             pRet = pEnt->v.enemy; break;
        case EV_ENT_aiment:            pRet = pEnt->v.aiment; break;
        case EV_ENT_owner:             pRet = pEnt->v.owner; break;
        case EV_ENT_groundentity:      pRet = pEnt->v.groundentity; break;
        case EV_ENT_pContainingEntity: pRet = pEnt->v.pContainingEntity; break;
        case EV_ENT_euser1:            pRet = pEnt->v.euser1; break;
        case EV_ENT_euser2:            pRet = pEnt->v.euser2; break;
        case EV_ENT_euser3:            pRet = pEnt->v.euser3; break;
        case EV_ENT_euser4:            pRet = pEnt->v.euser4; break;
        default: return 0;
    }
    if (!pRet || pRet->free)
        return 0;
    return ENTINDEX(pRet);
}

// entity_get_edict2(iIndex, iKey) — 与 entity_get_edict 相同，但出错返回 -1
cell AMX_NATIVE_CALL amxx_entity_get_edict2(AMX *amx, cell *params)
{
    (void)amx;
    int i = (int)params[1];
    if (i <= 0 || i > gpGlobals->maxEntities) return -1;
    edict_t *pEnt = INDEXENT(i);
    if (!pEnt) return -1;

    int idx = (int)params[2];
    edict_t *pRet = nullptr;
    switch (idx) {
        case EV_ENT_chain:             pRet = pEnt->v.chain; break;
        case EV_ENT_dmg_inflictor:     pRet = pEnt->v.dmg_inflictor; break;
        case EV_ENT_enemy:             pRet = pEnt->v.enemy; break;
        case EV_ENT_aiment:            pRet = pEnt->v.aiment; break;
        case EV_ENT_owner:             pRet = pEnt->v.owner; break;
        case EV_ENT_groundentity:      pRet = pEnt->v.groundentity; break;
        case EV_ENT_pContainingEntity: pRet = pEnt->v.pContainingEntity; break;
        case EV_ENT_euser1:            pRet = pEnt->v.euser1; break;
        case EV_ENT_euser2:            pRet = pEnt->v.euser2; break;
        case EV_ENT_euser3:            pRet = pEnt->v.euser3; break;
        case EV_ENT_euser4:            pRet = pEnt->v.euser4; break;
        default: return -1;
    }
    if (!pRet || pRet->free)
        return -1;
    return ENTINDEX(pRet);
}

cell AMX_NATIVE_CALL amxx_entity_set_edict(AMX *amx, cell *params)
{
    (void)amx;
    int i = (int)params[1];
    if (i <= 0 || i > gpGlobals->maxEntities) return 0;
    edict_t *pEnt = INDEXENT(i);
    if (!pEnt) return 0;

    int idx = (int)params[2];
    int iSetEnt = (int)params[3];
    edict_t *pSetEnt = (iSetEnt > 0) ? INDEXENT(iSetEnt) : nullptr;

    switch (idx) {
        case EV_ENT_chain:             pEnt->v.chain = pSetEnt; break;
        case EV_ENT_dmg_inflictor:     pEnt->v.dmg_inflictor = pSetEnt; break;
        case EV_ENT_enemy:             pEnt->v.enemy = pSetEnt; break;
        case EV_ENT_aiment:            pEnt->v.aiment = pSetEnt; break;
        case EV_ENT_owner:             pEnt->v.owner = pSetEnt; break;
        case EV_ENT_groundentity:      pEnt->v.groundentity = pSetEnt; break;
        case EV_ENT_pContainingEntity: pEnt->v.pContainingEntity = pSetEnt; break;
        case EV_ENT_euser1:            pEnt->v.euser1 = pSetEnt; break;
        case EV_ENT_euser2:            pEnt->v.euser2 = pSetEnt; break;
        case EV_ENT_euser3:            pEnt->v.euser3 = pSetEnt; break;
        case EV_ENT_euser4:            pEnt->v.euser4 = pSetEnt; break;
        default: return 0;
    }
    return 1;
}

cell AMX_NATIVE_CALL amxx_entity_get_byte(AMX *amx, cell *params)
{
    (void)amx;
    int i = (int)params[1];
    if (i <= 0 || i > gpGlobals->maxEntities) return 0;
    edict_t *pEnt = INDEXENT(i);
    if (!pEnt) return 0;

    int idx = (int)params[2];
    int iRetValue = 0;
    switch (idx) {
        case EV_BYTE_controller1: iRetValue = pEnt->v.controller[0]; break;
        case EV_BYTE_controller2: iRetValue = pEnt->v.controller[1]; break;
        case EV_BYTE_controller3: iRetValue = pEnt->v.controller[2]; break;
        case EV_BYTE_controller4: iRetValue = pEnt->v.controller[3]; break;
        case EV_BYTE_blending1:   iRetValue = pEnt->v.blending[0]; break;
        case EV_BYTE_blending2:   iRetValue = pEnt->v.blending[1]; break;
        default: return 0;
    }
    return iRetValue;
}

cell AMX_NATIVE_CALL amxx_entity_set_byte(AMX *amx, cell *params)
{
    (void)amx;
    int i = (int)params[1];
    if (i <= 0 || i > gpGlobals->maxEntities) return 0;
    edict_t *pEnt = INDEXENT(i);
    if (!pEnt) return 0;

    int idx = (int)params[2];
    int iNewValue = (int)params[3];
    if (iNewValue > 255) iNewValue = 255;
    if (iNewValue < 0) iNewValue = 0;

    switch (idx) {
        case EV_BYTE_controller1: pEnt->v.controller[0] = iNewValue; break;
        case EV_BYTE_controller2: pEnt->v.controller[1] = iNewValue; break;
        case EV_BYTE_controller3: pEnt->v.controller[2] = iNewValue; break;
        case EV_BYTE_controller4: pEnt->v.controller[3] = iNewValue; break;
        case EV_BYTE_blending1:   pEnt->v.blending[0] = iNewValue; break;
        case EV_BYTE_blending2:   pEnt->v.blending[1] = iNewValue; break;
        default: return 0;
    }
    return 1;
}

// ============================================================
// P1-6: Engine 模块补全 —— vector / traceresult / viewcone /
//       point_contents / kvd / sphere / grenade / string-table /
//       impulse 与 hook 注册表
// ============================================================

// 与 amxmodx engine_const.inc 中 TR_* 枚举完全对齐（0..9）
enum {
    ENG_TR_AllSolid = 0,
    ENG_TR_StartSolid,
    ENG_TR_InOpen,
    ENG_TR_InWater,
    ENG_TR_Fraction,
    ENG_TR_EndPos,
    ENG_TR_PlaneDist,
    ENG_TR_PlaneNormal,
    ENG_TR_Hit,
    ENG_TR_HitGroup
};

// get_distance(const origin1[3], const origin2[3]) — 返回两点整数距离
cell AMX_NATIVE_CALL amxx_get_distance(AMX *amx, cell *params)
{
    cell *a, *b;
    amx_GetAddr(amx, params[1], &a);
    amx_GetAddr(amx, params[2], &b);
    if (!a || !b) return 0;
    Vector v1((float)a[0], (float)a[1], (float)a[2]);
    Vector v2((float)b[0], (float)b[1], (float)b[2]);
    return (cell)(int)((v1 - v2).Length());
}

// vector_to_angle(const Float:fVector[3], Float:vReturn[3])
cell AMX_NATIVE_CALL amxx_vector_to_angle(AMX *amx, cell *params)
{
    cell *v, *r;
    amx_GetAddr(amx, params[1], &v);
    amx_GetAddr(amx, params[2], &r);
    if (!v || !r) return 0;
    Vector vec(amx_ctof(v[0]), amx_ctof(v[1]), amx_ctof(v[2]));
    Vector ang;
    VEC_TO_ANGLES(vec, ang);
    r[0] = amx_ftoc(ang.x);
    r[1] = amx_ftoc(ang.y);
    r[2] = amx_ftoc(ang.z);
    return 1;
}

// traceresult(type, any:...) — 从 g_lastTrace 读取最近一次 trace_* 结果
cell AMX_NATIVE_CALL amxx_traceresult(AMX *amx, cell *params)
{
    int type = (int)params[1];
    TraceResult *tr = &g_lastTrace;
    cell *c = nullptr;
    switch (type) {
        case ENG_TR_AllSolid:   return tr->fAllSolid;
        case ENG_TR_StartSolid: return tr->fStartSolid;
        case ENG_TR_InOpen:     return tr->fInOpen;
        case ENG_TR_InWater:    return tr->fInWater;
        case ENG_TR_HitGroup:   return tr->iHitgroup;
        case ENG_TR_Hit:
            if (!tr->pHit || FNullEnt(tr->pHit)) return -1;
            return ENTINDEX(tr->pHit);
        case ENG_TR_Fraction:
            amx_GetAddr(amx, params[2], &c);
            if (c) *c = amx_ftoc(tr->flFraction);
            return 1;
        case ENG_TR_EndPos:
            amx_GetAddr(amx, params[2], &c);
            if (c) { c[0] = amx_ftoc(tr->vecEndPos.x); c[1] = amx_ftoc(tr->vecEndPos.y); c[2] = amx_ftoc(tr->vecEndPos.z); }
            return 1;
        case ENG_TR_PlaneDist:
            amx_GetAddr(amx, params[2], &c);
            if (c) *c = amx_ftoc(tr->flPlaneDist);
            return 1;
        case ENG_TR_PlaneNormal:
            amx_GetAddr(amx, params[2], &c);
            if (c) { c[0] = amx_ftoc(tr->vecPlaneNormal.x); c[1] = amx_ftoc(tr->vecPlaneNormal.y); c[2] = amx_ftoc(tr->vecPlaneNormal.z); }
            return 1;
    }
    return 0;
}

// point_contents(const Float:fCheckAt[3])
cell AMX_NATIVE_CALL amxx_point_contents(AMX *amx, cell *params)
{
    cell *a; amx_GetAddr(amx, params[1], &a);
    if (!a) return 0;
    Vector pos(amx_ctof(a[0]), amx_ctof(a[1]), amx_ctof(a[2]));
    if (g_engfuncs.pfnPointContents) return (cell)g_engfuncs.pfnPointContents(pos);
    return 0;
}

// is_in_viewcone(entity, const Float:origin[3], use3d = 0)
// 与原版 engine 模块 SDK 派生实现一致：用 fov/2 弧度做点积比较
cell AMX_NATIVE_CALL amxx_is_in_viewcone(AMX *amx, cell *params)
{
    int src = (int)params[1];
    if (src <= 0 || src > gpGlobals->maxEntities) return 0;
    edict_t *pEdict = INDEXENT(src);
    if (!pEdict || pEdict->free) return 0;
    cell *addr; amx_GetAddr(amx, params[2], &addr);
    if (!addr) return 0;
    Vector origin(amx_ctof(addr[0]), amx_ctof(addr[1]), amx_ctof(addr[2]));

    int numArgs = (int)params[0] / (int)sizeof(cell);
    bool use2D = (numArgs < 3) || ((int)params[3] == 0);

    Vector vecLOS, vecForward;
    if (use2D) {
        MAKE_VECTORS(pEdict->v.angles);
        vecForward = gpGlobals->v_forward;
        vecLOS = origin - pEdict->v.origin;
        vecForward.z = 0;
        vecLOS.z = 0;
    } else {
        MAKE_VECTORS(pEdict->v.v_angle);
        vecForward = gpGlobals->v_forward;
        vecLOS = origin - (pEdict->v.origin + pEdict->v.view_ofs);
    }
    vecLOS = vecLOS.Normalize();
    float flDot = DotProduct(vecLOS, vecForward);
    float fov = pEdict->v.fov;
    if (fov <= 0) fov = 90.0f;
    return (flDot >= cosf(fov * (float)(M_PI / 360.0))) ? 1 : 0;
}

// copy_keyvalue(szClassName[], sizea, szKeyName[], sizeb, szValue[], sizec)
// 仅在 pfn_keyvalue 分发期间有效，否则返回 0
cell AMX_NATIVE_CALL amxx_copy_keyvalue(AMX *amx, cell *params)
{
    if (!g_inKeyValue) return 0;
    cell *c;
    amx_GetAddr(amx, params[1], &c);
    if (c) amx_SetString(c, g_currentKvd.szClassName ? g_currentKvd.szClassName : "", 0, 0, (int)params[2]);
    amx_GetAddr(amx, params[3], &c);
    if (c) amx_SetString(c, g_currentKvd.szKeyName ? g_currentKvd.szKeyName : "", 0, 0, (int)params[4]);
    amx_GetAddr(amx, params[5], &c);
    if (c) amx_SetString(c, g_currentKvd.szValue ? g_currentKvd.szValue : "", 0, 0, (int)params[6]);
    return 1;
}

// find_sphere_class(aroundent, const classname[], Float:radius, entlist[], maxents, const Float:origin[3])
// 返回写入 entlist 的实体数量
cell AMX_NATIVE_CALL amxx_find_sphere_class(AMX *amx, cell *params)
{
    cell *clsAddr; amx_GetAddr(amx, params[2], &clsAddr);
    char classToFind[128];
    amx_GetString(classToFind, clsAddr, 0, sizeof(classToFind));
    float radius = amx_ctof(params[3]);
    cell *entList; amx_GetAddr(amx, params[4], &entList);
    int maxents = (int)params[5];

    Vector vecOrigin;
    if ((int)params[1] > 0) {
        edict_t *pEntity = INDEXENT((int)params[1]);
        if (!pEntity || pEntity->free) return 0;
        vecOrigin = pEntity->v.origin;
    } else {
        cell *o; amx_GetAddr(amx, params[6], &o);
        if (o) vecOrigin = Vector(amx_ctof(o[0]), amx_ctof(o[1]), amx_ctof(o[2]));
    }

    int entsFound = 0;
    edict_t *pSearchEnt = nullptr;
    while (entsFound < maxents) {
        pSearchEnt = FIND_ENTITY_IN_SPHERE(pSearchEnt, vecOrigin, radius);
        if (FNullEnt(pSearchEnt)) break;
        if (strcmp(STRING(pSearchEnt->v.classname), classToFind) == 0)
            entList[entsFound++] = ENTINDEX(pSearchEnt);
    }
    return entsFound;
}

// get_grenade_id(id, model[], len, grenadeid = 0)
cell AMX_NATIVE_CALL amxx_get_grenade_id(AMX *amx, cell *params)
{
    int owner = (int)params[1];
    if (owner <= 0 || owner > gpGlobals->maxEntities) return 0;
    edict_t *pentOwner = INDEXENT(owner);
    if (!pentOwner) return 0;

    edict_t *pentFind = ((int)params[4] > 0) ? INDEXENT((int)params[4]) : nullptr;
    if (pentFind && pentFind->free) pentFind = nullptr;

    pentFind = FIND_ENTITY_BY_CLASSNAME(pentFind, "grenade");
    while (!FNullEnt(pentFind)) {
        if (pentFind->v.owner == pentOwner) {
            if ((int)params[3] > 0) {
                const char *szModel = STRING(pentFind->v.model);
                if (!szModel) szModel = "";
                cell *m; amx_GetAddr(amx, params[2], &m);
                if (m) amx_SetString(m, szModel, 0, 0, (int)params[3]);
            }
            return ENTINDEX(pentFind);
        }
        pentFind = FIND_ENTITY_BY_CLASSNAME(pentFind, "grenade");
    }
    return 0;
}

// eng_get_string(_string, _returnString[], _len) — 从引擎字符串表读取
cell AMX_NATIVE_CALL amxx_eng_get_string(AMX *amx, cell *params)
{
    const char *str = nullptr;
    if (g_engfuncs.pfnSzFromIndex) str = g_engfuncs.pfnSzFromIndex((int)params[1]);
    else str = STRING(params[1]);
    if (!str) str = "";
    cell *dest; amx_GetAddr(amx, params[2], &dest);
    return amx_SetString(dest, str, 0, 0, (int)params[3]);
}

// register_impulse(impulse, const function[]) — 返回注册 id
cell AMX_NATIVE_CALL amxx_register_impulse(AMX *amx, cell *params)
{
    int impulse = (int)params[1];
    cell *fnAddr; amx_GetAddr(amx, params[2], &fnAddr);
    char funcname[64]; amx_GetString(funcname, fnAddr, 0, sizeof(funcname));
    int funcidx;
    if (amx_FindPublic(amx, funcname, &funcidx) != AMX_ERR_NONE) return 0;
    ImpulseReg r;
    r.id = s_nextHookId++;
    r.impulse = impulse;
    r.amx = amx;
    r.funcidx = funcidx;
    g_impulseRegs.push_back(r);
    return r.id;
}

// unregister_impulse(registerid)
cell AMX_NATIVE_CALL amxx_unregister_impulse(AMX *amx, cell *params)
{
    (void)amx;
    int id = (int)params[1];
    for (size_t i = 0; i < g_impulseRegs.size(); ++i) {
        if (g_impulseRegs[i].id == id) {
            g_impulseRegs.erase(g_impulseRegs.begin() + i);
            return 1;
        }
    }
    return 0;
}

// unregister_think(registerid)
cell AMX_NATIVE_CALL amxx_unregister_think(AMX *amx, cell *params)
{
    (void)amx;
    int id = (int)params[1];
    for (size_t i = 0; i < g_thinkRegs.size(); ++i) {
        if (g_thinkRegs[i].id == id) {
            AMXXThinkTouch::GetInstance().UnregisterThink(g_thinkRegs[i].classname);
            g_thinkRegs.erase(g_thinkRegs.begin() + i);
            return 1;
        }
    }
    return 0;
}

// unregister_touch(registerid)
cell AMX_NATIVE_CALL amxx_unregister_touch(AMX *amx, cell *params)
{
    (void)amx;
    int id = (int)params[1];
    for (size_t i = 0; i < g_touchRegs.size(); ++i) {
        if (g_touchRegs[i].id == id) {
            AMXXThinkTouch::GetInstance().UnregisterTouch(g_touchRegs[i].toucher, g_touchRegs[i].touched);
            g_touchRegs.erase(g_touchRegs.begin() + i);
            return 1;
        }
    }
    return 0;
}

AMX_NATIVE_INFO entity_natives[] = {
    {"pev", amxx_pev},
    {"set_pev", amxx_set_pev},
    {"engfunc", amxx_engfunc},
    {"dllfunc", amxx_dllfunc},
    {"global_get", amxx_global_get},
    {"global_set", amxx_global_set},
    {"register_think", amxx_register_think},
    {"register_touch", amxx_register_touch},
    {"pev_valid", amxx_pev_valid},
    {"str_pev", amxx_str_pev},
    {"set_pev_string", amxx_set_pev_string},
    {"DispatchSpawn", amxx_dispatch_spawn},
    {"DispatchKeyValue", amxx_dispatch_keyvalue},
    {"find_ent_by_owner", amxx_find_ent_by_owner},
    {"find_ent_by_target", amxx_find_ent_by_target},
    {"entity_range", amxx_entity_range},
    {"RegisterHam", amxx_register_ham},
    {"ExecuteHam", amxx_execute_ham},
    {"ExecuteHamB", amxx_execute_ham_b},
    // P0: Ham 鏁版嵁璇诲啓
    {"get_ham_data_int", amxx_get_ham_data_int},
    {"set_ham_data_int", amxx_set_ham_data_int},
    {"get_ham_data_float", amxx_get_ham_data_float},
    {"set_ham_data_float", amxx_set_ham_data_float},
    {"RegisterHamFromEntity", amxx_register_ham_from_entity},
    {"RegisterHamFromPlayer", amxx_register_ham_from_player},
    {"register_forwardex", amxx_register_forwardex},
    {"unregister_forwardex", amxx_unregister_forwardex},
    {"create_forwardex", amxx_create_forwardex},
    // P1: pdata 私有数据读写绉佹湁鏁版嵁璇诲啓
    {"get_pdata_int", amxx_get_pdata_int},
    {"set_pdata_int", amxx_set_pdata_int},
    {"get_pdata_float", amxx_get_pdata_float},
    {"set_pdata_float", amxx_set_pdata_float},
    {"get_pdata_string", amxx_get_pdata_string},
    {"set_pdata_string", amxx_set_pdata_string},
    // P1: pdata CBase 访问
    {"get_pdata_cbase", amxx_get_pdata_cbase},
    // P1: Ham 参数设置
    {"SetHamParamFloat", amxx_SetHamParamFloat},
    {"SetHamParamInt", amxx_SetHamParamInt},
    {"SetHamParamInteger", amxx_SetHamParamInt},
    {"SetHamParamEntity", amxx_SetHamParamEntity},
    {"SetHamParamVector", amxx_SetHamParamVector},
    {"SetHamParamString", amxx_SetHamParamString},
    {"set_cd", amxx_set_cd},
    {"EnableHamForward", amxx_EnableHamForward},
    {"DisableHamForward", amxx_DisableHamForward},
    {"IsHamValid", amxx_IsHamValid},
    {"lookup_sequence", amxx_lookup_sequence},
    // P1: 引擎 Forward 注册
    {"register_forward", amxx_register_forward},
    {"unregister_forward", amxx_unregister_forward},
    // P2: 实体查找扩展
    {"find_ent_by_class", amxx_find_ent_by_class},
    {"find_ent_by_model", amxx_find_ent_by_model},
    {"find_ent_in_sphere", amxx_find_ent_in_sphere},
    // P3: trace 扩展
    {"trace_texture", amxx_trace_texture},
    {"trace_model", amxx_trace_model},
    {"trace_normal", amxx_trace_normal},
    // P3: 固体实体
    {"get_brush_entity", amxx_get_brush_entity},
    // P3: 向量运算
    {"vec_add", amxx_vec_add},
    {"vec_sub", amxx_vec_sub},
    {"vec_length", amxx_vec_length},
    {"vec_distance", amxx_vec_distance},
    {"vec_normalize", amxx_vec_normalize},
    // vector.inc 标准名 (与 vec_distance 同实现, 接受 Float:v1[3]/Float:v2[3])
    {"vector_distance", amxx_vec_distance},
    {"vec_angle", amxx_vec_angle},
    {"vec_dot_product", amxx_vec_dot_product},
    {"vec_cross", amxx_vec_cross},
    {"vec_reflect", amxx_vec_reflect},
    {"vec_mul", amxx_vec_mul},
    {"vec_div", amxx_vec_div},

    // ============== P1-2: Engine 模块补全 ==============
    {"get_global_float", amxx_get_global_float},
    {"get_global_int", amxx_get_global_int},
    {"get_global_string", amxx_get_global_string},
    {"get_global_vector", amxx_get_global_vector},
    {"get_global_edict", amxx_get_global_edict},
    {"get_global_edict2", amxx_get_global_edict2},
    {"find_ent_by_tname", amxx_find_ent_by_tname},
    {"attach_view", amxx_attach_view},
    {"set_view", amxx_set_view},
    {"playback_event", amxx_playback_event},
    {"get_usercmd", amxx_get_usercmd},
    {"set_usercmd", amxx_set_usercmd},
    {"fake_touch", amxx_fake_touch},
    {"force_use", amxx_force_use},
    {"set_lights", amxx_set_lights},
    {"create_entity", amxx_create_entity},
    {"remove_entity", amxx_remove_entity},
    {"is_valid_ent", amxx_is_valid_ent},
    {"entity_count", amxx_entity_count},
    {"entity_set_size", amxx_entity_set_size},
    {"entity_set_origin", amxx_entity_set_origin},
    {"entity_set_model", amxx_entity_set_model},
    // P1-5: Engine module entity_get_*/entity_set_* natives
    {"entity_get_int", amxx_entity_get_int},
    {"entity_set_int", amxx_entity_set_int},
    {"entity_get_float", amxx_entity_get_float},
    {"entity_set_float", amxx_entity_set_float},
    {"entity_get_vector", amxx_entity_get_vector},
    {"entity_set_vector", amxx_entity_set_vector},
    {"entity_get_string", amxx_entity_get_string},
    {"entity_set_string", amxx_entity_set_string},
    {"entity_get_edict", amxx_entity_get_edict},
    {"entity_set_edict", amxx_entity_set_edict},
    {"entity_get_byte", amxx_entity_get_byte},
    {"entity_set_byte", amxx_entity_set_byte},
    {"set_ent_rendering", amxx_set_ent_rendering},
    {"drop_to_floor", amxx_drop_to_floor},
    {"set_speak", amxx_set_speak},
    {"get_speak", amxx_get_speak},
    {"halflife_time", amxx_halflife_time},
    {"radius_damage", amxx_radius_damage},
    {"trace_line", amxx_trace_line},
    {"entity_intersects", amxx_entity_intersects},
    {"is_visible", amxx_is_visible},
    {"trace_hull", amxx_trace_hull},
    {"get_decal_index", amxx_get_decal_index},
    {"get_info_keybuffer", amxx_get_info_keybuffer},
    {"call_think", amxx_call_think},
    {"get_keyvalue", amxx_get_keyvalue},

    // ============== P1-6: Engine 模块补全（vector / trace / viewcone / sphere 等）==============
    {"get_distance", amxx_get_distance},
    {"vector_to_angle", amxx_vector_to_angle},
    {"traceresult", amxx_traceresult},
    {"point_contents", amxx_point_contents},
    {"is_in_viewcone", amxx_is_in_viewcone},
    {"copy_keyvalue", amxx_copy_keyvalue},
    {"find_sphere_class", amxx_find_sphere_class},
    {"get_grenade_id", amxx_get_grenade_id},
    {"eng_get_string", amxx_eng_get_string},
    {"register_impulse", amxx_register_impulse},
    {"unregister_impulse", amxx_unregister_impulse},
    {"unregister_think", amxx_unregister_think},
    {"unregister_touch", amxx_unregister_touch},

    // ============== P1-3: Ham GetHamReturn_* 系列 ==============
    {"GetHamReturnStatus", amxx_GetHamReturnStatus},
    {"GetHamReturnInt", amxx_GetHamReturnInt},
    {"GetHamReturnFloat", amxx_GetHamReturnFloat},
    {"GetHamReturnVector", amxx_GetHamReturnVector},
    {"GetHamReturnEntity", amxx_GetHamReturnEntity},
    {"GetHamReturnString", amxx_GetHamReturnString},

    // ============== P1-4: Fakemeta KeyValue 句柄 API ==============
    {"create_kvd", amxx_create_kvd},
    {"free_kvd", amxx_free_kvd},
    {"get_kvd", amxx_get_kvd},

    // ============== P1-7: Fakemeta pdata 扩展与其他缺失 natives ==============
    {"get_pdata_ent", amxx_get_pdata_ent},
    {"set_pdata_ent", amxx_set_pdata_ent},
    {"get_pdata_bool", amxx_get_pdata_bool},
    {"set_pdata_bool", amxx_set_pdata_bool},
    {"get_pdata_byte", amxx_get_pdata_byte},
    {"set_pdata_byte", amxx_set_pdata_byte},
    {"get_pdata_short", amxx_get_pdata_short},
    {"set_pdata_short", amxx_set_pdata_short},
    {"get_pdata_vector", amxx_get_pdata_vector},
    {"set_pdata_vector", amxx_set_pdata_vector},
    {"get_pdata_ehandle", amxx_get_pdata_ehandle},
    {"set_pdata_ehandle", amxx_set_pdata_ehandle},
    {"pev_serial", amxx_pev_serial},
    {"forward_return", amxx_forward_return},
    {"get_tr", amxx_get_tr},
    {"set_tr", amxx_set_tr},
    {"get_cd", amxx_get_cd},
    {"get_es", amxx_get_es},
    {"set_es", amxx_set_es},
    {"get_uc", amxx_get_uc},
    {"set_uc", amxx_set_uc},
    {"copy_infokey_buffer", amxx_copy_infokey_buffer},
    {"set_controller", amxx_set_controller},
    {"GetModelBoundingBox", amxx_GetModelBoundingBox},
    {"trace_forward", amxx_trace_forward},
    {"entity_get_edict2", amxx_entity_get_edict2},

    // ===== Ham Sandwich 补全 natives =====
    {"SetHamParamEntity2", amxx_SetHamParamEntity2},
    {"SetHamParamTraceResult", amxx_SetHamParamTraceResult},
    {"SetHamParamItemInfo", amxx_SetHamParamItemInfo},
    {"GetHamItemInfo", amxx_GetHamItemInfo},
    {"SetHamItemInfo", amxx_SetHamItemInfo},
    {"CreateHamItemInfo", amxx_CreateHamItemInfo},
    {"FreeHamItemInfo", amxx_FreeHamItemInfo},
    {"GetOrigHamReturnInteger", amxx_GetOrigHamReturnInteger},
    {"GetOrigHamReturnFloat", amxx_GetOrigHamReturnFloat},
    {"GetOrigHamReturnVector", amxx_GetOrigHamReturnVector},
    {"GetOrigHamReturnEntity", amxx_GetOrigHamReturnEntity},
    {"GetOrigHamReturnString", amxx_GetOrigHamReturnString},
    {"SetHamReturnInteger", amxx_SetHamReturnInteger},
    {"SetHamReturnFloat", amxx_SetHamReturnFloat},
    {"SetHamReturnVector", amxx_SetHamReturnVector},
    {"SetHamReturnEntity", amxx_SetHamReturnEntity},
    {"SetHamReturnString", amxx_SetHamReturnString},
    {"GetHamReturnInteger", amxx_GetHamReturnInteger},
    {"set_pdata_cbase", amxx_set_pdata_cbase},
    {"get_pdata_cbase_safe", amxx_get_pdata_cbase_safe},

    {nullptr, nullptr}
};

// ========== Fakemeta Forward 触发 ==========
void FireFMForward(int fmType, int playerIdx)
{
    for (auto &hook : g_forwardHooks) {
        if (hook.type != fmType)
            continue;
        AMX *amx = hook.amx;
        if (!amx || hook.funcidx < 0)
            continue;
        amx_Push(amx, playerIdx);
        cell retval;
        amx_Exec(amx, &retval, hook.funcidx);
    }
}

// P1-9: 常见引擎事件 Fakemeta forward 触发。
// 与 FireFMForward 不同，这些事件需要按引擎函数签名传递实体/字符串参数，
// 并尊重插件的 PLUGIN_HANDLED+ 返回值以决定是否跳过引擎原始函数。
// 返回非 0 表示有插件 supercede ，调用方应跳过原始函数。
int FireFMForwardSpawn(int entIdx)
{
    for (auto &hook : g_forwardHooks) {
        if (hook.type != FM_Spawn)
            continue;
        AMX *amx = hook.amx;
        if (!amx || hook.funcidx < 0)
            continue;
        amx_Push(amx, entIdx);
        cell retval;
        amx_Exec(amx, &retval, hook.funcidx);
        if ((int)retval >= PLUGIN_HANDLED)
            return 1;
    }
    return 0;
}

int FireFMForwardThink(int entIdx)
{
    for (auto &hook : g_forwardHooks) {
        if (hook.type != FM_Think)
            continue;
        AMX *amx = hook.amx;
        if (!amx || hook.funcidx < 0)
            continue;
        amx_Push(amx, entIdx);
        cell retval;
        amx_Exec(amx, &retval, hook.funcidx);
        if ((int)retval >= PLUGIN_HANDLED)
            return 1;
    }
    return 0;
}

// 引擎签名: pfnTouch(pentTouched, pentOther)
// 插件回调签名: forward_touch(touched, other)
int FireFMForwardTouch(int touchedIdx, int otherIdx)
{
    for (auto &hook : g_forwardHooks) {
        if (hook.type != FM_Touch)
            continue;
        AMX *amx = hook.amx;
        if (!amx || hook.funcidx < 0)
            continue;
        // 逆序 push：other 先，touched 后（最后 push = 第一个参数）
        amx_Push(amx, otherIdx);
        amx_Push(amx, touchedIdx);
        cell retval;
        amx_Exec(amx, &retval, hook.funcidx);
        if ((int)retval >= PLUGIN_HANDLED)
            return 1;
    }
    return 0;
}

// 引擎签名: pfnSetModel(edict, const char *model)
// 插件回调签名: forward_setmodel(ent, const model[])
int FireFMForwardSetModel(int entIdx, const char *model)
{
    for (auto &hook : g_forwardHooks) {
        if (hook.type != FM_SetModel)
            continue;
        AMX *amx = hook.amx;
        if (!amx || hook.funcidx < 0)
            continue;
        // 逆序 push：字符串先，ent 后（最后 push = 第一个参数）
        cell amx_addr;
        cell *phys_addr;
        amx_PushString(amx, &amx_addr, &phys_addr, model ? model : "", 0, 0);
        amx_Push(amx, entIdx);
        cell retval;
        amx_Exec(amx, &retval, hook.funcidx);
        amx_Release(amx, amx_addr);
        if ((int)retval >= PLUGIN_HANDLED)
            return 1;
    }
    return 0;
}

// ===== CD/ES/UC Forward 触发 =====
// 在 ReGameDLL 的 UpdateClientData/AddToFullPack/CmdStart 入口处调用，
// 将 struct 指针作为 cell 传给插件回调。插件可用 get_cd/set_cd 等读写数据。
// 返回非 0 表示有插件返回 FMRES_SUPERCEDE（4），调用方应跳过原始函数体。

// FM_UpdateClientData(ent, sendweapons, cd_handle)
int FireFMForwardUpdateClientData(int entIdx, int sendweapons, clientdata_t *cd)
{
    g_amxx_current_cd = cd;
    int supercede = 0;
    cell cd_handle = reinterpret_cast<cell>(cd);
    for (auto &hook : g_forwardHooks) {
        if (hook.type != FM_UpdateClientData)
            continue;
        AMX *amx = hook.amx;
        if (!amx || hook.funcidx < 0)
            continue;
        amx_Push(amx, cd_handle);
        amx_Push(amx, sendweapons);
        amx_Push(amx, entIdx);
        cell retval;
        amx_Exec(amx, &retval, hook.funcidx);
        if ((int)retval == 4) // FMRES_SUPERCEDE
            supercede = 1;
    }
    g_amxx_current_cd = nullptr;
    return supercede;
}

// FM_AddToFullPack(es_handle, e, ent, host, hostflags, player, pSet)
int FireFMForwardAddToFullPack(entity_state_t *state, int e, int entIdx, int hostIdx, int hostflags, int player, unsigned char *pSet)
{
    g_amxx_current_es = state;
    int supercede = 0;
    cell es_handle = reinterpret_cast<cell>(state);
    for (auto &hook : g_forwardHooks) {
        if (hook.type != FM_AddToFullPack)
            continue;
        AMX *amx = hook.amx;
        if (!amx || hook.funcidx < 0)
            continue;
        amx_Push(amx, reinterpret_cast<cell>(pSet));
        amx_Push(amx, player);
        amx_Push(amx, hostflags);
        amx_Push(amx, hostIdx);
        amx_Push(amx, entIdx);
        amx_Push(amx, e);
        amx_Push(amx, es_handle);
        cell retval;
        amx_Exec(amx, &retval, hook.funcidx);
        if ((int)retval == 4) // FMRES_SUPERCEDE
            supercede = 1;
    }
    g_amxx_current_es = nullptr;
    return supercede;
}

// FM_CmdStart(player, uc_handle, random_seed)
int FireFMForwardCmdStart(int playerIdx, usercmd_t *cmd, unsigned int random_seed)
{
    g_amxx_current_uc = cmd;
    int supercede = 0;
    cell uc_handle = reinterpret_cast<cell>(cmd);
    for (auto &hook : g_forwardHooks) {
        if (hook.type != FM_CmdStart)
            continue;
        AMX *amx = hook.amx;
        if (!amx || hook.funcidx < 0)
            continue;
        amx_Push(amx, (cell)random_seed);
        amx_Push(amx, uc_handle);
        amx_Push(amx, playerIdx);
        cell retval;
        amx_Exec(amx, &retval, hook.funcidx);
        if ((int)retval == 4) // FMRES_SUPERCEDE
            supercede = 1;
    }
    g_amxx_current_uc = nullptr;
    return supercede;
}

void RegisterEntityNatives(AMX *amx)
{
    amx_Register(amx, entity_natives, -1);
}

// 跨地图清理 native_entities 全局状态（KVD 池等）
void ResetEntityGlobals()
{
    s_kvdPool.clear();
    s_kvdNextHandle = 1;
    // P1-6: 清理 engine 模块 hook 注册表
    g_impulseRegs.clear();
    g_thinkRegs.clear();
    g_touchRegs.clear();
    s_nextHookId = 1;
    g_inKeyValue = false;
}
