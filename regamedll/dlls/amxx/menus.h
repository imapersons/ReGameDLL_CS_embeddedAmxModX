#pragma once
#ifndef AMXX_MENUS_H
#define AMXX_MENUS_H

#include "amx.h"
#include <string>
#include <vector>
#include <map>

class AMXXMenu
{
public:
    AMXXMenu(int menuId, const char *title);

    void SetTitle(const char *title) { m_title = title ? title : ""; }
    const char *GetTitle() const { return m_title.c_str(); }

    void AddItem(const char *name, const char *info, int access = 0);
    void AddText(const char *text);     // 纯文本行（不可选）
    void AddBlank();                    // 空白行（不可选）
    int ItemCount() const { return (int)m_items.size(); }

    const char *GetItemName(int index) const;
    const char *GetItemInfo(int index) const;
    bool IsItemText(int index) const;   // 是否为纯文本/空白行
    bool SetItemInfo(int index, const char *info); // 设置菜单项命令/info
    bool SetItemName(int index, const char *name); // 设置菜单项显示名
    int  GetItemAccess(int index) const;           // 获取菜单项权限
    bool SetItemAccess(int index, int access);     // 设置菜单项权限
    int  GetItemCallback(int index) const;         // 获取菜单项回调
    bool SetItemCallback(int index, int callback); // 设置菜单项回调
    int  FindItemId(int page, int key) const;      // 页/键 转为项目索引，无效返回 -1

    int GetId() const { return m_id; }
    int GetDisplayTime() const { return m_displayTime; }
    void SetDisplayTime(int time) { m_displayTime = time; }

    int GetItemsPerPage() const { return m_itemsPerPage; }
    void SetItemsPerPage(int count) { m_itemsPerPage = (count > 0) ? count : 8; }

private:
    int m_id;
    int m_displayTime;
    int m_itemsPerPage;
    std::string m_title;
    struct MenuItem {
        std::string name;
        std::string info;
        bool isText;    // true = 纯文本/空白行，不可选
        int  access;    // 权限标志位
        int  callback;  // 项目回调 funcidx，-1 = 无
        MenuItem() : isText(false), access(0), callback(-1) {}
    };
    std::vector<MenuItem> m_items;
};

class AMXXMenuSystem
{
public:
    static AMXXMenuSystem &GetInstance();

    void Init();
    void Shutdown();

    int CreateMenu(const char *title, AMX *amx = nullptr, cell funcidx = -1);
    bool DestroyMenu(int menuId);
    void DestroyAllMenus();
    AMXXMenu *GetMenu(int menuId);
    int GetPlayerMenu(int playerId) const;

    bool AddMenuItem(int menuId, const char *name, const char *info);
    bool AddMenuText(int menuId, const char *text);
    bool AddMenuBlank(int menuId);
    bool SetMenuTitle(int menuId, const char *title);

    int GetMenuPages(int menuId, int itemsPerPage = 8) const;

    bool ShowMenu(int playerId, int menuId, int page = 0, int keys = 0xFF, int time = 0);

    void OnMenuSelect(int playerId, int key);

    void RegisterCallback(int menuId, AMX *amx, cell funcidx);
    void RegisterMenuCmd(int menuId, int keys, AMX *amx, cell funcidx);
    void SetPlayerMenuCmd(int playerId, int menuId) { m_playerMenuCmd[playerId] = menuId; }
    bool CancelMenu(int playerId);
    void SetMenuDisplayTime(int menuId, int time);

private:
    AMXXMenuSystem();

    int m_nextMenuId;
    std::map<int, AMXXMenu *> m_menus;
    std::map<int, std::pair<AMX *, cell>> m_callbacks;

    struct ActiveMenu {
        int menuId;
        int page;
    };
    std::map<int, ActiveMenu> m_activeMenu;

    // register_menucmd 注册的菜单命令
    struct MenuCmd {
        int menuId;
        int keys;
        AMX *amx;
        cell funcidx;
    };
    std::vector<MenuCmd> m_menuCmds;

    // 玩家最后显示的旧式菜单 ID（用于 register_menucmd 回调）
    std::map<int, int> m_playerMenuCmd;
};

#endif
