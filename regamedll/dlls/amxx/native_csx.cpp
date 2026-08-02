#include "precompiled.h"

#include "native_csx.h"
#include "amx.h"
#include "runtime.h"
#include "forwards.h"

/* ReGameDLL includes */
#include "../extdll.h"
#include "../player.h"
#include "../cbase.h"
#include "../enginecallback.h"

#include <cstring>

// ===== Map objective bitflags (returned by get_map_objectives) =====
#define MAP_OBJECTIVE_BOMB    1
#define MAP_OBJECTIVE_HOSTAGE 2
#define MAP_OBJECTIVE_VIP     4
#define MAP_OBJECTIVE_ESCAPE  8

// CS weapon ID constants
#define CSW_KNIFE         28
#define CSW_P90           30
#define CSW_MAX_STANDARD  30

// Maximum custom weapons and starting weapon ID
#define MAX_CUSTOM_WEAPONS   16
#define CUSTOM_WEAPON_START  31

// Stats size: shots, hits, kills, deaths, headshots, damage
#define XMOD_STATS_SIZE 6

// ===== Data structures =====

struct CustomWeapon {
    char name[32];
    char logname[32];
    bool melee;
    bool valid;
};

struct WeaponInfoEntry {
    const char *name;
    const char *logname;
    bool melee;
};

struct WeaponStats {
    int shots;
    int hits;
    int kills;
    int deaths;
    int headshots;
    int damage;
};

// ===== Static data =====

// Standard CS weapons (1-based IDs: CSW_P228=1 ... CSW_KNIFE=28, CSW_P90=30)
// Array index = weaponId - 1, except CSW_P90 (ID 30) is at index 28.
// There is no weapon with ID 29.
static WeaponInfoEntry g_csWeapons[] = {
    {"P228",           "p228",      false}, // CSW_P228        = 1
    {"SCOUT",          "scout",     false}, // CSW_SCOUT       = 2
    {"HE Grenade",     "grenade",   false}, // CSW_HEGRENADE   = 3
    {"XM1014",         "xm1014",    false}, // CSW_XM1014      = 4
    {"C4",             "c4",        false}, // CSW_C4          = 5
    {"MAC10",          "mac10",     false}, // CSW_MAC10       = 6
    {"AUG",            "aug",       false}, // CSW_AUG         = 7
    {"Smoke Grenade",  "sgren",     false}, // CSW_SMOKEGRENADE= 8
    {"Elite",          "elite",     false}, // CSW_ELITE       = 9
    {"Fiveseven",      "fiveseven", false}, // CSW_FIVESEVEN   = 10
    {"UMP45",          "ump45",     false}, // CSW_UMP45       = 11
    {"SG550",          "sg550",     false}, // CSW_SG550       = 12
    {"Galil",          "galil",     false}, // CSW_GALIL       = 13
    {"Famas",          "famas",     false}, // CSW_FAMAS       = 14
    {"USP",            "usp",       false}, // CSW_USP         = 15
    {"Glock18",        "glock18",   false}, // CSW_GLOCK18     = 16
    {"AWP",            "awp",       false}, // CSW_AWP         = 17
    {"MP5 Navy",       "mp5navy",   false}, // CSW_MP5NAVY     = 18
    {"M249",           "m249",      false}, // CSW_M249        = 19
    {"M3 Super 90",    "m3",        false}, // CSW_M3          = 20
    {"M4A1",           "m4a1",      false}, // CSW_M4A1        = 21
    {"TMP",            "tmp",       false}, // CSW_TMP         = 22
    {"G3SG1",          "g3sg1",     false}, // CSW_G3SG1       = 23
    {"Flashbang",      "flashbang", false}, // CSW_FLASHBANG   = 24
    {"Desert Eagle",   "deagle",    false}, // CSW_DEAGLE      = 25
    {"SG552",          "sg552",     false}, // CSW_SG552       = 26
    {"AK47",           "ak47",      false}, // CSW_AK47        = 27
    {"Knife",          "knife",     true},  // CSW_KNIFE       = 28
    {"P90",            "p90",       false}, // CSW_P90         = 30
};

static CustomWeapon g_customWeapons[MAX_CUSTOM_WEAPONS];
static int g_numCustomWeapons = 0;

// Weapon stats storage (indexed by weapon ID; max ID = 30 + 16 = 46)
static WeaponStats g_weaponStats[64];

// ===== Helper functions =====

// Map a standard CSW weapon ID to g_csWeapons array index.
// Returns -1 for out-of-range or the gap at ID 29.
static int CsWeaponToArrayIndex(int wpnindex)
{
    if (wpnindex < 1 || wpnindex > CSW_MAX_STANDARD)
        return -1;
    if (wpnindex == 29)
        return -1; // No weapon with ID 29
    if (wpnindex == CSW_P90)
        return 28; // P90 is the 29th entry (index 28)
    return wpnindex - 1;
}

// Map a custom weapon ID to g_customWeapons array index.
static int CustomWeaponToArrayIndex(int wpnindex)
{
    if (wpnindex < CUSTOM_WEAPON_START)
        return -1;
    int idx = wpnindex - CUSTOM_WEAPON_START;
    if (idx >= g_numCustomWeapons)
        return -1;
    return idx;
}

// Check if any entity with the given classname exists in the map.
static bool EntityClassnameExists(const char *classname)
{
    edict_t *pEnt = FIND_ENTITY_BY_STRING(nullptr, "classname", classname);
    if (!pEnt || pEnt->free)
        return false;
    // Verify the match (some engines return a non-null sentinel on miss)
    const char *cls = STRING(pEnt->v.classname);
    return cls && strcmp(cls, classname) == 0;
}

// ===== Native implementations =====

// custom_weapon_add(const wpnname[], melee = 0, const logname[] = "")
cell AMX_NATIVE_CALL amxx_custom_weapon_add(AMX *amx, cell *params)
{
    if (g_numCustomWeapons >= MAX_CUSTOM_WEAPONS)
        return 0;

    cell *nameAddr;
    amx_GetAddr(amx, params[1], &nameAddr);
    if (!nameAddr)
        return 0;

    char wpnname[32];
    amx_GetString(wpnname, nameAddr, 0, sizeof(wpnname));

    int melee = params[2];

    char logname[32];
    logname[0] = '\0';
    int argCount = params[0] / (int)sizeof(cell);
    if (argCount >= 3) {
        cell *logAddr;
        amx_GetAddr(amx, params[3], &logAddr);
        if (logAddr)
            amx_GetString(logname, logAddr, 0, sizeof(logname));
    }

    // If logname is empty, fall back to wpnname
    if (logname[0] == '\0') {
        strncpy(logname, wpnname, sizeof(logname) - 1);
        logname[sizeof(logname) - 1] = '\0';
    }

    int idx = g_numCustomWeapons;
    strncpy(g_customWeapons[idx].name, wpnname, sizeof(g_customWeapons[idx].name) - 1);
    g_customWeapons[idx].name[sizeof(g_customWeapons[idx].name) - 1] = '\0';
    strncpy(g_customWeapons[idx].logname, logname, sizeof(g_customWeapons[idx].logname) - 1);
    g_customWeapons[idx].logname[sizeof(g_customWeapons[idx].logname) - 1] = '\0';
    g_customWeapons[idx].melee = (melee != 0);
    g_customWeapons[idx].valid = true;

    g_numCustomWeapons++;

    return CUSTOM_WEAPON_START + idx;
}

// custom_weapon_dmg(weapon, att, vic, damage, hitplace = 0)
cell AMX_NATIVE_CALL amxx_custom_weapon_dmg(AMX *amx, cell *params)
{
    int weapon   = params[1];
    int att      = params[2];
    int vic      = params[3];
    int damage   = params[4];
    int hitplace = (params[0] / (int)sizeof(cell) >= 5) ? params[5] : 0;

    // Store damage stats
    if (weapon >= 1 && weapon < (int)(sizeof(g_weaponStats) / sizeof(g_weaponStats[0]))) {
        g_weaponStats[weapon].damage += damage;
        g_weaponStats[weapon].hits++;
        if (hitplace == 1) // HIT_HEAD
            g_weaponStats[weapon].headshots++;
    }

    // Compute TK (team kill/attack): same team, different players
    int isTK = 0;
    if (att >= 1 && att <= gpGlobals->maxClients &&
        vic >= 1 && vic <= gpGlobals->maxClients && att != vic) {
        edict_t *pAttEdict = INDEXENT(att);
        edict_t *pVicEdict = INDEXENT(vic);
        if (pAttEdict && pAttEdict->pvPrivateData &&
            pVicEdict && pVicEdict->pvPrivateData) {
            CBasePlayer *pAttacker = GET_PRIVATE<CBasePlayer>(pAttEdict);
            CBasePlayer *pVictim   = GET_PRIVATE<CBasePlayer>(pVicEdict);
            if (pAttacker && pVictim) {
                if ((int)pAttacker->m_iTeam != 0 &&
                    pAttacker->m_iTeam == pVictim->m_iTeam)
                    isTK = 1;
            }
        }
    }

    // Trigger client_damage forward:
    // client_damage(attacker, victim, damage, weapon, hitplace, TK)
    ForwardCallParam fwdParams[6];
    fwdParams[0].type = FP_CELL;  fwdParams[0].val = att;            fwdParams[0].size = 0;
    fwdParams[1].type = FP_CELL;  fwdParams[1].val = vic;            fwdParams[1].size = 0;
    fwdParams[2].type = FP_FLOAT; fwdParams[2].fval = (float)damage; fwdParams[2].size = 0;
    fwdParams[3].type = FP_CELL;  fwdParams[3].val = weapon;         fwdParams[3].size = 0;
    fwdParams[4].type = FP_CELL;  fwdParams[4].val = hitplace;       fwdParams[4].size = 0;
    fwdParams[5].type = FP_CELL;  fwdParams[5].val = isTK;           fwdParams[5].size = 0;
    AMXXRuntime::GetInstance().ExecuteForwardEx("client_damage", 6, fwdParams);

    return 1;
}

// custom_weapon_shot(weapon, index)
cell AMX_NATIVE_CALL amxx_custom_weapon_shot(AMX *amx, cell *params)
{
    (void)amx;
    int weapon = params[1];
    // params[2] is the player index; not needed for aggregate shot counting
    (void)params[2];

    if (weapon >= 1 && weapon < (int)(sizeof(g_weaponStats) / sizeof(g_weaponStats[0]))) {
        g_weaponStats[weapon].shots++;
    }
    return 1;
}

// xmod_is_melee_wpn(wpnindex)
cell AMX_NATIVE_CALL amxx_xmod_is_melee_wpn(AMX *amx, cell *params)
{
    (void)amx;
    int wpnindex = params[1];

    int arrIdx = CsWeaponToArrayIndex(wpnindex);
    if (arrIdx >= 0)
        return g_csWeapons[arrIdx].melee ? 1 : 0;

    int customIdx = CustomWeaponToArrayIndex(wpnindex);
    if (customIdx >= 0)
        return g_customWeapons[customIdx].melee ? 1 : 0;

    return 0;
}

// xmod_get_wpnname(wpnindex, name[], len)
cell AMX_NATIVE_CALL amxx_xmod_get_wpnname(AMX *amx, cell *params)
{
    int wpnindex = params[1];

    const char *name = nullptr;

    int arrIdx = CsWeaponToArrayIndex(wpnindex);
    if (arrIdx >= 0) {
        name = g_csWeapons[arrIdx].name;
    } else {
        int customIdx = CustomWeaponToArrayIndex(wpnindex);
        if (customIdx >= 0 && g_customWeapons[customIdx].valid)
            name = g_customWeapons[customIdx].name;
    }

    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    if (!dest)
        return 0;

    if (!name)
        name = "";

    amx_SetString(dest, name, 0, 0, (int)params[3]);
    return 1;
}

// xmod_get_wpnlogname(wpnindex, name[], len)
cell AMX_NATIVE_CALL amxx_xmod_get_wpnlogname(AMX *amx, cell *params)
{
    int wpnindex = params[1];

    const char *logname = nullptr;

    int arrIdx = CsWeaponToArrayIndex(wpnindex);
    if (arrIdx >= 0) {
        logname = g_csWeapons[arrIdx].logname;
    } else {
        int customIdx = CustomWeaponToArrayIndex(wpnindex);
        if (customIdx >= 0 && g_customWeapons[customIdx].valid)
            logname = g_customWeapons[customIdx].logname;
    }

    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    if (!dest)
        return 0;

    if (!logname)
        logname = "";

    amx_SetString(dest, logname, 0, 0, (int)params[3]);
    return 1;
}

// xmod_get_maxweapons()
cell AMX_NATIVE_CALL amxx_xmod_get_maxweapons(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return CSW_MAX_STANDARD + g_numCustomWeapons;
}

// xmod_get_stats_size()
cell AMX_NATIVE_CALL amxx_xmod_get_stats_size(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return XMOD_STATS_SIZE;
}

// get_map_objectives()
cell AMX_NATIVE_CALL amxx_get_map_objectives(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    int objectives = 0;

    // Bomb objective: bomb zones
    if (EntityClassnameExists("func_bomb_target") ||
        EntityClassnameExists("info_bomb_target"))
        objectives |= MAP_OBJECTIVE_BOMB;

    // Hostage objective: hostages or rescue zones
    if (EntityClassnameExists("hostage_entity") ||
        EntityClassnameExists("func_hostage_rescue"))
        objectives |= MAP_OBJECTIVE_HOSTAGE;

    // VIP objective: VIP start zone
    if (EntityClassnameExists("func_vip_start") ||
        EntityClassnameExists("info_vip_start"))
        objectives |= MAP_OBJECTIVE_VIP;

    // Escape objective: escape zones
    if (EntityClassnameExists("func_escapezone") ||
        EntityClassnameExists("info_escapezone"))
        objectives |= MAP_OBJECTIVE_ESCAPE;

    return objectives;
}

// ===== CSStats (csstats.inc) — simplified in-memory stats storage =====
//
// Implements the 10 csstats.inc natives. There is no disk persistence: the
// "permanent storage" tier (get_stats/get_statsnum/get_user_stats2/get_stats2)
// is backed by a per-slot lifetime record that is cleared on map change
// (ResetCsxGlobals). Round stats (wrstats/rstats/vstats/astats) are cleared on
// every round restart (CsstatsResetRound, called from Hook_RestartRound).
//
// Stats field layout follows the STATSX_* constants (amxconst.inc):
//   0=Kills 1=Deaths 2=Headshots 3=Teamkills 4=Shots 5=Hits 6=Damage 7=Rank
// bodyhits is indexed by hitgroup (HIT_HEAD=1..HIT_RIGHTLEG=7); index 0 is
// reserved and not copied out (matches original CSX behaviour).
// Objective stats (STATSX_MAX_OBJECTIVE=4):
//   0=total defusions 1=bombs defused 2=bombs planted 3=bomb explosions

#define CSSTATS_MAX_PLAYERS   33   // player slots 1..32
#define CSSTATS_MAX_WEAPONS   64   // weapon id 0..63 (CSW_* + custom)
#define CSSTATS_MAX_BODYHITS   8
#define CSSTATS_MAX_STATS      8
#define CSSTATS_MAX_OBJECTIVE  4

// Stats field indices (match STATSX_*)
#define CSX_KILLS         0
#define CSX_DEATHS        1
#define CSX_HEADSHOTS     2
#define CSX_TEAMKILLS     3
#define CSX_SHOTS         4
#define CSX_HITS          5
#define CSX_DAMAGE        6
#define CSX_RANK          7
// Objective field indices (match STATSX_*)
#define CSX_TOTAL_DEFUSIONS  0
#define CSX_BOMBS_DEFUSED    1
#define CSX_BOMBS_PLANTED    2
#define CSX_BOMB_EXPLOSIONS  3

struct CSStatBlock {
    int kills;
    int deaths;
    int headshots;
    int teamKills;
    int shots;
    int hits;
    int damage;
    int bodyHits[CSSTATS_MAX_BODYHITS];
    int lastWeapon; // last weapon id used in this interaction (for vstats/astats wpnname)
};

// Round stats (reset each round): weapons[0]=total, attackers[0]/victims[0]=aggregate
struct CSPlayerRoundStats {
    CSStatBlock weapons[CSSTATS_MAX_WEAPONS];
    CSStatBlock attackers[CSSTATS_MAX_PLAYERS];
    CSStatBlock victims[CSSTATS_MAX_PLAYERS];
};

// Lifetime stats (reset on map change): weapons[0]=total
struct CSPlayerLifeStats {
    CSStatBlock weapons[CSSTATS_MAX_WEAPONS];
    int objective[CSSTATS_MAX_OBJECTIVE];
    char name[32];
    char authid[32];
    bool valid;
};

static CSPlayerRoundStats g_roundStats[CSSTATS_MAX_PLAYERS];
static CSPlayerLifeStats  g_lifeStats[CSSTATS_MAX_PLAYERS];

// ===== internal helpers =====

static bool CsstatsInPlayerRange(int index)
{
    return index >= 1 && index < CSSTATS_MAX_PLAYERS;
}

static bool CsstatsIsConnected(int index)
{
    if (!CsstatsInPlayerRange(index))
        return false;
    if (index > gpGlobals->maxClients)
        return false;
    edict_t *pEdict = INDEXENT(index);
    return pEdict && !pEdict->free && pEdict->pvPrivateData != nullptr;
}

static int CsstatsClampWeapon(int wpn)
{
    if (wpn < 0 || wpn >= CSSTATS_MAX_WEAPONS)
        return -1;
    return wpn;
}

static const char *CsstatsWeaponName(int wpnId)
{
    int arrIdx = CsWeaponToArrayIndex(wpnId);
    if (arrIdx >= 0)
        return g_csWeapons[arrIdx].name;
    int customIdx = CustomWeaponToArrayIndex(wpnId);
    if (customIdx >= 0 && g_customWeapons[customIdx].valid)
        return g_customWeapons[customIdx].name;
    return "";
}

static void CsstatsUpdateIdentity(int index)
{
    if (!CsstatsInPlayerRange(index))
        return;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free || !pEdict->pvPrivateData)
        return;
    CSPlayerLifeStats &ls = g_lifeStats[index];
    ls.valid = true;
    const char *name = STRING(pEdict->v.netname);
    if (name) {
        strncpy(ls.name, name, sizeof(ls.name) - 1);
        ls.name[sizeof(ls.name) - 1] = '\0';
    }
    const char *authid = GETPLAYERAUTHID(pEdict);
    if (!authid)
        authid = "STEAM_ID_PENDING";
    strncpy(ls.authid, authid, sizeof(ls.authid) - 1);
    ls.authid[sizeof(ls.authid) - 1] = '\0';
}

static bool CsstatsHasAnyStat(const CSStatBlock &s)
{
    return s.shots || s.hits || s.kills || s.deaths || s.damage || s.headshots;
}

static void CsstatsCopyStats(const CSStatBlock &s, cell *cpStats, int rank)
{
    cpStats[CSX_KILLS]     = s.kills;
    cpStats[CSX_DEATHS]    = s.deaths;
    cpStats[CSX_HEADSHOTS] = s.headshots;
    cpStats[CSX_TEAMKILLS] = s.teamKills;
    cpStats[CSX_SHOTS]     = s.shots;
    cpStats[CSX_HITS]      = s.hits;
    cpStats[CSX_DAMAGE]    = s.damage;
    cpStats[CSX_RANK]      = rank;
}

static void CsstatsCopyBodyHits(const CSStatBlock &s, cell *cpBodyHits)
{
    // Original CSX only copies hitgroups 1..7 (skips HIT_GENERIC=0).
    for (int i = 1; i < CSSTATS_MAX_BODYHITS; i++)
        cpBodyHits[i] = s.bodyHits[i];
}

static int CsstatsComputeScore(int index)
{
    const CSStatBlock &t = g_lifeStats[index].weapons[0];
    return t.kills - t.deaths - t.teamKills;
}

struct CSRankEntry {
    int slot;
    int score;
};

// Build a ranking of all valid lifetime records sorted by score desc, slot asc.
// Returns the number of ranked entries; entries written into `out`.
static int CsstatsBuildRanking(CSRankEntry *out)
{
    int n = 0;
    for (int i = 1; i < CSSTATS_MAX_PLAYERS; i++) {
        if (g_lifeStats[i].valid) {
            out[n].slot = i;
            out[n].score = CsstatsComputeScore(i);
            n++;
        }
    }
    for (int a = 0; a < n; a++) {
        for (int b = a + 1; b < n; b++) {
            if (out[b].score > out[a].score ||
                (out[b].score == out[a].score && out[b].slot < out[a].slot)) {
                CSRankEntry tmp = out[a];
                out[a] = out[b];
                out[b] = tmp;
            }
        }
    }
    return n;
}

static int CsstatsGetRank(int index)
{
    if (!g_lifeStats[index].valid)
        return 0;
    CSRankEntry ranking[CSSTATS_MAX_PLAYERS];
    int n = CsstatsBuildRanking(ranking);
    for (int i = 0; i < n; i++) {
        if (ranking[i].slot == index)
            return i + 1;
    }
    return 0;
}

// ===== recording API (called from amxx_hooks.cpp) =====

void CsstatsRecordDamage(int attacker, int victim, int weapon, int damage, int hitgroup)
{
    if (!CsstatsInPlayerRange(attacker) || !CsstatsInPlayerRange(victim))
        return;
    if (attacker == victim)
        return; // self-damage not tracked (matches original saveHit)
    int w = CsstatsClampWeapon(weapon);
    if (w < 0)
        return;
    if (hitgroup < 0 || hitgroup >= CSSTATS_MAX_BODYHITS)
        hitgroup = 0;
    if (damage < 0)
        damage = 0;

    CSPlayerRoundStats &ar = g_roundStats[attacker];
    CSPlayerRoundStats &vr = g_roundStats[victim];
    CSPlayerLifeStats  &al = g_lifeStats[attacker];

    // victim's attackers (damage received from this attacker)
    vr.attackers[attacker].hits++;        vr.attackers[attacker].damage += damage;
    vr.attackers[attacker].bodyHits[hitgroup]++; vr.attackers[attacker].lastWeapon = w;
    vr.attackers[0].hits++;               vr.attackers[0].damage += damage;
    vr.attackers[0].bodyHits[hitgroup]++;

    // attacker's victims (damage dealt to this victim)
    ar.victims[victim].hits++;            ar.victims[victim].damage += damage;
    ar.victims[victim].bodyHits[hitgroup]++; ar.victims[victim].lastWeapon = w;
    ar.victims[0].hits++;                 ar.victims[0].damage += damage;
    ar.victims[0].bodyHits[hitgroup]++;

    // attacker round weapon + round total
    ar.weapons[w].hits++;                 ar.weapons[w].damage += damage;
    ar.weapons[w].bodyHits[hitgroup]++;   ar.weapons[w].lastWeapon = w;
    ar.weapons[0].hits++;                 ar.weapons[0].damage += damage;
    ar.weapons[0].bodyHits[hitgroup]++;

    // attacker lifetime weapon + lifetime total
    al.weapons[w].hits++;                 al.weapons[w].damage += damage;
    al.weapons[w].bodyHits[hitgroup]++;
    al.weapons[0].hits++;                 al.weapons[0].damage += damage;
    al.weapons[0].bodyHits[hitgroup]++;

    CsstatsUpdateIdentity(attacker);
    CsstatsUpdateIdentity(victim);
}

void CsstatsRecordDeath(int killer, int victim, int weapon, bool headshot, bool isTK)
{
    if (!CsstatsInPlayerRange(victim))
        return;
    int w = CsstatsClampWeapon(weapon);
    int hs = headshot ? 1 : 0;
    int tk = isTK ? 1 : 0;

    CSPlayerRoundStats &vr = g_roundStats[victim];
    CSPlayerLifeStats  &vl = g_lifeStats[victim];

    // Suicide / world death: only the victim's death is recorded.
    if (killer < 1 || killer == victim) {
        vr.weapons[0].deaths++;
        vl.weapons[0].deaths++;
        CsstatsUpdateIdentity(victim);
        return;
    }
    if (!CsstatsInPlayerRange(killer) || w < 0)
        return;

    CSPlayerRoundStats &kr = g_roundStats[killer];
    CSPlayerLifeStats  &kl = g_lifeStats[killer];

    // victim's attackers (this killer's kills against the victim)
    vr.attackers[killer].kills++;         vr.attackers[killer].headshots += hs;
    vr.attackers[killer].teamKills += tk; vr.attackers[killer].lastWeapon = w;
    vr.attackers[0].kills++;              vr.attackers[0].headshots += hs;
    vr.attackers[0].teamKills += tk;

    // victim death attributed to the killer's weapon (round + lifetime)
    vr.weapons[w].deaths++;               vr.weapons[w].lastWeapon = w;
    vr.weapons[0].deaths++;
    vl.weapons[w].deaths++;
    vl.weapons[0].deaths++;

    // killer's victims (this victim's deaths to the killer)
    kr.victims[victim].deaths++;          kr.victims[victim].headshots += hs;
    kr.victims[victim].teamKills += tk;   kr.victims[victim].lastWeapon = w;
    kr.victims[0].deaths++;               kr.victims[0].headshots += hs;
    kr.victims[0].teamKills += tk;

    // killer round weapon + round total
    kr.weapons[w].kills++;                kr.weapons[w].headshots += hs;
    kr.weapons[w].teamKills += tk;        kr.weapons[w].lastWeapon = w;
    kr.weapons[0].kills++;                kr.weapons[0].headshots += hs;
    kr.weapons[0].teamKills += tk;

    // killer lifetime weapon + lifetime total
    kl.weapons[w].kills++;                kl.weapons[w].headshots += hs;
    kl.weapons[w].teamKills += tk;
    kl.weapons[0].kills++;                kl.weapons[0].headshots += hs;
    kl.weapons[0].teamKills += tk;

    CsstatsUpdateIdentity(killer);
    CsstatsUpdateIdentity(victim);
}

void CsstatsRecordShot(int attacker, int weapon)
{
    if (!CsstatsInPlayerRange(attacker))
        return;
    int w = CsstatsClampWeapon(weapon);
    if (w < 0)
        return;
    CSPlayerRoundStats &ar = g_roundStats[attacker];
    CSPlayerLifeStats  &al = g_lifeStats[attacker];
    ar.victims[0].shots++;
    ar.weapons[w].shots++;
    ar.weapons[0].shots++;
    al.weapons[w].shots++;
    al.weapons[0].shots++;
    CsstatsUpdateIdentity(attacker);
}

void CsstatsRecordBombPlant(int planter)
{
    if (!CsstatsInPlayerRange(planter))
        return;
    g_lifeStats[planter].objective[CSX_BOMBS_PLANTED]++;
    CsstatsUpdateIdentity(planter);
}

void CsstatsRecordBombDefuse(int defuser, bool success)
{
    if (!CsstatsInPlayerRange(defuser))
        return;
    g_lifeStats[defuser].objective[CSX_TOTAL_DEFUSIONS]++;
    if (success)
        g_lifeStats[defuser].objective[CSX_BOMBS_DEFUSED]++;
    CsstatsUpdateIdentity(defuser);
}

void CsstatsRecordBombExplode(int planter)
{
    if (!CsstatsInPlayerRange(planter))
        return;
    g_lifeStats[planter].objective[CSX_BOMB_EXPLOSIONS]++;
    CsstatsUpdateIdentity(planter);
}

void CsstatsResetRound()
{
    memset(g_roundStats, 0, sizeof(g_roundStats));
}

// ===== csstats native implementations =====

// get_user_wstats(index, wpnindex, stats[8], bodyhits[8])
cell AMX_NATIVE_CALL amxx_get_user_wstats(AMX *amx, cell *params)
{
    int index = (int)params[1];
    int weapon = (int)params[2];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    if (weapon < 0 || weapon >= CSSTATS_MAX_WEAPONS)
        return 0;
    if (!CsstatsInPlayerRange(index))
        return 0;

    const CSStatBlock &s = g_lifeStats[index].weapons[weapon];
    if (!CsstatsHasAnyStat(s))
        return 0;

    cell *cpStats, *cpBodyHits;
    amx_GetAddr(amx, params[3], &cpStats);
    amx_GetAddr(amx, params[4], &cpBodyHits);
    if (!cpStats || !cpBodyHits)
        return 0;

    CsstatsCopyStats(s, cpStats, 0);
    CsstatsCopyBodyHits(s, cpBodyHits);
    return 1;
}

// get_user_wrstats(index, wpnindex, stats[8], bodyhits[8])
cell AMX_NATIVE_CALL amxx_get_user_wrstats(AMX *amx, cell *params)
{
    int index = (int)params[1];
    int weapon = (int)params[2];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    if (weapon < 0 || weapon >= CSSTATS_MAX_WEAPONS)
        return 0;
    if (!CsstatsInPlayerRange(index))
        return 0;

    const CSStatBlock &s = g_roundStats[index].weapons[weapon];
    if (!CsstatsHasAnyStat(s))
        return 0;

    cell *cpStats, *cpBodyHits;
    amx_GetAddr(amx, params[3], &cpStats);
    amx_GetAddr(amx, params[4], &cpBodyHits);
    if (!cpStats || !cpBodyHits)
        return 0;

    CsstatsCopyStats(s, cpStats, 0);
    CsstatsCopyBodyHits(s, cpBodyHits);
    return 1;
}

// get_user_rstats(index, stats[8], bodyhits[8])
cell AMX_NATIVE_CALL amxx_get_user_rstats(AMX *amx, cell *params)
{
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    if (!CsstatsInPlayerRange(index))
        return 0;
    // Mirrors original CSX: returns 1 for any ranked/connected player.
    if (!g_lifeStats[index].valid && !CsstatsIsConnected(index))
        return 0;

    const CSStatBlock &s = g_roundStats[index].weapons[0]; // round total
    cell *cpStats, *cpBodyHits;
    amx_GetAddr(amx, params[2], &cpStats);
    amx_GetAddr(amx, params[3], &cpBodyHits);
    if (!cpStats || !cpBodyHits)
        return 0;

    CsstatsCopyStats(s, cpStats, 0);
    CsstatsCopyBodyHits(s, cpBodyHits);
    return 1;
}

// get_user_vstats(index, victim, stats[8], bodyhits[8], wpnname[]="", len=0)
cell AMX_NATIVE_CALL amxx_get_user_vstats(AMX *amx, cell *params)
{
    int index = (int)params[1];
    int victim = (int)params[2];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    if (victim < 0 || victim >= CSSTATS_MAX_PLAYERS)
        return 0;
    if (!CsstatsInPlayerRange(index))
        return 0;

    const CSStatBlock &s = g_roundStats[index].victims[victim];
    if (!CsstatsHasAnyStat(s))
        return 0;

    cell *cpStats, *cpBodyHits;
    amx_GetAddr(amx, params[3], &cpStats);
    amx_GetAddr(amx, params[4], &cpBodyHits);
    if (!cpStats || !cpBodyHits)
        return 0;

    CsstatsCopyStats(s, cpStats, 0);
    CsstatsCopyBodyHits(s, cpBodyHits);

    int argCount = (int)(params[0] / sizeof(cell));
    if (argCount >= 6 && params[6] > 0 && victim != 0) {
        cell *cpName;
        amx_GetAddr(amx, params[5], &cpName);
        if (cpName)
            amx_SetString(cpName, CsstatsWeaponName(s.lastWeapon), 0, 0, (int)params[6]);
    }
    return 1;
}

// get_user_astats(index, attacker, stats[8], bodyhits[8], wpnname[]="", len=0)
cell AMX_NATIVE_CALL amxx_get_user_astats(AMX *amx, cell *params)
{
    int index = (int)params[1];
    int attacker = (int)params[2];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    if (attacker < 0 || attacker >= CSSTATS_MAX_PLAYERS)
        return 0;
    if (!CsstatsInPlayerRange(index))
        return 0;

    const CSStatBlock &s = g_roundStats[index].attackers[attacker];
    if (!CsstatsHasAnyStat(s))
        return 0;

    cell *cpStats, *cpBodyHits;
    amx_GetAddr(amx, params[3], &cpStats);
    amx_GetAddr(amx, params[4], &cpBodyHits);
    if (!cpStats || !cpBodyHits)
        return 0;

    CsstatsCopyStats(s, cpStats, 0);
    CsstatsCopyBodyHits(s, cpBodyHits);

    int argCount = (int)(params[0] / sizeof(cell));
    if (argCount >= 6 && params[6] > 0 && attacker != 0) {
        cell *cpName;
        amx_GetAddr(amx, params[5], &cpName);
        if (cpName)
            amx_SetString(cpName, CsstatsWeaponName(s.lastWeapon), 0, 0, (int)params[6]);
    }
    return 1;
}

// reset_user_wstats(index) — resets current round weapon/attacker/victim stats
cell AMX_NATIVE_CALL amxx_reset_user_wstats(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    if (!CsstatsInPlayerRange(index))
        return 0;
    memset(&g_roundStats[index], 0, sizeof(CSPlayerRoundStats));
    return 1;
}

// get_stats(index, stats[8], bodyhits[8], name[], len, authid[]="", authidlen=0)
cell AMX_NATIVE_CALL amxx_get_stats(AMX *amx, cell *params)
{
    int index = (int)params[1]; // 0-based rank index
    if (index < 0)
        return 0;

    CSRankEntry ranking[CSSTATS_MAX_PLAYERS];
    int n = CsstatsBuildRanking(ranking);
    if (index >= n)
        return 0;

    int slot = ranking[index].slot;
    const CSPlayerLifeStats &ls = g_lifeStats[slot];
    const CSStatBlock &s = ls.weapons[0]; // lifetime total

    cell *cpStats, *cpBodyHits;
    amx_GetAddr(amx, params[2], &cpStats);
    amx_GetAddr(amx, params[3], &cpBodyHits);
    if (!cpStats || !cpBodyHits)
        return 0;

    CsstatsCopyStats(s, cpStats, index + 1);
    CsstatsCopyBodyHits(s, cpBodyHits);

    cell *cpName;
    amx_GetAddr(amx, params[4], &cpName);
    if (cpName)
        amx_SetString(cpName, ls.name, 0, 0, (int)params[5]);

    int argCount = (int)(params[0] / sizeof(cell));
    if (argCount >= 7 && params[7] > 0) {
        cell *cpAuthid;
        amx_GetAddr(amx, params[6], &cpAuthid);
        if (cpAuthid)
            amx_SetString(cpAuthid, ls.authid, 0, 0, (int)params[7]);
    }

    // Original returns the 1-based current position if a next entry exists, else 0.
    return (index + 1 < n) ? (index + 1) : 0;
}

// get_statsnum()
cell AMX_NATIVE_CALL amxx_get_statsnum(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    int count = 0;
    for (int i = 1; i < CSSTATS_MAX_PLAYERS; i++) {
        if (g_lifeStats[i].valid)
            count++;
    }
    return count;
}

// get_user_stats2(index, stats[4])
cell AMX_NATIVE_CALL amxx_get_user_stats2(AMX *amx, cell *params)
{
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    if (!CsstatsInPlayerRange(index))
        return 0;
    if (!g_lifeStats[index].valid)
        return 0;

    int rank = CsstatsGetRank(index);
    if (rank <= 0)
        return 0;

    cell *cpStats;
    amx_GetAddr(amx, params[2], &cpStats);
    if (!cpStats)
        return 0;

    const int *obj = g_lifeStats[index].objective;
    cpStats[CSX_TOTAL_DEFUSIONS] = obj[CSX_TOTAL_DEFUSIONS];
    cpStats[CSX_BOMBS_DEFUSED]   = obj[CSX_BOMBS_DEFUSED];
    cpStats[CSX_BOMBS_PLANTED]   = obj[CSX_BOMBS_PLANTED];
    cpStats[CSX_BOMB_EXPLOSIONS] = obj[CSX_BOMB_EXPLOSIONS];
    return rank;
}

// get_stats2(index, stats[4], authid[]="", authidlen=0)
cell AMX_NATIVE_CALL amxx_get_stats2(AMX *amx, cell *params)
{
    int index = (int)params[1]; // 0-based rank index
    if (index < 0)
        return 0;

    CSRankEntry ranking[CSSTATS_MAX_PLAYERS];
    int n = CsstatsBuildRanking(ranking);
    if (index >= n)
        return 0;

    int slot = ranking[index].slot;
    const CSPlayerLifeStats &ls = g_lifeStats[slot];

    cell *cpStats;
    amx_GetAddr(amx, params[2], &cpStats);
    if (!cpStats)
        return 0;

    const int *obj = ls.objective;
    cpStats[CSX_TOTAL_DEFUSIONS] = obj[CSX_TOTAL_DEFUSIONS];
    cpStats[CSX_BOMBS_DEFUSED]   = obj[CSX_BOMBS_DEFUSED];
    cpStats[CSX_BOMBS_PLANTED]   = obj[CSX_BOMBS_PLANTED];
    cpStats[CSX_BOMB_EXPLOSIONS] = obj[CSX_BOMB_EXPLOSIONS];

    int argCount = (int)(params[0] / sizeof(cell));
    if (argCount >= 4 && params[4] > 0) {
        cell *cpAuthid;
        amx_GetAddr(amx, params[3], &cpAuthid);
        if (cpAuthid)
            amx_SetString(cpAuthid, ls.authid, 0, 0, (int)params[4]);
    }

    return (index + 1 < n) ? (index + 1) : 0;
}

// ===== Registration =====

AMX_NATIVE_INFO csx_natives[] = {
    {"custom_weapon_add",    amxx_custom_weapon_add},
    {"custom_weapon_dmg",    amxx_custom_weapon_dmg},
    {"custom_weapon_shot",   amxx_custom_weapon_shot},
    {"xmod_is_melee_wpn",    amxx_xmod_is_melee_wpn},
    {"xmod_get_wpnname",     amxx_xmod_get_wpnname},
    {"xmod_get_wpnlogname",  amxx_xmod_get_wpnlogname},
    {"xmod_get_maxweapons",  amxx_xmod_get_maxweapons},
    {"xmod_get_stats_size",  amxx_xmod_get_stats_size},
    {"get_map_objectives",   amxx_get_map_objectives},

    // csstats.inc natives
    {"get_user_wstats",      amxx_get_user_wstats},
    {"get_user_wrstats",     amxx_get_user_wrstats},
    {"get_user_rstats",      amxx_get_user_rstats},
    {"get_user_vstats",      amxx_get_user_vstats},
    {"get_user_astats",      amxx_get_user_astats},
    {"reset_user_wstats",    amxx_reset_user_wstats},
    {"get_stats",            amxx_get_stats},
    {"get_statsnum",         amxx_get_statsnum},
    {"get_user_stats2",      amxx_get_user_stats2},
    {"get_stats2",           amxx_get_stats2},
    {nullptr, nullptr}
};

void RegisterCsxNatives(AMX *amx)
{
    amx_Register(amx, csx_natives, -1);
}

void ResetCsxGlobals()
{
    memset(g_customWeapons, 0, sizeof(g_customWeapons));
    g_numCustomWeapons = 0;
    memset(g_weaponStats, 0, sizeof(g_weaponStats));
    // csstats: clear both round and lifetime (permanent) storage on map change.
    memset(g_roundStats, 0, sizeof(g_roundStats));
    memset(g_lifeStats, 0, sizeof(g_lifeStats));
}
