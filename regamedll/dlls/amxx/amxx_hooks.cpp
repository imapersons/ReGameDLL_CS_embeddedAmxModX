#include "precompiled.h"

#include "amxx_hooks.h"
#include "runtime.h"
#include "forwards.h"
#include "native_entities.h"
#include "native_csx.h"
#include "player.h"
#include "../weapons.h"
#include "../gamerules.h"
#include "../enginecallback.h"

/* =========================================================================
 * 【兼容性说明】ReGameDLL Hookchains ↔ Xash3D DLL_FUNCTIONS 事件层
 * -------------------------------------------------------------------------
 * 当前 amxx_hooks.cpp 通过 ReGameDLL 提供的 g_ReGameHookchains 接口注册
 * 游戏事件回调。这套机制在原版 ReGameDLL (Windows PC mp.dll) 中已验证可用。
 *
 * 若要移植到 Android Xash3D / 标准 GoldSrc mp.dll（无 ReGame Hookchains），
 * 需要改用以下任一替代路径来触发相同的 AMXX forwards：
 *
 * 方案 A：DLL_FUNCTIONS 导出表代理（推荐 Xash3D 路径）
 *   在 Server DLL 的 GiveFnptrsToDll() 中保存原始函数指针，然后替换为：
 *     DLL_FUNCTIONS.pfnSpawn          → 代理 → 触发 player_spawn
 *     DLL_FUNCTIONS.pfnClientConnect  → 代理 → 触发 client_connect
 *     DLL_FUNCTIONS.pfnClientPutInServer → 代理 → 触发 client_putinserver
 *     DLL_FUNCTIONS.pfnClientDisconnect → 代理 → 触发 client_disconnect
 *     DLL_FUNCTIONS.pfnClientCommand  → 代理 → 触发 client_command
 *     DLL_FUNCTIONS.pfnThink          → 代理 → Think/实体事件
 *   TakeDamage/Killed 事件需要在 CBaseMonster / CBasePlayer 子类中手动
 *   在对应虚函数调用末尾转发。
 *
 * 方案 B：引擎事件 + 游戏消息 Hook (Xash message system)
 *   通过 hook REG_USER_MSG("DeathMsg", ...) 监听玩家死亡消息，
 *   解析参数后触发 client_death / player_death forwards。
 *   通过 hook "Damage" message / TraceAttack 配合来触发 client_damage。
 *
 * 方案 C（现状）：ReGameDLL Hookchains — 目前最完整、最稳定。
 *   所有 m_CBasePlayer_Spawn / m_CBasePlayer_TakeDamage 等 hook 都
 *   在下方 AMXXHooks::Init() 中注册。
 * =======================================================================*/

// player_spawn(id)
static void Hook_CBasePlayer_Spawn(IHookChainClass<void, CBasePlayer> *chain, CBasePlayer *pPlayer)
{
    ForwardCallParam params[1];
    params[0].type = FP_CELL;
    params[0].val = pPlayer ? pPlayer->entindex() : 0;
    params[0].size = 0;

    AMXXRuntime::GetInstance().ExecuteForwardEx("player_spawn", 1, params);
    chain->callNext(pPlayer);

    // FM_PlayerSpawn forward
    FireFMForward(FM_PlayerSpawn, pPlayer ? pPlayer->entindex() : 0);
}

// 辅助：获取攻击者的 CBasePlayer（用于获取武器 ID 和队伍信息）
static CBasePlayer *GetPlayerByEdictVars(entvars_t *pev)
{
    if (!pev) return nullptr;
    edict_t *pEdict = ENT(pev);
    if (!pEdict || !pEdict->pvPrivateData) return nullptr;
    return GET_PRIVATE<CBasePlayer>(pEdict);
}

// player_hurt(id, attacker, damage, armor, type) — 自定义 forward（保留）
// client_damage(attacker, victim, damage, weapon, hitplace, TK) — CSX 兼容 forward（参数顺序不同！）
static BOOL Hook_CBasePlayer_TakeDamage(IHookChainClass<BOOL, CBasePlayer, entvars_t *, entvars_t *, float &, int> *chain,
    CBasePlayer *pPlayer, entvars_t *pevInflictor, entvars_t *pevAttacker, float &flDamage, int bitsDamageType)
{
    (void)pevInflictor;
    int victimId = pPlayer ? pPlayer->entindex() : 0;
    int attackerId = pevAttacker ? ENTINDEX(ENT(pevAttacker)) : 0;

    // 记录伤害数据
    int wpnId = 0;
    CBasePlayer *pAttackerPlayer = GetPlayerByEdictVars(pevAttacker);
    if (pAttackerPlayer && pAttackerPlayer->m_pActiveItem) {
        wpnId = pAttackerPlayer->m_pActiveItem->m_iId;
    }
    AMXXRuntime::GetInstance().SetLastDamage(victimId, attackerId, (int)flDamage, wpnId);

    // ====== 自定义 forward: player_hurt(id, attacker, damage, armor, type) ======
    ForwardCallParam paramsHurt[5];
    paramsHurt[0].type = FP_CELL; paramsHurt[0].val = victimId; paramsHurt[0].size = 0;
    paramsHurt[1].type = FP_CELL; paramsHurt[1].val = attackerId; paramsHurt[1].size = 0;
    paramsHurt[2].type = FP_FLOAT; paramsHurt[2].fval = flDamage; paramsHurt[2].size = 0;
    paramsHurt[3].type = FP_CELL; paramsHurt[3].val = 0; paramsHurt[3].size = 0; // armor
    paramsHurt[4].type = FP_CELL; paramsHurt[4].val = bitsDamageType; paramsHurt[4].size = 0;
    int result = AMXXRuntime::GetInstance().ExecuteForwardEx("player_hurt", 5, paramsHurt);

    // ====== CSX 兼容 forward: client_damage(attacker, victim, damage, weapon, hitplace, TK) ======
    // hitplace (hitgroup)：CBasePlayer::TakeDamage() 使用旧式签名
    //   (entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType)
    // 不携带 hitgroup 信息。后续若有 TraceAttack hook 可从中获取 TraceResult.iHitgroup。
    // 当前折衷：传递 HIT_GENERIC=0，CSX 插件读到的 hitplace 始终为 0。
    // TK (Team Kill/Attack)：同队伍攻击（若攻击者和受害者都是玩家且同队）
    int hitgroup = 0;
    int isTK = 0;
    if (pPlayer && pAttackerPlayer && attackerId != victimId) {
        if (pPlayer->m_iTeam != UNASSIGNED && pPlayer->m_iTeam == pAttackerPlayer->m_iTeam) {
            isTK = 1;
        }
    }

    // ====== csstats: record per-player damage/hit/bodyhit stats ======
    CsstatsRecordDamage(attackerId, victimId, wpnId, (int)flDamage, hitgroup);

    ForwardCallParam paramsDamage[6];
    paramsDamage[0].type = FP_CELL; paramsDamage[0].val = attackerId; paramsDamage[0].size = 0;
    paramsDamage[1].type = FP_CELL; paramsDamage[1].val = victimId; paramsDamage[1].size = 0;
    paramsDamage[2].type = FP_FLOAT; paramsDamage[2].fval = flDamage; paramsDamage[2].size = 0;
    paramsDamage[3].type = FP_CELL; paramsDamage[3].val = wpnId; paramsDamage[3].size = 0;
    paramsDamage[4].type = FP_CELL; paramsDamage[4].val = hitgroup; paramsDamage[4].size = 0;
    paramsDamage[5].type = FP_CELL; paramsDamage[5].val = isTK; paramsDamage[5].size = 0;
    int result2 = AMXXRuntime::GetInstance().ExecuteForwardEx("client_damage", 6, paramsDamage);

    if (result == PLUGIN_HANDLED || result == PLUGIN_STOP ||
        result2 == PLUGIN_HANDLED || result2 == PLUGIN_STOP) {
        return FALSE;
    }

    return chain->callNext(pPlayer, pevInflictor, pevAttacker, flDamage, bitsDamageType);
}

// player_death(victim, attacker, weapon, headshot) — 自定义 forward（保留）
// client_death(killer, victim, weapon, hitplace, TK) — CSX 兼容 forward（参数顺序与含义不同！）
static void Hook_CBasePlayer_Killed(IHookChainClass<void, CBasePlayer, entvars_t *, int> *chain,
    CBasePlayer *pPlayer, entvars_t *pevAttacker, int iGib)
{
    int victimId = pPlayer ? pPlayer->entindex() : 0;
    int attackerId = pevAttacker ? ENTINDEX(ENT(pevAttacker)) : 0;

    // ====== 通用数据准备 ======
    bool headshot = false;
    int hitplace = 0; // HIT_GENERIC=0
    if (pPlayer) {
        headshot = pPlayer->m_bHeadshotKilled;
        if (headshot) hitplace = 1; // HIT_HEAD=1
    }

    // 武器 ID：取 *攻击者* 的武器（原代码错误地取受害者武器）
    int killerWeaponId = 0;
    CBasePlayer *pAttackerPlayer = GetPlayerByEdictVars(pevAttacker);
    if (pAttackerPlayer && pAttackerPlayer->m_pActiveItem) {
        killerWeaponId = pAttackerPlayer->m_pActiveItem->m_iId;
    }

    // TK (Team Kill)：同队伍击杀（若攻击者和受害者都是玩家且同队）
    int isTK = 0;
    if (pPlayer && pAttackerPlayer && attackerId != victimId) {
        if (pPlayer->m_iTeam != UNASSIGNED && pPlayer->m_iTeam == pAttackerPlayer->m_iTeam) {
            isTK = 1;
        }
    }

    // ====== csstats: record per-player kill/death/headshot/TK stats ======
    CsstatsRecordDeath(attackerId, victimId, killerWeaponId, headshot, isTK != 0);

    // ====== 自定义 forward: player_death(victim, attacker, weapon, headshot) ======
    ForwardCallParam paramsDeath[4];
    paramsDeath[0].type = FP_CELL; paramsDeath[0].val = victimId; paramsDeath[0].size = 0;
    paramsDeath[1].type = FP_CELL; paramsDeath[1].val = attackerId; paramsDeath[1].size = 0;
    paramsDeath[2].type = FP_CELL; paramsDeath[2].val = killerWeaponId; paramsDeath[2].size = 0;
    paramsDeath[3].type = FP_CELL; paramsDeath[3].val = headshot ? 1 : 0; paramsDeath[3].size = 0;
    int result = AMXXRuntime::GetInstance().ExecuteForwardEx("player_death", 4, paramsDeath);

    // ====== CSX 兼容 forward: client_death(killer, victim, weapon, hitplace, TK) ======
    ForwardCallParam paramsClientDeath[5];
    paramsClientDeath[0].type = FP_CELL; paramsClientDeath[0].val = attackerId; paramsClientDeath[0].size = 0;
    paramsClientDeath[1].type = FP_CELL; paramsClientDeath[1].val = victimId; paramsClientDeath[1].size = 0;
    paramsClientDeath[2].type = FP_CELL; paramsClientDeath[2].val = killerWeaponId; paramsClientDeath[2].size = 0;
    paramsClientDeath[3].type = FP_CELL; paramsClientDeath[3].val = hitplace; paramsClientDeath[3].size = 0;
    paramsClientDeath[4].type = FP_CELL; paramsClientDeath[4].val = isTK; paramsClientDeath[4].size = 0;
    int result2 = AMXXRuntime::GetInstance().ExecuteForwardEx("client_death", 5, paramsClientDeath);

    if (result != PLUGIN_HANDLED && result != PLUGIN_STOP &&
        result2 != PLUGIN_HANDLED && result2 != PLUGIN_STOP) {
        chain->callNext(pPlayer, pevAttacker, iGib);
    }

    // FM_PlayerKilled forward
    FireFMForward(FM_PlayerKilled, victimId);
}

// client_infochanged(id) / client_userinfochanged(id)
static void Hook_ClientUserInfoChanged(IHookChain<void, CBasePlayer *, char *> *chain,
    CBasePlayer *pPlayer, char *infobuffer)
{
    ForwardCallParam params[1];
    params[0].type = FP_CELL;
    params[0].val = pPlayer ? pPlayer->entindex() : 0;
    params[0].size = 0;

    AMXXRuntime::GetInstance().ExecuteForwardEx("client_infochanged", 1, params);
    AMXXRuntime::GetInstance().ExecuteForwardEx("client_userinfochanged", 1, params);
    chain->callNext(pPlayer, infobuffer);
}

// client_command(id, level, cid) — 通过 InternalCommand hook 触发
static void Hook_InternalCommand(IHookChain<void, edict_t *, const char *, const char *> *chain,
    edict_t *pEntity, const char *command, const char *arg1)
{
    ForwardCallParam params[1];
    params[0].type = FP_CELL;
    params[0].val = pEntity ? ENTINDEX(pEntity) : 0;
    params[0].size = 0;

    int result = AMXXRuntime::GetInstance().ExecuteForwardEx("client_command", 1, params);
    if (result == PLUGIN_HANDLED || result == PLUGIN_STOP) {
        return;
    }

    chain->callNext(pEntity, command, arg1);
}

// round_start() / round_restart() — 通过 CSGameRules_RestartRound hook 触发
static void Hook_RestartRound(IHookChain<void> *chain)
{
    // csstats: clear per-round weapon/attacker/victim stats at the start of a new round.
    CsstatsResetRound();
    AMXXRuntime::GetInstance().ExecuteForward("round_start", 0);
    AMXXRuntime::GetInstance().ExecuteForward("round_restart", 0);
    chain->callNext();
}

// round_end(winStatus, event, tmDelay) — 通过 RoundEnd hook 触发  
static bool Hook_RoundEnd(IHookChain<bool, int, ScenarioEventEndRound, float> *chain,
    int winStatus, ScenarioEventEndRound event, float tmDelay)
{
    ForwardCallParam params[3];
    params[0].type = FP_CELL; params[0].val = winStatus; params[0].size = 0;
    params[1].type = FP_CELL; params[1].val = (cell)event; params[1].size = 0;
    params[2].type = FP_FLOAT; params[2].fval = tmDelay; params[2].size = 0;
    AMXXRuntime::GetInstance().ExecuteForwardEx("round_end", 3, params);
    return chain->callNext(winStatus, event, tmDelay);
}

// round_freeze_end() — 通过 CSGameRules_OnRoundFreezeEnd hook 触发
static void Hook_OnRoundFreezeEnd(IHookChain<void> *chain)
{
    AMXXRuntime::GetInstance().ExecuteForward("round_freeze_end", 0);
    chain->callNext();
}

// FM_PlayerPreThink(id) — 每帧玩家预思考
static void Hook_CBasePlayer_PreThink(IHookChainClass<void, CBasePlayer> *chain, CBasePlayer *pPlayer)
{
    FireFMForward(FM_PlayerPreThink, pPlayer ? pPlayer->entindex() : 0);
    chain->callNext(pPlayer);
}

// FM_PlayerPostThink(id) — 每帧玩家后思考
static void Hook_CBasePlayer_PostThink(IHookChainClass<void, CBasePlayer> *chain, CBasePlayer *pPlayer)
{
    FireFMForward(FM_PlayerPostThink, pPlayer ? pPlayer->entindex() : 0);
    chain->callNext(pPlayer);
}

// ===== CSX Bomb Forwards =====

// PlantBomb hook — C4 被安放 (ShootSatchelCharge 创建 C4 实体后触发 bomb_planted)
static CGrenade *Hook_PlantBomb(IHookChain<CGrenade *, entvars_t *, Vector &, Vector &> *chain,
                                entvars_t *pevOwner, Vector &vecStart, Vector &vecAngles)
{
    CGrenade *pGrenade = chain->callNext(pevOwner, vecStart, vecAngles);

    // bomb_planted(planter)
    int planterIdx = 0;
    if (pevOwner) {
        edict_t *pEnt = ENT(pevOwner);
        if (pEnt) planterIdx = ENTINDEX(pEnt);
    }
    CsstatsRecordBombPlant(planterIdx);
    ForwardCallParam fcp[1];
    fcp[0].type = FP_CELL;
    fcp[0].val = planterIdx;
    fcp[0].size = 0;
    AMXXRuntime::GetInstance().ExecuteForwardEx("bomb_planted", 1, fcp);

    return pGrenade;
}

// DefuseBombStart hook — 玩家开始拆除 C4
static void Hook_CGrenade_DefuseBombStart(IHookChainClass<void, CGrenade, CBasePlayer *> *chain,
                                           CGrenade *pGrenade, CBasePlayer *pPlayer)
{
    chain->callNext(pGrenade, pPlayer);

    // bomb_defusing(defuser)
    ForwardCallParam fcp[1];
    fcp[0].type = FP_CELL;
    fcp[0].val = pPlayer ? pPlayer->entindex() : 0;
    fcp[0].size = 0;
    AMXXRuntime::GetInstance().ExecuteForwardEx("bomb_defusing", 1, fcp);
}

// DefuseBombEnd hook — C4 拆除结束 (成功或失败)
static void Hook_CGrenade_DefuseBombEnd(IHookChainClass<void, CGrenade, CBasePlayer *, bool> *chain,
                                        CGrenade *pGrenade, CBasePlayer *pPlayer, bool bDefused)
{
    chain->callNext(pGrenade, pPlayer, bDefused);

    // csstats: record defusion attempt (and success) for the defuser.
    CsstatsRecordBombDefuse(pPlayer ? pPlayer->entindex() : 0, bDefused);

    // bomb_defused(defuser) — 仅在成功拆除时触发
    if (bDefused) {
        ForwardCallParam fcp[1];
        fcp[0].type = FP_CELL;
        fcp[0].val = pPlayer ? pPlayer->entindex() : 0;
        fcp[0].size = 0;
        AMXXRuntime::GetInstance().ExecuteForwardEx("bomb_defused", 1, fcp);
    }
}

// ExplodeBomb hook — C4 爆炸
static void Hook_CGrenade_ExplodeBomb(IHookChainClass<void, CGrenade, TraceResult *, int> *chain,
                                      CGrenade *pGrenade, TraceResult *pTrace, int bitsDamageType)
{
    // bomb_explode(planter, defuser) — 在爆炸前触发，插件仍可访问 C4 实体
    int planterIdx = 0;
    int defuserIdx = 0;

    if (pGrenade && pGrenade->pev && pGrenade->pev->owner) {
        planterIdx = ENTINDEX(pGrenade->pev->owner);
    }

    // 检查是否有玩家正在拆除 (爆炸时拆除失败)
    if (pGrenade && pGrenade->m_pBombDefuser) {
        defuserIdx = pGrenade->m_pBombDefuser->entindex();
    }

    // csstats: record bomb explosion for the planter.
    CsstatsRecordBombExplode(planterIdx);

    ForwardCallParam fcp[2];
    fcp[0].type = FP_CELL;
    fcp[0].val = planterIdx;
    fcp[0].size = 0;
    fcp[1].type = FP_CELL;
    fcp[1].val = defuserIdx;
    fcp[1].size = 0;
    AMXXRuntime::GetInstance().ExecuteForwardEx("bomb_explode", 2, fcp);

    chain->callNext(pGrenade, pTrace, bitsDamageType);
}

// ===== csstats: weapon shot tracking =====
// Original CSX counts shots by monitoring CurWeapon clip-ammo decreases. In the
// embedded build we instead hook the ReGameDLL FireBullets3 / FireBuckshots
// hookchains, each of which fires exactly once per trigger pull. FireBullets3
// covers all bullet weapons (rifles, pistols, SMGs, snipers, MG); FireBuckshots
// covers shotguns (M3, XM1014). FireBullets is intentionally NOT hooked because
// shotguns call both FireBuckshots and FireBullets, which would double-count.

// CBaseEntity::FireBullets3 — bullet weapons
static Vector &Hook_CBaseEntity_FireBullets3(IHookChainClass<Vector &, CBaseEntity, Vector &, Vector &, float, float, int, int, int, float, entvars_t *, bool, int> *chain,
    CBaseEntity *pEntity, Vector &vecSrc, Vector &vecDirShooting, float flSpread, float flDistance, int iPenetration, int iBulletType, int iDamage, float flRangeModifier, entvars_t *pevAttacker, bool bPistol, int shared_rand)
{
    Vector &ret = chain->callNext(pEntity, vecSrc, vecDirShooting, flSpread, flDistance, iPenetration, iBulletType, iDamage, flRangeModifier, pevAttacker, bPistol, shared_rand);

    if (pEntity && pEntity->pev && (pEntity->pev->flags & FL_CLIENT)) {
        CBasePlayer *pPlayer = static_cast<CBasePlayer *>(pEntity);
        int attackerId = pPlayer->entindex();
        int wpnId = 0;
        if (pPlayer->m_pActiveItem)
            wpnId = pPlayer->m_pActiveItem->m_iId;
        CsstatsRecordShot(attackerId, wpnId);
    }
    return ret;
}

// CBaseEntity::FireBuckshots — shotguns (M3, XM1014)
static void Hook_CBaseEntity_FireBuckshots(IHookChainClass<void, CBaseEntity, ULONG, Vector &, Vector &, Vector &, float, int, int, entvars_t *> *chain,
    CBaseEntity *pEntity, ULONG cShots, Vector &vecSrc, Vector &vecDirShooting, Vector &vecSpread, float flDistance, int iBulletType, int iDamage, entvars_t *pevAttacker)
{
    chain->callNext(pEntity, cShots, vecSrc, vecDirShooting, vecSpread, flDistance, iBulletType, iDamage, pevAttacker);

    if (pEntity && pEntity->pev && (pEntity->pev->flags & FL_CLIENT)) {
        CBasePlayer *pPlayer = static_cast<CBasePlayer *>(pEntity);
        int attackerId = pPlayer->entindex();
        int wpnId = 0;
        if (pPlayer->m_pActiveItem)
            wpnId = pPlayer->m_pActiveItem->m_iId;
        CsstatsRecordShot(attackerId, wpnId);
    }
}

void AMXXHooks::Init()
{
    AMXX_LOG_DBG("[Hooks] Registering AMXX game event hooks...");

    g_ReGameHookchains.m_CBasePlayer_Spawn.registerHook(Hook_CBasePlayer_Spawn, HC_PRIORITY_DEFAULT);
    g_ReGameHookchains.m_CBasePlayer_TakeDamage.registerHook(Hook_CBasePlayer_TakeDamage, HC_PRIORITY_DEFAULT);
    g_ReGameHookchains.m_CBasePlayer_Killed.registerHook(Hook_CBasePlayer_Killed, HC_PRIORITY_DEFAULT);
    g_ReGameHookchains.m_CBasePlayer_PreThink.registerHook(Hook_CBasePlayer_PreThink, HC_PRIORITY_DEFAULT);
    g_ReGameHookchains.m_CBasePlayer_PostThink.registerHook(Hook_CBasePlayer_PostThink, HC_PRIORITY_DEFAULT);

    g_ReGameHookchains.m_CSGameRules_ClientUserInfoChanged.registerHook(Hook_ClientUserInfoChanged, HC_PRIORITY_DEFAULT);
    g_ReGameHookchains.m_InternalCommand.registerHook(Hook_InternalCommand, HC_PRIORITY_DEFAULT);

    g_ReGameHookchains.m_CSGameRules_RestartRound.registerHook(Hook_RestartRound, HC_PRIORITY_DEFAULT);
    g_ReGameHookchains.m_RoundEnd.registerHook(Hook_RoundEnd, HC_PRIORITY_DEFAULT);
    g_ReGameHookchains.m_CSGameRules_OnRoundFreezeEnd.registerHook(Hook_OnRoundFreezeEnd, HC_PRIORITY_DEFAULT);

    // CSX bomb event hooks
    g_ReGameHookchains.m_PlantBomb.registerHook(Hook_PlantBomb, HC_PRIORITY_DEFAULT);
    g_ReGameHookchains.m_CGrenade_DefuseBombStart.registerHook(Hook_CGrenade_DefuseBombStart, HC_PRIORITY_DEFAULT);
    g_ReGameHookchains.m_CGrenade_DefuseBombEnd.registerHook(Hook_CGrenade_DefuseBombEnd, HC_PRIORITY_DEFAULT);
    g_ReGameHookchains.m_CGrenade_ExplodeBomb.registerHook(Hook_CGrenade_ExplodeBomb, HC_PRIORITY_DEFAULT);

    // csstats shot tracking: FireBullets3 (bullet weapons) + FireBuckshots (shotguns)
    g_ReGameHookchains.m_CBaseEntity_FireBullets3.registerHook(Hook_CBaseEntity_FireBullets3, HC_PRIORITY_DEFAULT);
    g_ReGameHookchains.m_CBaseEntity_FireBuckshots.registerHook(Hook_CBaseEntity_FireBuckshots, HC_PRIORITY_DEFAULT);

    AMXX_LOG_DBG("[Hooks] AMXX game event hooks registered");
}

void AMXXHooks::Shutdown()
{
    AMXX_LOG_DBG("[Hooks] Unregistering AMXX game event hooks...");

    g_ReGameHookchains.m_CBasePlayer_Spawn.unregisterHook(Hook_CBasePlayer_Spawn);
    g_ReGameHookchains.m_CBasePlayer_TakeDamage.unregisterHook(Hook_CBasePlayer_TakeDamage);
    g_ReGameHookchains.m_CBasePlayer_Killed.unregisterHook(Hook_CBasePlayer_Killed);
    g_ReGameHookchains.m_CBasePlayer_PreThink.unregisterHook(Hook_CBasePlayer_PreThink);
    g_ReGameHookchains.m_CBasePlayer_PostThink.unregisterHook(Hook_CBasePlayer_PostThink);

    g_ReGameHookchains.m_CSGameRules_ClientUserInfoChanged.unregisterHook(Hook_ClientUserInfoChanged);
    g_ReGameHookchains.m_InternalCommand.unregisterHook(Hook_InternalCommand);

    g_ReGameHookchains.m_CSGameRules_RestartRound.unregisterHook(Hook_RestartRound);
    g_ReGameHookchains.m_RoundEnd.unregisterHook(Hook_RoundEnd);
    g_ReGameHookchains.m_CSGameRules_OnRoundFreezeEnd.unregisterHook(Hook_OnRoundFreezeEnd);

    // CSX bomb event hooks
    g_ReGameHookchains.m_PlantBomb.unregisterHook(Hook_PlantBomb);
    g_ReGameHookchains.m_CGrenade_DefuseBombStart.unregisterHook(Hook_CGrenade_DefuseBombStart);
    g_ReGameHookchains.m_CGrenade_DefuseBombEnd.unregisterHook(Hook_CGrenade_DefuseBombEnd);
    g_ReGameHookchains.m_CGrenade_ExplodeBomb.unregisterHook(Hook_CGrenade_ExplodeBomb);

    // csstats shot tracking
    g_ReGameHookchains.m_CBaseEntity_FireBullets3.unregisterHook(Hook_CBaseEntity_FireBullets3);
    g_ReGameHookchains.m_CBaseEntity_FireBuckshots.unregisterHook(Hook_CBaseEntity_FireBuckshots);

    AMXX_LOG_DBG("[Hooks] AMXX game event hooks unregistered");
}
