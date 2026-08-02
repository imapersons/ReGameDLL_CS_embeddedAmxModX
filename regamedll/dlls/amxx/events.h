#pragma once
#ifndef AMXX_EVENTS_H
#define AMXX_EVENTS_H

#include "amx.h"
#include <vector>
#include <map>
#include <string>

#define MAX_REG_MSGS 256
#define MAX_AMX_REG_MSG (MAX_REG_MSGS + 16)

// 消息参数类型 (与 messages.h 中的 MsgArgType 对应)
enum EventArgType {
    EVTARG_BYTE = 0,
    EVTARG_CHAR,
    EVTARG_SHORT,
    EVTARG_LONG,
    EVTARG_ANGLE,
    EVTARG_COORD,
    EVTARG_STRING,
    EVTARG_ENTITY
};

struct EventArg {
    int type;
    int iVal;
    float fVal;
    std::string sVal;
};

// 事件条件（过滤器）- 支持 =, !=, &, <, > 操作符
struct EventCondition {
    int paramId;       // 参数索引（0-based）
    char op;           // 操作符: '=', '!', '&', '<', '>'
    int iValue;        // 整数值
    float fValue;      // 浮点值
    std::string sValue; // 字符串值
    int type;          // 值类型: 0=int, 1=float, 2=string
    EventCondition *next;

    EventCondition() : paramId(0), op('='), iValue(0), fValue(0), type(0), next(nullptr) {}
};

// 事件 flags (与原版 AMXX register_event 一致)
enum EventFlags {
    EVTFLAG_WORLD   = (1 << 0),  // a: 世界事件
    EVTFLAG_CLIENT  = (1 << 1),  // b: 客户端事件
    EVTFLAG_ONCE    = (1 << 2),  // c: 只触发一次
    EVTFLAG_DEAD    = (1 << 3),  // d: 仅死亡玩家
    EVTFLAG_ALIVE   = (1 << 4),  // e: 仅存活玩家
    EVTFLAG_PLAYER  = (1 << 5),  // f: 仅人类玩家
    EVTFLAG_BOT     = (1 << 6),  // g: 仅 Bot
};

struct EventHook {
    AMX *amx;
    cell funcidx;
    int handle;  // 全局唯一事件 handle (由 m_nextEventHandle 分配, 从 1000 开始)
    std::string eventName;
    int flags;
    int msgId;
    int spForwardId;  // SP forward ID for this event

    EventCondition *conditions;  // 条件列表
    int m_Stamp;  // 用于 'c' (ONCE) 标志: 记录上次触发的 map stamp (0 = 本地图未触发过)
    bool enabled;  // 事件是否启用（enable_event/disable_event 控制）

    EventHook() : amx(nullptr), funcidx(-1), handle(0), flags(0), msgId(-1), spForwardId(0), conditions(nullptr), m_Stamp(0), enabled(true) {}
};

struct MessageHook {
    AMX *amx;
    cell funcidx;
    int msgId;
    int flags;

    MessageHook() : amx(nullptr), funcidx(-1), msgId(-1), flags(0) {}
};

// LogEvent 过滤器: "argnum=pattern" (精确匹配) 或 "argnum&pattern" (子串匹配)
struct LogEventFilter {
    int argnum;        // 日志参数索引 (0-based)
    bool substring;    // true = 子串匹配 (&), false = 精确匹配 (=)
    std::string text;  // 匹配文本
};

struct LogEventHook {
    AMX *amx;
    cell funcidx;
    int spForwardId;
    int argsnum;       // 期望的日志参数数量 (pos), 仅当日志参数数 == argsnum 时触发
    bool enabled;
    std::vector<LogEventFilter> filters;

    LogEventHook() : amx(nullptr), funcidx(-1), spForwardId(0), argsnum(0), enabled(true) {}
};

// 解析数据参数的临时存储（m_ParseVault/m_ReadVault 概念）
struct ParsedArg {
    int iValue;
    float fValue;
    std::string sValue;
    int type;  // 0=int, 1=float, 2=string

    ParsedArg() : iValue(0), fValue(0), type(0) {}
};

class AMXXEventSystem {
public:
    static AMXXEventSystem &GetInstance();

    void Init();

    // 核心事件注册
    int RegisterEvent(AMX *amx, const char *eventName, int funcidx, int flags);
    void UnregisterEvents(AMX *amx);
    void UnregisterEvent(int handle);

    // 消息注册
    int RegisterMessage(AMX *amx, int msgId, int funcidx, int flags);
    void UnregisterMessages(AMX *amx);

    // 事件条件（过滤器）添加
    void AddEventCondition(int eventHandle, const char *filter);

    // 启用/禁用事件钩子
    void SetEventEnabled(int eventHandle, bool enabled);

    // Log events (register_logevent)
    int RegisterLogEvent(AMX *amx, const char *funcname, int argsnum);
    void AddLogEventFilter(int handle, const char *filter);
    void SetLogEventEnabled(int handle, bool enabled);
    void OnLogMessage(const char *logString);

    // 触发事件/消息
    void FireEvent(const char *eventName, int numArgs, const char **args);
    void FireMessage(int msgId, int dest, edict_t *pEntity, const void *data, int size);

    // 内部使用：当消息发送时调用（由消息系统触发）
    void OnMessage(int msgId, int dest, edict_t *pEntity);

    // 检查是否有事件钩子注册了指定消息ID
    bool HasEventHook(int msgId);

    // 参数读取（供插件 read_data 使用）
    const char *GetEventArg(int index);
    int GetEventArgInt(int index);
    float GetEventArgFloat(int index);
    const char *GetEventArgString(int index);
    int GetEventArgsCount() const { return (int)m_readVault.size(); }
    const char *GetCurrentEventName() const { return m_currentEventName.c_str(); }
    int GetCurrentMsgType() const { return m_readMsgType; }

    // 消息 ID 查询（动态）
    int GetMsgIdByName(const char *name);

    // 清理
    void Clear();

    // 事件 flags 解析
    static int ParseEventFlags(const char *flagStr);

private:
    AMXXEventSystem();

    // 内部方法
    bool CheckEventConditions(const EventHook &hook);
    bool CheckEventFlags(const EventHook &hook, edict_t *pEntity);
    int ExecuteEventCallback(EventHook &hook);  // 返回插件返回值
    void UpdateReadVault();

    // 数据存储
    std::vector<EventHook> m_eventHooks;
    std::vector<MessageHook> m_messageHooks;
    std::vector<EventHook> m_msgHooks;  // 专门存储消息事件钩子（由 register_event 注册的 msg 类型）
    std::vector<LogEventHook> m_logEventHooks;  // register_logevent 注册的钩子

    // 当前解析/读取状态
    std::vector<ParsedArg> m_parseVault;    // 解析中写入
    std::vector<ParsedArg> m_readVault;    // 读取时使用（快照）
    int m_parsePos;
    int m_readPos;
    int m_parseMsgType;
    int m_readMsgType;

    // 事件参数（用于 FireEvent）
    std::vector<EventArg> m_currentArgs;
    std::vector<std::string> m_currentEventArgs;
    std::string m_currentEventName;

    // 动态消息 ID 缓存
    std::map<std::string, int> m_msgIdCache;

    // 当前是否为消息事件（影响 get_data_args 返回值）
    bool m_isMessageEvent;

    // 'c' (ONCE) 标志: 当前 map stamp。每次换图 (Clear) 时递增。
    // 新 hook 的 m_Stamp=0, 因 0 != m_mapStamp (>=1) 故首次触发, 触发后置为 m_mapStamp。
    int m_mapStamp;

    // OnMessage 重入保护: 嵌套消息调用时保存/恢复 m_readVault 状态
    int m_inExecute;

    // OnLogMessage 重入保护: 防止 logevent 回调中再次 log 触发递归
    int m_inLogExecute;

    // 全局唯一事件 handle 计数器 (register_event / register_event_ex 使用)
    int m_nextEventHandle;
};

// 事件 flags 字符串解析辅助
int UTIL_ReadEventFlags(const char *str);

#endif