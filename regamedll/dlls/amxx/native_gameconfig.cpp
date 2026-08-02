#include "precompiled.h"
#include "native_gameconfig.h"
#include "amx.h"
#include "native_fakemeta.h"
#include <string>
#include <vector>

// ===========================================
// GameConfig Module - 嵌入版实现
// ===========================================
// 内嵌版直接使用 native_fakemeta.cpp 中的内置 gamedata 表
// (kGamedataTable), 不读取外部 .txt 文件.
// LoadGameConfigFile 仅创建一个轻量句柄, 标识一次"逻辑加载".

struct GameConfigHandle {
    std::string name;
};

static std::vector<GameConfigHandle*> g_gameConfigs;

// 句柄为 1-based 索引, 0 = Invalid_GameConfig
static int AllocGameConfig(const char *name)
{
    GameConfigHandle *h = new GameConfigHandle();
    if (name) h->name = name;
    g_gameConfigs.push_back(h);
    return (int)g_gameConfigs.size();
}

static GameConfigHandle* GetGameConfig(int handle)
{
    if (handle <= 0 || handle > (int)g_gameConfigs.size())
        return nullptr;
    return g_gameConfigs[handle - 1];
}

static void FreeGameConfig(int handle)
{
    if (handle <= 0 || handle > (int)g_gameConfigs.size())
        return;
    if (g_gameConfigs[handle - 1]) {
        delete g_gameConfigs[handle - 1];
        g_gameConfigs[handle - 1] = nullptr;
    }
}

void ResetGameConfigGlobals()
{
    for (size_t i = 0; i < g_gameConfigs.size(); i++) {
        if (g_gameConfigs[i]) delete g_gameConfigs[i];
    }
    g_gameConfigs.clear();
}

// ============================================
// GameConfig_* Native Implementations
// ============================================

// LoadGameConfigFile(const file[])
// 嵌入版: 内置 gamedata 表已可用, 仅创建句柄返回成功.
// 返回 handle (>=1 成功, 0 失败).
cell AMX_NATIVE_CALL amxx_load_gameconfig_file(AMX *amx, cell *params)
{
    char file[256] = {0};
    cell *addr;
    amx_GetAddr(amx, params[1], &addr);
    if (addr) amx_GetString(file, addr, 0, sizeof(file));

    return (cell)AllocGameConfig(file);
}

// GameConfGetOffset(GameConfig:gc, const key[])
// 从内置 gamedata 表按 member name 查找 offset.
// 返回 offset 值, 失败返回 -1.
cell AMX_NATIVE_CALL amxx_gameconf_get_offset(AMX *amx, cell *params)
{
    GameConfigHandle *gc = GetGameConfig((int)params[1]);
    if (!gc) return -1;

    char key[128] = {0};
    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    if (!addr) return -1;
    amx_GetString(key, addr, 0, sizeof(key));

    FMTypeDescription desc;
    if (FMGameConfig_GetOffsetByMember(key, &desc)) {
        return (cell)desc.fieldOffset;
    }
    return -1;
}

// GameConfGetClassOffset(GameConfig:gc, const classname[], const key[])
// 调用 FMGameConfig_GetOffsetByClass 查找.
// 返回 desc.fieldOffset, 失败返回 -1.
cell AMX_NATIVE_CALL amxx_gameconf_get_class_offset(AMX *amx, cell *params)
{
    GameConfigHandle *gc = GetGameConfig((int)params[1]);
    if (!gc) return -1;

    char classname[128] = {0};
    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    if (!addr) return -1;
    amx_GetString(classname, addr, 0, sizeof(classname));

    char key[128] = {0};
    amx_GetAddr(amx, params[3], &addr);
    if (!addr) return -1;
    amx_GetString(key, addr, 0, sizeof(key));

    FMTypeDescription desc;
    if (FMGameConfig_GetOffsetByClass(classname, key, &desc)) {
        return (cell)desc.fieldOffset;
    }
    return -1;
}

// GameConfGetKeyValue(GameConfig:gc, const key[], buffer[], maxlen)
// 嵌入版无 .txt 键值, 写入空字符串, 返回 1 (true) 表示加载成功.
cell AMX_NATIVE_CALL amxx_gameconf_get_key_value(AMX *amx, cell *params)
{
    GameConfigHandle *gc = GetGameConfig((int)params[1]);
    if (!gc) {
        // 即使句柄无效也清空输出缓冲, 防止插件读到垃圾数据
        cell *dest;
        amx_GetAddr(amx, params[3], &dest);
        if (dest && params[4] > 0) amx_SetString(dest, "", 0, 0, (int)params[4]);
        return 0;
    }

    cell *dest;
    amx_GetAddr(amx, params[3], &dest);
    if (dest && params[4] > 0) amx_SetString(dest, "", 0, 0, (int)params[4]);
    return 1;
}

// GameConfGetAddress(GameConfig:gc, const key[])
// 嵌入版不支持符号地址查找, 返回 0.
cell AMX_NATIVE_CALL amxx_gameconf_get_address(AMX *amx, cell *params)
{
    (void)amx;
    GameConfigHandle *gc = GetGameConfig((int)params[1]);
    if (!gc) return 0;
    return 0;
}

// CloseGameConfigFile(&GameConfig:gc)
// 释放句柄并将引用置零. 返回 1 成功, 0 失败.
cell AMX_NATIVE_CALL amxx_close_gameconfig_file(AMX *amx, cell *params)
{
    cell *addr;
    amx_GetAddr(amx, params[1], &addr);
    if (!addr) return 0;

    int handle = (int)*addr;
    if (handle <= 0) return 0;

    FreeGameConfig(handle);
    *addr = 0;  // 自动置零, 防止误用
    return 1;
}

// ===== Native registration =====

AMX_NATIVE_INFO gameconfig_natives[] = {
    {"LoadGameConfigFile",     amxx_load_gameconfig_file},
    {"GameConfGetOffset",      amxx_gameconf_get_offset},
    {"GameConfGetClassOffset", amxx_gameconf_get_class_offset},
    {"GameConfGetKeyValue",    amxx_gameconf_get_key_value},
    {"GameConfGetAddress",     amxx_gameconf_get_address},
    {"CloseGameConfigFile",    amxx_close_gameconfig_file},
    {nullptr, nullptr}
};

void RegisterGameConfigNatives(AMX *amx)
{
    amx_Register(amx, gameconfig_natives, -1);
}
