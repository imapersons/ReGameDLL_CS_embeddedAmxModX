#include "precompiled.h"
#include "native_messages.h"
#include "messages.h"
#include "amx.h"

cell AMX_NATIVE_CALL amxx_register_message(AMX *amx, cell *params)
{
    int msgId = (int)params[1];

    // 原版 AMXX: 仅接受 1 <= msgId < 256 (register_message 校验)
    if (msgId < 1 || msgId > 255) {
        AMXX_LOG_ERR("[Messages] register_message: Invalid message id %d", msgId);
        return 0;
    }

    cell *funcname_addr;
    amx_GetAddr(amx, params[2], &funcname_addr);
    char funcname[256];
    amx_GetString(funcname, funcname_addr, 0, sizeof(funcname));

    int funcidx;
    if (amx_FindPublic(amx, funcname, &funcidx) != AMX_ERR_NONE) {
        AMXX_LOG_ERR("[Messages] register_message: Could not find function \"%s\"", funcname);
        return -1;
    }

    int flags = 0;
    if (params[0] / sizeof(cell) >= 3)
        flags = (int)params[3];

    // 返回唯一句柄, 供 unregister_message(msgid, handle) 使用
    return AMXXMessageSystem::GetInstance().RegisterHook(amx, msgId, funcidx, flags);
}

cell AMX_NATIVE_CALL amxx_get_msg_arg_int(AMX *amx, cell *params)
{
    (void)amx;
    return AMXXMessageSystem::GetInstance().GetArgInt((int)params[1]);
}

cell AMX_NATIVE_CALL amxx_get_msg_arg_float(AMX *amx, cell *params)
{
    (void)amx;
    float val = AMXXMessageSystem::GetInstance().GetArgFloat((int)params[1]);
    return amx_ftoc(val);
}

cell AMX_NATIVE_CALL amxx_get_msg_arg_string(AMX *amx, cell *params)
{
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    char buffer[1024];
    AMXXMessageSystem::GetInstance().GetArgString((int)params[1], buffer, sizeof(buffer));
    return amx_SetString(dest, buffer, 0, 0, params[3]);
}

cell AMX_NATIVE_CALL amxx_set_msg_arg_int(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    (void)params[2];
    int value = (int)params[3];
    AMXXMessageSystem::GetInstance().SetArgInt(index, value);
    return 1;
}

cell AMX_NATIVE_CALL amxx_set_msg_arg_string(AMX *amx, cell *params)
{
    cell *value_addr;
    amx_GetAddr(amx, params[3], &value_addr);
    char value[1024];
    amx_GetString(value, value_addr, 0, sizeof(value));
    (void)params[2];
    AMXXMessageSystem::GetInstance().SetArgString((int)params[1], value);
    return 1;
}

// set_msg_arg_float(arg, type, Float:value)
// 原版 AMXX messages.inc: 设置消息参数为浮点值 (与 set_msg_arg_int 对齐, 3 参数)
// type 参数被忽略 (与 set_msg_arg_int 一致), 直接通过 SetArgFloat 写入
cell AMX_NATIVE_CALL amxx_set_msg_arg_float(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    (void)params[2];  // type — 与 set_msg_arg_int 一致, 当前实现不依赖类型
    float value = amx_ctof(params[3]);
    AMXXMessageSystem::GetInstance().SetArgFloat(index, value);
    return 1;
}

cell AMX_NATIVE_CALL amxx_get_msg_args(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return AMXXMessageSystem::GetInstance().GetArgCount();
}

int AMX_NATIVE_CALL amxx_get_msg_dest(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return AMXXMessageSystem::GetInstance().GetCurrentDest();
}

int AMX_NATIVE_CALL amxx_get_msg_entity(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return AMXXMessageSystem::GetInstance().GetCurrentEntity();
}

int AMX_NATIVE_CALL amxx_get_msg_nameid(AMX *amx, cell *params)
{
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char name[256];
    amx_GetString(name, name_addr, 0, sizeof(name));
    int msgId = REG_USER_MSG(name, -1);
    return msgId;
}

int AMX_NATIVE_CALL amxx_get_msg_origin(AMX *amx, cell *params)
{
    cell *dest;
    amx_GetAddr(amx, params[1], &dest);

    AMXXMessageSystem &msgs = AMXXMessageSystem::GetInstance();

    // 原版 AMXX get_msg_origin: 仅 PVS/PAS 类目的消息才有有效 origin,
    // 其他目的返回 {0,0,0}
    int msgDest = msgs.GetCurrentDest();
    if (msgDest >= MSG_PVS && msgDest <= MSG_PAS_R) {
        float origin[3] = {0.0f, 0.0f, 0.0f};
        msgs.GetCurrentOrigin(origin);
        dest[0] = amx_ftoc(origin[0]);
        dest[1] = amx_ftoc(origin[1]);
        dest[2] = amx_ftoc(origin[2]);
    } else {
        dest[0] = 0;
        dest[1] = 0;
        dest[2] = 0;
    }
    return 1;
}

AMX_NATIVE_INFO message_natives[] = {
    {"register_message", amxx_register_message},
    {"get_msg_arg_int", amxx_get_msg_arg_int},
    {"get_msg_arg_float", amxx_get_msg_arg_float},
    {"get_msg_arg_string", amxx_get_msg_arg_string},
    {"set_msg_arg_int", amxx_set_msg_arg_int},
    {"set_msg_arg_string", amxx_set_msg_arg_string},
    {"set_msg_arg_float", amxx_set_msg_arg_float},
    {"get_msg_args", amxx_get_msg_args},
    {"unregister_message", amxx_unregister_message},
    {"set_msg_block", amxx_set_msg_block},
    {"get_msg_block", amxx_get_msg_block},
    {"get_orig_retval", amxx_get_orig_retval},
    {"set_msg_float", amxx_set_msg_float},
    {"get_msg_float", amxx_get_msg_float},
    // P2: 消息系统增强
    {"get_msg_dest", amxx_get_msg_dest},
    {"get_msg_entity", amxx_get_msg_entity},
    {"get_msg_nameid", amxx_get_msg_nameid},
    {"get_msg_origin", amxx_get_msg_origin},
    // P1-7: get_msg_argtype
    {"get_msg_argtype", amxx_get_msg_argtype},
    // P1-6: emessage_*/ewrite_* 系列 (绕过 AMXX 消息钩子层, 直接发送到引擎)
    {"emessage_begin", amxx_emessage_begin},
    {"emessage_begin_f", amxx_emessage_begin_f},
    {"emessage_end", amxx_emessage_end},
    {"ewrite_byte", amxx_ewrite_byte},
    {"ewrite_char", amxx_ewrite_char},
    {"ewrite_short", amxx_ewrite_short},
    {"ewrite_long", amxx_ewrite_long},
    {"ewrite_angle", amxx_ewrite_angle},
    {"ewrite_angle_f", amxx_ewrite_angle_f},
    {"ewrite_coord", amxx_ewrite_coord},
    {"ewrite_coord_f", amxx_ewrite_coord_f},
    {"ewrite_entity", amxx_ewrite_entity},
    {"ewrite_string", amxx_ewrite_string},
    {nullptr, nullptr}
};

cell AMX_NATIVE_CALL amxx_unregister_message(AMX *amx, cell *params)
{
    // 原版 AMXX: unregister_message(iMsgId, registeredmsg) 注销指定句柄
    int msgId = (int)params[1];
    int handle = (int)params[2];

    if (msgId < 1 || msgId > 255) {
        AMXX_LOG_ERR("[Messages] unregister_message: Invalid message id %d", msgId);
        return 0;
    }
    if (handle == -1) {
        AMXX_LOG_ERR("[Messages] unregister_message: Invalid registered message handle");
        return -1;
    }
    if (AMXXMessageSystem::GetInstance().UnregisterHook(msgId, handle))
        return handle;

    AMXX_LOG_ERR("[Messages] unregister_message: Invalid registered message handle (msgid=%d handle=%d)", msgId, handle);
    return -1;
}

cell AMX_NATIVE_CALL amxx_set_msg_block(AMX *amx, cell *params)
{
    (void)amx;
    int msgid = (int)params[1];
    int block = (int)params[2];

    // 原版 AMXX: 仅接受 1 <= msgid <= 255
    if (msgid < 1 || msgid > 255) {
        AMXX_LOG_ERR("[Messages] set_msg_block: Invalid message id %d", msgid);
        return 0;
    }

    // block 语义: BLOCK_NOT=0 (取消) / BLOCK_ONCE=1 (阻塞一次) / BLOCK_SET=2 (永久阻塞)
    AMXXMessageSystem::GetInstance().SetMsgBlock(msgid, block);
    return 1;
}

cell AMX_NATIVE_CALL amxx_get_msg_block(AMX *amx, cell *params)
{
    (void)amx;
    int msgid = (int)params[1];

    if (msgid < 1 || msgid > 255) {
        AMXX_LOG_ERR("[Messages] get_msg_block: Invalid message id %d", msgid);
        return 0;
    }

    return AMXXMessageSystem::GetInstance().GetMsgBlock(msgid);
}

cell AMX_NATIVE_CALL amxx_get_orig_retval(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return AMXXMessageSystem::GetInstance().GetOrigRetVal();
}

cell AMX_NATIVE_CALL amxx_set_msg_float(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    float value = amx_ctof(params[3]);
    // 直接设置浮点值，不改变类型
    // 假设参数类型为 MSGARG_ANGLE 或 MSGARG_COORD，或者通过 SetArgInt 但保留类型
    // 更好的方法是添加 SetArgFloat 方法
    AMXXMessageSystem::GetInstance().SetArgFloat(index, value);
    return 1;
}

cell AMX_NATIVE_CALL amxx_get_msg_float(AMX *amx, cell *params)
{
    (void)amx;
    float val = AMXXMessageSystem::GetInstance().GetArgFloat((int)params[1]);
    return amx_ftoc(val);
}

// ========== P1-7: get_msg_argtype ==========
// native get_msg_argtype(arg);
// 返回值与原版 AMXX msgtype 枚举一致: arg_byte=1, arg_char=2, arg_short=3,
// arg_long=4, arg_angle=5, arg_coord=6, arg_string=7, arg_entity=8。
// 嵌入版的 MsgArgType 枚举数值已与此一致, 故直接返回类型枚举值。
cell AMX_NATIVE_CALL amxx_get_msg_argtype(AMX *amx, cell *params)
{
    (void)amx;
    MsgArgType type = AMXXMessageSystem::GetInstance().GetArgType((int)params[1]);
    // P2: MSGARG_FLOAT(=9) 是嵌入版新增类型, 超出原版 msgtype 枚举域(1-8)。
    // Xash 下 write_float 按 long 写入, 归一化为 MSGARG_LONG(4) 保持枚举域。
    if (type == MSGARG_FLOAT)
        return (cell)MSGARG_LONG;
    return (cell)type;
}

// ========== P1-6: emessage_*/ewrite_* 系列 ==========
// 'e' 前缀变体绕过 AMXX 消息钩子层 (register_message/register_event),
// 直接调用引擎原始消息函数发送。与原版 AMXX emsg.cpp 行为一致。
static cell amxx_emessage_begin_impl(AMX *amx, cell *params, bool useFloat)
{
    int numparam = params[0] / sizeof(cell);
    int dest = params[1];
    int msgType = params[2];

    if (msgType <= 0)
        return 0;

    AMXXMessageSystem &msgs = AMXXMessageSystem::GetInstance();

    switch (dest) {
        case MSG_BROADCAST:
        case MSG_ALL:
        case MSG_SPEC:
            msgs.OrigMessageBegin(dest, msgType, nullptr, nullptr);
            break;
        case MSG_PVS: case MSG_PAS:
        case MSG_PVS_R: case MSG_PAS_R: {
            if (numparam < 3)
                return 0;
            cell *cpOrigin;
            amx_GetAddr(amx, params[3], &cpOrigin);
            if (!cpOrigin)
                return 0;
            float vecOrigin[3];
            if (!useFloat) {
                vecOrigin[0] = (float)cpOrigin[0];
                vecOrigin[1] = (float)cpOrigin[1];
                vecOrigin[2] = (float)cpOrigin[2];
            } else {
                vecOrigin[0] = amx_ctof(cpOrigin[0]);
                vecOrigin[1] = amx_ctof(cpOrigin[1]);
                vecOrigin[2] = amx_ctof(cpOrigin[2]);
            }
            msgs.OrigMessageBegin(dest, msgType, vecOrigin, nullptr);
            break;
        }
        case MSG_ONE_UNRELIABLE:
        case MSG_ONE: {
            if (numparam < 4)
                return 0;
            edict_t *pEntity = INDEXENT(params[4]);
            msgs.OrigMessageBegin(dest, msgType, nullptr, pEntity);
            break;
        }
        default:
            msgs.OrigMessageBegin(dest, msgType, nullptr, nullptr);
            break;
    }
    return 1;
}

cell AMX_NATIVE_CALL amxx_emessage_begin(AMX *amx, cell *params)
{
    return amxx_emessage_begin_impl(amx, params, false);
}

cell AMX_NATIVE_CALL amxx_emessage_begin_f(AMX *amx, cell *params)
{
    return amxx_emessage_begin_impl(amx, params, true);
}

cell AMX_NATIVE_CALL amxx_emessage_end(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    AMXXMessageSystem::GetInstance().OrigMessageEnd();
    return 1;
}

cell AMX_NATIVE_CALL amxx_ewrite_byte(AMX *amx, cell *params)
{
    (void)amx;
    AMXXMessageSystem::GetInstance().OrigWriteByte((int)params[1]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_ewrite_char(AMX *amx, cell *params)
{
    (void)amx;
    AMXXMessageSystem::GetInstance().OrigWriteChar((int)params[1]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_ewrite_short(AMX *amx, cell *params)
{
    (void)amx;
    AMXXMessageSystem::GetInstance().OrigWriteShort((int)params[1]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_ewrite_long(AMX *amx, cell *params)
{
    (void)amx;
    AMXXMessageSystem::GetInstance().OrigWriteLong((int)params[1]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_ewrite_angle(AMX *amx, cell *params)
{
    (void)amx;
    AMXXMessageSystem::GetInstance().OrigWriteAngle((float)params[1]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_ewrite_angle_f(AMX *amx, cell *params)
{
    (void)amx;
    AMXXMessageSystem::GetInstance().OrigWriteAngle(amx_ctof(params[1]));
    return 1;
}

cell AMX_NATIVE_CALL amxx_ewrite_coord(AMX *amx, cell *params)
{
    (void)amx;
    AMXXMessageSystem::GetInstance().OrigWriteCoord((float)params[1]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_ewrite_coord_f(AMX *amx, cell *params)
{
    (void)amx;
    AMXXMessageSystem::GetInstance().OrigWriteCoord(amx_ctof(params[1]));
    return 1;
}

cell AMX_NATIVE_CALL amxx_ewrite_entity(AMX *amx, cell *params)
{
    (void)amx;
    AMXXMessageSystem::GetInstance().OrigWriteEntity((int)params[1]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_ewrite_string(AMX *amx, cell *params)
{
    cell *addr;
    amx_GetAddr(amx, params[1], &addr);
    char buf[1024];
    amx_GetString(buf, addr, 0, sizeof(buf));
    AMXXMessageSystem::GetInstance().OrigWriteString(buf);
    return 1;
}


void RegisterMessageNatives(AMX *amx)
{
    amx_Register(amx, message_natives, -1);
}
