#pragma once
#ifndef AMXX_HAMSANDWICH_H
#define AMXX_HAMSANDWICH_H

#include "amx.h"
#include <map>
#include <vector>

// Standard AMXX Ham_* enum values, zero-based (matching ham_const.inc)
// Only lists the types currently bridged; other original values are kept so plugins can pass them
enum HamHookType
{
    Ham_Spawn = 0,
    Ham_Precache,
    Ham_Keyvalue,
    Ham_ObjectCaps,
    Ham_Activate,
    Ham_SetObjectCollisionBox,
    Ham_Classify,
    Ham_DeathNotice,
    Ham_TraceAttack,        // = 8
    Ham_TakeDamage,         // = 9
    Ham_TakeHealth,
    Ham_Killed,             // = 11
    Ham_BloodColor,
    Ham_TraceBleed,
    Ham_IsTriggered,
    Ham_MyMonsterPointer,
    Ham_MySquadMonsterPointer,
    Ham_GetToggleState,
    Ham_AddPoints,
    Ham_AddPointsToTeam,
    Ham_AddPlayerItem,      // = 20
    Ham_RemovePlayerItem,   // = 21
    Ham_GiveAmmo,
    Ham_GetDelay,
    Ham_IsMoving,
    Ham_OverrideReset,
    Ham_DamageDecal,
    Ham_SetToggleState,
    Ham_StartSneaking,
    Ham_StopSneaking,
    Ham_OnControls,
    Ham_IsSneaking,
    Ham_IsAlive,
    Ham_IsBSPModel,
    Ham_ReflectGauss,
    Ham_HasTarget,
    Ham_IsInWorld,
    Ham_IsPlayer,
    Ham_IsNetClient,
    Ham_TeamId,
    Ham_GetNextTarget,
    Ham_Think,              // = 41
    Ham_Touch,              // = 42
    Ham_Use,                // = 43
    Ham_Blocked,            // = 44
    Ham_Respawn,
    Ham_UpdateOwner,
    Ham_FBecomeProne,
    Ham_Center,
    Ham_EyePosition,
    Ham_EarPosition,
    Ham_BodyTarget,
    Ham_Illumination,
    Ham_FVisible,
    Ham_FVecVisible,
    Ham_Player_Jump,        // = 55
    Ham_Player_Duck,        // = 56
    Ham_Player_PreThink,    // = 57
    Ham_Player_PostThink,   // = 58
    // ===== Item/Weapon/CS section: values aligned with the original ham_const.inc =====
    Ham_Item_AddToPlayer = 63,
    Ham_Item_Deploy = 66,
    Ham_Item_Holster = 68,
    Ham_Item_PreFrame = 70,
    Ham_Item_PostFrame = 71,
    Ham_Weapon_PrimaryAttack = 87,
    Ham_Weapon_SecondaryAttack = 88,
    Ham_Weapon_Reload = 89,
    Ham_Weapon_WeaponIdle = 90,
    Ham_CS_RoundRespawn = 98,
    // Compatibility names (not in the original enum; values match the original entries)
    Ham_Weapon_Deploy  = Ham_Item_Deploy,    // Original Ham_Item_Deploy = 66
    Ham_Weapon_Holster = Ham_Item_Holster,   // Original Ham_Item_Holster = 68
    // Embedded extension (not in the original enum; custom values outside the original range)
    Ham_Grenade_Explode = 150,
    Ham_Max = 200
};

// Compatibility aliases: legacy names -> original standard names
#define Ham_PreThink              Ham_Player_PreThink
#define Ham_PostThink             Ham_Player_PostThink
#define Ham_CBasePlayer_Jump      Ham_Player_Jump
#define Ham_CBasePlayer_Duck      Ham_Player_Duck
#define Ham_CBasePlayer_RoundRespawn Ham_CS_RoundRespawn

// Ham forward return codes (matching ham_const.inc)
// Note: original HAM_IGNORED=1 (not 0); returning 0 is also interpreted as HAM_IGNORED
enum HamReturnCode
{
    HAM_IGNORED   = 1, // Call the original, return normal value (applies when no return is given)
    HAM_HANDLED   = 2, // Handled, still call the original
    HAM_OVERRIDE  = 3, // Still call the original, but override the return with the SetHamReturn value
    HAM_SUPERCEDE = 4  // Block the original; use the SetHamReturn value if set
};

struct HamCallback
{
    AMX *amx;
    cell funcidx;
    bool enabled;      // Toggled by HamEnable/HamDisable
    int forwardId;     // Global auto-increment forward handle, used by Enable/Disable/IsValid
    bool post;         // RegisterHam's post parameter (4th arg in the original signature)
    bool specialbot;   // RegisterHam's specialbot parameter (5th arg in the original signature)
};

// ===== Ham return context (shared by bridge functions and SetHamReturn*/GetOrigHamReturn*) =====
// returnType: 1=int 2=float 3=vector 4=entity 5=string
struct HamCtx
{
    int returnType;
    cell intVal;
    float floatVal;
    float vecVal[3];
    int entVal;
    char stringVal[256];
};

extern HamCtx g_hamCtx;      // Override return value set via SetHamReturn* in the current hook callback
extern HamCtx g_hamOrigCtx;  // Actual return value of the original (written after callNext)

// Convert g_hamCtx to a cell return value by returnType (vector/string handled by the caller)
inline cell HamCtxReturnAsCell()
{
    switch (g_hamCtx.returnType)
    {
    case 2: return amx_ftoc(g_hamCtx.floatVal);
    case 4: return (cell)g_hamCtx.entVal;
    default: return g_hamCtx.intVal;
    }
}

// Reset the context so SetHamReturn* values from a previous hook don't leak into this one
inline void HamCtxReset(HamCtx &ctx)
{
    ctx.returnType = 0;
    ctx.intVal = 0;
    ctx.floatVal = 0.0f;
    ctx.vecVal[0] = ctx.vecVal[1] = ctx.vecVal[2] = 0.0f;
    ctx.entVal = 0;
    ctx.stringVal[0] = '\0';
}

// ===== SetHamParam* support =====
// While a Ham callback runs, g_currentHamCtx holds pointers to the current hook's
// modifiable parameters. SetHamParamFloat/Int/Entity/Vector/String modify parameters
// through this context so the subsequent callNext() sees the new values.
struct HamParamSlot
{
    enum Type { PT_NONE, PT_INT, PT_FLOAT, PT_ENTITY, PT_VECTOR, PT_STRING } type;
    void *ptr;  // Points to the underlying C++ variable (int* / float* / int* (entity index) / Vector* / const char**)
};

struct HamDispatchContext
{
    HamHookType hookType;
    int paramCount;
    HamParamSlot params[8];
    bool active;

    void Clear()
    {
        active = false;
        hookType = (HamHookType)0;
        paramCount = 0;
        for (int i = 0; i < 8; i++) { params[i].type = HamParamSlot::PT_NONE; params[i].ptr = nullptr; }
    }
};

extern HamDispatchContext g_currentHamCtx;

class AMXXHamSandwich
{
public:
    static AMXXHamSandwich &GetInstance();

    void Init();
    void Shutdown();

    // Returns a forward handle (>=1), or 0 on failure
    int RegisterHam(HamHookType type, AMX *amx, cell funcidx, bool post = false, bool specialbot = false);
    void UnregisterByAMX(AMX *amx);

    bool EnableForward(int forwardId);
    bool DisableForward(int forwardId);
    bool IsForwardValid(int forwardId) const;

    // IsHamValid: check whether the Ham function number is in the bridged hook dispatch table
    bool IsHamFunctionValid(int func) const;

    // Returns the plugin HAM return code (HAM_SUPERCEDE/HAM_OVERRIDE means the original must be blocked)
    cell DispatchHam(HamHookType type, int paramCount, ...);
    bool DispatchHamBlocking(HamHookType type, int paramCount, ...);

    // Quick check whether any callback is registered for a type (avoids overhead for high-frequency hooks like Touch)
    bool HasCallbacks(HamHookType type) const;

private:
    AMXXHamSandwich() : m_nextForwardId(1) {}

    std::map<HamHookType, std::vector<HamCallback>> m_callbacks;
    int m_nextForwardId;   // Auto-incrementing forward handle
};

#endif
