#pragma once
#ifndef AMXX_NATIVE_ADMIN_H
#define AMXX_NATIVE_ADMIN_H

#include "amx.h"

extern AMX_NATIVE_INFO admin_natives[];

cell AMX_NATIVE_CALL amxx_register_clcmd(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_register_srvcmd(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_access(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_flags(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_user_flags(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_cmd_access(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_read_argc(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_read_argv(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_read_args(AMX *amx, cell *params);

void RegisterAdminNatives(AMX *amx);

cell AMX_NATIVE_CALL amxx_is_user_admin(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_remove_user_flags(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_has_rcon(AMX *amx, cell *params);

// P2-7: 客户端/服务端命令查询
cell AMX_NATIVE_CALL amxx_get_clcmd(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_clcmdsnum(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_srvcmd(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_srvcmdsnum(AMX *amx, cell *params);

#endif