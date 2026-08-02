#include "precompiled.h"
#include "native_datastructs.h"

/* ReGameDLL includes */
#include "../extdll.h"
#include "../enginecallback.h"
#include "../util.h"

#include "runtime.h"
#include "native_core.h"
#include "native_fun.h"
#include "native_cstrike.h"
#include "native_admin.h"
#include "native_datastructs.h"
#include "native_events.h"
#include "native_entities.h"
#include "native_menus.h"
#include "native_lang.h"
#include "native_log.h"
#include "native_vault.h"
#include "native_messages.h"
#include "native_fakemeta.h"
#include "native_sqlx.h"
#include "native_datapack.h"
#include "native_regex.h"
#include "native_json.h"
#include "native_csx.h"
#include "native_gameconfig.h"
#include "native_sockets.h"
#include "native_dbi.h"
#include "native_geoip.h"
#include "native_textparse.h"
#include "admin.h"
#include "events.h"
#include "menus.h"
#include "lang.h"
#include "amxxlog.h"
#include "vault.h"
#include "messages.h"
#include "amxx_hooks.h"
#include "thinktouch.h"
#include "hamsandwich.h"
#include "amx.h"
#include "../extdll.h"
#include <cstdarg>
#include <cstring>
#include <cstdio>

#ifdef _WIN32
#include <io.h>
#else
#include <dirent.h>
#endif

AMXXRuntime &AMXXRuntime::GetInstance()
{
    static AMXXRuntime instance;
    return instance;
}

AMXXRuntime::AMXXRuntime()
    : m_initialized(false)
    , m_pluginNativesCalled(false)
{
}

AMXXRuntime::~AMXXRuntime()
{
    Shutdown();
}

// ========== "amxx plugins" 服务器命令 ==========
// playerId > 0 表示由玩家触发，输出发往玩家控制台
static void AMXXPluginsCmdImpl(int playerId)
{
    AMXXRuntime &rt = AMXXRuntime::GetInstance();
    const auto &plugins = rt.GetPlugins();

    char buf[2048];
    int pos = 0;

    pos += snprintf(buf + pos, sizeof(buf) - pos,
        "[\n  AMX Mod X 1.8.2 Plugins\n"
        "  %-5s  %-18s  %-12s  %s\n"
        "  %-5s  %-18s  %-12s  %s\n",
        "Status", "Plugin name", "Version", "Author",
        "------", "-----------", "-------", "------");

    for (size_t i = 0; i < plugins.size(); i++) {
        AMXXPlugin *p = plugins[i];
        if (!p || !p->IsLoaded())
            continue;
        const char *status = p->IsLoaded() ? "running" : "stopped";
        const char *title = p->GetTitle() && p->GetTitle()[0] ? p->GetTitle() : p->GetName();
        const char *ver = p->GetVersion() && p->GetVersion()[0] ? p->GetVersion() : "?";
        const char *author = p->GetAuthor() && p->GetAuthor()[0] ? p->GetAuthor() : "?";
        pos += snprintf(buf + pos, sizeof(buf) - pos,
            "  %-5s  %-18s  %-12s  %s\n", status, title, ver, author);
    }

    pos += snprintf(buf + pos, sizeof(buf) - pos,
        "  %zu plugin(s) loaded\n]\n", plugins.size());

    if (playerId > 0) {
        edict_t *pEdict = INDEXENT(playerId);
        if (pEdict && !pEdict->free)
            CLIENT_PRINTF(pEdict, print_console, buf);
    } else {
        SERVER_PRINT(buf);
    }
}

// pfnAddServerCommand 回调（服务端控制台）
static void AMXXPluginsCmd(void)
{
    AMXXPluginsCmdImpl(0);
}

// ========== "amxx" 命令（子命令调度） ==========
static void AMXXServerCmdImpl(int playerId)
{
    const char *subcmd = CMD_ARGV(1);
    if (!subcmd || !*subcmd) {
        const char *usage = "[AMXX] Usage: amxx <command>\n[AMXX] Commands: plugins\n";
        const char *unknown = "[AMXX] Unknown command: ";
        if (playerId > 0) {
            edict_t *pEdict = INDEXENT(playerId);
            if (pEdict && !pEdict->free) {
                CLIENT_PRINTF(pEdict, print_console, usage);
            }
        } else {
            SERVER_PRINT(usage);
        }
        return;
    }

    if (stricmp(subcmd, "plugins") == 0) {
        AMXXPluginsCmdImpl(playerId);
    } else {
        char buf[256];
        snprintf(buf, sizeof(buf), "[AMXX] Unknown command: %s\n", subcmd);
        if (playerId > 0) {
            edict_t *pEdict = INDEXENT(playerId);
            if (pEdict && !pEdict->free)
                CLIENT_PRINTF(pEdict, print_console, buf);
        } else {
            SERVER_PRINT(buf);
        }
    }
}

static void AMXXServerCmd(void)
{
    AMXXServerCmdImpl(0);
}

void AMXXRuntime::Init()
{
    // 构建标记 - 用于验证 DLL 是否被正确加载
    AMXX_LOG_DBG("[Runtime] BUILD_MARKER: 2024-fix-hud-v3");
    AMXX_LOG_DBG("[Runtime] Initializing AMXX runtime...");

    if (m_initialized) {
        AMXX_LOG_DBG("[Runtime] Already initialized");
        return;
    }

    m_initialized = true;

    // 注册 amxmodx_version cvar (服务器浏览器可见, 兼容原版)
    static cvar_t amxmodx_version_cvar = { "amxmodx_version", "1.8.2", FCVAR_SERVER, 0.0f, nullptr };
    CVAR_REGISTER(&amxmodx_version_cvar);
    
    // 注册 amxmodx_modules cvar
    static cvar_t amxmodx_modules_cvar = { "amxmodx_modules", "0", FCVAR_SERVER, 0.0f, nullptr };
    CVAR_REGISTER(&amxmodx_modules_cvar);

    // 设置 localinfo 路径 (兼容原版 AMX Mod X 文件结构)
    SetLocalInfoPaths();

    AMXX_LOG_DBG("[Runtime] Creating forwards...");
    CreateForward("plugin_init");
    CreateForward("plugin_end");
    CreateForward("plugin_cfg");
    CreateForward("plugin_modules");      // 模块加载后回调 (预留, 静态编译版仍保持兼容)
    CreateForward("plugin_natives");      // 模块注册 natives 回调 (预留)
    CreateForward("plugin_precache");     // 地图资源预缓存回调
    CreateForward("plugin_pause");        // 插件被暂停时回调
    CreateForward("plugin_unpause");      // 插件恢复运行时回调
    CreateForward("plugin_log");          // 日志写入回调
    CreateForward("server_frame");
    CreateForwardEx("server_changelevel", ET_STOP);  // 地图切换前 (ET_STOP: 插件返回 PLUGIN_HANDLED 则停止后续插件)
    CreateForward("client_putinserver");
    CreateForward("client_disconnect");
    CreateForward("client_connect");
    CreateForward("client_infochanged");
    CreateForward("client_command");
    CreateForward("client_damage");
    CreateForward("client_death");        // CSX 兼容别名 (player_death 同参数)
    CreateForward("client_kill");
    CreateForward("client_authorized");
    CreateForward("client_userinfochanged");
    CreateForward("client_built");        // CSX: 玩家建造 (预留, 构造完成后可调用)
    CreateForward("player_spawn");
    CreateForward("player_death");
    CreateForward("player_hurt");
    CreateForward("grenade_throw");       // CS: 玩家丢出投掷物 (weapon index, gre ent index)
    CreateForward("round_start");
    CreateForward("round_end");
    CreateForward("round_freeze_end");
    CreateForward("round_restart");

    // CSX bomb forwards
    CreateForward("bomb_planting");     // 玩家开始安放 C4
    CreateForward("bomb_planted");      // C4 已安放完成
    CreateForward("bomb_defusing");     // 玩家开始拆除 C4
    CreateForward("bomb_defused");      // C4 已被拆除
    CreateForward("bomb_explode");      // C4 爆炸

    // AMXX 1.8.3 新增 forwards
    CreateForward("client_disconnected");  // client_disconnect 的增强版
    CreateForward("client_remove");        // client_disconnected 的别名

    AMXX_LOG_DBG("[Runtime] Created %zu forwards", m_forwards.size());

    AMXX_LOG_DBG("[Runtime] Initializing admin system...");
    AMXXAdminSystem::GetInstance().Init();

    AMXX_LOG_DBG("[Runtime] Initializing menu system...");
    AMXXMenuSystem::GetInstance().Init();

    AMXX_LOG_DBG("[Runtime] Initializing language system...");
    AMXXLang::GetInstance().Init();

    AMXX_LOG_DBG("[Runtime] Registering 'amxx' server commands...");
    g_engfuncs.pfnAddServerCommand("amxx", AMXXServerCmd);
    g_engfuncs.pfnAddServerCommand("amxx_plugins", AMXXPluginsCmd);
    // 同时注册到原生服务端命令系统，使 LAN 服主也能在游戏内控制台使用
    AMXXAdminSystem::GetInstance().RegisterNativeServerCommand("amxx", AMXXServerCmdImpl, "m");
    AMXXAdminSystem::GetInstance().RegisterNativeServerCommand("amxx_plugins", AMXXPluginsCmdImpl, "m");

    AMXX_LOG_DBG("[Runtime] Initializing log system...");
    AMXXLogSystem::GetInstance().Init();

    AMXX_LOG_DBG("[Runtime] Initializing vault system...");
    AMXXVault::GetInstance().Init();

    AMXX_LOG_DBG("[Runtime] Initializing message hook system...");
    AMXXMessageSystem::GetInstance().Init();

    AMXX_LOG_DBG("[Runtime] Registering game event hooks...");
    AMXXHooks::Init();

    AMXX_LOG_DBG("[Runtime] Initializing think/touch hook system...");
    AMXXThinkTouch::GetInstance().Init();

    AMXX_LOG_DBG("[Runtime] Initializing Ham Sandwich system...");
    AMXXHamSandwich::GetInstance().Init();

    // 从 plugins.ini 加载插件 (兼容原版 AMX Mod X)
    // 路径相对于游戏目录: <gamedir>/addons/amxmodx/configs/plugins.ini
    char gameDir[256] = {0};
    GET_GAME_DIR(gameDir);
    char pluginsIni[512];
    snprintf(pluginsIni, sizeof(pluginsIni), "%s/addons/amxmodx/configs/plugins.ini", gameDir);
    AMXX_LOG_DBG("[Runtime] Loading plugins from: %s", pluginsIni);
    LoadPluginsFromFile(pluginsIni);

    AMXX_LOG_DBG("[Runtime] Found %zu plugins", m_plugins.size());

    AMXX_LOG_DBG("[Runtime] Registering natives for loaded plugins...");
    for (size_t i = 0; i < m_plugins.size(); i++) {
        AMXXPlugin *plugin = m_plugins[i];
        if (plugin && plugin->IsLoaded()) {
            RegisterNatives(plugin->GetAMX());
            AMXX_LOG_DBG("[Runtime] Registered natives for: %s", plugin->GetName());
        }
    }

    AMXX_LOG_DBG("[Runtime] Linking plugin forwards...");
    for (size_t i = 0; i < m_plugins.size(); i++) {
        AMXXPlugin *plugin = m_plugins[i];
        if (!plugin || !plugin->IsLoaded())
            continue;

        AMXX_LOG_DBG("[Runtime] Processing plugin: %s", plugin->GetName());

        int index = plugin->FindPublic("plugin_init");
        if (index >= 0) {
            AMXXForward *forward = GetForward("plugin_init");
            if (forward) {
                forward->AddPlugin(plugin, index);
                AMXX_LOG_DBG("[Runtime] Linked plugin_init for: %s", plugin->GetName());
            }
        }

        index = plugin->FindPublic("plugin_end");
        if (index >= 0) {
            AMXXForward *forward = GetForward("plugin_end");
            if (forward)
                forward->AddPlugin(plugin, index);
        }

        index = plugin->FindPublic("plugin_cfg");
        if (index >= 0) {
            AMXXForward *forward = GetForward("plugin_cfg");
            if (forward)
                forward->AddPlugin(plugin, index);
        }

        index = plugin->FindPublic("server_frame");
        if (index >= 0) {
            AMXXForward *forward = GetForward("server_frame");
            if (forward)
                forward->AddPlugin(plugin, index);
        }

        index = plugin->FindPublic("client_putinserver");
        if (index >= 0) {
            AMXXForward *forward = GetForward("client_putinserver");
            if (forward)
                forward->AddPlugin(plugin, index);
        }

        index = plugin->FindPublic("client_disconnect");
        if (index >= 0) {
            AMXXForward *forward = GetForward("client_disconnect");
            if (forward)
                forward->AddPlugin(plugin, index);
        }

        index = plugin->FindPublic("client_connect");
        if (index >= 0) {
            AMXXForward *forward = GetForward("client_connect");
            if (forward)
                forward->AddPlugin(plugin, index);
        }

        index = plugin->FindPublic("client_infochanged");
        if (index >= 0) {
            AMXXForward *forward = GetForward("client_infochanged");
            if (forward)
                forward->AddPlugin(plugin, index);
        }

        index = plugin->FindPublic("client_command");
        if (index >= 0) {
            AMXXForward *forward = GetForward("client_command");
            if (forward)
                forward->AddPlugin(plugin, index);
        }

        index = plugin->FindPublic("client_damage");
        if (index >= 0) {
            AMXXForward *forward = GetForward("client_damage");
            if (forward)
                forward->AddPlugin(plugin, index);
        }

        index = plugin->FindPublic("client_kill");
        if (index >= 0) {
            AMXXForward *forward = GetForward("client_kill");
            if (forward)
                forward->AddPlugin(plugin, index);
        }

        index = plugin->FindPublic("client_authorized");
        if (index >= 0) {
            AMXXForward *forward = GetForward("client_authorized");
            if (forward)
                forward->AddPlugin(plugin, index);
        }

        index = plugin->FindPublic("client_userinfochanged");
        if (index >= 0) {
            AMXXForward *forward = GetForward("client_userinfochanged");
            if (forward)
                forward->AddPlugin(plugin, index);
        }

        index = plugin->FindPublic("player_spawn");
        if (index >= 0) {
            AMXXForward *forward = GetForward("player_spawn");
            if (forward)
                forward->AddPlugin(plugin, index);
        }

        index = plugin->FindPublic("player_death");
        if (index >= 0) {
            AMXXForward *forward = GetForward("player_death");
            if (forward)
                forward->AddPlugin(plugin, index);
        }

        index = plugin->FindPublic("player_hurt");
        if (index >= 0) {
            AMXXForward *forward = GetForward("player_hurt");
            if (forward)
                forward->AddPlugin(plugin, index);
        }

        index = plugin->FindPublic("round_start");
        if (index >= 0) {
            AMXXForward *forward = GetForward("round_start");
            if (forward)
                forward->AddPlugin(plugin, index);
        }

        index = plugin->FindPublic("round_end");
        if (index >= 0) {
            AMXXForward *forward = GetForward("round_end");
            if (forward)
                forward->AddPlugin(plugin, index);
        }

        index = plugin->FindPublic("round_freeze_end");
        if (index >= 0) {
            AMXXForward *forward = GetForward("round_freeze_end");
            if (forward)
                forward->AddPlugin(plugin, index);
        }

        index = plugin->FindPublic("round_restart");
        if (index >= 0) {
            AMXXForward *forward = GetForward("round_restart");
            if (forward)
                forward->AddPlugin(plugin, index);
        }

        // plugin_natives: 每个插件可在此回调中通过 register_native 注册动态 native
        // 必须在 plugin_init 之前链接和触发，使其他插件的 plugin_init 能调用到这些 native
        index = plugin->FindPublic("plugin_natives");
        if (index >= 0) {
            AMXXForward *forward = GetForward("plugin_natives");
            if (forward)
                forward->AddPlugin(plugin, index);
        }
    }

    AMXX_LOG_DBG("[Runtime] AMXX runtime initialized successfully");
}

void AMXXRuntime::OnServerActivate()
{
    m_taskManager.ClearAllTasks();
    m_taskManager.SetCurrentTime(0.0f);

    AMXX_BuildUserMsgMap();

    // P2-9: plugin_natives 仅在首次 OnServerActivate 触发 (原版 AMXX 行为)
    // 后续换图不再重复触发，避免 register_native 重复注册
    if (!m_pluginNativesCalled)
    {
        ExecuteForward("plugin_natives");
        m_pluginNativesCalled = true;
    }
    ExecuteForward("plugin_precache");
    ExecuteForward("plugin_init");

    // 读取并执行 amxx.cfg
    {
        char gameDir[256] = {0};
        GET_GAME_DIR(gameDir);
        char cfgPath[512];
        snprintf(cfgPath, sizeof(cfgPath), "%s/addons/amxmodx/configs/amxx.cfg", gameDir);
        FILE *fp = fopen(cfgPath, "r");
        if (fp) {
            char line[1024];
            while (fgets(line, sizeof(line), fp)) {
                size_t len = strlen(line);
                while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
                    line[--len] = '\0';
                if (len == 0 || line[0] == ';' || line[0] == '#' || line[0] == '/')
                    continue;

                char cmdLine[1026];
                snprintf(cmdLine, sizeof(cmdLine), "%s\n", line);
                SERVER_COMMAND(cmdLine);
                SERVER_EXECUTE();
            }
            fclose(fp);
        } else {
            AMXX_LOG("[Runtime] amxx.cfg NOT FOUND at %s", cfgPath);
        }
    }

    ExecuteForward("plugin_cfg");
}

void AMXXRuntime::OnServerDeactivate()
{
    m_taskManager.ExecuteMapEndTasks();
    ExecuteForward("plugin_end");
}

void AMXXRuntime::OnChangeLevel(const char *mapName)
{
    if (!mapName) return;
    // 原版 AMXX: C_ChangeLevel 中先调用 server_changelevel，再执行引擎 CHANGE_LEVEL
    // 参数: string:nextmap
    ForwardCallParam fcp[1];
    fcp[0].type = FP_STRING;
    fcp[0].str = mapName;
    ExecuteForwardEx("server_changelevel", 1, fcp);
    AMXX_LOG("[Runtime] server_changelevel forward called for map '%s'", mapName);
}

void AMXXRuntime::OnGrenadeThrow(CBasePlayer *player, int weaponId, edict_t *greEnt)
{
    if (!player || !greEnt) return;
    int playerIdx = ENTINDEX(ENT(player->pev)) ;  // 注意: player pev to edict -> 1-based
    int greIdx = ENTINDEX(greEnt);                 // 1-based
    ForwardCallParam fcp[3];
    fcp[0].type = FP_CELL;  fcp[0].val = playerIdx;   // index (player)
    fcp[1].type = FP_CELL;  fcp[1].val = greIdx;       // greindex (grenade entity)
    fcp[2].type = FP_CELL;  fcp[2].val = weaponId;      // windex (weapon ID)
    ExecuteForwardEx("grenade_throw", 3, fcp);
}

void AMXXRuntime::OnServerDeactivatePost()
{
    // 1. 清除所有 set_task 定时任务
    m_taskManager.ClearAllTasks();

    // P0-8 修复: 不再 ClearAll SPForwards —— 插件未卸载, 其 forward 句柄仍有效。
    // 原版 AMXX 不在换图时清 forward 句柄池, 只清 per-map hooks (event/message/ham 等)。
    // 旧代码 AMXXForwardManager::GetInstance().ClearAll() 会导致第二张地图 forward 句柄断链。

    // 2. 清除事件系统 hooks (event hooks + message hooks)
    AMXXEventSystem::GetInstance().Clear();

    // 3. 清除 HamSandwich callbacks
    AMXXHamSandwich::GetInstance().Shutdown();

    // 4. 清除 Think/Touch hooks
    AMXXThinkTouch::GetInstance().Shutdown();

    // 5. 清除菜单系统
    AMXXMenuSystem::GetInstance().Shutdown();

    // 6. 清除管理员命令系统 (清空插件注册的命令，保留 admins 配置)
    AMXXAdminSystem::GetInstance().Clear();

    // 7. 清除消息系统（清空插件注册的 hook，保留引擎级 hook）
    AMXXMessageSystem::GetInstance().Clear();

    // 8. 重置 damage records
    ResetDamageRecords();

    // 9. 重置 native_core.cpp 中的文件作用域静态变量
    ResetCoreNativeGlobals();

    // 10. 清理数据结构
    CleanupDatastructs();

    // 11. 清理 SQLx 状态
    ResetSqlxGlobals();

    // 12. 清理 CStrike 全局状态 (hostage/c4 maps)
    ResetCstrikeGlobals();

    // 13. 清理 native_entities 全局状态 (KVD 池)
    ResetEntityGlobals();

    // 14. 清理 DataPack 和 Regex/JSON 句柄
    ResetDataPackHandles();
    ResetRegexHandles();
    ResetJsonHandles();
    ResetCsxGlobals();

    // 15. 清理 GameConfig 句柄
    ResetGameConfigGlobals();

    // 16. 清理 TextParse (INI/SMC) 句柄
    ResetTextparseGlobals();

    // 17. 清理 Sockets 和 DBI 句柄
    ResetSocketsGlobals();
    ResetDbiGlobals();
}

void AMXXRuntime::Shutdown()
{
    AMXX_LOG_DBG("[Runtime] Shutting down AMXX runtime...");

    if (!m_initialized) {
        AMXX_LOG_DBG("[Runtime] Not initialized");
        return;
    }

    AMXX_LOG_DBG("[Runtime] Executing plugin_end forwards...");
    ExecuteForward("plugin_end");

    AMXX_LOG_DBG("[Runtime] Shutting down think/touch system...");
    AMXXThinkTouch::GetInstance().Shutdown();

    AMXX_LOG_DBG("[Runtime] Shutting down Ham Sandwich system...");
    AMXXHamSandwich::GetInstance().Shutdown();

    AMXX_LOG_DBG("[Runtime] Unregistering game event hooks...");
    AMXXHooks::Shutdown();

    AMXX_LOG_DBG("[Runtime] Unloading all plugins...");
    UnloadAllPlugins();

    AMXX_LOG_DBG("[Runtime] Cleaning up %zu forwards...", m_forwards.size());
    for (size_t i = 0; i < m_forwards.size(); i++) {
        delete m_forwards[i];
    }
    m_forwards.clear();

    m_initialized = false;
    AMXX_LOG_DBG("[Runtime] AMXX runtime shutdown complete");
}

void AMXXRuntime::Frame()
{
    if (!m_initialized)
        return;

    m_taskManager.SetCurrentTime(gpGlobals->time);
    m_taskManager.ProcessTasks();

    // RequestFrame 回调: 执行并清空 (每帧只执行一次)
    if (!m_frameCallbacks.empty()) {
        std::vector<AMXXFrameCallback> callbacks;
        callbacks.swap(m_frameCallbacks);
        for (size_t i = 0; i < callbacks.size(); i++) {
            AMXXPlugin *plugin = FindPluginByAMX(callbacks[i].amx);
            if (!plugin || !plugin->IsLoaded())
                continue;
            AMX *amx = plugin->GetAMX();
            amx_Push(amx, callbacks[i].data);
            cell retval;
            plugin->ExecutePublic(callbacks[i].funcIndex, &retval);
        }
    }

    SyncCvarBindings();
    PollCvarHooks();
    UpdateSilentFootsteps();   // P2: 静默脚步声每帧维持
    ExecuteForward("server_frame");
}

void AMXXRuntime::RequestFrame(AMX *amx, int funcIndex, cell data)
{
    AMXXFrameCallback cb;
    cb.amx = amx;
    cb.funcIndex = funcIndex;
    cb.data = data;
    m_frameCallbacks.push_back(cb);
}

bool AMXXRuntime::LoadPlugin(const char *filename)
{
    AMXX_LOG_DBG("[Runtime] LoadPlugin: %s", filename);

    AMXXPlugin *plugin = new AMXXPlugin();
    if (!plugin->Load(filename)) {
        AMXX_LOG_DBG("[Runtime] LoadPlugin failed: %s", filename);
        delete plugin;
        return false;
    }

    m_plugins.push_back(plugin);
    AMXX_LOG_DBG("[Runtime] LoadPlugin succeeded: %s", plugin->GetName());
    return true;
}

void AMXXRuntime::UnloadAllPlugins()
{
    AMXX_LOG_DBG("[Runtime] UnloadAllPlugins: %zu plugins", m_plugins.size());

    for (size_t i = 0; i < m_plugins.size(); i++) {
        AMXX_LOG_DBG("[Runtime] Unloading plugin: %s", m_plugins[i]->GetName());

        m_taskManager.RemoveTasksByPlugin(m_plugins[i]);
        AMXXThinkTouch::GetInstance().UnregisterByAMX(m_plugins[i]->GetAMX());
        AMXXHamSandwich::GetInstance().UnregisterByAMX(m_plugins[i]->GetAMX());

        for (size_t j = 0; j < m_forwards.size(); j++) {
            m_forwards[j]->RemovePlugin(m_plugins[i]);
        }

        m_plugins[i]->Unload();
        delete m_plugins[i];
    }
    m_plugins.clear();
    AMXX_LOG_DBG("[Runtime] UnloadAllPlugins complete");
}

void AMXXRuntime::RegisterNatives(AMX *amx)
{
    static AMX_NATIVE_INFO *combined = nullptr;
    static int totalNatives = 0;
    if (!combined) {
        int coreCount = 0, funCount = 0, cstrikeCount = 0, adminCount = 0;
        int dsCount = 0, eventCount = 0, entityCount = 0;
        while (core_natives[coreCount].name != nullptr) coreCount++;
        while (fun_natives[funCount].name != nullptr) funCount++;
        while (cstrike_natives[cstrikeCount].name != nullptr) cstrikeCount++;
        while (admin_natives[adminCount].name != nullptr) adminCount++;
        while (datastruct_natives[dsCount].name != nullptr) dsCount++;
        while (event_natives[eventCount].name != nullptr) eventCount++;
        while (entity_natives[entityCount].name != nullptr) entityCount++;
        int menuCount = 0;
        while (menu_natives[menuCount].name != nullptr) menuCount++;
        int langCount = 0;
        while (lang_natives[langCount].name != nullptr) langCount++;
        int logCount = 0;
        while (log_natives[logCount].name != nullptr) logCount++;
        int vaultCount = 0;
        while (vault_natives[vaultCount].name != nullptr) vaultCount++;
        int messageCount = 0;
        while (message_natives[messageCount].name != nullptr) messageCount++;
        int fakemetaCount = 0;
        while (fakemeta_natives[fakemetaCount].name != nullptr) fakemetaCount++;
        int sqlxCount = 0;
        while (sqlx_natives[sqlxCount].name != nullptr) sqlxCount++;
        int datapackCount = 0;
        while (datapack_natives[datapackCount].name != nullptr) datapackCount++;
        int regexCount = 0;
        while (regex_natives[regexCount].name != nullptr) regexCount++;
        int jsonCount = 0;
        while (json_natives[jsonCount].name != nullptr) jsonCount++;
        int csxCount = 0;
        while (csx_natives[csxCount].name != nullptr) csxCount++;
        int gameconfigCount = 0;
        while (gameconfig_natives[gameconfigCount].name != nullptr) gameconfigCount++;
        int textparseCount = 0;
        while (textparse_natives[textparseCount].name != nullptr) textparseCount++;
        int socketsCount = 0;
        while (sockets_natives[socketsCount].name != nullptr) socketsCount++;
        int dbiCount = 0;
        while (dbi_natives[dbiCount].name != nullptr) dbiCount++;
        int geoipCount = 0;
        while (geoip_natives[geoipCount].name != nullptr) geoipCount++;

        totalNatives = coreCount + funCount + cstrikeCount + adminCount + dsCount + eventCount + entityCount + menuCount + langCount + logCount + vaultCount + messageCount + fakemetaCount + sqlxCount + datapackCount + regexCount + jsonCount + csxCount + gameconfigCount + textparseCount + socketsCount + dbiCount + geoipCount;
        combined = new AMX_NATIVE_INFO[totalNatives + 1];
        int idx = 0;
        for (int i = 0; i < coreCount; i++)
            combined[idx++] = core_natives[i];
        for (int i = 0; i < funCount; i++)
            combined[idx++] = fun_natives[i];
        for (int i = 0; i < cstrikeCount; i++)
            combined[idx++] = cstrike_natives[i];
        for (int i = 0; i < adminCount; i++)
            combined[idx++] = admin_natives[i];
        for (int i = 0; i < dsCount; i++)
            combined[idx++] = datastruct_natives[i];
        for (int i = 0; i < eventCount; i++)
            combined[idx++] = event_natives[i];
        for (int i = 0; i < entityCount; i++)
            combined[idx++] = entity_natives[i];
        for (int i = 0; i < menuCount; i++)
            combined[idx++] = menu_natives[i];
        for (int i = 0; i < langCount; i++)
            combined[idx++] = lang_natives[i];
        for (int i = 0; i < logCount; i++)
            combined[idx++] = log_natives[i];
        for (int i = 0; i < vaultCount; i++)
            combined[idx++] = vault_natives[i];
        for (int i = 0; i < messageCount; i++)
            combined[idx++] = message_natives[i];
        for (int i = 0; i < fakemetaCount; i++)
            combined[idx++] = fakemeta_natives[i];
        for (int i = 0; i < sqlxCount; i++)
            combined[idx++] = sqlx_natives[i];
        for (int i = 0; i < datapackCount; i++)
            combined[idx++] = datapack_natives[i];
        for (int i = 0; i < regexCount; i++)
            combined[idx++] = regex_natives[i];
        for (int i = 0; i < jsonCount; i++)
            combined[idx++] = json_natives[i];
        for (int i = 0; i < csxCount; i++)
            combined[idx++] = csx_natives[i];
        for (int i = 0; i < gameconfigCount; i++)
            combined[idx++] = gameconfig_natives[i];
        for (int i = 0; i < socketsCount; i++)
            combined[idx++] = sockets_natives[i];
        for (int i = 0; i < dbiCount; i++)
            combined[idx++] = dbi_natives[i];
        for (int i = 0; i < geoipCount; i++)
            combined[idx++] = geoip_natives[i];
        for (int i = 0; i < textparseCount; i++)
            combined[idx++] = textparse_natives[i];
        combined[idx] = {nullptr, nullptr};
    }

    amx_Register(amx, combined, totalNatives);
}

AMXXForward *AMXXRuntime::GetForward(const char *name)
{
    for (size_t i = 0; i < m_forwards.size(); i++) {
        if (strcmp(m_forwards[i]->GetName(), name) == 0)
            return m_forwards[i];
    }
    return nullptr;
}

AMXXForward *AMXXRuntime::CreateForward(const char *name)
{
    AMXXForward *forward = GetForward(name);
    if (forward)
        return forward;

    forward = new AMXXForward(name);
    m_forwards.push_back(forward);
    return forward;
}

AMXXForward *AMXXRuntime::CreateForwardEx(const char *name, ForwardExecType execType)
{
    AMXXForward *forward = GetForward(name);
    if (forward)
        return forward;

    forward = new AMXXForward(name, execType);
    m_forwards.push_back(forward);
    return forward;
}

int AMXXRuntime::ExecuteForwardEx(const char *name, int paramCount, ForwardCallParam *params)
{
    AMXXForward *forward = GetForward(name);
    if (!forward)
        return PLUGIN_CONTINUE;

    int result = forward->Execute(paramCount, params);

    return result;
}

void AMXXRuntime::ExecuteForward(const char *name, int paramCount, ...)
{
    ForwardCallParam params[32];
    va_list args;
    va_start(args, paramCount);

    for (int i = 0; i < paramCount && i < 32; i++) {
        params[i].type = FP_CELL;
        params[i].val = va_arg(args, cell);
        params[i].size = 0;
    }

    va_end(args);

    ExecuteForwardEx(name, paramCount, paramCount > 0 ? params : nullptr);
}

PlayerDamageRecord &AMXXRuntime::GetDamageRecord(int playerId)
{
    if (playerId < 0) {
        static PlayerDamageRecord dummy;
        dummy.Reset();
        return dummy;
    }
    if (playerId >= (int)m_damageRecords.size()) {
        size_t oldSize = m_damageRecords.size();
        m_damageRecords.resize(playerId + 1);
        for (size_t i = oldSize; i < m_damageRecords.size(); i++) {
            m_damageRecords[i].Reset();
        }
    }
    return m_damageRecords[playerId];
}

void AMXXRuntime::ResetDamageRecords()
{
    for (auto &rec : m_damageRecords) {
        rec.Reset();
    }
}

void AMXXRuntime::SetLastDamage(int playerId, int attacker, int damage, int weapon)
{
    PlayerDamageRecord &rec = GetDamageRecord(playerId);
    rec.lastAttacker = attacker;
    rec.lastDamage = (float)damage;
    rec.lastWeapon = weapon;
}

int AMXXRuntime::GetLastAttacker(int playerId)
{
    return GetDamageRecord(playerId).lastAttacker;
}

int AMXXRuntime::GetLastWeapon(int playerId)
{
    return GetDamageRecord(playerId).lastWeapon;
}

AMXXPlugin *AMXXRuntime::FindPluginByAMX(AMX *amx)
{
    for (size_t i = 0; i < m_plugins.size(); i++) {
        if (m_plugins[i]->GetAMX() == amx) {
            return m_plugins[i];
        }
    }
    return nullptr;
}

void AMXXRuntime::ScanPlugins(const char *directory)
{
#ifdef _WIN32
    char pattern[512];
    snprintf(pattern, sizeof(pattern), "%s/*.amxx", directory);

    struct _finddata_t fileinfo;
    intptr_t handle = _findfirst(pattern, &fileinfo);

    if (handle != -1) {
        do {
            char fullpath[512];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", directory, fileinfo.name);

            LoadPlugin(fullpath);
        } while (_findnext(handle, &fileinfo) == 0);
        _findclose(handle);
    }

    snprintf(pattern, sizeof(pattern), "%s/*.amx", directory);
    handle = _findfirst(pattern, &fileinfo);

    if (handle != -1) {
        do {
            char fullpath[512];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", directory, fileinfo.name);

            LoadPlugin(fullpath);
        } while (_findnext(handle, &fileinfo) == 0);
        _findclose(handle);
    }
#else
    DIR *dir = opendir(directory);
    if (!dir)
        return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        size_t namelen = strlen(name);

        if (namelen < 5)
            continue;

        const char *ext = name + namelen - 5;
        bool isAMXX = (strcmp(ext, ".amxx") == 0);
        bool isAMX = (strcmp(ext, ".amx") == 0) && !isAMXX;

        if (!isAMXX && !isAMX)
            continue;

        char fullpath[512];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", directory, name);

        LoadPlugin(fullpath);
    }
    closedir(dir);
#endif
}

// 设置 localinfo 路径 (兼容原版 AMX Mod X 文件结构)
void AMXXRuntime::SetLocalInfoPaths()
{
    // 通过 infokey buffer 设置 localinfo (HL 引擎标准方式)
    // NULL infobuffer 获取服务器 localinfo
    char *infobuffer = g_engfuncs.pfnGetInfoKeyBuffer(NULL);
    
    // AMX Mod X 标准路径
    g_engfuncs.pfnSetKeyValue(infobuffer, "amxx_basedir", "addons/amxmodx");
    g_engfuncs.pfnSetKeyValue(infobuffer, "amxx_pluginsdir", "addons/amxmodx/plugins");
    g_engfuncs.pfnSetKeyValue(infobuffer, "amxx_modulesdir", "addons/amxmodx/modules");
    g_engfuncs.pfnSetKeyValue(infobuffer, "amxx_configsdir", "addons/amxmodx/configs");
    g_engfuncs.pfnSetKeyValue(infobuffer, "amxx_customdir", "addons/amxmodx/custom");
    g_engfuncs.pfnSetKeyValue(infobuffer, "amxx_logdir", "addons/amxmodx/logs");
    g_engfuncs.pfnSetKeyValue(infobuffer, "amxx_plugins", "addons/amxmodx/configs/plugins.ini");
    g_engfuncs.pfnSetKeyValue(infobuffer, "amxx_vault", "addons/amxmodx/configs/vault.ini");
    
    // AMX Mod 向后兼容路径
    g_engfuncs.pfnSetKeyValue(infobuffer, "amx_basedir", "addons/amxmodx");
    g_engfuncs.pfnSetKeyValue(infobuffer, "amx_configdir", "addons/amxmodx/configs");
    g_engfuncs.pfnSetKeyValue(infobuffer, "amx_langdir", "addons/amxmodx/data/amxmod-lang");
    g_engfuncs.pfnSetKeyValue(infobuffer, "amx_modulesdir", "addons/amxmodx/modules");
    g_engfuncs.pfnSetKeyValue(infobuffer, "amx_pluginsdir", "addons/amxmodx/plugins");
    g_engfuncs.pfnSetKeyValue(infobuffer, "amx_logdir", "addons/amxmodx/logs");
    
    AMXX_LOG_DBG("[Runtime] LocalInfo paths set (amxx_basedir=addons/amxmodx)");
}

// 从 plugins.ini 加载插件列表 (兼容原版 AMX Mod X plugins.ini 格式)
// 格式: pluginname.amxx [debug]  ; 注释
void AMXXRuntime::LoadPluginsFromFile(const char *configFile)
{
    FILE *fp = fopen(configFile, "r");
    if (!fp) {
        AMXX_LOG_DBG("[Runtime] plugins.ini not found: %s, falling back to directory scan", configFile);
        // 回退到目录扫描
        char gameDir[256] = {0};
        GET_GAME_DIR(gameDir);
        char pluginsDir[512];
        snprintf(pluginsDir, sizeof(pluginsDir), "%s/addons/amxmodx/plugins", gameDir);
        ScanPlugins(pluginsDir);
        return;
    }
    
    char line[256];
    // 从 plugins.ini 路径提取插件目录: <gamedir>/addons/amxmodx/configs -> <gamedir>/addons/amxmodx/plugins
    char pluginsDir[512];
    strncpy(pluginsDir, configFile, sizeof(pluginsDir)-1);
    pluginsDir[sizeof(pluginsDir)-1] = '\0';
    char *cfgPos = strstr(pluginsDir, "/configs/");
    if (cfgPos) {
        strcpy(cfgPos, "/plugins");
    } else {
        // 回退到游戏目录下的 plugins
        char gameDir[256] = {0};
        GET_GAME_DIR(gameDir);
        snprintf(pluginsDir, sizeof(pluginsDir), "%s/addons/amxmodx/plugins", gameDir);
    }
    
    while (fgets(line, sizeof(line), fp)) {
        // 去除行尾换行符
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        nl = strchr(line, '\r');
        if (nl) *nl = '\0';
        
        // 跳过空行和注释行 (; 或 # 或 //)
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;  // 跳过前导空格
        if (*p == ';' || *p == '#' || *p == '\0' || (*p == '/' && *(p+1) == '/'))
            continue;
        
        // 提取插件文件名 (第一个 token)
        char pluginName[128] = {0};
        int i = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != ';' && i < (int)sizeof(pluginName)-1) {
            pluginName[i++] = *p++;
        }
        pluginName[i] = '\0';
        
        if (pluginName[0] == '\0')
            continue;
        
        // 检查是否有 debug 标志
        bool debugMode = false;
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "debug", 5) == 0) {
            debugMode = true;
        }
        
        // 构建完整路径: <gamedir>/addons/amxmodx/plugins/pluginname.amxx
        char fullpath[512];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", pluginsDir, pluginName);
        
        AMXX_LOG_DBG("[Runtime] Loading plugin: %s%s", pluginName, debugMode ? " (debug)" : "");
        if (!LoadPlugin(fullpath)) {
            AMXX_LOG_DBG("[Runtime] Failed to load: %s", pluginName);
        } else if (debugMode && !m_plugins.empty()) {
            // Apply debug flag to the just-loaded plugin
            AMXXPlugin *last = m_plugins.back();
            last->SetFlags(last->GetFlags() | AMX_FLAG_DEBUG);
        }
    }
    
    fclose(fp);
}
