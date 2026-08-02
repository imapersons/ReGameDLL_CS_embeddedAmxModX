#include "precompiled.h"
#include "events.h"
#include "forwards.h"
#include "messages.h"
#include "plugin.h"
#include "runtime.h"
#include "native_core.h"
#include "../enginecallback.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

// ========== Global instance ==========
AMXXEventSystem &AMXXEventSystem::GetInstance()
{
    static AMXXEventSystem instance;
    return instance;
}

AMXXEventSystem::AMXXEventSystem()
    : m_parsePos(0), m_readPos(-1), m_parseMsgType(-1), m_readMsgType(-1), m_isMessageEvent(false),
      m_mapStamp(1), m_inExecute(0), m_inLogExecute(0), m_nextEventHandle(1000)
{
}

void AMXXEventSystem::Init()
{
    AMXX_LOG_DBG("[Events] Event system initialized");
}

// ========== Event flag parsing ==========
int AMXXEventSystem::ParseEventFlags(const char *flagStr)
{
    int flags = 0;
    if (!flagStr)
        return flags;

    for (const char *p = flagStr; *p; p++) {
        switch (*p) {
            case 'a': flags |= EVTFLAG_WORLD; break;
            case 'b': flags |= EVTFLAG_CLIENT; break;
            case 'c': flags |= EVTFLAG_ONCE; break;
            case 'd': flags |= EVTFLAG_DEAD; break;
            case 'e': flags |= EVTFLAG_ALIVE; break;
            case 'f': flags |= EVTFLAG_PLAYER; break;
            case 'g': flags |= EVTFLAG_BOT; break;
        }
    }
    return flags;
}

int UTIL_ReadEventFlags(const char *str)
{
    return AMXXEventSystem::ParseEventFlags(str);
}

// ========== Dynamic message ID lookup ==========
int AMXXEventSystem::GetMsgIdByName(const char *name)
{
    if (!name || !*name)
        return -1;

    // Check cache first
    auto it = m_msgIdCache.find(name);
    if (it != m_msgIdCache.end())
        return it->second;

    int msgId = -1;

    // All-digit event name: used directly as the message ID (original getEventId: atoi(msg) != 0 -> return pos)
    {
        bool allDigits = true;
        for (const char *p = name; *p; ++p) {
            if (!isdigit((unsigned char)*p)) { allDigits = false; break; }
        }
        if (allDigits) {
            int n = atoi(name);
            if (n != 0) {
                m_msgIdCache[name] = n;
                return n;
            }
        }
    }

    // Map the virtual event name "CS_DeathMsg" to the real message "DeathMsg"
    const char *queryName = name;
    char csDeathBuf[32];
    if (strcmp(name, "CS_DeathMsg") == 0) {
        strncpy(csDeathBuf, "DeathMsg", sizeof(csDeathBuf) - 1);
        csDeathBuf[sizeof(csDeathBuf) - 1] = '\0';
        queryName = csDeathBuf;
    }

    // Preferred: query dynamically through the engine's pfnRegUserMsg.
    // REG_USER_MSG(name, 0) returns the existing ID of a registered message (no double-registration),
    // matching get_user_msgid so engine-assigned IDs are handled correctly.
    if (queryName[0] && !isdigit((unsigned char)queryName[0])) {
        int engineId = REG_USER_MSG(const_cast<char *>(queryName), 0);
        if (engineId > 0)
            msgId = engineId;
    }

    // Fallback: if the engine API returned no valid ID, use a hardcoded table as a last resort.
    // Note: these are CS 1.6 defaults and may differ across mods/engine versions.
    if (msgId < 0) {
        static struct { const char *name; int id; } commonMsgs[] = {
            {"CurWeapon", 66},
            {"Damage", 102},
            {"DeathMsg", 107},
            {"TeamScore", 114},
            {"ScoreAttrib", 116},
            {"TextMsg", 76},
            {"SayText", 75},
            {"SendAudio", 77},
            {"ItemPickup", 124},
            {"Health", 122},
            {"Battery", 123},
            {"AmmoX", 126},
            {"AmmoPickup", 125},
            {"ResetHUD", 79},
            {"RoundTime", 101},
            {"Money", 120},
            {"StatusIcon", 105},
            {"StatusValue", 106},
            {"BarTime", 98},
            {"Scenario", 94},
            {"ClCorpse", 111},
            {"HLTV", 350},
            {nullptr, 0}
        };
        for (int i = 0; commonMsgs[i].name; i++) {
            if (strcmp(commonMsgs[i].name, queryName) == 0) {
                msgId = commonMsgs[i].id;
                break;
            }
        }
    }

    // For the "CS_DeathMsg" alias, fall back to MAX_REG_MSGS (virtual ID) if no real ID was resolved
    if (msgId < 0 && strcmp(name, "CS_DeathMsg") == 0)
        msgId = MAX_REG_MSGS;

    // Cache the result
    if (msgId >= 0)
        m_msgIdCache[name] = msgId;

    AMXX_LOG_DBG("[Events] GetMsgIdByName: '%s' -> %d", name, msgId);
    return msgId;
}

// ========== Event registration ==========
int AMXXEventSystem::RegisterEvent(AMX *amx, const char *eventName, int funcidx, int flags)
{
    AMXXPlugin *plugin = AMXXRuntime::GetInstance().FindPluginByAMX(amx);
    if (!plugin) {
        AMXX_LOG("[Events] RegisterEvent: Failed to find plugin for AMX=%p", amx);
        return -1;
    }

    EventHook hook;
    hook.amx = amx;
    hook.funcidx = funcidx;
    hook.eventName = eventName ? eventName : "";
    hook.flags = flags;
    hook.msgId = GetMsgIdByName(eventName);  // Try to resolve the name to a message ID
    hook.conditions = nullptr;
    hook.m_Stamp = 0;

    // If the event name cannot be resolved to a message ID, log an error and fail (register_event returns 0)
    if (hook.msgId < 0) {
        AMXX_LOG_ERR("[Events] register_event: Invalid event (name \"%s\")", eventName ? eventName : "");
        return -1;
    }

    hook.spForwardId = AMXXForwardManager::GetInstance().RegisterSPForwardByIndex(
        plugin, funcidx, 0, nullptr);

    // Allocate a globally unique handle (starting at 1000).
    // Message events and normal events share the same handle space so that
    // unregister/condition/enable operations can match exactly by handle.
    hook.handle = m_nextEventHandle++;

    // A message event if the name is all digits or was resolved to a message ID
    bool isMsgEvent = false;
    if (eventName) {
        // Check if the name is all digits
        bool allDigits = true;
        for (const char *p = eventName; *p; ++p) {
            if (!isdigit((unsigned char)*p)) { allDigits = false; break; }
        }
        if (allDigits) isMsgEvent = true;
        else if (hook.msgId > 0) isMsgEvent = true;  // resolved to a message ID
    }

    if (isMsgEvent) {
        m_msgHooks.push_back(hook);
        AMXX_LOG_DBG("[Events] Registered message event: '%s' (msgId=%d, handle=%d)", eventName, hook.msgId, hook.handle);
        return hook.handle;
    } else {
        m_eventHooks.push_back(hook);
        AMXX_LOG_DBG("[Events] Registered normal event: '%s' (handle=%d)", eventName, hook.handle);
        return hook.handle;
    }
}

void AMXXEventSystem::UnregisterEvents(AMX *amx)
{
    for (auto it = m_eventHooks.begin(); it != m_eventHooks.end();) {
        if (it->amx == amx) {
            // Free conditions
            EventCondition *cond = it->conditions;
            while (cond) {
                EventCondition *next = cond->next;
                delete cond;
                cond = next;
            }
            // Unregister SP Forward
            if (it->spForwardId > 0) {
                AMXXForwardManager::GetInstance().UnregisterSPForward(it->spForwardId);
            }
            it = m_eventHooks.erase(it);
        } else {
            ++it;
        }
    }
    // Message event hooks registered via register_event also belong to this plugin and must be cleaned up
    for (auto it = m_msgHooks.begin(); it != m_msgHooks.end();) {
        if (it->amx == amx) {
            EventCondition *cond = it->conditions;
            while (cond) {
                EventCondition *next = cond->next;
                delete cond;
                cond = next;
            }
            if (it->spForwardId > 0) {
                AMXXForwardManager::GetInstance().UnregisterSPForward(it->spForwardId);
            }
            it = m_msgHooks.erase(it);
        } else {
            ++it;
        }
    }
}

void AMXXEventSystem::UnregisterEvent(int handle)
{
    // Match by the globally unique handle in both the normal and message event lists
    EventHook *target = nullptr;
    for (auto &hook : m_eventHooks) {
        if (hook.handle == handle) { target = &hook; break; }
    }
    if (!target) {
        for (auto &hook : m_msgHooks) {
            if (hook.handle == handle) { target = &hook; break; }
        }
    }
    if (!target || !target->amx)
        return; // Not found or already unregistered

    // Free conditions
    EventCondition *cond = target->conditions;
    while (cond) {
        EventCondition *next = cond->next;
        delete cond;
        cond = next;
    }
    target->conditions = nullptr;

    // Unregister SP Forward
    if (target->spForwardId > 0) {
        AMXXForwardManager::GetInstance().UnregisterSPForward(target->spForwardId);
    }

    // Mark as unregistered (no erase; keeps the handle stable)
    target->amx = nullptr;
    target->funcidx = -1;
    target->enabled = false;
    target->eventName.clear();
    target->msgId = -1;

    AMXX_LOG_DBG("[Events] Unregistered event handle=%d", handle);
}

// ========== Message registration ==========
int AMXXEventSystem::RegisterMessage(AMX *amx, int msgId, int funcidx, int flags)
{
    MessageHook hook;
    hook.amx = amx;
    hook.funcidx = funcidx;
    hook.msgId = msgId;
    hook.flags = flags;

    m_messageHooks.push_back(hook);
    AMXX_LOG_DBG("[Events] Registered message: msgId=%d (funcidx=%d)", msgId, funcidx);
    return (int)m_messageHooks.size() - 1;
}

void AMXXEventSystem::UnregisterMessages(AMX *amx)
{
    for (auto it = m_messageHooks.begin(); it != m_messageHooks.end();) {
        if (it->amx == amx) {
            it = m_messageHooks.erase(it);
        } else {
            ++it;
        }
    }
}

// ========== Condition handling ==========
// filter format: "paramId&value", e.g. "2&0" means param 2 bitwise-AND 0
// Value parsing (mirrors original CEvent.cpp parseCondition):
//   - Leading '"': strip the quotes and treat as a string value ("2=xxx" / "2=\"xxx\"")
//   - Fully parseable by strtof: numeric (float if it contains a '.', otherwise int)
//   - Otherwise: use the remaining text as a string value (quotes not required)
void AMXXEventSystem::AddEventCondition(int eventHandle, const char *filter)
{
    if (!filter)
        return;

    // Match by the globally unique handle in both the normal and message event lists
    EventHook *hook = nullptr;
    for (auto &h : m_eventHooks) {
        if (h.handle == eventHandle) { hook = &h; break; }
    }
    if (!hook) {
        for (auto &h : m_msgHooks) {
            if (h.handle == eventHandle) { hook = &h; break; }
        }
    }
    if (!hook)
        return;

    EventCondition *cond = new EventCondition();

    // Parse param ID
    const char *p = filter;
    while (isdigit((unsigned char)*p)) p++;
    if (p == filter) {
        delete cond;
        return;
    }

    cond->paramId = atoi(filter);
    if (!*p) {
        delete cond;
        return;
    }

    // Parse operator
    cond->op = *p;
    p++;

    // Parse value
    if (*p == '"') {
        // Quoted string value: strip the quotes
        p++;
        const char *strStart = p;
        while (*p && *p != '"') p++;
        cond->sValue = std::string(strStart, p - strStart);
        cond->type = 2;
        cond->iValue = 0;
        cond->fValue = 0;
    } else {
        // Unquoted: numeric if the whole text parses via strtof, otherwise a string
        char *end = nullptr;
        float fval = strtof(p, &end);
        if (end != p && *end == '\0') {
            cond->fValue = fval;
            cond->iValue = (int)fval;
            // Match as float if it contains '.' or ',', otherwise as int (same as original parseCondition)
            cond->type = (strchr(p, '.') || strchr(p, ',')) ? 1 : 0;
        } else {
            // Non-numeric text (strtof stopped early): use the rest as a string value
            cond->sValue = p;
            cond->type = 2;
            cond->iValue = 0;
            cond->fValue = 0;
        }
    }

    // Add to condition chain
    cond->next = nullptr;
    if (!hook->conditions) {
        hook->conditions = cond;
    } else {
        EventCondition *c = hook->conditions;
        while (c->next) c = c->next;
        c->next = cond;
    }

    AMXX_LOG_DBG("[Events] Added condition: param=%d op=%c val=%s", cond->paramId, cond->op, filter);
}

// ========== Enable/disable events ==========
void AMXXEventSystem::SetEventEnabled(int eventHandle, bool enabled)
{
    // Match by the globally unique handle in both the normal and message event lists
    for (auto &hook : m_eventHooks) {
        if (hook.handle == eventHandle) {
            hook.enabled = enabled;
            return;
        }
    }
    for (auto &hook : m_msgHooks) {
        if (hook.handle == eventHandle) {
            hook.enabled = enabled;
            return;
        }
    }
}

// ========== Condition checking ==========
bool AMXXEventSystem::CheckEventConditions(const EventHook &hook)
{
    if (!hook.conditions)
        return true;

    // Check conditions against m_readVault (1-based AMXX param IDs)
    for (EventCondition *cond = hook.conditions; cond; cond = cond->next) {
        if (cond->paramId < 1 || cond->paramId > (int)m_readVault.size())
            continue;

        const ParsedArg &arg = m_readVault[cond->paramId - 1];
        bool match = false;

        switch (cond->type) {
            case 0: // Integer
                switch (cond->op) {
                    case '=': match = (arg.iValue == cond->iValue); break;
                    case '!': match = (arg.iValue != cond->iValue); break;
                    case '&': match = ((arg.iValue & cond->iValue) != 0); break;
                    case '<': match = (arg.iValue < cond->iValue); break;
                    case '>': match = (arg.iValue > cond->iValue); break;
                }
                break;
            case 1: // Float
                switch (cond->op) {
                    case '=': match = (arg.fValue == cond->fValue); break;
                    case '!': match = (arg.fValue != cond->fValue); break;
                    case '<': match = (arg.fValue < cond->fValue); break;
                    case '>': match = (arg.fValue > cond->fValue); break;
                }
                break;
            case 2: // String
                switch (cond->op) {
                    case '=': match = (arg.sValue == cond->sValue); break;
                    case '!': match = (arg.sValue != cond->sValue); break;
                    case '&': match = (strstr(arg.sValue.c_str(), cond->sValue.c_str()) != nullptr); break;
                }
                break;
        }

        if (!match)
            return false;
    }

    return true;
}

// ========== Flag checking ==========
// Original semantics (CEvent.cpp parserInit):
//   - Messages without an entity (pEntity==NULL) require the world ('a') flag;
//   - Messages with an entity require the client ('b') flag, then filter by alive/dead/player/bot;
//   - Events with no flags never fire.
bool AMXXEventSystem::CheckEventFlags(const EventHook &hook, edict_t *pEntity)
{
    int flags = hook.flags;

    if (!pEntity) {
        // No entity (world message): requires the world ('a') flag
        if (!(flags & EVTFLAG_WORLD))
            return false;

        // Check ONCE ('c' flag): fire at most once per map
        if (flags & EVTFLAG_ONCE) {
            if (hook.m_Stamp == m_mapStamp)
                return false;
        }

        return true;
    }

    // Has entity: requires the client ('b') flag
    if (!(flags & EVTFLAG_CLIENT))
        return false;

    // Check alive/dead
    if (flags & EVTFLAG_ALIVE) {
        if (pEntity->v.deadflag == DEAD_DEAD || pEntity->v.deadflag == DEAD_RESPAWNABLE)
            return false;
    }
    if (flags & EVTFLAG_DEAD) {
        if (pEntity->v.deadflag == DEAD_NO)
            return false;
    }

    // Check player/bot
    if (flags & EVTFLAG_PLAYER) {
        if (pEntity->v.flags & FL_CLIENT) {
            // Check if bot
            if (pEntity->v.flags & FL_FAKECLIENT)
                return false;
        } else {
            return false;
        }
    }
    if (flags & EVTFLAG_BOT) {
        if (!(pEntity->v.flags & FL_FAKECLIENT))
            return false;
    }

    // Check ONCE ('c' flag): each hook tracks its own m_Stamp and fires once per map.
    // m_mapStamp increments on each map change (Clear); hook.m_Stamp is set to m_mapStamp after firing.
    if (flags & EVTFLAG_ONCE) {
        if (hook.m_Stamp == m_mapStamp)
            return false;  // Already fired on this map
    }

    return true;
}

// Helper: parse a string as int/float, storing it as a string if neither parses
static int DetectArgType(const char *str, int &iVal, float &fVal)
{
    if (!str || !*str) {
        iVal = 0; fVal = 0.0f;
        return 2; // string
    }
    // Try integer
    char *end = nullptr;
    long lval = strtol(str, &end, 10);
    if (end && *end == '\0') {
        iVal = (int)lval;
        fVal = (float)lval;
        return 0; // int
    }
    // Try float
    end = nullptr;
    float fval = strtof(str, &end);
    if (end && *end == '\0') {
        iVal = (int)fval;
        fVal = fval;
        return 1; // float
    }
    // String
    iVal = 0; fVal = 0.0f;
    return 2; // string
}

// ========== Fire event (direct call, not message-driven) ==========
void AMXXEventSystem::FireEvent(const char *eventName, int numArgs, const char **args)
{
    if (m_eventHooks.empty())
        return;

    m_currentEventName = eventName ? eventName : "";
    m_currentEventArgs.clear();
    for (int i = 0; i < numArgs; i++) {
        m_currentEventArgs.push_back(args[i] ? args[i] : "");
    }

    // Fill the parse cache
    m_parseVault.clear();
    m_parsePos = 0;
    m_isMessageEvent = false;

    for (int i = 0; i < numArgs; i++) {
        ParsedArg arg;
        const char *str = args[i] ? args[i] : "";
        arg.type = DetectArgType(str, arg.iValue, arg.fValue);
        arg.sValue = str;
        m_parseVault.push_back(arg);
    }
    m_parsePos = (int)m_parseVault.size() - 1;
    UpdateReadVault();

    // Run normal event hooks (exact event name match, no substring matching)
    for (auto &hook : m_eventHooks) {
        if (!hook.enabled || !hook.amx)
            continue;
        if (hook.eventName == m_currentEventName) {
            int result = ExecuteEventCallback(hook);
            if (result == PLUGIN_HANDLED || result == PLUGIN_STOP) {
                AMXX_LOG_DBG("[Events] FireEvent '%s' returned %d, stopping", hook.eventName.c_str(), result);
                break;
            }
        }
    }
}

// ========== Fire message (driven by engine message callbacks) ==========
void AMXXEventSystem::FireMessage(int msgId, int dest, edict_t *pEntity, const void *data, int size)
{
    // Fire message hooks
    for (auto &hook : m_messageHooks) {
        if (hook.msgId == msgId) {
            AMX *amx = hook.amx;
            cell retval;
            if (amx_Exec(amx, &retval, hook.funcidx) == AMX_ERR_NONE) {
                AMXX_LOG_DBG("[Events] Message %d handled by plugin, result: %d", msgId, retval);
            }
        }
    }
}

// ========== Message handling (triggered by the message system's EndMessage) ==========
void AMXXEventSystem::OnMessage(int msgId, int dest, edict_t *pEntity)
{
    if (m_msgHooks.empty() && m_messageHooks.empty())
        return;

    // Reentrancy guard: if OnMessage is called again while callbacks are running (nested
    // messages/events), save the current readVault state and restore it afterwards so the
    // outer callback never reads corrupted data.
    struct VaultSnapshot {
        std::vector<ParsedArg> readVault;
        std::vector<ParsedArg> parseVault;
        int readPos;
        int readMsgType;
        int parsePos;
        int parseMsgType;
        std::string currentEventName;
        bool isMessageEvent;
    };
    VaultSnapshot saved;
    const bool reentered = (m_inExecute > 0);
    if (reentered) {
        saved.readVault = m_readVault;
        saved.parseVault = m_parseVault;
        saved.readPos = m_readPos;
        saved.readMsgType = m_readMsgType;
        saved.parsePos = m_parsePos;
        saved.parseMsgType = m_parseMsgType;
        saved.currentEventName = m_currentEventName;
        saved.isMessageEvent = m_isMessageEvent;
    }
    m_inExecute++;

    // Fetch message arguments
    AMXXMessageSystem &msgs = AMXXMessageSystem::GetInstance();
    int argCount = msgs.GetArgCount();

    // Fill the parse cache (stored 0-based; read_data accesses 1-based by subtracting 1)
    m_parseVault.clear();
    m_parsePos = 0;
    m_parseMsgType = msgId;
    m_isMessageEvent = true;

    for (int i = 1; i <= argCount; i++) {
        ParsedArg arg;
        MsgArgType msgArgType = msgs.GetArgType(i);
        switch (msgArgType) {
            case MSGARG_BYTE: case MSGARG_CHAR: case MSGARG_SHORT: case MSGARG_LONG: case MSGARG_ENTITY:
                arg.iValue = msgs.GetArgInt(i);
                arg.fValue = (float)arg.iValue;
                arg.type = 0;
                break;
            case MSGARG_ANGLE: case MSGARG_COORD:
                arg.fValue = msgs.GetArgFloat(i);
                arg.iValue = (int)arg.fValue;
                arg.type = 1;
                break;
            case MSGARG_STRING:
                arg.iValue = msgs.GetArgInt(i);
                arg.fValue = msgs.GetArgFloat(i);
                arg.type = 2;
                break;
            default:
                arg.iValue = msgs.GetArgInt(i);
                arg.fValue = msgs.GetArgFloat(i);
                arg.type = 0;
                break;
        }
        char buf[256];
        msgs.GetArgString(i, buf, sizeof(buf));
        arg.sValue = buf;
        m_parseVault.push_back(arg);
    }
    m_parsePos = (int)m_parseVault.size() - 1;
    UpdateReadVault();

    // Run message event hooks
    for (auto &hook : m_msgHooks) {
        if (!hook.enabled)
            continue;
        if (hook.msgId != msgId)
            continue;
        if (!CheckEventFlags(hook, pEntity))
            continue;
        if (!CheckEventConditions(hook))
            continue;
        // 'c' (ONCE): mark as fired on this map (set before execution to prevent reentrant re-firing)
        if (hook.flags & EVTFLAG_ONCE) {
            hook.m_Stamp = m_mapStamp;
        }
        int result = ExecuteEventCallback(hook);
        if (result == PLUGIN_HANDLED || result == PLUGIN_STOP) {
            AMXX_LOG_DBG("[Events] Message event (msgId=%d) returned %d, stopping", msgId, result);
            break;
        }
    }

    // Run message hooks registered via register_message
    for (auto &hook : m_messageHooks) {
        if (hook.msgId == msgId) {
            AMX *amx = hook.amx;
            cell retval;
            if (amx_Exec(amx, &retval, hook.funcidx) == AMX_ERR_NONE) {
                AMXX_LOG_DBG("[Events] Message %d hook returned %d", msgId, retval);
            }
        }
    }

    m_inExecute--;

    // Restore the outer readVault state
    if (reentered) {
        m_readVault = saved.readVault;
        m_parseVault = saved.parseVault;
        m_readPos = saved.readPos;
        m_readMsgType = saved.readMsgType;
        m_parsePos = saved.parsePos;
        m_parseMsgType = saved.parseMsgType;
        m_currentEventName = saved.currentEventName;
        m_isMessageEvent = saved.isMessageEvent;
    }
}

// ========== Internal execution ==========
int AMXXEventSystem::ExecuteEventCallback(EventHook &hook)
{
    AMX *amx = hook.amx;
    if (!amx || !amx->base)
        return PLUGIN_CONTINUE;

    // If a valid SP Forward ID exists, execute through the SP Forward system
    if (hook.spForwardId > 0) {
        AMXXSPForward *spForward = AMXXForwardManager::GetInstance().GetSPForward(hook.spForwardId);
        if (spForward && !spForward->IsFree()) {
            // Build the argument list from the current event arguments in m_readVault
            // (event callbacks usually take no parameters and read data via read_data)
            int result = spForward->Execute(0, nullptr);
            AMXX_LOG_DBG("[Events] SP Forward '%s' executed, result=%d", hook.eventName.c_str(), result);
            return result;
        }
    }

    // Fall back to direct execution
    cell retval = 0;
    int err = amx_Exec(amx, &retval, hook.funcidx);
    if (err != AMX_ERR_NONE) {
        AMXX_LOG("[Events] Event '%s' execution failed: error %d", hook.eventName.c_str(), err);
        return PLUGIN_CONTINUE;
    }

    return (int)retval;
}

void AMXXEventSystem::UpdateReadVault()
{
    m_readVault = m_parseVault;
    m_readPos = m_parsePos;
    m_readMsgType = m_parseMsgType;
}

bool AMXXEventSystem::HasEventHook(int msgId)
{
    for (auto &hook : m_eventHooks) {
        if (!hook.enabled || !hook.amx)
            continue;
        if (hook.msgId == msgId)
            return true;
    }
    return false;
}

// ========== Argument reading (1-based AMXX semantics: read_data(1) = first argument) ==========
const char *AMXXEventSystem::GetEventArg(int index)
{
    if (index < 1 || index > (int)m_readVault.size())
        return "";

    const ParsedArg &arg = m_readVault[index - 1];
    switch (arg.type) {
        case 0: {
            thread_local char buf[64];
            snprintf(buf, sizeof(buf), "%d", arg.iValue);
            return buf;
        }
        case 1: {
            thread_local char buf[64];
            snprintf(buf, sizeof(buf), "%g", arg.fValue);
            return buf;
        }
        case 2:
            return arg.sValue.c_str();
    }
    return "";
}

int AMXXEventSystem::GetEventArgInt(int index)
{
    if (index < 1 || index > (int)m_readVault.size())
        return 0;
    return m_readVault[index - 1].iValue;
}

float AMXXEventSystem::GetEventArgFloat(int index)
{
    if (index < 1 || index > (int)m_readVault.size())
        return 0.0f;
    return m_readVault[index - 1].fValue;
}

const char *AMXXEventSystem::GetEventArgString(int index)
{
    if (index < 1 || index > (int)m_readVault.size())
        return "";
    return m_readVault[index - 1].sValue.c_str();
}

// ========== Log Events (register_logevent) ==========
int AMXXEventSystem::RegisterLogEvent(AMX *amx, const char *funcname, int argsnum)
{
    if (!amx || !funcname || !*funcname)
        return -1;

    // 原版 AMXX registerLogEvent: pos 必须在 [1, MAX_LOGARGS] 内, 否则返回 0 (注册失败)
    if (argsnum < 1 || argsnum > 8) {
        AMXX_LOG_ERR("[Events] register_logevent: Invalid argsnum %d (must be 1-8)", argsnum);
        return -1;
    }

    AMXXPlugin *plugin = AMXXRuntime::GetInstance().FindPluginByAMX(amx);
    if (!plugin) {
        AMXX_LOG("[Events] RegisterLogEvent: Failed to find plugin for AMX=%p", amx);
        return -1;
    }

    int funcidx;
    if (amx_FindPublic(amx, funcname, &funcidx) != AMX_ERR_NONE) {
        AMXX_LOG("[Events] RegisterLogEvent: function '%s' not found", funcname);
        return -1;
    }

    LogEventHook hook;
    hook.amx = amx;
    hook.funcidx = funcidx;
    hook.argsnum = argsnum;
    hook.spForwardId = AMXXForwardManager::GetInstance().RegisterSPForwardByIndex(
        plugin, funcidx, 0, nullptr);

    m_logEventHooks.push_back(hook);
    int handle = (int)m_logEventHooks.size() - 1;
    AMXX_LOG_DBG("[Events] Registered log event: '%s' (argsnum=%d, handle=%d)", funcname, argsnum, handle);
    return handle;
}

// filter 格式: "argnum=pattern" (精确匹配 args[argnum])
//               "argnum&pattern" (子串匹配 args[argnum])
//               "pattern"        (无 argnum 前缀, 对第一个 log token 做子串匹配, 原版匹配 logArgs[0])
void AMXXEventSystem::AddLogEventFilter(int handle, const char *filter)
{
    if (handle < 0 || handle >= (int)m_logEventHooks.size() || !filter || !*filter)
        return;

    LogEventFilter f;
    f.argnum = -1;      // 默认: 匹配第一个 log token
    f.substring = true;

    const char *p = filter;
    // 解析可选的 argnum 前缀
    if (isdigit((unsigned char)*p)) {
        const char *start = p;
        while (isdigit((unsigned char)*p)) p++;
        if (*p == '=' || *p == '&') {
            f.argnum = atoi(start);
            f.substring = (*p == '&');
            p++;  // 跳过操作符
        } else {
            p = filter;  // 无有效操作符, 回退到第一个 token 匹配
        }
    }
    f.text = p;
    if (f.argnum < 0)
        f.substring = true;  // 无前缀匹配始终用子串

    m_logEventHooks[handle].filters.push_back(f);
    AMXX_LOG_DBG("[Events] Added log filter: argnum=%d sub=%d text='%s'", f.argnum, (int)f.substring, f.text.c_str());
}

void AMXXEventSystem::SetLogEventEnabled(int handle, bool enabled)
{
    if (handle >= 0 && handle < (int)m_logEventHooks.size())
        m_logEventHooks[handle].enabled = enabled;
}

void AMXXEventSystem::OnLogMessage(const char *logString)
{
    if (!logString || m_logEventHooks.empty())
        return;

    // 重入保护: 防止 logevent 回调内调用 log_amx/log_message 等导致递归
    if (m_inLogExecute > 0)
        return;
    m_inLogExecute++;

    // 存储当前日志事件数据 (供 read_logdata / read_logargc / read_logargv 使用)
    AMXX_SetLogEventData(logString);

    // 解析日志字符串为 token 列表 (按引号/括号/空白分割, 与原版 AMXX parseLogString 一致)
    // 例: World triggered "Round_Start" -> ["World", "triggered", "Round_Start"]
    //     Team "CT" scored "17" with "0" players -> ["Team", "CT", "scored", "17", "with", "0", "players"]
    std::vector<std::string> args;
    const char *b = logString;
    while (*b) {
        while (*b == ' ' || *b == '\t') b++;
        if (!*b) break;
        std::string token;
        if (*b == '"') {
            b++;
            while (*b && *b != '"' && token.size() < 127) token += *b++;
            if (*b == '"') b++;
        } else if (*b == '(') {
            b++;
            while (*b && *b != ')' && token.size() < 127) token += *b++;
            if (*b == ')') b++;
        } else {
            while (*b && *b != ' ' && *b != '\t' && *b != '"' && *b != '(' && token.size() < 127)
                token += *b++;
        }
        if (!token.empty())
            args.push_back(token);
    }

    for (auto &hook : m_logEventHooks) {
        if (!hook.enabled)
            continue;
        // argsnum 匹配: 仅当日志 token 数 == argsnum 时触发 (argsnum<=0 表示不限)
        if (hook.argsnum > 0 && hook.argsnum != (int)args.size())
            continue;

        bool match = true;
        for (const auto &filter : hook.filters) {
            if (filter.argnum >= 0 && filter.argnum < (int)args.size()) {
                const std::string &argVal = args[filter.argnum];
                if (filter.substring) {
                    if (argVal.find(filter.text) == std::string::npos) { match = false; break; }
                } else {
                    if (argVal != filter.text) { match = false; break; }
                }
            } else if (filter.argnum >= 0) {
                // argnum 越界: 过滤器不匹配
                match = false; break;
            } else {
                // 无前缀: 只对第一个 log token 匹配 (原版 AMXX 语义: 匹配 logArgs[0])
                if (args.empty()) { match = false; break; }
                const std::string &argVal = args[0];
                if (filter.substring) {
                    if (argVal.find(filter.text) == std::string::npos) { match = false; break; }
                } else {
                    if (argVal != filter.text) { match = false; break; }
                }
            }
        }

        if (match) {
            AMX *amx = hook.amx;
            if (!amx) continue;
            if (hook.spForwardId > 0) {
                AMXXSPForward *spf = AMXXForwardManager::GetInstance().GetSPForward(hook.spForwardId);
                if (spf && !spf->IsFree()) {
                    spf->Execute(0, nullptr);
                    continue;
                }
            }
            cell retval = 0;
            amx_Exec(amx, &retval, hook.funcidx);
            AMXX_LOG_DBG("[Events] Log event fired, retval=%d", (int)retval);
        }
    }

    m_inLogExecute--;
}

// ========== 清理 ==========
void AMXXEventSystem::Clear()
{
    for (auto &hook : m_eventHooks) {
        EventCondition *cond = hook.conditions;
        while (cond) {
            EventCondition *next = cond->next;
            delete cond;
            cond = next;
        }
        if (hook.spForwardId > 0)
            AMXXForwardManager::GetInstance().UnregisterSPForward(hook.spForwardId);
    }
    m_eventHooks.clear();
    for (auto &hook : m_msgHooks) {
        EventCondition *cond = hook.conditions;
        while (cond) {
            EventCondition *next = cond->next;
            delete cond;
            cond = next;
        }
        if (hook.spForwardId > 0)
            AMXXForwardManager::GetInstance().UnregisterSPForward(hook.spForwardId);
    }
    m_msgHooks.clear();
    m_messageHooks.clear();
    for (auto &hook : m_logEventHooks) {
        if (hook.spForwardId > 0)
            AMXXForwardManager::GetInstance().UnregisterSPForward(hook.spForwardId);
    }
    m_logEventHooks.clear();
    m_parseVault.clear();
    m_readVault.clear();
    m_currentArgs.clear();
    m_currentEventArgs.clear();
    m_msgIdCache.clear();
    m_parsePos = 0;
    m_readPos = -1;
    m_parseMsgType = -1;
    m_readMsgType = -1;
    m_isMessageEvent = false;
    // 换图时递增 map stamp, 使所有 'c' (ONCE) hook 在新地图重新可触发。
    // (本方法由 runtime OnServerDeactivatePost 在每张地图结束时调用)
    m_mapStamp++;
    m_inExecute = 0;
    m_inLogExecute = 0;
    AMXX_LOG_DBG("[Events] All events cleared (mapStamp=%d)", m_mapStamp);
}