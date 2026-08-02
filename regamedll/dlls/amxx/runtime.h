#pragma once
#ifndef AMXX_RUNTIME_H
#define AMXX_RUNTIME_H

#include "plugin.h"
#include "forwards.h"
#include "taskmgr.h"
#include <vector>
#include <string>

// 前向声明 — 避免在头文件中引入完整游戏头文件
class CBasePlayer;
struct edict_s;

struct PlayerDamageRecord
{
    int lastAttacker;
    float lastDamage;
    int lastWeapon;
    bool wasHeadshot;

    void Reset()
    {
        lastAttacker = 0;
        lastDamage = 0.0f;
        lastWeapon = 0;
        wasHeadshot = false;
    }
};

// RequestFrame 回调: 下一帧执行一次的回调 (对齐原版 amxmodx RequestFrame)
struct AMXXFrameCallback
{
    AMX *amx;
    int funcIndex;
    cell data;
};

class AMXXRuntime
{
public:
    static AMXXRuntime &GetInstance();

    void Init();
    void Shutdown();
    void Frame();
    void OnServerActivate();
    void OnServerDeactivate();
    void OnServerDeactivatePost();

    // 调用时机: CHANGE_LEVEL(mapName) 执行之前 (原版 server_changelevel)
    // 注意: 此处以 OnChangeLevel 显式入口为准，不依赖 metamod
    void OnChangeLevel(const char *mapName);

    // Grenade (CS-specific): 在投掷武器被玩家丢出后调用
    // weaponId = CSW_* weapon index, greId = grenade entity index (edict index - 1)
    void OnGrenadeThrow(CBasePlayer *player, int weaponId, edict_s *greEnt);

    bool LoadPlugin(const char *filename);
    void UnloadAllPlugins();

    void RegisterNatives(AMX *amx);

    AMXXForward *GetForward(const char *name);
    AMXXForward *CreateForward(const char *name);
    AMXXForward *CreateForwardEx(const char *name, ForwardExecType execType);

    void ExecuteForward(const char *name, int paramCount = 0, ...);
    int ExecuteForwardEx(const char *name, int paramCount, ForwardCallParam *params);

    AMXXTaskManager &GetTaskManager() { return m_taskManager; }

    // RequestFrame: 在下一帧调用指定 public 函数 (对齐原版 amxmodx RequestFrame)
    void RequestFrame(AMX *amx, int funcIndex, cell data);

    const std::vector<AMXXPlugin *> &GetPlugins() const { return m_plugins; }

    // 从 AMX* 查找对应的 AMXXPlugin*
    AMXXPlugin *FindPluginByAMX(AMX *amx);

    PlayerDamageRecord &GetDamageRecord(int playerId);
    void ResetDamageRecords();
    void SetLastDamage(int playerId, int attacker, int damage, int weapon);
    int GetLastAttacker(int playerId);
    int GetLastWeapon(int playerId);

private:
    AMXXRuntime();
    ~AMXXRuntime();

    void ScanPlugins(const char *directory);
    void LoadPluginsFromFile(const char *configFile);
    void SetLocalInfoPaths();

    bool m_initialized;
    bool m_pluginNativesCalled;  // P2-9: plugin_natives 仅在首次 OnServerActivate 触发
    std::vector<AMXXPlugin *> m_plugins;
    std::vector<AMXXForward *> m_forwards;
    AMXXTaskManager m_taskManager;
    std::vector<PlayerDamageRecord> m_damageRecords;
    std::vector<AMXXFrameCallback> m_frameCallbacks;
};

#endif
