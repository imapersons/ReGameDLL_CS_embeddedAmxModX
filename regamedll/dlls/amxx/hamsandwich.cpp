#include "precompiled.h"

#include "hamsandwich.h"
#include "../extdll.h"
#include "../player.h"
#include "../cbase.h"
#include "../API/CAPI_Impl.h"
#include <cstdarg>

// Forward declarations for ReGame hookchain bridge functions
void HamHook_Spawn(IHookChainClass<void, CBasePlayer> *chain, CBasePlayer *pPlayer);
BOOL HamHook_TakeDamage(IHookChainClass<BOOL, CBasePlayer, entvars_t *, entvars_t *, float &, int> *chain,
    CBasePlayer *pPlayer, entvars_t *pevInflictor, entvars_t *pevAttacker, float &flDamage, int bitsDamageType);
void HamHook_Killed(IHookChainClass<void, CBasePlayer, entvars_t *, int> *chain,
    CBasePlayer *pPlayer, entvars_t *pevAttacker, int iGib);
void HamHook_PreThink(IHookChainClass<void, CBasePlayer> *chain, CBasePlayer *pPlayer);
void HamHook_PostThink(IHookChainClass<void, CBasePlayer> *chain, CBasePlayer *pPlayer);
int HamHook_ObjectCaps(IHookChainClass<int, CBasePlayer> *chain, CBasePlayer *pPlayer);
BOOL HamHook_Weapon_CanDeploy(IHookChainClass<BOOL, CBasePlayerWeapon> *chain, CBasePlayerWeapon *pWeapon);
int HamHook_Weapon_DefaultReload(IHookChainClass<int, CBasePlayerWeapon, int, int, float> *chain,
    CBasePlayerWeapon *pWeapon, int iClipSize, int iAnimTime, float fDelay);
void HamHook_ItemPostFrame(IHookChainClass<void, CBasePlayerWeapon> *chain, CBasePlayerWeapon *pWeapon);
void HamHook_Grenade_ExplodeHe(IHookChainClass<void, CGrenade, TraceResult *, int> *chain,
    CGrenade *pGrenade, TraceResult *pTrace, int iUnused);
void HamHook_CBasePlayer_Jump(IHookChainClass<void, CBasePlayer> *chain, CBasePlayer *pPlayer);
void HamHook_CBasePlayer_Duck(IHookChainClass<void, CBasePlayer> *chain, CBasePlayer *pPlayer);
void HamHook_CBasePlayer_RoundRespawn(IHookChainClass<void, CBasePlayer> *chain, CBasePlayer *pPlayer);
void HamHook_TraceAttack(IHookChainClass<void, CBasePlayer, entvars_t *, float, Vector &, TraceResult *, int> *chain,
    CBasePlayer *pPlayer, entvars_t *pevAttacker, float flDamage, Vector &vecDir, TraceResult *ptr, int bitsDamageType);
BOOL HamHook_AddPlayerItem(IHookChainClass<BOOL, CBasePlayer, CBasePlayerItem *> *chain,
    CBasePlayer *pPlayer, CBasePlayerItem *pItem);

AMXXHamSandwich &AMXXHamSandwich::GetInstance()
{
    static AMXXHamSandwich instance;
    return instance;
}

// Global context backing SetHamParam*
HamDispatchContext g_currentHamCtx;
// Ham return context (shared by SetHamReturn*/GetOrigHamReturn* and bridge functions)
HamCtx g_hamCtx;
HamCtx g_hamOrigCtx;

bool AMXXHamSandwich::HasCallbacks(HamHookType type) const
{
    auto it = m_callbacks.find(type);
    return it != m_callbacks.end() && !it->second.empty();
}

void AMXXHamSandwich::Init()
{
    AMXX_LOG_DBG("[Ham] Initializing Ham Sandwich system...");
    m_callbacks.clear();
    m_nextForwardId = 1;

    // Register ReGame hookchain hooks (coexist independently with the hooks in amxx_hooks.cpp)
    g_ReGameHookchains.m_CBasePlayer_Spawn.registerHook(HamHook_Spawn, HC_PRIORITY_DEFAULT);
    g_ReGameHookchains.m_CBasePlayer_TakeDamage.registerHook(HamHook_TakeDamage, HC_PRIORITY_DEFAULT);
    g_ReGameHookchains.m_CBasePlayer_Killed.registerHook(HamHook_Killed, HC_PRIORITY_DEFAULT);
    g_ReGameHookchains.m_CBasePlayer_PreThink.registerHook(HamHook_PreThink, HC_PRIORITY_DEFAULT);
    g_ReGameHookchains.m_CBasePlayer_PostThink.registerHook(HamHook_PostThink, HC_PRIORITY_DEFAULT);

    // Additional Ham hooks: register only hookchains available in ReGame
    g_ReGameHookchains.m_CBasePlayer_ObjectCaps.registerHook(HamHook_ObjectCaps, HC_PRIORITY_DEFAULT);
    g_ReGameHookchains.m_CBasePlayerWeapon_CanDeploy.registerHook(HamHook_Weapon_CanDeploy, HC_PRIORITY_DEFAULT);
    g_ReGameHookchains.m_CBasePlayerWeapon_DefaultReload.registerHook(HamHook_Weapon_DefaultReload, HC_PRIORITY_DEFAULT);
    g_ReGameHookchains.m_CBasePlayerWeapon_ItemPostFrame.registerHook(HamHook_ItemPostFrame, HC_PRIORITY_DEFAULT);
    g_ReGameHookchains.m_CGrenade_ExplodeHeGrenade.registerHook(HamHook_Grenade_ExplodeHe, HC_PRIORITY_DEFAULT);
    g_ReGameHookchains.m_CBasePlayer_Jump.registerHook(HamHook_CBasePlayer_Jump, HC_PRIORITY_DEFAULT);
    g_ReGameHookchains.m_CBasePlayer_Duck.registerHook(HamHook_CBasePlayer_Duck, HC_PRIORITY_DEFAULT);
    g_ReGameHookchains.m_CBasePlayer_RoundRespawn.registerHook(HamHook_CBasePlayer_RoundRespawn, HC_PRIORITY_DEFAULT);

    // TraceAttack and AddPlayerItem (hookchains provided by ReGame)
    g_ReGameHookchains.m_CBasePlayer_TraceAttack.registerHook(HamHook_TraceAttack, HC_PRIORITY_DEFAULT);
    g_ReGameHookchains.m_CBasePlayer_AddPlayerItem.registerHook(HamHook_AddPlayerItem, HC_PRIORITY_DEFAULT);

    AMXX_LOG_DBG("[Ham] Ham Sandwich initialized (16 ReGame hooks registered)");
}

void AMXXHamSandwich::Shutdown()
{
    g_ReGameHookchains.m_CBasePlayer_Spawn.unregisterHook(HamHook_Spawn);
    g_ReGameHookchains.m_CBasePlayer_TakeDamage.unregisterHook(HamHook_TakeDamage);
    g_ReGameHookchains.m_CBasePlayer_Killed.unregisterHook(HamHook_Killed);
    g_ReGameHookchains.m_CBasePlayer_PreThink.unregisterHook(HamHook_PreThink);
    g_ReGameHookchains.m_CBasePlayer_PostThink.unregisterHook(HamHook_PostThink);

    g_ReGameHookchains.m_CBasePlayer_ObjectCaps.unregisterHook(HamHook_ObjectCaps);
    g_ReGameHookchains.m_CBasePlayerWeapon_CanDeploy.unregisterHook(HamHook_Weapon_CanDeploy);
    g_ReGameHookchains.m_CBasePlayerWeapon_DefaultReload.unregisterHook(HamHook_Weapon_DefaultReload);
    g_ReGameHookchains.m_CBasePlayerWeapon_ItemPostFrame.unregisterHook(HamHook_ItemPostFrame);
    g_ReGameHookchains.m_CGrenade_ExplodeHeGrenade.unregisterHook(HamHook_Grenade_ExplodeHe);
    g_ReGameHookchains.m_CBasePlayer_Jump.unregisterHook(HamHook_CBasePlayer_Jump);
    g_ReGameHookchains.m_CBasePlayer_Duck.unregisterHook(HamHook_CBasePlayer_Duck);
    g_ReGameHookchains.m_CBasePlayer_RoundRespawn.unregisterHook(HamHook_CBasePlayer_RoundRespawn);

    g_ReGameHookchains.m_CBasePlayer_TraceAttack.unregisterHook(HamHook_TraceAttack);
    g_ReGameHookchains.m_CBasePlayer_AddPlayerItem.unregisterHook(HamHook_AddPlayerItem);

    m_callbacks.clear();
    AMXX_LOG_DBG("[Ham] Ham Sandwich shut down");
}

int AMXXHamSandwich::RegisterHam(HamHookType type, AMX *amx, cell funcidx, bool post, bool specialbot)
{
    HamCallback cb;
    cb.amx = amx;
    cb.funcidx = funcidx;
    cb.enabled = true;
    cb.forwardId = m_nextForwardId++;
    cb.post = post;
    cb.specialbot = specialbot;
    m_callbacks[type].push_back(cb);

    const char *name;
    switch (type) {
        case Ham_Spawn:       name = "Spawn";       break;
        case Ham_Think:       name = "Think";       break;
        case Ham_Touch:       name = "Touch";       break;
        case Ham_TakeDamage:  name = "TakeDamage";  break;
        case Ham_Killed:      name = "Killed";      break;
        case Ham_TraceAttack: name = "TraceAttack"; break;
        case Ham_PreThink:    name = "PreThink";    break;
        case Ham_PostThink:   name = "PostThink";   break;
        case Ham_Use:         name = "Use";         break;
        case Ham_Blocked:     name = "Blocked";     break;
        case Ham_ObjectCaps:  name = "ObjectCaps";  break;
        case Ham_Weapon_PrimaryAttack:   name = "Weapon_PrimaryAttack";   break;
        case Ham_Weapon_SecondaryAttack: name = "Weapon_SecondaryAttack"; break;
        case Ham_Weapon_Reload:          name = "Weapon_Reload";          break;
        // Ham_Weapon_Deploy/Ham_Item_Deploy share the value 66; Ham_Weapon_Holster/Ham_Item_Holster share 68
        case Ham_Weapon_Deploy:          name = "Weapon_Deploy";          break;
        case Ham_Weapon_Holster:         name = "Weapon_Holster";         break;
        case Ham_Item_PreFrame:          name = "Item_PreFrame";          break;
        case Ham_Item_PostFrame:         name = "Item_PostFrame";         break;
        case Ham_Grenade_Explode:        name = "Grenade_Explode";        break;
        case Ham_CBasePlayer_Jump:       name = "CBasePlayer_Jump";       break;
        case Ham_CBasePlayer_Duck:       name = "CBasePlayer_Duck";       break;
        case Ham_CBasePlayer_RoundRespawn: name = "CBasePlayer_RoundRespawn"; break;
        default:              name = "Unknown";     break;
    }
    AMXX_LOG_DBG("[Ham] Registered Ham_%s (funcidx=%d, forwardId=%d, post=%d)", name, funcidx, cb.forwardId, post ? 1 : 0);
    return cb.forwardId;
}

void AMXXHamSandwich::UnregisterByAMX(AMX *amx)
{
    for (auto &pair : m_callbacks) {
        auto &vec = pair.second;
        for (auto it = vec.begin(); it != vec.end(); ) {
            if (it->amx == amx)
                it = vec.erase(it);
            else
                ++it;
        }
    }
}

// Linear search for a callback by forwardId (callbacks are few, so linear is fine)
static HamCallback *FindCallbackById(std::map<HamHookType, std::vector<HamCallback>> &callbacks, int forwardId)
{
    for (auto &pair : callbacks) {
        for (auto &cb : pair.second) {
            if (cb.forwardId == forwardId)
                return &cb;
        }
    }
    return nullptr;
}

static const HamCallback *FindCallbackByIdConst(const std::map<HamHookType, std::vector<HamCallback>> &callbacks, int forwardId)
{
    for (const auto &pair : callbacks) {
        for (const auto &cb : pair.second) {
            if (cb.forwardId == forwardId)
                return &cb;
        }
    }
    return nullptr;
}

bool AMXXHamSandwich::EnableForward(int forwardId)
{
    HamCallback *cb = FindCallbackById(m_callbacks, forwardId);
    if (!cb) return false;
    cb->enabled = true;
    return true;
}

bool AMXXHamSandwich::DisableForward(int forwardId)
{
    HamCallback *cb = FindCallbackById(m_callbacks, forwardId);
    if (!cb) return false;
    cb->enabled = false;
    return true;
}

bool AMXXHamSandwich::IsForwardValid(int forwardId) const
{
    return FindCallbackByIdConst(m_callbacks, forwardId) != nullptr;
}

// IsHamValid: the function number is within [0, Ham_CS_RoundRespawn] and the hook has a bridge function (in the dispatch table)
bool AMXXHamSandwich::IsHamFunctionValid(int func) const
{
    if (func < 0 || func > (int)Ham_CS_RoundRespawn)
        return false;
    switch ((HamHookType)func)
    {
    case Ham_Spawn:
    case Ham_ObjectCaps:
    case Ham_TraceAttack:
    case Ham_TakeDamage:
    case Ham_Killed:
    case Ham_AddPlayerItem:
    case Ham_Player_Jump:
    case Ham_Player_Duck:
    case Ham_Player_PreThink:
    case Ham_Player_PostThink:
    case Ham_Weapon_Deploy:   // 66 = Ham_Item_Deploy
    case Ham_Weapon_Holster:  // 68 = Ham_Item_Holster
    case Ham_Item_PostFrame:  // 71
    case Ham_Weapon_PrimaryAttack:  // 87
    case Ham_Weapon_SecondaryAttack: // 88
    case Ham_Weapon_Reload:   // 89
    case Ham_CS_RoundRespawn: // 98
        return true;
    default:
        return false;
    }
}

// Refresh params[] from g_currentHamCtx (propagates SetHamParam*-modified values across the plugin chain)
static void RefreshParamsFromCtx(cell *params, int paramCount)
{
    if (!g_currentHamCtx.active || g_currentHamCtx.paramCount < paramCount)
        return;
    for (int j = 0; j < paramCount && j < 8; j++)
    {
        auto &slot = g_currentHamCtx.params[j];
        if (!slot.ptr) continue;
        switch (slot.type)
        {
        case HamParamSlot::PT_INT:
        case HamParamSlot::PT_ENTITY:
            params[j] = *(int *)slot.ptr;
            break;
        case HamParamSlot::PT_FLOAT:
            params[j] = amx_ftoc(*(float *)slot.ptr);
            break;
        default:
            break;
        }
    }
}

cell AMXXHamSandwich::DispatchHam(HamHookType type, int paramCount, ...)
{
    cell params[8];
    va_list args;
    va_start(args, paramCount);
    for (int i = 0; i < paramCount && i < 8; i++)
        params[i] = va_arg(args, cell);
    va_end(args);

    // Aggregate return codes from all plugins. HAM_SUPERCEDE/HAM_OVERRIDE stick once seen
    // (a later HAM_HANDLED/HAM_IGNORED cannot downgrade them); otherwise HANDLED wins over IGNORED.
    cell result = HAM_IGNORED;
    auto &cbs = m_callbacks[type];
    for (auto &cb : cbs) {
        if (cb.amx && cb.enabled) {
            RefreshParamsFromCtx(params, paramCount);
            for (int j = paramCount - 1; j >= 0; j--)
                amx_Push(cb.amx, params[j]);
            cell retval;
            amx_Exec(cb.amx, &retval, cb.funcidx);
            if (retval == HAM_SUPERCEDE || retval == HAM_OVERRIDE)
                result = retval;
            else if (retval == HAM_HANDLED && result == HAM_IGNORED)
                result = HAM_HANDLED;
        }
    }
    return result;
}

bool AMXXHamSandwich::DispatchHamBlocking(HamHookType type, int paramCount, ...)
{
    cell params[8];
    va_list args;
    va_start(args, paramCount);
    for (int i = 0; i < paramCount && i < 8; i++)
        params[i] = va_arg(args, cell);
    va_end(args);

    // Only HAM_SUPERCEDE/HAM_OVERRIDE block the original function;
    // HAM_HANDLED(3) is not treated as blocking.
    bool blocked = false;
    auto &cbs = m_callbacks[type];
    for (auto &cb : cbs) {
        if (cb.amx && cb.enabled) {
            RefreshParamsFromCtx(params, paramCount);
            for (int j = paramCount - 1; j >= 0; j--)
                amx_Push(cb.amx, params[j]);
            cell retval;
            amx_Exec(cb.amx, &retval, cb.funcidx);
            if (retval == HAM_SUPERCEDE || retval == HAM_OVERRIDE)
                blocked = true;
        }
    }
    return blocked;
}

// ===== ReGame hookchain bridge functions =====

void HamHook_Spawn(IHookChainClass<void, CBasePlayer> *chain, CBasePlayer *pPlayer)
{
    int id = pPlayer ? pPlayer->entindex() : 0;
    cell result = AMXXHamSandwich::GetInstance().DispatchHam(Ham_Spawn, 1, (cell)id);
    // HAM_SUPERCEDE/HAM_OVERRIDE block the original (callNext is not called)
    if (result == HAM_SUPERCEDE || result == HAM_OVERRIDE)
        return;
    chain->callNext(pPlayer);
}

// Original forward signature: (victim, idinflictor, idattacker, Float:damage, damagebits), 5 params
BOOL HamHook_TakeDamage(IHookChainClass<BOOL, CBasePlayer, entvars_t *, entvars_t *, float &, int> *chain,
    CBasePlayer *pPlayer, entvars_t *pevInflictor, entvars_t *pevAttacker, float &flDamage, int bitsDamageType)
{
    int vic = pPlayer ? pPlayer->entindex() : 0;
    int inf = pevInflictor ? ENTINDEX(ENT(pevInflictor)) : 0;
    int att = pevAttacker ? ENTINDEX(ENT(pevAttacker)) : 0;

    // Set up the SetHamParam* context: 1=victim(int) 2=inflictor(int) 3=attacker(int) 4=damage(float) 5=damagebits(int)
    g_currentHamCtx.Clear();
    g_currentHamCtx.hookType = Ham_TakeDamage;
    g_currentHamCtx.paramCount = 5;
    g_currentHamCtx.params[0].type = HamParamSlot::PT_ENTITY; g_currentHamCtx.params[0].ptr = &vic;
    g_currentHamCtx.params[1].type = HamParamSlot::PT_ENTITY; g_currentHamCtx.params[1].ptr = &inf;
    g_currentHamCtx.params[2].type = HamParamSlot::PT_ENTITY; g_currentHamCtx.params[2].ptr = &att;
    g_currentHamCtx.params[3].type = HamParamSlot::PT_FLOAT;  g_currentHamCtx.params[3].ptr = &flDamage;
    g_currentHamCtx.params[4].type = HamParamSlot::PT_INT;    g_currentHamCtx.params[4].ptr = &bitsDamageType;
    g_currentHamCtx.active = true;

    HamCtxReset(g_hamCtx);
    cell result = AMXXHamSandwich::GetInstance().DispatchHam(Ham_TakeDamage, 5,
        (cell)vic, (cell)inf, (cell)att, amx_ftoc(flDamage), (cell)bitsDamageType);

    g_currentHamCtx.active = false;

    // HAM_SUPERCEDE: block the original and return the value set via SetHamReturn*
    if (result == HAM_SUPERCEDE)
        return (BOOL)HamCtxReturnAsCell();

    BOOL ret = chain->callNext(pPlayer, pevInflictor, pevAttacker, flDamage, bitsDamageType);

    // Post stage: save the actual return value of the original for GetOrigHamReturn*
    g_hamOrigCtx.returnType = 1;
    g_hamOrigCtx.intVal = ret;

    // HAM_OVERRIDE: call the original but override its return value with SetHamReturn*
    if (result == HAM_OVERRIDE)
        return (BOOL)HamCtxReturnAsCell();

    return ret;
}

void HamHook_Killed(IHookChainClass<void, CBasePlayer, entvars_t *, int> *chain,
    CBasePlayer *pPlayer, entvars_t *pevAttacker, int iGib)
{
    int vic = pPlayer ? pPlayer->entindex() : 0;
    int att = pevAttacker ? ENTINDEX(ENT(pevAttacker)) : 0;
    cell result = AMXXHamSandwich::GetInstance().DispatchHam(Ham_Killed, 3, (cell)vic, (cell)att, (cell)iGib);
    if (result == HAM_SUPERCEDE || result == HAM_OVERRIDE)
        return;
    chain->callNext(pPlayer, pevAttacker, iGib);
}

void HamHook_PreThink(IHookChainClass<void, CBasePlayer> *chain, CBasePlayer *pPlayer)
{
    int id = pPlayer ? pPlayer->entindex() : 0;
    cell result = AMXXHamSandwich::GetInstance().DispatchHam(Ham_PreThink, 1, (cell)id);
    if (result == HAM_SUPERCEDE || result == HAM_OVERRIDE)
        return;
    chain->callNext(pPlayer);
}

void HamHook_PostThink(IHookChainClass<void, CBasePlayer> *chain, CBasePlayer *pPlayer)
{
    int id = pPlayer ? pPlayer->entindex() : 0;
    cell result = AMXXHamSandwich::GetInstance().DispatchHam(Ham_PostThink, 1, (cell)id);
    if (result == HAM_SUPERCEDE || result == HAM_OVERRIDE)
        return;
    chain->callNext(pPlayer);
}

// ===== New Ham hook bridge functions =====

int HamHook_ObjectCaps(IHookChainClass<int, CBasePlayer> *chain, CBasePlayer *pPlayer)
{
    int id = pPlayer ? pPlayer->entindex() : 0;
    HamCtxReset(g_hamCtx);
    cell result = AMXXHamSandwich::GetInstance().DispatchHam(Ham_ObjectCaps, 1, (cell)id);

    // HAM_SUPERCEDE: block the original and return the value set via SetHamReturn*
    if (result == HAM_SUPERCEDE)
        return (int)HamCtxReturnAsCell();

    int ret = chain->callNext(pPlayer);

    // Post stage: save the actual return value of the original for GetOrigHamReturn*
    g_hamOrigCtx.returnType = 1;
    g_hamOrigCtx.intVal = ret;

    // HAM_OVERRIDE: call the original but override its return value with SetHamReturn*
    if (result == HAM_OVERRIDE)
        return (int)HamCtxReturnAsCell();

    return ret;
}

BOOL HamHook_Weapon_CanDeploy(IHookChainClass<BOOL, CBasePlayerWeapon> *chain, CBasePlayerWeapon *pWeapon)
{
    CBasePlayer *pPlayer = pWeapon ? static_cast<CBasePlayer *>(pWeapon->m_pPlayer) : nullptr;
    int id = pPlayer ? pPlayer->entindex() : 0;
    HamCtxReset(g_hamCtx);
    cell result = AMXXHamSandwich::GetInstance().DispatchHam(Ham_Weapon_Deploy, 1, (cell)id);
    if (result == HAM_SUPERCEDE)
        return (BOOL)HamCtxReturnAsCell();
    BOOL ret = chain->callNext(pWeapon);
    g_hamOrigCtx.returnType = 1;
    g_hamOrigCtx.intVal = ret;
    if (result == HAM_OVERRIDE)
        return (BOOL)HamCtxReturnAsCell();
    return ret;
}

int HamHook_Weapon_DefaultReload(IHookChainClass<int, CBasePlayerWeapon, int, int, float> *chain,
    CBasePlayerWeapon *pWeapon, int iClipSize, int iAnimTime, float fDelay)
{
    CBasePlayer *pPlayer = pWeapon ? static_cast<CBasePlayer *>(pWeapon->m_pPlayer) : nullptr;
    int id = pPlayer ? pPlayer->entindex() : 0;
    HamCtxReset(g_hamCtx);
    cell result = AMXXHamSandwich::GetInstance().DispatchHam(Ham_Weapon_Reload, 1, (cell)id);
    if (result == HAM_SUPERCEDE)
        return (int)HamCtxReturnAsCell();
    int ret = chain->callNext(pWeapon, iClipSize, iAnimTime, fDelay);
    g_hamOrigCtx.returnType = 1;
    g_hamOrigCtx.intVal = ret;
    if (result == HAM_OVERRIDE)
        return (int)HamCtxReturnAsCell();
    return ret;
}

void HamHook_ItemPostFrame(IHookChainClass<void, CBasePlayerWeapon> *chain, CBasePlayerWeapon *pWeapon)
{
    CBasePlayer *pPlayer = pWeapon ? static_cast<CBasePlayer *>(pWeapon->m_pPlayer) : nullptr;
    int id = pPlayer ? pPlayer->entindex() : 0;
    cell result = AMXXHamSandwich::GetInstance().DispatchHam(Ham_Item_PostFrame, 1, (cell)id);
    if (result == HAM_SUPERCEDE || result == HAM_OVERRIDE)
        return;
    chain->callNext(pWeapon);
}

void HamHook_Grenade_ExplodeHe(IHookChainClass<void, CGrenade, TraceResult *, int> *chain,
    CGrenade *pGrenade, TraceResult *pTrace, int iUnused)
{
    int id = pGrenade ? pGrenade->entindex() : 0;
    cell result = AMXXHamSandwich::GetInstance().DispatchHam(Ham_Grenade_Explode, 1, (cell)id);
    if (result == HAM_SUPERCEDE || result == HAM_OVERRIDE)
        return;
    chain->callNext(pGrenade, pTrace, iUnused);
}

void HamHook_CBasePlayer_Jump(IHookChainClass<void, CBasePlayer> *chain, CBasePlayer *pPlayer)
{
    int id = pPlayer ? pPlayer->entindex() : 0;
    cell result = AMXXHamSandwich::GetInstance().DispatchHam(Ham_CBasePlayer_Jump, 1, (cell)id);
    if (result == HAM_SUPERCEDE || result == HAM_OVERRIDE)
        return;
    chain->callNext(pPlayer);
}

void HamHook_CBasePlayer_Duck(IHookChainClass<void, CBasePlayer> *chain, CBasePlayer *pPlayer)
{
    int id = pPlayer ? pPlayer->entindex() : 0;
    cell result = AMXXHamSandwich::GetInstance().DispatchHam(Ham_CBasePlayer_Duck, 1, (cell)id);
    if (result == HAM_SUPERCEDE || result == HAM_OVERRIDE)
        return;
    chain->callNext(pPlayer);
}

void HamHook_CBasePlayer_RoundRespawn(IHookChainClass<void, CBasePlayer> *chain, CBasePlayer *pPlayer)
{
    int id = pPlayer ? pPlayer->entindex() : 0;
    cell result = AMXXHamSandwich::GetInstance().DispatchHam(Ham_CBasePlayer_RoundRespawn, 1, (cell)id);
    if (result == HAM_SUPERCEDE || result == HAM_OVERRIDE)
        return;
    chain->callNext(pPlayer);
}

// Ham_TraceAttack bridge
// Original forward signature: (this, idattacker, Float:damage, Float:direction[3], traceresult, damagebits), 6 params
void HamHook_TraceAttack(IHookChainClass<void, CBasePlayer, entvars_t *, float, Vector &, TraceResult *, int> *chain,
    CBasePlayer *pPlayer, entvars_t *pevAttacker, float flDamage, Vector &vecDir, TraceResult *ptr, int bitsDamageType)
{
    int vic = pPlayer ? pPlayer->entindex() : 0;
    int att = pevAttacker ? ENTINDEX(ENT(pevAttacker)) : 0;

    // Set up the SetHamParam* context: 1=victim 2=attacker 3=damage(float) 4=direction(vector) 5=traceresult 6=damagebits
    // No slot for traceresult (5): SetHamParamTraceResult is not implemented, avoids writing a pointer as an int
    g_currentHamCtx.Clear();
    g_currentHamCtx.hookType = Ham_TraceAttack;
    g_currentHamCtx.paramCount = 6;
    g_currentHamCtx.params[0].type = HamParamSlot::PT_ENTITY; g_currentHamCtx.params[0].ptr = &vic;
    g_currentHamCtx.params[1].type = HamParamSlot::PT_ENTITY; g_currentHamCtx.params[1].ptr = &att;
    g_currentHamCtx.params[2].type = HamParamSlot::PT_FLOAT;  g_currentHamCtx.params[2].ptr = &flDamage;
    g_currentHamCtx.params[3].type = HamParamSlot::PT_VECTOR; g_currentHamCtx.params[3].ptr = &vecDir;
    g_currentHamCtx.params[5].type = HamParamSlot::PT_INT;    g_currentHamCtx.params[5].ptr = &bitsDamageType;
    g_currentHamCtx.active = true;

    HamCtxReset(g_hamCtx);
    cell result = AMXXHamSandwich::GetInstance().DispatchHam(Ham_TraceAttack, 8,
        (cell)vic, (cell)att, amx_ftoc(flDamage),
        amx_ftoc(vecDir.x), amx_ftoc(vecDir.y), amx_ftoc(vecDir.z),
        (cell)(intptr_t)ptr, (cell)bitsDamageType);

    g_currentHamCtx.active = false;

    if (result == HAM_SUPERCEDE || result == HAM_OVERRIDE)
        return;
    chain->callNext(pPlayer, pevAttacker, flDamage, vecDir, ptr, bitsDamageType);
}

// Ham_AddPlayerItem bridge
// Original signature: Ham_AddPlayerItem(this=player, itemId) returns BOOL
BOOL HamHook_AddPlayerItem(IHookChainClass<BOOL, CBasePlayer, CBasePlayerItem *> *chain,
    CBasePlayer *pPlayer, CBasePlayerItem *pItem)
{
    int id = pPlayer ? pPlayer->entindex() : 0;
    int itemId = pItem ? pItem->entindex() : 0;

    g_currentHamCtx.Clear();
    g_currentHamCtx.hookType = Ham_AddPlayerItem;
    g_currentHamCtx.paramCount = 2;
    g_currentHamCtx.params[0].type = HamParamSlot::PT_ENTITY; g_currentHamCtx.params[0].ptr = &id;
    g_currentHamCtx.params[1].type = HamParamSlot::PT_ENTITY; g_currentHamCtx.params[1].ptr = &itemId;
    g_currentHamCtx.active = true;

    HamCtxReset(g_hamCtx);
    cell result = AMXXHamSandwich::GetInstance().DispatchHam(Ham_AddPlayerItem, 2,
        (cell)id, (cell)itemId);

    g_currentHamCtx.active = false;

    if (result == HAM_SUPERCEDE)
        return (BOOL)HamCtxReturnAsCell();
    BOOL ret = chain->callNext(pPlayer, pItem);
    g_hamOrigCtx.returnType = 1;
    g_hamOrigCtx.intVal = ret;
    if (result == HAM_OVERRIDE)
        return (BOOL)HamCtxReturnAsCell();
    return ret;
}
