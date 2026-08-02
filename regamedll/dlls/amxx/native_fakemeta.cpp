#include "precompiled.h"
#include "native_fakemeta.h"
#include "native_entities.h"
#include "native_messages.h"
#include "amx.h"
#include "../extdll.h"
#include "../enginecallback.h"
#include "../util.h"
#include "../player.h"
#include "../cbase.h"
#include "../weapons.h"
#include "../weapontype.h"
#include "../gamerules.h"

#include <cstdio>
#include <cmath>
#include <cstdarg>
#include <cstdlib>
#include <vector>

// TraceResult storage for fakemeta
struct FakeMetaTrace {
    TraceResult tr;
};
static std::vector<FakeMetaTrace*> g_traces;

// 供 engfunc 等跨模块使用：把 tr2 句柄（create_tr2 返回值, 1 起）解析为 TraceResult*
// handle <= 0 或无效时返回 nullptr（调用方应回退到全局 g_lastTrace）
TraceResult *FM_GetTraceResult(int handle)
{
    if (handle <= 0)
        return nullptr;
    int idx = handle - 1;
    if (idx < 0 || idx >= (int)g_traces.size() || !g_traces[idx])
        return nullptr;
    return &g_traces[idx]->tr;
}

// Helper: Get player by index
static CBasePlayer *FM_GetPlayer(int index)
{
    if (index < 1 || index > gpGlobals->maxClients)
        return nullptr;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return nullptr;
    return GET_PRIVATE<CBasePlayer>(pEdict);
}

// ===== Entity creation/lookup (wrappers around engfunc) =====

// fm_create_entity(classname[]) - returns entity index
cell AMX_NATIVE_CALL amxx_fm_create_entity(AMX *amx, cell *params)
{
    cell *classname_addr;
    amx_GetAddr(amx, params[1], &classname_addr);
    char classname[256];
    amx_GetString(classname, classname_addr, 0, sizeof(classname));
    edict_t *pEdict = CREATE_NAMED_ENTITY(MAKE_STRING(classname));
    return pEdict ? ENTINDEX(pEdict) : 0;
}

// fm_find_entity_by_class(startEnt, classname[])
cell AMX_NATIVE_CALL amxx_fm_find_ent_by_class(AMX *amx, cell *params)
{
    int startEnt = (int)params[1];
    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    char classname[256];
    amx_GetString(classname, addr, 0, sizeof(classname));
    edict_t *pStart = (startEnt > 0) ? INDEXENT(startEnt) : nullptr;
    edict_t *pEdict = FIND_ENTITY_BY_STRING(pStart, "classname", classname);
    return pEdict ? ENTINDEX(pEdict) : 0;
}

// fm_find_entity_by_model(startEnt, model[])
cell AMX_NATIVE_CALL amxx_fm_find_ent_by_model(AMX *amx, cell *params)
{
    int startEnt = (int)params[1];
    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    char model[256];
    amx_GetString(model, addr, 0, sizeof(model));
    edict_t *pStart = (startEnt > 0) ? INDEXENT(startEnt) : nullptr;
    edict_t *pEdict = FIND_ENTITY_BY_STRING(pStart, "model", model);
    return pEdict ? ENTINDEX(pEdict) : 0;
}

// fm_find_entity_by_target(startEnt, targetname[])
cell AMX_NATIVE_CALL amxx_fm_find_ent_by_target(AMX *amx, cell *params)
{
    int startEnt = (int)params[1];
    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    char target[256];
    amx_GetString(target, addr, 0, sizeof(target));
    edict_t *pStart = (startEnt > 0) ? INDEXENT(startEnt) : nullptr;
    edict_t *pEdict = FIND_ENTITY_BY_STRING(pStart, "targetname", target);
    return pEdict ? ENTINDEX(pEdict) : 0;
}

// fm_find_entity_by_owner(startEnt, classname[], owner)
cell AMX_NATIVE_CALL amxx_fm_find_ent_by_owner(AMX *amx, cell *params)
{
    int startEnt = (int)params[1];
    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    char classname[256];
    amx_GetString(classname, addr, 0, sizeof(classname));
    int ownerEnt = (int)params[3];

    edict_t *pStart = (startEnt > 0) ? INDEXENT(startEnt) : nullptr;
    int startIdx = (startEnt > 0) ? startEnt + 1 : 0;
    if (startIdx < 0) startIdx = 0;

    for (int i = startIdx; i <= gpGlobals->maxEntities; i++) {
        edict_t *pEnt = INDEXENT(i);
        if (!pEnt || pEnt->free) continue;
        if (classname[0]) {
            const char *entClass = STRING(pEnt->v.classname);
            if (!entClass || strcmp(entClass, classname) != 0) continue;
        }
        if (pEnt->v.owner == INDEXENT(ownerEnt))
            return i;
    }
    return 0;
}

// fm_find_entity_in_sphere(startEnt, origin, radius)
cell AMX_NATIVE_CALL amxx_fm_find_ent_in_sphere(AMX *amx, cell *params)
{
    int startEnt = (int)params[1];
    Vector pos;
    pos.x = amx_ctof(params[2]);
    pos.y = amx_ctof(params[3]);
    pos.z = amx_ctof(params[4]);
    float radius = amx_ctof(params[5]);

    edict_t *pStart = (startEnt > 0) ? INDEXENT(startEnt) : nullptr;
    edict_t *pEnt = FIND_ENTITY_IN_SPHERE(pStart, pos, radius);
    return pEnt ? ENTINDEX(pEnt) : 0;
}

// ===== Entity manipulation =====

// fm_give_item(index, item[]) - alias wrapper
cell AMX_NATIVE_CALL amxx_fm_give_item(AMX *amx, cell *params)
{
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free || !pEdict->pvPrivateData)
        return 0;

    cell *item_addr;
    amx_GetAddr(amx, params[2], &item_addr);
    char item[64];
    amx_GetString(item, item_addr, 0, sizeof(item));

    CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
    if (!pPlayer) return 0;

    CBaseEntity *pEntity = pPlayer->GiveNamedItem(item);
    return pEntity ? ENTINDEX(pEntity->edict()) : 0;
}

// fm_get_aim_entity(index, [&body, [&dist]])
cell AMX_NATIVE_CALL amxx_fm_get_aim_entity(AMX *amx, cell *params)
{
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict) return 0;

    TraceResult tr;
    Vector vecSrc = pEdict->v.origin + pEdict->v.view_ofs;
    MAKE_VECTORS(pEdict->v.v_angle);
    Vector vecEnd = vecSrc + gpGlobals->v_forward * 8192.0f;
    TRACE_LINE(vecSrc, vecEnd, dont_ignore_monsters, pEdict, &tr);

    cell *bodyOut = nullptr;
    cell *distOut = nullptr;
    if (params[0] / sizeof(cell) >= 2)
        amx_GetAddr(amx, params[2], &bodyOut);
    if (params[0] / sizeof(cell) >= 3)
        amx_GetAddr(amx, params[3], &distOut);

    int hitEnt = 0;
    int hitBody = 0;
    float dist = 0.0f;
    if (tr.pHit) {
        hitEnt = ENTINDEX(tr.pHit);
        hitBody = tr.iHitgroup;
        dist = (tr.vecEndPos - vecSrc).Length();
    }
    if (bodyOut) *bodyOut = hitBody;
    if (distOut) *distOut = (cell)dist;
    return hitEnt;
}

// fm_radius_damage(origin, range, damage, attacker, weaponId)
cell AMX_NATIVE_CALL amxx_fm_radius_damage(AMX *amx, cell *params)
{
    Vector vecSrc;
    vecSrc.x = amx_ctof(params[1]);
    vecSrc.y = amx_ctof(params[2]);
    vecSrc.z = amx_ctof(params[3]);
    float radius = amx_ctof(params[4]);
    float damage = amx_ctof(params[5]);
    edict_t *pAttacker = INDEXENT((int)params[6]);
    int weaponId = (int)params[7];

    // Use game's radius damage function
    ::RadiusDamage(vecSrc, pAttacker ? &pAttacker->v : nullptr, nullptr, damage, radius, CLASS_NONE, weaponId == 0 ? DMG_BLAST : weaponId);
    return 1;
}

// fm_get_user_model(index, buffer[], len)
cell AMX_NATIVE_CALL amxx_fm_get_user_model(AMX *amx, cell *params)
{
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict) return 0;

    const char *model = STRING(pEdict->v.model);
    if (!model) model = "";

    // Strip prefix and .mdl suffix
    const char *shortModel = model;
    const char *prefix = "models/player/";
    size_t prefixLen = strlen(prefix);
    if (strncmp(model, prefix, prefixLen) == 0)
        shortModel = model + prefixLen;

    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    int maxlen = (int)params[3];
    char stripped[64];
    snprintf(stripped, sizeof(stripped), "%s", shortModel);
    char *dot = strrchr(stripped, '.');
    if (dot) *dot = '\0';
    return amx_SetString(dest, stripped, 0, 0, maxlen);
}

// fm_set_user_model(index, model[])
cell AMX_NATIVE_CALL amxx_fm_set_user_model(AMX *amx, cell *params)
{
    int index = (int)params[1];
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict) return 0;

    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    char model[128];
    amx_GetString(model, addr, 0, sizeof(model));

    // Rebuild full model path
    char fullModel[256];
    if (strstr(model, "models/") == model)
        snprintf(fullModel, sizeof(fullModel), "%s", model);
    else
        snprintf(fullModel, sizeof(fullModel), "models/player/%s/%s.mdl", model, model);

    SET_MODEL(pEdict, fullModel);
    return 1;
}

// fm_get_speed_of(index)
cell AMX_NATIVE_CALL amxx_fm_get_speed_of(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict) return 0;
    float speed = pEdict->v.velocity.Length();
    return amx_ftoc(speed);
}

// fm_is_ent_visible(index, targetEnt)
cell AMX_NATIVE_CALL amxx_fm_is_ent_visible(AMX *amx, cell *params)
{
    int index = (int)params[1];
    int targetEnt = (int)params[2];
    edict_t *pEdict = INDEXENT(index);
    edict_t *pTarget = INDEXENT(targetEnt);
    if (!pEdict || !pTarget) return 0;

    TraceResult tr;
    Vector vecSrc = pEdict->v.origin + pEdict->v.view_ofs;
    TRACE_LINE(vecSrc, pTarget->v.origin, ignore_monsters, pEdict, &tr);
    return (tr.flFraction >= 1.0f || tr.pHit == pTarget) ? 1 : 0;
}

// fm_get_entity_flags(index)
cell AMX_NATIVE_CALL amxx_fm_get_entity_flags(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict) return 0;
    return (cell)pEdict->v.flags;
}

// fm_set_entity_flags(index, flags)
cell AMX_NATIVE_CALL amxx_fm_set_entity_flags(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict) return 0;
    pEdict->v.flags = (int)params[2];
    return 1;
}

// fm_drop_to_floor(index)
cell AMX_NATIVE_CALL amxx_fm_drop_to_floor(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict) return 0;
    DROP_TO_FLOOR(pEdict);
    return 1;
}

// fm_walk_move(index, x, y, z, speed)
cell AMX_NATIVE_CALL amxx_fm_walk_move(AMX *amx, cell *params)
{
    int index = (int)params[1];
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict) return 0;
    Vector vecGoal;
    vecGoal.x = amx_ctof(params[2]);
    vecGoal.y = amx_ctof(params[3]);
    vecGoal.z = amx_ctof(params[4]);
    float speed = amx_ctof(params[5]);
    float goal = vecGoal.Length();
    WALK_MOVE(pEdict, goal, speed, WALKMOVE_NORMAL);
    return 1;
}

// ===== P1: Fakemeta 补充 =====

// fm_set_rendering(entity, fx, r, g, b, amount)
cell AMX_NATIVE_CALL amxx_fm_set_rendering(AMX *amx, cell *params)
{
    int index = (int)params[1];
    int fx = (int)params[2];
    int r = (int)params[3];
    int g = (int)params[4];
    int b = (int)params[5];
    int amount = (int)params[6];
    (void)amx;

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict) return 0;

    pEdict->v.rendermode = fx;
    pEdict->v.renderamt = amount;
    pEdict->v.rendercolor.x = (float)r;
    pEdict->v.rendercolor.y = (float)g;
    pEdict->v.rendercolor.z = (float)b;
    pEdict->v.renderfx = kRenderFxNone;
    return 1;
}

// fm_entity_set_model(entity, model[])
cell AMX_NATIVE_CALL amxx_fm_entity_set_model(AMX *amx, cell *params)
{
    int index = (int)params[1];
    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    char model[256];
    amx_GetString(model, addr, 0, sizeof(model));

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict) return 0;

    SET_MODEL(pEdict, model);
    return 1;
}

// fm_remove_entity(entity)
cell AMX_NATIVE_CALL amxx_fm_remove_entity(AMX *amx, cell *params)
{
    int index = (int)params[1];
    (void)amx;

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free) return 0;

    REMOVE_ENTITY(pEdict);
    return 1;
}

// fm_set_velocity(entity, x, y, z)
cell AMX_NATIVE_CALL amxx_fm_set_velocity(AMX *amx, cell *params)
{
    int index = (int)params[1];
    (void)amx;

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict) return 0;

    pEdict->v.velocity.x = amx_ctof(params[2]);
    pEdict->v.velocity.y = amx_ctof(params[3]);
    pEdict->v.velocity.z = amx_ctof(params[4]);
    return 1;
}

// fm_entity_set_size(entity, mins[3], maxs[3])
cell AMX_NATIVE_CALL amxx_fm_entity_set_size(AMX *amx, cell *params)
{
    int index = (int)params[1];
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict) return 0;

    cell *mins_addr, *maxs_addr;
    amx_GetAddr(amx, params[2], &mins_addr);
    amx_GetAddr(amx, params[3], &maxs_addr);

    Vector mins(amx_ctof(mins_addr[0]), amx_ctof(mins_addr[1]), amx_ctof(mins_addr[2]));
    Vector maxs(amx_ctof(maxs_addr[0]), amx_ctof(maxs_addr[1]), amx_ctof(maxs_addr[2]));
    SET_SIZE(pEdict, mins, maxs);
    return 1;
}

// ===========================================
// Additional Fakemeta natives
// ===========================================

// create_tr2() - create a trace result handle
cell AMX_NATIVE_CALL amxx_create_tr2(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    FakeMetaTrace *trace = new FakeMetaTrace();
    int idx = g_traces.size();
    g_traces.push_back(trace);
    return idx + 1;
}

// free_tr2(handle) - free a trace result handle
cell AMX_NATIVE_CALL amxx_free_tr2(AMX *amx, cell *params)
{
    (void)amx;
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_traces.size() || !g_traces[idx]) return 0;
    delete g_traces[idx];
    g_traces[idx] = nullptr;
    return 1;
}

// ===== TR_* 枚举（与 amxmodx fakemeta_const.inc 完全对齐）=====
// TR_AllSolid=0, TR_StartSolid=1, TR_InOpen=2, TR_InWater=3,
// TR_flFraction=4, TR_vecEndPos=5, TR_flPlaneDist=6,
// TR_vecPlaneNormal=7, TR_pHit=8, TR_iHitgroup=9
enum {
    TR_AllSolid_e = 0,
    TR_StartSolid_e,
    TR_InOpen_e,
    TR_InWater_e,
    TR_flFraction_e,
    TR_vecEndPos_e,
    TR_flPlaneDist_e,
    TR_vecPlaneNormal_e,
    TR_pHit_e,
    TR_iHitgroup_e
};

// get_tr2(handle, TR_member, [output]) - get trace result field
// 与原版 fakemeta fm_tr2.cpp 行为一致：
//   - int 字段（AllSolid/StartSolid/InOpen/InWater/pHit/iHitgroup）直接返回值
//   - float 字段（flFraction/flPlaneDist）写入 params[3]，返回 1
//   - vector 字段（vecEndPos/vecPlaneNormal）写入 params[3][0..2]，返回 1
//   - pHit 为空时返回 -1
cell AMX_NATIVE_CALL amxx_get_tr2(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    int field = params[2];

    if (idx < 0 || idx >= (int)g_traces.size() || !g_traces[idx]) return 0;

    TraceResult *tr = &g_traces[idx]->tr;
    cell *ptr = nullptr;

    switch (field) {
        case TR_AllSolid_e:
            return tr->fAllSolid;
        case TR_StartSolid_e:
            return tr->fStartSolid;
        case TR_InOpen_e:
            return tr->fInOpen;
        case TR_InWater_e:
            return tr->fInWater;
        case TR_flFraction_e:
            amx_GetAddr(amx, params[3], &ptr);
            if (ptr) *ptr = amx_ftoc(tr->flFraction);
            return 1;
        case TR_vecEndPos_e:
            amx_GetAddr(amx, params[3], &ptr);
            if (ptr) {
                ptr[0] = amx_ftoc(tr->vecEndPos.x);
                ptr[1] = amx_ftoc(tr->vecEndPos.y);
                ptr[2] = amx_ftoc(tr->vecEndPos.z);
            }
            return 1;
        case TR_flPlaneDist_e:
            amx_GetAddr(amx, params[3], &ptr);
            if (ptr) *ptr = amx_ftoc(tr->flPlaneDist);
            return 1;
        case TR_vecPlaneNormal_e:
            amx_GetAddr(amx, params[3], &ptr);
            if (ptr) {
                ptr[0] = amx_ftoc(tr->vecPlaneNormal.x);
                ptr[1] = amx_ftoc(tr->vecPlaneNormal.y);
                ptr[2] = amx_ftoc(tr->vecPlaneNormal.z);
            }
            return 1;
        case TR_pHit_e:
            if (!tr->pHit || FNullEnt(tr->pHit))
                return -1;
            return ENTINDEX(tr->pHit);
        case TR_iHitgroup_e:
            return tr->iHitgroup;
        default:
            return 0;
    }
}

// set_tr2(handle, TR_member, value) - set trace result field
// 与原版 fakemeta fm_tr2.cpp 行为一致：
//   - int 字段：tr->field = *ptr
//   - float 字段：tr->field = amx_ctof(*ptr)
//   - vector 字段：tr->field.x/y/z = amx_ctof(ptr[0..2])
//   - pHit：将实体索引转换为 edict_t* 后写入
cell AMX_NATIVE_CALL amxx_set_tr2(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    int field = params[2];

    if (idx < 0 || idx >= (int)g_traces.size() || !g_traces[idx]) return 0;
    if (params[0] / sizeof(cell) < 3) return 0;

    TraceResult *tr = &g_traces[idx]->tr;
    cell *ptr = nullptr;
    amx_GetAddr(amx, params[3], &ptr);
    if (!ptr) return 0;

    switch (field) {
        case TR_AllSolid_e:
            tr->fAllSolid = *ptr;
            return 1;
        case TR_StartSolid_e:
            tr->fStartSolid = *ptr;
            return 1;
        case TR_InOpen_e:
            tr->fInOpen = *ptr;
            return 1;
        case TR_InWater_e:
            tr->fInWater = *ptr;
            return 1;
        case TR_flFraction_e:
            tr->flFraction = amx_ctof(*ptr);
            return 1;
        case TR_vecEndPos_e:
            tr->vecEndPos.x = amx_ctof(ptr[0]);
            tr->vecEndPos.y = amx_ctof(ptr[1]);
            tr->vecEndPos.z = amx_ctof(ptr[2]);
            return 1;
        case TR_flPlaneDist_e:
            tr->flPlaneDist = amx_ctof(*ptr);
            return 1;
        case TR_vecPlaneNormal_e:
            tr->vecPlaneNormal.x = amx_ctof(ptr[0]);
            tr->vecPlaneNormal.y = amx_ctof(ptr[1]);
            tr->vecPlaneNormal.z = amx_ctof(ptr[2]);
            return 1;
        case TR_pHit_e: {
            int entIdx = *ptr;
            if (entIdx <= 0) {
                tr->pHit = nullptr;
            } else {
                edict_t *pEdict = INDEXENT(entIdx);
                if (!pEdict) return 0;
                tr->pHit = pEdict;
            }
            return 1;
        }
        case TR_iHitgroup_e:
            tr->iHitgroup = *ptr;
            return 1;
        default:
            return 0;
    }
}

// dllfunc / point_contents / is_in_viewcone 已在 native_entities.cpp 中完整实现，
// 此处仅注册到 fakemeta_natives 表（声明见 native_entities.h）

// angle_vector(angle, dest[]) - convert angles to forward vector
cell AMX_NATIVE_CALL amxx_angle_vector(AMX *amx, cell *params)
{
    (void)amx;
    Vector angle(amx_ctof(params[1]), amx_ctof(params[2]), amx_ctof(params[3]));
    Vector forward;
    
    float sr, sp, sy, cr, cp, cy;
    sy = sin(angle.y * (M_PI / 180.0));
    cy = cos(angle.y * (M_PI / 180.0));
    sp = sin(angle.x * (M_PI / 180.0));
    cp = cos(angle.x * (M_PI / 180.0));
    sr = sin(angle.z * (M_PI / 180.0));
    cr = cos(angle.z * (M_PI / 180.0));
    
    forward.x = cp * cy;
    forward.y = cp * sy;
    forward.z = -sp;
    
    cell *dest;
    amx_GetAddr(amx, params[4], &dest);
    dest[0] = amx_ftoc(forward.x);
    dest[1] = amx_ftoc(forward.y);
    dest[2] = amx_ftoc(forward.z);
    
    return 1;
}

// get_distance_f(x1, y1, z1, x2, y2, z2)
cell AMX_NATIVE_CALL amxx_get_distance_f(AMX *amx, cell *params)
{
    (void)amx;
    Vector v1(amx_ctof(params[1]), amx_ctof(params[2]), amx_ctof(params[3]));
    Vector v2(amx_ctof(params[4]), amx_ctof(params[5]), amx_ctof(params[6]));
    float dist = (v1 - v2).Length();
    return amx_ftoc(dist);
}

// vector_length(x, y, z)
cell AMX_NATIVE_CALL amxx_vector_length(AMX *amx, cell *params)
{
    (void)amx;
    Vector v(amx_ctof(params[1]), amx_ctof(params[2]), amx_ctof(params[3]));
    float len = v.Length();
    return amx_ftoc(len);
}

// P1-9: set_orig_retval —— 原始返回值写入尚未实现，桩实现返回 0 避免插件崩溃
cell AMX_NATIVE_CALL amxx_set_orig_retval(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return 0;
}

// ===========================================
// 完整 Fakemeta gamedata + PvData + natives
// ===========================================

static void FM_LogError(AMX *amx, const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    AMXX_LOG_ERR("[%s] %s", "Fakemeta", buf);
    if (amx) amx_RaiseError(amx, AMX_ERR_NATIVE);
}

struct GamedataTableEntry { const char *cls; const char *memb; FMFieldType type; int off; int sz; bool uns; };
static const GamedataTableEntry kGamedataTable[] = {
    { "CGameRules", "m_bFreezePeriod", FMFieldType::FIELD_BOOLEAN, offsetof(CGameRules, m_bFreezePeriod), 0, false },
    { "CGameRules", "m_bBombDropped",  FMFieldType::FIELD_BOOLEAN, offsetof(CGameRules, m_bBombDropped),  0, false },
    { "CBasePlayer", "m_iMenu",        FMFieldType::FIELD_INTEGER, offsetof(CBasePlayer, m_iMenu),       0, false },
    { "CBasePlayer", "m_iAccount",     FMFieldType::FIELD_INTEGER, offsetof(CBasePlayer, m_iAccount),    0, false },
    { "CBasePlayer", "m_bIsVIP",       FMFieldType::FIELD_BOOLEAN, offsetof(CBasePlayer, m_bIsVIP),      0, false },
};
static const size_t kGamedataTableCount = sizeof(kGamedataTable) / sizeof(kGamedataTable[0]);

bool FMGameConfig_GetOffsetByClass(const char *className, const char *memberName, FMTypeDescription *out)
{
    if (!className || !memberName || !out) return false;
    for (size_t i = 0; i < kGamedataTableCount; i++) {
        if (strcmp(kGamedataTable[i].cls, className) == 0 &&
            strcmp(kGamedataTable[i].memb, memberName) == 0) {
            out->fieldType     = kGamedataTable[i].type;
            out->fieldOffset   = kGamedataTable[i].off;
            out->fieldSize     = kGamedataTable[i].sz;
            out->fieldUnsigned = kGamedataTable[i].uns;
            return true;
        }
    }
    return false;
}

bool FMGameConfig_GetOffsetByMember(const char *memberName, FMTypeDescription *out)
{
    if (!memberName || !out) return false;
    for (size_t i = 0; i < kGamedataTableCount; i++) {
        if (strcmp(kGamedataTable[i].memb, memberName) == 0) {
            out->fieldType     = kGamedataTable[i].type;
            out->fieldOffset   = kGamedataTable[i].off;
            out->fieldSize     = kGamedataTable[i].sz;
            out->fieldUnsigned = kGamedataTable[i].uns;
            return true;
        }
    }
    return false;
}

namespace FMPvData {

static inline size_t FM_PtrSize() { return sizeof(void*); }

FMBaseFieldType GetBaseDataType(FMFieldType t)
{
    switch (t) {
        case FMFieldType::FIELD_INTEGER:
        case FMFieldType::FIELD_STRINGINT:
        case FMFieldType::FIELD_SHORT:
        case FMFieldType::FIELD_CHARACTER:
        case FMFieldType::FIELD_CLASS:
        case FMFieldType::FIELD_STRUCTURE:
        case FMFieldType::FIELD_POINTER:
        case FMFieldType::FIELD_FUNCTION:
        case FMFieldType::FIELD_BOOLEAN:
            return FMBaseFieldType::Integer;
        case FMFieldType::FIELD_FLOAT:
            return FMBaseFieldType::Float;
        case FMFieldType::FIELD_VECTOR:
            return FMBaseFieldType::Vector;
        case FMFieldType::FIELD_CLASSPTR:
        case FMFieldType::FIELD_ENTVARS:
        case FMFieldType::FIELD_EDICT:
        case FMFieldType::FIELD_EHANDLE:
            return FMBaseFieldType::Entity;
        case FMFieldType::FIELD_STRINGPTR:
        case FMFieldType::FIELD_STRING:
            return FMBaseFieldType::String;
        default:
            return FMBaseFieldType::None;
    }
}

const char* GetBaseTypeName(FMBaseFieldType t)
{
    switch (t) {
        case FMBaseFieldType::Integer: return "integer";
        case FMBaseFieldType::Float:   return "float";
        case FMBaseFieldType::Vector:  return "vector";
        case FMBaseFieldType::Entity:  return "entity";
        case FMBaseFieldType::String:  return "string";
        default: return "unknown";
    }
}

cell GetInt(void *pObject, const FMTypeDescription &data, int element)
{
    char *p = (char*)pObject + data.fieldOffset;
    switch (data.fieldType) {
        case FMFieldType::FIELD_INTEGER:
        case FMFieldType::FIELD_STRINGINT:
            return (cell)*((int32_t*)(p + element * sizeof(int32_t)));
        case FMFieldType::FIELD_SHORT:
            if (data.fieldUnsigned)
                return (cell)*((uint16_t*)(p + element * sizeof(uint16_t)));
            else
                return (cell)*((int16_t*)(p + element * sizeof(uint16_t)));
        case FMFieldType::FIELD_CHARACTER:
            if (data.fieldUnsigned)
                return (cell)*((uint8_t*)(p + element * sizeof(uint8_t)));
            else
                return (cell)*((int8_t*)(p + element * sizeof(int8_t)));
        case FMFieldType::FIELD_BOOLEAN:
            return *((bool*)(p + element * sizeof(bool))) ? 1 : 0;
        case FMFieldType::FIELD_CLASS:
        case FMFieldType::FIELD_STRUCTURE: {
            size_t sz = data.fieldSize ? (size_t)data.fieldSize : sizeof(void*);
            return (cell)(uintptr_t)(p + element * sz);
        }
        case FMFieldType::FIELD_POINTER:
        case FMFieldType::FIELD_FUNCTION:
            return (cell)(uintptr_t)*((void**)(p + element * FM_PtrSize()));
        default:
            return 0;
    }
}

void SetInt(void *pObject, const FMTypeDescription &data, cell value, int element)
{
    char *p = (char*)pObject + data.fieldOffset;
    switch (data.fieldType) {
        case FMFieldType::FIELD_INTEGER:
        case FMFieldType::FIELD_STRINGINT:
            *((int32_t*)(p + element * sizeof(int32_t))) = (int32_t)value;
            break;
        case FMFieldType::FIELD_SHORT:
            if (data.fieldUnsigned)
                *((uint16_t*)(p + element * sizeof(uint16_t))) = (uint16_t)value;
            else
                *((int16_t*)(p + element * sizeof(uint16_t))) = (int16_t)value;
            break;
        case FMFieldType::FIELD_CHARACTER:
            if (data.fieldUnsigned)
                *((uint8_t*)(p + element * sizeof(uint8_t))) = (uint8_t)value;
            else
                *((int8_t*)(p + element * sizeof(int8_t))) = (int8_t)value;
            break;
        case FMFieldType::FIELD_BOOLEAN:
            *((bool*)(p + element * sizeof(bool))) = (value != 0);
            break;
        case FMFieldType::FIELD_POINTER:
        case FMFieldType::FIELD_FUNCTION:
            *((void**)(p + element * FM_PtrSize())) = (void*)(uintptr_t)value;
            break;
        default:
            break;
    }
}

cell GetFloat(void *pObject, const FMTypeDescription &data, int element)
{
    char *p = (char*)pObject + data.fieldOffset;
    float v = *((float*)(p + element * sizeof(float)));
    return amx_ftoc(v);
}

void SetFloat(void *pObject, const FMTypeDescription &data, float value, int element)
{
    char *p = (char*)pObject + data.fieldOffset;
    *((float*)(p + element * sizeof(float))) = value;
}

void GetVector(void *pObject, const FMTypeDescription &data, cell *pVector, int element)
{
    char *p = (char*)pObject + data.fieldOffset;
    float *v = (float*)(p + element * sizeof(Vector));
    pVector[0] = amx_ftoc(v[0]);
    pVector[1] = amx_ftoc(v[1]);
    pVector[2] = amx_ftoc(v[2]);
}

void SetVector(void *pObject, const FMTypeDescription &data, const cell *pVector, int element)
{
    char *p = (char*)pObject + data.fieldOffset;
    float *v = (float*)(p + element * sizeof(Vector));
    v[0] = amx_ctof(pVector[0]);
    v[1] = amx_ctof(pVector[1]);
    v[2] = amx_ctof(pVector[2]);
}

cell GetEntity(void *pObject, const FMTypeDescription &data, int element)
{
    char *p = (char*)pObject + data.fieldOffset;
    switch (data.fieldType) {
        case FMFieldType::FIELD_CLASSPTR: {
            CBaseEntity *pCBase = *((CBaseEntity**)(p + element * FM_PtrSize()));
            if (!pCBase) return 0;
            entvars_t *pev = pCBase->pev;
            if (!pev) return 0;
            edict_t *pEdict = ENT(pev);
            if (!pEdict || pEdict->free) return 0;
            return ENTINDEX(pEdict);
        }
        case FMFieldType::FIELD_ENTVARS: {
            entvars_t *pev = *((entvars_t**)(p + element * FM_PtrSize()));
            if (!pev) return 0;
            edict_t *pEdict = ENT(pev);
            if (!pEdict || pEdict->free) return 0;
            return ENTINDEX(pEdict);
        }
        case FMFieldType::FIELD_EDICT: {
            edict_t *pEdict = *((edict_t**)(p + element * FM_PtrSize()));
            if (!pEdict || pEdict->free) return 0;
            return ENTINDEX(pEdict);
        }
        case FMFieldType::FIELD_EHANDLE: {
            edict_t *pEdict = *((edict_t**)(p + element * FM_PtrSize()));
            if (!pEdict || pEdict->free) return 0;
            return ENTINDEX(pEdict);
        }
        default:
            return 0;
    }
}

void SetEntity(void *pObject, const FMTypeDescription &data, int value, int element)
{
    char *p = (char*)pObject + data.fieldOffset;
    edict_t *pEdict = nullptr;
    if (value != -1) {
        pEdict = INDEXENT(value);
        if (!pEdict || pEdict->free) pEdict = nullptr;
    }
    switch (data.fieldType) {
        case FMFieldType::FIELD_CLASSPTR: {
            CBaseEntity *pCBase = nullptr;
            if (pEdict && pEdict->pvPrivateData)
                pCBase = (CBaseEntity*)pEdict->pvPrivateData;
            *((CBaseEntity**)(p + element * FM_PtrSize())) = pCBase;
            break;
        }
        case FMFieldType::FIELD_ENTVARS: {
            entvars_t *pev = pEdict ? &pEdict->v : nullptr;
            *((entvars_t**)(p + element * FM_PtrSize())) = pev;
            break;
        }
        case FMFieldType::FIELD_EDICT:
        case FMFieldType::FIELD_EHANDLE:
            *((edict_t**)(p + element * FM_PtrSize())) = pEdict;
            break;
        default:
            break;
    }
}

char* GetString(void *pObject, const FMTypeDescription &data, int element)
{
    char *p = (char*)pObject + data.fieldOffset;
    if (data.fieldType == FMFieldType::FIELD_STRING) {
        int sz = data.fieldSize ? data.fieldSize : 1;
        return (char*)(p + element * sz);
    } else if (data.fieldType == FMFieldType::FIELD_STRINGPTR) {
        return *((char**)(p + element * FM_PtrSize()));
    }
    return nullptr;
}

cell SetString(void *pObject, const FMTypeDescription &data, const char *value, int maxlen, int element)
{
    char *p = (char*)pObject + data.fieldOffset;
    int copyLen = (maxlen >= 0) ? maxlen : (int)strlen(value);
    if (data.fieldType == FMFieldType::FIELD_STRING) {
        int sz = data.fieldSize ? data.fieldSize : 1;
        char *dst = (char*)(p + element * sz);
        int bufLen = data.fieldSize ? (data.fieldSize - 1) : copyLen;
        int toCopy = (copyLen < bufLen) ? copyLen : bufLen;
        if (toCopy > 0) strncpy(dst, value, (size_t)toCopy);
        dst[toCopy] = '\0';
        return toCopy;
    } else if (data.fieldType == FMFieldType::FIELD_STRINGPTR) {
        char **ppDst = (char**)(p + element * FM_PtrSize());
        char *oldBuf = *ppDst;
        int needLen = copyLen + 1;
        int oldLen = oldBuf ? (int)strlen(oldBuf) : 0;
        if (!oldBuf || needLen > (oldLen + 1)) {
            if (oldBuf) free(oldBuf);
            *ppDst = (char*)malloc((size_t)needLen);
        }
        char *dst = *ppDst;
        if (dst) {
            strncpy(dst, value, (size_t)copyLen);
            dst[copyLen] = '\0';
        }
        return copyLen;
    }
    return 0;
}

}

static bool FM_GetTypeDescription(AMX *amx, cell *params, int pos, FMTypeDescription &data,
                                   const char *&className, const char *&memberName)
{
    cell *clsAddr, *memAddr;
    amx_GetAddr(amx, params[pos], &clsAddr);
    amx_GetAddr(amx, params[pos + 1], &memAddr);
    int lenCls = 0, lenMem = 0;
    amx_StrLen(clsAddr, &lenCls);
    amx_StrLen(memAddr, &lenMem);

    static char s_cls[256], s_mem[256];
    if (lenCls > 0) amx_GetString(s_cls, clsAddr, 0, sizeof(s_cls)); else s_cls[0] = '\0';
    if (lenMem > 0) amx_GetString(s_mem, memAddr, 0, sizeof(s_mem)); else s_mem[0] = '\0';
    className = s_cls;
    memberName = s_mem;

    if (!lenCls || !lenMem) {
        FM_LogError(amx, "Either class (\"%s\") or member (\"%s\") is empty", className, memberName);
        return false;
    }
    if (!FMGameConfig_GetOffsetByClass(className, memberName, &data)) {
        FM_LogError(amx, "Could not find class \"%s\" and/or member \"%s\" in gamedata", className, memberName);
        return false;
    }
    if (data.fieldOffset < 0) {
        FM_LogError(amx, "Invalid offset %d retrieved from \"%s\" member", data.fieldOffset, memberName);
        return false;
    }
    return true;
}

static bool FM_CheckData(AMX *amx, const FMTypeDescription &data, int element,
                          FMBaseFieldType baseType, const char *memberName)
{
    if (baseType != FMBaseFieldType::None && baseType != FMPvData::GetBaseDataType(data.fieldType)) {
        FM_LogError(amx, "Data field is not %s-based", FMPvData::GetBaseTypeName(baseType));
        return false;
    }
    if (element < 0 || (data.fieldSize > 0 && element >= data.fieldSize)) {
        FM_LogError(amx, "Invalid element index %d, value must be between 0 and %d",
                     element, (data.fieldSize > 0) ? (data.fieldSize - 1) : 0);
        return false;
    }
    if (element > 0 && !data.fieldSize) {
        FM_LogError(amx, "Member \"%s\" is not an array. Element %d is invalid.", memberName, element);
        return false;
    }
    return true;
}

static bool FM_CheckEntityPdata(AMX *amx, int entity, void *&outPvData)
{
    outPvData = nullptr;
    if (entity < 1 || entity > gpGlobals->maxEntities) {
        FM_LogError(amx, "Invalid entity index %d", entity);
        return false;
    }
    edict_t *pEdict = INDEXENT(entity);
    if (!pEdict || pEdict->free || !pEdict->pvPrivateData) {
        FM_LogError(amx, "Entity %d is invalid or has no private data", entity);
        return false;
    }
    outPvData = pEdict->pvPrivateData;
    return true;
}

static bool FM_CheckEntityValue(AMX *amx, int value)
{
    if (value == -1) return true;
    if (value < 1 || value > gpGlobals->maxEntities) {
        FM_LogError(amx, "Invalid entity index %d", value);
        return false;
    }
    edict_t *pEdict = INDEXENT(value);
    if (!pEdict || pEdict->free) {
        FM_LogError(amx, "Entity %d is invalid", value);
        return false;
    }
    return true;
}

static bool FM_CheckGameRules(AMX *amx, void *&outGr)
{
    outGr = nullptr;
    if (!g_pGameRules) {
        FM_LogError(amx, "GameRules data is disabled. Check your AMXX log.");
        return false;
    }
    outGr = (void*)g_pGameRules;
    return true;
}

// ========== get_ent_data 系列 ==========

cell AMX_NATIVE_CALL amxx_get_ent_data(AMX *amx, cell *params)
{
    void *pPv = nullptr;
    if (!FM_CheckEntityPdata(amx, (int)params[1], pPv)) return 0;
    FMTypeDescription data;
    const char *cls, *mem;
    if (!FM_GetTypeDescription(amx, params, 2, data, cls, mem)) return 0;
    int element = (params[0] / sizeof(cell) >= 4) ? (int)params[4] : 0;
    if (!FM_CheckData(amx, data, element, FMBaseFieldType::Integer, mem)) return 0;
    return FMPvData::GetInt(pPv, data, element);
}

cell AMX_NATIVE_CALL amxx_set_ent_data(AMX *amx, cell *params)
{
    void *pPv = nullptr;
    if (!FM_CheckEntityPdata(amx, (int)params[1], pPv)) return 0;
    FMTypeDescription data;
    const char *cls, *mem;
    if (!FM_GetTypeDescription(amx, params, 2, data, cls, mem)) return 0;
    if (data.fieldType == FMFieldType::FIELD_CLASS || data.fieldType == FMFieldType::FIELD_STRUCTURE) {
        FM_LogError(amx, "Setting directly to a class or structure address is not available");
        return 0;
    }
    int element = (params[0] / sizeof(cell) >= 5) ? (int)params[5] : 0;
    if (!FM_CheckData(amx, data, element, FMBaseFieldType::Integer, mem)) return 0;
    FMPvData::SetInt(pPv, data, params[4], element);
    return 1;
}

cell AMX_NATIVE_CALL amxx_get_ent_data_float(AMX *amx, cell *params)
{
    void *pPv = nullptr;
    if (!FM_CheckEntityPdata(amx, (int)params[1], pPv)) return 0;
    FMTypeDescription data;
    const char *cls, *mem;
    if (!FM_GetTypeDescription(amx, params, 2, data, cls, mem)) return 0;
    int element = (params[0] / sizeof(cell) >= 4) ? (int)params[4] : 0;
    if (!FM_CheckData(amx, data, element, FMBaseFieldType::Float, mem)) return 0;
    return FMPvData::GetFloat(pPv, data, element);
}

cell AMX_NATIVE_CALL amxx_set_ent_data_float(AMX *amx, cell *params)
{
    void *pPv = nullptr;
    if (!FM_CheckEntityPdata(amx, (int)params[1], pPv)) return 0;
    FMTypeDescription data;
    const char *cls, *mem;
    if (!FM_GetTypeDescription(amx, params, 2, data, cls, mem)) return 0;
    int element = (params[0] / sizeof(cell) >= 5) ? (int)params[5] : 0;
    if (!FM_CheckData(amx, data, element, FMBaseFieldType::Float, mem)) return 0;
    FMPvData::SetFloat(pPv, data, amx_ctof(params[4]), element);
    return 1;
}

cell AMX_NATIVE_CALL amxx_get_ent_data_vector(AMX *amx, cell *params)
{
    void *pPv = nullptr;
    if (!FM_CheckEntityPdata(amx, (int)params[1], pPv)) return 0;
    FMTypeDescription data;
    const char *cls, *mem;
    if (!FM_GetTypeDescription(amx, params, 2, data, cls, mem)) return 0;
    int element = (params[0] / sizeof(cell) >= 4) ? (int)params[4] : 0;
    if (!FM_CheckData(amx, data, element, FMBaseFieldType::Vector, mem)) return 0;
    cell *dst;
    amx_GetAddr(amx, params[3], &dst);
    FMPvData::GetVector(pPv, data, dst, element);
    return 1;
}

cell AMX_NATIVE_CALL amxx_set_ent_data_vector(AMX *amx, cell *params)
{
    void *pPv = nullptr;
    if (!FM_CheckEntityPdata(amx, (int)params[1], pPv)) return 0;
    FMTypeDescription data;
    const char *cls, *mem;
    if (!FM_GetTypeDescription(amx, params, 2, data, cls, mem)) return 0;
    int element = (params[0] / sizeof(cell) >= 5) ? (int)params[5] : 0;
    if (!FM_CheckData(amx, data, element, FMBaseFieldType::Vector, mem)) return 0;
    cell *src;
    amx_GetAddr(amx, params[4], &src);
    FMPvData::SetVector(pPv, data, src, element);
    return 1;
}

cell AMX_NATIVE_CALL amxx_get_ent_data_entity(AMX *amx, cell *params)
{
    void *pPv = nullptr;
    if (!FM_CheckEntityPdata(amx, (int)params[1], pPv)) return 0;
    FMTypeDescription data;
    const char *cls, *mem;
    if (!FM_GetTypeDescription(amx, params, 2, data, cls, mem)) return 0;
    int element = (params[0] / sizeof(cell) >= 4) ? (int)params[4] : 0;
    if (!FM_CheckData(amx, data, element, FMBaseFieldType::Entity, mem)) return 0;
    return FMPvData::GetEntity(pPv, data, element);
}

cell AMX_NATIVE_CALL amxx_set_ent_data_entity(AMX *amx, cell *params)
{
    void *pPv = nullptr;
    if (!FM_CheckEntityPdata(amx, (int)params[1], pPv)) return 0;
    FMTypeDescription data;
    const char *cls, *mem;
    if (!FM_GetTypeDescription(amx, params, 2, data, cls, mem)) return 0;
    int value = (int)params[4];
    if (!FM_CheckEntityValue(amx, value)) return 0;
    int element = (params[0] / sizeof(cell) >= 5) ? (int)params[5] : 0;
    if (!FM_CheckData(amx, data, element, FMBaseFieldType::Entity, mem)) return 0;
    FMPvData::SetEntity(pPv, data, value, element);
    return 1;
}

cell AMX_NATIVE_CALL amxx_get_ent_data_string(AMX *amx, cell *params)
{
    void *pPv = nullptr;
    if (!FM_CheckEntityPdata(amx, (int)params[1], pPv)) return 0;
    FMTypeDescription data;
    const char *cls, *mem;
    if (!FM_GetTypeDescription(amx, params, 2, data, cls, mem)) return 0;
    int argc = params[0] / sizeof(cell);
    int element = (argc >= 6) ? (int)params[6] : 0;
    if (!FM_CheckData(amx, data, element, FMBaseFieldType::String, mem)) return 0;
    int maxlen = (int)params[5];
    if (data.fieldSize > 0 && maxlen > (data.fieldSize - 1))
        maxlen = data.fieldSize - 1;
    char *src = FMPvData::GetString(pPv, data, element);
    const char *from = src ? src : "";
    cell *dst;
    amx_GetAddr(amx, params[4], &dst);
    int written = amx_SetString(dst, from, 0, 0, (size_t)(maxlen + 1));
    return written;
}

cell AMX_NATIVE_CALL amxx_set_ent_data_string(AMX *amx, cell *params)
{
    void *pPv = nullptr;
    if (!FM_CheckEntityPdata(amx, (int)params[1], pPv)) return 0;
    FMTypeDescription data;
    const char *cls, *mem;
    if (!FM_GetTypeDescription(amx, params, 2, data, cls, mem)) return 0;
    int argc = params[0] / sizeof(cell);
    int element = (argc >= 6) ? (int)params[6] : 0;
    if (!FM_CheckData(amx, data, element, FMBaseFieldType::String, mem)) return 0;
    cell *srcAddr;
    amx_GetAddr(amx, params[4], &srcAddr);
    char buf[2048];
    amx_GetString(buf, srcAddr, 0, sizeof(buf));
    int maxlen = (int)params[5];
    return FMPvData::SetString(pPv, data, buf, maxlen, element);
}

cell AMX_NATIVE_CALL amxx_get_ent_data_size(AMX *amx, cell *params)
{
    FMTypeDescription data;
    const char *cls, *mem;
    if (!FM_GetTypeDescription(amx, params, 1, data, cls, mem)) return 0;
    return data.fieldSize;
}

cell AMX_NATIVE_CALL amxx_find_ent_data_info(AMX *amx, cell *params)
{
    FMTypeDescription data;
    const char *cls, *mem;
    if (!FM_GetTypeDescription(amx, params, 1, data, cls, mem)) return 0;
    int argc = params[0] / sizeof(cell);
    if (argc >= 3) {
        cell *pFieldType;
        amx_GetAddr(amx, params[3], &pFieldType);
        *pFieldType = (cell)data.fieldType;
    }
    if (argc >= 4) {
        cell *pSize;
        amx_GetAddr(amx, params[4], &pSize);
        *pSize = (data.fieldSize > 0) ? (cell)data.fieldSize : 0;
    }
    if (argc >= 5) {
        cell *pUnsigned;
        amx_GetAddr(amx, params[5], &pUnsigned);
        *pUnsigned = data.fieldUnsigned ? 1 : 0;
    }
    return data.fieldOffset;
}

// ========== get_gamerules 系列 ==========

cell AMX_NATIVE_CALL amxx_get_gamerules_int(AMX *amx, cell *params)
{
    void *pGr = nullptr;
    if (!FM_CheckGameRules(amx, pGr)) return 0;
    FMTypeDescription data;
    const char *cls, *mem;
    if (!FM_GetTypeDescription(amx, params, 1, data, cls, mem)) return 0;
    int element = (params[0] / sizeof(cell) >= 3) ? (int)params[3] : 0;
    if (!FM_CheckData(amx, data, element, FMBaseFieldType::Integer, mem)) return 0;
    return FMPvData::GetInt(pGr, data, element);
}

cell AMX_NATIVE_CALL amxx_set_gamerules_int(AMX *amx, cell *params)
{
    void *pGr = nullptr;
    if (!FM_CheckGameRules(amx, pGr)) return 0;
    FMTypeDescription data;
    const char *cls, *mem;
    if (!FM_GetTypeDescription(amx, params, 1, data, cls, mem)) return 0;
    if (data.fieldType == FMFieldType::FIELD_CLASS || data.fieldType == FMFieldType::FIELD_STRUCTURE) {
        FM_LogError(amx, "Setting directly to a class or structure address is not available");
        return 0;
    }
    int element = (params[0] / sizeof(cell) >= 4) ? (int)params[4] : 0;
    if (!FM_CheckData(amx, data, element, FMBaseFieldType::Integer, mem)) return 0;
    FMPvData::SetInt(pGr, data, params[3], element);
    return 1;
}

cell AMX_NATIVE_CALL amxx_get_gamerules_float(AMX *amx, cell *params)
{
    void *pGr = nullptr;
    if (!FM_CheckGameRules(amx, pGr)) return 0;
    FMTypeDescription data;
    const char *cls, *mem;
    if (!FM_GetTypeDescription(amx, params, 1, data, cls, mem)) return 0;
    int element = (params[0] / sizeof(cell) >= 3) ? (int)params[3] : 0;
    if (!FM_CheckData(amx, data, element, FMBaseFieldType::Float, mem)) return 0;
    return FMPvData::GetFloat(pGr, data, element);
}

cell AMX_NATIVE_CALL amxx_set_gamerules_float(AMX *amx, cell *params)
{
    void *pGr = nullptr;
    if (!FM_CheckGameRules(amx, pGr)) return 0;
    FMTypeDescription data;
    const char *cls, *mem;
    if (!FM_GetTypeDescription(amx, params, 1, data, cls, mem)) return 0;
    int element = (params[0] / sizeof(cell) >= 4) ? (int)params[4] : 0;
    if (!FM_CheckData(amx, data, element, FMBaseFieldType::Float, mem)) return 0;
    FMPvData::SetFloat(pGr, data, amx_ctof(params[3]), element);
    return 1;
}

cell AMX_NATIVE_CALL amxx_get_gamerules_vector(AMX *amx, cell *params)
{
    void *pGr = nullptr;
    if (!FM_CheckGameRules(amx, pGr)) return 0;
    FMTypeDescription data;
    const char *cls, *mem;
    if (!FM_GetTypeDescription(amx, params, 1, data, cls, mem)) return 0;
    int element = (params[0] / sizeof(cell) >= 4) ? (int)params[4] : 0;
    if (!FM_CheckData(amx, data, element, FMBaseFieldType::Vector, mem)) return 0;
    cell *dst;
    amx_GetAddr(amx, params[2], &dst);
    FMPvData::GetVector(pGr, data, dst, element);
    return 1;
}

cell AMX_NATIVE_CALL amxx_set_gamerules_vector(AMX *amx, cell *params)
{
    void *pGr = nullptr;
    if (!FM_CheckGameRules(amx, pGr)) return 0;
    FMTypeDescription data;
    const char *cls, *mem;
    if (!FM_GetTypeDescription(amx, params, 1, data, cls, mem)) return 0;
    int element = (params[0] / sizeof(cell) >= 4) ? (int)params[4] : 0;
    if (!FM_CheckData(amx, data, element, FMBaseFieldType::Vector, mem)) return 0;
    cell *src;
    amx_GetAddr(amx, params[3], &src);
    FMPvData::SetVector(pGr, data, src, element);
    return 1;
}

cell AMX_NATIVE_CALL amxx_get_gamerules_entity(AMX *amx, cell *params)
{
    void *pGr = nullptr;
    if (!FM_CheckGameRules(amx, pGr)) return 0;
    FMTypeDescription data;
    const char *cls, *mem;
    if (!FM_GetTypeDescription(amx, params, 1, data, cls, mem)) return 0;
    int element = (params[0] / sizeof(cell) >= 3) ? (int)params[3] : 0;
    if (!FM_CheckData(amx, data, element, FMBaseFieldType::Entity, mem)) return 0;
    return FMPvData::GetEntity(pGr, data, element);
}

cell AMX_NATIVE_CALL amxx_set_gamerules_entity(AMX *amx, cell *params)
{
    void *pGr = nullptr;
    if (!FM_CheckGameRules(amx, pGr)) return 0;
    FMTypeDescription data;
    const char *cls, *mem;
    if (!FM_GetTypeDescription(amx, params, 1, data, cls, mem)) return 0;
    int value = (int)params[3];
    if (!FM_CheckEntityValue(amx, value)) return 0;
    int element = (params[0] / sizeof(cell) >= 4) ? (int)params[4] : 0;
    if (!FM_CheckData(amx, data, element, FMBaseFieldType::Entity, mem)) return 0;
    FMPvData::SetEntity(pGr, data, value, element);
    return 1;
}

cell AMX_NATIVE_CALL amxx_get_gamerules_string(AMX *amx, cell *params)
{
    void *pGr = nullptr;
    if (!FM_CheckGameRules(amx, pGr)) return 0;
    FMTypeDescription data;
    const char *cls, *mem;
    if (!FM_GetTypeDescription(amx, params, 1, data, cls, mem)) return 0;
    int argc = params[0] / sizeof(cell);
    int element = (argc >= 5) ? (int)params[5] : 0;
    if (!FM_CheckData(amx, data, element, FMBaseFieldType::String, mem)) return 0;
    int maxlen = (int)params[4];
    if (data.fieldSize > 0 && maxlen > (data.fieldSize - 1))
        maxlen = data.fieldSize - 1;
    char *src = FMPvData::GetString(pGr, data, element);
    const char *from = src ? src : "";
    cell *dst;
    amx_GetAddr(amx, params[3], &dst);
    int written = amx_SetString(dst, from, 0, 0, (size_t)(maxlen + 1));
    return written;
}

cell AMX_NATIVE_CALL amxx_set_gamerules_string(AMX *amx, cell *params)
{
    void *pGr = nullptr;
    if (!FM_CheckGameRules(amx, pGr)) return 0;
    FMTypeDescription data;
    const char *cls, *mem;
    if (!FM_GetTypeDescription(amx, params, 1, data, cls, mem)) return 0;
    int argc = params[0] / sizeof(cell);
    int element = (argc >= 5) ? (int)params[5] : 0;
    if (!FM_CheckData(amx, data, element, FMBaseFieldType::String, mem)) return 0;
    cell *srcAddr;
    amx_GetAddr(amx, params[3], &srcAddr);
    char buf[2048];
    amx_GetString(buf, srcAddr, 0, sizeof(buf));
    int maxlen = (int)params[4];
    return FMPvData::SetString(pGr, data, buf, maxlen, element);
}

cell AMX_NATIVE_CALL amxx_get_gamerules_size(AMX *amx, cell *params)
{
    FMTypeDescription data;
    const char *cls, *mem;
    if (!FM_GetTypeDescription(amx, params, 1, data, cls, mem)) return 0;
    return data.fieldSize;
}

cell AMX_NATIVE_CALL amxx_find_gamerules_info(AMX *amx, cell *params)
{
    FMTypeDescription data;
    const char *cls, *mem;
    if (!FM_GetTypeDescription(amx, params, 1, data, cls, mem)) return 0;
    int argc = params[0] / sizeof(cell);
    if (argc >= 3) {
        cell *pFieldType;
        amx_GetAddr(amx, params[3], &pFieldType);
        *pFieldType = (cell)data.fieldType;
    }
    if (argc >= 4) {
        cell *pSize;
        amx_GetAddr(amx, params[4], &pSize);
        *pSize = (data.fieldSize > 0) ? (cell)data.fieldSize : 0;
    }
    if (argc >= 5) {
        cell *pUnsigned;
        amx_GetAddr(amx, params[5], &pUnsigned);
        *pUnsigned = data.fieldUnsigned ? 1 : 0;
    }
    return data.fieldOffset;
}

// ===== Native registration =====

AMX_NATIVE_INFO fakemeta_natives[] = {
    // Entity creation/lookup
    {"fm_create_entity", amxx_fm_create_entity},
    {"fm_find_entity_by_class", amxx_fm_find_ent_by_class},
    {"fm_find_entity_by_model", amxx_fm_find_ent_by_model},
    {"fm_find_entity_by_target", amxx_fm_find_ent_by_target},
    {"fm_find_entity_by_owner", amxx_fm_find_ent_by_owner},
    {"fm_find_entity_in_sphere", amxx_fm_find_ent_in_sphere},

    // Entity manipulation
    {"fm_give_item", amxx_fm_give_item},
    {"fm_get_aim_entity", amxx_fm_get_aim_entity},
    {"fm_radius_damage", amxx_fm_radius_damage},

    // Player model
    {"fm_get_user_model", amxx_fm_get_user_model},
    {"fm_set_user_model", amxx_fm_set_user_model},

    // Utility
    {"fm_get_speed_of", amxx_fm_get_speed_of},
    {"fm_is_ent_visible", amxx_fm_is_ent_visible},
    {"fm_get_entity_flags", amxx_fm_get_entity_flags},
    {"fm_set_entity_flags", amxx_fm_set_entity_flags},
    {"fm_drop_to_floor", amxx_fm_drop_to_floor},
    {"fm_walk_move", amxx_fm_walk_move},

    // P1: Fakemeta 补充
    {"fm_set_rendering", amxx_fm_set_rendering},
    {"fm_entity_set_model", amxx_fm_entity_set_model},
    {"fm_remove_entity", amxx_fm_remove_entity},
    {"fm_set_velocity", amxx_fm_set_velocity},
    {"fm_entity_set_size", amxx_fm_entity_set_size},

    // Additional Fakemeta natives
    {"create_tr2", amxx_create_tr2},
    {"free_tr2", amxx_free_tr2},
    {"get_tr2", amxx_get_tr2},
    {"set_tr2", amxx_set_tr2},
    {"point_contents", amxx_point_contents},
    {"angle_vector", amxx_angle_vector},
    {"is_in_viewcone", amxx_is_in_viewcone},
    {"get_distance_f", amxx_get_distance_f},
    {"vector_length", amxx_vector_length},

    // P1-9: FakeMeta 引擎 forward 注册 / 引擎函数调用 / 原始返回值
    // register_forward / unregister_forward / engfunc / dllfunc 实现位于
    // native_entities.cpp；get_orig_retval 实现位于 native_messages.cpp。
    // 在 fakemeta_natives 中一并注册，使 fakemeta 模块自洽，且 amx_Register
    // 对同名 native 仅首次解析生效，重复注册无副作用。
    {"register_forward",   amxx_register_forward},
    {"unregister_forward", amxx_unregister_forward},
    {"engfunc",            amxx_engfunc},
    {"dllfunc",            amxx_dllfunc},
    {"get_orig_retval",    amxx_get_orig_retval},
    {"set_orig_retval",    amxx_set_orig_retval},

    {"get_ent_data",         amxx_get_ent_data},
    {"set_ent_data",         amxx_set_ent_data},
    {"get_ent_data_float",   amxx_get_ent_data_float},
    {"set_ent_data_float",   amxx_set_ent_data_float},
    {"get_ent_data_vector",  amxx_get_ent_data_vector},
    {"set_ent_data_vector",  amxx_set_ent_data_vector},
    {"get_ent_data_entity",  amxx_get_ent_data_entity},
    {"set_ent_data_entity",  amxx_set_ent_data_entity},
    {"get_ent_data_string",  amxx_get_ent_data_string},
    {"set_ent_data_string",  amxx_set_ent_data_string},
    {"get_ent_data_size",    amxx_get_ent_data_size},
    {"find_ent_data_info",   amxx_find_ent_data_info},
    {"get_gamerules_int",    amxx_get_gamerules_int},
    {"set_gamerules_int",    amxx_set_gamerules_int},
    {"get_gamerules_float",  amxx_get_gamerules_float},
    {"set_gamerules_float",  amxx_set_gamerules_float},
    {"get_gamerules_vector", amxx_get_gamerules_vector},
    {"set_gamerules_vector", amxx_set_gamerules_vector},
    {"get_gamerules_entity", amxx_get_gamerules_entity},
    {"set_gamerules_entity", amxx_set_gamerules_entity},
    {"get_gamerules_string", amxx_get_gamerules_string},
    {"set_gamerules_string", amxx_set_gamerules_string},
    {"get_gamerules_size",   amxx_get_gamerules_size},
    {"find_gamerules_info",  amxx_find_gamerules_info},

    {nullptr, nullptr}
};

void RegisterFakemetaNatives(AMX *amx)
{
    amx_Register(amx, fakemeta_natives, -1);
}
