// messages.cpp
#include "precompiled.h"
#include "messages.h"
#include "events.h"
#include "amx.h"
#include "../extdll.h"
#include "../enginecallback.h"
#include "../util.h"
#include <cstring>
#include <cstdarg>
#include <cstdio>

static enginefuncs_t g_origEngFuncs;
static bool g_engfuncsHooked = false;

// AlertMessage hook: 捕获 at_logged 日志消息并触发 register_logevent 回调。
// 格式化后转发给引擎原始函数 (以 "%s" 形式, 避免无法转发 va_list 到变参函数的问题)。
static void Hook_AlertMessage(ALERT_TYPE atype, const char *szFmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, szFmt);
    vsnprintf(buf, sizeof(buf), szFmt, ap);
    va_end(ap);

    if (atype == at_logged) {
        // 触发 register_logevent 注册的回调
        AMXXEventSystem::GetInstance().OnLogMessage(buf);
    }

    // 转发给引擎原始 AlertMessage (以格式化后的字符串传入, 确保日志正常写入)
    if (g_origEngFuncs.pfnAlertMessage)
        g_origEngFuncs.pfnAlertMessage(atype, "%s", buf);
}

static void Hook_MessageBegin(int msg_dest, int msg_type, const float *origin, edict_t *ed)
{
    AMXXMessageSystem::GetInstance().BeginMessage(msg_dest, msg_type, origin, ed);
}

static void Hook_MessageEnd(void)
{
    AMXXMessageSystem::GetInstance().EndMessage();
}

static void Hook_WriteByte(int value)
{
    AMXXMessageSystem::GetInstance().WriteByte(value);
}

static void Hook_WriteChar(int value)
{
    AMXXMessageSystem::GetInstance().WriteChar(value);
}

static void Hook_WriteShort(int value)
{
    AMXXMessageSystem::GetInstance().WriteShort(value);
}

static void Hook_WriteLong(int value)
{
    AMXXMessageSystem::GetInstance().WriteLong(value);
}

static void Hook_WriteAngle(float value)
{
    AMXXMessageSystem::GetInstance().WriteAngle(value);
}

static void Hook_WriteCoord(float value)
{
    AMXXMessageSystem::GetInstance().WriteCoord(value);
}

static void Hook_WriteString(const char *value)
{
    AMXXMessageSystem::GetInstance().WriteString(value);
}

static void Hook_WriteEntity(int value)
{
    AMXXMessageSystem::GetInstance().WriteEntity(value);
}

AMXXMessageSystem &AMXXMessageSystem::GetInstance()
{
    static AMXXMessageSystem instance;
    return instance;
}

AMXXMessageSystem::AMXXMessageSystem()
{
    m_inMessage = false;
    m_inBlock = false;
    m_blockDepth = 0;
    m_blockMsgId = 0;
    m_nextHandle = 1;
    m_origRetVal = 0;
    m_currentMsgType = 0;
    m_currentDest = 0;
    m_currentMsgId = 0;
    m_currentEntity = nullptr;
    m_hasOrigin = false;
    m_currentOrigin[0] = m_currentOrigin[1] = m_currentOrigin[2] = 0.0f;
}

void AMXXMessageSystem::Init()
{
    AMXX_LOG_DBG("[Messages] Initializing message hook system...");
    m_hooks.clear();
    m_args.clear();
    m_inMessage = false;
    m_inBlock = false;
    m_blockDepth = 0;
    m_blockMsgId = 0;
    m_msgBlocks.clear();
    m_hasOrigin = false;

    if (!g_engfuncsHooked) {
        g_origEngFuncs = g_engfuncs;
        g_engfuncsHooked = true;

        g_engfuncs.pfnMessageBegin = Hook_MessageBegin;
        g_engfuncs.pfnMessageEnd = Hook_MessageEnd;
        g_engfuncs.pfnWriteByte = Hook_WriteByte;
        g_engfuncs.pfnWriteChar = Hook_WriteChar;
        g_engfuncs.pfnWriteShort = Hook_WriteShort;
        g_engfuncs.pfnWriteLong = Hook_WriteLong;
        g_engfuncs.pfnWriteAngle = Hook_WriteAngle;
        g_engfuncs.pfnWriteCoord = Hook_WriteCoord;
        g_engfuncs.pfnWriteString = Hook_WriteString;
        g_engfuncs.pfnWriteEntity = Hook_WriteEntity;

        // Hook AlertMessage 以支持 register_logevent (捕获 at_logged 日志消息)
        g_engfuncs.pfnAlertMessage = Hook_AlertMessage;

        AMXX_LOG_DBG("[Messages] Engine message functions hooked");
    }
}

void AMXXMessageSystem::Clear()
{
    m_hooks.clear();
    m_args.clear();
    m_inMessage = false;
    m_inBlock = false;
    m_blockDepth = 0;
    m_blockMsgId = 0;
    m_msgBlocks.clear();
    m_hasOrigin = false;
    m_origRetVal = 0;
    m_currentMsgType = 0;
    m_currentDest = 0;
    m_currentMsgId = 0;
    m_currentEntity = nullptr;
    AMXX_LOG_DBG("[Messages] Per-map state cleared");
}

int AMXXMessageSystem::RegisterHook(AMX *amx, int msgId, cell funcidx, int flags)
{
    MsgHookInfo hook;
    hook.amx = amx;
    hook.funcidx = funcidx;
    hook.msgId = msgId;
    hook.flags = flags;
    hook.handle = m_nextHandle++;
    m_hooks.push_back(hook);
    AMXX_LOG_DBG("[Messages] Registered hook for message %d (funcidx=%d, handle=%d)", msgId, funcidx, hook.handle);
    return hook.handle;
}

void AMXXMessageSystem::UnregisterHooks(AMX *amx)
{
    for (auto it = m_hooks.begin(); it != m_hooks.end();) {
        if (it->amx == amx) {
            it = m_hooks.erase(it);
        } else {
            ++it;
        }
    }
}

bool AMXXMessageSystem::UnregisterHook(int msgId, int handle)
{
    for (auto it = m_hooks.begin(); it != m_hooks.end(); ++it) {
        if (it->msgId == msgId && it->handle == handle) {
            m_hooks.erase(it);
            return true;
        }
    }
    return false;
}

void AMXXMessageSystem::SetMsgBlock(int msgId, int block)
{
    m_msgBlocks[msgId] = block;
}

int AMXXMessageSystem::GetMsgBlock(int msgId) const
{
    auto it = m_msgBlocks.find(msgId);
    return it != m_msgBlocks.end() ? it->second : BLOCK_NOT;
}

bool AMXXMessageSystem::HasHook(int msgId)
{
    // 检查 register_message 注册的钩子
    for (auto &hook : m_hooks) {
        if (hook.msgId == msgId)
            return true;
    }
    // 检查 register_event 注册的事件
    return AMXXEventSystem::GetInstance().HasEventHook(msgId);
}

void AMXXMessageSystem::BeginMessage(int dest, int msgType, const float *origin, edict_t *pEntity)
{
    if (m_inMessage) {
        // 嵌套消息: 若外层是阻塞消息则一并吞掉, 否则透传给引擎 (保持原有重入行为)
        if (m_inBlock) {
            m_blockDepth++;
            return;
        }
        g_origEngFuncs.pfnMessageBegin(dest, msgType, origin, pEntity);
        return;
    }

    m_inMessage = true;
    m_currentDest = dest;
    m_currentMsgId = msgType;
    m_currentEntity = pEntity;
    if (origin) {
        m_currentOrigin[0] = origin[0];
        m_currentOrigin[1] = origin[1];
        m_currentOrigin[2] = origin[2];
        m_hasOrigin = true;
    } else {
        m_hasOrigin = false;
    }
    m_args.clear();

    // 按消息 ID 阻塞 (原版 AMXX msgBlocks[msgid] 语义: C_MessageBegin 先于 hook 检查)。
    // BLOCK_ONCE 在 EndMessage 中发送一次后自动清除为 BLOCK_NOT。
    auto it = m_msgBlocks.find(msgType);
    if (it != m_msgBlocks.end() && it->second != BLOCK_NOT) {
        m_inBlock = true;
        m_blockMsgId = msgType;
        return;  // 吞掉整条消息 (Begin/Write/End 均不转发到引擎)
    }

    if (!HasHook(msgType)) {
        g_origEngFuncs.pfnMessageBegin(dest, msgType, origin, pEntity);
    }
}

void AMXXMessageSystem::EndMessage()
{
    if (!m_inMessage)
        return;

    // 阻塞消息: 吞掉, 若是 BLOCK_ONCE 则自动解除
    if (m_inBlock) {
        if (m_blockDepth > 0) {
            m_blockDepth--;
            return;
        }
        m_inBlock = false;
        if (m_msgBlocks[m_blockMsgId] == BLOCK_ONCE)
            m_msgBlocks[m_blockMsgId] = BLOCK_NOT;
        m_inMessage = false;
        m_args.clear();
        return;
    }

    if (HasHook(m_currentMsgId)) {
        int result = FireHooks();

        // P0-8 修复: 在 FireHooks (register_message) 之后触发 register_event 回调。
        // 此前 OnMessage 在 FireHooks 内部调用, 现独立为显式步骤, 确保 register_event
        // 钩子 (DeathMsg/CurWeapon 等消息驱动事件) 能正确触发, 且不受 register_message
        // 阻塞决策影响 (事件在消息构造期间触发, 与原版 AMXX 语义一致)。
        AMXXEventSystem::GetInstance().OnMessage(m_currentMsgId, m_currentDest, m_currentEntity);

        // 原版 AMXX: mres & 1 才阻止消息。
        // PLUGIN_CONTINUE(0) / PLUGIN_HANDLED_MAIN(2) 放行, PLUGIN_HANDLED(1) 及奇数返回值阻止。
        if (result & 1) {
            m_inMessage = false;
            m_args.clear();
            return;
        }

        // 重发消息时保留原始 origin (原版 MESSAGE_BEGIN(msgDest, msgType, msgOrigin, msgpEntity))
        g_origEngFuncs.pfnMessageBegin(m_currentDest, m_currentMsgId,
            m_hasOrigin ? m_currentOrigin : nullptr, m_currentEntity);
        for (auto &arg : m_args) {
            switch (arg.type) {
                case MSGARG_BYTE: g_origEngFuncs.pfnWriteByte(arg.iVal); break;
                case MSGARG_CHAR: g_origEngFuncs.pfnWriteChar(arg.iVal); break;
                case MSGARG_SHORT: g_origEngFuncs.pfnWriteShort(arg.iVal); break;
                case MSGARG_LONG: g_origEngFuncs.pfnWriteLong(arg.iVal); break;
                case MSGARG_ANGLE: g_origEngFuncs.pfnWriteAngle(arg.fVal); break;
                case MSGARG_COORD: g_origEngFuncs.pfnWriteCoord(arg.fVal); break;
                case MSGARG_FLOAT: g_origEngFuncs.pfnWriteLong(*(int *)&arg.fVal); break; // 按 long 写入
                case MSGARG_STRING: g_origEngFuncs.pfnWriteString(arg.sVal.c_str()); break;
                case MSGARG_ENTITY: g_origEngFuncs.pfnWriteEntity(arg.iVal); break;
                default: break;
            }
        }
    }

    g_origEngFuncs.pfnMessageEnd();
    m_inMessage = false;
    m_args.clear();
}

void AMXXMessageSystem::WriteByte(int value)
{
    if (m_inBlock) return;  // 阻塞消息: 吞掉
    if (!m_inMessage || !HasHook(m_currentMsgId)) {
        g_origEngFuncs.pfnWriteByte(value);
        return;
    }
    MsgArg arg; arg.type = MSGARG_BYTE; arg.iVal = value; arg.fVal = 0.0f; m_args.push_back(arg);
}

void AMXXMessageSystem::WriteChar(int value)
{
    if (m_inBlock) return;  // 阻塞消息: 吞掉
    if (!m_inMessage || !HasHook(m_currentMsgId)) {
        g_origEngFuncs.pfnWriteChar(value);
        return;
    }
    MsgArg arg; arg.type = MSGARG_CHAR; arg.iVal = value; arg.fVal = 0.0f; m_args.push_back(arg);
}

void AMXXMessageSystem::WriteShort(int value)
{
    if (m_inBlock) return;  // 阻塞消息: 吞掉
    if (!m_inMessage || !HasHook(m_currentMsgId)) {
        g_origEngFuncs.pfnWriteShort(value);
        return;
    }
    MsgArg arg; arg.type = MSGARG_SHORT; arg.iVal = value; arg.fVal = 0.0f; m_args.push_back(arg);
}

void AMXXMessageSystem::WriteLong(int value)
{
    if (m_inBlock) return;  // 阻塞消息: 吞掉
    if (!m_inMessage || !HasHook(m_currentMsgId)) {
        g_origEngFuncs.pfnWriteLong(value);
        return;
    }
    MsgArg arg; arg.type = MSGARG_LONG; arg.iVal = value; arg.fVal = 0.0f; m_args.push_back(arg);
}

void AMXXMessageSystem::WriteAngle(float value)
{
    if (m_inBlock) return;  // 阻塞消息: 吞掉
    if (!m_inMessage || !HasHook(m_currentMsgId)) {
        g_origEngFuncs.pfnWriteAngle(value);
        return;
    }
    MsgArg arg; arg.type = MSGARG_ANGLE; arg.fVal = value; arg.iVal = (int)value; m_args.push_back(arg);
}

void AMXXMessageSystem::WriteCoord(float value)
{
    if (m_inBlock) return;  // 阻塞消息: 吞掉
    if (!m_inMessage || !HasHook(m_currentMsgId)) {
        g_origEngFuncs.pfnWriteCoord(value);
        return;
    }
    MsgArg arg; arg.type = MSGARG_COORD; arg.fVal = value; arg.iVal = (int)value; m_args.push_back(arg);
}

void AMXXMessageSystem::WriteString(const char *value)
{
    if (m_inBlock) return;  // 阻塞消息: 吞掉
    if (!m_inMessage || !HasHook(m_currentMsgId)) {
        g_origEngFuncs.pfnWriteString(value);
        return;
    }
    MsgArg arg; arg.type = MSGARG_STRING; arg.sVal = value ? value : ""; arg.iVal = 0; arg.fVal = 0.0f; m_args.push_back(arg);
}

void AMXXMessageSystem::WriteEntity(int value)
{
    if (m_inBlock) return;  // 阻塞消息: 吞掉
    if (!m_inMessage || !HasHook(m_currentMsgId)) {
        g_origEngFuncs.pfnWriteEntity(value);
        return;
    }
    MsgArg arg; arg.type = MSGARG_ENTITY; arg.iVal = value; arg.fVal = 0.0f; m_args.push_back(arg);
}

int AMXXMessageSystem::FireHooks()
{
    int finalResult = 0;

    // 调用 register_message 注册的钩子 (register_event 由 EndMessage 中单独调用 OnMessage 触发)
    for (auto &hook : m_hooks) {
        if (hook.msgId != m_currentMsgId)
            continue;

        AMX *amx = hook.amx;
        if (!amx)
            continue;

        cell retval = 0;
        amx_Push(amx, ENTINDEX(m_currentEntity));
        amx_Push(amx, m_currentDest);
        amx_Push(amx, m_currentMsgId);
        amx_Exec(amx, &retval, hook.funcidx);

        AMXX_LOG_DBG("[Messages] Hook for msg %d returned %d", m_currentMsgId, retval);
        if (retval > finalResult)
            finalResult = retval;
    }

    return finalResult;
}

int AMXXMessageSystem::GetArgInt(int index)
{
    if (index < 1 || index > (int)m_args.size())
        return 0;
    const MsgArg &arg = m_args[index - 1];
    switch (arg.type) {
        case MSGARG_BYTE: return arg.iVal;
        case MSGARG_CHAR: return arg.iVal;
        case MSGARG_SHORT: return arg.iVal;
        case MSGARG_LONG: return arg.iVal;
        case MSGARG_ANGLE: return (int)arg.fVal;
        case MSGARG_COORD: return (int)arg.fVal;
        case MSGARG_FLOAT: return (int)arg.fVal;
        case MSGARG_ENTITY: return arg.iVal;
        case MSGARG_STRING: return atoi(arg.sVal.c_str());
        default: return 0;
    }
}

float AMXXMessageSystem::GetArgFloat(int index)
{
    if (index < 1 || index > (int)m_args.size())
        return 0.0f;
    const MsgArg &arg = m_args[index - 1];
    switch (arg.type) {
        case MSGARG_ANGLE: return arg.fVal;
        case MSGARG_COORD: return arg.fVal;
        case MSGARG_FLOAT: return arg.fVal;
        case MSGARG_STRING: return (float)atof(arg.sVal.c_str());
        default: return (float)GetArgInt(index);
    }
}

void AMXXMessageSystem::GetArgString(int index, char *dest, int maxlen)
{
    if (!dest || maxlen <= 0)
        return;
    if (index < 1 || index > (int)m_args.size()) {
        dest[0] = '\0';
        return;
    }
    const MsgArg &arg = m_args[index - 1];
    switch (arg.type) {
        case MSGARG_STRING:
            strncpy(dest, arg.sVal.c_str(), maxlen - 1);
            dest[maxlen - 1] = '\0';
            break;
        default:
            snprintf(dest, maxlen, "%d", GetArgInt(index));
            break;
    }
}

void AMXXMessageSystem::SetArgInt(int index, int value)
{
    if (index < 1 || index > (int)m_args.size())
        return;
    MsgArg &arg = m_args[index - 1];
    arg.iVal = value;
    // 如果类型是浮点相关，同时更新 fVal 以保持一致
    if (arg.type == MSGARG_ANGLE || arg.type == MSGARG_COORD || arg.type == MSGARG_FLOAT)
        arg.fVal = (float)value;
}

void AMXXMessageSystem::SetArgString(int index, const char *value)
{
    if (index < 1 || index > (int)m_args.size())
        return;
    MsgArg &arg = m_args[index - 1];
    arg.type = MSGARG_STRING;
    arg.sVal = value ? value : "";
    arg.iVal = 0;
    arg.fVal = 0.0f;
}

void AMXXMessageSystem::SetArgFloat(int index, float value)
{
    if (index < 1 || index > (int)m_args.size())
        return;
    MsgArg &arg = m_args[index - 1];
    // 保持类型，如果当前类型是 ANGLE/COORD/FLOAT 则更新 fVal，否则设为 FLOAT
    if (arg.type == MSGARG_ANGLE || arg.type == MSGARG_COORD || arg.type == MSGARG_FLOAT) {
        arg.fVal = value;
        arg.iVal = (int)value;
    } else {
        arg.type = MSGARG_FLOAT;
        arg.fVal = value;
        arg.iVal = (int)value;
        arg.sVal.clear();
    }
}

MsgArgType AMXXMessageSystem::GetArgType(int index)
{
    if (index < 1 || index > (int)m_args.size())
        return MSGARG_NONE;
    return m_args[index - 1].type;
}

// ========== Orig* 系列: 直接调用引擎原始消息函数, 绕过 AMXX 钩子层 ==========
// 供 emessage_begin/emessage_end/ewrite_* 等 native 使用。
// 'e' 前缀变体不触发 register_message/register_event, 直接发送到引擎。
void AMXXMessageSystem::OrigMessageBegin(int dest, int msgType, const float *origin, edict_t *pEntity)
{
    g_origEngFuncs.pfnMessageBegin(dest, msgType, origin, pEntity);
}

void AMXXMessageSystem::OrigMessageEnd()
{
    g_origEngFuncs.pfnMessageEnd();
}

void AMXXMessageSystem::OrigWriteByte(int value)
{
    g_origEngFuncs.pfnWriteByte(value);
}

void AMXXMessageSystem::OrigWriteChar(int value)
{
    g_origEngFuncs.pfnWriteChar(value);
}

void AMXXMessageSystem::OrigWriteShort(int value)
{
    g_origEngFuncs.pfnWriteShort(value);
}

void AMXXMessageSystem::OrigWriteLong(int value)
{
    g_origEngFuncs.pfnWriteLong(value);
}

void AMXXMessageSystem::OrigWriteAngle(float value)
{
    g_origEngFuncs.pfnWriteAngle(value);
}

void AMXXMessageSystem::OrigWriteCoord(float value)
{
    g_origEngFuncs.pfnWriteCoord(value);
}

void AMXXMessageSystem::OrigWriteString(const char *value)
{
    g_origEngFuncs.pfnWriteString(value ? value : "");
}

void AMXXMessageSystem::OrigWriteEntity(int value)
{
    g_origEngFuncs.pfnWriteEntity(value);
}