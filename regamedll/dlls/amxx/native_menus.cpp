#include "precompiled.h"
#include "native_menus.h"
#include "menus.h"
#include "amx.h"

#include "extdll.h"
#include "enginecallback.h"
#include "util.h"

// P2: 前向声明（menu_natives 注册表里会引用）
cell AMX_NATIVE_CALL amxx_menu_setcmd(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_menu_itemtext(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_menu_itemcmd(AMX *amx, cell *params);

cell AMX_NATIVE_CALL amxx_menu_create(AMX *amx, cell *params)
{
    cell *title_addr;
    amx_GetAddr(amx, params[1], &title_addr);
    char title[256];
    amx_GetString(title, title_addr, 0, sizeof(title));

    cell funcidx = -1;
    int numParams = (int)(params[0] / sizeof(cell));
    if (numParams >= 2) {
        cell *handler_addr;
        amx_GetAddr(amx, params[2], &handler_addr);
        char handler[256];
        amx_GetString(handler, handler_addr, 0, sizeof(handler));
        if (handler[0] != '\0')
            amx_FindPublic(amx, handler, &funcidx);
    }

    return AMXXMenuSystem::GetInstance().CreateMenu(title, amx, funcidx);
}

cell AMX_NATIVE_CALL amxx_menu_additem(AMX *amx, cell *params)
{
    int menuId = params[1];
    cell *name_addr, *info_addr;
    amx_GetAddr(amx, params[2], &name_addr);
    amx_GetAddr(amx, params[3], &info_addr);
    char name[256], info[256];
    amx_GetString(name, name_addr, 0, sizeof(name));
    amx_GetString(info, info_addr, 0, sizeof(info));
    return AMXXMenuSystem::GetInstance().AddMenuItem(menuId, name, info) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_menu_settitle(AMX *amx, cell *params)
{
    int menuId = params[1];
    cell *title_addr;
    amx_GetAddr(amx, params[2], &title_addr);
    char title[256];
    amx_GetString(title, title_addr, 0, sizeof(title));
    return AMXXMenuSystem::GetInstance().SetMenuTitle(menuId, title) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_menu_pages(AMX *amx, cell *params)
{
    (void)amx;
    int menuId = params[1];
    return AMXXMenuSystem::GetInstance().GetMenuPages(menuId);
}

cell AMX_NATIVE_CALL amxx_menu_display(AMX *amx, cell *params)
{
    (void)amx;
    int playerId = params[1];
    int menuId = params[2];
    int page = 0;
    int numParams = (int)(params[0] / sizeof(cell));
    if (numParams >= 3)
        page = params[3];

    return AMXXMenuSystem::GetInstance().ShowMenu(playerId, menuId, page) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_menu_destroy(AMX *amx, cell *params)
{
    (void)amx;
    return AMXXMenuSystem::GetInstance().DestroyMenu((int)params[1]) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_menu_item_getinfo(AMX *amx, cell *params)
{
    int menuId = params[1];
    int item = params[2];

    AMXXMenu *menu = AMXXMenuSystem::GetInstance().GetMenu(menuId);
    if (!menu)
        return 0;

    if (item < 0 || item >= menu->ItemCount())
        return 0;

    int numParams = (int)(params[0] / sizeof(cell));

    if (numParams >= 3) {
        cell *access_addr;
        if (amx_GetAddr(amx, params[3], &access_addr) == AMX_ERR_NONE)
            *access_addr = (cell)menu->GetItemAccess(item);
    }
    if (numParams >= 6) {
        cell *info_addr;
        if (amx_GetAddr(amx, params[4], &info_addr) == AMX_ERR_NONE)
            amx_SetString(info_addr, menu->GetItemInfo(item), 0, 0, params[5]);
    }
    if (numParams >= 8) {
        cell *name_addr;
        if (amx_GetAddr(amx, params[6], &name_addr) == AMX_ERR_NONE)
            amx_SetString(name_addr, menu->GetItemName(item), 0, 0, params[7]);
    }
    if (numParams >= 9) {
        cell *callback_addr;
        if (amx_GetAddr(amx, params[8], &callback_addr) == AMX_ERR_NONE)
            *callback_addr = (cell)menu->GetItemCallback(item);
    }

    return 1;
}

cell AMX_NATIVE_CALL amxx_show_menu(AMX *amx, cell *params)
{
    int playerId = params[1];
    int keys = params[2];
    int time = params[3];

    cell *title_addr;
    amx_GetAddr(amx, params[4], &title_addr);
    char title[512];
    amx_GetString(title, title_addr, 0, sizeof(title));

    if (playerId < 1 || playerId > gpGlobals->maxClients)
        return 0;

    edict_t *pEdict = INDEXENT(playerId);
    if (!pEdict || pEdict->free)
        return 0;

    int menuMsgId = REG_USER_MSG("ShowMenu", -1);
    MESSAGE_BEGIN(MSG_ONE, menuMsgId, nullptr, pEdict);
        WRITE_SHORT(keys);
        WRITE_CHAR(time > 0 ? time : -1);
        WRITE_BYTE(0);
        WRITE_STRING(title);
    MESSAGE_END();

    // 记录玩家当前显示的旧式菜单（供 register_menucmd 回调使用）
    AMXXMenuSystem::GetInstance().SetPlayerMenuCmd(playerId, menuMsgId);

    AMXX_LOG_DBG("[Menu] show_menu to player %d (keys=%d, time=%d)", playerId, keys, time);
    return 1;
}

cell AMX_NATIVE_CALL amxx_menu_cancel(AMX *amx, cell *params)
{
    int playerId = (int)params[1];
    (void)amx;
    return AMXXMenuSystem::GetInstance().CancelMenu(playerId) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_menu_display_time(AMX *amx, cell *params)
{
    int menuId = (int)params[1];
    int time = (int)params[2];
    (void)amx;
    AMXXMenuSystem::GetInstance().SetMenuDisplayTime(menuId, time);
    return 1;
}

cell AMX_NATIVE_CALL amxx_register_menuid(AMX *amx, cell *params)
{
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char menuname[256];
    amx_GetString(menuname, name_addr, 0, sizeof(menuname));

    // 返回引擎消息 ID，与原版 AMXX 一致
    return REG_USER_MSG(menuname, -1);
}

cell AMX_NATIVE_CALL amxx_register_menucmd(AMX *amx, cell *params)
{
    int menu_id = params[1];
    int keys = params[2];
    int handler = params[3];

    // 注册菜单命令到菜单系统
    AMXXMenuSystem::GetInstance().RegisterMenuCmd(menu_id, keys, amx, handler);

    AMXX_LOG_DBG("[Menu] Registered menucmd: menuid=%d, keys=%d, handler=%d", menu_id, keys, handler);
    return 1;
}

cell AMX_NATIVE_CALL amxx_menu_setprop(AMX *amx, cell *params)
{
    int menuId = (int)params[1];
    int prop = (int)params[2];
    int value = (int)params[3];
    
    AMXXMenu *menu = AMXXMenuSystem::GetInstance().GetMenu(menuId);
    if (!menu)
        return 0;
    
    // prop: 菜单属性
    // 0 = MPROP_EXITNAME - 设置退出项名称（暂不支持）
    // 1 = MPROP_NUMBER_OF_COLUMNS - 设置列数（暂不支持）
    // 2 = MPROP_BACKNAME - 设置返回项名称（暂不支持）
    // 3 = MPROP_NEXTNAME - 设置下一页名称（暂不支持）
    // 4 = MPROP_EXIT - 是否显示退出项
    // 5 = MPROP_NOEXIT - 禁止退出（暂不支持）
    // 6 = MPROP_PERPAGE - 每页显示项目数
    
    if (prop == 6) {
        // 设置每页项目数
        menu->SetItemsPerPage(value);
        return 1;
    }
    
    return 1;
}

// P2-6: 菜单标准 API 补全

// menu_items(menu) - 返回菜单项目数
cell AMX_NATIVE_CALL amxx_menu_items(AMX *amx, cell *params)
{
    (void)amx;
    AMXXMenu *menu = AMXXMenuSystem::GetInstance().GetMenu((int)params[1]);
    if (!menu)
        return 0;
    return menu->ItemCount();
}

// menu_find_id(menu, page, key) - 通过页/键位置查找项目 id
cell AMX_NATIVE_CALL amxx_menu_find_id(AMX *amx, cell *params)
{
    (void)amx;
    AMXXMenu *menu = AMXXMenuSystem::GetInstance().GetMenu((int)params[1]);
    if (!menu)
        return -1;
    return menu->FindItemId((int)params[2], (int)params[3]);
}

// menu_item_setcall(menu, item, callback=-1) - 设置项目回调
cell AMX_NATIVE_CALL amxx_menu_item_setcall(AMX *amx, cell *params)
{
    (void)amx;
    AMXXMenu *menu = AMXXMenuSystem::GetInstance().GetMenu((int)params[1]);
    if (!menu)
        return 0;
    int callback = (params[0] / sizeof(cell) >= 3) ? (int)params[3] : -1;
    return menu->SetItemCallback((int)params[2], callback) ? 1 : 0;
}

// menu_item_setaccess(menu, item, access=0) - 设置项目权限
cell AMX_NATIVE_CALL amxx_menu_item_setaccess(AMX *amx, cell *params)
{
    (void)amx;
    AMXXMenu *menu = AMXXMenuSystem::GetInstance().GetMenu((int)params[1]);
    if (!menu)
        return 0;
    int access = (params[0] / sizeof(cell) >= 3) ? (int)params[3] : 0;
    return menu->SetItemAccess((int)params[2], access) ? 1 : 0;
}

// menu_item_setcmd(menu, item, const cmd[]) - 设置项目命令/info (标准名)
cell AMX_NATIVE_CALL amxx_menu_item_setcmd(AMX *amx, cell *params)
{
    return amxx_menu_setcmd(amx, params);
}

// menu_item_setname(menu, item, const name[]) - 设置项目显示名 (标准名)
cell AMX_NATIVE_CALL amxx_menu_item_setname(AMX *amx, cell *params)
{
    int menuId = (int)params[1];
    int item = (int)params[2];
    cell *name_addr;
    amx_GetAddr(amx, params[3], &name_addr);
    char name[256];
    amx_GetString(name, name_addr, 0, sizeof(name));

    AMXXMenu *menu = AMXXMenuSystem::GetInstance().GetMenu(menuId);
    if (!menu) return 0;
    return menu->SetItemName(item, name) ? 1 : 0;
}

// menu_addblank2(menu) - 在末尾添加空白行（无页偏移）
cell AMX_NATIVE_CALL amxx_menu_addblank2(AMX *amx, cell *params)
{
    (void)amx;
    int menuId = (int)params[1];
    return AMXXMenuSystem::GetInstance().AddMenuBlank(menuId) ? 1 : 0;
}

// menu_addtext2(menu, const text[]) - 在末尾添加文本行（无页偏移）
cell AMX_NATIVE_CALL amxx_menu_addtext2(AMX *amx, cell *params)
{
    int menuId = (int)params[1];
    cell *text_addr;
    amx_GetAddr(amx, params[2], &text_addr);
    char text[256];
    amx_GetString(text, text_addr, 0, sizeof(text));
    return AMXXMenuSystem::GetInstance().AddMenuText(menuId, text) ? 1 : 0;
}

AMX_NATIVE_INFO menu_natives[] = {
    {"menu_create", amxx_menu_create},
    {"menu_additem", amxx_menu_additem},
    {"menu_addtext", amxx_menu_addtext},
    {"menu_addblank", amxx_menu_addblank},
    {"menu_settitle", amxx_menu_settitle},
    {"menu_pages", amxx_menu_pages},
    {"menu_display", amxx_menu_display},
    {"menu_item_getinfo", amxx_menu_item_getinfo},
    {"menu_destroy", amxx_menu_destroy},
    {"show_menu", amxx_show_menu},
    {"menu_makecallback", amxx_menu_makecallback},
    {"player_menu_info", amxx_player_menu_info},
    {"menu_cancel", amxx_menu_cancel},
    {"menu_display_time", amxx_menu_display_time},
    {"register_menuid", amxx_register_menuid},
    {"register_menucmd", amxx_register_menucmd},
    {"menu_setprop", amxx_menu_setprop},
    {"menu_setcmd", amxx_menu_setcmd},
    {"menu_itemtext", amxx_menu_itemtext},
    {"menu_itemcmd", amxx_menu_itemcmd},
    // P2-6: 标准菜单 API
    {"menu_items", amxx_menu_items},
    {"menu_find_id", amxx_menu_find_id},
    {"menu_item_setcall", amxx_menu_item_setcall},
    {"menu_item_setaccess", amxx_menu_item_setaccess},
    {"menu_item_setcmd", amxx_menu_item_setcmd},
    {"menu_item_setname", amxx_menu_item_setname},
    {"menu_addblank2", amxx_menu_addblank2},
    {"menu_addtext2", amxx_menu_addtext2},
    {nullptr, nullptr}
};

// P2: 菜单增强

// menu_addtext(menuid, text[]) - 添加纯文本行（不可选）
cell AMX_NATIVE_CALL amxx_menu_addtext(AMX *amx, cell *params)
{
    int menuId = (int)params[1];
    cell *text_addr;
    amx_GetAddr(amx, params[2], &text_addr);
    char text[256];
    amx_GetString(text, text_addr, 0, sizeof(text));
    return AMXXMenuSystem::GetInstance().AddMenuText(menuId, text) ? 1 : 0;
}

// menu_addblank(menuid) - 添加空白行
cell AMX_NATIVE_CALL amxx_menu_addblank(AMX *amx, cell *params)
{
    (void)amx;
    int menuId = (int)params[1];
    return AMXXMenuSystem::GetInstance().AddMenuBlank(menuId) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_menu_makecallback(AMX *amx, cell *params)
{
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char funcname[256];
    amx_GetString(funcname, name_addr, 0, sizeof(funcname));

    int funcidx;
    if (amx_FindPublic(amx, funcname, &funcidx) != AMX_ERR_NONE)
        return 0;
    return funcidx;
}

cell AMX_NATIVE_CALL amxx_player_menu_info(AMX *amx, cell *params)
{
    int playerId = (int)params[1];
    int numParams = (int)(params[0] / sizeof(cell));

    int menuId = AMXXMenuSystem::GetInstance().GetPlayerMenu(playerId);
    int newmenu = 0;
    int menutime = 0;

    if (menuId > 0) {
        AMXXMenu *menu = AMXXMenuSystem::GetInstance().GetMenu(menuId);
        if (menu) {
            newmenu = 1;
            menutime = menu->GetDisplayTime();
        }
    }

    if (numParams >= 2) { cell *out; amx_GetAddr(amx, params[2], &out); if (out) *out = menuId; }
    if (numParams >= 3) { cell *out; amx_GetAddr(amx, params[3], &out); if (out) *out = newmenu; }
    if (numParams >= 4) { cell *out; amx_GetAddr(amx, params[4], &out); if (out) *out = menutime; }

    return menuId > 0 ? 1 : 0;
}

// P2: 菜单高级 natives

// menu_setcmd(menu, item, cmd[]) - 设置菜单项的命令/info 字符串
cell AMX_NATIVE_CALL amxx_menu_setcmd(AMX *amx, cell *params)
{
    int menuId = (int)params[1];
    int item = (int)params[2];
    cell *cmd_addr;
    amx_GetAddr(amx, params[3], &cmd_addr);
    char cmd[256];
    amx_GetString(cmd, cmd_addr, 0, sizeof(cmd));

    AMXXMenu *menu = AMXXMenuSystem::GetInstance().GetMenu(menuId);
    if (!menu) return 0;
    return menu->SetItemInfo(item, cmd) ? 1 : 0;
}

// menu_itemtext(menu, item, text[], maxlen) - 获取纯文本/空白行的显示文本
cell AMX_NATIVE_CALL amxx_menu_itemtext(AMX *amx, cell *params)
{
    int menuId = (int)params[1];
    int item = (int)params[2];
    cell *dest;
    amx_GetAddr(amx, params[3], &dest);
    int maxlen = (int)params[4];

    AMXXMenu *menu = AMXXMenuSystem::GetInstance().GetMenu(menuId);
    if (!menu) return 0;
    const char *text = menu->GetItemName(item);
    amx_SetString(dest, text, 0, 0, maxlen);
    return 1;
}

// menu_itemcmd(menu, item, cmd[], maxlen) - 获取菜单项的命令/info 字符串
cell AMX_NATIVE_CALL amxx_menu_itemcmd(AMX *amx, cell *params)
{
    int menuId = (int)params[1];
    int item = (int)params[2];
    cell *dest;
    amx_GetAddr(amx, params[3], &dest);
    int maxlen = (int)params[4];

    AMXXMenu *menu = AMXXMenuSystem::GetInstance().GetMenu(menuId);
    if (!menu) return 0;
    const char *cmd = menu->GetItemInfo(item);
    amx_SetString(dest, cmd, 0, 0, maxlen);
    return 1;
}

void RegisterMenuNatives(AMX *amx)
{
    amx_Register(amx, menu_natives, -1);
}
