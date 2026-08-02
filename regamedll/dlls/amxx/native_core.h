#pragma once
#ifndef AMXX_NATIVE_CORE_H
#define AMXX_NATIVE_CORE_H

#include "amx.h"

extern AMX_NATIVE_INFO core_natives[];

void RegisterCoreNatives(AMX *amx);
void ResetCoreNativeGlobals();
void SyncCvarBindings();
void ResetCvarBindings();
void PollCvarHooks();
void amxx_format_string(AMX *amx, cell *params, int fmtIndex, char *output, size_t outputSize);

// 在 OnServerActivate 调用，构建已知消息名 -> ID 映射
// 用于 get_user_msgid / get_user_msgname 反查 (替代原版 metamod GET_USER_MSG_ID/NAME)
void AMXX_BuildUserMsgMap();

cell AMX_NATIVE_CALL amxx_register_plugin(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_server_print(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_server_cmd(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_client_print(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_client_cmd(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_console_print(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_name(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_model(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_players(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_playersnum(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_maxplayers(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_is_user_alive(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_task(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_change_task(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_remove_task(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_task_exists(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_gametime(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_hudmessage(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_show_hudmessage(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_next_hudchannel(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_random_num(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_random_float(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_strlen(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_equal(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_contain(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_remove_quotes(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_read_argc(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_read_argv(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_read_argv_int(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_read_argv_float(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_read_args(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_log_amx(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_log_message(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_time(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_msgid(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_msgname(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_test_string(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_test_string2(AMX *amx, cell *params);

cell AMX_NATIVE_CALL amxx_numargs(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_getarg(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_setarg(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_heapspace(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_funcidx(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_swapchars(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_tolower(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_toupper(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_min(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_max(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_clamp(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_random(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_strcpy(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_strcat(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_strcmp(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_strfind(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_strtok(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_format(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_fopen(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_fclose(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_fread(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_fwrite(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_fseek(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_ftell(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_feof(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_file_size(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_floatround(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_floatsqroot(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_floatabs(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_floatcos(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_floatsin(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_floattan(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_cvar_string(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_cvar_num(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_cvar_string(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_cvar_num(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_register_cvar(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_mapname(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_change_map(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_client_kick(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_client_disconnect(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_ip(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_team(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_user_team(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_give_item(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_strip_user_weapons(AMX *amx, cell *params);

cell AMX_NATIVE_CALL amxx_give_weapon(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_drop_weapon(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_strip_weapon(AMX *amx, cell *params);

cell AMX_NATIVE_CALL amxx_is_user_connected(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_is_user_bot(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_is_user_hltv(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_health(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_user_health(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_armor(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_user_armor(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_frags(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_user_frags(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_deaths(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_user_deaths(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_weapon(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_weaponname(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_weapons(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_user_has_weapon(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_ammo(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_ping(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_time(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_origin(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_attacker(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_aiming(AMX *amx, cell *params);

cell AMX_NATIVE_CALL amxx_precache_model(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_precache_sound(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_precache_generic(AMX *amx, cell *params);

cell AMX_NATIVE_CALL amxx_set_dhudmessage(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_show_dhudmessage(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_show_motd(AMX *amx, cell *params);

cell AMX_NATIVE_CALL amxx_message_begin(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_message_end(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_write_byte(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_write_short(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_write_long(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_write_float(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_write_string(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_write_coord(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_message_begin_f(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_write_char(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_write_entity(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_write_angle(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_write_angle_f(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_write_coord_f(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_client_print_color(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_server_exec(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_task_ex(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_remove_tasks(AMX *amx, cell *params);

// 字符串操作扩展
cell AMX_NATIVE_CALL amxx_strtoupper(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_strtolower(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_strreplace(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_strins(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_strdel(AMX *amx, cell *params);

// 数学运算
cell AMX_NATIVE_CALL amxx_floatadd(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_floatmul(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_floatdiv(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_floatsub(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_floatmod(AMX *amx, cell *params);

// 位操作
cell AMX_NATIVE_CALL amxx_bit(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_bits(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_bit_set(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_bit_get(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_bit_test(AMX *amx, cell *params);

// 文件系统
cell AMX_NATIVE_CALL amxx_file_exists(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_file_delete(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_mkdir(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_read_dir(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_delete_dir(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_rename_file(AMX *amx, cell *params);

// 字符串操作扩展
cell AMX_NATIVE_CALL amxx_copy(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_add(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_num_to_str(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_str_to_num(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_float_to_str(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_str_to_float(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_floatstr(AMX *amx, cell *params);   // float.inc 别名
cell AMX_NATIVE_CALL amxx_floatfract(AMX *amx, cell *params);  // float.inc 小数部分
cell AMX_NATIVE_CALL amxx_trim(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_containi(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_equali(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_strncmp(AMX *amx, cell *params);

// 数学运算扩展
cell AMX_NATIVE_CALL amxx_abs(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_ceil(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_floor(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_pow(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_sqrt(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_log(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_log10(AMX *amx, cell *params);

// 位操作扩展
cell AMX_NATIVE_CALL amxx_bit_reset(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_bit_flip(AMX *amx, cell *params);

// AMX参数操作
cell AMX_NATIVE_CALL amxx_set_string(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_param_byref(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_param_byref(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_float_byref(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_float_byref(AMX *amx, cell *params);

// 数据结构
cell AMX_NATIVE_CALL amxx_arrayset(AMX *amx, cell *params);

// 函数调用
cell AMX_NATIVE_CALL amxx_callfunc_begin(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_callfunc_begin_i(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_callfunc_end(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_callfunc_push_int(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_callfunc_push_float(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_callfunc_push_str(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_callfunc_push_array(AMX *amx, cell *params);

// P1: 服务器信息
cell AMX_NATIVE_CALL amxx_is_dedicated_server(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_is_linux_server(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_amxx_verstring(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_server_name(AMX *amx, cell *params);
// P1: 目录路径
cell AMX_NATIVE_CALL amxx_get_configsdir(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_datadir(AMX *amx, cell *params);
// P1: localinfo
cell AMX_NATIVE_CALL amxx_set_localinfo(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_localinfo(AMX *amx, cell *params);
// P1: 音效
cell AMX_NATIVE_CALL amxx_emit_sound(AMX *amx, cell *params);
// P1: CVar 增强
cell AMX_NATIVE_CALL amxx_get_cvar_flags(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_cvar_flags(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_cvar_pointer(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_query_client_cvar(AMX *amx, cell *params);
// P1: 插件管理
cell AMX_NATIVE_CALL amxx_plugin_flags(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_module_exists(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_plugins(AMX *amx, cell *params);
// P1: 字符串工具
cell AMX_NATIVE_CALL amxx_replace_string(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_replace_all(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_parse(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_sort_custom_1d(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_sort_custom_2d(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_SortCustom2D(AMX *amx, cell *params);  // sorting.inc 标准名 (callback 版)
// P3: 排序
cell AMX_NATIVE_CALL amxx_sort_integ(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_sort_float(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_sort_str(AMX *amx, cell *params);

// P2: 时间处理
cell AMX_NATIVE_CALL amxx_get_systime(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_parse_time(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_format_time(AMX *amx, cell *params);
// P2: 字符串
cell AMX_NATIVE_CALL amxx_split(AMX *amx, cell *params);
// P2: pcvar
cell AMX_NATIVE_CALL amxx_pcvar_num(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_pcvar_num(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_pcvar_string(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_pcvar_num(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_pcvar_string(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_pcvar_flags(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_pcvar_flags(AMX *amx, cell *params);

// cvars.inc: 补全 9 个 native
cell AMX_NATIVE_CALL amxx_create_cvar(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_cvar_exists(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_hook_cvar_change(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_disable_cvar_hook(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_enable_cvar_hook(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_remove_cvar_flags(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_pcvar_bool(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_pcvar_bounds(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_pcvar_bounds(AMX *amx, cell *params);

// P3: 参数/字符串工具
cell AMX_NATIVE_CALL amxx_argbreak(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_string_to_array(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_array_to_string(AMX *amx, cell *params);

cell AMX_NATIVE_CALL amxx_get_user_userid(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_authid(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_weaponname(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_weapon_id(AMX *amx, cell *params);

// 高优先级缺失
cell AMX_NATIVE_CALL amxx_get_basedir(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_build_path(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_amxx_version(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_amxx_time(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_info(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_user_info(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_register_cvar_change(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_console_cmd(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_color_chat(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_color_chat_team(AMX *amx, cell *params);

// P3: 玩家数据扩展
cell AMX_NATIVE_CALL amxx_get_user_velocity(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_rendering(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_maxspeed(AMX *amx, cell *params);

// P2: CS 统计数据
cell AMX_NATIVE_CALL amxx_get_user_stats(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_reset_user_stats(AMX *amx, cell *params);

// P1: Bot 控制
cell AMX_NATIVE_CALL amxx_server_cmd_ex(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_engclient_cmd(AMX *amx, cell *params);
// P1: 文件操作高级封装
cell AMX_NATIVE_CALL amxx_read_file(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_write_file(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_write_filepos(AMX *amx, cell *params);

// 数学扩展
cell AMX_NATIVE_CALL amxx_floatasin(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_floatacos(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_floatatan(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_floatatan2(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_floatsinh(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_floatcosh(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_floattanh(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_floatlog(AMX *amx, cell *params);

// 核心扩展
cell AMX_NATIVE_CALL amxx_register_native(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_func_id(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_fail_state(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_param(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_param_f(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_string(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_array_f(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_array_f(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_array(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_array(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_param_convert(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_callfunc_push_intrf(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_callfunc_push_floatrf(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_register_library(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_create_one_forward(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_prepare_array(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_read_data(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_kvd(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_vdformat(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_log_error(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_execute_forward(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_create_multi_forward(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_create_multi_forward_ex(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_destroy_forward(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_forward_func_count(AMX *amx, cell *params);

// 玩家状态扩展
cell AMX_NATIVE_CALL amxx_set_user_origin(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_velocity_by_aim(AMX *amx, cell *params);

// 其他 natives
cell AMX_NATIVE_CALL amxx_pause(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_unpause(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_isalnum(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_num_to_word(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_module_filter(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_native_filter(AMX *amx, cell *params);

// xvar natives
cell AMX_NATIVE_CALL amxx_get_xvar_id(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_xvar_num(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_xvar_num(AMX *amx, cell *params);

// fread_blocks
cell AMX_NATIVE_CALL amxx_fread_blocks(AMX *amx, cell *params);

// file.inc 补全: 27 个文件/目录 native
cell AMX_NATIVE_CALL amxx_fread_raw(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_fwrite_blocks(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_fwrite_raw(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_fputs(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_fprintf(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_fgetc(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_fputc(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_fungetc(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_fflush(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_filesize(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_dir_exists(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_rmdir(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_unlink(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_open_dir(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_next_file(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_close_dir(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_LoadFileForMe(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_GetFileTime(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_SetFilePermissions(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_FileReadInt8(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_FileReadUint8(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_FileReadInt16(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_FileReadUint16(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_FileReadInt32(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_FileWriteInt8(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_FileWriteInt16(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_FileWriteInt32(AMX *amx, cell *params);

// P2-1 & P2-2: utility natives
cell AMX_NATIVE_CALL amxx_microsec(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_md5(AMX *amx, cell *params);

// 补全核心 natives
cell AMX_NATIVE_CALL amxx_get_user_index(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_amxclient_cmd(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_CreateHudSyncObj(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_ShowSyncHudMsg(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_ClearSyncHud(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_user_menu(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_read_logdata(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_read_logargc(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_read_logargv(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_parse_loguser(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_xvar_exists(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_is_user_authorized(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_force_unmodified(AMX *amx, cell *params);

// P2-8: 补全核心 natives (event data / xvar float / player / weapon / hash / precache)
cell AMX_NATIVE_CALL amxx_read_datanum(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_read_datatype(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_xvar_float(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_xvar_float(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_find_player_ex(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_weaponid(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_has_map_ent_class(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_engclient_print(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_precache_event(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_is_jit_enabled(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_md5_file(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_request_frame(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_autoexec_config(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_hash_string(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_hash_file(AMX *amx, cell *params);

// P2-9: 补全核心 natives (engine log / module / arch / debug / addr)
cell AMX_NATIVE_CALL amxx_elog_message(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_require_module(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_is_amd64_server(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_error_filter(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_dbg_trace_begin(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_dbg_trace_next(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_dbg_trace_info(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_dbg_fmt_error(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_int3(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_var_addr(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_addr_val(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_addr_val(AMX *amx, cell *params);

// 日志事件数据存储 (由 events.cpp OnLogMessage 调用)
void AMXX_SetLogEventData(const char *logString);

// string.inc: 补全 21 个缺失 native
cell AMX_NATIVE_CALL amxx_replace_stringex(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_fmt(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_format_args(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_strtol(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_strtof(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_copyc(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_setc(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_strtok2(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_mb_strtolower(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_mb_strtoupper(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_mb_ucfirst(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_mb_strtotitle(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_is_string_category(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_isdigit(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_isalpha(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_is_char_mb(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_is_char_upper(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_is_char_lower(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_char_bytes(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_argparse(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_split_string(AMX *amx, cell *params);

#endif
