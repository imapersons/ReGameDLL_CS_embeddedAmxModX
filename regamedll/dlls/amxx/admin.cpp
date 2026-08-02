#include "precompiled.h"
#include "admin.h"
#include "amx.h"
#include "menus.h"
#include "forwards.h"
#include "../extdll.h"
#include "../enginecallback.h"
#include "../util.h"
#include "../player.h"
#include "runtime.h"  // For AMXXRuntime access
#include <cstring>
#include <cstdio>

// Forward declaration
static void AMXXServerCommandCallback(void);

AMXXAdminSystem &AMXXAdminSystem::GetInstance()
{
    static AMXXAdminSystem instance;
    return instance;
}

AMXXAdminSystem::AMXXAdminSystem()
    : m_nextCmdId(0)
{
}

void AMXXAdminSystem::Init()
{
    AMXX_LOG_DBG("[Admin] Initializing admin system...");
    LoadAdmins("addons/amxmodx/configs/admins.ini");
    
    // client_command forward is registered by AMXXRuntime, not here
    AMXX_LOG_DBG("[Admin] Admin system initialized");
}

// Helper: extract the next quoted field from a string (e.g. "value")
static char *ExtractQuotedField(char *&p)
{
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '"') return nullptr;
    p++;
    char *result = p;
    while (*p && *p != '"') p++;
    if (*p == '"') {
        *p = '\0';
        p++;
    }
    return result;
}

void AMXXAdminSystem::LoadAdmins(const char *filename)
{
    AMXX_LOG_DBG("[Admin] Loading admins from: %s", filename);
    m_admins.clear();
    
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        AMXX_LOG_DBG("[Admin] admins.ini not found, skipping");
        return;
    }
    
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }
        
        if (len == 0 || line[0] == ';' || line[0] == '#')
            continue;

        // ====== Format 1: original AMXX quoted format ======
        char tmpLine[1024];
        strncpy(tmpLine, line, sizeof(tmpLine) - 1);
        tmpLine[sizeof(tmpLine) - 1] = '\0';
        char *p = tmpLine;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '"') {
            char *f1 = ExtractQuotedField(p);
            char *f2 = ExtractQuotedField(p);
            char *f3 = ExtractQuotedField(p);
            char *f4 = ExtractQuotedField(p);
            
            if (f1 && f3) {
                AdminEntry entry;
                const char *authField = f1 ? f1 : "";
                const char *passwordField = f2 ? f2 : "";
                const char *accessField = f3 ? f3 : "";
                const char *accountFlags = f4 ? f4 : "";

                bool isSteamId = (strchr(accountFlags, 'c') != nullptr);
                bool isIpBased = (strchr(accountFlags, 'd') != nullptr);

                if (isSteamId) {
                    entry.authid = authField;
                } else if (isIpBased) {
                    entry.ip = authField;
                } else {
                    entry.name = authField;
                }
                entry.password = passwordField;
                entry.access = ParseFlags(accessField);
                entry.flags = ParseFlags(accountFlags);

                m_admins.push_back(entry);
                AMXX_LOG_DBG("[Admin] Loaded admin (AMXX fmt): name='%s' authid='%s' ip='%s' access=%d flags=%d acct='%s'",
                         entry.name.c_str(), entry.authid.c_str(), entry.ip.c_str(), entry.access, entry.flags, accountFlags);
                continue;
            }
        }

        // ====== Format 2: traditional colon-separated format ======
        char *name = line;
        char *password = strchr(line, ':');
        if (!password) continue;
        *password++ = '\0';
        
        char *authid = strchr(password, ':');
        if (!authid) continue;
        *authid++ = '\0';
        
        char *ip = strchr(authid, ':');
        if (!ip) continue;
        *ip++ = '\0';
        
        char *flags_str = strchr(ip, ':');
        if (!flags_str) continue;
        *flags_str++ = '\0';
        
        AdminEntry entry;
        entry.name = name;
        entry.password = password;
        entry.authid = authid;
        entry.ip = ip;
        entry.access = ParseFlags(flags_str);
        entry.flags = 0;

        m_admins.push_back(entry);
        AMXX_LOG_DBG("[Admin] Loaded admin (colon fmt): name='%s' authid='%s' access=%d",
                 name, entry.authid.c_str(), entry.access);
    }
    
    fclose(fp);
    AMXX_LOG_DBG("[Admin] Loaded %zu admins", m_admins.size());
}

int AMXXAdminSystem::FlagCharToFlag(const char c)
{
    switch (c) {
        case 'a': return FLAG_A;
        case 'b': return FLAG_B;
        case 'c': return FLAG_C;
        case 'd': return FLAG_D;
        case 'e': return FLAG_E;
        case 'f': return FLAG_F;
        case 'g': return FLAG_G;
        case 'h': return FLAG_H;
        case 'i': return FLAG_I;
        case 'j': return FLAG_J;
        case 'k': return FLAG_K;
        case 'l': return FLAG_L;
        case 'm': return FLAG_M;
        case 'n': return FLAG_N;
        case 'o': return FLAG_O;
        case 'p': return FLAG_P;
        case 'q': return FLAG_Q;
        case 'r': return FLAG_R;
        case 's': return FLAG_S;
        case 't': return FLAG_T;
        case 'u': return FLAG_U;
        case 'v': return FLAG_V;
        case 'w': return FLAG_W;
        case 'x': return FLAG_X;
        case 'y': return FLAG_Y;
        case 'z': return FLAG_Z;
        default: return 0;
    }
}

int AMXXAdminSystem::ParseFlags(const char *flags)
{
    int result = 0;
    if (!flags) return result;
    
    for (size_t i = 0; flags[i]; i++) {
        result |= FlagCharToFlag(tolower(flags[i]));
    }
    return result;
}

// ========== Command matching ==========
bool AMXXAdminSystem::MatchCommandLine(const RegisteredCommand &cmd, const char *name, const char *arg)
{
    if (stricmp(cmd.cmd.c_str(), name) != 0)
        return false;
    if (cmd.args.empty())
        return true;
    if (arg && *arg)
        return stricmp(cmd.args.c_str(), arg) == 0;
    return true;
}

// ========== Command prefix registration ==========
void AMXXAdminSystem::RegisterPrefix(const char *prefix)
{
    if (!prefix || !*prefix)
        return;
    for (auto &p : m_prefixes) {
        if (p.prefix == prefix)
            return;
    }
    CommandPrefix cp;
    cp.prefix = prefix;
    m_prefixes.push_back(cp);
    AMXX_LOG_DBG("[Admin] Registered command prefix: '%s'", prefix);
}

// ========== Native server command registration ==========
void AMXXAdminSystem::RegisterNativeServerCommand(const char *cmd, NativeServerCommandHandler handler, const char *flags)
{
    if (!cmd || !*cmd || !handler)
        return;
    for (auto &nc : m_nativeCommands) {
        if (stricmp(nc.cmd.c_str(), cmd) == 0)
            return;
    }
    NativeServerCommand nc;
    nc.cmd = cmd;
    nc.handler = handler;
    nc.flags = flags ? ParseFlags(flags) : 0;
    m_nativeCommands.push_back(nc);
    AMXX_LOG_DBG("[Admin] Registered native server command: '%s' (flags=%d)", cmd, nc.flags);
}

// ========== Command registration ==========
int AMXXAdminSystem::RegisterCommand(AMX *amx, const char *cmd, cell funcidx, const char *flags, const char *description)
{
    return RegisterClientCommand(amx, cmd, funcidx, flags, description);
}

int AMXXAdminSystem::RegisterClientCommand(AMX *amx, const char *cmd, cell funcidx, const char *flags, const char *description)
{
    RegisteredCommand rc;
    char cmdName[64], argPattern[64];
    *cmdName = *argPattern = 0;
    sscanf(cmd, "%s %s", cmdName, argPattern);
    rc.cmd = cmdName;
    rc.args = argPattern;
    rc.amx = amx;
    rc.funcidx = funcidx;
    rc.flags = flags ? flags : "";
    rc.description = description ? description : "";
    rc.cmdType = CMD_ClientCommand;
    rc.id = --m_nextCmdId;
    m_commands.push_back(rc);
    printf("[AMXX] [Admin] RegisterClientCommand: '%s' (type=CMD_ClientCommand, flags='%s', id=%d)\n",
             cmdName, flags ? flags : "", rc.id);
    AMXX_LOG_DBG("[Admin] Registered client command: '%s' (args='%s', flags='%s', id=%d)",
             cmdName, argPattern, flags ? flags : "", rc.id);
    return rc.id;
}

int AMXXAdminSystem::RegisterServerCommand(AMX *amx, const char *cmd, cell funcidx, const char *flags, const char *description)
{
    RegisteredCommand rc;
    char cmdName[64], argPattern[64];
    *cmdName = *argPattern = 0;
    sscanf(cmd, "%s %s", cmdName, argPattern);
    rc.cmd = cmdName;
    rc.args = argPattern;
    rc.amx = amx;
    rc.funcidx = funcidx;
    rc.flags = flags ? flags : "";
    rc.description = description ? description : "";
    rc.cmdType = CMD_ServerCommand;
    rc.id = --m_nextCmdId;
    m_commands.push_back(rc);
    if (m_serverCmdRegistered.find(cmdName) == m_serverCmdRegistered.end()) {
        m_serverCmdRegistered.insert(cmdName);
        g_engfuncs.pfnAddServerCommand((char*)cmdName, AMXXServerCommandCallback);
    }
    printf("[AMXX] [Admin] Registered server command: '%s' (id=%d, total=%zu)\n", cmdName, rc.id, m_commands.size());
    return rc.id;
}

int AMXXAdminSystem::RegisterConsoleCommand(AMX *amx, const char *cmd, cell funcidx, const char *flags, const char *description)
{
    RegisteredCommand rc;
    char cmdName[64], argPattern[64];
    *cmdName = *argPattern = 0;
    sscanf(cmd, "%s %s", cmdName, argPattern);
    rc.cmd = cmdName;
    rc.args = argPattern;
    rc.amx = amx;
    rc.funcidx = funcidx;
    rc.flags = flags ? flags : "";
    rc.description = description ? description : "";
    rc.cmdType = CMD_ConsoleCommand;
    rc.id = --m_nextCmdId;
    m_commands.push_back(rc);
    printf("[AMXX] [Admin] RegisterConsoleCommand: '%s' (type=CMD_ConsoleCommand, flags='%s', id=%d)\n",
             cmdName, flags ? flags : "", rc.id);
    if (m_serverCmdRegistered.find(cmdName) == m_serverCmdRegistered.end()) {
        m_serverCmdRegistered.insert(cmdName);
        g_engfuncs.pfnAddServerCommand((char*)cmdName, AMXXServerCommandCallback);
        printf("[AMXX] [Admin] RegisterConsoleCommand: also registered as server cmd: '%s'\n", cmdName);
    }
    AMXX_LOG_DBG("[Admin] Registered console command: '%s' (id=%d)", cmdName, rc.id);
    return rc.id;
}

// ========== Command lookup ==========
RegisteredCommand *AMXXAdminSystem::FindCommand(const char *cmd, const char *arg, int cmdType)
{
    for (auto &prefix : m_prefixes) {
        if (strncmp(prefix.prefix.c_str(), cmd, prefix.prefix.length()) == 0) {
            for (auto &rc : m_commands) {
                if (rc.cmdType == cmdType && MatchCommandLine(rc, cmd, arg))
                    return &rc;
            }
        }
    }
    for (auto &rc : m_commands) {
        if (rc.cmdType == cmdType && MatchCommandLine(rc, cmd, arg))
            return &rc;
    }
    return nullptr;
}

// ========== client_command forward execution ==========
int AMXXAdminSystem::ExecuteClientCommandForward(edict_t *pEntity, const char *cmd)
{
    int playerId = pEntity ? ENTINDEX(pEntity) : 0;
    ForwardCallParam params[1];
    params[0].type = FP_CELL;
    params[0].val = playerId;
    // Invoke the forward managed by AMXXRuntime so plugin callbacks are executed
    return AMXXRuntime::GetInstance().ExecuteForwardEx("client_command", 1, params);
}

// ========== Server command callback ==========
static void AMXXServerCommandCallback(void)
{
    const char *cmdName = CMD_ARGV(0);
    if (!cmdName || !*cmdName)
        return;
    int argc = CMD_ARGC();
    printf("[AMXX] [Admin] AMXXServerCommandCallback ENTER: cmd='%s' argc=%d\n", cmdName, argc);

    AMXXAdminSystem &admin = AMXXAdminSystem::GetInstance();
    const char **argv = new const char*[argc];
    for (int i = 0; i < argc; i++) {
        argv[i] = CMD_ARGV(i);
    }
    admin.SaveCommandArgs(argc, argv);
    delete[] argv;

    bool handled = admin.ExecuteServerCommand(cmdName);
    if (!handled) {
        edict_t *pHost = INDEXENT(1);
        if (pHost && !pHost->free && (pHost->v.flags & FL_CLIENT)) {
            printf("[AMXX] [Admin] AMXXServerCommandCallback: forwarding '%s' to host player\n", cmdName);
            admin.ExecuteCommand(cmdName, pHost);
        }
    }
}

bool AMXXAdminSystem::ExecuteServerCommand(const char *cmd)
{
    const char *arg = GetSavedArgv(1);
    if (!arg) arg = CMD_ARGV_(1);
    RegisteredCommand *rc = FindCommand(cmd, arg, CMD_ServerCommand);
    if (!rc) rc = FindCommand(cmd, arg, CMD_ClientCommand);
    if (!rc) rc = FindCommand(cmd, arg, CMD_ConsoleCommand);
    if (!rc) {
        for (auto &nc : m_nativeCommands) {
            if (stricmp(nc.cmd.c_str(), cmd) == 0) {
                printf("[AMXX] [Admin] ExecuteServerCommand: native cmd='%s'\n", cmd);
                nc.handler(0);
                return true;
            }
        }
        return false;
    }

    printf("[AMXX] [Admin] ExecuteServerCommand: found '%s' (type=%d, id=%d, funcidx=%d)\n", cmd, rc->cmdType, rc->id, rc->funcidx);
    AMX *amx = rc->amx;
    cell retval = 0;
    int requiredFlags = ParseFlags(rc->flags.c_str());
    int serverId = IS_DEDICATED_SERVER() ? 0 : 1;
    amx_Push(amx, rc->id);
    amx_Push(amx, requiredFlags);
    amx_Push(amx, serverId);
    int err = amx_Exec(amx, &retval, rc->funcidx);
    if (err != AMX_ERR_NONE) {
        printf("[AMXX] [Admin] ExecuteServerCommand: %s execution FAILED: error %d\n", cmd, err);
    }
    if (retval == 0) {
        return false;
    }
    printf("[AMXX] [Admin] ExecuteServerCommand: %s done, retval=%d\n", cmd, retval);
    return true;
}

// ========== Client command execution ==========
bool AMXXAdminSystem::ExecuteCommand(const char *cmd, edict_t *pEntity)
{
    int playerId = pEntity ? ENTINDEX(pEntity) : 0;
    printf("[AMXX] [Admin] ExecuteCommand ENTER: cmd='%s' player=%d\n", cmd, playerId);

    // 1. client_command forward
    int fwdResult = ExecuteClientCommandForward(pEntity, cmd);
    if (fwdResult == PLUGIN_HANDLED || fwdResult == PLUGIN_STOP) {
        printf("[AMXX] ExecuteCommand: '%s' intercepted by client_command forward\n", cmd);
        return true;
    }

    // 2. menuselect
    if (strcmp(cmd, "menuselect") == 0) {
        const char *arg = CMD_ARGV_(1);
        if (arg && *arg) {
            int key = atoi(arg);
            // menuselect sends 1-10, where 10 = exit key (slot 0)
            // Convert to the AMXX standard: 0=exit, 1-9=menu items
            if (key == 10) key = 0;
            AMXXMenuSystem::GetInstance().OnMenuSelect(playerId, key);
        }
        return true;
    }

    const char *arg = CMD_ARGV_(1);
    RegisteredCommand *rc = nullptr;

    // 3. Lookup the command, preferring the one the player can access (flags=0 or satisfied)
    //    Avoids a flagged variant of the same name blocking the unrestricted one
    auto findAllCmds = [&](int cmdType) -> void {
        for (auto &prefix : m_prefixes) {
            if (strncmp(prefix.prefix.c_str(), cmd, prefix.prefix.length()) == 0) {
                for (auto &c : m_commands) {
                    if (c.cmdType == cmdType && MatchCommandLine(c, cmd, arg)) {
                        int rf = ParseFlags(c.flags.c_str());
                        if (rf == 0 || CheckAccess(playerId, rf)) {
                            rc = &c;
                            return;
                        }
                    }
                }
            }
        }
        for (auto &c : m_commands) {
            if (c.cmdType == cmdType && MatchCommandLine(c, cmd, arg)) {
                int rf = ParseFlags(c.flags.c_str());
                if (rf == 0 || CheckAccess(playerId, rf)) {
                    rc = &c;
                    return;
                }
            }
        }
    };

    if (playerId > 0) {
        if (!IS_DEDICATED_SERVER() && playerId == 1) {
            findAllCmds(CMD_ServerCommand);
            if (!rc) findAllCmds(CMD_ClientCommand);
            if (!rc) findAllCmds(CMD_ConsoleCommand);
        } else {
            findAllCmds(CMD_ClientCommand);
            if (!rc) findAllCmds(CMD_ConsoleCommand);
        }
    } else {
        findAllCmds(CMD_ServerCommand);
        if (!rc) findAllCmds(CMD_ConsoleCommand);
    }

    // 4. Execute the registered command
    if (rc) {
        int requiredFlags = ParseFlags(rc->flags.c_str());
        // Access was already checked during lookup; no need to deny again here
        printf("[AMXX] ExecuteCommand: executing '%s' (type=%d, flags=%d)\n", cmd, rc->cmdType, requiredFlags);
        int argc = CMD_ARGC();
        const char **argv = new const char*[argc];
        for (int i = 0; i < argc; i++) argv[i] = CMD_ARGV(i);
        SaveCommandArgs(argc, argv);
        delete[] argv;
        AMX *amx = rc->amx;
        cell retval;
        amx_Push(amx, rc->id);
        amx_Push(amx, requiredFlags);
        amx_Push(amx, playerId);
        amx_Exec(amx, &retval, rc->funcidx);
        return retval != 0;
    }

    // 5. Native server commands
    bool nativeAllowed = (playerId == 0) || (!IS_DEDICATED_SERVER() && playerId == 1);
    if (nativeAllowed) {
        for (auto &nc : m_nativeCommands) {
            if (stricmp(nc.cmd.c_str(), cmd) == 0) {
                if (nc.flags != 0 && !CheckAccess(playerId, nc.flags)) {
                    printf("[AMXX] Native cmd '%s' denied for player %d\n", cmd, playerId);
                    return true;
                }
                printf("[AMXX] Native cmd '%s' executed by %d\n", cmd, playerId);
                nc.handler(playerId);
                return true;
            }
        }
    } else {
        for (auto &nc : m_nativeCommands) {
            if (stricmp(nc.cmd.c_str(), cmd) == 0) {
                printf("[AMXX] Player %d DENIED native server cmd '%s'\n", playerId, cmd);
                return true;
            }
        }
    }

    printf("[AMXX] ExecuteCommand EXIT: '%s' NOT FOUND (player=%d)\n", cmd, playerId);
    return false;
}

// ========== Permission system ==========
int AMXXAdminSystem::GetUserFlags(int playerId)
{
    auto it = m_userFlags.find(playerId);
    if (it != m_userFlags.end()) {
        int flags = it->second;
        if (flags & (ADMIN_ADMIN | ADMIN_USER))
            flags = (1 << 26) - 1;
        return flags;
    }

    if (playerId == 0) {
        printf("[AMXX] [GetUserFlags] player=%d path=console ADMIN_ALL_FLAGS\n", playerId);
        return ADMIN_ALL_FLAGS;
    }

    if (playerId < 1 || playerId > gpGlobals->maxClients) {
        printf("[AMXX] [GetUserFlags] player=%d path=invalid\n", playerId);
        return 0;
    }
    
    edict_t *pEdict = INDEXENT(playerId);
    if (!pEdict || !pEdict->pvPrivateData) {
        printf("[AMXX] [GetUserFlags] player=%d path=no_edict\n", playerId);
        return 0;
    }
    
    char *infobuffer = g_engfuncs.pfnGetInfoKeyBuffer(pEdict);
    // Get authid via the engine function, not InfoKeyValue("authid")
    const char *authid = g_engfuncs.pfnGetPlayerAuthId(pEdict);
    const char *name = g_engfuncs.pfnInfoKeyValue(infobuffer, "name");

    // Get player IP: prefer the engine SV_GetClientIDString or read directly from netadr
    const char *ip = g_engfuncs.pfnInfoKeyValue(infobuffer, "ip");
    if (!ip || !*ip) {
        // Fallback: read via the "*ip" key
        ip = g_engfuncs.pfnInfoKeyValue(infobuffer, "*ip");
    }
    printf("[AMXX] [GetUserFlags] player=%d authid='%s' ip='%s' name='%s'\n",
           playerId, authid ? authid : "(null)", ip ? ip : "(null)", name ? name : "(null)");
    
    if (ip && strcmp(ip, "127.0.0.1") == 0) {
        AMXX_LOG_DBG("[Admin] Loopback player %d (ip=%s) auto-granted all admin flags", playerId, ip);
        printf("[AMXX] [GetUserFlags] player=%d path=loopback ADMIN_ALL_FLAGS\n", playerId);
        return ADMIN_ALL_FLAGS;
    }
    if (authid && (strcmp(authid, "STEAM_ID_LAN") == 0 || strcmp(authid, "STEAM_ID_PENDING") == 0)) {
        AMXX_LOG_DBG("[Admin] LAN player %d (authid=%s) auto-granted all admin flags", playerId, authid);
        printf("[AMXX] [GetUserFlags] player=%d path=lan ADMIN_ALL_FLAGS\n", playerId);
        return ADMIN_ALL_FLAGS;
    }
    
    for (auto &entry : m_admins) {
        bool match = false;
        if (!entry.authid.empty() && authid && strcmp(entry.authid.c_str(), authid) == 0)
            match = true;
        if (!match && !entry.ip.empty() && ip && strstr(ip, entry.ip.c_str()))
            match = true;
        if (!match && !entry.name.empty() && name && strcmp(entry.name.c_str(), name) == 0)
            match = true;

        if (!match)
            continue;

        if (!entry.password.empty()) {
            const char *pw = g_engfuncs.pfnInfoKeyValue(infobuffer, "_pw");
            if (!pw || strcmp(pw, entry.password.c_str()) != 0) {
                continue;
            }
        }

        return entry.access;
    }

    printf("[AMXX] [GetUserFlags] player=%d path=default 0\n", playerId);
    return 0;
}

void AMXXAdminSystem::SetUserFlags(int playerId, int flags)
{
    m_userFlags[playerId] |= flags;
    AMXX_LOG_DBG("[Admin] SetUserFlags: player %d |= %d (now %d)", playerId, flags, m_userFlags[playerId]);
}

void AMXXAdminSystem::RemoveFlags(int playerId, int flags)
{
    auto it = m_userFlags.find(playerId);
    if (it != m_userFlags.end()) {
        it->second &= ~flags;
        AMXX_LOG_DBG("[Admin] RemoveFlags: player %d flags removed %d, remaining %d", playerId, flags, it->second);
    }
}

bool AMXXAdminSystem::IsAdmin(int playerId)
{
    int flags = GetUserFlags(playerId);
    return flags != 0;
}

bool AMXXAdminSystem::CheckAccess(int playerId, int flags)
{
    int userFlags = GetUserFlags(playerId);
    return (userFlags & flags) != 0;
}

int AMXXAdminSystem::CmdAccess(int playerId, int flags, const char *cmd, bool silent)
{
    if (playerId == 0)
        return 1;
    if (playerId < 1 || playerId > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(playerId);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;
    int userFlags = GetUserFlags(playerId);
    if ((userFlags & flags) != 0)
        return 1;
    if (!silent) {
        char msg[256];
        snprintf(msg, sizeof(msg), "[AMXX] Access denied (requires flag %d, you have %d)", flags, userFlags);
        CLIENT_PRINTF(pEdict, print_console, msg);
    }
    return 0;
}

const AdminEntry *AMXXAdminSystem::GetAdmin(int index) const
{
    if (index < 0 || index >= (int)m_admins.size())
        return nullptr;
    return &m_admins[index];
}

bool AMXXAdminSystem::MatchAuth(const AdminEntry &entry, const char *authid, const char *ip, const char *name) const
{
    if (!entry.authid.empty() && authid && strcmp(entry.authid.c_str(), authid) == 0)
        return true;
    if (!entry.ip.empty() && ip && strstr(ip, entry.ip.c_str()))
        return true;
    if (!entry.name.empty() && name && strcmp(entry.name.c_str(), name) == 0)
        return true;
    return false;
}

int AMXXAdminSystem::FindAdmin(const char *authid, const char *ip, const char *name) const
{
    for (size_t i = 0; i < m_admins.size(); i++) {
        if (MatchAuth(m_admins[i], authid, ip, name))
            return (int)i;
    }
    return -1;
}

void AMXXAdminSystem::PushAdmin(const char *authData, const char *password, int access, int flags)
{
    AdminEntry entry;
    entry.password = password ? password : "";
    entry.access = access;
    entry.flags = flags;

    // Determine auth type from FLAG_* bits (matching original AMXX behavior)
    // FLAG_C (1<<2) = "c" = auth by steamid
    // FLAG_D (1<<3) = "d" = auth by IP
    // FLAG_A (1<<0) = "a" = auth by name (default if no c/d)
    if (flags & FLAG_C) {
        entry.authid = authData ? authData : "";
    } else if (flags & FLAG_D) {
        entry.ip = authData ? authData : "";
    } else {
        entry.name = authData ? authData : "";
    }

    m_admins.push_back(entry);
    AMXX_LOG_DBG("[Admin] Push admin: auth='%s' (name='%s' authid='%s' ip='%s') access=%d flags=%d",
                 authData ? authData : "", entry.name.c_str(), entry.authid.c_str(), entry.ip.c_str(), entry.access, entry.flags);
}

bool AMXXAdminSystem::RemoveAdmin(int index)
{
    if (index < 0 || index >= (int)m_admins.size())
        return false;
    m_admins.erase(m_admins.begin() + index);
    AMXX_LOG_DBG("[Admin] Removed admin at index %d", index);
    return true;
}

void AMXXAdminSystem::ReloadAdmins()
{
    LoadAdmins("addons/amxmodx/configs/admins.ini");
}

void AMXXAdminSystem::SetUserAuthorized(int playerId, bool authorized)
{
    if (playerId < 1 || playerId > gpGlobals->maxClients)
        return;
    if ((int)m_authorized.size() <= playerId)
        m_authorized.resize(playerId + 1, false);
    m_authorized[playerId] = authorized;
    AMXX_LOG_DBG("[Admin] Player %d authorized = %s", playerId, authorized ? "true" : "false");
}

bool AMXXAdminSystem::IsUserAuthorized(int playerId) const
{
    if (playerId < 1 || playerId >= (int)m_authorized.size())
        return false;
    return m_authorized[playerId];
}

// ========== Command argument saving ==========
void AMXXAdminSystem::SaveCommandArgs(int argc, const char **argv)
{
    m_currentCmdArgc = argc;
    m_currentCmdArgs.clear();
    for (int i = 0; i < argc; i++) {
        m_currentCmdArgs.push_back(argv[i] ? argv[i] : "");
    }
}

const char *AMXXAdminSystem::GetSavedArgv(int index) const
{
    if (index >= 0 && index < (int)m_currentCmdArgs.size()) {
        return m_currentCmdArgs[index].c_str();
    }
    return nullptr;
}

int AMXXAdminSystem::GetCommandCount(int cmdType) const
{
    int count = 0;
    for (auto &rc : m_commands) {
        if (rc.cmdType == cmdType)
            count++;
    }
    return count;
}

void AMXXAdminSystem::Clear()
{
    m_commands.clear();
    m_prefixes.clear();
    m_userFlags.clear();
    m_currentCmdArgs.clear();
    m_currentCmdArgc = 0;
    m_nextCmdId = 0;
    m_nativeCommands.clear();
    m_serverCmdRegistered.clear();
    AMXX_LOG_DBG("[Admin] Admin system cleared");
}