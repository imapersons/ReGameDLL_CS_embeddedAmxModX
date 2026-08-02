#include "precompiled.h"

#include "native_fun.h"
#include "amx.h"

/* ReGameDLL includes */
#include "../extdll.h"
#include "../player.h"
#include "../cbase.h"
#include "../enginecallback.h"
#include "../weapontype.h"

#include <vector>
#include <cstring>

// ============================================================
// Hitzone 支持 (原版的 Players.GetBodyHits / SetBodyHits 简化实现)
// ============================================================
// HITZONES_DEFAULT = 0 表示重置为默认 (所有部位都能打)
// 原版位掩码: 1<<0 = HIT_HEAD, 1<<1 = HIT_CHEST, 1<<2 = HIT_STOMACH,
//             1<<3 = HIT_LEFTARM, 1<<4 = HIT_RIGHTARM, 1<<5 = HIT_LEFTLEG,
//             1<<6 = HIT_RIGHTLEG, 1<<7 = HIT_SHIELD (盾牌, CS)
#define AMXX_MAX_PLAYERS  33  // 0 是全体目标, 1~32 是玩家

struct HitzoneMap {
    int global;     // attacker=0 & target=0 全局默认
    int perTarget[AMXX_MAX_PLAYERS];  // attacker=0, target=T: 所有人打 T 时
    int perAttacker[AMXX_MAX_PLAYERS]; // attacker=A, target=0: A 打所有人时
    int pair[AMXX_MAX_PLAYERS][AMXX_MAX_PLAYERS]; // attacker=A, target=T: A vs T
};

static HitzoneMap s_hitzones;
static bool s_hitzoneInit = false;
static const int HITZONES_DEFAULT_MASK = 0xFF; // 所有部位都允许

static void InitHitzones()
{
    if (s_hitzoneInit) return;
    s_hitzones.global = HITZONES_DEFAULT_MASK;
    for (int i = 0; i < AMXX_MAX_PLAYERS; i++) {
        s_hitzones.perTarget[i] = HITZONES_DEFAULT_MASK;
        s_hitzones.perAttacker[i] = HITZONES_DEFAULT_MASK;
        for (int j = 0; j < AMXX_MAX_PLAYERS; j++)
            s_hitzones.pair[i][j] = HITZONES_DEFAULT_MASK;
    }
    s_hitzoneInit = true;
}

void ResetFunGlobals()
{
    s_hitzoneInit = false;
    InitHitzones();
}

// 脚步声静默追踪
static bool s_silentFootsteps[AMXX_MAX_PLAYERS] = {false};

// P2: 静默模式下每帧维持 flTimeStepSound=999
// 原版安装 pfnPlayerPreThink 钩子每帧重置; 此处简化为由 AMXXRuntime::Frame() 轮询,
// 对记录为静默且仍有效的玩家重新设置 (footsteps=0 时恢复默认 400)。
void UpdateSilentFootsteps()
{
    for (int i = 1; i <= gpGlobals->maxClients; i++) {
        if (!s_silentFootsteps[i])
            continue;
        edict_t *pEdict = INDEXENT(i);
        if (!pEdict || pEdict->free || !pEdict->pvPrivateData)
            continue;
        pEdict->v.flTimeStepSound = 999.0f;
    }
}

// ============================================================
// 已有函数 (原实现保持不变)
// ============================================================

cell AMX_NATIVE_CALL fun_set_user_gravity(AMX *amx, cell *params)
{
	int index = params[1];
	float gravity = amx_ctof(params[2]);

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;

	pPlayer->pev->gravity = gravity;
	return 1;
}

cell AMX_NATIVE_CALL fun_get_user_gravity(AMX *amx, cell *params)
{
	int index = params[1];

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;

	return amx_ftoc(pPlayer->pev->gravity);
}

cell AMX_NATIVE_CALL fun_set_user_maxspeed(AMX *amx, cell *params)
{
	int index = params[1];
	float maxspeed = amx_ctof(params[2]);

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;

    // 原版 SETCLIENTMAXSPEED 对应 MDLL_ClientCommand 里的回调, 此处直接改 pev
	pPlayer->pev->maxspeed = maxspeed;
    if (g_engfuncs.pfnSetClientMaxspeed) {
        g_engfuncs.pfnSetClientMaxspeed(pEdict, maxspeed);
    }
	return 1;
}

cell AMX_NATIVE_CALL fun_get_user_maxspeed(AMX *amx, cell *params)
{
	int index = params[1];

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;

	return amx_ftoc(pPlayer->pev->maxspeed);
}

cell AMX_NATIVE_CALL fun_user_has_weapon(AMX *amx, cell *params)
{
	int index = params[1];
	int weaponId = params[2];

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;

	WeaponSlotInfo *info = GetWeaponSlot((WeaponIdType)weaponId);
	if (!info || !info->weaponName)
		return 0;

	return pPlayer->HasNamedPlayerItem(info->weaponName) ? 1 : 0;
}

cell AMX_NATIVE_CALL fun_get_weapon_name(AMX *amx, cell *params)
{
	int weaponId = params[1];
	int maxlen = params[3];

	cell *dest;
	amx_GetAddr(amx, params[2], &dest);

	const char *name = WeaponIDToAlias(weaponId);
	if (!name)
		name = "";

	return amx_SetString(dest, name, 0, 0, maxlen);
}

cell AMX_NATIVE_CALL fun_set_rendering(AMX *amx, cell *params)
{
	int index = params[1];
	int fx = params[2];
	int r = params[3];
	int g = params[4];
	int b = params[5];
	int render = params[6];
	int amount = params[7];

	if (index < 0 || index > gpGlobals->maxEntities)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict)
		return 0;

	pEdict->v.renderfx = fx;
	pEdict->v.rendercolor.x = (float)r;
	pEdict->v.rendercolor.y = (float)g;
	pEdict->v.rendercolor.z = (float)b;
	pEdict->v.renderamt = (float)amount;
	pEdict->v.rendermode = render;

	return 1;
}

cell AMX_NATIVE_CALL fun_set_user_rendering(AMX *amx, cell *params)
{
	int index = params[1];
	int fx = params[2];
	int r = params[3];
	int g = params[4];
	int b = params[5];
	int render = params[6];
	int amount = params[7];

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	pEdict->v.renderfx = fx;
	pEdict->v.rendercolor.x = (float)r;
	pEdict->v.rendercolor.y = (float)g;
	pEdict->v.rendercolor.z = (float)b;
	pEdict->v.renderamt = (float)amount;
	pEdict->v.rendermode = render;

	return 1;
}

cell AMX_NATIVE_CALL fun_set_user_godmode(AMX *amx, cell *params)
{
	int index = params[1];
	int godmode = params[2];

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	pEdict->v.takedamage = godmode ? DAMAGE_NO : DAMAGE_AIM;
	return 1;
}

cell AMX_NATIVE_CALL fun_get_user_godmode(AMX *amx, cell *params)
{
	int index = params[1];

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	return (pEdict->v.takedamage == DAMAGE_NO) ? 1 : 0;
}

cell AMX_NATIVE_CALL fun_set_user_noclip(AMX *amx, cell *params)
{
	int index = params[1];
	int noclip = params[2];

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	if (noclip) {
		pEdict->v.movetype = MOVETYPE_NOCLIP;
	} else {
		pEdict->v.movetype = MOVETYPE_WALK;
	}

	return 1;
}

cell AMX_NATIVE_CALL fun_get_user_noclip(AMX *amx, cell *params)
{
	int index = params[1];

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	return (pEdict->v.movetype == MOVETYPE_NOCLIP) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_user_kill(AMX *amx, cell *params)
{
	int index = params[1];
	(void)amx;

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;

	// 原版要求 ingame && IsAlive() (CPlayer::ingame 等价于已连接且可用的网络客户端)
	if (pEdict->free || !pPlayer->IsNetClient())
		return 0;

	if (!pPlayer->IsAlive())
		return 0;

	float bef = pEdict->v.frags;
	ClientKill(pEdict);

	if (params[2])
		pEdict->v.frags = bef;

	return 1;
}

cell AMX_NATIVE_CALL amxx_user_slap(AMX *amx, cell *params)
{
	int index = params[1];
	int power = (int)params[2];
	(void)amx;

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	if (power < 0)
		power = 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;

	if (!pPlayer->IsAlive())
		return 0;

	if (pEdict->v.health <= power)
	{
		// Health too low — kill the player, preserve frags
		float bef = pEdict->v.frags;
		ClientKill(pEdict);
		pEdict->v.frags = bef;
	}
	else
	{
		int numparam = params[0] / (int)sizeof(cell);

		if (numparam < 3 || params[3])
		{
			// Random velocity push
			pEdict->v.velocity.x += RANDOM_LONG(-600, 600);
			pEdict->v.velocity.y += RANDOM_LONG(-180, 180);
			pEdict->v.velocity.z += RANDOM_LONG(100, 200);
		}
		else
		{
			// Directional push based on player facing
			Vector v_forward, v_right;
			AngleVectors(pEdict->v.angles, v_forward, v_right, nullptr);
			pEdict->v.velocity = pEdict->v.velocity + v_forward * 220 + Vector(0, 0, 200);
		}

		pEdict->v.punchangle.x = (vec_t)RANDOM_LONG(-10, 10);
		pEdict->v.punchangle.y = (vec_t)RANDOM_LONG(-10, 10);
		pEdict->v.health -= power;

		int armor = (int)pEdict->v.armorvalue;
		armor -= power;
		if (armor < 0)
			armor = 0;
		pEdict->v.armorvalue = (float)armor;
		pEdict->v.dmg_inflictor = pEdict;

		// Emit slap sound (CS variant)
		static const char *cs_sound[4] =
		{
			"player/bhit_flesh-3.wav",
			"player/bhit_flesh-2.wav",
			"player/pl_die1.wav",
			"player/pl_pain6.wav"
		};
		EMIT_SOUND_DYN2(pEdict, CHAN_VOICE, cs_sound[RANDOM_LONG(0, 3)], 1.0, ATTN_NORM, 0, PITCH_NORM);
	}

	return 1;
}

cell AMX_NATIVE_CALL amxx_set_user_footsteps(AMX *amx, cell *params)
{
	int index = params[1];
	int footsteps = params[2];

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

    // set=1 → 静默, set=0 → 默认
    // P2: 静默保持由 UpdateSilentFootsteps() 每帧维持 flTimeStepSound=999 (Frame 轮询)
    if (footsteps == 0) {
        // footsteps=0: 开启 (默认)
        pEdict->v.flTimeStepSound = 400.0f;
        s_silentFootsteps[index] = false;
    } else {
        // footsteps=1: 静默
        pEdict->v.flTimeStepSound = 999.0f;
        s_silentFootsteps[index] = true;
    }
	return 1;
}

cell AMX_NATIVE_CALL amxx_set_user_blood(AMX *amx, cell *params)
{
	int index = params[1];
	int blood = params[2];
	(void)amx;
	if (index < 1 || index > gpGlobals->maxClients)
		return 0;
	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;
	if (blood) {
		pEdict->v.effects &= ~EF_NODRAW;
	} else {
		pEdict->v.effects |= EF_NODRAW;
	}
	return 1;
}

// ============================================================
// P1-1: 新增函数
// ============================================================

// get_client_listen(receiver, sender)
cell AMX_NATIVE_CALL fun_get_client_listen(AMX *amx, cell *params)
{
    int receiver = params[1];
    int sender = params[2];
    if (receiver < 1 || receiver > gpGlobals->maxClients) return 0;
    if (sender < 1 || sender > gpGlobals->maxClients) return 0;
    if (!g_engfuncs.pfnVoice_GetClientListening) return 0;
    return g_engfuncs.pfnVoice_GetClientListening(receiver, sender) ? 1 : 0;
}

// set_client_listen(receiver, sender, listen)
cell AMX_NATIVE_CALL fun_set_client_listen(AMX *amx, cell *params)
{
    int receiver = params[1];
    int sender = params[2];
    int listen = params[3];
    if (receiver < 1 || receiver > gpGlobals->maxClients) return 0;
    if (sender < 1 || sender > gpGlobals->maxClients) return 0;
    if (!g_engfuncs.pfnVoice_SetClientListening) return 0;
    return g_engfuncs.pfnVoice_SetClientListening(receiver, sender, listen ? TRUE : FALSE) ? 1 : 0;
}

// set_user_armor(index, armor)
cell AMX_NATIVE_CALL fun_set_user_armor(AMX *amx, cell *params)
{
    int index = params[1];
    if (index < 1 || index > gpGlobals->maxClients) return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData) return 0;
    pEdict->v.armorvalue = (float)params[2];
    return 1;
}

// set_user_health(index, health)
cell AMX_NATIVE_CALL fun_set_user_health(AMX *amx, cell *params)
{
    int index = params[1];
    int health = params[2];
    if (index < 1 || index > gpGlobals->maxClients) return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData) return 0;
    if (health > 0) {
        pEdict->v.health = (float)health;
    } else {
        // health ≤ 0: 杀死玩家 (MDLL_ClientKill 在 ReGameDLL 内嵌版未定义，用 client.h 声明的 ClientKill 全局函数)
        ClientKill(pEdict);
    }
    return 1;
}

// set_user_origin(index, const origin[3])
cell AMX_NATIVE_CALL fun_set_user_origin(AMX *amx, cell *params)
{
    int index = params[1];
    if (index < 1 || index > gpGlobals->maxClients) return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData) return 0;

    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    if (!addr) return 0;

    Vector v((float)addr[0], (float)addr[1], (float)addr[2]);

    // 重置实体包围盒 (同原版)
    SET_SIZE(pEdict, pEdict->v.mins, pEdict->v.maxs);
    SET_ORIGIN(pEdict, v);
    return 1;
}

// get_user_rendering(index, &fx, &r, &g, &b, &render, &amount)
cell AMX_NATIVE_CALL fun_get_user_rendering(AMX *amx, cell *params)
{
    int index = params[1];
    if (index < 1 || index > gpGlobals->maxClients) return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData) return 0;

    int argCount = params[0] / (int)sizeof(cell);
    cell *addr;

    if (argCount >= 2) { amx_GetAddr(amx, params[2], &addr); if (addr) *addr = pEdict->v.renderfx; }
    if (argCount >= 3) { amx_GetAddr(amx, params[3], &addr); if (addr) *addr = (int)pEdict->v.rendercolor.x; }
    if (argCount >= 4) { amx_GetAddr(amx, params[4], &addr); if (addr) *addr = (int)pEdict->v.rendercolor.y; }
    if (argCount >= 5) { amx_GetAddr(amx, params[5], &addr); if (addr) *addr = (int)pEdict->v.rendercolor.z; }
    if (argCount >= 6) { amx_GetAddr(amx, params[6], &addr); if (addr) *addr = pEdict->v.rendermode; }
    if (argCount >= 7) { amx_GetAddr(amx, params[7], &addr); if (addr) *addr = (int)pEdict->v.renderamt; }
    return 1;
}

// set_user_hitzones(index = 0, target = 0, body = HITZONES_DEFAULT)
// 原版语义: body 直接按位存储 (1<<0=头, 1<<1=胸, 1<<2=腹, 1<<3=左臂, 1<<4=右臂,
// 1<<5=左腿, 1<<6=右腿, 1<<7=盾牌), body = 0 表示所有部位禁打。
// (HITZONES_DEFAULT 常量由插件侧自行换算, native 不做 0 -> 0xFF 转换)
cell AMX_NATIVE_CALL fun_set_user_hitzones(AMX *amx, cell *params)
{
    (void)amx;
    InitHitzones();
    int attacker = params[1];
    int target = params[2];
    int body = params[3];

    if (attacker < 0 || attacker > gpGlobals->maxClients) return 0;
    if (target < 0 || target > gpGlobals->maxClients) return 0;

    if (attacker == 0 && target == 0) {
        s_hitzones.global = body;
        for (int i = 0; i < AMXX_MAX_PLAYERS; i++) {
            s_hitzones.perTarget[i] = body;
            s_hitzones.perAttacker[i] = body;
            for (int j = 0; j < AMXX_MAX_PLAYERS; j++)
                s_hitzones.pair[i][j] = body;
        }
    } else if (attacker == 0 && target != 0) {
        s_hitzones.perTarget[target] = body;
        for (int a = 1; a <= gpGlobals->maxClients; a++)
            s_hitzones.pair[a][target] = body;
    } else if (attacker != 0 && target == 0) {
        s_hitzones.perAttacker[attacker] = body;
        for (int t = 1; t <= gpGlobals->maxClients; t++)
            s_hitzones.pair[attacker][t] = body;
    } else {
        s_hitzones.pair[attacker][target] = body;
    }
    return 1;
}

// get_user_hitzones(index, target)
cell AMX_NATIVE_CALL fun_get_user_hitzones(AMX *amx, cell *params)
{
    (void)amx;
    InitHitzones();
    int attacker = params[1];
    int target = params[2];
    if (attacker < 1 || attacker > gpGlobals->maxClients) return 0;
    if (target < 1 || target > gpGlobals->maxClients) return 0;
    return s_hitzones.pair[attacker][target];
}

// spawn(entity) — 直接调用 DispatchSpawn
cell AMX_NATIVE_CALL fun_spawn_entity(AMX *amx, cell *params)
{
    (void)amx;
    int index = params[1];
    if (index < 0 || index > gpGlobals->maxEntities) return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict) return 0;
    // MDLL_Spawn 在 ReGameDLL 内嵌版未定义，等价于 cbase.h 声明的 DispatchSpawn
    DispatchSpawn(pEdict);
    return 1;
}

// get_user_footsteps(index) — 返回 0=正常, 1=静默 (与原版一致)
cell AMX_NATIVE_CALL fun_get_user_footsteps(AMX *amx, cell *params)
{
    (void)amx;
    int index = params[1];
    if (index < 1 || index > gpGlobals->maxClients) return 0;
    return s_silentFootsteps[index] ? 1 : 0;
}

// set_user_frags(index, frags)
cell AMX_NATIVE_CALL fun_set_user_frags(AMX *amx, cell *params)
{
    (void)amx;
    int index = params[1];
    if (index < 1 || index > gpGlobals->maxClients) return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict) return 0;
    CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
    pEdict->v.frags = (float)params[2];
    // 同步消息给客户端
    MESSAGE_BEGIN(MSG_ALL, gmsgScoreInfo);
    WRITE_BYTE(index);
    WRITE_SHORT(params[2]);
    WRITE_SHORT(pPlayer ? pPlayer->m_iDeaths : 0);
    WRITE_SHORT(pEdict->v.team ? (int)pEdict->v.team : 0);
    MESSAGE_END();
    return 1;
}

// strip_user_weapons(index)
cell AMX_NATIVE_CALL fun_strip_user_weapons(AMX *amx, cell *params)
{
    int index = params[1];
    (void)amx;
    if (index < 1 || index > gpGlobals->maxClients) return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData) return 0;
    CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
    if (!pPlayer) return 0;
    pPlayer->RemoveAllItems(FALSE);
    return 1;
}

// give_item(index, item[]) — 返回 -1 表示失败, entity index 表示成功
cell AMX_NATIVE_CALL fun_give_item(AMX *amx, cell *params)
{
    int index = params[1];
    if (index < 1 || index > gpGlobals->maxClients) return -1;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData) return -1;
    CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
    if (!pPlayer) return -1;

    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    if (!addr) return -1;
    char itemName[128];
    amx_GetString(itemName, addr, 0, sizeof(itemName));
    if (!itemName[0]) return -1;

    // 只允许 weapon_*, item_*, ammobox_*, 其他一律拒绝
    if (strncmp(itemName, "weapon_", 7) != 0
        && strncmp(itemName, "item_", 5) != 0
        && strncmp(itemName, "ammo_", 5) != 0
        && strncmp(itemName, "ammobox_", 8) != 0)
    {
        return -1;
    }

    // 调用原版 GiveNamedItem + 手动 touch
    CBaseEntity *pEntity = pPlayer->GiveNamedItem(itemName);
    if (!pEntity) return -1;

    edict_t *pEnt = pEntity->edict();
    if (!pEnt) return -1;
    int oldSolid = pEnt->v.solid;

    // MDLL_Spawn/MDLL_Touch 在 ReGameDLL 内嵌版未定义，替换为 cbase.h 声明的 DispatchSpawn/DispatchTouch
    DispatchSpawn(pEnt);
    DispatchTouch(pEnt, pEdict);

    if (pEnt->v.solid == oldSolid) {
        // 没被接收, 移除
        REMOVE_ENTITY(pEnt);
        return -1;
    }
    return ENTINDEX(pEnt);
}

// ============================================================
// Native 注册
// ============================================================

AMX_NATIVE_INFO fun_natives[] = {
    // 语音收听控制 (原 Fun 模块专用, core 中不存在)
    {"get_client_listen", fun_get_client_listen},
    {"set_client_listen", fun_set_client_listen},

    // 伤害区域 (原 Fun 模块专用, core 中不存在)
    {"set_user_hitzones", fun_set_user_hitzones},
    {"get_user_hitzones", fun_get_user_hitzones},

    // 实体 spawn (原 Fun 模块专用)
    {"spawn", fun_spawn_entity},

    // 脚步声 (core 中只有 set, 缺 get)
    {"get_user_footsteps", fun_get_user_footsteps},
    {"set_user_footsteps", amxx_set_user_footsteps},

    // P1-3: 补全原 Fun 模块 native (以下未在 native_core.cpp 注册)
    {"set_user_godmode", fun_set_user_godmode},
    {"get_user_godmode", fun_get_user_godmode},
    {"set_user_noclip", fun_set_user_noclip},
    {"get_user_noclip", fun_get_user_noclip},
    {"set_user_gravity", fun_set_user_gravity},
    {"get_user_gravity", fun_get_user_gravity},
    {"set_user_maxspeed", fun_set_user_maxspeed},
    {"set_user_rendering", fun_set_user_rendering},
    {"set_rendering", fun_set_rendering},
    {"user_kill", amxx_user_kill},
    {"user_slap", amxx_user_slap},
    {"set_user_blood", amxx_set_user_blood},
    {"get_weapon_name", fun_get_weapon_name},

    // ============== 以下 native 已在 native_core.cpp 注册 ==============
    // set_user_armor / set_user_health / set_user_frags / set_user_origin
    // get_user_rendering / get_user_maxspeed
    // give_item / strip_user_weapons / user_has_weapon
    // =================================================================

    // P1: 补全 Fun 模块专用 native (core 不注册, 与原版对齐)
    {"set_user_armor",      fun_set_user_armor},
    {"set_user_health",     fun_set_user_health},
    {"set_user_origin",     fun_set_user_origin},
    {"get_user_rendering",  fun_get_user_rendering},
    {"set_user_frags",      fun_set_user_frags},
    {"strip_user_weapons",  fun_strip_user_weapons},
    {"give_item",           fun_give_item},

	{nullptr, nullptr}
};

void RegisterFunNatives(AMX *amx)
{
    InitHitzones();
    amx_Register(amx, fun_natives, -1);
}
