#pragma once
#ifndef AMXX_NATIVE_MENUS_H
#define AMXX_NATIVE_MENUS_H

#include "amx.h"

extern AMX_NATIVE_INFO menu_natives[];

void RegisterMenuNatives(AMX *amx);

cell AMX_NATIVE_CALL amxx_menu_makecallback(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_player_menu_info(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_menu_cancel(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_menu_display_time(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_menu_addtext(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_menu_addblank(AMX *amx, cell *params);

// P2-6: 标准菜单 API
cell AMX_NATIVE_CALL amxx_menu_items(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_menu_find_id(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_menu_item_setcall(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_menu_item_setaccess(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_menu_item_setcmd(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_menu_item_setname(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_menu_addblank2(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_menu_addtext2(AMX *amx, cell *params);

#endif
