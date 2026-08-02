#pragma once
#ifndef AMXX_NATIVE_MESSAGES_H
#define AMXX_NATIVE_MESSAGES_H

#include "amx.h"

extern AMX_NATIVE_INFO message_natives[];

cell AMX_NATIVE_CALL amxx_unregister_message(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_msg_block(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_msg_block(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_orig_retval(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_msg_float(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_msg_float(AMX *amx, cell *params);
// messages.inc: set_msg_arg_float(arg, type, Float:value)
cell AMX_NATIVE_CALL amxx_set_msg_arg_float(AMX *amx, cell *params);
// P2: 消息系统增强
cell AMX_NATIVE_CALL amxx_get_msg_dest(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_msg_entity(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_msg_nameid(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_msg_origin(AMX *amx, cell *params);

// P1-7: get_msg_argtype
cell AMX_NATIVE_CALL amxx_get_msg_argtype(AMX *amx, cell *params);

// P1-6: emessage_*/ewrite_* 系列 (绕过 AMXX 消息钩子层, 直接发送到引擎)
cell AMX_NATIVE_CALL amxx_emessage_begin(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_emessage_begin_f(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_emessage_end(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_ewrite_byte(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_ewrite_char(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_ewrite_short(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_ewrite_long(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_ewrite_angle(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_ewrite_angle_f(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_ewrite_coord(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_ewrite_coord_f(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_ewrite_entity(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_ewrite_string(AMX *amx, cell *params);

void RegisterMessageNatives(AMX *amx);

#endif
