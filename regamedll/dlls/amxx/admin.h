#pragma once
#ifndef AMXX_ADMIN_H
#define AMXX_ADMIN_H

#include "amx.h"
#include "forwards.h"
#include <string>
#include <vector>
#include <map>
#include <set>

// ADMIN_ALL = 0 means "no access required, accessible to everyone"
#define ADMIN_ALL		0
// ADMIN_ALL_FLAGS: all flags combined; grants console/local players full access
#define ADMIN_ALL_FLAGS		(ADMIN_IMMUNITY|ADMIN_RESERVATION|ADMIN_KICK|ADMIN_BAN|ADMIN_SLAY|ADMIN_MAP|ADMIN_CVAR|ADMIN_CONFIG|ADMIN_CHAT|ADMIN_VOTE|ADMIN_PASSWORD|ADMIN_RCON|ADMIN_CHEATS|ADMIN_LEVEL_B|ADMIN_LEVEL_C|ADMIN_LEVEL_D|ADMIN_LEVEL_E|ADMIN_LEVEL_F|ADMIN_LEVEL_G|ADMIN_LEVEL_H|ADMIN_MENU|ADMIN_BAN_TEMP|ADMIN_ADMIN|ADMIN_USER)
#define ADMIN_IMMUNITY		(1<<0)   // flag "a"
#define ADMIN_RESERVATION	(1<<1)   // flag "b"
#define ADMIN_KICK		(1<<2)   // flag "c"
#define ADMIN_BAN		(1<<3)   // flag "d"
#define ADMIN_SLAY		(1<<4)   // flag "e"
#define ADMIN_MAP		(1<<5)   // flag "f"
#define ADMIN_CVAR		(1<<6)   // flag "g"
#define ADMIN_CONFIG		(1<<7)   // flag "h"
#define ADMIN_CHAT		(1<<8)   // flag "i"
#define ADMIN_VOTE		(1<<9)   // flag "j"
#define ADMIN_PASSWORD		(1<<10)  // flag "k"
#define ADMIN_RCON		(1<<11)  // flag "l"
#define ADMIN_CHEATS		(1<<12)  // flag "m"

// PAWN-side extended flags (used by get_user_flags bit checks on the PAWN side)
// Note: ADMIN_LEVEL_A and ADMIN_CHEATS share bit (1<<12) by design
#define ADMIN_LEVEL_A		(1<<12)  // flag "m" — shared with ADMIN_CHEATS
#define ADMIN_LEVEL_B		(1<<13)  // flag "n"
#define ADMIN_LEVEL_C		(1<<14)  // flag "o"
#define ADMIN_LEVEL_D		(1<<15)  // flag "p"
#define ADMIN_LEVEL_E		(1<<16)  // flag "q"
#define ADMIN_LEVEL_F		(1<<17)  // flag "r"
#define ADMIN_LEVEL_G		(1<<18)  // flag "s"
#define ADMIN_LEVEL_H		(1<<19)  // flag "t"
#define ADMIN_MENU		(1<<20)  // flag "u"
#define ADMIN_BAN_TEMP		(1<<21)  // flag "v"
#define ADMIN_ADMIN		(1<<24)  // flag "y"
#define ADMIN_USER		(1<<25)  // flag "z"

#define FLAG_A		ADMIN_IMMUNITY
#define FLAG_B		ADMIN_RESERVATION
#define FLAG_C		ADMIN_KICK
#define FLAG_D		ADMIN_BAN
#define FLAG_E		ADMIN_SLAY
#define FLAG_F		ADMIN_MAP
#define FLAG_G		ADMIN_CVAR
#define FLAG_H		ADMIN_CONFIG
#define FLAG_I		ADMIN_CHAT
#define FLAG_J		ADMIN_VOTE
#define FLAG_K		ADMIN_PASSWORD
#define FLAG_L		ADMIN_RCON
#define FLAG_M		ADMIN_CHEATS
#define FLAG_N		ADMIN_LEVEL_B
#define FLAG_O		ADMIN_LEVEL_C
#define FLAG_P		ADMIN_LEVEL_D
#define FLAG_Q		ADMIN_LEVEL_E
#define FLAG_R		ADMIN_LEVEL_F
#define FLAG_S		ADMIN_LEVEL_G
#define FLAG_T		ADMIN_LEVEL_H
#define FLAG_U		ADMIN_MENU
#define FLAG_V		ADMIN_BAN_TEMP
#define FLAG_W		(1<<22)
#define FLAG_X		(1<<23)
#define FLAG_Y		ADMIN_ADMIN
#define FLAG_Z		ADMIN_USER

// Command types
enum CommandType {
    CMD_ConsoleCommand,
    CMD_ClientCommand,
    CMD_ServerCommand
};

// AdminProp enum - matches original AMXX amxconst.inc
enum AdminProp {
    AdminProp_Auth = 0,       // Auth data string (steamid/ip/name)
    AdminProp_Password = 1,    // Password string
    AdminProp_Access = 2,      // Access flags (ADMIN_*)
    AdminProp_Flags = 3,       // Auth behavior flags (FLAG_*)
};

struct AdminEntry {
    std::string name;
    std::string password;
    std::string authid;
    std::string ip;
    int access;      // ADMIN_* flags (what the admin can do)
    int flags;       // FLAG_* auth behavior flags (how to authenticate: name/steamid/ip)

    AdminEntry() : access(0), flags(0) {}
};

// Command prefix struct - supports prefix matching such as "amx_*"
struct CommandPrefix {
    std::string prefix;
    std::vector<std::string> commandIds;
};

// Native server command callback type (implemented in C++, not an AMXX plugin callback)
// playerId > 0 means triggered by a player; output should go to the player's console
// CMD_ARGV / CMD_ARGC are already filled by the engine on both call paths
typedef void (*NativeServerCommandHandler)(int playerId);

struct NativeServerCommand {
    std::string cmd;
    int flags;                       // required access flag mask; 0 = no access required
    NativeServerCommandHandler handler;
};

struct RegisteredCommand {
    std::string cmd;
    std::string args;    // optional argument match pattern, e.g. "<name>"
    AMX *amx;
    cell funcidx;
    std::string flags;
    std::string description;
    int cmdType;        // Console/Client/Server
    int id;             // unique ID
    
    RegisteredCommand() : amx(nullptr), funcidx(-1), cmdType(CMD_ClientCommand), id(0) {}
};

class AMXXAdminSystem {
public:
    static AMXXAdminSystem &GetInstance();

    void Init();
    void LoadAdmins(const char *filename);
    
    // Command registration - supports type distinction and prefixes
    int RegisterCommand(AMX *amx, const char *cmd, cell funcidx, const char *flags, const char *description);
    int RegisterClientCommand(AMX *amx, const char *cmd, cell funcidx, const char *flags, const char *description);
    int RegisterServerCommand(AMX *amx, const char *cmd, cell funcidx, const char *flags, const char *description);
    int RegisterConsoleCommand(AMX *amx, const char *cmd, cell funcidx, const char *flags, const char *description);
    
    // Command prefix registration
    void RegisterPrefix(const char *prefix);
    
    // Native server command registration (C++ implemented commands, e.g. "amxx plugins")
    void RegisterNativeServerCommand(const char *cmd, NativeServerCommandHandler handler, const char *flags);
    
    // Command execution
    bool ExecuteServerCommand(const char *cmd);
    bool ExecuteCommand(const char *cmd, edict_t *pEntity);
    
    // client_command forward (plugins may hook this event to intercept commands)
    void RegisterClientCommandForward(int forwardId);
    int ExecuteClientCommandForward(edict_t *pEntity, const char *cmd);
    
    // Command lookup (supports prefix + argument matching)
    RegisteredCommand *FindCommand(const char *cmd, const char *arg, int cmdType);
    
    // Access checks
    int GetUserFlags(int playerId);
    void SetUserFlags(int playerId, int flags);
    void RemoveFlags(int playerId, int flags);
    bool IsAdmin(int playerId);
    bool CheckAccess(int playerId, int flags);
    int CmdAccess(int playerId, int flags, const char *cmd, bool silent);

    int GetAdminsNum() const { return (int)m_admins.size(); }
    const AdminEntry *GetAdmin(int index) const;
    int FindAdmin(const char *authid, const char *ip, const char *name) const;
    void PushAdmin(const char *authData, const char *password, int access, int flags);
    bool RemoveAdmin(int index);
    void ReloadAdmins();

    void SetUserAuthorized(int playerId, bool authorized);
    bool IsUserAuthorized(int playerId) const;
    
    // Command argument save/restore (prevents overwrite during async command execution)
    void SaveCommandArgs(int argc, const char **argv);
    int GetSavedArgc() const { return m_currentCmdArgc; }
    const char *GetSavedArgv(int index) const;
    
    // Get command list (used by get_cvar_flags, etc.)
    std::vector<RegisteredCommand> &GetCommands() { return m_commands; }
    int GetCommandCount(int cmdType) const;
    
    void Clear();

private:
    AMXXAdminSystem();

    std::vector<AdminEntry> m_admins;
    std::vector<RegisteredCommand> m_commands;
    std::vector<NativeServerCommand> m_nativeCommands;
    std::set<std::string> m_serverCmdRegistered;  // command names already registered via pfnAddServerCommand
    std::vector<CommandPrefix> m_prefixes;
    std::vector<bool> m_authorized;
    std::map<int, int> m_userFlags;   // dynamic per-player flag overrides (set by set_user_flags)
    
    // client_command forward ID (plugins can intercept commands through it)
    int m_clientCommandForwardId;
    
    // 命令参数保存（用于解决异步命令执行时参数被覆盖的问题）
    std::vector<std::string> m_currentCmdArgs;
    int m_currentCmdArgc;
    
    // 命令 ID 计数器
    int m_nextCmdId;

    int FlagCharToFlag(const char c);
    int ParseFlags(const char *flags);
    bool MatchAuth(const AdminEntry &entry, const char *authid, const char *ip, const char *name) const;
    static bool MatchCommandLine(const RegisteredCommand &cmd, const char *name, const char *arg);
};

#endif