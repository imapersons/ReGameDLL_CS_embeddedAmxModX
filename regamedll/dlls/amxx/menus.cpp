#include "precompiled.h"
#include "menus.h"
#include "amx.h"
#include "extdll.h"
#include "enginecallback.h"
#include "util.h"
#include <cstdio>
#include <cstring>

#define MENU_ITEMS_PER_PAGE 8

AMXXMenu::AMXXMenu(int menuId, const char *title) : m_id(menuId), m_displayTime(0), m_itemsPerPage(MENU_ITEMS_PER_PAGE)
{
    m_title = title ? title : "";
}

void AMXXMenu::AddItem(const char *name, const char *info, int access)
{
    MenuItem item;
    item.name = name ? name : "";
    item.info = info ? info : "";
    item.isText = false;
    item.access = access;
    item.callback = -1;
    m_items.push_back(item);
}

void AMXXMenu::AddText(const char *text)
{
    MenuItem item;
    item.name = text ? text : "";
    item.info = "";
    item.isText = true;
    m_items.push_back(item);
}

void AMXXMenu::AddBlank()
{
    MenuItem item;
    item.name = "";
    item.info = "";
    item.isText = true;
    m_items.push_back(item);
}

bool AMXXMenu::IsItemText(int index) const
{
    if (index < 0 || index >= (int)m_items.size())
        return false;
    return m_items[index].isText;
}

const char *AMXXMenu::GetItemName(int index) const
{
    if (index < 0 || index >= (int)m_items.size())
        return "";
    return m_items[index].name.c_str();
}

const char *AMXXMenu::GetItemInfo(int index) const
{
    if (index < 0 || index >= (int)m_items.size())
        return "";
    return m_items[index].info.c_str();
}

bool AMXXMenu::SetItemInfo(int index, const char *info)
{
    if (index < 0 || index >= (int)m_items.size())
        return false;
    m_items[index].info = info ? info : "";
    return true;
}

bool AMXXMenu::SetItemName(int index, const char *name)
{
    if (index < 0 || index >= (int)m_items.size())
        return false;
    m_items[index].name = name ? name : "";
    return true;
}

int AMXXMenu::GetItemAccess(int index) const
{
    if (index < 0 || index >= (int)m_items.size())
        return 0;
    return m_items[index].access;
}

bool AMXXMenu::SetItemAccess(int index, int access)
{
    if (index < 0 || index >= (int)m_items.size())
        return false;
    m_items[index].access = access;
    return true;
}

int AMXXMenu::GetItemCallback(int index) const
{
    if (index < 0 || index >= (int)m_items.size())
        return -1;
    return m_items[index].callback;
}

bool AMXXMenu::SetItemCallback(int index, int callback)
{
    if (index < 0 || index >= (int)m_items.size())
        return false;
    m_items[index].callback = callback;
    return true;
}

int AMXXMenu::FindItemId(int page, int key) const
{
    if (key <= 0)
        return -1;
    int perPage = (m_itemsPerPage > 0) ? m_itemsPerPage : 8;
    int total = (int)m_items.size();
    int item = page * perPage + (key - 1);
    if (item < 0 || item >= total)
        return -1;
    return item;
}

AMXXMenuSystem &AMXXMenuSystem::GetInstance()
{
    static AMXXMenuSystem instance;
    return instance;
}

AMXXMenuSystem::AMXXMenuSystem() : m_nextMenuId(1)
{
}

void AMXXMenuSystem::Init()
{
    AMXX_LOG_DBG("[Menu] Initializing menu system...");
    DestroyAllMenus();
    m_callbacks.clear();
    m_activeMenu.clear();
    m_nextMenuId = 1;
}

void AMXXMenuSystem::Shutdown()
{
    AMXX_LOG_DBG("[Menu] Shutting down menu system...");
    DestroyAllMenus();
    m_callbacks.clear();
    m_activeMenu.clear();
    m_menuCmds.clear();  // 清空 register_menucmd 累积的数据，防止跨地图泄漏
    m_playerMenuCmd.clear();
    m_nextMenuId = 1;
}

void AMXXMenuSystem::DestroyAllMenus()
{
    for (auto &kv : m_menus) {
        delete kv.second;
    }
    m_menus.clear();
}

int AMXXMenuSystem::CreateMenu(const char *title, AMX *amx, cell funcidx)
{
    int id = m_nextMenuId++;
    m_menus[id] = new AMXXMenu(id, title);
    if (amx && funcidx >= 0)
        m_callbacks[id] = std::make_pair(amx, funcidx);
    AMXX_LOG_DBG("[Menu] Created menu %d: %s", id, title ? title : "");
    return id;
}

bool AMXXMenuSystem::DestroyMenu(int menuId)
{
    auto it = m_menus.find(menuId);
    if (it == m_menus.end())
        return false;
    delete it->second;
    m_menus.erase(it);
    m_callbacks.erase(menuId);

    // 从活跃菜单中移除
    for (auto activeIt = m_activeMenu.begin(); activeIt != m_activeMenu.end(); ) {
        if (activeIt->second.menuId == menuId)
            activeIt = m_activeMenu.erase(activeIt);
        else
            ++activeIt;
    }

    AMXX_LOG_DBG("[Menu] Destroyed menu %d", menuId);
    return true;
}

AMXXMenu *AMXXMenuSystem::GetMenu(int menuId)
{
    auto it = m_menus.find(menuId);
    if (it == m_menus.end())
        return nullptr;
    return it->second;
}

int AMXXMenuSystem::GetPlayerMenu(int playerId) const
{
    auto it = m_activeMenu.find(playerId);
    if (it == m_activeMenu.end())
        return 0;
    return it->second.menuId;
}

bool AMXXMenuSystem::AddMenuItem(int menuId, const char *name, const char *info)
{
    AMXXMenu *menu = GetMenu(menuId);
    if (!menu)
        return false;
    menu->AddItem(name, info);
    AMXX_LOG_DBG("[Menu] Added item to menu %d: %s", menuId, name ? name : "");
    return true;
}

bool AMXXMenuSystem::AddMenuText(int menuId, const char *text)
{
    AMXXMenu *menu = GetMenu(menuId);
    if (!menu) return false;
    menu->AddText(text);
    return true;
}

bool AMXXMenuSystem::AddMenuBlank(int menuId)
{
    AMXXMenu *menu = GetMenu(menuId);
    if (!menu) return false;
    menu->AddBlank();
    return true;
}

bool AMXXMenuSystem::SetMenuTitle(int menuId, const char *title)
{
    AMXXMenu *menu = GetMenu(menuId);
    if (!menu)
        return false;
    menu->SetTitle(title);
    return true;
}

int AMXXMenuSystem::GetMenuPages(int menuId, int itemsPerPage) const
{
    auto it = m_menus.find(menuId);
    if (it == m_menus.end())
        return 0;

    int count = it->second->ItemCount();
    if (count <= 0)
        return 0;

    return (count + itemsPerPage - 1) / itemsPerPage;
}

void AMXXMenuSystem::OnMenuSelect(int playerId, int key)
{
    // 优先检查新式菜单（menu_create/menu_display）
    auto it = m_activeMenu.find(playerId);
    if (it != m_activeMenu.end()) {
        int menuId = it->second.menuId;
        int page = it->second.page;
        AMXXMenu *menu = GetMenu(menuId);
        if (menu) {
            int totalPages = GetMenuPages(menuId, menu->GetItemsPerPage());

            if (key == 0) {
                m_activeMenu.erase(it);
                return;
            }

            if (key == 8 && totalPages > 1) {
                ShowMenu(playerId, menuId, page - 1);
                return;
            }

            if (key == 9 && totalPages > 1) {
                ShowMenu(playerId, menuId, page + 1);
                return;
            }

            int itemIndex = (page * menu->GetItemsPerPage()) + (key - 1);
            if (itemIndex >= 0 && itemIndex < menu->ItemCount()) {
                auto cb = m_callbacks.find(menuId);
                if (cb != m_callbacks.end()) {
                    AMX *amx = cb->second.first;
                    cell funcidx = cb->second.second;
                    if (amx && funcidx >= 0) {
                        amx_Push(amx, itemIndex);
                        amx_Push(amx, menuId);
                        amx_Push(amx, playerId);
                        cell retval;
                        int err = amx_Exec(amx, &retval, funcidx);
                        if (err == AMX_ERR_NONE) {
                            AMXX_LOG_DBG("[Menu] Menu %d callback returned %d", menuId, retval);
                        }
                    }
                }
            }
            return;
        }
        m_activeMenu.erase(it);
    }

    // 检查旧式菜单（show_menu + register_menucmd）
    auto cmdIt = m_playerMenuCmd.find(playerId);
    if (cmdIt != m_playerMenuCmd.end()) {
        int cmdMenuId = cmdIt->second;
        for (auto &mc : m_menuCmds) {
            if (mc.menuId == cmdMenuId) {
                AMX *amx = mc.amx;
                if (amx && mc.funcidx >= 0) {
                    amx_Push(amx, key);
                    amx_Push(amx, playerId);
                    cell retval;
                    amx_Exec(amx, &retval, mc.funcidx);
                    AMXX_LOG_DBG("[Menu] register_menucmd handler called: player=%d key=%d", playerId, key);
                }
                return;
            }
        }
    }
}

bool AMXXMenuSystem::ShowMenu(int playerId, int menuId, int page, int keys, int time)
{
    AMXXMenu *menu = GetMenu(menuId);
    if (!menu)
        return false;

    if (playerId < 1 || playerId > gpGlobals->maxClients)
        return false;

    edict_t *pEdict = INDEXENT(playerId);
    if (!pEdict || pEdict->free)
        return false;

    int totalItems = menu->ItemCount();
    int perPage = menu->GetItemsPerPage();
    int totalPages = GetMenuPages(menuId, perPage);
    if (totalPages <= 0)
        return false;

    if (page < 0) page = 0;
    if (page >= totalPages) page = totalPages - 1;

    int startIdx = page * perPage;
    int endIdx = startIdx + perPage;
    if (endIdx > totalItems) endIdx = totalItems;

    char buffer[1024];
    char items[768];
    items[0] = '\0';

    const char *keyNames[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };
    for (int i = startIdx; i < endIdx; i++) {
        char line[128];
        snprintf(line, sizeof(line), "%s. %s\n", keyNames[i - startIdx], menu->GetItemName(i));
        strncat(items, line, sizeof(items) - strlen(items) - 1);
    }

    char nav[128];
    nav[0] = '\0';
    if (totalPages > 1) {
        strncat(nav, "\n8. Prev\n9. Next", sizeof(nav) - strlen(nav) - 1);
    }

    snprintf(buffer, sizeof(buffer), "%s\n\n%s%s\n0. Exit", menu->GetTitle(), items, nav);

    int menuMsgId = REG_USER_MSG("ShowMenu", -1);
    MESSAGE_BEGIN(MSG_ONE, menuMsgId, nullptr, pEdict);
        WRITE_SHORT(keys);
        WRITE_CHAR(time > 0 ? time : -1);
        WRITE_BYTE(0);
        WRITE_STRING(buffer);
    MESSAGE_END();

    ActiveMenu am;
    am.menuId = menuId;
    am.page = page;
    m_activeMenu[playerId] = am;
    menu->SetDisplayTime(time);
    AMXX_LOG_DBG("[Menu] Showed menu %d page %d/%d to player %d", menuId, page + 1, totalPages, playerId);
    return true;
}

void AMXXMenuSystem::RegisterCallback(int menuId, AMX *amx, cell funcidx)
{
    m_callbacks[menuId] = std::make_pair(amx, funcidx);
    AMXX_LOG_DBG("[Menu] Registered callback for menu %d (funcidx=%d)", menuId, funcidx);
}

bool AMXXMenuSystem::CancelMenu(int playerId)
{
    auto it = m_activeMenu.find(playerId);
    if (it == m_activeMenu.end())
        return false;
    m_activeMenu.erase(it);
    return true;
}

void AMXXMenuSystem::SetMenuDisplayTime(int menuId, int time)
{
    AMXXMenu *menu = GetMenu(menuId);
    if (menu)
        menu->SetDisplayTime(time);
}

void AMXXMenuSystem::RegisterMenuCmd(int menuId, int keys, AMX *amx, cell funcidx)
{
    MenuCmd mc;
    mc.menuId = menuId;
    mc.keys = keys;
    mc.amx = amx;
    mc.funcidx = funcidx;
    m_menuCmds.push_back(mc);
}
