#include "precompiled.h"

#include "thinktouch.h"
#include "forwards.h"
#include "native_entities.h"
#include "../extdll.h"
#include "../enginecallback.h"
#include "../cbase.h"

extern DLL_FUNCTIONS gFunctionTable;

static void (*g_origThink)(edict_t *pent) = nullptr;
static void (*g_origTouch)(edict_t *pentTouched, edict_t *pentOther) = nullptr;
static int  (*g_origSpawn)(edict_t *pent) = nullptr;
static void (*g_origSetModel)(edict_t *e, const char *m) = nullptr;

AMXXThinkTouch &AMXXThinkTouch::GetInstance()
{
    static AMXXThinkTouch instance;
    return instance;
}

void AMXXThinkTouch::Init()
{
    AMXX_LOG("[ThinkTouch] Initializing think/touch hook system...");
    HookDispatch();
    AMXX_LOG("[ThinkTouch] Think/touch hooks installed");
}

void AMXXThinkTouch::Shutdown()
{
    AMXX_LOG("[ThinkTouch] Shutting down think/touch system...");
    UnhookDispatch();
    m_thinks.clear();
    m_touches.clear();
    AMXX_LOG("[ThinkTouch] Think/touch system shut down");
}

void AMXXThinkTouch::RegisterThink(const char *classname, AMX *amx, cell funcidx)
{
    ThinkCallback cb;
    cb.amx = amx;
    cb.funcidx = funcidx;
    m_thinks[classname] = cb;
    AMXX_LOG("[ThinkTouch] Registered think for class '%s' (funcidx=%d)", classname, funcidx);
}

void AMXXThinkTouch::RegisterTouch(const char *toucherClass, const char *touchedClass, AMX *amx, cell funcidx)
{
    ThinkCallback cb;
    cb.amx = amx;
    cb.funcidx = funcidx;

    TouchKey key;
    key.toucher = toucherClass;
    key.touched = touchedClass;
    m_touches[key] = cb;
    AMXX_LOG("[ThinkTouch] Registered touch: '%s' x '%s' (funcidx=%d)", toucherClass, touchedClass, funcidx);
}

void AMXXThinkTouch::UnregisterByAMX(AMX *amx)
{
    for (auto it = m_thinks.begin(); it != m_thinks.end(); ) {
        if (it->second.amx == amx)
            it = m_thinks.erase(it);
        else
            ++it;
    }
    for (auto it = m_touches.begin(); it != m_touches.end(); ) {
        if (it->second.amx == amx)
            it = m_touches.erase(it);
        else
            ++it;
    }
}

void AMXXThinkTouch::UnregisterThink(const char *classname)
{
    if (!classname)
        return;
    m_thinks.erase(classname);
}

void AMXXThinkTouch::UnregisterTouch(const char *toucherClass, const char *touchedClass)
{
    TouchKey key;
    key.toucher = toucherClass ? toucherClass : "";
    key.touched = touchedClass ? touchedClass : "";
    m_touches.erase(key);
}

const ThinkCallback *AMXXThinkTouch::FindThink(const char *classname) const
{
    auto it = m_thinks.find(classname);
    return (it != m_thinks.end()) ? &it->second : nullptr;
}

const ThinkCallback *AMXXThinkTouch::FindTouch(const char *toucherClass, const char *touchedClass) const
{
    TouchKey key;
    key.toucher = toucherClass;
    key.touched = touchedClass;
    auto it = m_touches.find(key);
    return (it != m_touches.end()) ? &it->second : nullptr;
}

static void Hook_DispatchThink(edict_t *pent)
{
    // P1-9: FM_Think forward（全局，所有实体）
    if (FireFMForwardThink(ENTINDEX(pent)))
        return;  // 插件 supercede，跳过 classname 回调与原始 Think

    // 调用 AMX think 回调
    const char *classname = STRING(pent->v.classname);
    const ThinkCallback *cb = AMXXThinkTouch::GetInstance().FindThink(classname);
    if (cb && cb->amx) {
        cell retval;
        amx_Push(cb->amx, ENTINDEX(pent));
        amx_Exec(cb->amx, &retval, cb->funcidx);
        // 插件返回 PLUGIN_HANDLED 时跳过引擎原始 Think
        if (retval >= PLUGIN_HANDLED)
            return;
    }

    // 调用原始 DispatchThink
    if (g_origThink)
        g_origThink(pent);
}

static void Hook_DispatchTouch(edict_t *pentTouched, edict_t *pentOther)
{
    // P1-9: FM_Touch forward（全局，所有实体）
    if (FireFMForwardTouch(ENTINDEX(pentTouched), ENTINDEX(pentOther)))
        return;  // 插件 supercede，跳过 classname 回调与原始 Touch

    // 调用 AMX touch 回调
    const char *toucher = STRING(pentOther->v.classname);
    const char *touched = STRING(pentTouched->v.classname);
    const ThinkCallback *cb = AMXXThinkTouch::GetInstance().FindTouch(toucher, touched);
    if (cb && cb->amx) {
        cell retval;
        amx_Push(cb->amx, ENTINDEX(pentTouched));
        amx_Push(cb->amx, ENTINDEX(pentOther));
        amx_Exec(cb->amx, &retval, cb->funcidx);
        // 插件返回 PLUGIN_HANDLED 时跳过引擎原始 Touch
        if (retval >= PLUGIN_HANDLED)
            return;
    }

    // 调用原始 DispatchTouch
    if (g_origTouch)
        g_origTouch(pentTouched, pentOther);
}

// P1-9: FM_Spawn forward（包装 DispatchSpawn）
static int Hook_DispatchSpawn(edict_t *pent)
{
    if (FireFMForwardSpawn(ENTINDEX(pent)))
        return 0;  // 插件 supercede，跳过原始 Spawn

    if (g_origSpawn)
        return g_origSpawn(pent);
    return 0;
}

// P1-9: FM_SetModel forward（包装 engfuncs pfnSetModel）
static void Hook_SetModel(edict_t *e, const char *m)
{
    if (!FireFMForwardSetModel(ENTINDEX(e), m)) {
        if (g_origSetModel)
            g_origSetModel(e, m);
    }
}

void AMXXThinkTouch::HookDispatch()
{
    if (m_hooked)
        return;

    g_origThink = gFunctionTable.pfnThink;
    g_origTouch = gFunctionTable.pfnTouch;
    g_origSpawn = gFunctionTable.pfnSpawn;

    gFunctionTable.pfnThink = Hook_DispatchThink;
    gFunctionTable.pfnTouch = Hook_DispatchTouch;
    gFunctionTable.pfnSpawn = Hook_DispatchSpawn;

    // P1-9: FM_SetModel —— 钩住引擎 pfnSetModel（g_engfuncs 为 DLL 持有的可写副本）
    if (g_engfuncs.pfnSetModel) {
        g_origSetModel = g_engfuncs.pfnSetModel;
        g_engfuncs.pfnSetModel = Hook_SetModel;
    }

    m_hooked = true;
}

void AMXXThinkTouch::UnhookDispatch()
{
    if (!m_hooked)
        return;

    gFunctionTable.pfnThink = g_origThink;
    gFunctionTable.pfnTouch = g_origTouch;
    gFunctionTable.pfnSpawn = g_origSpawn;

    if (g_origSetModel)
        g_engfuncs.pfnSetModel = g_origSetModel;

    g_origThink = nullptr;
    g_origTouch = nullptr;
    g_origSpawn = nullptr;
    g_origSetModel = nullptr;
    m_hooked = false;
}
