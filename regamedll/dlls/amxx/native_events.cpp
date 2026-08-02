#include "precompiled.h"
#include "native_events.h"
#include "native_core.h"
#include "events.h"
#include "forwards.h"
#include "amx.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

// ========== 辅助函数 ==========
static int AMXX_ReadFlags(const char *str)
{
    int flags = 0;
    if (!str) return flags;
    
    for (const char *p = str; *p; p++) {
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

// ========== register_event ==========
// native register_event(const event[], const function[], const flags[], const cond[] = "", ...);
cell AMX_NATIVE_CALL amxx_register_event(AMX *amx, cell *params)
{
    int numArgs = params[0] / sizeof(cell);

    // params[1] = event name
    cell *addr;
    char eventName[256];
    amx_GetAddr(amx, params[1], &addr);
    amx_GetString(eventName, addr, 0, sizeof(eventName));

    // params[2] = function name
    cell *funcname_addr;
    amx_GetAddr(amx, params[2], &funcname_addr);
    char funcname[256];
    amx_GetString(funcname, funcname_addr, 0, sizeof(funcname));

    // Find function
    int funcidx;
    if (amx_FindPublic(amx, funcname, &funcidx) != AMX_ERR_NONE) {
        AMXX_LOG("[Events] register_event: function '%s' not found for event '%s'", funcname, eventName);
        return 0;
    }

    // params[3] = flags string (optional)
    int flags = 0;
    if (numArgs > 2) {
        cell *flags_addr;
        amx_GetAddr(amx, params[3], &flags_addr);
        char flagsStr[32];
        amx_GetString(flagsStr, flags_addr, 0, sizeof(flagsStr));
        flags = AMXX_ReadFlags(flagsStr);
    }

    // Register the event
    int handle = AMXXEventSystem::GetInstance().RegisterEvent(amx, eventName, funcidx, flags);

    // 注册失败 (未知事件名等): 原版 LogError 后返回 0
    if (handle < 0)
        return 0;

    // Add conditions (params 4+)
    for (int i = 4; i < numArgs; i++) {
        cell *cond_addr;
        amx_GetAddr(amx, params[i], &cond_addr);
        char condStr[256];
        amx_GetString(condStr, cond_addr, 0, sizeof(condStr));
        if (condStr[0]) {
            AMXXEventSystem::GetInstance().AddEventCondition(handle, condStr);
        }
    }

    return handle;
}

// ========== register_event_ex ==========
// native register_event_ex(const event[], const function[], RegisterEventFlags:flags, const cond[] = "", ...);
cell AMX_NATIVE_CALL amxx_register_event_ex(AMX *amx, cell *params)
{
    int numArgs = params[0] / sizeof(cell);

    // params[1] = event name
    cell *addr;
    char eventName[256];
    amx_GetAddr(amx, params[1], &addr);
    amx_GetString(eventName, addr, 0, sizeof(eventName));

    // params[2] = function name
    cell *funcname_addr;
    amx_GetAddr(amx, params[2], &funcname_addr);
    char funcname[256];
    amx_GetString(funcname, funcname_addr, 0, sizeof(funcname));

    int funcidx;
    if (amx_FindPublic(amx, funcname, &funcidx) != AMX_ERR_NONE) {
        AMXX_LOG("[Events] register_event_ex: function '%s' not found", funcname);
        return 0;
    }

    // params[3] = flags (int)
    int flags = params[3];

    int handle = AMXXEventSystem::GetInstance().RegisterEvent(amx, eventName, funcidx, flags);

    // 注册失败 (未知事件名等): 原版 LogError 后返回 0
    if (handle < 0)
        return 0;

    // Add conditions (params 4+)
    for (int i = 4; i < numArgs; i++) {
        cell *cond_addr;
        amx_GetAddr(amx, params[i], &cond_addr);
        char condStr[256];
        amx_GetString(condStr, cond_addr, 0, sizeof(condStr));
        if (condStr[0]) {
            AMXXEventSystem::GetInstance().AddEventCondition(handle, condStr);
        }
    }

    return handle;
}

// ========== enable_event ==========
cell AMX_NATIVE_CALL amxx_enable_event(AMX *amx, cell *params)
{
    (void)amx;
    AMXXEventSystem::GetInstance().SetEventEnabled((int)params[1], true);
    return 1;
}

// ========== disable_event ==========
cell AMX_NATIVE_CALL amxx_disable_event(AMX *amx, cell *params)
{
    (void)amx;
    AMXXEventSystem::GetInstance().SetEventEnabled((int)params[1], false);
    return 1;
}

// ========== unregister_event ==========
cell AMX_NATIVE_CALL amxx_unregister_event(AMX *amx, cell *params)
{
    (void)params;
    AMXXEventSystem::GetInstance().UnregisterEvents(amx);
    return 1;
}

// ========== read_data_int ==========
cell AMX_NATIVE_CALL amxx_read_data_int(AMX *amx, cell *params)
{
    int index = params[1];
    return AMXXEventSystem::GetInstance().GetEventArgInt(index);
}

// ========== read_data_float ==========
cell AMX_NATIVE_CALL amxx_read_data_float(AMX *amx, cell *params)
{
    int index = params[1];
    float val = AMXXEventSystem::GetInstance().GetEventArgFloat(index);
    return amx_ftoc(val);
}

// ========== read_data_string (alias for read_data) ==========
cell AMX_NATIVE_CALL amxx_read_data_string(AMX *amx, cell *params)
{
    return amxx_read_data(amx, params);
}

// ========== get_data_args ==========
cell AMX_NATIVE_CALL amxx_get_data_args(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return AMXXEventSystem::GetInstance().GetEventArgsCount();
}

// ========== get_data_msg ==========
cell AMX_NATIVE_CALL amxx_get_data_msg(AMX *amx, cell *params)
{
    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    int msgType = AMXXEventSystem::GetInstance().GetCurrentMsgType();
    char msgBuf[32];
    snprintf(msgBuf, sizeof(msgBuf), "%d", msgType);
    return amx_SetString(dest, msgBuf, 0, 0, (int)params[2]);
}

// ========== unregister_event_by_handle(handle) ==========
// 原版 unregister_event(handle) 语义：只注销指定 handle，而非当前 amx 所有事件。
// 现有 amxx_unregister_event 被实现为 UnregisterEvents(amx)（所有事件）。这里添加按 handle 版本。
cell AMX_NATIVE_CALL amxx_unregister_event_by_handle(AMX *amx, cell *params)
{
    (void)amx;
    int handle = (int)params[1];
    AMXXEventSystem::GetInstance().UnregisterEvent(handle);
    return 1;
}

// ========== fire_event ==========
cell AMX_NATIVE_CALL amxx_fire_event(AMX *amx, cell *params)
{
    cell *addr;
    char eventName[256];
    amx_GetAddr(amx, params[1], &addr);
    amx_GetString(eventName, addr, 0, sizeof(eventName));

    int numArgs = params[2];
    const char **args = new const char*[numArgs];

    for (int i = 0; i < numArgs; i++) {
        cell *argAddr;
        amx_GetAddr(amx, params[3 + i], &argAddr);
        static thread_local char buffer[512];
        amx_GetString(buffer, argAddr, 0, sizeof(buffer));
        args[i] = buffer;
    }

    AMXXEventSystem::GetInstance().FireEvent(eventName, numArgs, args);

    delete[] args;
    return 1;
}

// ========== register_logevent ==========
// native register_logevent(const function[], argsnum, ...);
// argsnum: 期望的日志参数 (token) 数量; 仅当日志 token 数 == argsnum 时触发。
// 原版 AMXX registerLogEvent: argsnum 必须在 [1, MAX_LOGARGS(8)] 内, 越界返回 0。
// 可变参数: 过滤器字符串, 格式 "argnum=pattern" / "argnum&pattern" / "pattern"
cell AMX_NATIVE_CALL amxx_register_logevent(AMX *amx, cell *params)
{
    int numArgs = params[0] / sizeof(cell);

    // params[1] = function name
    cell *addr;
    amx_GetAddr(amx, params[1], &addr);
    char funcname[256];
    amx_GetString(funcname, addr, 0, sizeof(funcname));

    // params[2] = argsnum
    int argsnum = (int)params[2];

    int handle = AMXXEventSystem::GetInstance().RegisterLogEvent(amx, funcname, argsnum);
    if (handle < 0) {
        AMXX_LOG("[Events] register_logevent: failed to register '%s'", funcname);
        return 0;
    }

    // Add filters (params 3+): "argnum=pattern" / "argnum&pattern" / "pattern"
    for (int i = 3; i <= numArgs; i++) {
        cell *faddr;
        amx_GetAddr(amx, params[i], &faddr);
        char filterStr[256];
        amx_GetString(filterStr, faddr, 0, sizeof(filterStr));
        if (filterStr[0]) {
            AMXXEventSystem::GetInstance().AddLogEventFilter(handle, filterStr);
        }
    }

    return handle;
}

// ========== enable_logevent ==========
// native enable_logevent(handle);
cell AMX_NATIVE_CALL amxx_enable_logevent(AMX *amx, cell *params)
{
    (void)amx;
    AMXXEventSystem::GetInstance().SetLogEventEnabled((int)params[1], true);
    return 1;
}

// ========== disable_logevent ==========
// native disable_logevent(handle);
cell AMX_NATIVE_CALL amxx_disable_logevent(AMX *amx, cell *params)
{
    (void)amx;
    AMXXEventSystem::GetInstance().SetLogEventEnabled((int)params[1], false);
    return 1;
}

// ========== Native registration table ==========
AMX_NATIVE_INFO event_natives[] = {
    // Event management
    {"register_event", amxx_register_event},
    {"register_event_ex", amxx_register_event_ex},
    {"unregister_event",     amxx_unregister_event_by_handle},
    {"unregister_event_all", amxx_unregister_event},
    {"enable_event", amxx_enable_event},
    {"disable_event", amxx_disable_event},

    // Log events
    {"register_logevent", amxx_register_logevent},
    {"enable_logevent", amxx_enable_logevent},
    {"disable_logevent", amxx_disable_logevent},

    // Data reading (0-based indexing, matching AMXX original)
    {"read_data_int", amxx_read_data_int},
    {"read_data_float", amxx_read_data_float},
    {"read_data_string", amxx_read_data_string},
    {"get_data_args", amxx_get_data_args},
    {"get_data_msg", amxx_get_data_msg},

    // Legacy aliases
    {"fire_event", amxx_fire_event},
    {"event_num", amxx_get_data_args},

    // Event management aliases
    {"event_get", amxx_read_data_int},
    {"event_getint", amxx_read_data_int},
    {"event_getfloat", amxx_read_data_float},
    {"event_getstring", amxx_read_data_string},

    {nullptr, nullptr}
};

void RegisterEventNatives(AMX *amx)
{
    amx_Register(amx, event_natives, -1);
}