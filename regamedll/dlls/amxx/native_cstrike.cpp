#include "precompiled.h"

#include "native_cstrike.h"
#include "amx.h"
#include "native_core.h"

/* ReGameDLL includes */
#include "../extdll.h"
#include "../player.h"
#include "../cbase.h"
#include "../enginecallback.h"
#include "../weapons.h"

#include <map>

// CS Team constants (matching TeamName enum in ReGameDLL)
#define CS_TEAM_UNASSIGNED 0
#define CS_TEAM_T          1   // TERRORIST
#define CS_TEAM_CT         2   // CT
#define CS_TEAM_SPECTATOR  3

// CS Armor type constants (matching ArmorType enum in ReGameDLL)
#define CS_ARMOR_NONE      0
#define CS_ARMOR_KEVLAR    1
#define CS_ARMOR_VESTHELM  2

// CS_DONTCHANGE sentinel for cs_set_user_team model parameter
#define CS_DONTCHANGE      0

// CSW_* weapon ID constants (standard AMXX cstrike values; align with WeaponIdType enum)
#define CSW_NONE           0
#define CSW_P228           1
#define CSW_SCOUT          3
#define CSW_HEGRENADE      4
#define CSW_XM1014         5
#define CSW_C4             6
#define CSW_MAC10          7
#define CSW_AUG            8
#define CSW_SMOKEGRENADE   9
#define CSW_ELITE          10
#define CSW_FIVESEVEN      11
#define CSW_UMP45          12
#define CSW_SG550          13
#define CSW_GALIL          14
#define CSW_FAMAS          15
#define CSW_USP            16
#define CSW_GLOCK18        17
#define CSW_AWP            18
#define CSW_MP5NAVY        19
#define CSW_M249           20
#define CSW_M3             21
#define CSW_M4A1           22
#define CSW_TMP            23
#define CSW_G3SG1          24
#define CSW_FLASHBANG      25
#define CSW_DEAGLE         26
#define CSW_SG552          27
#define CSW_AK47           28
#define CSW_KNIFE          29
#define CSW_P90            30

cell AMX_NATIVE_CALL cs_get_user_team(AMX *amx, cell *params)
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

	// Original signature: cs_get_user_team(index, &any:model = CS_DONTCHANGE)
	// 2nd parameter outputs m_iModelName by reference
	int argCount = params[0] / (int)sizeof(cell);
	if (argCount >= 2)
	{
		cell *modelByRef = nullptr;
		amx_GetAddr(amx, params[2], &modelByRef);
		if (modelByRef)
			*modelByRef = static_cast<cell>(pPlayer->m_iModelName);
	}

	return static_cast<cell>(pPlayer->m_iTeam);
}

cell AMX_NATIVE_CALL cs_set_user_team(AMX *amx, cell *params)
{
    int index = params[1];
    int team = params[2];
    int model = params[3];

    if (index < 1 || index > gpGlobals->maxClients)
        return 0;

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;

    CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
    if (!pPlayer)
        return 0;

    pPlayer->m_iTeam = static_cast<TeamName>(team);

    const char *teamName;
    switch (team) {
        case CS_TEAM_UNASSIGNED: teamName = "UNASSIGNED"; break;
        case CS_TEAM_T: teamName = "TERRORIST"; break;
        case CS_TEAM_CT: teamName = "CT"; break;
        case CS_TEAM_SPECTATOR: teamName = "SPECTATOR"; break;
        default:
        {
            // 原版: 未知队伍映射为 "TEAM_%i" (TeamName[i])
            static char teamBuf[16];
            snprintf(teamBuf, sizeof(teamBuf), "TEAM_%i", team);
            teamName = teamBuf;
            break;
        }
    }
    pPlayer->pev->team = ALLOC_STRING(teamName);

    // 原版语义: model > 0 时写入 m_iModelName; model >= 0 时重置模型
    // (CS_DONTCHANGE == 0 时也会把模型重置为默认模型)
    if (model > 0)
    {
        pPlayer->m_iModelName = static_cast<ModelName>(model);
    }

    if (model >= 0)
    {
        pPlayer->SetPlayerModel(pPlayer->m_bHasC4);
    }

    // Send TeamInfo message unless explicitly suppressed
    bool sendTeamInfo = true;
    int argCount = params[0] / (int)sizeof(cell);
    if (argCount >= 4)
        sendTeamInfo = (params[4] != 0);

    if (sendTeamInfo)
    {
        MESSAGE_BEGIN(MSG_ALL, gmsgTeamInfo);
            WRITE_BYTE(index);
            WRITE_STRING(teamName);
        MESSAGE_END();
    }

    return 1;
}

cell AMX_NATIVE_CALL cs_get_user_money(AMX *amx, cell *params)
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

	return static_cast<cell>(pPlayer->m_iAccount);
}

cell AMX_NATIVE_CALL cs_set_user_money(AMX *amx, cell *params)
{
	int index = params[1];
	int money = params[2];
	int flash = (params[0] / (int)sizeof(cell) >= 3) ? (params[3] != 0 ? 1 : 0) : 1;  // 原版默认 flash = 1

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;

	pPlayer->m_iAccount = money;

	MESSAGE_BEGIN(MSG_ONE, gmsgMoney, nullptr, pEdict);
		WRITE_LONG(money);
		WRITE_BYTE(flash);
	MESSAGE_END();

	return 1;
}

cell AMX_NATIVE_CALL cs_get_user_deaths(AMX *amx, cell *params)
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

	return static_cast<cell>(pPlayer->m_iDeaths);
}

cell AMX_NATIVE_CALL cs_set_user_deaths(AMX *amx, cell *params)
{
	int index = params[1];
	int deaths = params[2];

	// 第 3 参 scoreboard: 是否向全体客户端发送 ScoreInfo 更新计分板 (原版默认 true)
	int argCount = params[0] / (int)sizeof(cell);
	bool scoreboard = (argCount >= 3) ? (params[3] != 0) : true;

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;

	pPlayer->m_iDeaths = deaths;

	if (scoreboard)
	{
		// 原版 ScoreInfo: BYTE(玩家) SHORT(杀敌) SHORT(死亡) SHORT(0) SHORT(队伍)
		// 第 3 字段固定 0, 队伍字段写 m_iTeam (不可写 pev->team 字符串池句柄)
		MESSAGE_BEGIN(MSG_ALL, gmsgScoreInfo);
			WRITE_BYTE(index);
			WRITE_SHORT((int)pEdict->v.frags);
			WRITE_SHORT(deaths);
			WRITE_SHORT(0);
			WRITE_SHORT((int)pPlayer->m_iTeam);
		MESSAGE_END();
	}

	return 1;
}

cell AMX_NATIVE_CALL cs_get_user_armor(AMX *amx, cell *params)
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

	// Optional by-ref armortype output (native cs_get_user_armor(index, &CsArmorType:armortype))
	int argCount = params[0] / (int)sizeof(cell);
	if (argCount >= 2)
	{
		cell *armorByRef;
		amx_GetAddr(amx, params[2], &armorByRef);
		if (armorByRef)
			*armorByRef = static_cast<cell>(pPlayer->m_iKevlar);
	}

	return static_cast<cell>(pPlayer->pev->armorvalue);
}

cell AMX_NATIVE_CALL cs_set_user_armor(AMX *amx, cell *params)
{
	int index = params[1];
	int armor = params[2];
	int type  = params[3];

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;

	pPlayer->pev->armorvalue = (float)armor;
	pPlayer->m_iKevlar = static_cast<ArmorType>(type);

	if (type == CS_ARMOR_KEVLAR || type == CS_ARMOR_VESTHELM)
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgArmorType, nullptr, pEdict);
			WRITE_BYTE(type == CS_ARMOR_VESTHELM ? 1 : 0);
		MESSAGE_END();
	}

	return 1;
}

cell AMX_NATIVE_CALL cs_get_user_defuse(AMX *amx, cell *params)
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

	return pPlayer->m_bHasDefuser ? 1 : 0;
}

cell AMX_NATIVE_CALL cs_set_user_defuse(AMX *amx, cell *params)
{
	int index = params[1];
	int hasDefuser = params[2];
	int argCount = params[0] / (int)sizeof(cell);

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;

	// 原版语义: 写 m_bHasDefuser (拆弹钳), 并同步 body 子模型
	bool kit = (hasDefuser != 0);
	pPlayer->m_bHasDefuser = kit;
	pEdict->v.body = kit ? 1 : 0;

	if (kit)
	{
		// cs_set_user_defuse(index, defusekit, r = 0, g = 160, b = 0, icon[] = "defuser", flash = 0)
		int r = 0, g = 160, b = 0;
		const char *icon = "defuser";
		int flash = 0;

		if (argCount >= 3 && params[3] != -1) r = params[3];
		if (argCount >= 4 && params[4] != -1) g = params[4];
		if (argCount >= 5 && params[5] != -1) b = params[5];
		if (argCount >= 6 && params[6] != -1)
		{
			cell *iconAddr;
			amx_GetAddr(amx, params[6], &iconAddr);
			if (iconAddr)
			{
				static char iconBuf[64];
				amx_GetString(iconBuf, iconAddr, 0, sizeof(iconBuf));
				if (iconBuf[0])
					icon = iconBuf;
			}
		}
		if (argCount >= 7) flash = params[7];

		// 原版 StatusIcon: 首字节 status 按 flash 换算 (flash == 1 ? 2 : 1), 其后为 icon + RGB
		MESSAGE_BEGIN(MSG_ONE, gmsgStatusIcon, nullptr, pEdict);
			WRITE_BYTE(flash == 1 ? 2 : 1);
			WRITE_STRING(icon);
			WRITE_BYTE(r);
			WRITE_BYTE(g);
			WRITE_BYTE(b);
		MESSAGE_END();
	}
	else
	{
		MESSAGE_BEGIN(MSG_ONE, gmsgStatusIcon, nullptr, pEdict);
			WRITE_BYTE(0);
			WRITE_STRING("defuser");
		MESSAGE_END();
	}

	return 1;
}

cell AMX_NATIVE_CALL cs_get_user_plant(AMX *amx, cell *params)
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

	return pPlayer->IsBombGuy() ? 1 : 0;
}

cell AMX_NATIVE_CALL cs_set_user_plant(AMX *amx, cell *params)
{
	int index = params[1];
	int plant = params[2];

	// 第 3 参 showbombicon: 安放 C4 时是否显示炸弹图标 (默认 1)
	int argCount = params[0] / (int)sizeof(cell);
	bool showBombIcon = (argCount >= 3) ? (params[3] != 0) : true;

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;

	// 原版语义: 只写 m_bHasC4 标志并控制 StatusIcon (显示/隐藏 C4 图标), 不触碰武器列表
	pPlayer->m_bHasC4 = (plant != 0);

	if (plant)
	{
		if (showBombIcon)
		{
			MESSAGE_BEGIN(MSG_ONE, gmsgStatusIcon, nullptr, pEdict);
				WRITE_BYTE(1);
				WRITE_STRING("c4");
				WRITE_BYTE(0);   // R
				WRITE_BYTE(160); // G
				WRITE_BYTE(0);   // B
			MESSAGE_END();
		}
	}
	else
	{
		// 清除炸弹图标 (参照原版, 不受 showbombicon 控制)
		MESSAGE_BEGIN(MSG_ONE, gmsgStatusIcon, nullptr, pEdict);
			WRITE_BYTE(0);
			WRITE_STRING("c4");
		MESSAGE_END();
	}

	return 1;
}

cell AMX_NATIVE_CALL cs_get_user_primweapon(AMX *amx, cell *params)
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

	CBasePlayerItem *pWeapon = pPlayer->m_rgpPlayerItems[PRIMARY_WEAPON_SLOT];
	if (!pWeapon)
		return 0;

	CBasePlayerWeapon *pBaseWeapon = static_cast<CBasePlayerWeapon *>(pWeapon);
	return pBaseWeapon->m_iId;
}

cell AMX_NATIVE_CALL cs_get_user_secweapon(AMX *amx, cell *params)
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

	CBasePlayerItem *pWeapon = pPlayer->m_rgpPlayerItems[PISTOL_SLOT];
	if (!pWeapon)
		return 0;

	CBasePlayerWeapon *pBaseWeapon = static_cast<CBasePlayerWeapon *>(pWeapon);
	return pBaseWeapon->m_iId;
}

// cs_get_user_weapon(index, &clip=0, &ammo=0) — returns active weapon id, writes clip and ammo by-ref
cell AMX_NATIVE_CALL cs_get_user_weapon(AMX *amx, cell *params)
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

	if (pPlayer->m_pActiveItem)
	{
		CBasePlayerWeapon *pWeapon = static_cast<CBasePlayerWeapon *>(pPlayer->m_pActiveItem);

		cell *clipRef;
		amx_GetAddr(amx, params[2], &clipRef);
		if (clipRef)
			*clipRef = pWeapon->m_iClip;

		cell *ammoRef;
		amx_GetAddr(amx, params[3], &ammoRef);
		if (ammoRef)
		{
			int iAmmoType = pWeapon->m_iPrimaryAmmoType;
			*ammoRef = (iAmmoType <= 0) ? 0 : pPlayer->m_rgAmmo[iAmmoType];
		}

		return pWeapon->m_iId;
	}

	return 0;
}

// cs_get_user_hasprim(index) — returns 1 if player has a primary weapon
cell AMX_NATIVE_CALL cs_get_user_hasprim(AMX *amx, cell *params)
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

	return pPlayer->m_bHasPrimary ? 1 : 0;
}

cell AMX_NATIVE_CALL cs_get_user_kevlar(AMX *amx, cell *params)
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

	return static_cast<cell>(pPlayer->m_iKevlar);
}

cell AMX_NATIVE_CALL cs_set_user_kevlar(AMX *amx, cell *params)
{
	int index = params[1];
	int armorType = params[2];

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;

	pPlayer->m_iKevlar = static_cast<ArmorType>(armorType);
	return 1;
}

cell AMX_NATIVE_CALL cs_get_user_helmet(AMX *amx, cell *params)
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

	return (pPlayer->m_iKevlar == ARMOR_VESTHELM) ? 1 : 0;
}

cell AMX_NATIVE_CALL cs_set_user_helmet(AMX *amx, cell *params)
{
	int index = params[1];
	int hasHelmet = params[2];

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;

	if (hasHelmet) {
		pPlayer->m_iKevlar = ARMOR_VESTHELM;
	} else if (pPlayer->m_iKevlar == ARMOR_VESTHELM) {
		pPlayer->m_iKevlar = ARMOR_KEVLAR;
	}
	return 1;
}

cell AMX_NATIVE_CALL cs_get_user_nvg(AMX *amx, cell *params)
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

	return pPlayer->m_bHasNightVision ? 1 : 0;
}

cell AMX_NATIVE_CALL cs_set_user_nvg(AMX *amx, cell *params)
{
	int index = params[1];
	int hasNVG = params[2];

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;

	pPlayer->m_bHasNightVision = (hasNVG != 0);
	return 1;
}

cell AMX_NATIVE_CALL cs_get_user_defuser(AMX *amx, cell *params)
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

	return pPlayer->m_bHasDefuser ? 1 : 0;
}

cell AMX_NATIVE_CALL cs_set_user_defuser(AMX *amx, cell *params)
{
	int index = params[1];
	int hasDefuser = params[2];

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;

	pPlayer->m_bHasDefuser = (hasDefuser != 0);
	return 1;
}

cell AMX_NATIVE_CALL cs_set_user_model(AMX *amx, cell *params)
{
	int index = params[1];

	// 原版校验: 拒绝无效字符串句柄 (-1) 与空字符串
	if (params[2] == -1)
	{
		AMXX_LOG_ERR("[cstrike] cs_set_user_model: Invalid model");
		return 0;
	}

	cell *addr = nullptr;
	amx_GetAddr(amx, params[2], &addr);
	if (!addr)
	{
		AMXX_LOG_ERR("[cstrike] cs_set_user_model: Invalid model");
		return 0;
	}

	char modelName[128];
	amx_GetString(modelName, addr, 0, sizeof(modelName));

	if (!modelName[0])
	{
		AMXX_LOG_ERR("[cstrike] cs_set_user_model: Model can not be empty");
		return 0;
	}

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;

	pPlayer->SetNewPlayerModel(modelName);
	return 1;
}

cell AMX_NATIVE_CALL cs_reset_user_model(AMX *amx, cell *params)
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

	pPlayer->SetPlayerModel(pPlayer->m_bHasC4);
	return 1;
}

// 武器 ID -> m_rgAmmo 弹药槽位映射
// 使用 ReGameDLL 的 GetWeaponInfo() -> ammoType (AmmoType 枚举即 m_rgAmmo 槽位索引, 与原版 ammoIndex1 一致)
static int CsWeaponToAmmoIndex(int weaponId)
{
	WeaponInfoStruct *info = GetWeaponInfo(weaponId);
	if (!info)
		return -1;

	int ammoIndex = (int)info->ammoType;
	if (ammoIndex <= AMMO_NONE || ammoIndex >= AMMO_MAX_TYPES)
		return -1;

	return ammoIndex;
}

cell AMX_NATIVE_CALL cs_set_user_bpammo(AMX *amx, cell *params)
{
	int index = params[1];
	int weaponId = params[2];
	int amount = params[3];

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;

	int ammoIndex = CsWeaponToAmmoIndex(weaponId);
	if (ammoIndex < 0 || ammoIndex >= MAX_AMMO_SLOTS)
	{
		AMXX_LOG_ERR("Invalid weapon id %d", weaponId);
		return 0;
	}

	pPlayer->m_rgAmmo[ammoIndex] = amount;
	return 1;
}

cell AMX_NATIVE_CALL cs_get_user_bpammo(AMX *amx, cell *params)
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

	int ammoIndex = CsWeaponToAmmoIndex(weaponId);
	if (ammoIndex < 0 || ammoIndex >= MAX_AMMO_SLOTS)
	{
		AMXX_LOG_ERR("Invalid weapon id %d", weaponId);
		return 0;
	}

	return pPlayer->m_rgAmmo[ammoIndex];
}

cell AMX_NATIVE_CALL cs_get_user_zoom(AMX *amx, cell *params)
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

	// 映射: 90->CS_SET_NO_ZOOM(1), 40->CS_SET_FIRST_ZOOM(2),
	//       15/10->CS_SET_SECOND_ZOOM(3), 25->CS_SET_AUGSG552_ZOOM(4), 其它->0
	switch (pPlayer->m_iFOV)
	{
		case 90: return 1; // CS_SET_NO_ZOOM
		case 40: return 2; // CS_SET_FIRST_ZOOM
		case 15:
		case 10: return 3; // CS_SET_SECOND_ZOOM
		case 25: return 4; // CS_SET_AUGSG552_ZOOM
		default: return 0;
	}
}

cell AMX_NATIVE_CALL cs_set_user_zoom(AMX *amx, cell *params)
{
	int index = params[1];
	int zoomType = params[2];

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;

	int fov;
	switch (zoomType)
	{
		case 0: // CS_RESET_ZOOM
		case 1: // CS_SET_NO_ZOOM
			fov = 90;
			break;
		case 2: // CS_SET_FIRST_ZOOM
			fov = 40;
			break;
		case 3: // CS_SET_SECOND_ZOOM
		{
			// 非 AWP 狙击 (G3SG1/SG550/SCOUT) 用 10, 其它 (AWP 类) 用 15 (参照原版)
			int weapon = 0;
			if (pPlayer->m_pActiveItem)
				weapon = static_cast<CBasePlayerWeapon *>(pPlayer->m_pActiveItem)->m_iId;
			fov = (weapon == WEAPON_G3SG1 || weapon == WEAPON_SG550 || weapon == WEAPON_SCOUT) ? 10 : 15;
			break;
		}
		case 4: // CS_SET_AUGSG552_ZOOM
			fov = 25;
			break;
		default:
			return 0;
	}

	pPlayer->m_iFOV = fov;
	return 1;
}

cell AMX_NATIVE_CALL cs_get_user_submodel(AMX *amx, cell *params)
{
	int index = params[1];

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	return (cell)pEdict->v.body;
}

cell AMX_NATIVE_CALL cs_set_user_submodel(AMX *amx, cell *params)
{
	int index = params[1];
	int submodel = params[2];

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	pEdict->v.body = submodel;
	return 1;
}

cell AMX_NATIVE_CALL cs_get_user_lastactivity(AMX *amx, cell *params)
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

	// 原版语义: 返回 m_fLastMovement 原始时间戳 (绝对游戏时间), 而非相对流逝时间
	return amx_ftoc(pPlayer->m_fLastMovement);
}

cell AMX_NATIVE_CALL cs_set_user_lastactivity(AMX *amx, cell *params)
{
	int index = params[1];
	float lastActivity = amx_ctof(params[2]);

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;

	pPlayer->m_fLastMovement = lastActivity;
	return 1;
}

cell AMX_NATIVE_CALL cs_get_user_tked(AMX *amx, cell *params)
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

	return (pPlayer->m_iTeamKills > 0) ? 1 : 0;
}

cell AMX_NATIVE_CALL cs_set_user_tked(AMX *amx, cell *params)
{
	int index = params[1];
	int tkValue = params[2];

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;

	pPlayer->m_iTeamKills = tkValue;
	return 1;
}

cell AMX_NATIVE_CALL cs_get_user_vip(AMX *amx, cell *params)
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

	return pPlayer->m_bIsVIP ? 1 : 0;
}

cell AMX_NATIVE_CALL cs_get_user_hostagekills(AMX *amx, cell *params)
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

	return (cell)pPlayer->m_iHostagesKilled;
}

cell AMX_NATIVE_CALL amxx_cs_get_user_model(AMX *amx, cell *params)
{
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;

    // 原版: 读取客户端 info 缓冲的 "model" keyvalue (GETCLIENTKEYVALUE)
    const char *model = g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pEdict), "model");
    if (!model) model = "";

    cell *dest;
    amx_GetAddr(amx, params[2], &dest);

    return amx_SetString(dest, model, 0, 0, (int)params[3]);
}

// ===== P3: CStrike 扩展 =====

// cs_get_user_plant_zone(index) - 获取玩家所在 C4 安放区 (0=无, 1=A, 2=B)
// 注意：m_bInBombZone 在当前 CBasePlayer 中不存在，返回 0
cell AMX_NATIVE_CALL cs_get_user_plant_zone(AMX *amx, cell *params)
{
    int index = params[1];
    (void)amx;
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;
    CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
    if (!pPlayer) return 0;
    // m_bInBombZone 不存在于当前 CBasePlayer 实现中
    return 0;
}

// cs_get_user_plant_target(index) - 获取 C4 安放目标实体索引
cell AMX_NATIVE_CALL cs_get_user_plant_target(AMX *amx, cell *params)
{
    int index = params[1];
    (void)amx;
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;
    CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
    if (!pPlayer) return 0;
    // Return the bomb target entity if player has C4 and is in bomb zone
    if (pPlayer->m_bHasC4 && false /* m_bInBombZone doesn't exist */)
        return index;
    return 0;
}

// ===== 缺失的 AMXX cstrike natives 补全 =====

// cs_get_user_driving(index) - CS 中默认无载具 (func_tank 不算驾驶), 简化返回 0
cell AMX_NATIVE_CALL cs_get_user_driving(AMX *amx, cell *params)
{
    (void)amx;
    int index = params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;
    // CS 默认无载具, 返回 0
    return 0;
}

// cs_set_user_vip(index, vip=1, model=1, scoreboard=1)
// 镜像 cs_get_user_vip: 直接读写 pPlayer->m_bIsVIP
cell AMX_NATIVE_CALL cs_set_user_vip(AMX *amx, cell *params)
{
    (void)amx;
    int index = params[1];
    int vip = params[2];
    int argCount = params[0] / (int)sizeof(cell);
    int model = (argCount >= 3) ? params[3] : 1;
    int scoreboard = (argCount >= 4) ? params[4] : 1;

    if (index < 1 || index > gpGlobals->maxClients)
        return 0;

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;

    CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
    if (!pPlayer)
        return 0;

    pPlayer->m_bIsVIP = (vip != 0);

    // 更新模型为 VIP 模型
    if (model && vip)
    {
        pPlayer->m_iModelName = MODEL_VIP;
        pPlayer->SetPlayerModel(pPlayer->m_bHasC4);
    }

    // 更新计分板
    if (scoreboard)
    {
        MESSAGE_BEGIN(MSG_ALL, gmsgScoreInfo);
            WRITE_BYTE(index);
            WRITE_SHORT((int)pEdict->v.frags);
            WRITE_SHORT(pPlayer->m_iDeaths);
            WRITE_SHORT(0);
            WRITE_SHORT(pPlayer->m_iTeam);
        MESSAGE_END();
    }

    return 1;
}

// cs_set_user_hostagekills(index, value)
// 镜像 cs_get_user_hostagekills: 直接写 pPlayer->m_iHostagesKilled
cell AMX_NATIVE_CALL cs_set_user_hostagekills(AMX *amx, cell *params)
{
    (void)amx;
    int index = params[1];
    int value = params[2];

    if (index < 1 || index > gpGlobals->maxClients)
        return 0;

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;

    CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
    if (!pPlayer)
        return 0;

    pPlayer->m_iHostagesKilled = value;
    return 1;
}

// cs_user_spawn(player) - 复活玩家 (仅当玩家死亡时调用 Spawn)
cell AMX_NATIVE_CALL cs_user_spawn(AMX *amx, cell *params)
{
    (void)amx;
    int index = params[1];

    if (index < 1 || index > gpGlobals->maxClients)
        return 0;

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;

    CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
    if (!pPlayer)
        return 0;

    // 仅在玩家死亡时复活 (避免对存活玩家造成意外重置)
    if (pEdict->v.deadflag == DEAD_NO && pEdict->v.health > 0)
        return 1;

    pPlayer->Spawn();
    return 1;
}

// cs_get_weaponbox_item(weaponboxIndex) - CS weaponbox 私有数据布局依赖实现, 简化返回 0
cell AMX_NATIVE_CALL cs_get_weaponbox_item(AMX *amx, cell *params)
{
    (void)amx;
    (void)params;
    return 0;
}

// cs_create_entity(const classname[])
cell AMX_NATIVE_CALL cs_create_entity(AMX *amx, cell *params)
{
    cell *addr;
    amx_GetAddr(amx, params[1], &addr);
    if (!addr)
        return 0;

    char classname[128];
    amx_GetString(classname, addr, 0, sizeof(classname));

    edict_t *pEnt = CREATE_ENTITY();
    if (!pEnt)
        return 0;

    pEnt->v.classname = ALLOC_STRING(classname);
    DispatchSpawn(pEnt);

    return ENTINDEX(pEnt);
}

// cs_find_ent_by_class(start_index, const classname[])
// 从 start_index + 1 开始遍历实体, 匹配 classname
cell AMX_NATIVE_CALL cs_find_ent_by_class(AMX *amx, cell *params)
{
    (void)amx;
    int start = params[1];

    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    if (!addr)
        return 0;

    char classname[128];
    amx_GetString(classname, addr, 0, sizeof(classname));

    for (int i = start + 1; i <= gpGlobals->maxEntities; i++)
    {
        edict_t *pEnt = INDEXENT(i);
        if (!pEnt || pEnt->free)
            continue;

        const char *cls = STRING(pEnt->v.classname);
        if (cls && strcmp(cls, classname) == 0)
            return i;
    }
    return 0;
}

// cs_find_ent_by_owner(start_index, const classname[], owner)
// 遍历实体, 匹配 classname 与 owner
cell AMX_NATIVE_CALL cs_find_ent_by_owner(AMX *amx, cell *params)
{
    (void)amx;
    int start = params[1];
    int owner = params[3];

    if (owner < 1 || owner > gpGlobals->maxClients)
        return 0;

    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    if (!addr)
        return 0;

    char classname[128];
    amx_GetString(classname, addr, 0, sizeof(classname));

    edict_t *pOwner = INDEXENT(owner);
    if (!pOwner)
        return 0;

    for (int i = start + 1; i <= gpGlobals->maxEntities; i++)
    {
        edict_t *pEnt = INDEXENT(i);
        if (!pEnt || pEnt->free)
            continue;

        if (pEnt->v.owner != pOwner)
            continue;

        // classname 为空时匹配任意 (兼容常见用法); 否则要求精确匹配
        const char *cls = STRING(pEnt->v.classname);
        if (classname[0] == '\0' || (cls && strcmp(cls, classname) == 0))
            return i;
    }
    return 0;
}

// cs_set_ent_class(index, const classname[])
cell AMX_NATIVE_CALL cs_set_ent_class(AMX *amx, cell *params)
{
    (void)amx;
    int index = params[1];

    if (index < 1 || index > gpGlobals->maxEntities)
        return 0;

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free)
        return 0;

    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    if (!addr)
        return 0;

    char classname[128];
    amx_GetString(classname, addr, 0, sizeof(classname));

    pEdict->v.classname = ALLOC_STRING(classname);
    return 1;
}

// cs_get_item_id(const name[], &CsWeaponClassType:classid=CS_WEAPONCLASS_NONE)
// 兼容 "weapon_*" 与裸别名, 返回 CSW_* ID, classid 输出武器类别
cell AMX_NATIVE_CALL cs_get_item_id(AMX *amx, cell *params)
{
    cell *addr;
    amx_GetAddr(amx, params[1], &addr);
    if (!addr)
        return 0;

    char name[64];
    amx_GetString(name, addr, 0, sizeof(name));

    int weaponId = 0;

    if      (strcmp(name, "weapon_p228") == 0 || strcmp(name, "p228") == 0)         weaponId = CSW_P228;
    else if (strcmp(name, "weapon_glock18") == 0 || strcmp(name, "glock18") == 0)   weaponId = CSW_GLOCK18;
    else if (strcmp(name, "weapon_scout") == 0 || strcmp(name, "scout") == 0)       weaponId = CSW_SCOUT;
    else if (strcmp(name, "weapon_hegrenade") == 0 || strcmp(name, "hegrenade") == 0) weaponId = CSW_HEGRENADE;
    else if (strcmp(name, "weapon_xm1014") == 0 || strcmp(name, "xm1014") == 0)     weaponId = CSW_XM1014;
    else if (strcmp(name, "weapon_c4") == 0 || strcmp(name, "c4") == 0)             weaponId = CSW_C4;
    else if (strcmp(name, "weapon_mac10") == 0 || strcmp(name, "mac10") == 0)       weaponId = CSW_MAC10;
    else if (strcmp(name, "weapon_aug") == 0 || strcmp(name, "aug") == 0)           weaponId = CSW_AUG;
    else if (strcmp(name, "weapon_smokegrenade") == 0 || strcmp(name, "smokegrenade") == 0) weaponId = CSW_SMOKEGRENADE;
    else if (strcmp(name, "weapon_elite") == 0 || strcmp(name, "elite") == 0)       weaponId = CSW_ELITE;
    else if (strcmp(name, "weapon_fiveseven") == 0 || strcmp(name, "fiveseven") == 0) weaponId = CSW_FIVESEVEN;
    else if (strcmp(name, "weapon_ump45") == 0 || strcmp(name, "ump45") == 0)       weaponId = CSW_UMP45;
    else if (strcmp(name, "weapon_sg550") == 0 || strcmp(name, "sg550") == 0)       weaponId = CSW_SG550;
    else if (strcmp(name, "weapon_galil") == 0 || strcmp(name, "galil") == 0)       weaponId = CSW_GALIL;
    else if (strcmp(name, "weapon_famas") == 0 || strcmp(name, "famas") == 0)       weaponId = CSW_FAMAS;
    else if (strcmp(name, "weapon_usp") == 0 || strcmp(name, "usp") == 0)           weaponId = CSW_USP;
    else if (strcmp(name, "weapon_awp") == 0 || strcmp(name, "awp") == 0)           weaponId = CSW_AWP;
    else if (strcmp(name, "weapon_mp5navy") == 0 || strcmp(name, "mp5navy") == 0)   weaponId = CSW_MP5NAVY;
    else if (strcmp(name, "weapon_m249") == 0 || strcmp(name, "m249") == 0)         weaponId = CSW_M249;
    else if (strcmp(name, "weapon_m3") == 0 || strcmp(name, "m3") == 0)             weaponId = CSW_M3;
    else if (strcmp(name, "weapon_m4a1") == 0 || strcmp(name, "m4a1") == 0)         weaponId = CSW_M4A1;
    else if (strcmp(name, "weapon_tmp") == 0 || strcmp(name, "tmp") == 0)           weaponId = CSW_TMP;
    else if (strcmp(name, "weapon_g3sg1") == 0 || strcmp(name, "g3sg1") == 0)       weaponId = CSW_G3SG1;
    else if (strcmp(name, "weapon_flashbang") == 0 || strcmp(name, "flashbang") == 0) weaponId = CSW_FLASHBANG;
    else if (strcmp(name, "weapon_deagle") == 0 || strcmp(name, "deagle") == 0)     weaponId = CSW_DEAGLE;
    else if (strcmp(name, "weapon_sg552") == 0 || strcmp(name, "sg552") == 0)       weaponId = CSW_SG552;
    else if (strcmp(name, "weapon_ak47") == 0 || strcmp(name, "ak47") == 0)         weaponId = CSW_AK47;
    else if (strcmp(name, "weapon_knife") == 0 || strcmp(name, "knife") == 0)       weaponId = CSW_KNIFE;
    else if (strcmp(name, "weapon_p90") == 0 || strcmp(name, "p90") == 0)           weaponId = CSW_P90;

    if (weaponId != 0)
    {
        // CsWeaponClassType 与 WeaponClassType 数值一致
        WeaponClassType cls = WeaponIDToWeaponClass(weaponId);
        int argCount = params[0] / (int)sizeof(cell);
        if (argCount >= 2)
        {
            cell *classRef;
            amx_GetAddr(amx, params[2], &classRef);
            if (classRef)
                *classRef = static_cast<cell>(cls);
        }
    }

    return weaponId;
}

// cs_get_item_alias(itemid, name[], name_maxlen, altname[]="", altname_maxlen=0)
// 返回武器别名到 name[], 备用别名到 altname[] (CS 中无备用别名, 留空)
cell AMX_NATIVE_CALL cs_get_item_alias(AMX *amx, cell *params)
{
    int itemid = params[1];

    const char *weaponName = nullptr;
    const char *altName = "";

    switch (itemid) {
        case CSW_P228:         weaponName = "p228";         break;
        case CSW_GLOCK18:      weaponName = "glock18";      break;
        case CSW_SCOUT:        weaponName = "scout";        break;
        case CSW_HEGRENADE:    weaponName = "hegrenade";    break;
        case CSW_XM1014:       weaponName = "xm1014";       break;
        case CSW_C4:           weaponName = "c4";           break;
        case CSW_MAC10:        weaponName = "mac10";        break;
        case CSW_AUG:          weaponName = "aug";          break;
        case CSW_SMOKEGRENADE: weaponName = "smokegrenade"; break;
        case CSW_ELITE:        weaponName = "elite";        break;
        case CSW_FIVESEVEN:    weaponName = "fiveseven";    break;
        case CSW_UMP45:        weaponName = "ump45";        break;
        case CSW_SG550:        weaponName = "sg550";        break;
        case CSW_GALIL:        weaponName = "galil";        break;
        case CSW_FAMAS:        weaponName = "famas";        break;
        case CSW_USP:          weaponName = "usp";          break;
        case CSW_AWP:          weaponName = "awp";          break;
        case CSW_MP5NAVY:      weaponName = "mp5navy";      break;
        case CSW_M249:         weaponName = "m249";         break;
        case CSW_M3:           weaponName = "m3";           break;
        case CSW_M4A1:         weaponName = "m4a1";         break;
        case CSW_TMP:          weaponName = "tmp";          break;
        case CSW_G3SG1:        weaponName = "g3sg1";        break;
        case CSW_FLASHBANG:    weaponName = "flashbang";    break;
        case CSW_DEAGLE:       weaponName = "deagle";       break;
        case CSW_SG552:        weaponName = "sg552";        break;
        case CSW_AK47:         weaponName = "ak47";         break;
        case CSW_KNIFE:        weaponName = "knife";        break;
        case CSW_P90:          weaponName = "p90";          break;
        default: return 0;
    }

    cell *nameAddr;
    amx_GetAddr(amx, params[2], &nameAddr);
    if (nameAddr)
        amx_SetString(nameAddr, weaponName, 0, 0, params[3]);

    int argCount = params[0] / (int)sizeof(cell);
    if (argCount >= 5)
    {
        cell *altAddr;
        amx_GetAddr(amx, params[4], &altAddr);
        if (altAddr)
            amx_SetString(altAddr, altName, 0, 0, params[5]);
    }

    return 1;
}

// cs_get_translated_item_alias(const alias[], itemname[], maxlength)
// 简化: 直接将别名复制到 itemname
cell AMX_NATIVE_CALL cs_get_translated_item_alias(AMX *amx, cell *params)
{
    cell *srcAddr;
    amx_GetAddr(amx, params[1], &srcAddr);
    if (!srcAddr)
        return 0;

    char alias[64];
    amx_GetString(alias, srcAddr, 0, sizeof(alias));

    cell *destAddr;
    amx_GetAddr(amx, params[2], &destAddr);
    if (!destAddr)
        return 0;

    amx_SetString(destAddr, alias, 0, 0, params[3]);
    return 1;
}

// cs_get_user_weapon_entity(playerIndex)
// 返回玩家当前活跃武器实体索引
cell AMX_NATIVE_CALL cs_get_user_weapon_entity(AMX *amx, cell *params)
{
    (void)amx;
    int index = params[1];

    if (index < 1 || index > gpGlobals->maxClients)
        return 0;

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;

    CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
    if (!pPlayer || !pPlayer->m_pActiveItem)
        return 0;

    return pPlayer->m_pActiveItem->entindex();
}

AMX_NATIVE_INFO cstrike_natives[] = {
	{"cs_get_user_team", cs_get_user_team},
	{"cs_set_user_team", cs_set_user_team},
	{"cs_get_user_money", cs_get_user_money},
	{"cs_set_user_money", cs_set_user_money},
	{"cs_get_user_deaths", cs_get_user_deaths},
	{"cs_set_user_deaths", cs_set_user_deaths},
	{"cs_get_user_armor", cs_get_user_armor},
	{"cs_set_user_armor", cs_set_user_armor},
	{"cs_get_user_defuse", cs_get_user_defuse},
	{"cs_set_user_defuse", cs_set_user_defuse},
	{"cs_get_user_plant", cs_get_user_plant},
	{"cs_set_user_plant", cs_set_user_plant},
	{"cs_get_user_primweapon", cs_get_user_primweapon},
	{"cs_get_user_secweapon", cs_get_user_secweapon},
	{"cs_get_user_weapon", cs_get_user_weapon},
	{"cs_get_user_hasprim", cs_get_user_hasprim},
	{"cs_get_user_kevlar", cs_get_user_kevlar},
	{"cs_set_user_kevlar", cs_set_user_kevlar},
	{"cs_get_user_helmet", cs_get_user_helmet},
	{"cs_set_user_helmet", cs_set_user_helmet},
	{"cs_get_user_nvg", cs_get_user_nvg},
	{"cs_set_user_nvg", cs_set_user_nvg},
	{"cs_get_user_defuser", cs_get_user_defuser},
	{"cs_set_user_defuser", cs_set_user_defuser},
	{"cs_set_user_model", cs_set_user_model},
	{"cs_reset_user_model", cs_reset_user_model},
	{"cs_set_user_bpammo", cs_set_user_bpammo},
	{"cs_get_user_bpammo", cs_get_user_bpammo},
	{"cs_get_user_zoom", cs_get_user_zoom},
	{"cs_set_user_zoom", cs_set_user_zoom},
	{"cs_get_user_submodel", cs_get_user_submodel},
	{"cs_set_user_submodel", cs_set_user_submodel},
	{"cs_get_user_lastactivity", cs_get_user_lastactivity},
	{"cs_set_user_lastactivity", cs_set_user_lastactivity},
	{"cs_get_user_tked", cs_get_user_tked},
	{"cs_set_user_tked", cs_set_user_tked},
	{"cs_get_user_vip", cs_get_user_vip},
	{"cs_get_user_hostagekills", cs_get_user_hostagekills},
	// P1: CStrike 增强
	{"cs_get_user_armortype", cs_get_user_armortype},
	{"cs_set_user_armortype", cs_set_user_armortype},
	{"cs_get_no_knives", cs_get_no_knives},
	{"cs_set_no_knives", cs_set_no_knives},
	{"cs_get_weapon_info", cs_get_weapon_info},
	{"cs_get_weapon_ammo", amxx_cs_get_weapon_ammo},
	{"cs_set_weapon_ammo", cs_set_weapon_ammo},
	{"cs_get_weapon_clip", cs_get_weapon_clip},
	{"cs_set_weapon_clip", cs_set_weapon_clip},
	{"cs_get_user_weap_ammo", cs_get_user_weap_ammo},
	// P2: CStrike 武器增强
	{"cs_get_weapon_burst", cs_get_weapon_burst},
	{"cs_set_weapon_burst", cs_set_weapon_burst},
	{"cs_get_weapon_silen", cs_get_weapon_silen},
	{"cs_set_weapon_silen", cs_set_weapon_silen},
	{"cs_get_armoury_type", cs_get_armoury_type},
	{"cs_set_armoury_type", cs_set_armoury_type},
	{"cs_get_user_mapzones", cs_get_user_mapzones},
	{"cs_get_user_buyzone", cs_get_user_buyzone},
	// P3: CStrike 扩展
	{"cs_get_user_team_id", cs_get_user_team_id},
	{"cs_get_user_stationary", cs_get_user_stationary},
	{"cs_get_hostage_foll", cs_get_hostage_follow},   // 官方注册名 (原版 cstrike 模块)
	{"cs_set_hostage_foll", cs_set_hostage_follow},   // 官方注册名 (原版 cstrike 模块)
	{"cs_get_hostage_follow", cs_get_hostage_follow}, // 兼容别名
	{"cs_set_hostage_follow", cs_set_hostage_follow}, // 兼容别名
	{"cs_get_hostage_id", cs_get_hostage_id},
	// P1-6: 人质 lastuse/nextuse + C4 explode/defusing
	{"cs_get_hostage_lastuse", cs_get_hostage_lastuse},
	{"cs_set_hostage_lastuse", cs_set_hostage_lastuse},
	{"cs_get_hostage_nextuse", cs_get_hostage_nextuse},
	{"cs_set_hostage_nextuse", cs_set_hostage_nextuse},
	{"cs_get_c4_explode_time", cs_get_c4_explode_time},
	{"cs_set_c4_explode_time", cs_set_c4_explode_time},
	{"cs_get_c4_defusing", cs_get_c4_defusing},
	{"cs_set_c4_defusing", cs_set_c4_defusing},
	{"cs_get_user_shield", cs_get_user_shield},
	{"cs_set_user_shield", cs_set_user_shield},
	{"cs_get_user_model", amxx_cs_get_user_model},
	// P3: CStrike 扩展
	{"cs_get_user_plant_zone", cs_get_user_plant_zone},
	{"cs_get_user_plant_target", cs_get_user_plant_target},

	// ===== 兼容性别名 =====
	{"cs_get_user_hp", amxx_get_user_health},
	{"cs_set_user_hp", amxx_set_user_health},
	{"cs_get_user_ap", amxx_get_user_armor},
	{"cs_set_user_ap", amxx_set_user_armor},
	{"cs_get_weapon_name", amxx_get_weaponname},
	{"cs_get_weapon_id", amxx_get_weapon_id},
	{"cs_get_user_teamname", cs_get_user_team},
	// 缺失的 AMXX cstrike natives 补全
	{"cs_get_user_driving", cs_get_user_driving},
	{"cs_set_user_vip", cs_set_user_vip},
	{"cs_set_user_hostagekills", cs_set_user_hostagekills},
	{"cs_user_spawn", cs_user_spawn},
	{"cs_get_weaponbox_item", cs_get_weaponbox_item},
	{"cs_create_entity", cs_create_entity},
	{"cs_find_ent_by_class", cs_find_ent_by_class},
	{"cs_find_ent_by_owner", cs_find_ent_by_owner},
	{"cs_set_ent_class", cs_set_ent_class},
	{"cs_get_item_id", cs_get_item_id},
	{"cs_get_item_alias", cs_get_item_alias},
	{"cs_get_translated_item_alias", cs_get_translated_item_alias},
	{"cs_get_user_weapon_entity", cs_get_user_weapon_entity},
	{nullptr, nullptr}
};

// ===== P1: CStrike 增强 =====

cell AMX_NATIVE_CALL cs_get_user_armortype(AMX *amx, cell *params)
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

	return static_cast<cell>(pPlayer->m_iKevlar);
}

cell AMX_NATIVE_CALL cs_set_user_armortype(AMX *amx, cell *params)
{
	int index = params[1];
	int armorType = params[2];

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;

	pPlayer->m_iKevlar = static_cast<ArmorType>(armorType);
	return 1;
}

// m_bNotKilled 是 CBasePlayer 中控制禁止刀子的标志
// 在 ReGameDLL 中，no_knives 相当于 CSGameRules 的状态
static bool g_noKnives = false;

cell AMX_NATIVE_CALL cs_get_no_knives(AMX *amx, cell *params)
{
	(void)amx; (void)params;
	return g_noKnives ? 1 : 0;
}

cell AMX_NATIVE_CALL cs_set_no_knives(AMX *amx, cell *params)
{
	(void)amx;
	int noKnives = params[1];

	g_noKnives = (noKnives != 0);

	// 应用到所有已连接的玩家
	for (int i = 1; i <= gpGlobals->maxClients; i++) {
		edict_t *pEdict = INDEXENT(i);
		if (!pEdict || pEdict->free || !(pEdict->v.flags & FL_CLIENT))
			continue;
		CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
		if (!pPlayer)
			continue;

		if (g_noKnives) {
			// 移除刀子（遍历武器槽查找）
			for (int slot = 0; slot < MAX_ITEM_TYPES; slot++) {
				CBasePlayerItem *pItem = pPlayer->m_rgpPlayerItems[slot];
				while (pItem) {
					CBasePlayerItem *nextItem = pItem->m_pNext;
					if (pItem->m_iId == WEAPON_KNIFE) {
						pPlayer->RemovePlayerItem(pItem);
						break;
					}
					pItem = nextItem;
				}
			}
		} else {
			// 给予刀子（如果还没有）
			if (!pPlayer->HasNamedPlayerItem("weapon_knife")) {
				pPlayer->GiveNamedItem("weapon_knife");
			}
		}
	}
	return 1;
}

// cs_get_weapon_info(weapon_id, CsWeaponInfo:type) — 直接返回 cell (无字符串参数)
// type: 0=CS_WEAPONINFO_COST(价格), 1=CS_WEAPONINFO_AMMO(弹夹价格),
//       2=CS_WEAPONINFO_AMMO_TYPE(弹药类型), 3=CS_WEAPONINFO_MAXCLIP(弹夹容量),
//       4=CS_WEAPONINFO_MAXAMMO(最大备弹), 5=CS_WEAPONINFO_SLOT(武器槽位)
// 数据取自 ReGameDLL 的 GetWeaponInfo()/GetWeaponSlot() (与原版 cstrike.cpp WeaponsList 表一致)
cell AMX_NATIVE_CALL cs_get_weapon_info(AMX *amx, cell *params)
{
	int weaponId = params[1];
	int type = params[2];

	WeaponInfoStruct *info = GetWeaponInfo(weaponId);
	if (!info)
		return 0;

	switch (type) {
		case 0: // CS_WEAPONINFO_COST
			return info->cost;
		case 1: // CS_WEAPONINFO_AMMO
			return info->clipCost;
		case 2: // CS_WEAPONINFO_AMMO_TYPE
			return info->ammoType;
		case 3: // CS_WEAPONINFO_MAXCLIP
			return info->gunClipSize;
		case 4: // CS_WEAPONINFO_MAXAMMO
			return info->maxRounds;
		case 5: // CS_WEAPONINFO_SLOT
		{
			WeaponSlotInfo *slot = GetWeaponSlot((WeaponIdType)weaponId);
			return slot ? slot->slot : 0;
		}
		default:
			return 0;
	}
}

// cs_get_weapon_ammo(index)
// 返回指定武器实体的弹药数（当前弹匣中的子弹数）
// index 是武器实体索引（不是玩家索引）
cell AMX_NATIVE_CALL amxx_cs_get_weapon_ammo(AMX *amx, cell *params)
{
	int index = (int)params[1];
	if (index < 1 || index > gpGlobals->maxEntities) return 0;
	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || pEdict->free || !pEdict->pvPrivateData) return 0;
	CBasePlayerWeapon *pWeapon = GET_PRIVATE<CBasePlayerWeapon>(pEdict);
	if (!pWeapon)
		return 0;
	return pWeapon->m_iClip;
}

// cs_set_weapon_ammo(entity, amount)
// 直接设置玩家当前持有武器的弹匣子弹数
cell AMX_NATIVE_CALL cs_set_weapon_ammo(AMX *amx, cell *params)
{
	int index = params[1];
	int amount = params[2];

	if (index < 1 || index > gpGlobals->maxClients)
		return 0;

	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;

	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;

	if (pPlayer->m_pActiveItem) {
		CBasePlayerWeapon *pWeapon = static_cast<CBasePlayerWeapon *>(pPlayer->m_pActiveItem);
		pWeapon->m_iClip = amount;
		return 1;
	}
	return 0;
}

// cs_get_weapon_clip(entity) - 获取武器弹匣子弹数
cell AMX_NATIVE_CALL cs_get_weapon_clip(AMX *amx, cell *params)
{
	int entity = params[1];
	(void)amx;
	if (entity < 1 || entity > gpGlobals->maxEntities)
		return 0;
	edict_t *pEdict = INDEXENT(entity);
	if (!pEdict || pEdict->free || !pEdict->pvPrivateData)
		return 0;
	CBasePlayerWeapon *pWeapon = GET_PRIVATE<CBasePlayerWeapon>(pEdict);
	if (!pWeapon)
		return 0;
	return pWeapon->m_iClip;
}

// cs_set_weapon_clip(entity, amount) - 设置武器弹匣子弹数
cell AMX_NATIVE_CALL cs_set_weapon_clip(AMX *amx, cell *params)
{
	int entity = params[1];
	int amount = params[2];
	(void)amx;
	if (entity < 1 || entity > gpGlobals->maxEntities)
		return 0;
	edict_t *pEdict = INDEXENT(entity);
	if (!pEdict || pEdict->free || !pEdict->pvPrivateData)
		return 0;
	CBasePlayerWeapon *pWeapon = GET_PRIVATE<CBasePlayerWeapon>(pEdict);
	if (!pWeapon)
		return 0;
	pWeapon->m_iClip = amount;
	return 1;
}

// cs_get_user_weap_ammo(index, slot) - 获取玩家武器槽的 ammo1
cell AMX_NATIVE_CALL cs_get_user_weap_ammo(AMX *amx, cell *params)
{
	int index = params[1];
	int slot = params[2];
	(void)amx;
	if (index < 1 || index > gpGlobals->maxClients)
		return 0;
	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;
	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;
	CBasePlayerItem *pItem = pPlayer->m_rgpPlayerItems[slot];
	if (!pItem)
		return 0;
	CBasePlayerWeapon *pWeapon = static_cast<CBasePlayerWeapon *>(pItem);
	if (!pWeapon)
		return 0;
	return pPlayer->m_rgAmmo[pWeapon->m_iPrimaryAmmoType];
}

// ===== P2: CStrike 武器增强 =====

cell AMX_NATIVE_CALL cs_get_weapon_burst(AMX *amx, cell *params)
{
	int index = params[1];
	(void)amx;
	if (index < 1 || index > gpGlobals->maxClients)
		return 0;
	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;
	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer || !pPlayer->m_pActiveItem)
		return 0;
	CBasePlayerWeapon *pWeapon = static_cast<CBasePlayerWeapon *>(pPlayer->m_pActiveItem);
	if ((pWeapon->m_iId == WEAPON_GLOCK18 || pWeapon->m_iId == WEAPON_FAMAS) && pWeapon->m_iShotsFired > 0)
		return 1;
	return 0;
}

cell AMX_NATIVE_CALL cs_set_weapon_burst(AMX *amx, cell *params)
{
	int index = params[1];
	int burst = params[2];
	(void)amx;
	if (index < 1 || index > gpGlobals->maxClients)
		return 0;
	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;
	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer || !pPlayer->m_pActiveItem)
		return 0;
	CBasePlayerWeapon *pWeapon = static_cast<CBasePlayerWeapon *>(pPlayer->m_pActiveItem);
	if (pWeapon->m_iId == WEAPON_GLOCK18 || pWeapon->m_iId == WEAPON_FAMAS) {
		pWeapon->m_iShotsFired = burst ? 3 : 0;
	}
	return 1;
}

cell AMX_NATIVE_CALL cs_get_weapon_silen(AMX *amx, cell *params)
{
	int index = params[1];
	(void)amx;
	if (index < 1 || index > gpGlobals->maxClients)
		return 0;
	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;
	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer || !pPlayer->m_pActiveItem)
		return 0;
	CBasePlayerWeapon *pWeapon = static_cast<CBasePlayerWeapon *>(pPlayer->m_pActiveItem);
	return pWeapon->m_bSecondarySilencerOn ? 1 : 0;
}

cell AMX_NATIVE_CALL cs_set_weapon_silen(AMX *amx, cell *params)
{
	int index = params[1];
	int silencer = params[2];
	(void)amx;
	if (index < 1 || index > gpGlobals->maxClients)
		return 0;
	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;
	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer || !pPlayer->m_pActiveItem)
		return 0;
	CBasePlayerWeapon *pWeapon = static_cast<CBasePlayerWeapon *>(pPlayer->m_pActiveItem);
	if (pWeapon->m_iId == WEAPON_USP || pWeapon->m_iId == WEAPON_M4A1) {
		pWeapon->m_bSecondarySilencerOn = (silencer != 0);
	}
	return 1;
}

static int g_armouryType = 0;

cell AMX_NATIVE_CALL cs_get_armoury_type(AMX *amx, cell *params)
{
	(void)amx; (void)params;
	return g_armouryType;
}

cell AMX_NATIVE_CALL cs_set_armoury_type(AMX *amx, cell *params)
{
	(void)amx;
	g_armouryType = (int)params[1];
	return 1;
}

cell AMX_NATIVE_CALL cs_get_user_mapzones(AMX *amx, cell *params)
{
	int index = (int)params[1];
	(void)amx;
	if (index < 1 || index > gpGlobals->maxClients)
		return 0;
	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;
	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;
	// m_signals.GetState() 返回玩家当前所在的区域标志位:
	// SIGNAL_BUY=BIT(0), SIGNAL_BOMB=BIT(1), SIGNAL_RESCUE=BIT(2),
	// SIGNAL_ESCAPE=BIT(3), SIGNAL_VIPSAFETY=BIT(4)
	// 与 AMXX cs_get_user_mapzones 返回值位定义完全一致
	return pPlayer->m_signals.GetState();
}

cell AMX_NATIVE_CALL cs_get_user_buyzone(AMX *amx, cell *params)
{
	int index = (int)params[1];
	(void)amx;
	if (index < 1 || index > gpGlobals->maxClients)
		return 0;
	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;
	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;
	return (pPlayer->m_signals.GetState() & SIGNAL_BUY) ? 1 : 0;
}

// Hostage 跟随状态管理（全局映射：entindex -> 跟随者 ID）
static std::map<int, int> g_hostageFollow;
static std::map<int, int> g_hostageNextId;
// P1-6: Hostage lastuse/nextuse + C4 状态
static std::map<int, float> g_hostageLastUse;
static std::map<int, float> g_hostageNextUse;
static std::map<int, float> g_c4ExplodeTime;
static std::map<int, bool>  g_c4Defusing;

void RegisterCstrikeNatives(AMX *amx)
{
    amx_Register(amx, cstrike_natives, -1);
}

// 跨地图清理 CStrike 全局状态
void ResetCstrikeGlobals()
{
    g_hostageFollow.clear();
    g_hostageNextId.clear();
    g_hostageLastUse.clear();
    g_hostageNextUse.clear();
    g_c4ExplodeTime.clear();
    g_c4Defusing.clear();
}

// ===== P3: CStrike 扩展 =====

cell AMX_NATIVE_CALL cs_get_user_team_id(AMX *amx, cell *params)
{
	int index = params[1];
	(void)amx;
	if (index < 1 || index > gpGlobals->maxClients)
		return CS_TEAM_UNASSIGNED;
	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return CS_TEAM_UNASSIGNED;
	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return CS_TEAM_UNASSIGNED;
	// m_iTeam 在 CS 中是 TeamName 枚举: UNASSIGNED=0, TERRORIST=1, CT=2, SPECTATOR=3
	return static_cast<cell>(pPlayer->m_iTeam);
}

cell AMX_NATIVE_CALL cs_get_user_stationary(AMX *amx, cell *params)
{
	int index = params[1];
	(void)amx;
	if (index < 1 || index > gpGlobals->maxClients)
		return 0;
	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;
	// stationary 检测：velocity 接近 0 且没有按键输入
	if (pEdict->v.velocity.Length() < 1.0f)
		return 1;
	return 0;
}

cell AMX_NATIVE_CALL cs_get_hostage_follow(AMX *amx, cell *params)
{
	int index = params[1];
	(void)amx;
	auto it = g_hostageFollow.find(index);
	if (it != g_hostageFollow.end())
		return it->second;
	return 0;
}

cell AMX_NATIVE_CALL cs_set_hostage_follow(AMX *amx, cell *params)
{
	int index = params[1];
	int followerId = params[2];
	(void)amx;
	g_hostageFollow[index] = followerId;
	return 1;
}

cell AMX_NATIVE_CALL cs_get_hostage_id(AMX *amx, cell *params)
{
	int index = params[1];
	(void)amx;
	auto it = g_hostageNextId.find(index);
	if (it != g_hostageNextId.end())
		return it->second;
	// 如果未设置，尝试查找该玩家正在 interact 的人质
	if (index >= 1 && index <= gpGlobals->maxClients) {
		edict_t *pEdict = INDEXENT(index);
		if (pEdict && pEdict->pvPrivateData) {
			CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
			if (pPlayer) {
				// 遍历实体查找 hostage_entity
				for (int i = 1; i <= gpGlobals->maxEntities; i++) {
					edict_t *pEnt = INDEXENT(i);
					if (!pEnt || pEnt->free) continue;
					const char *classname = STRING(pEnt->v.classname);
					if (classname && strcmp(classname, "hostage_entity") == 0) {
						// euser4 是 int (entity index)
						if (pEnt->v.euser4 && ENTINDEX(pEnt->v.euser4) == index) {
							g_hostageNextId[index] = i;
							return i;
						}
					}
				}
			}
		}
	}
	return 0;
}

// ===== P1-6: 人质 lastuse/nextuse + C4 explode/defusing =====
// cs_get_hostage_lastuse(index)
cell AMX_NATIVE_CALL cs_get_hostage_lastuse(AMX *amx, cell *params)
{
	(void)amx;
	int index = params[1];
	auto it = g_hostageLastUse.find(index);
	if (it != g_hostageLastUse.end())
		return amx_ftoc(it->second);
	return 0;
}

// cs_set_hostage_lastuse(index, Float:value)
cell AMX_NATIVE_CALL cs_set_hostage_lastuse(AMX *amx, cell *params)
{
	(void)amx;
	int index = params[1];
	float value = amx_ctof(params[2]);
	g_hostageLastUse[index] = value;
	return 1;
}

// cs_get_hostage_nextuse(index)
cell AMX_NATIVE_CALL cs_get_hostage_nextuse(AMX *amx, cell *params)
{
	(void)amx;
	int index = params[1];
	auto it = g_hostageNextUse.find(index);
	if (it != g_hostageNextUse.end())
		return amx_ftoc(it->second);
	return 0;
}

// cs_set_hostage_nextuse(index, Float:value)
cell AMX_NATIVE_CALL cs_set_hostage_nextuse(AMX *amx, cell *params)
{
	(void)amx;
	int index = params[1];
	float value = amx_ctof(params[2]);
	g_hostageNextUse[index] = value;
	return 1;
}

// cs_get_c4_explode_time(index) — index = C4 实体的 edict index
cell AMX_NATIVE_CALL cs_get_c4_explode_time(AMX *amx, cell *params)
{
	(void)amx;
	int index = params[1];
	auto it = g_c4ExplodeTime.find(index);
	if (it != g_c4ExplodeTime.end())
		return amx_ftoc(it->second);
	// fallback: 如果未显式设置，尝试从实体 pev->dmgtime 获取
	if (index >= 0 && index <= gpGlobals->maxEntities) {
		edict_t *pEnt = INDEXENT(index);
		if (pEnt && !pEnt->free)
			return amx_ftoc(pEnt->v.dmgtime);
	}
	return 0;
}

// cs_set_c4_explode_time(index, Float:value)
cell AMX_NATIVE_CALL cs_set_c4_explode_time(AMX *amx, cell *params)
{
	(void)amx;
	int index = params[1];
	float value = amx_ctof(params[2]);
	g_c4ExplodeTime[index] = value;
	// 同步到实体 pev->dmgtime，使游戏逻辑也能用到
	if (index >= 0 && index <= gpGlobals->maxEntities) {
		edict_t *pEnt = INDEXENT(index);
		if (pEnt && !pEnt->free)
			pEnt->v.dmgtime = value;
	}
	return 1;
}

// cs_get_c4_defusing(c4index)
cell AMX_NATIVE_CALL cs_get_c4_defusing(AMX *amx, cell *params)
{
	(void)amx;
	int index = params[1];
	auto it = g_c4Defusing.find(index);
	if (it != g_c4Defusing.end())
		return it->second ? 1 : 0;
	return 0;
}

// cs_set_c4_defusing(c4index, bool:defusing)
cell AMX_NATIVE_CALL cs_set_c4_defusing(AMX *amx, cell *params)
{
	(void)amx;
	int index = params[1];
	bool defusing = (params[2] != 0);
	g_c4Defusing[index] = defusing;
	return 1;
}

// 盾牌支持（CS 1.6）
cell AMX_NATIVE_CALL cs_get_user_shield(AMX *amx, cell *params)
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
	// 检测是否持有盾牌 (weapon_shield)
	return pPlayer->HasNamedPlayerItem("weapon_shield") ? 1 : 0;
}

cell AMX_NATIVE_CALL cs_set_user_shield(AMX *amx, cell *params)
{
	int index = params[1];
	int shield = params[2];
	(void)amx;
	if (index < 1 || index > gpGlobals->maxClients)
		return 0;
	edict_t *pEdict = INDEXENT(index);
	if (!pEdict || !pEdict->pvPrivateData)
		return 0;
	CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
	if (!pPlayer)
		return 0;
	if (shield) {
		if (!pPlayer->HasNamedPlayerItem("weapon_shield"))
			pPlayer->GiveNamedItem("weapon_shield");
	} else {
		// 移除盾牌
		for (int slot = 0; slot < MAX_ITEM_TYPES; slot++) {
			CBasePlayerItem *pItem = pPlayer->m_rgpPlayerItems[slot];
			while (pItem) {
				CBasePlayerItem *nextItem = pItem->m_pNext;
				if (pItem->m_iId == WEAPON_SHIELDGUN) { // WEAPON_SHIELDGUN = 99
					pPlayer->RemovePlayerItem(pItem);
					break;
				}
				pItem = nextItem;
			}
		}
	}
	return 1;
}