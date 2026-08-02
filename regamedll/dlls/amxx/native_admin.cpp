#include "precompiled.h"
#include "admin.h"
#include "amx.h"
#include "runtime.h"
#include "../extdll.h"
#include "../enginecallback.h"

#include <cstdio>

// 辅助：将标志字符串 (如 "abc") 转为位掩码，与 amxx_read_flags 逻辑一致
static int AdminFlagsStrToBitmask(const char *str)
{
    int result = 0;
    if (!str) return result;
    for (int i = 0; str[i]; i++) {
        char c = tolower((unsigned char)str[i]);
        if (c >= 'a' && c <= 'z')
            result |= (1 << (c - 'a'));
    }
    return result;
}

// 辅助：从 AMX* 查找插件 ID (1-based, 兼容 AMXX)
static int AmxToPluginId(AMX *amx)
{
    const auto &plugins = AMXXRuntime::GetInstance().GetPlugins();
    for (size_t i = 0; i < plugins.size(); i++) {
        if (plugins[i] && plugins[i]->GetAMX() == amx)
            return (int)(i + 1);
    }
    return -1;
}


cell AMX_NATIVE_CALL amxx_register_clcmd(AMX *amx, cell *params)
{
    cell *addr;
    char cmd[256], description[128];

    if (amx_GetAddr(amx, params[1], &addr) != AMX_ERR_NONE || !addr) {
        cmd[0] = '\0';
    } else {
        amx_GetString(cmd, addr, 0, sizeof(cmd));
    }

    char funcname[256];
    if (amx_GetAddr(amx, params[2], &addr) != AMX_ERR_NONE || !addr) {
        AMXX_LOG("[Admin] register_clcmd: invalid function name for cmd '%s'", cmd);
        return 0;
    }
    amx_GetString(funcname, addr, 0, sizeof(funcname));

    int funcidx;
    if (amx_FindPublic(amx, funcname, &funcidx) != AMX_ERR_NONE) {
        AMXX_LOG("[Admin] register_clcmd: function '%s' not found for cmd '%s'", funcname, cmd);
        return 0;
    }

    // flags 是整数（admin flag bits），-1 = ADMIN_ALL(0)
    cell num_params = params[0] / sizeof(cell);
    int flagsInt = -1;
    if (num_params >= 3) {
        flagsInt = (int)params[3];
    }
    if (flagsInt == -1) flagsInt = 0;

    // 转换为 flag 字符串（与 ParseFlags 逆操作）
    char flags[27] = {0};
    int fpos = 0;
    for (int bit = 0; bit < 26; bit++) {
        if (flagsInt & (1 << bit))
            flags[fpos++] = 'a' + bit;
    }

    description[0] = '\0';
    if (num_params >= 4 && amx_GetAddr(amx, params[4], &addr) == AMX_ERR_NONE && addr) {
        amx_GetString(description, addr, 0, sizeof(description));
    }

    AMXXAdminSystem::GetInstance().RegisterCommand(amx, cmd, funcidx, flags, description);
    return 1;
}

cell AMX_NATIVE_CALL amxx_register_srvcmd(AMX *amx, cell *params)
{
    cell *addr;
    char cmd[256], description[128];

    if (amx_GetAddr(amx, params[1], &addr) != AMX_ERR_NONE || !addr) {
        cmd[0] = '\0';
    } else {
        amx_GetString(cmd, addr, 0, sizeof(cmd));
    }

    char funcname[256];
    if (amx_GetAddr(amx, params[2], &addr) != AMX_ERR_NONE || !addr) {
        printf("[AMXX] [Admin] register_srvcmd FAIL: invalid func name arg for cmd '%s'\n", cmd);
        return 0;
    }
    amx_GetString(funcname, addr, 0, sizeof(funcname));

    int funcidx;
    if (amx_FindPublic(amx, funcname, &funcidx) != AMX_ERR_NONE) {
        printf("[AMXX] [Admin] register_srvcmd FAIL: public '%s' not found for cmd '%s'\n", funcname, cmd);
        return 0;
    }

    // flags 是整数（admin flag bits），-1 = ADMIN_ALL(0)
    cell num_params = params[0] / sizeof(cell);
    int flagsInt = -1;
    if (num_params >= 3) {
        flagsInt = (int)params[3];
    }
    if (flagsInt == -1) flagsInt = 0;

    char flags[27] = {0};
    int fpos = 0;
    for (int bit = 0; bit < 26; bit++) {
        if (flagsInt & (1 << bit))
            flags[fpos++] = 'a' + bit;
    }

    description[0] = '\0';
    if (num_params >= 4 && amx_GetAddr(amx, params[4], &addr) == AMX_ERR_NONE && addr) {
        amx_GetString(description, addr, 0, sizeof(description));
    }

    AMXXAdminSystem::GetInstance().RegisterServerCommand(amx, cmd, funcidx, flags, description);
    printf("[AMXX] [Admin] register_srvcmd OK: cmd='%s' func='%s' flags='%s'\n", cmd, funcname, flags);
    return 1;
}

cell AMX_NATIVE_CALL amxx_access(AMX *amx, cell *params)
{
    int playerId = params[1];
    int flags = params[2];
    
    return AMXXAdminSystem::GetInstance().CheckAccess(playerId, flags) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_get_user_flags(AMX *amx, cell *params)
{
    int playerId = params[1];
    int flags = AMXXAdminSystem::GetInstance().GetUserFlags(playerId);
    printf("[AMXX] [get_user_flags] player=%d → flags=%d\n", playerId, flags);
    return flags;
}

cell AMX_NATIVE_CALL amxx_set_user_flags(AMX *amx, cell *params)
{
    int playerId = params[1];
    int flags = params[2];
    (void)amx;

    AMXXAdminSystem::GetInstance().SetUserFlags(playerId, flags);

    return 1;
}

cell AMX_NATIVE_CALL amxx_cmd_access(AMX *amx, cell *params)
{
    int playerId = params[1];
    int flags = params[2];
    bool silent = params[3] != 0;
    (void)amx;

    return AMXXAdminSystem::GetInstance().CmdAccess(playerId, flags, "", silent);
}

cell AMX_NATIVE_CALL amxx_admins_num(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return AMXXAdminSystem::GetInstance().GetAdminsNum();
}

// native admins_push(const AuthData[], const Password[], Access, Flags);
cell AMX_NATIVE_CALL amxx_admins_push(AMX *amx, cell *params)
{
    cell *auth_addr, *pass_addr;
    amx_GetAddr(amx, params[1], &auth_addr);
    amx_GetAddr(amx, params[2], &pass_addr);

    char authData[256], password[256];
    amx_GetString(authData, auth_addr, 0, sizeof(authData));
    amx_GetString(password, pass_addr, 0, sizeof(password));

    int access = (int)params[3];
    int flags = (int)params[4];
    AMXXAdminSystem::GetInstance().PushAdmin(authData, password, access, flags);
    return 1;
}

// native admins_lookup(num, AdminProp:Property, Buffer[] = "", BufferSize = 0);
// Returns: property value if AdminProp_Access or AdminProp_Flags, 0 otherwise
cell AMX_NATIVE_CALL amxx_admins_lookup(AMX *amx, cell *params)
{
    int index = (int)params[1];
    int prop = (int)params[2];

    const AdminEntry *entry = AMXXAdminSystem::GetInstance().GetAdmin(index);
    if (!entry) {
        AMXX_LOG("[Admin] admins_lookup: invalid index %d", index);
        return 0;
    }

    int numParams = params[0] / sizeof(cell);

    switch (prop) {
        case AdminProp_Auth: {
            // Return the auth data string into Buffer
            if (numParams >= 4) {
                cell *bufAddr;
                amx_GetAddr(amx, params[3], &bufAddr);
                if (bufAddr) {
                    int bufSize = (int)params[4];
                    const char *authData = "";
                    if (!entry->authid.empty())
                        authData = entry->authid.c_str();
                    else if (!entry->ip.empty())
                        authData = entry->ip.c_str();
                    else if (!entry->name.empty())
                        authData = entry->name.c_str();
                    amx_SetString(bufAddr, authData, 0, 0, bufSize);
                }
            }
            return 0;
        }
        case AdminProp_Password: {
            if (numParams >= 4) {
                cell *bufAddr;
                amx_GetAddr(amx, params[3], &bufAddr);
                if (bufAddr) {
                    int bufSize = (int)params[4];
                    amx_SetString(bufAddr, entry->password.c_str(), 0, 0, bufSize);
                }
            }
            return 0;
        }
        case AdminProp_Access:
            return (cell)entry->access;
        case AdminProp_Flags:
            return (cell)entry->flags;
        default:
            AMXX_LOG("[Admin] admins_lookup: unknown AdminProp %d", prop);
            return 0;
    }
}

cell AMX_NATIVE_CALL amxx_admins_remove(AMX *amx, cell *params)
{
    (void)amx;
    return AMXXAdminSystem::GetInstance().RemoveAdmin((int)params[1]) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_reload_admins(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    AMXXAdminSystem::GetInstance().ReloadAdmins();
    return 1;
}

cell AMX_NATIVE_CALL amxx_get_user_auths(AMX *amx, cell *params)
{
    int playerId = params[1];
    cell *auth_addr, *ip_addr, *name_addr;
    amx_GetAddr(amx, params[2], &auth_addr);
    amx_GetAddr(amx, params[3], &ip_addr);
    amx_GetAddr(amx, params[4], &name_addr);

    if (playerId < 1 || playerId > gpGlobals->maxClients)
        return 0;

    edict_t *pEdict = INDEXENT(playerId);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;

    char *infobuffer = g_engfuncs.pfnGetInfoKeyBuffer(pEdict);
    const char *authid = g_engfuncs.pfnInfoKeyValue(infobuffer, "authid");
    const char *ip = g_engfuncs.pfnInfoKeyValue(infobuffer, "ip");
    const char *name = STRING(pEdict->v.netname);

    amx_SetString(auth_addr, authid ? authid : "", 0, 0, params[5]);
    amx_SetString(ip_addr, ip ? ip : "", 0, 0, params[6]);
    amx_SetString(name_addr, name ? name : "", 0, 0, params[7]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_is_user_admin(AMX *amx, cell *params)
{
    (void)amx;
    int index = params[1];
    AMXXAdminSystem &adminSys = AMXXAdminSystem::GetInstance();
    return adminSys.IsAdmin(index) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_remove_user_flags(AMX *amx, cell *params)
{
    (void)amx;
    int index = params[1];
    int flags = params[2];
    AMXXAdminSystem &adminSys = AMXXAdminSystem::GetInstance();
    adminSys.RemoveFlags(index, flags);
    return 1;
}

// has_rcon(id) - 检查玩家是否有 rcon 权限
cell AMX_NATIVE_CALL amxx_has_rcon(AMX *amx, cell *params)
{
    int index = params[1];
    (void)amx;
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;
    // 检查玩家是否拥有 ADMIN_RCON 标志或通过服务器管理员验证
    AMXXAdminSystem &adminSys = AMXXAdminSystem::GetInstance();
    int flags = adminSys.GetUserFlags(index);
    return (flags & ADMIN_RCON) ? 1 : 0;
}

// amxx_register_concmd - Register a console command (alias for register_clcmd)
cell AMX_NATIVE_CALL amxx_register_concmd(AMX *amx, cell *params)
{
    cell *addr;
    char cmd[256], description[128];

    if (amx_GetAddr(amx, params[1], &addr) != AMX_ERR_NONE || !addr) {
        cmd[0] = '\0';
    } else {
        amx_GetString(cmd, addr, 0, sizeof(cmd));
    }

    char funcname[256];
    if (amx_GetAddr(amx, params[2], &addr) != AMX_ERR_NONE || !addr) {
        AMXX_LOG("[Admin] register_concmd: invalid function name for cmd '%s'", cmd);
        return 0;
    }
    amx_GetString(funcname, addr, 0, sizeof(funcname));

    int funcidx;
    if (amx_FindPublic(amx, funcname, &funcidx) != AMX_ERR_NONE) {
        AMXX_LOG("[Admin] register_concmd: function '%s' not found for cmd '%s'", funcname, cmd);
        return 0;
    }

    // flags 是整数（admin flag bits），-1 = ADMIN_ALL(0)
    cell num_params = params[0] / sizeof(cell);
    int flagsInt = -1;
    if (num_params >= 3) {
        flagsInt = (int)params[3];
    }
    if (flagsInt == -1) flagsInt = 0;

    // 转换为 flag 字符串（与 ParseFlags 逆操作）
    char flags[27] = {0};
    int fpos = 0;
    for (int bit = 0; bit < 26; bit++) {
        if (flagsInt & (1 << bit))
            flags[fpos++] = 'a' + bit;
    }

    description[0] = '\0';
    if (num_params >= 4 && amx_GetAddr(amx, params[4], &addr) == AMX_ERR_NONE && addr) {
        amx_GetString(description, addr, 0, sizeof(description));
    }

    AMXXAdminSystem::GetInstance().RegisterConsoleCommand(amx, cmd, funcidx, flags, description);
    return 1;
}

// amxx_get_concmd - 获取已注册命令的信息
// native get_concmd(index, cmd[], len1, &flags, info[], len2, flag, id = -1, bool:ignore_info = false);
cell AMX_NATIVE_CALL amxx_get_concmd(AMX *amx, cell *params)
{
    int num_params = params[0] / sizeof(cell);
    int targetIndex = (int)params[1];
    int filterFlag = (num_params >= 7) ? (int)params[7] : 0;
    int filterPlid = (num_params >= 8) ? (int)params[8] : -1;
    bool ignoreInfo = (num_params >= 9) ? (params[9] != 0) : false;

    int curIndex = 0;
    auto &cmds = AMXXAdminSystem::GetInstance().GetCommands();
    for (auto &cmd : cmds) {
        if (cmd.cmdType != CMD_ConsoleCommand)
            continue;
        // 插件 ID 过滤
        if (filterPlid != -1) {
            int cmdPlid = AmxToPluginId(cmd.amx);
            if (cmdPlid != filterPlid)
                continue;
        }
        // 权限标志过滤 (flag==0 不过滤; 否则命令权限与过滤标志有交集)
        int cmdAccess = AdminFlagsStrToBitmask(cmd.flags.c_str());
        if (filterFlag != 0 && (cmdAccess & filterFlag) == 0)
            continue;

        if (curIndex == targetIndex) {
            // 写入命令名
            cell *cmdAddr;
            amx_GetAddr(amx, params[2], &cmdAddr);
            amx_SetString(cmdAddr, cmd.cmd.c_str(), 0, 0, (int)params[3]);

            // 写入权限标志
            cell *flagsAddr;
            amx_GetAddr(amx, params[4], &flagsAddr);
            *flagsAddr = (cell)cmdAccess;

            // 写入描述
            if (!ignoreInfo) {
                cell *infoAddr;
                amx_GetAddr(amx, params[5], &infoAddr);
                int infoLen = (num_params >= 6) ? (int)params[6] : 0;
                if (infoLen > 0 && infoAddr)
                    amx_SetString(infoAddr, cmd.description.c_str(), 0, 0, infoLen);
            }
            return 1;
        }
        curIndex++;
    }
    return 0;
}

// amxx_get_concmdsnum - 获取已注册控制台命令数量
// native get_concmdsnum(flag, id = -1);
cell AMX_NATIVE_CALL amxx_get_concmdsnum(AMX *amx, cell *params)
{
    (void)amx;
    int num_params = params[0] / sizeof(cell);
    int filterFlag = (int)params[1];
    int filterPlid = (num_params >= 2) ? (int)params[2] : -1;

    int count = 0;
    auto &cmds = AMXXAdminSystem::GetInstance().GetCommands();
    for (auto &cmd : cmds) {
        if (cmd.cmdType != CMD_ConsoleCommand)
            continue;
        if (filterPlid != -1) {
            int cmdPlid = AmxToPluginId(cmd.amx);
            if (cmdPlid != filterPlid)
                continue;
        }
        int cmdAccess = AdminFlagsStrToBitmask(cmd.flags.c_str());
        if (filterFlag != 0 && (cmdAccess & filterFlag) == 0)
            continue;
        count++;
    }
    return count;
}

// amxx_get_concmd_plid - 根据命令名获取插件 ID
// native get_concmd_plid(cmd[], &flags, id = -1);
cell AMX_NATIVE_CALL amxx_get_concmd_plid(AMX *amx, cell *params)
{
    (void)amx;
    int num_params = params[0] / sizeof(cell);
    int filterPlid = (num_params >= 3) ? (int)params[3] : -1;

    cell *cmdAddr;
    amx_GetAddr(amx, params[1], &cmdAddr);
    char cmdName[256];
    amx_GetString(cmdName, cmdAddr, 0, sizeof(cmdName));

    auto &cmds = AMXXAdminSystem::GetInstance().GetCommands();
    for (auto &cmd : cmds) {
        if (cmd.cmdType != CMD_ConsoleCommand)
            continue;
        if (stricmp(cmd.cmd.c_str(), cmdName) != 0)
            continue;
        int cmdPlid = AmxToPluginId(cmd.amx);
        if (filterPlid != -1 && cmdPlid != filterPlid)
            continue;

        // 输出权限标志
        cell *flagsAddr;
        amx_GetAddr(amx, params[2], &flagsAddr);
        if (flagsAddr)
            *flagsAddr = (cell)AdminFlagsStrToBitmask(cmd.flags.c_str());
        return cmdPlid;
    }
    return 0;
}

// amxx_read_flags - Convert flag string to integer bitmask
cell AMX_NATIVE_CALL amxx_read_flags(AMX *amx, cell *params)
{
    cell *addr;
    amx_GetAddr(amx, params[1], &addr);

    char flags[256];
    amx_GetString(flags, addr, 0, sizeof(flags));

    // 参考原版 AMXX 的 UTIL_ReadFlags: flags |= (1<<(*c++ - 'a'))
    int result = 0;
    for (int i = 0; flags[i]; i++) {
        char c = tolower(flags[i]);
        if (c >= 'a' && c <= 'z') {
            result |= (1 << (c - 'a'));
        }
    }

    return result;
}

// amxx_get_flags - Convert integer bitmask to flag string
cell AMX_NATIVE_CALL amxx_get_flags(AMX *amx, cell *params)
{
    int flags = params[1];
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    int maxlen = params[3];

    char buf[32];
    int pos = 0;

    struct FlagEntry {
        int flag;
        char ch;
    };

    static const FlagEntry flagTable[] = {
        {FLAG_A, 'a'}, {FLAG_B, 'b'}, {FLAG_C, 'c'}, {FLAG_D, 'd'},
        {FLAG_E, 'e'}, {FLAG_F, 'f'}, {FLAG_G, 'g'}, {FLAG_H, 'h'},
        {FLAG_I, 'i'}, {FLAG_J, 'j'}, {FLAG_K, 'k'}, {FLAG_L, 'l'},
        {FLAG_M, 'm'}, {FLAG_N, 'n'}, {FLAG_O, 'o'}, {FLAG_P, 'p'},
        {FLAG_Q, 'q'}, {FLAG_R, 'r'}, {FLAG_S, 's'}, {FLAG_T, 't'},
        {FLAG_U, 'u'}, {FLAG_V, 'v'}, {FLAG_W, 'w'}, {FLAG_X, 'x'},
        {FLAG_Y, 'y'}, {FLAG_Z, 'z'},
    };

    for (int i = 0; i < 26 && pos < maxlen - 1; i++) {
        if (flags & flagTable[i].flag) {
            buf[pos++] = flagTable[i].ch;
        }
    }

    buf[pos] = '\0';
    amx_SetString(dest, buf, 0, 0, maxlen);
    return pos;
}

// amxx_admins_flush - Flush admin cache and reload
cell AMX_NATIVE_CALL amxx_admins_flush(AMX *amx, cell *params)
{
    (void)amx;
    int reload = 1;
    if (params[0] / (int)sizeof(cell) >= 2)
        reload = params[1];

    if (reload)
        AMXXAdminSystem::GetInstance().Init();

    return 1;
}

// flag_to_bit(flag) - 将单个标志字符转为位掩码: 'a' → 1, 'b' → 2, 'c' → 4, ...
cell AMX_NATIVE_CALL amxx_flag_to_bit(AMX *amx, cell *params)
{
    (void)amx;
    cell *addr;
    amx_GetAddr(amx, params[1], &addr);
    if (!addr) return 0;
    char c = (char)*addr;
    if (c >= 'a' && c <= 'z') return (1 << (c - 'a'));
    if (c >= 'A' && c <= 'Z') return (1 << (c - 'A'));
    return 0;
}

// bit_to_flag(bit, flag[]) - 将位掩码转为标志字符: 1 → 'a', 2 → 'b', 4 → 'c', ...
cell AMX_NATIVE_CALL amxx_bit_to_flag(AMX *amx, cell *params)
{
    int bit = (int)params[1];
    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    if (!addr) return 0;
    for (int i = 0; i < 26; i++) {
        if (bit & (1 << i)) {
            char c = 'a' + i;
            *addr = (cell)c;
            return 1;
        }
    }
    *addr = 0;
    return 0;
}

// strip_flags(flags, str[]) - 从字符串中去除对应位已设置的标志字符
cell AMX_NATIVE_CALL amxx_strip_flags(AMX *amx, cell *params)
{
    int flags = (int)params[1];
    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    if (!addr) return 0;

    char buf[256];
    amx_GetString(buf, addr, 0, sizeof(buf));

    char result[256];
    int pos = 0;
    for (int i = 0; buf[i]; i++) {
        char c = tolower((unsigned char)buf[i]);
        if (c >= 'a' && c <= 'z') {
            if (!(flags & (1 << (c - 'a'))))
                result[pos++] = buf[i];
        } else {
            result[pos++] = buf[i];
        }
    }
    result[pos] = '\0';
    amx_SetString(addr, result, 0, 0, 256);
    return pos;
}

// P2-7: 客户端/服务端命令查询 natives

// get_clcmd(index, command[], len1, &flags, info[], len2, flag, &bool:info_ml = false)
// 检索第 index 个客户端命令信息，返回 1 成功
cell AMX_NATIVE_CALL amxx_get_clcmd(AMX *amx, cell *params)
{
    int num_params = params[0] / sizeof(cell);
    int targetIndex = (int)params[1];
    int filterFlag = (num_params >= 7) ? (int)params[7] : 0;

    int curIndex = 0;
    auto &cmds = AMXXAdminSystem::GetInstance().GetCommands();
    for (auto &cmd : cmds) {
        if (cmd.cmdType != CMD_ClientCommand)
            continue;
        int cmdAccess = AdminFlagsStrToBitmask(cmd.flags.c_str());
        if (filterFlag != 0 && (cmdAccess & filterFlag) == 0)
            continue;

        if (curIndex == targetIndex) {
            cell *cmdAddr;
            amx_GetAddr(amx, params[2], &cmdAddr);
            amx_SetString(cmdAddr, cmd.cmd.c_str(), 0, 0, (int)params[3]);

            cell *flagsAddr;
            amx_GetAddr(amx, params[4], &flagsAddr);
            if (flagsAddr) *flagsAddr = (cell)cmdAccess;

            int infoLen = (num_params >= 6) ? (int)params[6] : 0;
            if (infoLen > 0) {
                cell *infoAddr;
                amx_GetAddr(amx, params[5], &infoAddr);
                if (infoAddr)
                    amx_SetString(infoAddr, cmd.description.c_str(), 0, 0, infoLen);
            }
            if (num_params >= 8) {
                cell *mlAddr;
                amx_GetAddr(amx, params[8], &mlAddr);
                if (mlAddr) *mlAddr = 0;
            }
            return 1;
        }
        curIndex++;
    }
    return 0;
}

// get_clcmdsnum(flag) - 返回匹配 flag 的客户端命令数量
cell AMX_NATIVE_CALL amxx_get_clcmdsnum(AMX *amx, cell *params)
{
    (void)amx;
    int filterFlag = (int)params[1];
    int count = 0;
    auto &cmds = AMXXAdminSystem::GetInstance().GetCommands();
    for (auto &cmd : cmds) {
        if (cmd.cmdType != CMD_ClientCommand)
            continue;
        int cmdAccess = AdminFlagsStrToBitmask(cmd.flags.c_str());
        if (filterFlag != 0 && (cmdAccess & filterFlag) == 0)
            continue;
        count++;
    }
    return count;
}

// get_srvcmd(index, server_cmd[], len1, &flags, info[], len2, flag, &bool:info_ml = false)
// 检索第 index 个服务端命令信息
cell AMX_NATIVE_CALL amxx_get_srvcmd(AMX *amx, cell *params)
{
    int num_params = params[0] / sizeof(cell);
    int targetIndex = (int)params[1];
    int filterFlag = (num_params >= 7) ? (int)params[7] : 0;

    int curIndex = 0;
    auto &cmds = AMXXAdminSystem::GetInstance().GetCommands();
    for (auto &cmd : cmds) {
        if (cmd.cmdType != CMD_ServerCommand)
            continue;
        int cmdAccess = AdminFlagsStrToBitmask(cmd.flags.c_str());
        if (filterFlag != 0 && (cmdAccess & filterFlag) == 0)
            continue;

        if (curIndex == targetIndex) {
            cell *cmdAddr;
            amx_GetAddr(amx, params[2], &cmdAddr);
            amx_SetString(cmdAddr, cmd.cmd.c_str(), 0, 0, (int)params[3]);

            cell *flagsAddr;
            amx_GetAddr(amx, params[4], &flagsAddr);
            if (flagsAddr) *flagsAddr = (cell)cmdAccess;

            int infoLen = (num_params >= 6) ? (int)params[6] : 0;
            if (infoLen > 0) {
                cell *infoAddr;
                amx_GetAddr(amx, params[5], &infoAddr);
                if (infoAddr)
                    amx_SetString(infoAddr, cmd.description.c_str(), 0, 0, infoLen);
            }
            if (num_params >= 8) {
                cell *mlAddr;
                amx_GetAddr(amx, params[8], &mlAddr);
                if (mlAddr) *mlAddr = 0;
            }
            return 1;
        }
        curIndex++;
    }
    return 0;
}

// get_srvcmdsnum(flag) - 返回匹配 flag 的服务端命令数量
cell AMX_NATIVE_CALL amxx_get_srvcmdsnum(AMX *amx, cell *params)
{
    (void)amx;
    int filterFlag = (int)params[1];
    int count = 0;
    auto &cmds = AMXXAdminSystem::GetInstance().GetCommands();
    for (auto &cmd : cmds) {
        if (cmd.cmdType != CMD_ServerCommand)
            continue;
        int cmdAccess = AdminFlagsStrToBitmask(cmd.flags.c_str());
        if (filterFlag != 0 && (cmdAccess & filterFlag) == 0)
            continue;
        count++;
    }
    return count;
}

AMX_NATIVE_INFO admin_natives[] = {
    {"register_clcmd", amxx_register_clcmd},
    {"register_srvcmd", amxx_register_srvcmd},
    {"register_concmd", amxx_register_concmd},
    {"access", amxx_access},
    {"get_user_flags", amxx_get_user_flags},
    {"set_user_flags", amxx_set_user_flags},
    {"cmd_access", amxx_cmd_access},
    {"admins_num", amxx_admins_num},
    {"admins_push", amxx_admins_push},
    {"admins_lookup", amxx_admins_lookup},
    {"admins_remove", amxx_admins_remove},
    {"reload_admins", amxx_reload_admins},
    {"get_user_auths", amxx_get_user_auths},
    {"is_user_admin", amxx_is_user_admin},
    {"remove_user_flags", amxx_remove_user_flags},
    {"has_rcon", amxx_has_rcon},
    {"get_concmd", amxx_get_concmd},
    {"get_concmdsnum", amxx_get_concmdsnum},
    {"get_concmd_plid", amxx_get_concmd_plid},
    {"get_clcmd", amxx_get_clcmd},
    {"get_clcmdsnum", amxx_get_clcmdsnum},
    {"get_srvcmd", amxx_get_srvcmd},
    {"get_srvcmdsnum", amxx_get_srvcmdsnum},
    {"read_flags", amxx_read_flags},
    {"get_flags", amxx_get_flags},
    {"admins_flush", amxx_admins_flush},
    {"flag_to_bit", amxx_flag_to_bit},
    {"bit_to_flag", amxx_bit_to_flag},
    {"strip_flags", amxx_strip_flags},
    {nullptr, nullptr}
};

void RegisterAdminNatives(AMX *amx)
{
    amx_Register(amx, admin_natives, -1);
}
