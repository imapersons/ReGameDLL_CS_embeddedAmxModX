// messages.h
#pragma once
#ifndef AMXX_MESSAGES_H
#define AMXX_MESSAGES_H

#include "amx.h"
#include <vector>
#include <map>
#include <string>

enum MsgArgType {
    MSGARG_NONE,
    MSGARG_BYTE,
    MSGARG_CHAR,
    MSGARG_SHORT,
    MSGARG_LONG,
    MSGARG_ANGLE,
    MSGARG_COORD,
    MSGARG_STRING,
    MSGARG_ENTITY,
    MSGARG_FLOAT   // 新增，用于明确表示浮点数
};

// 消息阻塞状态 (与原版 AMXX messages.h / message_const.inc 一致)
#define BLOCK_NOT  0  // 不阻塞
#define BLOCK_ONCE 1  // 阻塞一次, 消息结束后自动解除
#define BLOCK_SET  2  // 永久阻塞 (直到再次设置)

struct MsgArg {
    MsgArgType type;
    union {
        int iVal;
        float fVal;
    };
    std::string sVal;
};

struct MsgHookInfo {
    AMX *amx;
    cell funcidx;
    int msgId;
    int flags;
    int handle;   // 唯一句柄 (register_message 返回值, 供 unregister_message 使用)

    MsgHookInfo() : amx(nullptr), funcidx(0), msgId(0), flags(0), handle(0) {}
};

class AMXXMessageSystem {
public:
    static AMXXMessageSystem &GetInstance();

    void Init();
    void Clear();  // 跨地图清理（保留引擎 hook，清空插件注册的 hooks）

    int RegisterHook(AMX *amx, int msgId, cell funcidx, int flags);  // 返回注册句柄
    void UnregisterHooks(AMX *amx);
    bool UnregisterHook(int msgId, int handle);  // unregister_message(msgid, handle)

    // 按消息 ID 阻塞控制 (原版 AMXX msgBlocks[msgid] 语义)
    void SetMsgBlock(int msgId, int block);
    int GetMsgBlock(int msgId) const;

    void BeginMessage(int dest, int msgType, const float *origin, edict_t *pEntity);
    void EndMessage();

    void WriteByte(int value);
    void WriteChar(int value);
    void WriteShort(int value);
    void WriteLong(int value);
    void WriteAngle(float value);
    void WriteCoord(float value);
    void WriteString(const char *value);
    void WriteEntity(int value);

    // emessage_*/ewrite_* 系列: 绕过 AMXX 消息钩子层 (register_message/register_event),
    // 直接调用引擎原始函数。供 emessage_begin/ewrite_* 等 native 使用。
    void OrigMessageBegin(int dest, int msgType, const float *origin, edict_t *pEntity);
    void OrigMessageEnd();
    void OrigWriteByte(int value);
    void OrigWriteChar(int value);
    void OrigWriteShort(int value);
    void OrigWriteLong(int value);
    void OrigWriteAngle(float value);
    void OrigWriteCoord(float value);
    void OrigWriteString(const char *value);
    void OrigWriteEntity(int value);

    // 参数访问
    int GetArgInt(int index);
    float GetArgFloat(int index);
    void GetArgString(int index, char *dest, int maxlen);
    MsgArgType GetArgType(int index);

    // 参数修改（用于 register_message 钩子）
    void SetArgInt(int index, int value);
    void SetArgString(int index, const char *value);
    void SetArgFloat(int index, float value);  // 新增：修正浮点数设置

    // 状态查询
    int GetCurrentMsgId() const { return m_currentMsgId; }
    int GetCurrentDest() const { return m_currentDest; }
    int GetArgCount() const { return (int)m_args.size(); }

    // 当前消息 origin (BeginMessage 保存的 pOrigin, 供 get_msg_origin 与重发使用)
    bool GetCurrentOrigin(float out[3]) const
    {
        if (!m_hasOrigin)
            return false;
        out[0] = m_currentOrigin[0];
        out[1] = m_currentOrigin[1];
        out[2] = m_currentOrigin[2];
        return true;
    }

    int GetOrigRetVal() const { return m_origRetVal; }
    void SetOrigRetVal(int val) { m_origRetVal = val; }
    int GetCurrentMsgType() const { return m_currentMsgType; }
    int GetCurrentEntity() const { return m_currentEntity ? ENTINDEX(m_currentEntity) : 0; }

private:
    AMXXMessageSystem();

    bool HasHook(int msgId);
    int FireHooks();

    std::vector<MsgHookInfo> m_hooks;

    bool m_inMessage;
    bool m_inBlock;       // 当前消息被按 ID 阻塞 (吞掉 Begin/Write/End)
    int m_blockDepth;     // 阻塞消息嵌套深度
    int m_blockMsgId;     // 当前阻塞的消息 ID
    std::map<int, int> m_msgBlocks;  // msgid -> BLOCK_NOT/BLOCK_ONCE/BLOCK_SET
    int m_nextHandle;     // register_message 句柄分配计数器
    int m_origRetVal;
    int m_currentMsgType;
    int m_currentDest;
    int m_currentMsgId;
    edict_t *m_currentEntity;
    float m_currentOrigin[3];  // BeginMessage 的 origin
    bool m_hasOrigin;
    std::vector<MsgArg> m_args;
};

#endif