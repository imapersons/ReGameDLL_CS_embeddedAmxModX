#include "precompiled.h"
#include "native_datastructs.h"

/* ReGameDLL includes */
#include "../extdll.h"
#include "../enginecallback.h"

#include "native_core.h"
#include "lang.h"
#include "amx.h"
#include "runtime.h"
#include "plugin.h"
#include "admin.h"
#include "menus.h"
#include "messages.h"
#include "events.h"
#include "../client.h"
#include "../util.h"
#include "../cdll_dll.h"
#include "../player.h"
#include "../weapons.h"
#include "../weapontype.h"

/* Standard includes */
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <cctype>
#include <cstdarg>
#include <map>
#include <set>
#include <cstddef>
#include <algorithm>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

// 引擎全局变量宏定义 
#define GET_MAXENTITIES() ((int)g_engfuncs.pfnCVarGetFloat("sv_maxentities"))

// 辅助函数
static bool is_flag_char(char c);
static bool is_length_modifier(char c);
static bool is_format_type(char c);

// append_file 实现位于文件中部（write_file 附近），此处先声明供注册表引用
cell AMX_NATIVE_CALL amxx_append_file(AMX *amx, cell *params);

// ========== 跨地图全局状态重置 ==========
// 声明在 native_core.h，实现在文件末尾
//=======================================

cell AMX_NATIVE_CALL amxx_get_user_userid(AMX *amx, cell *params)
{
    int index = params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return -1;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return -1;
    return GETPLAYERUSERID(pEdict);
}

cell AMX_NATIVE_CALL amxx_get_user_authid(AMX *amx, cell *params)
{
    int index = params[1];
    cell *dest = nullptr;
    amx_GetAddr(amx, params[2], &dest);

    if (index < 1 || index > gpGlobals->maxClients) {
        if (dest) amx_SetString(dest, "", 0, 0, params[3]);
        return 0;
    }
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free) {
        if (dest) amx_SetString(dest, "", 0, 0, params[3]);
        return 0;
    }

    const char *authid = GETPLAYERAUTHID(pEdict);
    if (!authid) authid = "STEAM_ID_PENDING";
    if (dest) amx_SetString(dest, authid, 0, 0, params[3]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_get_weaponname(AMX *amx, cell *params)
{
    int weaponId = params[1];

    cell *dest;
    amx_GetAddr(amx, params[2], &dest);

    // CS weapon name mapping for common weapon IDs
    const char *weaponName = "";
    switch (weaponId) {
        case 1: weaponName = "weapon_p228"; break;
        case 2: weaponName = "weapon_glock18"; break;
        case 3: weaponName = "weapon_scout"; break;
        case 4: weaponName = "weapon_hegrenade"; break;
        case 5: weaponName = "weapon_xm1014"; break;
        case 6: weaponName = "weapon_c4"; break;
        case 7: weaponName = "weapon_mac10"; break;
        case 8: weaponName = "weapon_aug"; break;
        case 9: weaponName = "weapon_smokegrenade"; break;
        case 10: weaponName = "weapon_elite"; break;
        case 11: weaponName = "weapon_fiveseven"; break;
        case 12: weaponName = "weapon_ump45"; break;
        case 13: weaponName = "weapon_sg550"; break;
        case 14: weaponName = "weapon_galil"; break;
        case 15: weaponName = "weapon_famas"; break;
        case 16: weaponName = "weapon_usp"; break;
        case 17: weaponName = "weapon_glock18"; break;
        case 18: weaponName = "weapon_awp"; break;
        case 19: weaponName = "weapon_mp5navy"; break;
        case 20: weaponName = "weapon_m249"; break;
        case 21: weaponName = "weapon_m3"; break;
        case 22: weaponName = "weapon_m4a1"; break;
        case 23: weaponName = "weapon_tmp"; break;
        case 24: weaponName = "weapon_g3sg1"; break;
        case 25: weaponName = "weapon_flashbang"; break;
        case 26: weaponName = "weapon_deagle"; break;
        case 27: weaponName = "weapon_sg552"; break;
        case 28: weaponName = "weapon_ak47"; break;
        case 29: weaponName = "weapon_knife"; break;
        case 30: weaponName = "weapon_p90"; break;
        default: weaponName = ""; break;
    }
    return amx_SetString(dest, weaponName, 0, 0, params[3]);
}

cell AMX_NATIVE_CALL amxx_get_weapon_id(AMX *amx, cell *params)
{
    cell *addr;
    amx_GetAddr(amx, params[1], &addr);
    char weaponName[64];
    amx_GetString(weaponName, addr, 0, sizeof(weaponName));

    if (strcmp(weaponName, "weapon_p228") == 0) return 1;
    if (strcmp(weaponName, "weapon_glock18") == 0) return 2;
    if (strcmp(weaponName, "weapon_scout") == 0) return 3;
    if (strcmp(weaponName, "weapon_hegrenade") == 0) return 4;
    if (strcmp(weaponName, "weapon_xm1014") == 0) return 5;
    if (strcmp(weaponName, "weapon_c4") == 0) return 6;
    if (strcmp(weaponName, "weapon_mac10") == 0) return 7;
    if (strcmp(weaponName, "weapon_aug") == 0) return 8;
    if (strcmp(weaponName, "weapon_smokegrenade") == 0) return 9;
    if (strcmp(weaponName, "weapon_elite") == 0) return 10;
    if (strcmp(weaponName, "weapon_fiveseven") == 0) return 11;
    if (strcmp(weaponName, "weapon_ump45") == 0) return 12;
    if (strcmp(weaponName, "weapon_sg550") == 0) return 13;
    if (strcmp(weaponName, "weapon_galil") == 0) return 14;
    if (strcmp(weaponName, "weapon_famas") == 0) return 15;
    if (strcmp(weaponName, "weapon_usp") == 0) return 16;
    if (strcmp(weaponName, "weapon_awp") == 0) return 18;
    if (strcmp(weaponName, "weapon_mp5navy") == 0) return 19;
    if (strcmp(weaponName, "weapon_m249") == 0) return 20;
    if (strcmp(weaponName, "weapon_m3") == 0) return 21;
    if (strcmp(weaponName, "weapon_m4a1") == 0) return 22;
    if (strcmp(weaponName, "weapon_tmp") == 0) return 23;
    if (strcmp(weaponName, "weapon_g3sg1") == 0) return 24;
    if (strcmp(weaponName, "weapon_flashbang") == 0) return 25;
    if (strcmp(weaponName, "weapon_deagle") == 0) return 26;
    if (strcmp(weaponName, "weapon_sg552") == 0) return 27;
    if (strcmp(weaponName, "weapon_ak47") == 0) return 28;
    if (strcmp(weaponName, "weapon_knife") == 0) return 29;
    if (strcmp(weaponName, "weapon_p90") == 0) return 30;
    return 0;
}

// CVar 变更回调存储
struct CVarChangeCallback {
    AMX *amx;
    cell funcidx;
    std::string cvarName;
};
static std::map<std::string, std::vector<CVarChangeCallback>> g_cvarCallbacks;

// hook_cvar_change 存储: 基于 pcvar 指针的 hook, 返回 1-based handle
struct CvarHook {
    cvar_t *pcvar;
    AMX *amx;
    int funcidx;
    bool disabled;
    std::string lastValue;  // 轮询用: 上次已知的 cvar 字符串值
};
static std::vector<CvarHook> g_cvarHooks;

// pcvar bounds 存储 (引擎层不支持强制执行, 仅记录)
struct CvarBoundEntry {
    bool hasUpper;
    bool hasLower;
    float upperValue;
    float lowerValue;
};
static std::map<cvar_t *, CvarBoundEntry> g_cvarBounds;

// 插件注册的 CVar 追踪 (用于 get_plugins_cvarsnum / get_plugins_cvar)
struct PluginCvarEntry {
    int pluginId;       // 1-based 插件 ID
    std::string name;
    int flags;
    std::string description;
};
static std::vector<PluginCvarEntry> g_pluginCvars;

// floatcmp(value1, value2) - 比较两个浮点数
cell AMX_NATIVE_CALL amxx_floatcmp(AMX *amx, cell *params)
{
    (void)amx;
    float a = amx_ctof(params[1]);
    float b = amx_ctof(params[2]);
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

// formatex(output[], len, format[], ...) - 格式化字符串
cell AMX_NATIVE_CALL amxx_formatex(AMX *amx, cell *params)
{
    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    int maxlen = (int)params[2];

    cell *fmt_addr;
    amx_GetAddr(amx, params[3], &fmt_addr);
    char format[4096];
    amx_GetString(format, fmt_addr, 0, sizeof(format));

    int numParams = (int)(params[0] / sizeof(cell));
    int extraParams = numParams - 3;

    if (extraParams <= 0) {
        return amx_SetString(dest, format, 0, 0, maxlen);
    }

    // 简单格式化：遍历格式字符串，逐个替换格式说明符
    char buffer[4096];
    size_t outPos = 0;
    size_t len = strlen(format);
    int varargIdx = 0; // varargs 从 params[4] 开始

    for (size_t i = 0; i < len && outPos < sizeof(buffer) - 1; i++) {
        if (format[i] == '%' && i + 1 < len) {
            size_t start = i;
            i++;
            if (format[i] == '%') {
                buffer[outPos++] = '%';
                continue;
            }

            // 跳过 flags
            while (i < len && (format[i] == '-' || format[i] == '+' || format[i] == ' ' || format[i] == '#' || format[i] == '0'))
                i++;

            // 跳过 width
            if (i < len && format[i] == '*') {
                i++;
            } else {
                while (i < len && format[i] >= '0' && format[i] <= '9')
                    i++;
            }

            // 跳过 .precision
            if (i < len && format[i] == '.') {
                i++;
                if (i < len && format[i] == '*') {
                    i++;
                } else {
                    while (i < len && format[i] >= '0' && format[i] <= '9')
                        i++;
                }
            }

            // 跳过 length modifier
            if (i < len && (format[i] == 'h' || format[i] == 'l' || format[i] == 'L' || format[i] == 'I' || format[i] == 'q'))
                i++;

            if (i >= len) {
                buffer[outPos++] = '%';
                break;
            }

            char fmtType = format[i];
            char fmtSpec[64];
            size_t specLen = (i - start + 1);
            if (specLen >= sizeof(fmtSpec)) specLen = sizeof(fmtSpec) - 1;
            strncpy(fmtSpec, format + start, specLen);
            fmtSpec[specLen] = '\0';

            if (varargIdx >= extraParams)
                break;

            cell currentParam = params[4 + varargIdx];
            varargIdx++;

            switch (fmtType) {
            case 's': {
                cell *strAddr;
                amx_GetAddr(amx, currentParam, &strAddr);
                char strBuf[2048];
                amx_GetString(strBuf, strAddr, 0, sizeof(strBuf));
                int n = snprintf(buffer + outPos, sizeof(buffer) - outPos, fmtSpec, strBuf);
                if (n > 0) outPos += (size_t)n;
                break;
            }
            case 'd':
            case 'i':
            case 'c': {
                int val = (fmtType == 'c') ? ((int)currentParam & 0xFF) : (int)currentParam;
                int n = snprintf(buffer + outPos, sizeof(buffer) - outPos, fmtSpec, val);
                if (n > 0) outPos += (size_t)n;
                break;
            }
            case 'f':
            case 'F':
            case 'e':
            case 'E':
            case 'g':
            case 'G':
            case 'a':
            case 'A': {
                int n = snprintf(buffer + outPos, sizeof(buffer) - outPos, fmtSpec, amx_ctof(currentParam));
                if (n > 0) outPos += (size_t)n;
                break;
            }
            case 'u':
            case 'o':
            case 'x':
            case 'X':
            case 'p': {
                int n = snprintf(buffer + outPos, sizeof(buffer) - outPos, fmtSpec, (unsigned int)currentParam);
                if (n > 0) outPos += (size_t)n;
                break;
            }
            default:
                if (outPos < sizeof(buffer) - 1) buffer[outPos++] = '%';
                break;
            }
        } else {
            buffer[outPos++] = format[i];
        }
    }
    buffer[outPos] = '\0';

    // 返回实际写入长度（amx_SetString 会截断到 maxlen-1）
    int written = (int)outPos;
    if (written >= maxlen)
        written = maxlen - 1;
    amx_SetString(dest, buffer, 0, 0, maxlen);
    return (cell)written;
}

// vformat(output[], len, format[], argstart) - 同 format() 但不同调用约定
// argstart 为调用者函数（Pawn）参数中第一个可变参数的下标（从 1 起算），
// 可变参数通过 amx->frm 从调用者的参数栈中读取（参考原版 string.cpp vformat）
cell AMX_NATIVE_CALL amxx_vformat(AMX *amx, cell *params)
{
    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    int maxlen = (int)params[2];

    // 通过 amx->frm 访问调用者函数的参数数组：
    // 调用者帧布局 [saved frm][retaddr][params[0]=字节数][param1][param2]...
    AMX_HEADER *hdr = (AMX_HEADER *)amx->base;
    unsigned char *data = amx->data ? amx->data : amx->base + (int)hdr->dat;
    cell *callerParams = (cell *)(data + (int)amx->frm + 2 * sizeof(cell));
    cell callerMax = callerParams[0] / sizeof(cell);

    int argstart = (int)params[4];

    // 把调用者的 vararg 参数读入本地参数数组，再复用现有格式化逻辑
    // 原版限制最多 16 个格式化参数（MAX_FORMAT_ARGS）
    enum { MAX_FORMAT_ARGS = 16 };
    cell localParams[2 + MAX_FORMAT_ARGS];
    int numVarargs = 0;
    int idx = argstart;
    while (idx <= callerMax && numVarargs < MAX_FORMAT_ARGS) {
        localParams[2 + numVarargs] = callerParams[idx];
        numVarargs++;
        idx++;
    }
    localParams[0] = (cell)((1 + numVarargs) * sizeof(cell)); // 字节数
    localParams[1] = params[3]; // 格式串（沿用调用者传入的 AMX 地址）

    char buffer[4096];
    amxx_format_string(amx, localParams, 1, buffer, sizeof(buffer));

    int written = (int)strlen(buffer);
    if (written >= maxlen)
        written = maxlen - 1;
    amx_SetString(dest, buffer, 0, 0, maxlen);
    return (cell)written;
}

// float(value) - 整数转浮点数
cell AMX_NATIVE_CALL amxx_float(AMX *amx, cell *params)
{
    (void)amx;
    int val = (int)params[1];
    REAL f = (REAL)val;
    cell ret = amx_ftoc(f);
    return ret;
}

// replace(text[], maxlen, search[], replace[]) - 字符串替换
cell AMX_NATIVE_CALL amxx_replace(AMX *amx, cell *params)
{
    cell *text_addr;
    amx_GetAddr(amx, params[1], &text_addr);
    int maxlen = (int)params[2];

    cell *search_addr;
    amx_GetAddr(amx, params[3], &search_addr);

    cell *replace_addr;
    amx_GetAddr(amx, params[4], &replace_addr);

    char text[4096], search[256], replace[256];
    amx_GetString(text, text_addr, 0, sizeof(text));
    amx_GetString(search, search_addr, 0, sizeof(search));
    amx_GetString(replace, replace_addr, 0, sizeof(replace));

    std::string str(text);
    size_t searchLen = strlen(search);
    size_t replaceLen = strlen(replace);
    size_t pos = 0;
    int count = 0;
    while ((pos = str.find(search, pos)) != std::string::npos) {
        str.replace(pos, searchLen, replace);
        pos += replaceLen;
        count++;
    }

    amx_SetString(text_addr, str.c_str(), 0, 0, maxlen);
    return count;
}

// isspace(ch) - 检查字符是否为空格
cell AMX_NATIVE_CALL amxx_isspace(AMX *amx, cell *params)
{
    (void)amx;
    int ch = (int)params[1] & 0xFF;
    return (ch >= 0 && isspace(ch)) ? 1 : 0;
}

// ucfirst(text[], maxlen) - 字符串首字母大写
cell AMX_NATIVE_CALL amxx_ucfirst(AMX *amx, cell *params)
{
    cell *text_addr;
    amx_GetAddr(amx, params[1], &text_addr);
    int maxlen = (int)params[2];

    char text[4096];
    amx_GetString(text, text_addr, 0, sizeof(text));

    if (text[0] != '\0') {
        text[0] = (char)toupper((unsigned char)text[0]);
    }

    return amx_SetString(text_addr, text, 0, 0, maxlen);
}

// abort(code, fmt[], ...) - 中止并记录错误
cell AMX_NATIVE_CALL amxx_abort(AMX *amx, cell *params)
{
    int code = (int)params[1];

    cell *fmt_addr;
    amx_GetAddr(amx, params[2], &fmt_addr);
    char format[1024];
    amx_GetString(format, fmt_addr, 0, sizeof(format));

    // 简单日志输出
    SERVER_PRINT(format);
    SERVER_PRINT("\n");

    return 0;
}

// find_player(flags[], value[], indexstart=0) - 搜索玩家
// flags 是字符串，每个字符代表一种搜索条件（原版 AMXX 格式）
//   a(1)=name 精确  b(2)=name 子串  c(4)=authid  d(8)=IP  e(16)=team
//   f(32)=dead  g(64)=alive  h(128)=bot  i(256)=非bot  j(512)=全部匹配
//   k(1024)=userid  l(2048)=不区分大小写
cell AMX_NATIVE_CALL amxx_find_player(AMX *amx, cell *params)
{
    cell *flags_addr;
    amx_GetAddr(amx, params[1], &flags_addr);
    char flags_str[32];
    amx_GetString(flags_str, flags_addr, 0, sizeof(flags_str));

    // 解析标志字符串为位掩码
    int flags = 0;
    for (int f = 0; flags_str[f]; f++) {
        char c = tolower(flags_str[f]);
        if (c >= 'a' && c <= 'l')
            flags |= (1 << (c - 'a'));
    }

    typedef int (*STRCMP)(const char*, const char*);
    STRCMP compareFunc;
    if (flags & 2048)  // l = 不区分大小写
        compareFunc = stricmp;
    else
        compareFunc = strcmp;

    int userid = 0;
    cell *value_addr;
    char value[256];
    if (flags & 31) {
        // 需要字符串值参数
        amx_GetAddr(amx, params[2], &value_addr);
        amx_GetString(value, value_addr, 0, sizeof(value));
    } else if (flags & 1024) {
        // k = userid 是整数参数
        userid = (int)params[2];
    }

    int startIdx = 0;
    int numParams = (int)(params[0] / sizeof(cell));
    if (numParams >= 3)
        startIdx = (int)params[3];

    int maxClients = gpGlobals->maxClients;
    int result = 0;

    for (int i = (startIdx > 0 ? startIdx : 1); i <= maxClients; i++) {
        edict_t *pEdict = INDEXENT(i);
        if (!pEdict || pEdict->free)
            continue;
        CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
        if (!pPlayer || !pPlayer->IsNetClient())
            continue;

        const char *name = STRING(pEdict->v.netname);
        bool isAlive = (pEdict->v.deadflag == DEAD_NO);
        bool isBot = (pEdict->v.flags & FL_FAKECLIENT) != 0;

        if (isAlive ? (flags & 64) : (flags & 32))
            continue;  // g=64 排除存活玩家, f=32 排除死亡玩家

        if (isBot ? (flags & 128) : (flags & 256))
            continue;  // h=128 排除bot, i=256 排除人类

        bool matched = true;

        if (flags & 1) {  // a = name 精确匹配
            if (!name || compareFunc(name, value) != 0)
                matched = false;
        }
        if (matched && (flags & 2)) {  // b = name 子串匹配
            if (flags & 2048) {  // l = 不区分大小写
                if (name) {
                    bool found = false;
                    for (size_t si = 0; name[si] && !found; si++) {
                        size_t vi = 0;
                        while (name[si + vi] && value[vi] &&
                               tolower((unsigned char)name[si + vi]) == tolower((unsigned char)value[vi]))
                            vi++;
                        if (value[vi] == '\0')
                            found = true;
                    }
                    if (!found) matched = false;
                } else {
                    matched = false;
                }
            } else {
                if (!name || strstr(name, value) == NULL)
                    matched = false;
            }
        }
        if (matched && (flags & 4)) {  // c = authid
            const char *authid = GETPLAYERAUTHID(pEdict);
            if (!authid || compareFunc(authid, value) != 0)
                matched = false;
        }
        if (matched && (flags & 8)) {  // d = IP 前缀匹配
            char *infobuffer = g_engfuncs.pfnGetInfoKeyBuffer(pEdict);
            const char *ip = g_engfuncs.pfnInfoKeyValue(infobuffer, "ip");
            int ilen = (int)strlen(value);
            if (!ip || strncmp(ip, value, ilen) != 0)
                matched = false;
        }
        if (matched && (flags & 16)) {  // e = team
            const char *team = STRING(pEdict->v.team);
            if (!team || compareFunc(team, value) != 0)
                matched = false;
        }
        if (matched && (flags & 1024)) {  // k = userid
            if (userid != GETPLAYERUSERID(pEdict))
                matched = false;
        }

        if (matched) {
            result = i;
            if ((flags & 512) == 0)  // j = 继续搜索全部匹配
                break;
        }
    }

    return result;
}

// is_user_connecting(index) - 检查玩家是否正在连接中
cell AMX_NATIVE_CALL amxx_is_user_connecting(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free)
        return 0;

    // 有 edict 但没有 private data 或不是网络客户端，视为连接中
    if (!pEdict->pvPrivateData)
        return 1;

    CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
    if (!pPlayer || !pPlayer->IsNetClient())
        return 1;

    // 检查是否 fully connected (通过 netname 判断)
    const char *name = STRING(pEdict->v.netname);
    if (!name || name[0] == '\0')
        return 1;

    return 0;
}

// set_cvar_float(name[], value) - 设置浮点 CVar
cell AMX_NATIVE_CALL amxx_set_cvar_float(AMX *amx, cell *params)
{
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char name[64];
    amx_GetString(name, name_addr, 0, sizeof(name));

    float value = amx_ctof(params[2]);
    g_engfuncs.pfnCVarSetFloat(name, value);

    return 1;
}

// get_cvar_float(name[]) - 获取浮点 CVar
cell AMX_NATIVE_CALL amxx_get_cvar_float(AMX *amx, cell *params)
{
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char name[64];
    amx_GetString(name, name_addr, 0, sizeof(name));

    float value = g_engfuncs.pfnCVarGetFloat(name);
    return amx_ftoc(value);
}

// get_pcvar_float(cvar_ptr) - 通过指针获取 CVar 浮点值
cell AMX_NATIVE_CALL amxx_get_pcvar_float(AMX *amx, cell *params)
{
    (void)amx;
    cvar_t *cvar = (cvar_t *)(intptr_t)params[1];
    if (!cvar) {
        float zero = 0.0f;
        return amx_ftoc(zero);
    }
    return amx_ftoc(cvar->value);
}

// set_pcvar_float(cvar_ptr, value) - 通过指针设置 CVar 浮点值
cell AMX_NATIVE_CALL amxx_set_pcvar_float(AMX *amx, cell *params)
{
    (void)amx;
    cvar_t *cvar = (cvar_t *)(intptr_t)params[1];
    if (!cvar) return 0;
    // P1-11 修复: 直接写 cvar->value, 不经过引擎字符串路径 (避免帧延迟)
    cvar->value = amx_ctof(params[2]);
    return 1;
}

// get_cvar_bool(name[]) - 获取布尔 CVar（非零为 true）
cell AMX_NATIVE_CALL amxx_get_cvar_bool(AMX *amx, cell *params)
{
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char name[64];
    amx_GetString(name, name_addr, 0, sizeof(name));
    float value = g_engfuncs.pfnCVarGetFloat(name);
    return (value != 0.0f) ? 1 : 0;
}

// get_pcvar_bool(cvar_ptr) - 通过指针获取布尔 CVar
cell AMX_NATIVE_CALL amxx_get_pcvar_bool(AMX *amx, cell *params)
{
    (void)amx;
    cvar_t *cvar = (cvar_t *)(intptr_t)params[1];
    if (!cvar) return 0;
    return (cvar->value != 0.0f) ? 1 : 0;
}

// ===== P2: bind_pcvar per-frame 同步 =====
struct CvarBinding {
    cvar_t *cvar;
    AMX *amx;
    cell amx_addr;  // Pawn 变量的 AMX 相对地址 (params 中的值)
    int type;       // 0=int, 1=float, 2=string
    int maxlen;     // string 模式的最大长度
};
static std::vector<CvarBinding> g_cvarBindings;

void SyncCvarBindings()
{
    for (auto &b : g_cvarBindings) {
        if (!b.cvar || !b.amx) continue;
        cell *addr;
        if (amx_GetAddr(b.amx, b.amx_addr, &addr) != AMX_ERR_NONE || !addr) continue;
        switch (b.type) {
            case 0: *addr = (cell)b.cvar->value; break;
            case 1: *addr = amx_ftoc(b.cvar->value); break;
            case 2: amx_SetString(addr, b.cvar->string ? b.cvar->string : "", 0, 0, b.maxlen); break;
        }
    }
}

void ResetCvarBindings()
{
    g_cvarBindings.clear();
}

// hook_cvar_change 轮询: 在每帧检测 cvar 值变更并触发回调
// 回调签名: public cvar_change_callback(pcvar, const old_value[], const new_value[])
void PollCvarHooks()
{
    for (auto &hook : g_cvarHooks) {
        if (hook.disabled || !hook.pcvar || !hook.amx)
            continue;

        // 验证插件仍然加载
        AMXXPlugin *plugin = AMXXRuntime::GetInstance().FindPluginByAMX(hook.amx);
        if (!plugin || !plugin->IsLoaded())
            continue;

        const char *currentStr = hook.pcvar->string ? hook.pcvar->string : "";
        if (hook.lastValue == currentStr)
            continue;

        std::string oldValue = hook.lastValue;
        std::string newValue = currentStr;

        // 逆序 push: new_value, old_value, pcvar (最后 push = 第一个参数)
        cell amx_addr_new = 0, amx_addr_old = 0;
        cell *phys_new = nullptr, *phys_old = nullptr;
        amx_PushString(hook.amx, &amx_addr_new, &phys_new, newValue.c_str(), 0, 0);
        amx_PushString(hook.amx, &amx_addr_old, &phys_old, oldValue.c_str(), 0, 0);
        amx_Push(hook.amx, (cell)(intptr_t)hook.pcvar);

        cell retval;
        amx_Exec(hook.amx, &retval, hook.funcidx);

        if (amx_addr_old) amx_Release(hook.amx, amx_addr_old);
        if (amx_addr_new) amx_Release(hook.amx, amx_addr_new);

        hook.lastValue = newValue;
    }
}

// bind_pcvar_num(cvar_ptr, &variable) - 将 pcvar 绑定到 cell 变量（每帧自动同步）
cell AMX_NATIVE_CALL amxx_bind_pcvar_num(AMX *amx, cell *params)
{
    cvar_t *cvar = (cvar_t *)(intptr_t)params[1];
    if (!cvar) return 0;
    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    if (!addr) return 0;
    *addr = (cell)cvar->value;
    g_cvarBindings.push_back({cvar, amx, params[2], 0, 0});
    return 1;
}

// bind_pcvar_float(cvar_ptr, &Float:variable)
cell AMX_NATIVE_CALL amxx_bind_pcvar_float(AMX *amx, cell *params)
{
    cvar_t *cvar = (cvar_t *)(intptr_t)params[1];
    if (!cvar) return 0;
    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    if (!addr) return 0;
    *addr = amx_ftoc(cvar->value);
    g_cvarBindings.push_back({cvar, amx, params[2], 1, 0});
    return 1;
}

// bind_pcvar_string(cvar_ptr, output[], maxlen) — 每帧自动把 cvar->string 同步到 buffer
cell AMX_NATIVE_CALL amxx_bind_pcvar_string(AMX *amx, cell *params)
{
    cvar_t *cvar = (cvar_t *)(intptr_t)params[1];
    if (!cvar) return 0;
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    int maxlen = (int)params[3];
    if (!dest || maxlen <= 0) return 0;
    amx_SetString(dest, cvar->string ? cvar->string : "", 0, 0, maxlen);
    g_cvarBindings.push_back({cvar, amx, params[2], 2, maxlen});
    return 1;
}

// is_map_valid(map[]) - 检查地图是否有效
cell AMX_NATIVE_CALL amxx_is_map_valid(AMX *amx, cell *params)
{
    cell *map_addr;
    amx_GetAddr(amx, params[1], &map_addr);
    char mapname[64];
    amx_GetString(mapname, map_addr, 0, sizeof(mapname));

    return IS_MAP_VALID(mapname) ? 1 : 0;
}

// get_modname(output[], len) - 获取当前模组名称
cell AMX_NATIVE_CALL amxx_get_modname(AMX *amx, cell *params)
{
    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    int maxlen = (int)params[2];

    char gamedir[256];
    GET_GAME_DIR(gamedir);
    
    // 提取最后的目录名
    const char *modname = gamedir;
    char *lastSlash = strrchr(gamedir, '\\');
    if (!lastSlash) lastSlash = strrchr(gamedir, '/');
    if (lastSlash) modname = lastSlash + 1;
    
    if (!modname || modname[0] == '\0') modname = "cstrike";

    return amx_SetString(dest, modname, 0, 0, maxlen);
}

// fgets(fp, output[], maxlen) - 从文件读取一行
cell AMX_NATIVE_CALL amxx_fgets(AMX *amx, cell *params)
{
    FILE *fp = (FILE *)(intptr_t)params[1];
    if (!fp)
        return 0;

    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    int maxlen = (int)params[3];

    char buffer[4096];
    if (!fgets(buffer, (int)sizeof(buffer), fp))
        return 0;

    // 去掉末尾换行符
    size_t len = strlen(buffer);
    if (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r'))
        buffer[--len] = '\0';
    if (len > 0 && buffer[len - 1] == '\r')
        buffer[--len] = '\0';

    amx_SetString(dest, buffer, 0, 0, maxlen);
    return (cell)len;
}

// get_timeleft() - 获取地图剩余时间（参考原版 AMXX 实现）
cell AMX_NATIVE_CALL amxx_get_timeleft(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    float timelimit = g_engfuncs.pfnCVarGetFloat("mp_timelimit");
    if (timelimit <= 0.0f) {
        return 0;
    }

    // 原版 AMXX 返回整数剩余秒数
    int timeleft = (int)(timelimit * 60.0f - gpGlobals->time);
    if (timeleft < 0)
        timeleft = 0;

    return timeleft;
}

// get_pluginsnum() - 获取已加载插件数量
cell AMX_NATIVE_CALL amxx_get_pluginsnum(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    AMXXRuntime &runtime = AMXXRuntime::GetInstance();
    return (cell)runtime.GetPlugins().size();
}

// get_plugin(index, output[], len) - 获取插件名称
cell AMX_NATIVE_CALL amxx_get_plugin(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    int maxlen = (int)params[3];

    AMXXRuntime &runtime = AMXXRuntime::GetInstance();
    const auto &plugins = runtime.GetPlugins();

    if (index < 0 || index >= (int)plugins.size())
        return amx_SetString(dest, "", 0, 0, maxlen);

    const char *name = plugins[index]->GetName();
    if (!name) name = "";
    return amx_SetString(dest, name, 0, 0, maxlen);
}

// 内置静态模块列表 (静态编译版无独立模块概念，但需兼容 require_module/get_modulesnum 等)
struct StaticModule {
    const char *name;
    const char *author;
    const char *description;
    const char *version;
};
static const StaticModule s_builtinModules[] = {
    {"amxmodx",     "AMXX Team",       "AMX Mod X Core",       "1.8.2"},
    {"cstrike",     "AMXX Team",       "CS Strike Module",     "1.8.2"},
    {"fun",         "AMXX Team",       "Fun Module",           "1.8.2"},
    {"engine",      "AMXX Team",       "Engine Module",        "1.8.2"},
    {"fakemeta",    "AMXX Team",       "FakeMeta Module",      "1.8.2"},
    {"hamsandwich", "AMXX Team",       "Ham Sandwich Module",  "1.8.2"},
    {"nvault",      "AMXX Team",       "nVault Module",        "1.8.2"},
};
static const int s_numBuiltinModules = (int)(sizeof(s_builtinModules) / sizeof(s_builtinModules[0]));

// get_modulesnum() - 获取模块数量
cell AMX_NATIVE_CALL amxx_get_modulesnum(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return s_numBuiltinModules;
}

// get_module(index, name[], len, author[], len2, description[], len3, version[], len4)
// 返回: 1=运行中, 0=已停止, -1=无效索引
cell AMX_NATIVE_CALL amxx_get_module(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    if (index < 0 || index >= s_numBuiltinModules)
        return -1;

    const StaticModule &mod = s_builtinModules[index];
    cell *dest;

    amx_GetAddr(amx, params[2], &dest);
    amx_SetString(dest, mod.name, 0, 0, (int)params[3]);

    int num_params = params[0] / sizeof(cell);
    if (num_params >= 5) {
        amx_GetAddr(amx, params[4], &dest);
        amx_SetString(dest, mod.author, 0, 0, (int)params[5]);
    }
    if (num_params >= 7) {
        amx_GetAddr(amx, params[6], &dest);
        amx_SetString(dest, mod.description, 0, 0, (int)params[7]);
    }
    if (num_params >= 9) {
        amx_GetAddr(amx, params[8], &dest);
        amx_SetString(dest, mod.version, 0, 0, (int)params[9]);
    }
    return 1;  // 模块运行中
}

// find_plugin_byfile(filename[], ignoreCase=1) - 按文件名搜索插件
cell AMX_NATIVE_CALL amxx_find_plugin_byfile(AMX *amx, cell *params)
{
    (void)amx;
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char filename[256];
    amx_GetString(filename, name_addr, 0, sizeof(filename));

    int ignoreCase = 1;
    int numParams = (int)(params[0] / sizeof(cell));
    if (numParams >= 2)
        ignoreCase = (int)params[2];

    AMXXRuntime &runtime = AMXXRuntime::GetInstance();
    const auto &plugins = runtime.GetPlugins();

    for (size_t i = 0; i < plugins.size(); i++) {
        const char *fname = plugins[i]->GetFilename();
        if (!fname) continue;

        if (ignoreCase) {
            if (_stricmp(fname, filename) == 0)
                return (cell)i + 1; // 1-based index
        } else {
            if (strcmp(fname, filename) == 0)
                return (cell)i + 1;
        }
    }
    return 0;
}

// get_plugins_cvarsnum() - 获取所有插件注册的 CVar 总数
cell AMX_NATIVE_CALL amxx_get_plugins_cvarsnum(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return (cell)g_pluginCvars.size();
}

// get_plugins_cvar(num, name[], namelen, &flags=0, &plid=0, &bbind=0)
// 按索引获取插件 CVar 信息, 返回 1 成功, 0 失败
cell AMX_NATIVE_CALL amxx_get_plugins_cvar(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    if (index < 0 || index >= (int)g_pluginCvars.size())
        return 0;

    const PluginCvarEntry &entry = g_pluginCvars[index];

    // 写入 cvar 名称
    cell *nameAddr;
    amx_GetAddr(amx, params[2], &nameAddr);
    amx_SetString(nameAddr, entry.name.c_str(), 0, 0, (int)params[3]);

    int num_params = params[0] / sizeof(cell);
    // 写入 flags
    if (num_params >= 4) {
        cell *flagsAddr;
        amx_GetAddr(amx, params[4], &flagsAddr);
        if (flagsAddr) *flagsAddr = (cell)entry.flags;
    }
    // 写入 plugin id
    if (num_params >= 5) {
        cell *plidAddr;
        amx_GetAddr(amx, params[5], &plidAddr);
        if (plidAddr) *plidAddr = (cell)entry.pluginId;
    }
    // 写入 bind plugin id (同 plid)
    if (num_params >= 6) {
        cell *bbindAddr;
        amx_GetAddr(amx, params[6], &bbindAddr);
        if (bbindAddr) *bbindAddr = (cell)entry.pluginId;
    }
    return 1;
}

// is_plugin_loaded(name[]) - 检查插件是否已加载
// 原版返回 0 基插件索引；未加载返回 -1。
// 默认按插件注册名(title, 如 "Menus Front-End")匹配(不区分大小写)；
// 第二个参数 useFilename 非 0 时按文件名匹配(区分大小写)。
cell AMX_NATIVE_CALL amxx_is_plugin_loaded(AMX *amx, cell *params)
{
    (void)amx;
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char name[256];
    amx_GetString(name, name_addr, 0, sizeof(name));

    int numParams = (int)(params[0] / sizeof(cell));
    bool useFilename = (numParams >= 2 && params[2] != 0);

    AMXXRuntime &runtime = AMXXRuntime::GetInstance();
    const auto &plugins = runtime.GetPlugins();

    for (size_t i = 0; i < plugins.size(); i++) {
        if (useFilename) {
            const char *pname = plugins[i]->GetName();
            if (pname && strcmp(pname, name) == 0)
                return (cell)i;
        } else {
            const char *ptitle = plugins[i]->GetTitle();
            if (ptitle && ptitle[0] && _stricmp(ptitle, name) == 0)
                return (cell)i;
        }
    }
    return -1;
}

// is_module_loaded(name[]) - 检查模块是否加载（同 module_exists）
cell AMX_NATIVE_CALL amxx_is_module_loaded(AMX *amx, cell *params)
{
    (void)amx;
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char name[256];
    amx_GetString(name, name_addr, 0, sizeof(name));

    // 内置模块始终存在
    if (strcmp(name, "amxmodx") == 0 || strcmp(name, "AMXModX") == 0) return 1;
    if (strcmp(name, "cstrike") == 0 || strcmp(name, "CStrike") == 0) return 1;
    if (strcmp(name, "fun") == 0 || strcmp(name, "Fun") == 0) return 1;
    if (strcmp(name, "engine") == 0 || strcmp(name, "Engine") == 0) return 1;
    if (strcmp(name, "fakemeta") == 0 || strcmp(name, "FakeMeta") == 0) return 1;
    if (strcmp(name, "hamsandwich") == 0 || strcmp(name, "HamSandwich") == 0) return 1;
    if (strcmp(name, "nvault") == 0 || strcmp(name, "NVault") == 0) return 1;
    if (strcmp(name, "regex") == 0 || strcmp(name, "Regex") == 0) return 1;

    return 0;
}

// LibraryExists(library[], type) - 检查库是否存在
// type: 0=module, 1=metamod, 2=game
cell AMX_NATIVE_CALL amxx_LibraryExists(AMX *amx, cell *params)
{
    (void)amx;
    int type = (int)params[2];

    // 只处理 type=0 (module)，其他类型返回 0
    if (type != 0)
        return 0;

    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char name[256];
    amx_GetString(name, name_addr, 0, sizeof(name));

    // 复用 is_module_loaded 的逻辑
    if (strcmp(name, "amxmodx") == 0 || strcmp(name, "AMXModX") == 0) return 1;
    if (strcmp(name, "cstrike") == 0 || strcmp(name, "CStrike") == 0) return 1;
    if (strcmp(name, "fun") == 0 || strcmp(name, "Fun") == 0) return 1;
    if (strcmp(name, "engine") == 0 || strcmp(name, "Engine") == 0) return 1;
    if (strcmp(name, "fakemeta") == 0 || strcmp(name, "FakeMeta") == 0) return 1;
    if (strcmp(name, "hamsandwich") == 0 || strcmp(name, "HamSandwich") == 0) return 1;
    if (strcmp(name, "nvault") == 0 || strcmp(name, "NVault") == 0) return 1;
    if (strcmp(name, "regex") == 0 || strcmp(name, "Regex") == 0) return 1;

    return 0;
}

// =================================================================

// ===== P2-1: microsec() native =====
// microsec() - returns current game time in microseconds (truncated to cell)
cell AMX_NATIVE_CALL amxx_microsec(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return (cell)(gpGlobals->time * 1000000.0f);
}

// ===== P2-2: md5() native =====
// md5(const szString[], md5buffer[16]) - compute MD5 hash, fill 16-byte buffer
// Minimal self-contained MD5 implementation (public domain reference).
namespace {
    struct amxx_md5_ctx {
        uint32_t a, b, c, d;
        uint32_t bits[2];
        unsigned char buf[64];
    };

    static const uint32_t amxx_md5_k[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
        0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
        0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
        0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
        0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
        0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
        0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
        0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
    };

    static const uint32_t amxx_md5_s[64] = {
        7,12,17,22, 7,12,17,22, 7,12,17,22, 7,12,17,22,
        5, 9,14,20, 5, 9,14,20, 5, 9,14,20, 5, 9,14,20,
        4,11,16,23, 4,11,16,23, 4,11,16,23, 4,11,16,23,
        6,10,15,21, 6,10,15,21, 6,10,15,21, 6,10,15,21
    };

    static inline uint32_t amxx_md5_rotl(uint32_t x, uint32_t n) { return (x << n) | (x >> (32 - n)); }

    static void amxx_md5_transform(amxx_md5_ctx *ctx, const unsigned char block[64])
    {
        uint32_t m[16];
        for (int i = 0; i < 16; i++)
            m[i] = (uint32_t)block[i*4] | ((uint32_t)block[i*4+1] << 8) |
                   ((uint32_t)block[i*4+2] << 16) | ((uint32_t)block[i*4+3] << 24);

        uint32_t a = ctx->a, b = ctx->b, c = ctx->c, d = ctx->d;

        for (int i = 0; i < 64; i++) {
            uint32_t f, g;
            if (i < 16)      { f = (b & c) | (~b & d);      g = i; }
            else if (i < 32) { f = (d & b) | (~d & c);      g = (5*i + 1) % 16; }
            else if (i < 48) { f = b ^ c ^ d;               g = (3*i + 5) % 16; }
            else             { f = c ^ (b | ~d);            g = (7*i) % 16; }

            uint32_t temp = d;
            d = c;
            c = b;
            b = b + amxx_md5_rotl(a + f + m[g] + amxx_md5_k[i], amxx_md5_s[i]);
            a = temp;
        }

        ctx->a += a; ctx->b += b; ctx->c += c; ctx->d += d;
    }

    static void amxx_md5_init(amxx_md5_ctx *ctx)
    {
        ctx->a = 0x67452301; ctx->b = 0xefcdab89;
        ctx->c = 0x98badcfe; ctx->d = 0x10325476;
        ctx->bits[0] = 0; ctx->bits[1] = 0;
    }

    static void amxx_md5_update(amxx_md5_ctx *ctx, const unsigned char *data, size_t len)
    {
        size_t t = (size_t)(ctx->bits[0] / 8) % 64;
        ctx->bits[0] += (uint32_t)(len * 8);
        if (ctx->bits[0] < (uint32_t)(len * 8)) ctx->bits[1]++;
        ctx->bits[1] += (uint32_t)(len >> 29);

        size_t pos = 0;
        if (t > 0) {
            size_t copy = 64 - t;
            if (copy > len) copy = len;
            memcpy(ctx->buf + t, data, copy);
            if (t + copy == 64) {
                amxx_md5_transform(ctx, ctx->buf);
                pos += copy;
            } else {
                return;
            }
        }

        while (pos + 64 <= len) {
            amxx_md5_transform(ctx, data + pos);
            pos += 64;
        }

        if (pos < len)
            memcpy(ctx->buf, data + pos, len - pos);
    }

    static void amxx_md5_final(unsigned char digest[16], amxx_md5_ctx *ctx)
    {
        size_t t = (size_t)(ctx->bits[0] / 8) % 64;
        unsigned char pad[64];
        memset(pad, 0, sizeof(pad));
        pad[0] = 0x80;
        if (t < 56)
            amxx_md5_update(ctx, pad, 56 - t);
        else
            amxx_md5_update(ctx, pad, 64 + 56 - t);

        unsigned char lenbytes[8];
        lenbytes[0] = (unsigned char)(ctx->bits[0] & 0xff);
        lenbytes[1] = (unsigned char)((ctx->bits[0] >> 8) & 0xff);
        lenbytes[2] = (unsigned char)((ctx->bits[0] >> 16) & 0xff);
        lenbytes[3] = (unsigned char)((ctx->bits[0] >> 24) & 0xff);
        lenbytes[4] = (unsigned char)(ctx->bits[1] & 0xff);
        lenbytes[5] = (unsigned char)((ctx->bits[1] >> 8) & 0xff);
        lenbytes[6] = (unsigned char)((ctx->bits[1] >> 16) & 0xff);
        lenbytes[7] = (unsigned char)((ctx->bits[1] >> 24) & 0xff);
        amxx_md5_update(ctx, lenbytes, 8);

        digest[0]  = (unsigned char)(ctx->a & 0xff);
        digest[1]  = (unsigned char)((ctx->a >> 8) & 0xff);
        digest[2]  = (unsigned char)((ctx->a >> 16) & 0xff);
        digest[3]  = (unsigned char)((ctx->a >> 24) & 0xff);
        digest[4]  = (unsigned char)(ctx->b & 0xff);
        digest[5]  = (unsigned char)((ctx->b >> 8) & 0xff);
        digest[6]  = (unsigned char)((ctx->b >> 16) & 0xff);
        digest[7]  = (unsigned char)((ctx->b >> 24) & 0xff);
        digest[8]  = (unsigned char)(ctx->c & 0xff);
        digest[9]  = (unsigned char)((ctx->c >> 8) & 0xff);
        digest[10] = (unsigned char)((ctx->c >> 16) & 0xff);
        digest[11] = (unsigned char)((ctx->c >> 24) & 0xff);
        digest[12] = (unsigned char)(ctx->d & 0xff);
        digest[13] = (unsigned char)((ctx->d >> 8) & 0xff);
        digest[14] = (unsigned char)((ctx->d >> 16) & 0xff);
        digest[15] = (unsigned char)((ctx->d >> 24) & 0xff);
    }
}

cell AMX_NATIVE_CALL amxx_md5(AMX *amx, cell *params)
{
    cell *str_addr;
    amx_GetAddr(amx, params[1], &str_addr);
    char str[4096];
    amx_GetString(str, str_addr, 0, sizeof(str));

    amxx_md5_ctx ctx;
    amxx_md5_init(&ctx);
    amxx_md5_update(&ctx, (const unsigned char *)str, strlen(str));

    unsigned char digest[16];
    amxx_md5_final(digest, &ctx);

    cell *out;
    amx_GetAddr(amx, params[2], &out);
    if (out) {
        for (int i = 0; i < 16; i++)
            out[i] = (cell)digest[i];
    }
    return 1;
}

// ===== P2-8: 补全核心 natives =====

// read_datanum() - 返回当前客户端消息参数数量 (register_message/register_event 回调中使用)
cell AMX_NATIVE_CALL amxx_read_datanum(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    AMXXMessageSystem &msgs = AMXXMessageSystem::GetInstance();
    if (msgs.GetArgCount() > 0)
        return msgs.GetArgCount();
    return AMXXEventSystem::GetInstance().GetEventArgsCount();
}

// read_datatype() - 返回当前客户端消息的消息 ID
cell AMX_NATIVE_CALL amxx_read_datatype(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    AMXXMessageSystem &msgs = AMXXMessageSystem::GetInstance();
    if (msgs.GetArgCount() > 0)
        return msgs.GetCurrentMsgId();
    return AMXXEventSystem::GetInstance().GetCurrentMsgType();
}

// find_player_ex(FindPlayerFlags:flags, ...) - 使用整数标志查找玩家
// 与 find_player 的区别: flags 直接为位掩码而非字符串
// 位定义: a=1(name) b=2(substring) c=4(authid) d=8(ip) e=16(team)
//         f=32(exclude dead) g=64(exclude alive) h=128(exclude bots) i=256(exclude human)
//         j=512(last matched) k=1024(userid) l=2048(case insensitive) m=4096(include connecting)
cell AMX_NATIVE_CALL amxx_find_player_ex(AMX *amx, cell *params)
{
    int flags = (int)params[1];

    typedef int (*STRCMP)(const char*, const char*);
    STRCMP compareFunc;
    if (flags & 2048)  // l = 不区分大小写
        compareFunc = stricmp;
    else
        compareFunc = strcmp;

    int userid = 0;
    cell *value_addr;
    char value[256];
    if (flags & 31) {
        // 需要字符串值参数
        amx_GetAddr(amx, params[2], &value_addr);
        amx_GetString(value, value_addr, 0, sizeof(value));
    } else if (flags & 1024) {
        // k = userid 是整数参数
        userid = (int)params[2];
    }

    int maxClients = gpGlobals->maxClients;
    int result = 0;

    for (int i = 1; i <= maxClients; i++) {
        edict_t *pEdict = INDEXENT(i);
        if (!pEdict || pEdict->free)
            continue;
        CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
        if (!pPlayer || !pPlayer->IsNetClient())
            continue;

        const char *name = STRING(pEdict->v.netname);
        bool isAlive = (pEdict->v.deadflag == DEAD_NO);
        bool isBot = (pEdict->v.flags & FL_FAKECLIENT) != 0;

        if (isAlive ? (flags & 64) : (flags & 32))
            continue;  // g=64 排除存活玩家, f=32 排除死亡玩家

        if (isBot ? (flags & 128) : (flags & 256))
            continue;  // h=128 排除bot, i=256 排除人类

        bool matched = true;

        if (flags & 1) {  // a = name 精确匹配
            if (!name || compareFunc(name, value) != 0)
                matched = false;
        }
        if (matched && (flags & 2)) {  // b = name 子串匹配
            if (flags & 2048) {
                if (name) {
                    bool found = false;
                    for (size_t si = 0; name[si] && !found; si++) {
                        size_t vi = 0;
                        while (name[si + vi] && value[vi] &&
                               tolower((unsigned char)name[si + vi]) == tolower((unsigned char)value[vi]))
                            vi++;
                        if (value[vi] == '\0')
                            found = true;
                    }
                    if (!found) matched = false;
                } else {
                    matched = false;
                }
            } else {
                if (!name || strstr(name, value) == NULL)
                    matched = false;
            }
        }
        if (matched && (flags & 4)) {  // c = authid
            const char *authid = GETPLAYERAUTHID(pEdict);
            if (!authid || compareFunc(authid, value) != 0)
                matched = false;
        }
        if (matched && (flags & 8)) {  // d = IP 前缀匹配
            char *infobuffer = g_engfuncs.pfnGetInfoKeyBuffer(pEdict);
            const char *ip = g_engfuncs.pfnInfoKeyValue(infobuffer, "ip");
            int ilen = (int)strlen(value);
            if (!ip || strncmp(ip, value, ilen) != 0)
                matched = false;
        }
        if (matched && (flags & 16)) {  // e = team
            const char *team = STRING(pEdict->v.team);
            if (!team || compareFunc(team, value) != 0)
                matched = false;
        }
        if (matched && (flags & 1024)) {  // k = userid
            if (userid != GETPLAYERUSERID(pEdict))
                matched = false;
        }

        if (matched) {
            result = i;
            if ((flags & 512) == 0)  // j = 继续搜索全部匹配
                break;
        }
    }

    return result;
}

// get_weaponid(const name[]) - 武器名 -> 武器 ID (标准 AMXX native, 等同 get_weapon_id)
cell AMX_NATIVE_CALL amxx_get_weaponid(AMX *amx, cell *params)
{
    return amxx_get_weapon_id(amx, params);
}

// has_map_ent_class(const classname[]) - 检查地图中是否含有指定类名的实体
cell AMX_NATIVE_CALL amxx_has_map_ent_class(AMX *amx, cell *params)
{
    cell *addr;
    amx_GetAddr(amx, params[1], &addr);
    char classname[64];
    amx_GetString(classname, addr, 0, sizeof(classname));

    edict_t *pent = FIND_ENTITY_BY_CLASSNAME(NULL, classname);
    return FNullEnt(pent) ? 0 : 1;
}

// engclient_print(player, type, const message[], any:...) - 引擎层打印消息
// type: 0=console, 1=center, 2=chat (engprint_* 常量)
cell AMX_NATIVE_CALL amxx_engclient_print(AMX *amx, cell *params)
{
    int id = (int)params[1];
    int type = (int)params[2];

    char message[512];
    amxx_format_string(amx, params, 3, message, sizeof(message));
    int len = (int)strlen(message);

    // center 消息要求 \n 转 \r
    if (type == 1) {
        for (int j = 0; j < len; j++)
            if (message[j] == '\n')
                message[j] = '\r';
    }

    PRINT_TYPE printType = (PRINT_TYPE)type;

    if (id == 0) {
        // 发送给所有客户端
        for (int i = 1; i <= gpGlobals->maxClients; i++) {
            edict_t *pEdict = INDEXENT(i);
            if (pEdict && !pEdict->free)
                g_engfuncs.pfnClientPrintf(pEdict, printType, message);
        }
    } else {
        if (id < 1 || id > gpGlobals->maxClients)
            return 0;
        edict_t *pEdict = INDEXENT(id);
        if (!pEdict || pEdict->free)
            return 0;
        g_engfuncs.pfnClientPrintf(pEdict, printType, message);
    }

    return len;
}

// precache_event(type, const Name[], any:...) - 预缓存事件文件
cell AMX_NATIVE_CALL amxx_precache_event(AMX *amx, cell *params)
{
    (void)amx;
    int type = (int)params[1];
    cell *name_addr;
    amx_GetAddr(amx, params[2], &name_addr);
    char name[256];
    amx_GetString(name, name_addr, 0, sizeof(name));

    // 简化: 忽略可变格式参数, 直接使用事件名 (事件名通常无格式说明符)
    return g_engfuncs.pfnPrecacheEvent(type, name);
}

// is_jit_enabled() - 检查 JIT 是否启用 (嵌入式实现不使用 JIT, 返回 0)
cell AMX_NATIVE_CALL amxx_is_jit_enabled(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return 0;
}

// md5_file(const file[], md5buffer[34]) - 计算文件 MD5
cell AMX_NATIVE_CALL amxx_md5_file(AMX *amx, cell *params)
{
    cell *file_addr;
    amx_GetAddr(amx, params[1], &file_addr);
    char filepath[256];
    amx_GetString(filepath, file_addr, 0, sizeof(filepath));

    FILE *fp = fopen(filepath, "rb");
    if (!fp)
        return 0;

    amxx_md5_ctx ctx;
    amxx_md5_init(&ctx);

    unsigned char buf[4096];
    size_t bytesRead;
    while ((bytesRead = fread(buf, 1, sizeof(buf), fp)) > 0)
        amxx_md5_update(&ctx, buf, bytesRead);

    fclose(fp);

    unsigned char digest[16];
    amxx_md5_final(digest, &ctx);

    cell *out;
    amx_GetAddr(amx, params[2], &out);
    if (out) {
        for (int i = 0; i < 16; i++)
            out[i] = (cell)digest[i];
    }
    return 1;
}

// RequestFrame(const callback[], any:data = 0) - 下一帧调用回调
cell AMX_NATIVE_CALL amxx_request_frame(AMX *amx, cell *params)
{
    cell *func_addr;
    amx_GetAddr(amx, params[1], &func_addr);
    char funcname[256];
    amx_GetString(funcname, func_addr, 0, sizeof(funcname));

    int funcidx;
    if (amx_FindPublic(amx, funcname, &funcidx) != AMX_ERR_NONE)
        return 0;

    cell data = 0;
    int numParams = (int)(params[0] / sizeof(cell));
    if (numParams >= 2)
        data = params[2];

    AMXXRuntime::GetInstance().RequestFrame(amx, funcidx, data);
    return 1;
}

// AutoExecConfig(bool:autoCreate = true, const name[] = "", const folder[] = "") - 自动执行配置文件
cell AMX_NATIVE_CALL amxx_autoexec_config(AMX *amx, cell *params)
{
    (void)amx;
    int numParams = (int)(params[0] / sizeof(cell));
    bool autoCreate = (numParams >= 1) ? (params[1] != 0) : true;

    char name[128] = "";
    char folder[128] = "";

    if (numParams >= 2) {
        cell *name_addr;
        amx_GetAddr(amx, params[2], &name_addr);
        amx_GetString(name, name_addr, 0, sizeof(name));
    }
    if (numParams >= 3) {
        cell *folder_addr;
        amx_GetAddr(amx, params[3], &folder_addr);
        amx_GetString(folder, folder_addr, 0, sizeof(folder));
    }

    // 构建配置文件路径: addons/amxmodx/configs/<folder>/<name>.cfg
    char cmd[512];
    if (name[0]) {
        if (folder[0])
            snprintf(cmd, sizeof(cmd), "exec addons/amxmodx/configs/%s/%s.cfg", folder, name);
        else
            snprintf(cmd, sizeof(cmd), "exec addons/amxmodx/configs/%s.cfg", name);
    } else {
        // 无 name 参数时, 使用调用插件文件名 (简化: 直接返回)
        return 1;
    }

    SERVER_COMMAND(cmd);
    SERVER_EXECUTE();
    return 1;
}

// hash_string(const string[], const HashType:type, output[], const outputSize) - 计算字符串哈希
// HashType: 0=Sha1, 1=Sha256, 2=Sha384, 3=Sha512, 4=Keccak256, 5=Sha3_256, 6=Sha3_512, 7=Md5
// 简化实现: 均使用 MD5 (32 字节十六进制), 兼容大多数插件用途
cell AMX_NATIVE_CALL amxx_hash_string(AMX *amx, cell *params)
{
    cell *str_addr;
    amx_GetAddr(amx, params[1], &str_addr);
    char str[4096];
    amx_GetString(str, str_addr, 0, sizeof(str));

    amxx_md5_ctx ctx;
    amxx_md5_init(&ctx);
    amxx_md5_update(&ctx, (const unsigned char *)str, strlen(str));

    unsigned char digest[16];
    amxx_md5_final(digest, &ctx);

    // 输出十六进制字符串
    char hex[33];
    static const char hexchars[] = "0123456789abcdef";
    for (int i = 0; i < 16; i++) {
        hex[i * 2] = hexchars[(digest[i] >> 4) & 0xF];
        hex[i * 2 + 1] = hexchars[digest[i] & 0xF];
    }
    hex[32] = '\0';

    cell *out;
    amx_GetAddr(amx, params[3], &out);
    int outputSize = (int)params[4];
    return amx_SetString(out, hex, 0, 0, outputSize);
}

// hash_file(const fileName[], const HashType:type, output[], const outputSize) - 计算文件哈希
cell AMX_NATIVE_CALL amxx_hash_file(AMX *amx, cell *params)
{
    cell *file_addr;
    amx_GetAddr(amx, params[1], &file_addr);
    char filepath[256];
    amx_GetString(filepath, file_addr, 0, sizeof(filepath));

    FILE *fp = fopen(filepath, "rb");
    if (!fp)
        return 0;

    amxx_md5_ctx ctx;
    amxx_md5_init(&ctx);

    unsigned char buf[4096];
    size_t bytesRead;
    while ((bytesRead = fread(buf, 1, sizeof(buf), fp)) > 0)
        amxx_md5_update(&ctx, buf, bytesRead);

    fclose(fp);

    unsigned char digest[16];
    amxx_md5_final(digest, &ctx);

    char hex[33];
    static const char hexchars[] = "0123456789abcdef";
    for (int i = 0; i < 16; i++) {
        hex[i * 2] = hexchars[(digest[i] >> 4) & 0xF];
        hex[i * 2 + 1] = hexchars[digest[i] & 0xF];
    }
    hex[32] = '\0';

    cell *out;
    amx_GetAddr(amx, params[3], &out);
    int outputSize = (int)params[4];
    return amx_SetString(out, hex, 0, 0, outputSize);
}

// 前向声明：实现位于文件后部，但注册表需在此处引用
cell AMX_NATIVE_CALL amxx_power(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_sqroot(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_tickcount(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_time(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_date(AMX *amx, cell *params);

AMX_NATIVE_INFO core_natives[] = {
    {"register_plugin", amxx_register_plugin},
    {"server_print", amxx_server_print},
    {"server_cmd", amxx_server_cmd},
    {"client_print", amxx_client_print},
    {"client_cmd", amxx_client_cmd},
    {"console_print", amxx_console_print},
    {"get_user_name", amxx_get_user_name},
    {"get_user_model", amxx_get_user_model},
    {"get_players", amxx_get_players},
    {"get_playersnum", amxx_get_playersnum},
    {"get_maxplayers", amxx_get_maxplayers},
    {"is_user_alive", amxx_is_user_alive},
    {"set_task", amxx_set_task},
    {"change_task", amxx_change_task},
    {"remove_task", amxx_remove_task},
    {"task_exists", amxx_task_exists},
    {"get_gametime", amxx_get_gametime},
    {"set_hudmessage", amxx_set_hudmessage},
    {"show_hudmessage", amxx_show_hudmessage},
    {"next_hudchannel", amxx_next_hudchannel},
    {"random_num", amxx_random_num},
    {"random_float", amxx_random_float},
    {"strlen", amxx_strlen},
    {"equal", amxx_equal},
    {"contain", amxx_contain},
    {"remove_quotes", amxx_remove_quotes},
    {"read_argc", amxx_read_argc},
    {"read_argv", amxx_read_argv},
    {"read_argv_int", amxx_read_argv_int},
    {"read_argv_float", amxx_read_argv_float},
    {"read_args", amxx_read_args},
    {"log_amx", amxx_log_amx},
    {"log_message", amxx_log_message},
    {"get_time", amxx_get_time},
    {"get_user_msgid", amxx_get_user_msgid},
    {"get_user_msgname", amxx_get_user_msgname},
    {"numargs", amxx_numargs},
    {"getarg", amxx_getarg},
    {"setarg", amxx_setarg},
    {"heapspace", amxx_heapspace},
    {"funcidx", amxx_funcidx},
    {"swapchars", amxx_swapchars},
    {"tolower", amxx_tolower},
    {"toupper", amxx_toupper},
    {"min", amxx_min},
    {"max", amxx_max},
    {"clamp", amxx_clamp},
    {"random", amxx_random},
    {"strcpy", amxx_strcpy},
    {"strcat", amxx_strcat},
    {"strcmp", amxx_strcmp},
    {"strfind", amxx_strfind},
    {"strtok", amxx_strtok},
    {"format", amxx_format},
    {"fopen", amxx_fopen},
    {"fclose", amxx_fclose},
    {"fread", amxx_fread},
    {"fwrite", amxx_fwrite},
    {"fseek", amxx_fseek},
    {"ftell", amxx_ftell},
    {"feof", amxx_feof},
    {"file_size", amxx_file_size},
    {"fread_blocks", amxx_fread_blocks},
    {"floatround", amxx_floatround},
    {"floatsqroot", amxx_floatsqroot},
    {"floatabs", amxx_floatabs},
    {"floatcos", amxx_floatcos},
    {"floatsin", amxx_floatsin},
    {"floattan", amxx_floattan},
    {"floatasin", amxx_floatasin},
    {"floatacos", amxx_floatacos},
    {"floatatan", amxx_floatatan},
    {"floatatan2", amxx_floatatan2},
    {"floatsinh", amxx_floatsinh},
    {"floatcosh", amxx_floatcosh},
    {"floattanh", amxx_floattanh},
    {"floatlog", amxx_floatlog},
    {"get_cvar_string", amxx_get_cvar_string},
    {"get_cvar_num", amxx_get_cvar_num},
    {"set_cvar_string", amxx_set_cvar_string},
    {"set_cvar_num", amxx_set_cvar_num},
    {"register_cvar", amxx_register_cvar},
    {"get_mapname", amxx_get_mapname},
    {"change_map", amxx_change_map},
    {"engine_changelevel", amxx_change_map}, // 原版别名：触发 server_changelevel forward 后切换地图
    {"client_kick", amxx_client_kick},
    {"client_disconnect", amxx_client_disconnect},
    {"get_user_ip", amxx_get_user_ip},
    {"get_user_team", amxx_get_user_team},
    {"set_user_team", amxx_set_user_team},
    {"give_item", amxx_give_item},
    {"strip_user_weapons", amxx_strip_user_weapons},

    {"is_user_connected", amxx_is_user_connected},
    {"is_user_bot", amxx_is_user_bot},
    {"is_user_hltv", amxx_is_user_hltv},
    {"get_user_health", amxx_get_user_health},
    {"set_user_health", amxx_set_user_health},
    {"get_user_armor", amxx_get_user_armor},
    {"set_user_armor", amxx_set_user_armor},
    {"get_user_frags", amxx_get_user_frags},
    {"set_user_frags", amxx_set_user_frags},
    {"get_user_deaths", amxx_get_user_deaths},
    {"set_user_deaths", amxx_set_user_deaths},
    {"get_user_weapon", amxx_get_user_weapon},
    {"get_user_weaponname", amxx_get_user_weaponname},
    {"get_user_weapons", amxx_get_user_weapons},
    {"user_has_weapon", amxx_user_has_weapon},
    {"get_user_ammo", amxx_get_user_ammo},
    {"get_user_ping", amxx_get_user_ping},
    {"get_user_time", amxx_get_user_time},
    {"get_user_origin", amxx_get_user_origin},
    {"get_user_attacker", amxx_get_user_attacker},
    {"get_user_aiming", amxx_get_user_aiming},

    {"precache_model", amxx_precache_model},
    {"precache_sound", amxx_precache_sound},
    {"precache_generic", amxx_precache_generic},

    {"set_dhudmessage", amxx_set_dhudmessage},
    {"show_dhudmessage", amxx_show_dhudmessage},
    {"show_motd", amxx_show_motd},

    {"test_string", amxx_test_string},
    {"test_string2", amxx_test_string2},
    {"message_begin", amxx_message_begin},
    {"message_end", amxx_message_end},
    {"write_byte", amxx_write_byte},
    {"write_short", amxx_write_short},
    {"write_long", amxx_write_long},
    {"write_float", amxx_write_float},
    {"write_string", amxx_write_string},
    {"write_coord", amxx_write_coord},
    {"client_print_color", amxx_client_print_color},
    {"server_exec", amxx_server_exec},
    {"remove_tasks", amxx_remove_tasks},
    
    // 字符串操作扩展
    {"strtoupper", amxx_strtoupper},
    {"strtolower", amxx_strtolower},
    {"strreplace", amxx_strreplace},
    {"strins", amxx_strins},
    {"strdel", amxx_strdel},
    
    // 数学运算
    {"floatadd", amxx_floatadd},
    {"floatmul", amxx_floatmul},
    {"floatdiv", amxx_floatdiv},
    {"floatsub", amxx_floatsub},
    {"floatmod", amxx_floatmod},
    
    // 位操作
    {"bit", amxx_bit},
    {"bits", amxx_bits},
    {"bit_set", amxx_bit_set},
    {"bit_get", amxx_bit_get},
    {"bit_test", amxx_bit_test},
    
    // 文件系统
    {"file_exists", amxx_file_exists},
    {"file_delete", amxx_file_delete},
    {"delete_file", amxx_file_delete},
    {"mkdir", amxx_mkdir},
    {"read_dir", amxx_read_dir},
    {"delete_dir", amxx_delete_dir},
    {"rename_file", amxx_rename_file},
    
    // 字符串操作扩展
    {"copy", amxx_copy},
    {"add", amxx_add},
    {"num_to_str", amxx_num_to_str},
    {"str_to_num", amxx_str_to_num},
    {"float_to_str", amxx_float_to_str},
    {"str_to_float", amxx_str_to_float},
    {"floatstr", amxx_floatstr},   // float.inc: str_to_float 的别名
    {"trim", amxx_trim},
    {"containi", amxx_containi},
    {"equali", amxx_equali},
    {"strncmp", amxx_strncmp},
    
    // 数学运算扩展
    {"abs", amxx_abs},
    {"ceil", amxx_ceil},
    {"floor", amxx_floor},
    {"pow", amxx_pow},
    {"sqrt", amxx_sqrt},
    {"floatpower", amxx_pow},
    {"floatceil", amxx_ceil},
    {"floatfloor", amxx_floor},
    {"floatfract", amxx_floatfract},  // float.inc: 返回小数部分
    {"log", amxx_log},
    {"log10", amxx_log10},
    
    // 位操作扩展
    {"bit_reset", amxx_bit_reset},
    {"bit_flip", amxx_bit_flip},
    
    // AMX参数操作
    {"set_string", amxx_set_string},
    {"set_param_byref", amxx_set_param_byref},
    {"get_param_byref", amxx_get_param_byref},
    {"get_float_byref", amxx_get_float_byref},
    {"set_float_byref", amxx_set_float_byref},
    
    // 数据结构
    {"arrayset", amxx_arrayset},
    
    // 函数调用
    {"callfunc_begin", amxx_callfunc_begin},
    {"callfunc_begin_i", amxx_callfunc_begin_i},
    {"callfunc_end", amxx_callfunc_end},
    {"callfunc_push_int", amxx_callfunc_push_int},
    {"callfunc_push_float", amxx_callfunc_push_float},
    {"callfunc_push_str", amxx_callfunc_push_str},
    {"callfunc_push_array", amxx_callfunc_push_array},
    {"callfunc_push_intrf", amxx_callfunc_push_intrf},
    {"callfunc_push_floatrf", amxx_callfunc_push_floatrf},

    // P1: 服务器信息
    {"is_dedicated_server", amxx_is_dedicated_server},
    {"is_linux_server", amxx_is_linux_server},
    {"get_amxx_verstring", amxx_get_amxx_verstring},
    {"server_name", amxx_server_name},
    // P1: 目录路径
    {"get_configsdir", amxx_get_configsdir},
    {"get_datadir", amxx_get_datadir},
    // P1: localinfo
    {"set_localinfo", amxx_set_localinfo},
    {"get_localinfo", amxx_get_localinfo},
    // P1: 音效
    {"emit_sound", amxx_emit_sound},
    // P1: CVar 增强
    {"get_cvar_flags", amxx_get_cvar_flags},
    {"set_cvar_flags", amxx_set_cvar_flags},
    {"get_cvar_pointer", amxx_get_cvar_pointer},
    {"query_client_cvar", amxx_query_client_cvar},
    // P1: 插件管理
    {"plugin_flags", amxx_plugin_flags},
    {"module_exists", amxx_module_exists},
    {"get_plugins", amxx_get_plugins},
    // P1: 字符串工具
    {"replace_string", amxx_replace_string},
    {"replace_all", amxx_replace_all},
    {"parse", amxx_parse},
    {"sort_custom_1d", amxx_sort_custom_1d},
    {"sort_custom_2d", amxx_sort_custom_2d},
    {"SortCustom2D", amxx_SortCustom2D},  // sorting.inc 标准名 (callback 版)
    
    // P3: 排序
    {"sort_integ", amxx_sort_integ},
    {"sort_float", amxx_sort_float},
    {"sort_str", amxx_sort_str},

    // P2: 时间处理
    {"get_systime", amxx_get_systime},
    {"parse_time", amxx_parse_time},
    {"format_time", amxx_format_time},
    // P2: 字符串
    {"split", amxx_split},
    // P2: pcvar
    {"pcvar_num", amxx_pcvar_num},
    {"get_pcvar_num", amxx_get_pcvar_num},
    {"get_pcvar_string", amxx_get_pcvar_string},
    {"set_pcvar_num", amxx_set_pcvar_num},
    {"set_pcvar_string", amxx_set_pcvar_string},
    {"get_pcvar_flags", amxx_get_pcvar_flags},
    {"set_pcvar_flags", amxx_set_pcvar_flags},
    {"set_pcvar_float", amxx_set_pcvar_float},
    {"get_pcvar_bool", amxx_get_pcvar_bool},
    {"get_cvar_bool", amxx_get_cvar_bool},
    {"bind_pcvar_num", amxx_bind_pcvar_num},
    {"bind_pcvar_float", amxx_bind_pcvar_float},
    {"bind_pcvar_string", amxx_bind_pcvar_string},

    // cvars.inc: 补全 9 个 native
    {"create_cvar", amxx_create_cvar},
    {"cvar_exists", amxx_cvar_exists},
    {"hook_cvar_change", amxx_hook_cvar_change},
    {"disable_cvar_hook", amxx_disable_cvar_hook},
    {"enable_cvar_hook", amxx_enable_cvar_hook},
    {"remove_cvar_flags", amxx_remove_cvar_flags},
    {"set_pcvar_bool", amxx_set_pcvar_bool},
    {"get_pcvar_bounds", amxx_get_pcvar_bounds},
    {"set_pcvar_bounds", amxx_set_pcvar_bounds},

    // P3: 参数/字符串工具
    {"argbreak", amxx_argbreak},
    {"string_to_array", amxx_string_to_array},
    {"array_to_string", amxx_array_to_string},
    
    {"get_user_userid", amxx_get_user_userid},
    {"get_user_authid", amxx_get_user_authid},
    {"get_weaponname", amxx_get_weaponname},
    {"get_weapon_id", amxx_get_weapon_id},
    
    // P2: CS 统计数据
    {"get_user_stats", amxx_get_user_stats},
    {"reset_user_stats", amxx_reset_user_stats},
    
    // P1: Bot 控制
    {"server_cmd_ex", amxx_server_cmd_ex},
    {"engclient_cmd", amxx_engclient_cmd},
    // P1: 文件操作高级封装
    {"read_file", amxx_read_file},
    {"write_file", amxx_write_file},
    {"append_file", amxx_append_file},
    {"write_filepos", amxx_write_filepos},
    
    // P3: 玩家数据扩展
    {"get_user_velocity", amxx_get_user_velocity},
    {"get_user_rendering", amxx_get_user_rendering},
    {"get_user_maxspeed", amxx_get_user_maxspeed},
    
    // 武器操作
    {"give_weapon", amxx_give_weapon},
    {"drop_weapon", amxx_drop_weapon},
    {"strip_weapon", amxx_strip_weapon},
    
    // 高优先级缺失
    {"get_basedir", amxx_get_basedir},
    {"build_path", amxx_build_path},
    {"amxx_version", amxx_amxx_version},
    {"amxx_time", amxx_amxx_time},
    {"get_user_info", amxx_get_user_info},
    {"set_user_info", amxx_set_user_info},
    {"register_cvar_change", amxx_register_cvar_change},
    {"console_cmd", amxx_console_cmd},
    {"ColorChat", amxx_color_chat},
    {"ColorChatTeam", amxx_color_chat_team},
    
    // 新增核心 natives
    {"floatcmp", amxx_floatcmp},
    {"formatex", amxx_formatex},
    {"vformat", amxx_vformat},
    {"float", amxx_float},
    {"replace", amxx_replace},
    {"isspace", amxx_isspace},
    {"ucfirst", amxx_ucfirst},
    {"abort", amxx_abort},
    {"find_player", amxx_find_player},
    {"is_user_connecting", amxx_is_user_connecting},
    {"set_cvar_float", amxx_set_cvar_float},
    {"get_cvar_float", amxx_get_cvar_float},
    {"get_pcvar_float", amxx_get_pcvar_float},
    {"is_map_valid", amxx_is_map_valid},
    {"get_modname", amxx_get_modname},
    {"fgets", amxx_fgets},
    {"get_timeleft", amxx_get_timeleft},
    {"get_pluginsnum", amxx_get_pluginsnum},
    {"get_plugin", amxx_get_plugin},
    {"get_modulesnum", amxx_get_modulesnum},
    {"get_module", amxx_get_module},
    {"find_plugin_byfile", amxx_find_plugin_byfile},
    {"get_plugins_cvarsnum", amxx_get_plugins_cvarsnum},
    {"get_plugins_cvar", amxx_get_plugins_cvar},
    {"is_plugin_loaded", amxx_is_plugin_loaded},
    {"is_module_loaded", amxx_is_module_loaded},
    {"LibraryExists", amxx_LibraryExists},
    
    // Additional core natives
    {"register_native", amxx_register_native},
    {"get_func_id", amxx_get_func_id},
    {"set_fail_state", amxx_set_fail_state},
    {"get_param", amxx_get_param},
    {"get_param_f", amxx_get_param_f},
    {"get_string", amxx_get_string},
    {"get_array_f", amxx_get_array_f},
    {"set_array_f", amxx_set_array_f},
    {"get_array", amxx_get_array},
    {"set_array", amxx_set_array},
    {"param_convert", amxx_param_convert},
    {"register_library", amxx_register_library},
    {"read_data", amxx_read_data},
    {"set_kvd", amxx_set_kvd},
    {"vdformat", amxx_vdformat},
    {"log_error", amxx_log_error},
    {"ExecuteForward", amxx_execute_forward},
    {"CreateMultiForward", amxx_create_multi_forward},
    {"CreateMultiForwardEx", amxx_create_multi_forward},
    {"CreateOneForward", amxx_create_one_forward},
    {"DestroyForward", amxx_destroy_forward},
    {"PrepareArray", amxx_prepare_array},
    {"GetForwardFunctionCount", amxx_get_forward_func_count},
    
    // Player state natives
    {"set_user_origin", amxx_set_user_origin},
    {"velocity_by_aim", amxx_velocity_by_aim},
    
    // Other natives
    {"pause", amxx_pause},
    {"unpause", amxx_unpause},
    {"isalnum", amxx_isalnum},
    {"num_to_word", amxx_num_to_word},
    {"set_module_filter", amxx_set_module_filter},
    {"set_native_filter", amxx_set_native_filter},
    // xvar natives
    {"get_xvar_id", amxx_get_xvar_id},
    {"get_xvar_num", amxx_get_xvar_num},
    {"set_xvar_num", amxx_set_xvar_num},

    // P2-1 & P2-2: utility natives
    {"microsec", amxx_microsec},
    {"md5", amxx_md5},

    // 补全核心 natives
    {"get_user_index", amxx_get_user_index},
    {"amxclient_cmd", amxx_amxclient_cmd},
    {"CreateHudSyncObj", amxx_CreateHudSyncObj},
    {"ShowSyncHudMsg", amxx_ShowSyncHudMsg},
    {"ClearSyncHud", amxx_ClearSyncHud},
    {"get_user_menu", amxx_get_user_menu},
    {"read_logdata", amxx_read_logdata},
    {"read_logargc", amxx_read_logargc},
    {"read_logargv", amxx_read_logargv},
    {"parse_loguser", amxx_parse_loguser},
    {"xvar_exists", amxx_xvar_exists},
    {"is_user_authorized", amxx_is_user_authorized},
    {"force_unmodified", amxx_force_unmodified},

    // P2-8: 补全核心 natives (event data / xvar float / player / weapon / hash / precache)
    {"read_datanum", amxx_read_datanum},
    {"read_datatype", amxx_read_datatype},
    {"get_xvar_float", amxx_get_xvar_float},
    {"set_xvar_float", amxx_set_xvar_float},
    {"find_player_ex", amxx_find_player_ex},
    {"get_weaponid", amxx_get_weaponid},
    {"has_map_ent_class", amxx_has_map_ent_class},
    {"engclient_print", amxx_engclient_print},
    {"precache_event", amxx_precache_event},
    {"is_jit_enabled", amxx_is_jit_enabled},
    {"md5_file", amxx_md5_file},
    {"RequestFrame", amxx_request_frame},
    {"AutoExecConfig", amxx_autoexec_config},
    {"hash_string", amxx_hash_string},
    {"hash_file", amxx_hash_file},
    {"power", amxx_power},
    {"sqroot", amxx_sqroot},
    {"tickcount", amxx_tickcount},
    {"time", amxx_time},
    {"date", amxx_date},

    {"message_begin_f", amxx_message_begin_f},
    {"write_char", amxx_write_char},
    {"write_entity", amxx_write_entity},
    {"write_angle", amxx_write_angle},
    {"write_angle_f", amxx_write_angle_f},
    {"write_coord_f", amxx_write_coord_f},

    // P2-9: 补全核心 natives (engine log / module / arch / debug / addr)
    {"elog_message", amxx_elog_message},
    {"require_module", amxx_require_module},
    {"is_amd64_server", amxx_is_amd64_server},
    {"set_error_filter", amxx_set_error_filter},
    {"dbg_trace_begin", amxx_dbg_trace_begin},
    {"dbg_trace_next", amxx_dbg_trace_next},
    {"dbg_trace_info", amxx_dbg_trace_info},
    {"dbg_fmt_error", amxx_dbg_fmt_error},
    {"int3", amxx_int3},
    {"get_var_addr", amxx_get_var_addr},
    {"get_addr_val", amxx_get_addr_val},
    {"set_addr_val", amxx_set_addr_val},

    // string.inc: 补全 21 个缺失 native
    {"replace_stringex", amxx_replace_stringex},
    {"fmt", amxx_fmt},
    {"format_args", amxx_format_args},
    {"strtol", amxx_strtol},
    {"strtof", amxx_strtof},
    {"copyc", amxx_copyc},
    {"setc", amxx_setc},
    {"strtok2", amxx_strtok2},
    {"mb_strtolower", amxx_mb_strtolower},
    {"mb_strtoupper", amxx_mb_strtoupper},
    {"mb_ucfirst", amxx_mb_ucfirst},
    {"mb_strtotitle", amxx_mb_strtotitle},
    {"is_string_category", amxx_is_string_category},
    {"isdigit", amxx_isdigit},
    {"isalpha", amxx_isalpha},
    {"is_char_mb", amxx_is_char_mb},
    {"is_char_upper", amxx_is_char_upper},
    {"is_char_lower", amxx_is_char_lower},
    {"get_char_bytes", amxx_get_char_bytes},
    {"argparse", amxx_argparse},
    {"split_string", amxx_split_string},

    // file.inc 补全: 27 个文件/目录 native
    {"fread_raw", amxx_fread_raw},
    {"fwrite_blocks", amxx_fwrite_blocks},
    {"fwrite_raw", amxx_fwrite_raw},
    {"fputs", amxx_fputs},
    {"fprintf", amxx_fprintf},
    {"fgetc", amxx_fgetc},
    {"fputc", amxx_fputc},
    {"fungetc", amxx_fungetc},
    {"fflush", amxx_fflush},
    {"filesize", amxx_filesize},
    {"dir_exists", amxx_dir_exists},
    {"rmdir", amxx_rmdir},
    {"unlink", amxx_unlink},
    {"open_dir", amxx_open_dir},
    {"next_file", amxx_next_file},
    {"close_dir", amxx_close_dir},
    {"LoadFileForMe", amxx_LoadFileForMe},
    {"GetFileTime", amxx_GetFileTime},
    {"SetFilePermissions", amxx_SetFilePermissions},
    {"FileReadInt8", amxx_FileReadInt8},
    {"FileReadUint8", amxx_FileReadUint8},
    {"FileReadInt16", amxx_FileReadInt16},
    {"FileReadUint16", amxx_FileReadUint16},
    {"FileReadInt32", amxx_FileReadInt32},
    {"FileWriteInt8", amxx_FileWriteInt8},
    {"FileWriteInt16", amxx_FileWriteInt16},
    {"FileWriteInt32", amxx_FileWriteInt32},

    {nullptr, nullptr}
};

// ===== P2: CS 统计数据 =====
// get_user_stats(index, stats[8], bodyparts[8]) - 获取玩家 CS 统计
// stats[0] = kills, stats[1] = deaths, stats[2] = headshots
// stats[3] = shots, stats[4] = hits, stats[5] = damage
// stats[6] = deaths, stats[7] = (reserved)
cell AMX_NATIVE_CALL amxx_get_user_stats(AMX *amx, cell *params)
{
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;
    CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
    if (!pPlayer) return 0;

    cell *stats, *bodyparts;
    amx_GetAddr(amx, params[2], &stats);
    amx_GetAddr(amx, params[3], &bodyparts);

    // Get stats from player death record
    AMXXRuntime &runtime = AMXXRuntime::GetInstance();
    PlayerDamageRecord &record = runtime.GetDamageRecord(index);

    stats[0] = 0;       // kills - not directly tracked in CBasePlayer
    stats[1] = pPlayer->m_iDeaths;  // deaths
    stats[2] = record.wasHeadshot ? 1 : 0;  // headshots
    stats[3] = 0;       // shots
    stats[4] = 0;       // hits
    stats[5] = (int)record.lastDamage;  // damage
    stats[6] = pPlayer->m_iDeaths;
    stats[7] = 0;

    // bodyparts stats
    for (int i = 0; i < 8; i++)
        bodyparts[i] = 0;

    return 1;
}

// reset_user_stats(index) - 重置玩家统计
cell AMX_NATIVE_CALL amxx_reset_user_stats(AMX *amx, cell *params)
{
    int index = (int)params[1];
    (void)amx;
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;
    CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
    if (!pPlayer) return 0;

    pPlayer->m_iDeaths = 0;
    AMXXRuntime::GetInstance().ResetDamageRecords();
    return 1;
}

// ===== P1: Bot 控制 =====

// server_cmd_ex(format, ...) - 格式化后执行服务器命令
cell AMX_NATIVE_CALL amxx_server_cmd_ex(AMX *amx, cell *params)
{
    cell *fmt_addr;
    amx_GetAddr(amx, params[1], &fmt_addr);
    char fmt[1024];
    amx_GetString(fmt, fmt_addr, 0, sizeof(fmt));

    int numArgs = (int)(params[0] / sizeof(cell)) - 1;
    char buffer[4096];

    if (numArgs <= 0) {
        snprintf(buffer, sizeof(buffer), "%s", fmt);
    } else if (numArgs == 1) {
        snprintf(buffer, sizeof(buffer), fmt, (int)params[2]);
    } else if (numArgs == 2) {
        snprintf(buffer, sizeof(buffer), fmt, (int)params[2], (int)params[3]);
    } else if (numArgs == 3) {
        snprintf(buffer, sizeof(buffer), fmt, (int)params[2], (int)params[3], (int)params[4]);
    } else if (numArgs == 4) {
        snprintf(buffer, sizeof(buffer), fmt, (int)params[2], (int)params[3], (int)params[4], (int)params[5]);
    } else {
        snprintf(buffer, sizeof(buffer), "%s", fmt);
    }

    SERVER_COMMAND(buffer);
    SERVER_EXECUTE();
    return 1;
}

// engclient_cmd(index, command[], arg1[], arg2[]) - 在客户端执行命令
cell AMX_NATIVE_CALL amxx_engclient_cmd(AMX *amx, cell *params)
{
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free)
        return 0;

    cell *cmd_addr, *arg1_addr, *arg2_addr;
    amx_GetAddr(amx, params[2], &cmd_addr);
    amx_GetAddr(amx, params[3], &arg1_addr);
    amx_GetAddr(amx, params[4], &arg2_addr);

    char cmd[256], arg1[256], arg2[256];
    amx_GetString(cmd, cmd_addr, 0, sizeof(cmd));
    amx_GetString(arg1, arg1_addr, 0, sizeof(arg1));
    amx_GetString(arg2, arg2_addr, 0, sizeof(arg2));

    g_engfuncs.pfnClientCommand(pEdict, "%s %s %s", cmd, arg1, arg2);
    return 1;
}

// ===== P1: 文件操作高级封装 =====

// read_file(filename, line, text[], maxlen, &linelen)
cell AMX_NATIVE_CALL amxx_read_file(AMX *amx, cell *params)
{
    cell *filename_addr;
    amx_GetAddr(amx, params[1], &filename_addr);
    char filename[512];
    amx_GetString(filename, filename_addr, 0, sizeof(filename));

    int lineNum = (int)params[2];
    cell *dest;
    amx_GetAddr(amx, params[3], &dest);
    int maxlen = (int)params[4];
    cell *linelen_out = nullptr;
    if (params[0] / sizeof(cell) >= 5)
        amx_GetAddr(amx, params[5], &linelen_out);

    FILE *fp = fopen(filename, "r");
    if (!fp) return 0;

    char buffer[4096];
    int currentLine = 0;
    bool found = false;

    while (fgets(buffer, sizeof(buffer), fp)) {
        if (currentLine == lineNum) {
            // Remove trailing newline
            size_t len = strlen(buffer);
            while (len > 0 && (buffer[len-1] == '\n' || buffer[len-1] == '\r'))
                buffer[--len] = '\0';

            amx_SetString(dest, buffer, 0, 0, maxlen);
            if (linelen_out)
                *linelen_out = (cell)len;
            found = true;
            break;
        }
        currentLine++;
    }

    fclose(fp);
    return found ? 1 : 0;
}

// write_file(filename, text[], line = -1)
cell AMX_NATIVE_CALL amxx_write_file(AMX *amx, cell *params)
{
    cell *filename_addr;
    amx_GetAddr(amx, params[1], &filename_addr);
    char filename[512];
    amx_GetString(filename, filename_addr, 0, sizeof(filename));

    cell *text_addr;
    amx_GetAddr(amx, params[2], &text_addr);
    char text[4096];
    amx_GetString(text, text_addr, 0, sizeof(text));

    int lineNum = (int)params[3];

    if (lineNum < 0) {
        // Append to file
        FILE *fp = fopen(filename, "a");
        if (!fp) return 0;
        fprintf(fp, "%s\n", text);
        fclose(fp);
        return 1;
    }

    // Write at specific line - read all, modify, rewrite
    FILE *fp = fopen(filename, "r");
    if (!fp) return 0;

    std::vector<std::string> lines;
    char buf[4096];
    while (fgets(buf, sizeof(buf), fp)) {
        size_t len = strlen(buf);
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
            buf[--len] = '\0';
        lines.push_back(buf);
    }
    fclose(fp);

    // Ensure we have enough lines
    while ((int)lines.size() <= lineNum)
        lines.push_back("");

    lines[lineNum] = text;

    fp = fopen(filename, "w");
    if (!fp) return 0;
    for (size_t i = 0; i < lines.size(); i++)
        fprintf(fp, "%s\n", lines[i].c_str());
    fclose(fp);
    return 1;
}

// append_file(filename, text[]) - 以追加模式写一行文本（自动加 \n）
cell AMX_NATIVE_CALL amxx_append_file(AMX *amx, cell *params)
{
    cell *filename_addr;
    amx_GetAddr(amx, params[1], &filename_addr);
    char filename[512];
    amx_GetString(filename, filename_addr, 0, sizeof(filename));

    cell *text_addr;
    amx_GetAddr(amx, params[2], &text_addr);
    char text[4096];
    amx_GetString(text, text_addr, 0, sizeof(text));

    FILE *fp = fopen(filename, "a");
    if (!fp)
        return 0;
    fprintf(fp, "%s\n", text);
    fclose(fp);
    return 1;
}

// P2: write_filepos(filename, text[], pos, flags = 0)
// 在文件指定字节位置写入文本（覆盖该位置起的内容）
cell AMX_NATIVE_CALL amxx_write_filepos(AMX *amx, cell *params)
{
    cell *filename_addr;
    amx_GetAddr(amx, params[1], &filename_addr);
    char filename[512];
    amx_GetString(filename, filename_addr, 0, sizeof(filename));

    cell *text_addr;
    amx_GetAddr(amx, params[2], &text_addr);
    char text[4096];
    amx_GetString(text, text_addr, 0, sizeof(text));

    long pos = (long)params[3];
    int flags = (params[0] / (int)sizeof(cell) >= 4) ? (int)params[4] : 0;

    // flags & 1: 追加模式（在 pos 位置插入而非覆盖）
    if (flags & 1) {
        // 插入模式：读取 pos 之后的内容，写入 text，再追加原内容
        FILE *fp = fopen(filename, "r");
        if (!fp && !(flags & 2)) return 0;
        std::string tail;
        if (fp) {
            fseek(fp, pos, SEEK_SET);
            char buf[4096];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
                tail.append(buf, n);
            fclose(fp);
        }
        fp = fopen(filename, pos > 0 ? "r+b" : "wb");
        if (!fp) return 0;
        if (pos > 0) fseek(fp, pos, SEEK_SET);
        fwrite(text, 1, strlen(text), fp);
        if (!tail.empty())
            fwrite(tail.data(), 1, tail.size(), fp);
        fclose(fp);
    } else {
        // 覆盖模式：在 pos 位置直接写入
        FILE *fp = fopen(filename, pos > 0 ? "r+b" : "wb");
        if (!fp) return 0;
        if (pos > 0) fseek(fp, pos, SEEK_SET);
        fwrite(text, 1, strlen(text), fp);
        fclose(fp);
    }
    return 1;
}

// get_user_velocity(index, vel[3])
cell AMX_NATIVE_CALL amxx_get_user_velocity(AMX *amx, cell *params)
{
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict) return 0;

    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    dest[0] = amx_ftoc(pEdict->v.velocity.x);
    dest[1] = amx_ftoc(pEdict->v.velocity.y);
    dest[2] = amx_ftoc(pEdict->v.velocity.z);
    return 1;
}

// get_user_rendering(index, &fx, &r, &g, &b, &amount)
cell AMX_NATIVE_CALL amxx_get_user_rendering(AMX *amx, cell *params)
{
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict) return 0;

    cell *fxOut = nullptr, *rOut = nullptr, *gOut = nullptr, *bOut = nullptr, *amtOut = nullptr;
    if (params[0] / sizeof(cell) >= 2) amx_GetAddr(amx, params[2], &fxOut);
    if (params[0] / sizeof(cell) >= 3) amx_GetAddr(amx, params[3], &rOut);
    if (params[0] / sizeof(cell) >= 4) amx_GetAddr(amx, params[4], &gOut);
    if (params[0] / sizeof(cell) >= 5) amx_GetAddr(amx, params[5], &bOut);
    if (params[0] / sizeof(cell) >= 6) amx_GetAddr(amx, params[6], &amtOut);

    // P2-6: when rendermode is kRenderNormal (0), return sensible defaults
    // instead of potentially stale rendercolor/renderamt values.
    if (pEdict->v.rendermode == kRenderNormal) {
        if (fxOut) *fxOut = 0;
        if (rOut) *rOut = 0;
        if (gOut) *gOut = 0;
        if (bOut) *bOut = 0;
        if (amtOut) *amtOut = 0;
    } else {
        if (fxOut) *fxOut = (cell)pEdict->v.renderfx;
        if (rOut) *rOut = (cell)pEdict->v.rendercolor.x;
        if (gOut) *gOut = (cell)pEdict->v.rendercolor.y;
        if (bOut) *bOut = (cell)pEdict->v.rendercolor.z;
        if (amtOut) *amtOut = (cell)pEdict->v.renderamt;
    }
    return 1;
}

// get_user_maxspeed(index)
cell AMX_NATIVE_CALL amxx_get_user_maxspeed(AMX *amx, cell *params)
{
    int index = (int)params[1];
    (void)amx;
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict) return 0;
    return amx_ftoc(pEdict->v.maxspeed);
}

// give_weapon(index, weapon[]) - 给予玩家指定武器 (返回武器实体索引)
cell AMX_NATIVE_CALL amxx_give_weapon(AMX *amx, cell *params)
{
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;
    CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
    if (!pPlayer) return 0;

    cell *weapon_addr;
    amx_GetAddr(amx, params[2], &weapon_addr);
    char weapon[64];
    amx_GetString(weapon, weapon_addr, 0, sizeof(weapon));

    CBaseEntity *pEntity = pPlayer->GiveNamedItem(weapon);
    return pEntity ? ENTINDEX(pEntity->edict()) : 0;
}

// drop_weapon(index) - 丢弃玩家当前武器
cell AMX_NATIVE_CALL amxx_drop_weapon(AMX *amx, cell *params)
{
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;
    CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
    if (!pPlayer) return 0;

    if (pPlayer->m_pActiveItem) {
        CBasePlayerItem *pItem = pPlayer->m_pActiveItem;
        CBasePlayerWeapon *pWeapon = static_cast<CBasePlayerWeapon *>(pItem);
        pWeapon->Holster();  // or pPlayer->DropPlayerItem
        return 1;
    }
    return 0;
}

// strip_weapon(index, weapon[]) - 移除玩家指定武器
cell AMX_NATIVE_CALL amxx_strip_weapon(AMX *amx, cell *params)
{
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;
    CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
    if (!pPlayer) return 0;

    cell *weapon_addr;
    amx_GetAddr(amx, params[2], &weapon_addr);
    char weapon[64];
    amx_GetString(weapon, weapon_addr, 0, sizeof(weapon));

    // Scan through all weapon slots
    for (int i = MAX_ITEM_TYPES - 1; i >= 0; i--) {
        CBasePlayerItem *pItem = pPlayer->m_rgpPlayerItems[i];
        while (pItem) {
            const char *classname = STRING(pItem->pev->classname);
            if (classname && strcmp(classname, weapon) == 0) {
                pPlayer->RemovePlayerItem(pItem);
                pItem->pev->flags |= FL_KILLME;
                return 1;
            }
            pItem = pItem->m_pNext;
        }
    }
    return 0;
}

// ===== 高优先级缺失 =====

// get_basedir(buffer[], len) - 获取游戏基础目录
cell AMX_NATIVE_CALL amxx_get_basedir(AMX *amx, cell *params)
{
    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    int maxlen = (int)params[2];
    char basedir[512];
    GET_GAME_DIR(basedir);
    return amx_SetString(dest, basedir, 0, 0, maxlen);
}

// build_path(dest[], len, fmt[], ...) - 构建路径
cell AMX_NATIVE_CALL amxx_build_path(AMX *amx, cell *params)
{
    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    int maxlen = (int)params[2];

    cell *fmt_addr;
    amx_GetAddr(amx, params[3], &fmt_addr);
    char fmt[512];
    amx_GetString(fmt, fmt_addr, 0, sizeof(fmt));

    int numArgs = (int)(params[0] / sizeof(cell)) - 3;
    char path[1024];

    if (numArgs <= 0) {
        snprintf(path, sizeof(path), "%s", fmt);
    } else if (numArgs == 1) {
        snprintf(path, sizeof(path), fmt, (int)params[4]);
    } else if (numArgs == 2) {
        snprintf(path, sizeof(path), fmt, (int)params[4], (int)params[5]);
    } else if (numArgs == 3) {
        snprintf(path, sizeof(path), fmt, (int)params[4], (int)params[5], (int)params[6]);
    } else {
        snprintf(path, sizeof(path), "%s", fmt);
    }

    // 如果路径以 "addons/amxmodx/" 开头，前面加上游戏目录
    char basedir[512];
    GET_GAME_DIR(basedir);
    char fullPath[1024];
    snprintf(fullPath, sizeof(fullPath), "%s/%s", basedir, path);
    return amx_SetString(dest, fullPath, 0, 0, maxlen);
}

// amxx_version(dest[], len) - 获取 AMXX 版本
// 与 amxmodx_version cvar (runtime.cpp 中注册为 "1.8.2") 保持一致
cell AMX_NATIVE_CALL amxx_amxx_version(AMX *amx, cell *params)
{
    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    return amx_SetString(dest, "1.8.2", 0, 0, (int)params[2]);
}

// amxx_time(dest[], len, format) - 获取 AMXX 格式化时间
cell AMX_NATIVE_CALL amxx_amxx_time(AMX *amx, cell *params)
{
    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    int maxlen = (int)params[2];

    cell *fmt_addr;
    amx_GetAddr(amx, params[3], &fmt_addr);
    char fmt[256];
    amx_GetString(fmt, fmt_addr, 0, sizeof(fmt));

    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    char buffer[512];
    strftime(buffer, sizeof(buffer), fmt[0] ? fmt : "%Y-%m-%d %H:%M:%S", timeinfo);
    return amx_SetString(dest, buffer, 0, 0, maxlen);
}

// get_user_info(index, key[], dest[], len) - 从 info buffer 获取玩家信息
cell AMX_NATIVE_CALL amxx_get_user_info(AMX *amx, cell *params)
{
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict) return 0;

    cell *key_addr;
    amx_GetAddr(amx, params[2], &key_addr);
    char key[64];
    amx_GetString(key, key_addr, 0, sizeof(key));

    cell *dest;
    amx_GetAddr(amx, params[3], &dest);
    int maxlen = (int)params[4];

    char *infobuffer = g_engfuncs.pfnGetInfoKeyBuffer(pEdict);
    if (!infobuffer) {
        amx_SetString(dest, "", 0, 0, maxlen);
        return 0;
    }
    const char *value = g_engfuncs.pfnInfoKeyValue(infobuffer, key);
    if (!value) value = "";
    return amx_SetString(dest, value, 0, 0, maxlen);
}

// set_user_info(index, key[], value[]) - 设置玩家 info buffer
cell AMX_NATIVE_CALL amxx_set_user_info(AMX *amx, cell *params)
{
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict) return 0;

    cell *key_addr, *val_addr;
    amx_GetAddr(amx, params[2], &key_addr);
    amx_GetAddr(amx, params[3], &val_addr);
    char key[64], value[256];
    amx_GetString(key, key_addr, 0, sizeof(key));
    amx_GetString(value, val_addr, 0, sizeof(value));

    char *infobuffer = g_engfuncs.pfnGetInfoKeyBuffer(pEdict);
    if (!infobuffer) return 0;
    g_engfuncs.pfnSetClientKeyValue(index, infobuffer, key, value);
    return 1;
}

// register_cvar_change(cvar[], funcname[]) - 注册 CVar 变更回调
cell AMX_NATIVE_CALL amxx_register_cvar_change(AMX *amx, cell *params)
{
    cell *cvar_addr, *funcname_addr;
    amx_GetAddr(amx, params[1], &cvar_addr);
    amx_GetAddr(amx, params[2], &funcname_addr);
    char cvar[128], funcname[128];
    amx_GetString(cvar, cvar_addr, 0, sizeof(cvar));
    amx_GetString(funcname, funcname_addr, 0, sizeof(funcname));

    int funcidx;
    if (amx_FindPublic(amx, funcname, &funcidx) != AMX_ERR_NONE)
        return 0;

    CVarChangeCallback cb;
    cb.amx = amx;
    cb.funcidx = funcidx;
    cb.cvarName = cvar;
    g_cvarCallbacks[cvar].push_back(cb);
    return 1;
}

// console_cmd(index, cmd[]) - 在玩家控制台执行命令
cell AMX_NATIVE_CALL amxx_console_cmd(AMX *amx, cell *params)
{
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free)
        return 0;

    cell *cmd_addr;
    amx_GetAddr(amx, params[2], &cmd_addr);
    char cmd[1024];
    amx_GetString(cmd, cmd_addr, 0, sizeof(cmd));
    CLIENT_PRINTF(pEdict, print_console, cmd);
    return 1;
}

// ColorChat(index, color, msg[], ...) - 彩色聊天
cell AMX_NATIVE_CALL amxx_color_chat(AMX *amx, cell *params)
{
    // 复用 client_print_color 实现
    // params[1] = index, params[2] = color, params[3+] = format
    // 平移 params[0] 使其看起来像 client_print_color(index, color, msg, ...)
    cell newParams[32];
    int numParams = (int)(params[0] / sizeof(cell));
    newParams[0] = numParams * sizeof(cell);
    newParams[1] = params[1];  // index
    newParams[2] = params[2];  // color
    newParams[3] = params[3];  // msg format start
    for (int i = 4; i <= numParams; i++)
        newParams[i] = params[i];
    return amxx_client_print_color(amx, newParams);
}

// ColorChatTeam(index, team, msg[], ...) - 队伍彩色聊天
cell AMX_NATIVE_CALL amxx_color_chat_team(AMX *amx, cell *params)
{
    int index = (int)params[1];
    int team = (int)params[2];
    // Map team to color: 0=default, 1=red(T), 2=blue(CT)
    int color;
    switch (team) {
        case 1: color = 2; break;  // T → RED
        case 2: color = 3; break;  // CT → BLUE
        default: color = 0; break; // default
    }
    cell newParams[32];
    int numParams = (int)(params[0] / sizeof(cell));
    newParams[0] = numParams * sizeof(cell);
    newParams[1] = params[1];  // index
    newParams[2] = color;      // mapped color
    newParams[3] = params[3];  // msg
    for (int i = 4; i <= numParams; i++)
        newParams[i] = params[i];
    return amxx_client_print_color(amx, newParams);
}

// ===== 注册 =====

void RegisterCoreNatives(AMX *amx)
{
    AMXX_LOG_DBG("[Natives] Registering core natives...");
    int count = 0;
    while (core_natives[count].name != nullptr) count++;
    AMXX_LOG_DBG("[Natives] Found %d core natives to register", count);
    amx_Register(amx, core_natives, -1);
    AMXX_LOG_DBG("[Natives] Core natives registered successfully");
}

cell AMX_NATIVE_CALL amxx_register_plugin(AMX *amx, cell *params)
{
    int numParams = (int)(params[0] / sizeof(cell));

    char title[256], version[256], author[256];
    cell *addr;

    amx_GetAddr(amx, params[1], &addr);
    amx_GetString(title, addr, 0, sizeof(title));
    amx_GetAddr(amx, params[2], &addr);
    amx_GetString(version, addr, 0, sizeof(version));
    amx_GetAddr(amx, params[3], &addr);
    amx_GetString(author, addr, 0, sizeof(author));

    AMXX_LOG_DBG("[Natives] register_plugin: title='%s', version='%s', author='%s'", title, version, author);

    AMXXRuntime &runtime = AMXXRuntime::GetInstance();
    for (size_t i = 0; i < runtime.GetPlugins().size(); i++) {
        AMXXPlugin *plugin = runtime.GetPlugins()[i];
        if (plugin && plugin->GetAMX() == amx && plugin->IsLoaded()) {
            plugin->SetTitle(title);
            plugin->SetVersion(version);
            plugin->SetAuthor(author);
            AMXX_LOG_DBG("[Natives] register_plugin: Plugin '%s' registered successfully", plugin->GetName());
            return 1;
        }
    }

    AMXX_LOG("[Natives] register_plugin: Plugin not found for AMX");
    return 0;
}

static bool is_flag_char(char c)
{
    return c == '-' || c == '+' || c == ' ' || c == '#' || c == '0';
}

static bool is_length_modifier(char c)
{
    return c == 'l' || c == 'h' || c == 'z' || c == 't' || c == 'j' || c == 'L';
}

static bool is_format_type(char c)
{
    return c == 'd' || c == 'i' || c == 'u' || c == 'o' || c == 'x' || c == 'X'
        || c == 'f' || c == 'F' || c == 'e' || c == 'E' || c == 'g' || c == 'G'
        || c == 'a' || c == 'A'
        || c == 'c' || c == 's' || c == 'p' || c == 'n';
}

void amxx_format_string(AMX *amx, cell *params, int fmtIndex, char *output, size_t outputSize)
{
    char fmtBuffer[1024];
    cell *addr;
    amx_GetAddr(amx, params[fmtIndex], &addr);
    amx_GetString(fmtBuffer, addr, 0, sizeof(fmtBuffer));

    int numParams = (int)(params[0] / sizeof(cell));
    int paramIdx = fmtIndex + 1;

    size_t outPos = 0;
    size_t len = strlen(fmtBuffer);

    for (size_t i = 0; i < len && outPos < outputSize - 1; i++) {
        if (fmtBuffer[i] == '%' && i + 1 < len) {
            char nextChar = fmtBuffer[i + 1];

            // 处理 %%
            if (nextChar == '%') {
                output[outPos++] = '%';
                i++; // 跳过第二个 %
                continue;
            }

            // 处理 %L（语言翻译）
            if (nextChar == 'L') {
                // 格式: %L lang_id key [...]
                // 参数1: lang（LANG_SERVER=0 或玩家ID）
                // 参数2: key（翻译键字符串）
                if (paramIdx + 1 > numParams) {
                    // 参数不足，直接输出 %L
                    output[outPos++] = '%';
                    output[outPos++] = 'L';
                    i++;
                    continue;
                }
                // 跳过 lang 参数
                paramIdx++; // lang 参数
                // 读取 key
                cell *keyAddr;
                amx_GetAddr(amx, params[paramIdx], &keyAddr);
                char keyBuf[256];
                amx_GetString(keyBuf, keyAddr, 0, sizeof(keyBuf));
                paramIdx++; // 消耗 key 参数

                char langResult[1024] = "";
                // 读取 lang 参数（LANG_SERVER=0 用服务器语言，>0 为玩家 index）
                cell *langAddr;
                amx_GetAddr(amx, params[paramIdx - 1], &langAddr);
                int langTarget = (int)*langAddr;
                // 通过 lang.h 的 GetLangForTarget 解析实际语言代码
                const char *langCode = AMXXLang::GetInstance().GetLangForTarget(langTarget);
                bool found = AMXXLang::GetInstance().GetString(keyBuf, langCode, langResult, sizeof(langResult));
                if (!found) {
                    strncpy(langResult, keyBuf, sizeof(langResult) - 1);
                }

                // 对翻译结果二次格式化（可能包含 %s, %d 等）
                char secBuf[1024];
                size_t secPos = 0;
                size_t resLen = strlen(langResult);
                for (size_t j = 0; j < resLen && secPos < sizeof(secBuf) - 1; j++) {
                    if (langResult[j] == '%' && j + 1 < resLen) {
                        char nxt = langResult[j + 1];
                        if (nxt == '%') {
                            secBuf[secPos++] = '%';
                            j++; // 跳过第二个 %
                            continue;
                        }
                        // 提取完整格式说明符（从 % 到类型字符）
                        size_t fmtStart = j;
                        size_t k = j + 1;
                        // 跳过 flags, width, precision, length
                        while (k < resLen && (is_flag_char(langResult[k]) ||
                               (langResult[k] >= '0' && langResult[k] <= '9') ||
                               langResult[k] == '*' || langResult[k] == '.' ||
                               is_length_modifier(langResult[k]))) {
                            k++;
                        }
                        if (k >= resLen || !is_format_type(langResult[k])) {
                            // 无效格式，原样输出
                            secBuf[secPos++] = langResult[j];
                            continue;
                        }
                        char fmtSpec[32];
                        size_t specLen = k - fmtStart + 1;
                        if (specLen >= sizeof(fmtSpec)) specLen = sizeof(fmtSpec) - 1;
                        strncpy(fmtSpec, langResult + fmtStart, specLen);
                        fmtSpec[specLen] = '\0';

                        char typeChar = langResult[k];
                        // 检查参数是否足够
                        if (paramIdx > numParams) {
                            // 无参数，输出原格式
                            strncpy(secBuf + secPos, fmtSpec, sizeof(secBuf) - secPos - 1);
                            secPos += strlen(fmtSpec);
                            j = k;
                            continue;
                        }
                        cell currentParam = params[paramIdx];
                        paramIdx++;

                        switch (typeChar) {
                            case 's': {
                                cell *strAddr;
                                amx_GetAddr(amx, currentParam, &strAddr);
                                char strVal[1024];
                                amx_GetString(strVal, strAddr, 0, sizeof(strVal));
                                int n = snprintf(secBuf + secPos, sizeof(secBuf) - secPos, fmtSpec, strVal);
                                if (n > 0) secPos += (size_t)n;
                                break;
                            }
                            case 'd': case 'i': case 'c': {
                                int val = (typeChar == 'c') ? ((int)currentParam & 0xFF) : (int)currentParam;
                                int n = snprintf(secBuf + secPos, sizeof(secBuf) - secPos, fmtSpec, val);
                                if (n > 0) secPos += (size_t)n;
                                break;
                            }
                            case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A': {
                                int n = snprintf(secBuf + secPos, sizeof(secBuf) - secPos, fmtSpec, amx_ctof(currentParam));
                                if (n > 0) secPos += (size_t)n;
                                break;
                            }
                            case 'u': case 'o': case 'x': case 'X': case 'p': {
                                int n = snprintf(secBuf + secPos, sizeof(secBuf) - secPos, fmtSpec, (unsigned int)currentParam);
                                if (n > 0) secPos += (size_t)n;
                                break;
                            }
                            default:
                                strncpy(secBuf + secPos, fmtSpec, sizeof(secBuf) - secPos - 1);
                                secPos += strlen(fmtSpec);
                                break;
                        }
                        j = k; // 跳到格式类型字符位置，循环会再 ++
                    } else {
                        secBuf[secPos++] = langResult[j];
                    }
                }
                secBuf[secPos] = '\0';
                // 追加到主输出
                size_t slen = strlen(secBuf);
                if (slen > outputSize - outPos - 1)
                    slen = outputSize - outPos - 1;
                memcpy(output + outPos, secBuf, slen);
                outPos += slen;
                i++; // 跳过 'L'
                continue;
            }

            // 普通格式说明符（非 %L）
            // 提取完整格式说明符
            size_t fmtStart = i;
            size_t k = i + 1;
            while (k < len && (is_flag_char(fmtBuffer[k]) ||
                   (fmtBuffer[k] >= '0' && fmtBuffer[k] <= '9') ||
                   fmtBuffer[k] == '*' || fmtBuffer[k] == '.' ||
                   is_length_modifier(fmtBuffer[k]))) {
                k++;
            }
            if (k >= len || !is_format_type(fmtBuffer[k])) {
                // 无效格式，原样输出
                output[outPos++] = '%';
                if (i + 1 < len) output[outPos++] = fmtBuffer[++i];
                continue;
            }
            char fmtSpec[64];
            size_t specLen = k - fmtStart + 1;
            if (specLen >= sizeof(fmtSpec)) specLen = sizeof(fmtSpec) - 1;
            strncpy(fmtSpec, fmtBuffer + fmtStart, specLen);
            fmtSpec[specLen] = '\0';

            char typeChar = fmtBuffer[k];
            if (paramIdx > numParams) {
                // 参数不足，直接输出格式字符串
                strncpy(output + outPos, fmtSpec, outputSize - outPos - 1);
                outPos += strlen(fmtSpec);
                i = k;
                continue;
            }
            cell currentParam = params[paramIdx];
            paramIdx++;

            switch (typeChar) {
                case 's': {
                    cell *strAddr;
                    amx_GetAddr(amx, currentParam, &strAddr);
                    char strVal[1024];
                    amx_GetString(strVal, strAddr, 0, sizeof(strVal));
                    int n = snprintf(output + outPos, outputSize - outPos, fmtSpec, strVal);
                    if (n > 0) outPos += (size_t)n;
                    break;
                }
                case 'd': case 'i': case 'c': {
                    int val = (typeChar == 'c') ? ((int)currentParam & 0xFF) : (int)currentParam;
                    int n = snprintf(output + outPos, outputSize - outPos, fmtSpec, val);
                    if (n > 0) outPos += (size_t)n;
                    break;
                }
                case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A': {
                    int n = snprintf(output + outPos, outputSize - outPos, fmtSpec, amx_ctof(currentParam));
                    if (n > 0) outPos += (size_t)n;
                    break;
                }
                case 'u': case 'o': case 'x': case 'X': case 'p': {
                    int n = snprintf(output + outPos, outputSize - outPos, fmtSpec, (unsigned int)currentParam);
                    if (n > 0) outPos += (size_t)n;
                    break;
                }
                default:
                    strncpy(output + outPos, fmtSpec, outputSize - outPos - 1);
                    outPos += strlen(fmtSpec);
                    break;
            }
            i = k; // 跳到格式类型字符位置，循环会再 ++
        } else {
            output[outPos++] = fmtBuffer[i];
        }
    }
    output[outPos] = '\0';
}

cell AMX_NATIVE_CALL amxx_server_print(AMX *amx, cell *params)
{
    char output[2048];
    amxx_format_string(amx, params, 1, output, sizeof(output));
    SERVER_PRINT(output);
    printf("%s\n", output);
    return 0;
}

// AMXX 打印类型与 CS 引擎宏值完全一致：
//   print_notify=1 → HUD_PRINTNOTIFY=1
//   print_console=2 → HUD_PRINTCONSOLE=2
//   print_chat=3 → HUD_PRINTTALK=3
//   print_center=4 → HUD_PRINTCENTER=4
// 因此直接使用 raw type 值即可，无需转换。

static void UTIL_ClientPrint(edict_t *pEntity, int cs_dest, char *msg)
{
    // Xash3D 颜色代码转换：\x01→^1, \x03→^3, \x04→^4
    char converted[512];
    size_t j = 0;
    for (size_t i = 0; msg[i] && j < sizeof(converted) - 3; i++)
    {
        unsigned char c = (unsigned char)msg[i];
        if (c == 0x01) { converted[j++] = '^'; converted[j++] = '1'; }
        else if (c == 0x03) { converted[j++] = '^'; converted[j++] = '3'; }
        else if (c == 0x04) { converted[j++] = '^'; converted[j++] = '4'; }
        else { converted[j++] = c; }
    }
    converted[j] = '\0';

    const int MAX_MSG_LEN = 187;
    if (j > MAX_MSG_LEN)
        converted[MAX_MSG_LEN] = '\0';

    switch (cs_dest)
    {
        case HUD_PRINTTALK:
        {
            extern int gmsgSayText;
            if (gmsgSayText <= 0)
                return;

            // Xash3D CS 客户端要求 source 必须 > 0 才能显示在聊天框
            int source = 1;
            if (pEntity) {
                source = ENTINDEX(pEntity);
            } else {
                // 找一个在线的玩家作为 source
                for (int i = 1; i <= gpGlobals->maxClients; i++) {
                    edict_t *pEdict = INDEXENT(i);
                    if (pEdict && !pEdict->free) {
                        CBasePlayer *pSrcPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
                        if (pSrcPlayer && pSrcPlayer->IsNetClient()) {
                            source = i;
                            break;
                        }
                    }
                }
            }

            if (pEntity)
                MESSAGE_BEGIN(MSG_ONE, gmsgSayText, nullptr, pEntity);
            else
                MESSAGE_BEGIN(MSG_ALL, gmsgSayText);

            WRITE_BYTE(source);
            WRITE_STRING(converted);
            WRITE_BYTE(1);
            MESSAGE_END();
            break;
        }

        case HUD_PRINTCENTER:
        {
            if (pEntity)
                MESSAGE_BEGIN(MSG_ONE, SVC_TEMPENTITY, nullptr, pEntity);
            else
                MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);

            WRITE_BYTE(TE_TEXTMESSAGE);
            WRITE_BYTE(1);                                              // channel
            WRITE_SHORT(FixedSigned16(-1.0f, (1 << 13)));              // x center
            WRITE_SHORT(FixedSigned16(0.3f, (1 << 13)));               // y
            WRITE_BYTE(0);                                              // effect 0=fade
            WRITE_BYTE(255); WRITE_BYTE(255); WRITE_BYTE(255); WRITE_BYTE(200);  // primary color
            WRITE_BYTE(255); WRITE_BYTE(255); WRITE_BYTE(255); WRITE_BYTE(200);  // secondary color
            WRITE_SHORT(FixedUnsigned16(0.01f, (1 << 8)));              // fadeinTime
            WRITE_SHORT(FixedUnsigned16(0.1f, (1 << 8)));               // fadeoutTime
            WRITE_SHORT(FixedUnsigned16(6.0f, (1 << 8)));               // holdTime
            WRITE_STRING(converted);
            MESSAGE_END();
            break;
        }

        case HUD_PRINTCONSOLE:
        case HUD_PRINTNOTIFY:
        {
            char fullMsg[512];
            int n = snprintf(fullMsg, sizeof(fullMsg), "%s\n", converted);
            if (n < 0) break;
            if (pEntity)
            {
                g_engfuncs.pfnClientPrintf(pEntity, print_console, fullMsg);
            }
            else
            {
                for (int i = 1; i <= gpGlobals->maxClients; i++)
                {
                    edict_t *pEdict = INDEXENT(i);
                    if (pEdict && !pEdict->free)
                        g_engfuncs.pfnClientPrintf(pEdict, print_console, fullMsg);
                }
            }
            break;
        }
    }
}

// client_print(id, type, fmt, ...)
cell AMX_NATIVE_CALL amxx_client_print(AMX *amx, cell *params)
{
    int id = (int)params[1];
    int cs_type = (int)params[2];

    char message[1024];
    amxx_format_string(amx, params, 3, message, sizeof(message));
    int len = (int)strlen(message);

    // CS 引擎中心消息要求 \n 转 \r
    if (cs_type == HUD_PRINTCENTER) {
        for (int j = 0; j < len; j++)
            if (message[j] == '\n')
                message[j] = '\r';
    }

    // 控制台/notify 消息限制 + 双换行
    if (cs_type == HUD_PRINTNOTIFY || cs_type == HUD_PRINTCONSOLE) {
        if (len > 125) len = 125;
        message[len++] = '\n';
        message[len++] = '\n';
    } else {
        message[len++] = '\n';
    }
    message[len] = '\0';

    if (id == 0) {
        UTIL_ClientPrint(nullptr, cs_type, message);
    } else {
        if (id < 1 || id > gpGlobals->maxClients)
            return 0;
        edict_t *pEdict = INDEXENT(id);
        if (!pEdict || pEdict->free)
            return 0;
        UTIL_ClientPrint(pEdict, cs_type, message);
    }

    return len;
}

cell AMX_NATIVE_CALL amxx_get_user_name(AMX *amx, cell *params)
{
    int index = (int)params[1];
    int maxlen = (int)params[3];
    const char *name;

    // 参考原版 AMXX：当索引无效时返回 hostname
    if (index < 1 || index > gpGlobals->maxClients) {
        name = CVAR_GET_STRING("hostname");
    } else {
        edict_t *pEdict = INDEXENT(index);
        if (!pEdict || pEdict->free) {
            name = CVAR_GET_STRING("hostname");
        } else {
            name = STRING(pEdict->v.netname);
        }
    }

    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    return amx_SetString(dest, name, 0, 0, maxlen);
}

cell AMX_NATIVE_CALL amxx_get_playersnum(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    int count = 0;
    for (int i = 1; i <= gpGlobals->maxClients; i++) {
        edict_t *pEdict = INDEXENT(i);
        if (!pEdict || pEdict->free)
            continue;
        CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
        if (!pPlayer || !pPlayer->IsNetClient())
            continue;
        count++;
    }
    return count;
}

// 使用 gpGlobals->maxClients 获取最大玩家数
cell AMX_NATIVE_CALL amxx_get_maxplayers(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return gpGlobals->maxClients;
}

cell AMX_NATIVE_CALL amxx_get_players(AMX *amx, cell *params)
{
    int numParams = (int)(params[0] / sizeof(cell));

    char flags_str[32] = "";
    if (numParams >= 3 && params[3] != 0) {
        cell *flags_addr;
        amx_GetAddr(amx, params[3], &flags_addr);
        amx_GetString(flags_str, flags_addr, 0, sizeof(flags_str));
    }

    // 转为位掩码（与原版 UTIL_ReadFlags 一致：'a'=bit0, 'b'=bit1, ...）
    int flags = 0;
    for (int i = 0; flags_str[i]; i++) {
        char c = flags_str[i];
        if (c >= 'a' && c <= 'z')
            flags |= (1 << (c - 'a'));
    }

    // 原版 flag 语义（amxmodx.cpp get_players）：
    // a=只含存活, b=只含死亡, c=排除 bot, d=排除人类, e=按 team 过滤,
    // f=按 name 子串匹配, g=大小写不敏感, h=排除 HLTV, i=包含未连接, j=排除无 team

    // 解析第 4 参数（team/name 匹配字符串）
    const char *matchStr = nullptr;
    char matchBuf[256] = "";
    if (flags & 48) { // 'e'(16) 或 'f'(32)
        if (numParams >= 4 && params[4] != 0) {
            cell *match_addr;
            amx_GetAddr(amx, params[4], &match_addr);
            amx_GetString(matchBuf, match_addr, 0, sizeof(matchBuf));
            matchStr = matchBuf;
        }
    }

    // 若 'e' 按队伍过滤，将 team 名称解析为 teamId
    int matchTeam = -1;
    if ((flags & 16) && matchStr) { // 'e'
        // CS 队伍名映射
        if (!_stricmp(matchStr, "TERRORIST") || !_stricmp(matchStr, "T"))
            matchTeam = 1;
        else if (!_stricmp(matchStr, "CT") || !_stricmp(matchStr, "COUNTER-TERRORIST"))
            matchTeam = 2;
        else if (!_stricmp(matchStr, "SPECTATOR") || !_stricmp(matchStr, "SPEC"))
            matchTeam = 3;
        else if ((flags & 64)) // 'g' 大小写不敏感 - 尝试数字
            matchTeam = atoi(matchStr);
        else
            matchTeam = atoi(matchStr);
    }

    cell *players_out;
    amx_GetAddr(amx, params[1], &players_out);
    cell *count_out;
    amx_GetAddr(amx, params[2], &count_out);

    int count = 0;
    for (int i = 1; i <= gpGlobals->maxClients; i++) {
        edict_t *pEdict = INDEXENT(i);
        if (!pEdict || pEdict->free)
            continue;

        CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
        if (!pPlayer)
            continue;

        bool isBot = (pEdict->v.flags & FL_FAKECLIENT) ? true : false;
        bool isHLTV = (pEdict->v.flags & FL_PROXY) ? true : false;
        bool isConnected = pPlayer->IsNetClient() != 0;
        bool alive = pPlayer->IsAlive();
        int teamId = (int)pEdict->v.team;
        const char *playerName = STRING(pEdict->v.netname);

        // 'i': 包含未完全连接的玩家；若未设 'i' 则跳过未连接
        if (!isConnected && !(flags & 256)) // 'i'=256
            continue;

        // a/b: 存活/死亡过滤
        // 原版: pPlayer->IsAlive() ? (flags & 2) : (flags & 1) → skip
        if (alive ? (flags & 2) : (flags & 1))
            continue;

        // c/d: bot/人类过滤
        // 原版: pPlayer->IsBot() ? (flags & 4) : (flags & 8) → skip
        if (isBot ? (flags & 4) : (flags & 8))
            continue;

        // e: team 过滤
        if ((flags & 16) && teamId != matchTeam)
            continue;

        // h: 排除 HLTV
        if ((flags & 128) && isHLTV)
            continue;

        // j: 排除无 team 的玩家
        if ((flags & 512) && teamId == 0) // 'j'=512
            continue;

        // f: name 子串匹配
        if ((flags & 32) && matchStr) {
            if (flags & 64) {
                // 'g' 大小写不敏感子串搜索
                if (!playerName)
                    continue;
                bool found = false;
                size_t nlen = strlen(matchStr);
                size_t hlen = strlen(playerName);
                if (nlen <= hlen) {
                    for (size_t k = 0; k + nlen <= hlen; k++) {
                        if (_strnicmp(playerName + k, matchStr, nlen) == 0) {
                            found = true;
                            break;
                        }
                    }
                }
                if (!found)
                    continue;
            } else {
                if (!playerName || !strstr(playerName, matchStr))
                    continue;
            }
        }

        players_out[count++] = (cell)i;
    }

    *count_out = count;
    return 1;
}

cell AMX_NATIVE_CALL amxx_is_user_alive(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];

    if (index < 1 || index > gpGlobals->maxClients)
        return 0;

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return 0;

    CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
    if (!pPlayer)
        return 0;

    return pPlayer->IsAlive() ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_set_task(AMX *amx, cell *params)
{
    // 参考原版 AMXX 的 set_task 实现
    // 参数: params[1]=time, params[2]=function, params[3]=id, params[4]=parameter, params[5]=len, params[6]=flags, params[7]=repeat

    AMXXRuntime &runtime = AMXXRuntime::GetInstance();
    
    // 查找插件（AMXXRuntime 没有 FindPluginByAMX 方法，需要手动遍历）
    AMXXPlugin *plugin = nullptr;
    for (size_t i = 0; i < runtime.GetPlugins().size(); i++) {
        if (runtime.GetPlugins()[i]->GetAMX() == amx) {
            plugin = runtime.GetPlugins()[i];
            break;
        }
    }
    
    if (!plugin) {
        AMXX_LOG("[set_task] Failed to find plugin for AMX=%p", amx);
        return 0;
    }

    // 获取函数名
    char funcname[256];
    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    amx_GetString(funcname, addr, 0, sizeof(funcname));

    // 查找函数索引
    int funcIndex = plugin->FindPublic(funcname);
    if (funcIndex < 0) {
        AMXX_LOG("[set_task] Function '%s' not found in plugin '%s'", funcname, plugin->GetName());
        return 0;
    }

    // 获取时间参数
    float base = amx_ctof(params[1]);
    if (base < 0.1f) {
        base = 0.1f;
    }

    // 获取任务ID
    cell taskId = params[3];

    // 获取标志字符串并转换为整数标志
    char flags_str[32] = "";
    cell *flags_addr;
    amx_GetAddr(amx, params[6], &flags_addr);
    amx_GetString(flags_str, flags_addr, 0, sizeof(flags_str));

    // 参考原版 UTIL_ReadFlags: flags |= (1<<(*c++ - 'a'))
    int flags = 0;
    for (int i = 0; flags_str[i]; i++) {
        flags |= (1 << (flags_str[i] - 'a'));
    }

    // 获取参数长度和参数数组
    int paramLen = (int)params[5];
    cell *paramAddr = NULL;
    if (paramLen > 0) {
        amx_GetAddr(amx, params[4], &paramAddr);
    }

    // 获取重复次数
    int repeat = (int)params[7];

    // 注册任务（参考原版 AMXX）
    runtime.GetTaskManager().CreateTask(plugin, amx, funcIndex, flags, taskId, base, paramLen, paramAddr, repeat);
    AMXX_LOG_DBG("[set_task] Created task %d (plugin='%s', func='%s', base=%.2f, flags='%s'/%d, repeat=%d)",
        taskId, plugin->GetName(), funcname, base, flags_str, flags, repeat);

    return 1;
}
cell AMX_NATIVE_CALL amxx_remove_task(AMX *amx, cell *params)
{
    int taskId = (int)params[1];
    AMXXRuntime &runtime = AMXXRuntime::GetInstance();
    // P2: 返回移除的任务数量 (原版 remove_task(id, outside=0) 语义)
    return runtime.GetTaskManager().RemoveTask(taskId, params[2] ? nullptr : amx);
}

cell AMX_NATIVE_CALL amxx_task_exists(AMX *amx, cell *params)
{
    int taskId = (int)params[1];
    AMXXRuntime &runtime = AMXXRuntime::GetInstance();
    return runtime.GetTaskManager().TaskExists(taskId, params[2] ? nullptr : amx) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_change_task(AMX *amx, cell *params)
{
    int taskId = (int)params[1];
    float newBase = amx_ctof(params[2]);
    AMXXRuntime &runtime = AMXXRuntime::GetInstance();
    return runtime.GetTaskManager().ChangeTask(taskId, newBase, params[3] ? nullptr : amx) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_get_gametime(AMX *amx, cell *params)
{
    (void)amx;
    (void)params;
    return amx_ftoc(gpGlobals->time);
}

static hudtextparms_t g_hudset = {};

// P2-2: 每玩家自动 HUD channel 分配 (原版 AMXX 行为: channel=-1 时循环 0..3)
static int g_nextHudChannel[33] = {0}; // index 1..maxClients

static int GetNextHudChannel(int playerIndex)
{
    if (playerIndex < 1 || playerIndex > 32) return 0;
    int ch = g_nextHudChannel[playerIndex]++;
    if (g_nextHudChannel[playerIndex] > 3) g_nextHudChannel[playerIndex] = 0;
    return ch;
}

cell AMX_NATIVE_CALL amxx_set_hudmessage(AMX *amx, cell *params)
{
    cell num_params = params[0] / sizeof(cell);

    // 参考原版 AMXX：先处理 color2 和 a1
    if (num_params >= 13)
    {
        cell *color2;
        amx_GetAddr(amx, params[13], &color2);

        g_hudset.a1 = static_cast<byte>(params[12]);
        g_hudset.a2 = static_cast<byte>(color2[3]);
        g_hudset.r2 = static_cast<byte>(color2[0]);
        g_hudset.g2 = static_cast<byte>(color2[1]);
        g_hudset.b2 = static_cast<byte>(color2[2]);
    }
    else
    {
        // 参考原版 AMXX 默认值
        g_hudset.a1 = 0;
        g_hudset.a2 = 0;
        g_hudset.r2 = 255;
        g_hudset.g2 = 255;
        g_hudset.b2 = 250;
    }

    // 再设置其他参数
    g_hudset.r1 = static_cast<byte>(params[1]);
    g_hudset.g1 = static_cast<byte>(params[2]);
    g_hudset.b1 = static_cast<byte>(params[3]);
    g_hudset.x = amx_ctof(params[4]);
    g_hudset.y = amx_ctof(params[5]);
    g_hudset.effect = (int)params[6];
    g_hudset.fxTime = amx_ctof(params[7]);
    g_hudset.holdTime = amx_ctof(params[8]);
    g_hudset.fadeinTime = amx_ctof(params[9]);
    g_hudset.fadeoutTime = amx_ctof(params[10]);
    g_hudset.channel = (int)params[11];

    // P2-2: 不再强制将 channel<0 归一化为 4。
    // 原版 AMXX: channel=-1 表示"自动分配"，在 show_hudmessage 中为每玩家循环 0..3。
    // show_hudmessage 负责将 -1 解析为实际 channel。

    return 1;
}

// 参考原版 AMXX 的 UTIL_SplitHudMessage 实现
// 处理 HUD 消息换行，限制每行最多69个字符
char* UTIL_SplitHudMessage(const char *src)
{
    static char message[512];
    short b = 0, d = 0, e = 0, c = -1;

    while (src[d] && e < 480)
    {
        if (src[d] == ' ')
        {
            c = e;
        }
        else if (src[d] == '\n')
        {
            c = -1;
            b = 0;
        }

        message[e++] = src[d++];

        if (++b == 69)
        {
            if (c == -1)
            {
                message[e++] = '\n';
                b = 0;
            } else {
                message[c] = '\n';
                b = e - c - 1;
                c = -1;
            }
        }
    }

    message[e] = 0;
    return message;
}

cell AMX_NATIVE_CALL amxx_show_hudmessage(AMX *amx, cell *params)
{
    int id = (int)params[1];

    char message[1024];
    amxx_format_string(amx, params, 2, message, sizeof(message));

    // 使用 UTIL_SplitHudMessage 处理消息换行 (参考原版 AMXX)
    char *splitMessage = UTIL_SplitHudMessage(message);

    // 透传 set_hudmessage 设置的参数给 TE_TEXTMESSAGE。
    // 仅做最小修正（不覆盖插件意图）：
    //   1. a1=0 → 255（透明文字不可见，这是通用问题）
    //      同时若 r1=g1=b1=0（插件默认未设颜色），则 fallback 到白 (255,255,255)
    //   2. y < 0.25 → 0.25（Xash3D 引擎 bug：TE_TEXTMESSAGE 在此范围内不渲染）
    // 以下参数由插件全权控制（不再强制覆盖）：
    //   channel、x、effect、r1/g1/b1（有颜色时）、r2/g2/b2/a2、
    //   fadeinTime/fadeoutTime/holdTime/fxTime
    int maxClients = gpGlobals ? gpGlobals->maxClients : 32;
    if (id == 0) {
        for (int i = 1; i <= maxClients; i++) {
            CBaseEntity *pEntity = UTIL_PlayerByIndex(i);
            if (!pEntity || !pEntity->IsNetClient())
                continue;

            hudtextparms_t hp = g_hudset;
            // 最小修正，见上方注释
            if (hp.a1 == 0) { hp.a1 = 255; if (hp.r1 == 0 && hp.g1 == 0 && hp.b1 == 0) { hp.r1 = 255; hp.g1 = 255; hp.b1 = 255; } }
            // P2-2: channel=-1 时自动分配每玩家的下一个 channel (0..3 循环)
            if (hp.channel < 0)
                hp.channel = GetNextHudChannel(i);
            else
                hp.channel = abs(hp.channel % 5);

            MESSAGE_BEGIN(MSG_ONE, SVC_TEMPENTITY, nullptr, pEntity->edict());
                WRITE_BYTE(TE_TEXTMESSAGE);
                WRITE_BYTE(hp.channel & 0xFF);
                WRITE_SHORT(FixedSigned16(hp.x, (1<<13)));
                WRITE_SHORT(FixedSigned16(hp.y, (1<<13)));
                WRITE_BYTE(hp.effect);
                WRITE_BYTE(hp.r1); WRITE_BYTE(hp.g1); WRITE_BYTE(hp.b1); WRITE_BYTE(hp.a1);
                WRITE_BYTE(hp.r2); WRITE_BYTE(hp.g2); WRITE_BYTE(hp.b2); WRITE_BYTE(hp.a2);
                WRITE_SHORT(FixedUnsigned16(hp.fadeinTime, (1<<8)));
                WRITE_SHORT(FixedUnsigned16(hp.fadeoutTime, (1<<8)));
                WRITE_SHORT(FixedUnsigned16(hp.holdTime, (1<<8)));
                if (hp.effect == 2)
                    WRITE_SHORT(FixedUnsigned16(hp.fxTime, (1<<8)));
                WRITE_STRING(splitMessage);
            MESSAGE_END();
        }
    } else {
        if (id < 1 || id > maxClients)
            return 0;

        CBaseEntity *pEntity = UTIL_PlayerByIndex(id);
        if (!pEntity || !pEntity->IsNetClient())
            return 0;

        hudtextparms_t hp = g_hudset;
        // 最小修正，见上方注释
        if (hp.a1 == 0) { hp.a1 = 255; if (hp.r1 == 0 && hp.g1 == 0 && hp.b1 == 0) { hp.r1 = 255; hp.g1 = 255; hp.b1 = 255; } }
        // P2-2: channel=-1 时自动分配每玩家的下一个 channel (0..3 循环)
        if (hp.channel < 0)
            hp.channel = GetNextHudChannel(id);
        else
            hp.channel = abs(hp.channel % 5);

        MESSAGE_BEGIN(MSG_ONE, SVC_TEMPENTITY, nullptr, pEntity->edict());
            WRITE_BYTE(TE_TEXTMESSAGE);
            WRITE_BYTE(hp.channel & 0xFF);
            WRITE_SHORT(FixedSigned16(hp.x, (1<<13)));
            WRITE_SHORT(FixedSigned16(hp.y, (1<<13)));
            WRITE_BYTE(hp.effect);
            WRITE_BYTE(hp.r1); WRITE_BYTE(hp.g1); WRITE_BYTE(hp.b1); WRITE_BYTE(hp.a1);
            WRITE_BYTE(hp.r2); WRITE_BYTE(hp.g2); WRITE_BYTE(hp.b2); WRITE_BYTE(hp.a2);
            WRITE_SHORT(FixedUnsigned16(hp.fadeinTime, (1<<8)));
            WRITE_SHORT(FixedUnsigned16(hp.fadeoutTime, (1<<8)));
            WRITE_SHORT(FixedUnsigned16(hp.holdTime, (1<<8)));
            if (hp.effect == 2)
                WRITE_SHORT(FixedUnsigned16(hp.fxTime, (1<<8)));
            WRITE_STRING(splitMessage);
            MESSAGE_END();
    }

    return 1;
}

// P2-2: next_hudchannel - 返回指定玩家的下一个 HUD channel (0..3 循环)
cell AMX_NATIVE_CALL amxx_next_hudchannel(AMX *amx, cell *params)
{
    int index = (int)params[1];
    int maxClients = gpGlobals ? gpGlobals->maxClients : 32;
    if (index < 1 || index > maxClients)
        return 0;
    return (cell)GetNextHudChannel(index);
}

cell AMX_NATIVE_CALL amxx_console_print(AMX *amx, cell *params)
{
    int id = (int)params[1];

    char message[1024];
    amxx_format_string(amx, params, 2, message, sizeof(message));
    int len = (int)strlen(message);

    if (id == 0) {
        for (int i = 1; i <= gpGlobals->maxClients; i++) {
            edict_t *pEdict = INDEXENT(i);
            if (!pEdict || pEdict->free)
                continue;
            CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
            if (!pPlayer || !pPlayer->IsNetClient())
                continue;
            UTIL_ClientPrint(pEdict, HUD_PRINTCONSOLE, message);
        }
    } else if (id < 1 || id > gpGlobals->maxClients) {
        // 原版 AMXX 行为：无效玩家索引输出到服务端控制台
        SERVER_PRINT(message);
        SERVER_PRINT("\n");
    } else {
        edict_t *pEdict = INDEXENT(id);
        if (!pEdict || pEdict->free)
            return 0;
        CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
        if (!pPlayer || !pPlayer->IsNetClient())
            return 0;
        UTIL_ClientPrint(pEdict, HUD_PRINTCONSOLE, message);
    }

    return len;
}

cell AMX_NATIVE_CALL amxx_server_cmd(AMX *amx, cell *params)
{
    char cmd[1024];
    amxx_format_string(amx, params, 1, cmd, sizeof(cmd));
    int len = (int)strlen(cmd);
    cmd[len++] = '\n';
    cmd[len] = '\0';
    SERVER_COMMAND(cmd);
    return len;
}

cell AMX_NATIVE_CALL amxx_client_cmd(AMX *amx, cell *params)
{
    int id = (int)params[1];

    char cmd[512];
    amxx_format_string(amx, params, 2, cmd, sizeof(cmd));
    int len = (int)strlen(cmd);
    cmd[len++] = '\n';
    cmd[len] = '\0';

    if (id == 0) {
        for (int i = 1; i <= gpGlobals->maxClients; i++) {
            edict_t *pEdict = INDEXENT(i);
            if (!pEdict || pEdict->free)
                continue;
            if (!(pEdict->v.flags & FL_FAKECLIENT) && (pEdict->v.flags & FL_CLIENT))
                CLIENT_COMMAND(pEdict, "%s", cmd);
        }
    } else {
        if (id < 1 || id > gpGlobals->maxClients)
            return 0;
        edict_t *pEdict = INDEXENT(id);
        if (!pEdict || pEdict->free)
            return 0;
        if (!(pEdict->v.flags & FL_FAKECLIENT) && (pEdict->v.flags & FL_CLIENT))
            CLIENT_COMMAND(pEdict, "%s", cmd);
    }

    return len;
}

cell AMX_NATIVE_CALL amxx_random_num(AMX *amx, cell *params)
{
    (void)amx;
    return RANDOM_LONG((int)params[1], (int)params[2]);
}

cell AMX_NATIVE_CALL amxx_random_float(AMX *amx, cell *params)
{
    (void)amx;
    float val = RANDOM_FLOAT(amx_ctof(params[1]), amx_ctof(params[2]));
    return amx_ftoc(val);
}

cell AMX_NATIVE_CALL amxx_strlen(AMX *amx, cell *params)
{
    cell *cptr;
    int result = amx_GetAddr(amx, params[1], &cptr);
    if (result != AMX_ERR_NONE || cptr == NULL) {
        return 0;
    }
    cell len = 0;
    while (*cptr) {
        cptr++;
        len++;
    }
    return len;
}

cell AMX_NATIVE_CALL amxx_equal(AMX *amx, cell *params)
{
    cell *a, *b;
    amx_GetAddr(amx, params[1], &a);
    amx_GetAddr(amx, params[2], &b);
    int c = (int)params[3];

    if (c) {
        while (--c && *a && (*a == *b))
            ++a, ++b;
        return (*a == *b) ? 1 : 0;
    }

    while (*a == *b && *b)
        ++a, ++b;

    return (*a == *b) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_contain(AMX *amx, cell *params)
{
    cell *a, *b, *start;
    amx_GetAddr(amx, params[1], &a);
    amx_GetAddr(amx, params[2], &b);
    start = a;
    cell *sub = b;

    while (*a) {
        if (*a == *b) {
            a++;
            b++;
            if (!*b)
                return (int)(a - start - (b - sub));
        } else {
            a = ++start;
            b = sub;
        }
    }

    return -1;
}

cell AMX_NATIVE_CALL amxx_remove_quotes(AMX *amx, cell *params)
{
    cell *text;
    amx_GetAddr(amx, params[1], &text);

    if (*text == '\"') {
        cell *temp = text;
        while (*temp)
            temp++;
        temp--;
        if (*temp == '\"')
            *temp = 0;
        memmove(text, text + 1, (temp - text) * sizeof(cell));
    }

    return 1;
}

cell AMX_NATIVE_CALL amxx_read_argc(AMX *amx, cell *params)
{
    (void)amx;
    (void)params;
    // 使用保存的命令参数（防止异步执行时参数被覆盖）
    AMXXAdminSystem &admin = AMXXAdminSystem::GetInstance();
    int savedArgc = admin.GetSavedArgc();
    if (savedArgc > 0) {
        return savedArgc;
    }
    return CMD_ARGC();
}

cell AMX_NATIVE_CALL amxx_read_argv(AMX *amx, cell *params)
{
    int argc = (int)params[1];
    int maxlen = (int)params[3];
    
    // 使用保存的命令参数（防止异步执行时参数被覆盖）
    AMXXAdminSystem &admin = AMXXAdminSystem::GetInstance();
    const char *value = admin.GetSavedArgv(argc);
    
    if (!value) {
        value = CMD_ARGV(argc);
        if (!value) value = "";
    }

    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    return amx_SetString(dest, value, 0, 0, maxlen);
}

cell AMX_NATIVE_CALL amxx_read_argv_int(AMX *amx, cell *params)
{
    (void)amx;
    int argc = (int)params[1];
    // 先检查保存的参数，后回退到引擎 CMD_ARGV
    AMXXAdminSystem &admin = AMXXAdminSystem::GetInstance();
    const char *value = admin.GetSavedArgv(argc);
    if (!value) {
        value = CMD_ARGV(argc);
    }
    if (!value) return 0;
    return atoi(value);
}

cell AMX_NATIVE_CALL amxx_read_argv_float(AMX *amx, cell *params)
{
    (void)amx;
    int argc = (int)params[1];
    AMXXAdminSystem &admin = AMXXAdminSystem::GetInstance();
    const char *value = admin.GetSavedArgv(argc);
    if (!value) {
        value = CMD_ARGV(argc);
    }
    if (!value) return 0;
    float val = atof(value);
    return amx_ftoc(val);
}

cell AMX_NATIVE_CALL amxx_read_args(AMX *amx, cell *params)
{
    int maxlen = (int)params[2];

    AMXXAdminSystem &admin = AMXXAdminSystem::GetInstance();
    int savedArgc = admin.GetSavedArgc();

    char buffer[4096];
    buffer[0] = '\0';

    if (savedArgc > 1) {
        // 使用保存的 argv 拼接 (从 argv[1] 开始，跳过命令名)
        size_t pos = 0;
        for (int i = 1; i < savedArgc && pos < sizeof(buffer) - 1; i++) {
            const char *a = admin.GetSavedArgv(i);
            if (!a) continue;
            if (i > 1 && pos < sizeof(buffer) - 1) {
                buffer[pos++] = ' ';
            }
            size_t alen = strlen(a);
            size_t space = sizeof(buffer) - 1 - pos;
            if (alen > space) alen = space;
            memcpy(buffer + pos, a, alen);
            pos += alen;
            buffer[pos] = '\0';
        }
    } else {
        // 没有保存的参数，使用引擎 CMD_ARGS()
        const char *value = CMD_ARGS();
        if (!value) value = "";
        strncpy(buffer, value, sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
    }

    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    return amx_SetString(dest, buffer, 0, 0, maxlen);
}

cell AMX_NATIVE_CALL amxx_log_amx(AMX *amx, cell *params)
{
    char msg[1024];
    amxx_format_string(amx, params, 1, msg, sizeof(msg));
    ALERT(at_logged, "[AMXX] %s\n", msg);
    return 0;
}

cell AMX_NATIVE_CALL amxx_log_message(AMX *amx, cell *params)
{
    char msg[1024];
    amxx_format_string(amx, params, 1, msg, sizeof(msg));
    ALERT(at_logged, "%s\n", msg);
    return 0;
}

cell AMX_NATIVE_CALL amxx_get_time(AMX *amx, cell *params)
{
    cell *fmt_addr;
    amx_GetAddr(amx, params[1], &fmt_addr);
    char fmt[256];
    amx_GetString(fmt, fmt_addr, 0, sizeof(fmt));

    time_t td = time(nullptr);
    struct tm *lt = localtime(&td);

    char date[512];
    strftime(date, sizeof(date) - 1, fmt, lt);
    date[sizeof(date) - 1] = '\0';

    int maxlen = (int)params[3];
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    return amx_SetString(dest, date, 0, 0, maxlen);
}

// ========== User Message ID<->Name 映射 ==========
// 原版 AMXX 通过 metamod 的 GET_USER_MSG_ID 查询消息 ID，
// 引擎本身未暴露 pfnGetUserMsgName，因此本实现维护运行时映射表：
//   1. 在 OnServerActivate 时遍历所有已知 CS 1.6 消息名，调用 REG_USER_MSG(name, 0)
//      查询其动态分配的 ID（REG_USER_MSG 对已注册消息返回现有 ID，不会重复注册）
//   2. get_user_msgid 调用 REG_USER_MSG 后同步更新映射表
//   3. get_user_msgname 通过映射表反查名称
// 这样可正确处理引擎动态分配 ID、mod 注册自定义消息等场景。

#include <string>

static std::map<int, std::string> g_userMsgIdToName;
static bool g_userMsgMapInitialized = false;

// 已知的 CS 1.6 标准用户消息名 (源自 ReGameDLL client.cpp 的 REG_USER_MSG 调用)
static const char *const g_knownCSMsgNames[] = {
    "CurWeapon", "Geiger", "Flashlight", "FlashBat", "Health",
    "Damage", "Battery", "Train", "HudTextPro", "HudText",
    "SayText", "TextMsg", "WeaponList", "ResetHUD", "InitHUD",
    "ViewMode", "GameTitle", "DeathMsg", "ScoreAttrib", "ScoreInfo",
    "TeamInfo", "TeamScore", "GameMode", "MOTD", "ServerName",
    "AmmoPickup", "WeapPickup", "ItemPickup", "HideWeapon", "SetFOV",
    "ShowMenu", "ScreenShake", "ScreenFade", "AmmoX", "SendAudio",
    "RoundTime", "Money", "ArmorType", "BlinkAcct", "StatusValue",
    "StatusText", "StatusIcon", "BarTime", "ReloadSound", "Crosshair",
    "NVGToggle", "Radar", "Spectator", "VGUIMenu", "TutorText",
    "TutorLine", "TutorState", "TutorClose", "AllowSpec", "BombDrop",
    "BombPickup", "HostagePos", "HostageK", "SendCorpse", "HLTV",
    "SpecHealth", "ForceCam", "ADStop", "ReceiveW", "ScenarioIcon",
    "BotVoice", "BuyClose", "ItemStatus", "Location", "SpecHealth2",
    "BarTime2", "BotProgress", "Brass", "Fog", "ShowTimer",
    "Account", "HealthInfo", "CZCareer", "CZCareerHUD", "TaskTime",
    "ShadowIdx", nullptr
};

// 在 OnServerActivate 调用，构建已知消息名 -> ID 映射
// 此时 ReGameDLL 已完成所有标准消息的注册
void AMXX_BuildUserMsgMap()
{
    if (g_userMsgMapInitialized) return;
    g_userMsgMapInitialized = true;

    for (int i = 0; g_knownCSMsgNames[i] != nullptr; i++) {
        // REG_USER_MSG 对已注册消息返回现有 ID，不会重复注册
        int id = REG_USER_MSG(const_cast<char *>(g_knownCSMsgNames[i]), 0);
        if (id > 0) {
            g_userMsgIdToName[id] = g_knownCSMsgNames[i];
        }
    }
    AMXX_LOG_DBG("[Core] Built user message map: %zu entries", g_userMsgIdToName.size());
}

cell AMX_NATIVE_CALL amxx_get_user_msgid(AMX *amx, cell *params)
{
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char msgname[256];
    amx_GetString(msgname, name_addr, 0, sizeof(msgname));

    // REG_USER_MSG 返回已有 ID（若 name 已注册），否则注册新消息
    // 传 size=0 避免分配数据空间
    int id = REG_USER_MSG(msgname, 0);

    // 同步更新映射表（支持自定义消息反查名称）
    if (id > 0) {
        g_userMsgIdToName[id] = msgname;
    }
    return id;
}

cell AMX_NATIVE_CALL amxx_get_user_msgname(AMX *amx, cell *params)
{
    int msgid = (int)params[1];
    int maxlen = (int)params[3];

    // 确保映射表已初始化（懒加载，防止 OnServerActivate 未触发的情况）
    if (!g_userMsgMapInitialized) {
        AMXX_BuildUserMsgMap();
    }

    const char *name = "";
    auto it = g_userMsgIdToName.find(msgid);
    if (it != g_userMsgIdToName.end()) {
        name = it->second.c_str();
    }

    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    return amx_SetString(dest, name, 0, 0, maxlen);
}

cell AMX_NATIVE_CALL amxx_test_string(AMX *amx, cell *params)
{
    cell *phys_addr;
    amx_GetAddr(amx, params[1], &phys_addr);

    const char *testStr = "HELLO";
    amx_SetString(phys_addr, testStr, 0, 0, params[2]);

    return 1;
}

cell AMX_NATIVE_CALL amxx_test_string2(AMX *amx, cell *params)
{
    char input[512];
    cell *input_addr;
    amx_GetAddr(amx, params[1], &input_addr);
    amx_GetString(input, input_addr, 0, sizeof(input));

    cell *output_addr;
    amx_GetAddr(amx, params[2], &output_addr);
    amx_SetString(output_addr, input, 0, 0, params[3]);

    return 1;
}

cell AMX_NATIVE_CALL amxx_numargs(AMX *amx, cell *params)
{
    (void)params;
    AMX_HEADER *hdr = (AMX_HEADER *)amx->base;
    unsigned char *data = amx->data ? amx->data : amx->base + (int)hdr->dat;
    cell bytes = *(cell *)(data + (int)amx->frm + 2 * sizeof(cell));
    return bytes / sizeof(cell);
}

cell AMX_NATIVE_CALL amxx_getarg(AMX *amx, cell *params)
{
    AMX_HEADER *hdr = (AMX_HEADER *)amx->base;
    unsigned char *data = amx->data ? amx->data : amx->base + (int)hdr->dat;
    cell value = *(cell *)(data + (int)amx->frm + ((int)params[1] + 3) * sizeof(cell));
    value += params[2] * sizeof(cell);
    return *(cell *)(data + (int)value);
}

cell AMX_NATIVE_CALL amxx_setarg(AMX *amx, cell *params)
{
    AMX_HEADER *hdr = (AMX_HEADER *)amx->base;
    unsigned char *data = amx->data ? amx->data : amx->base + (int)hdr->dat;
    cell value = *(cell *)(data + (int)amx->frm + ((int)params[1] + 3) * sizeof(cell));
    value += params[2] * sizeof(cell);
    if (value < 0 || (value >= amx->hea && value < amx->stk))
        return 0;
    *(cell *)(data + (int)value) = params[3];
    return 1;
}

cell AMX_NATIVE_CALL amxx_heapspace(AMX *amx, cell *params)
{
    (void)params;
    return amx->stk - amx->hea;
}

cell AMX_NATIVE_CALL amxx_funcidx(AMX *amx, cell *params)
{
    char name[64];
    cell *cstr;
    int index, err, len;

    amx_GetAddr(amx, params[1], &cstr);
    amx_StrLen(cstr, &len);
    if (len >= 64)
        return 0;

    amx_GetString(name, cstr, 0, sizeof(name));
    err = amx_FindPublic(amx, name, &index);
    if (err != AMX_ERR_NONE)
        index = -1;
    return index;
}

cell AMX_NATIVE_CALL amxx_swapchars(AMX *amx, cell *params)
{
    union {
        cell c;
        unsigned char b[4];
    } value;
    unsigned char t;

    (void)amx;
    value.c = params[1];
    t = value.b[0];
    value.b[0] = value.b[3];
    value.b[3] = t;
    t = value.b[1];
    value.b[1] = value.b[2];
    value.b[2] = t;
    return value.c;
}

cell AMX_NATIVE_CALL amxx_tolower(AMX *amx, cell *params)
{
    (void)amx;
    return (cell)tolower((int)params[1]);
}

cell AMX_NATIVE_CALL amxx_toupper(AMX *amx, cell *params)
{
    (void)amx;
    return (cell)toupper((int)params[1]);
}

cell AMX_NATIVE_CALL amxx_min(AMX *amx, cell *params)
{
    (void)amx;
    return params[1] <= params[2] ? params[1] : params[2];
}

cell AMX_NATIVE_CALL amxx_max(AMX *amx, cell *params)
{
    (void)amx;
    return params[1] >= params[2] ? params[1] : params[2];
}

cell AMX_NATIVE_CALL amxx_clamp(AMX *amx, cell *params)
{
    cell value = params[1];
    if (params[2] > params[3])
        return 0;
    if (value < params[2])
        value = params[2];
    else if (value > params[3])
        value = params[3];
    return value;
}

#define RANDOM_SEED 0xcaa938dbL
static unsigned long random_seed = RANDOM_SEED;
#define RANDOM_MULT 1103515245L

cell AMX_NATIVE_CALL amxx_random(AMX *amx, cell *params)
{
    (void)amx;
    unsigned long lo, hi, ll, lh, hh, hl;
    unsigned long result;

    lo = random_seed & 0xffff;
    hi = random_seed >> 16;
    random_seed = random_seed * RANDOM_MULT + 12345;
    ll = lo * (RANDOM_MULT & 0xffff);
    lh = lo * (RANDOM_MULT >> 16);
    hl = hi * (RANDOM_MULT & 0xffff);
    hh = hi * (RANDOM_MULT >> 16);
    result = ((ll + 12345) >> 16) + lh + hl + (hh << 16);
    result &= ~LONG_MIN;
    if (params[1] != 0)
        result %= params[1];
    return (cell)result;
}

cell AMX_NATIVE_CALL amxx_strcpy(AMX *amx, cell *params)
{
    cell *dest_addr, *src_addr;
    amx_GetAddr(amx, params[1], &dest_addr);
    amx_GetAddr(amx, params[2], &src_addr);
    char src_buf[4096];
    amx_GetString(src_buf, src_addr, 0, sizeof(src_buf));
    return amx_SetString(dest_addr, src_buf, 0, 0, params[3]);
}

cell AMX_NATIVE_CALL amxx_strcat(AMX *amx, cell *params)
{
    cell *dest_addr, *src_addr;
    amx_GetAddr(amx, params[1], &dest_addr);
    amx_GetAddr(amx, params[2], &src_addr);
    int num = (int)params[3];

    // 先走到 dest 现有字符串末尾（最多消耗 maxlength 个 cell）
    cell *pdest = dest_addr;
    while (*pdest && num) {
        pdest++;
        num--;
    }

    if (!num)
        return 0;

    // 再把 src 逐字符追加到 dest 末尾（不超过剩余空间）
    cell *psrc = src_addr;
    while (*psrc && num) {
        *pdest++ = *psrc++;
        num--;
    }
    *pdest = 0;

    // 返回已处理（跳过 + 复制）的字符数，与原版 n_strcat 一致
    return (cell)((int)params[3] - num);
}

cell AMX_NATIVE_CALL amxx_strcmp(AMX *amx, cell *params)
{
    cell *a_addr, *b_addr;
    amx_GetAddr(amx, params[1], &a_addr);
    amx_GetAddr(amx, params[2], &b_addr);
    char a_buf[1024], b_buf[1024];
    amx_GetString(a_buf, a_addr, 0, sizeof(a_buf));
    amx_GetString(b_buf, b_addr, 0, sizeof(b_buf));
    return (cell)strcmp(a_buf, b_buf);
}

cell AMX_NATIVE_CALL amxx_strfind(AMX *amx, cell *params)
{
    cell *str_addr, *sub_addr;
    amx_GetAddr(amx, params[1], &str_addr);
    amx_GetAddr(amx, params[2], &sub_addr);
    char str_buf[4096], sub_buf[256];
    amx_GetString(str_buf, str_addr, 0, sizeof(str_buf));
    amx_GetString(sub_buf, sub_addr, 0, sizeof(sub_buf));

    // 原版签名: strfind(const string[], const sub[], bool:ignorecase = false, pos = 0)
    int num_params = params[0] / sizeof(cell);
    bool ignorecase = (num_params >= 3) ? (params[3] != 0) : false;
    int pos = (num_params >= 4) ? (int)params[4] : 0;

    int strLen = (int)strlen(str_buf);
    // pos 超出字符串长度返回 -1 (原版行为)
    if (pos < 0 || pos > strLen)
        return -1;

    if (ignorecase) {
        char lowerStr[4096], lowerSub[256];
        for (int i = 0; i <= strLen; i++)
            lowerStr[i] = (char)tolower((unsigned char)str_buf[i]);
        int subLen = (int)strlen(sub_buf);
        for (int i = 0; i <= subLen; i++)
            lowerSub[i] = (char)tolower((unsigned char)sub_buf[i]);
        char *found = strstr(lowerStr + pos, lowerSub);
        if (!found)
            return -1;
        return (cell)(found - lowerStr);
    } else {
        char *found = strstr(str_buf + pos, sub_buf);
        if (!found)
            return -1;
        return (cell)(found - str_buf);
    }
}

cell AMX_NATIVE_CALL amxx_strtok(AMX *amx, cell *params)
{
    cell *str_addr;
    amx_GetAddr(amx, params[1], &str_addr);

    char text[4096];
    amx_GetString(text, str_addr, 0, sizeof(text));

    int leftMax = (int)params[3];
    int rightMax = (int)params[5];
    char token = (char)(params[6] & 0xFF); // 取低字节
    int trim = (int)params[7];

    char left[4096], right[4096];
    size_t leftPos = 0, rightPos = 0;
    size_t len = strlen(text);

    // 找到 token 第一次出现的位置（找不到则整段文本进 Left）
    size_t tokenPos = len;
    for (size_t i = 0; i < len; i++) {
        if (text[i] == token) {
            tokenPos = i;
            break;
        }
    }

    // 左边 = text[0, tokenPos)
    for (size_t i = 0; i < tokenPos && leftPos < sizeof(left) - 1; i++)
        left[leftPos++] = text[i];
    left[leftPos] = '\0';

    // 右边 = text(tokenPos, len)（token 本身被丢弃）
    if (tokenPos < len) {
        for (size_t i = tokenPos + 1; i < len && rightPos < sizeof(right) - 1; i++)
            right[rightPos++] = text[i];
    }
    right[rightPos] = '\0';

    // trimSpaces=1 时左右两边都去掉开头/结尾空白
    if (trim) {
        for (int side = 0; side < 2; side++) {
            char *buf = (side == 0) ? left : right;
            size_t s = 0;
            while (buf[s] && isspace((unsigned char)buf[s]))
                s++;
            size_t e = strlen(buf);
            while (e > s && isspace((unsigned char)buf[e - 1]))
                e--;
            buf[e] = '\0';
            if (s > 0)
                memmove(buf, buf + s, e - s + 1);
        }
    }

    cell *left_addr, *right_addr;
    amx_GetAddr(amx, params[2], &left_addr);
    amx_GetAddr(amx, params[4], &right_addr);
    amx_SetString(left_addr, left, 0, 0, leftMax);
    amx_SetString(right_addr, right, 0, 0, rightMax);

    return 1;
}

cell AMX_NATIVE_CALL amxx_format(AMX *amx, cell *params)
{
    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    int len = (int)params[2];
    
    char buffer[4096];
    amxx_format_string(amx, params, 3, buffer, sizeof(buffer));
    
    // 返回实际写入长度（amx_SetString 会截断到 len-1）
    int written = (int)strlen(buffer);
    if (written >= len)
        written = len - 1;
    amx_SetString(dest, buffer, 0, 0, len);
    return (cell)written;
}

cell AMX_NATIVE_CALL amxx_fopen(AMX *amx, cell *params)
{
    cell *filename_addr, *mode_addr;
    amx_GetAddr(amx, params[1], &filename_addr);
    amx_GetAddr(amx, params[2], &mode_addr);
    char filename[512], mode[16];
    amx_GetString(filename, filename_addr, 0, sizeof(filename));
    amx_GetString(mode, mode_addr, 0, sizeof(mode));
    FILE *fp = fopen(filename, mode);
    return (cell)(intptr_t)fp;
}

cell AMX_NATIVE_CALL amxx_fclose(AMX *amx, cell *params)
{
    FILE *fp = (FILE *)(intptr_t)params[1];
    if (!fp)
        return 0;
    return fclose(fp) == 0 ? 1 : 0;
}

// BLOCK_* 数据块模式常量（与旧版 AMXX file.inc 一致）
#define BLOCK_CHAR  0
#define BLOCK_SHORT 1
#define BLOCK_INT   2
#define BLOCK_BYTE  3
#define BLOCK_BITS  4

// fread(file, &data, mode) - 按 mode 从文件读取单值到 data 的 cell
// BLOCK_CHAR/BLOCK_BYTE→1 字节, BLOCK_SHORT→2 字节, BLOCK_INT→4 字节, BLOCK_BITS→1 位
// 返回读取的字节数（BLOCK_BITS 返回 1）
cell AMX_NATIVE_CALL amxx_fread(AMX *amx, cell *params)
{
    FILE *fp = (FILE *)(intptr_t)params[1];
    if (!fp)
        return 0;
    cell *data;
    amx_GetAddr(amx, params[2], &data);
    if (!data)
        return 0;

    int mode = (int)params[3];
    if (mode == BLOCK_BITS) {
        // 按位读取（1 位/次）：读取 1 字节，取其最低位
        int byte = fgetc(fp);
        if (byte == EOF)
            return 0;
        *data = (byte & 1) ? 1 : 0;
        return 1;
    }

    int size = 0;
    switch (mode) {
        case BLOCK_CHAR:
        case BLOCK_BYTE: size = 1; break;
        case BLOCK_SHORT: size = 2; break;
        case BLOCK_INT: size = 4; break;
        default: return 0;
    }

    unsigned char buf[4] = { 0, 0, 0, 0 };
    size_t n = fread(buf, 1, (size_t)size, fp);
    if (n == 0)
        return 0;
    // 按小端序把字节拼到 cell（带符号扩展）
    if (mode == BLOCK_CHAR || mode == BLOCK_BYTE) {
        *data = (cell)(signed char)buf[0];
    } else if (mode == BLOCK_SHORT) {
        short v = (short)((unsigned short)buf[0] | ((unsigned short)buf[1] << 8));
        *data = (cell)v;
    } else {
        int v = (int)((unsigned int)buf[0] | ((unsigned int)buf[1] << 8) |
                      ((unsigned int)buf[2] << 16) | ((unsigned int)buf[3] << 24));
        *data = (cell)v;
    }
    return (cell)n;
}

// fread_blocks(file, data[], blocks, mode) - 读 blocks 个块到数组，返回读取的块数
cell AMX_NATIVE_CALL amxx_fread_blocks(AMX *amx, cell *params)
{
    FILE *fp = (FILE *)(intptr_t)params[1];
    if (!fp)
        return 0;

    cell *data;
    amx_GetAddr(amx, params[2], &data);
    if (!data)
        return 0;

    cell blocks = params[3];
    int mode = (int)params[4];
    if (blocks <= 0)
        return 0;

    cell readCount = 0;
    switch (mode) {
        case BLOCK_CHAR:
        case BLOCK_BYTE: {
            for (cell i = 0; i < blocks; i++) {
                unsigned char buf[1];
                if (fread(buf, 1, 1, fp) != 1)
                    break;
                data[i] = (cell)(signed char)buf[0];
                readCount++;
            }
            break;
        }
        case BLOCK_SHORT: {
            for (cell i = 0; i < blocks; i++) {
                unsigned char buf[2];
                if (fread(buf, 1, 2, fp) != 2)
                    break;
                short v = (short)((unsigned short)buf[0] | ((unsigned short)buf[1] << 8));
                data[i] = (cell)v;
                readCount++;
            }
            break;
        }
        case BLOCK_INT: {
            for (cell i = 0; i < blocks; i++) {
                unsigned char buf[4];
                if (fread(buf, 1, 4, fp) != 4)
                    break;
                int v = (int)((unsigned int)buf[0] | ((unsigned int)buf[1] << 8) |
                              ((unsigned int)buf[2] << 16) | ((unsigned int)buf[3] << 24));
                data[i] = (cell)v;
                readCount++;
            }
            break;
        }
        case BLOCK_BITS: {
            // 按位读取：每个块 1 位（读取 1 字节，取其最低位）
            for (cell i = 0; i < blocks; i++) {
                int byte = fgetc(fp);
                if (byte == EOF)
                    break;
                data[i] = (byte & 1) ? 1 : 0;
                readCount++;
            }
            break;
        }
        default:
            return 0;
    }
    return readCount;
}

// fwrite(file, data, mode) - 按 mode 写单个值到文件，返回写入的字节数
cell AMX_NATIVE_CALL amxx_fwrite(AMX *amx, cell *params)
{
    FILE *fp = (FILE *)(intptr_t)params[1];
    if (!fp)
        return 0;

    cell data = params[2];
    int mode = (int)params[3];

    switch (mode) {
        case BLOCK_CHAR:
        case BLOCK_BYTE: {
            char v = (char)(data & 0xFF);
            return (cell)fwrite(&v, 1, 1, fp);
        }
        case BLOCK_SHORT: {
            short v = (short)(data & 0xFFFF);
            return (cell)fwrite(&v, 1, 2, fp);
        }
        case BLOCK_INT: {
            int v = (int)data;
            return (cell)fwrite(&v, 1, 4, fp);
        }
        case BLOCK_BITS: {
            // 按位写入（1 位/次）：写入 1 字节
            char v = (data & 1) ? 1 : 0;
            return (cell)fwrite(&v, 1, 1, fp);
        }
        default:
            return 0;
    }
}

cell AMX_NATIVE_CALL amxx_fseek(AMX *amx, cell *params)
{
    FILE *fp = (FILE *)(intptr_t)params[1];
    if (!fp)
        return -1;
    return fseek(fp, (long)params[2], (int)params[3]);
}

cell AMX_NATIVE_CALL amxx_ftell(AMX *amx, cell *params)
{
    FILE *fp = (FILE *)(intptr_t)params[1];
    if (!fp)
        return -1;
    return (cell)ftell(fp);
}

cell AMX_NATIVE_CALL amxx_feof(AMX *amx, cell *params)
{
    FILE *fp = (FILE *)(intptr_t)params[1];
    if (!fp)
        return 1;
    return feof(fp) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_file_size(AMX *amx, cell *params)
{
    cell *filename_addr;
    amx_GetAddr(amx, params[1], &filename_addr);
    char filename[512];
    amx_GetString(filename, filename_addr, 0, sizeof(filename));
    FILE *fp = fopen(filename, "rb");
    if (!fp)
        return -1;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fclose(fp);
    return (cell)size;
}

cell AMX_NATIVE_CALL amxx_floatround(AMX *amx, cell *params)
{
    (void)amx;
    float value = amx_ctof(params[1]);
    int method = (int)params[2];
    cell result;
    switch (method) {
        case 0: result = (cell)floor(value + 0.5); break;  // round (负数 .5 向正无穷)
        case 1: result = (cell)floor(value); break;        // floor
        case 2: result = (cell)ceil(value); break;         // ceil
        case 3: result = (cell)value; break;               // tozero (截断)
        default: result = (cell)floor(value + 0.5); break; // round
    }
    return result;
}

cell AMX_NATIVE_CALL amxx_floatsqroot(AMX *amx, cell *params)
{
    (void)amx;
    float value = amx_ctof(params[1]);
    float result = (float)sqrt(value);
    return amx_ftoc(result);
}

cell AMX_NATIVE_CALL amxx_floatabs(AMX *amx, cell *params)
{
    (void)amx;
    float value = amx_ctof(params[1]);
    float result = (float)fabs(value);
    return amx_ftoc(result);
}

cell AMX_NATIVE_CALL amxx_floatcos(AMX *amx, cell *params)
{
    (void)amx;
    float value = amx_ctof(params[1]);
    float result = (float)cos(value);
    return amx_ftoc(result);
}

cell AMX_NATIVE_CALL amxx_floatsin(AMX *amx, cell *params)
{
    (void)amx;
    float value = amx_ctof(params[1]);
    float result = (float)sin(value);
    return amx_ftoc(result);
}

cell AMX_NATIVE_CALL amxx_floattan(AMX *amx, cell *params)
{
    (void)amx;
    float value = amx_ctof(params[1]);
    float result = (float)tan(value);
    return amx_ftoc(result);
}

cell AMX_NATIVE_CALL amxx_floatasin(AMX *amx, cell *params)
{
    (void)amx;
    float value = amx_ctof(params[1]);
    float result = (float)asin(value);
    return amx_ftoc(result);
}

cell AMX_NATIVE_CALL amxx_floatacos(AMX *amx, cell *params)
{
    (void)amx;
    float value = amx_ctof(params[1]);
    float result = (float)acos(value);
    return amx_ftoc(result);
}

cell AMX_NATIVE_CALL amxx_floatatan(AMX *amx, cell *params)
{
    (void)amx;
    float value = amx_ctof(params[1]);
    float result = (float)atan(value);
    return amx_ftoc(result);
}

cell AMX_NATIVE_CALL amxx_floatatan2(AMX *amx, cell *params)
{
    (void)amx;
    float y = amx_ctof(params[1]);
    float x = amx_ctof(params[2]);
    float result = (float)atan2(y, x);
    return amx_ftoc(result);
}

cell AMX_NATIVE_CALL amxx_floatsinh(AMX *amx, cell *params)
{
    (void)amx;
    float value = amx_ctof(params[1]);
    float result = (float)sinh(value);
    return amx_ftoc(result);
}

cell AMX_NATIVE_CALL amxx_floatcosh(AMX *amx, cell *params)
{
    (void)amx;
    float value = amx_ctof(params[1]);
    float result = (float)cosh(value);
    return amx_ftoc(result);
}

cell AMX_NATIVE_CALL amxx_floattanh(AMX *amx, cell *params)
{
    (void)amx;
    float value = amx_ctof(params[1]);
    float result = (float)tanh(value);
    return amx_ftoc(result);
}

cell AMX_NATIVE_CALL amxx_floatlog(AMX *amx, cell *params)
{
    (void)amx;
    float value = amx_ctof(params[1]);
    float result = (float)log(value);
    return amx_ftoc(result);
}

cell AMX_NATIVE_CALL amxx_get_cvar_string(AMX *amx, cell *params)
{
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char name[64];
    amx_GetString(name, name_addr, 0, sizeof(name));
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    const char *value = g_engfuncs.pfnCVarGetString(name);
    if (!value)
        value = "";
    // 返回实际写入长度（原版 get_cvar_string 返回 strlen）
    int written = (int)strlen(value);
    if (written >= (int)params[3])
        written = (int)params[3] - 1;
    amx_SetString(dest, value, 0, 0, params[3]);
    return (cell)written;
}

cell AMX_NATIVE_CALL amxx_get_cvar_num(AMX *amx, cell *params)
{
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char name[64];
    amx_GetString(name, name_addr, 0, sizeof(name));
    float result = g_engfuncs.pfnCVarGetFloat(name);
    return (cell)result; // 返回整数（原版 cvars.cpp 返回 (int)var->value）
}

cell AMX_NATIVE_CALL amxx_set_cvar_string(AMX *amx, cell *params)
{
    cell *name_addr, *value_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    amx_GetAddr(amx, params[2], &value_addr);
    char name[64], value[512];
    amx_GetString(name, name_addr, 0, sizeof(name));
    amx_GetString(value, value_addr, 0, sizeof(value));
    g_engfuncs.pfnCVarSetString(name, value);
    return 1;
}

cell AMX_NATIVE_CALL amxx_set_cvar_num(AMX *amx, cell *params)
{
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char name[64];
    amx_GetString(name, name_addr, 0, sizeof(name));
    g_engfuncs.pfnCVarSetFloat(name, amx_ctof(params[2]));
    return 1;
}

cell AMX_NATIVE_CALL amxx_register_cvar(AMX *amx, cell *params)
{
    cell *name_addr, *value_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    amx_GetAddr(amx, params[2], &value_addr);
    char name[64], value[512];
    amx_GetString(name, name_addr, 0, sizeof(name));
    amx_GetString(value, value_addr, 0, sizeof(value));

    // P2-1: 原版 register_cvar(name[], string[], flags=0, Float:fvalue=0.0)
    // params[3] = flags (可选), params[4] = fvalue (可选)
    cell num_params = params[0] / sizeof(cell);
    int flags = (num_params >= 3) ? (int)params[3] : 0;
    float fvalue = (num_params >= 4) ? amx_ctof(params[4]) : 0.0f;

    // 获取插件 ID (1-based)
    int pluginId = -1;
    const auto &plugins = AMXXRuntime::GetInstance().GetPlugins();
    for (size_t i = 0; i < plugins.size(); i++) {
        if (plugins[i] && plugins[i]->GetAMX() == amx) {
            pluginId = (int)(i + 1);
            break;
        }
    }

    // 记录到插件 cvar 追踪表 (避免重复)
    bool alreadyTracked = false;
    for (const auto &entry : g_pluginCvars) {
        if (entry.pluginId == pluginId && entry.name == name) {
            alreadyTracked = true;
            break;
        }
    }
    if (!alreadyTracked) {
        PluginCvarEntry entry;
        entry.pluginId = pluginId;
        entry.name = name;
        entry.flags = flags;
        g_pluginCvars.push_back(entry);
    }

    // P2: 原版行为——cvar 已存在时不重复注册, 复用引擎指针返回 (不泄漏内存)
    cvar_t *existing = g_engfuncs.pfnCVarGetPointer(name);
    if (existing) {
        return (cell)(intptr_t)existing;
    }

    // Use heap-allocated cvar_t with persistent string storage
    cvar_t *cvar = (cvar_t *)malloc(sizeof(cvar_t));
    if (!cvar) return 0;
    memset(cvar, 0, sizeof(cvar_t));
    cvar->name = strdup(name);
    cvar->string = strdup(value);
    cvar->flags = flags;
    // 若插件提供了 fvalue 则用它，否则从 string 推导
    cvar->value = (num_params >= 4) ? fvalue : (float)atof(value);

    g_engfuncs.pfnCVarRegister(cvar);
    return (cell)(intptr_t)cvar;
}

cell AMX_NATIVE_CALL amxx_get_mapname(AMX *amx, cell *params)
{
    (void)amx;
    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    return amx_SetString(dest, STRING(gpGlobals->mapname), 0, 0, params[2]);
}

cell AMX_NATIVE_CALL amxx_change_map(AMX *amx, cell *params)
{
    (void)amx;
    cell *mapname_addr;
    amx_GetAddr(amx, params[1], &mapname_addr);
    char mapname[64];
    amx_GetString(mapname, mapname_addr, 0, sizeof(mapname));
    // 原版 AMXX: server_changelevel forward 在 CHANGE_LEVEL 之前触发
    AMXXRuntime::GetInstance().OnChangeLevel(mapname);
    g_engfuncs.pfnChangeLevel(mapname, "");
    return 1;
}

cell AMX_NATIVE_CALL amxx_client_kick(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free)
        return 0;
    g_engfuncs.pfnClientCommand(pEdict, "disconnect");
    return 1;
}

cell AMX_NATIVE_CALL amxx_client_disconnect(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free)
        return 0;
    g_engfuncs.pfnClientCommand(pEdict, "disconnect");
    return 1;
}

cell AMX_NATIVE_CALL amxx_get_user_ip(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients) {
        cell *dest;
        amx_GetAddr(amx, params[2], &dest);
        return amx_SetString(dest, "", 0, 0, params[3]);
    }
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free) {
        cell *dest;
        amx_GetAddr(amx, params[2], &dest);
        return amx_SetString(dest, "", 0, 0, params[3]);
    }
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    
    char *infobuffer = g_engfuncs.pfnGetInfoKeyBuffer(pEdict);
    const char *ip = g_engfuncs.pfnInfoKeyValue(infobuffer, "ip");
    if (!ip)
        ip = "";
    
    return amx_SetString(dest, ip, 0, 0, params[3]);
}

cell AMX_NATIVE_CALL amxx_get_user_team(AMX *amx, cell *params)
{
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return -1;

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free || !pEdict->pvPrivateData)
        return -1;

    CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
    if (!pPlayer)
        return 0;

    // ReGameDLL 中队伍存于 CBasePlayer::m_iTeam（0=UNASSIGNED, 1=T, 2=CT, 3=SPECTATOR）
    int teamId = (int)pPlayer->m_iTeam;

    // 若提供了输出缓冲（params[2] 且 params[3] > 0），按 teamId 映射队伍名称
    int numParams = (int)(params[0] / sizeof(cell));
    if (numParams >= 3 && params[2] != 0 && params[3] > 0) {
        cell *dest;
        amx_GetAddr(amx, params[2], &dest);
        const char *teamName = "UNASSIGNED";
        switch (teamId) {
            case 1: teamName = "TERRORIST"; break;
            case 2: teamName = "CT"; break;
            case 3: teamName = "SPECTATOR"; break;
            default: teamName = "UNASSIGNED"; break;
        }
        amx_SetString(dest, teamName, 0, 0, params[3]);
    }

    return (cell)teamId;
}

cell AMX_NATIVE_CALL amxx_set_user_team(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free)
        return 0;
    cell *team_addr;
    amx_GetAddr(amx, params[2], &team_addr);
    char team[32];
    amx_GetString(team, team_addr, 0, sizeof(team));
    pEdict->v.team = ALLOC_STRING(team);
    return 1;
}

cell AMX_NATIVE_CALL amxx_give_item(AMX *amx, cell *params)
{
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free || !pEdict->pvPrivateData)
        return 0;

    cell *item_addr;
    amx_GetAddr(amx, params[2], &item_addr);
    char item[64];
    amx_GetString(item, item_addr, 0, sizeof(item));

    CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
    if (!pPlayer)
        return 0;

    CBaseEntity *pEntity = pPlayer->GiveNamedItem(item);
    if (!pEntity)
        return 0;

    // 返回实体 index（原版行为：成功返回 entindex，失败返回 0）
    edict_t *pEntEdict = pEntity->edict();
    if (!pEntEdict || pEntEdict->free)
        return 0;

    return (cell)ENTINDEX(pEntEdict);
}

cell AMX_NATIVE_CALL amxx_strip_user_weapons(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;

    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free || !pEdict->pvPrivateData)
        return 0;

    CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
    if (!pPlayer)
        return 0;

    pPlayer->RemoveAllItems(FALSE);
    return 1;
}

static CBasePlayer *GetPlayerByIndex(int index)
{
    if (index < 1 || index > gpGlobals->maxClients)
        return nullptr;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || !pEdict->pvPrivateData)
        return nullptr;
    return GET_PRIVATE<CBasePlayer>(pEdict);
}

static edict_t *GetEdictByIndex(int index)
{
    if (index < 1 || index > gpGlobals->maxClients)
        return nullptr;
    return INDEXENT(index);
}

cell AMX_NATIVE_CALL amxx_is_user_connected(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free)
        return 0;
    CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(pEdict);
    return (pPlayer && pPlayer->IsNetClient()) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_is_user_bot(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    edict_t *pEdict = GetEdictByIndex(index);
    if (!pEdict)
        return 0;
    return (pEdict->v.flags & FL_FAKECLIENT) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_is_user_hltv(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    edict_t *pEdict = GetEdictByIndex(index);
    if (!pEdict)
        return 0;
    char *infobuffer = g_engfuncs.pfnGetInfoKeyBuffer(pEdict);
    const char *val = g_engfuncs.pfnInfoKeyValue(infobuffer, "hltv");
    return (val && atoi(val) != 0) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_get_user_health(AMX *amx, cell *params)
{
    (void)amx;
    CBasePlayer *pPlayer = GetPlayerByIndex((int)params[1]);
    if (!pPlayer)
        return 0;
    return (cell)pPlayer->pev->health;
}

cell AMX_NATIVE_CALL amxx_set_user_health(AMX *amx, cell *params)
{
    (void)amx;
    CBasePlayer *pPlayer = GetPlayerByIndex((int)params[1]);
    if (!pPlayer)
        return 0;
    pPlayer->pev->health = amx_ctof(params[2]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_get_user_armor(AMX *amx, cell *params)
{
    (void)amx;
    CBasePlayer *pPlayer = GetPlayerByIndex((int)params[1]);
    if (!pPlayer)
        return 0;
    return (cell)pPlayer->pev->armorvalue;
}

cell AMX_NATIVE_CALL amxx_set_user_armor(AMX *amx, cell *params)
{
    (void)amx;
    CBasePlayer *pPlayer = GetPlayerByIndex((int)params[1]);
    if (!pPlayer)
        return 0;
    pPlayer->pev->armorvalue = (float)params[2];
    return 1;
}

cell AMX_NATIVE_CALL amxx_get_user_frags(AMX *amx, cell *params)
{
    (void)amx;
    CBasePlayer *pPlayer = GetPlayerByIndex((int)params[1]);
    if (!pPlayer)
        return 0;
    return (cell)pPlayer->pev->frags;
}

cell AMX_NATIVE_CALL amxx_set_user_frags(AMX *amx, cell *params)
{
    (void)amx;
    CBasePlayer *pPlayer = GetPlayerByIndex((int)params[1]);
    if (!pPlayer)
        return 0;
    pPlayer->pev->frags = (float)params[2];
    return 1;
}

cell AMX_NATIVE_CALL amxx_get_user_deaths(AMX *amx, cell *params)
{
    (void)amx;
    CBasePlayer *pPlayer = GetPlayerByIndex((int)params[1]);
    if (!pPlayer)
        return 0;
    return (cell)pPlayer->m_iDeaths;
}

cell AMX_NATIVE_CALL amxx_set_user_deaths(AMX *amx, cell *params)
{
    (void)amx;
    CBasePlayer *pPlayer = GetPlayerByIndex((int)params[1]);
    if (!pPlayer)
        return 0;
    pPlayer->m_iDeaths = (int)params[2];
    return 1;
}

cell AMX_NATIVE_CALL amxx_get_user_weapon(AMX *amx, cell *params)
{
    CBasePlayer *pPlayer = GetPlayerByIndex((int)params[1]);
    int weaponId = 0;
    int clip = 0;
    int ammo = 0;

    if (pPlayer && pPlayer->m_pActiveItem) {
        weaponId = pPlayer->m_pActiveItem->m_iId;

        // 提取 clip 弹药（当前弹匣子弹数）
        CBasePlayerWeapon *pWeapon = static_cast<CBasePlayerWeapon *>(pPlayer->m_pActiveItem);
        clip = pWeapon->m_iClip;

        // 提取备用弹药（通过武器的主弹药类型索引）
        int ammoIndex = pWeapon->m_iPrimaryAmmoType;
        if (ammoIndex >= 0 && ammoIndex < MAX_AMMO_SLOTS) {
            ammo = pPlayer->m_rgAmmo[ammoIndex];
        }
    }

    if (params[0] / sizeof(cell) >= 2) {
        cell *clipOut;
        amx_GetAddr(amx, params[2], &clipOut);
        if (clipOut) *clipOut = clip;
    }
    if (params[0] / sizeof(cell) >= 3) {
        cell *ammoOut;
        amx_GetAddr(amx, params[3], &ammoOut);
        if (ammoOut) *ammoOut = ammo;
    }

    return weaponId;
}

cell AMX_NATIVE_CALL amxx_get_user_ammo(AMX *amx, cell *params)
{
    (void)amx;
    CBasePlayer *pPlayer = GetPlayerByIndex((int)params[1]);
    if (!pPlayer)
        return 0;
    int slot = (int)params[2];
    if (slot < 0 || slot >= MAX_AMMO_SLOTS)
        return 0;
    return pPlayer->m_rgAmmo[slot];
}

cell AMX_NATIVE_CALL amxx_get_user_ping(AMX *amx, cell *params)
{
    edict_t *pEdict = GetEdictByIndex((int)params[1]);
    if (!pEdict)
        return 0;
    int ping = 0, packet_loss = 0;
    g_engfuncs.pfnGetPlayerStats(pEdict, &ping, &packet_loss);

    if (params[0] / sizeof(cell) >= 2) {
        cell *pingOut;
        amx_GetAddr(amx, params[2], &pingOut);
        if (pingOut) *pingOut = ping;
    }
    if (params[0] / sizeof(cell) >= 3) {
        cell *lossOut;
        amx_GetAddr(amx, params[3], &lossOut);
        if (lossOut) *lossOut = packet_loss;
    }

    return ping;
}

cell AMX_NATIVE_CALL amxx_get_user_time(AMX *amx, cell *params)
{
    (void)amx;
    edict_t *pEdict = GetEdictByIndex((int)params[1]);
    if (!pEdict)
        return 0;
    float time = gpGlobals->time - pEdict->v.teleport_time;
    return amx_ftoc(time);
}

// get_user_origin 的 mode 4 (lastHit) 使用的 trace 命中位置记录
static Vector g_amxx_lastTrace(0, 0, 0);

cell AMX_NATIVE_CALL amxx_get_user_origin(AMX *amx, cell *params)
{
    int index = (int)params[1];
    edict_t *pEdict = GetEdictByIndex(index);
    if (!pEdict)
        return 0;

    // 原版签名: get_user_origin(index, origin[3], mode)
    int mode = 0;
    if (params[0] / sizeof(cell) >= 3)
        mode = (int)params[3];

    cell *dest;
    amx_GetAddr(amx, params[2], &dest);

    // mode 4: 返回上一条 trace 的命中位置 (lastHit), 未命中为 0
    if (mode == 4) {
        dest[0] = (cell)g_amxx_lastTrace.x;
        dest[1] = (cell)g_amxx_lastTrace.y;
        dest[2] = (cell)g_amxx_lastTrace.z;
        return 1;
    }

    Vector pos = pEdict->v.origin;

    // mode 0/2: 仅 origin; mode 1/3/5: origin + view_ofs (eye 位置)
    if (mode && mode != 2)
        pos = pos + pEdict->v.view_ofs;

    // mode 2/3/5: 从 pos 沿 v_angle 方向 TRACE_LINE 9999 距离
    if (mode > 1) {
        Vector v_angle = pEdict->v.v_angle;
        float v_vec[3];
        v_vec[0] = v_angle.x;
        v_vec[1] = v_angle.y;
        v_vec[2] = v_angle.z;

        Vector forward;
        g_engfuncs.pfnAngleVectors(v_vec, forward, NULL, NULL);

        TraceResult trEnd;
        Vector v_dest = pos + forward * 9999.0f;

        float f_pos[3];
        f_pos[0] = pos.x;
        f_pos[1] = pos.y;
        f_pos[2] = pos.z;

        float f_dest[3];
        f_dest[0] = v_dest.x;
        f_dest[1] = v_dest.y;
        f_dest[2] = v_dest.z;

        TRACE_LINE(f_pos, f_dest, dont_ignore_monsters, pEdict, &trEnd);

        if (trEnd.flFraction < 1.0f) {
            pos = trEnd.vecEndPos;
            g_amxx_lastTrace = pos;
        } else {
            pos = Vector(0, 0, 0);
        }
    }

    dest[0] = (cell)pos.x;
    dest[1] = (cell)pos.y;
    dest[2] = (cell)pos.z;
    return 1;
}

cell AMX_NATIVE_CALL amxx_get_user_attacker(AMX *amx, cell *params)
{
    CBasePlayer *pPlayer = GetPlayerByIndex((int)params[1]);
    if (!pPlayer)
        return 0;

    cell *attackerOut = nullptr;
    cell *damageOut = nullptr;
    cell *weaponOut = nullptr;
    if (params[0] / sizeof(cell) >= 2)
        amx_GetAddr(amx, params[2], &attackerOut);
    if (params[0] / sizeof(cell) >= 3)
        amx_GetAddr(amx, params[3], &damageOut);
    if (params[0] / sizeof(cell) >= 4)
        amx_GetAddr(amx, params[4], &weaponOut);

    PlayerDamageRecord &record = AMXXRuntime::GetInstance().GetDamageRecord((int)params[1]);

    if (attackerOut)
        *attackerOut = record.lastAttacker;
    if (damageOut)
        *damageOut = (cell)record.lastDamage;
    if (weaponOut)
        *weaponOut = record.lastWeapon;

    return record.lastAttacker;
}

cell AMX_NATIVE_CALL amxx_get_user_aiming(AMX *amx, cell *params)
{
    int index = (int)params[1];
    edict_t *pEdict = GetEdictByIndex(index);
    if (!pEdict)
        return 0;

    TraceResult tr;
    Vector vecSrc = pEdict->v.origin + pEdict->v.view_ofs;
    MAKE_VECTORS(pEdict->v.v_angle);
    Vector vecEnd = vecSrc + gpGlobals->v_forward * 8192.0f;
    TRACE_LINE(vecSrc, vecEnd, dont_ignore_monsters, pEdict, &tr);

    cell *idOut = nullptr;
    cell *bodyOut = nullptr;
    if (params[0] / sizeof(cell) >= 2)
        amx_GetAddr(amx, params[2], &idOut);
    if (params[0] / sizeof(cell) >= 3)
        amx_GetAddr(amx, params[3], &bodyOut);

    int hitId = 0;
    int hitBody = 0;
    if (tr.pHit) {
        hitId = ENTINDEX(tr.pHit);
        hitBody = tr.iHitgroup;
    }

    if (idOut)
        *idOut = hitId;
    if (bodyOut)
        *bodyOut = hitBody;

    float dist = 0.0f;
    if (tr.flFraction < 1.0f)
        dist = (tr.vecEndPos - vecSrc).Length();
    return amx_ftoc(dist);
}

cell AMX_NATIVE_CALL amxx_precache_model(AMX *amx, cell *params)
{
    (void)amx;
    cell *filename_addr;
    amx_GetAddr(amx, params[1], &filename_addr);
    char filename[256];
    amx_GetString(filename, filename_addr, 0, sizeof(filename));
    return g_engfuncs.pfnPrecacheModel(filename);
}

cell AMX_NATIVE_CALL amxx_precache_sound(AMX *amx, cell *params)
{
    (void)amx;
    cell *filename_addr;
    amx_GetAddr(amx, params[1], &filename_addr);
    char filename[256];
    amx_GetString(filename, filename_addr, 0, sizeof(filename));
    return g_engfuncs.pfnPrecacheSound(filename);
}

cell AMX_NATIVE_CALL amxx_precache_generic(AMX *amx, cell *params)
{
    (void)amx;
    cell *filename_addr;
    amx_GetAddr(amx, params[1], &filename_addr);
    char filename[256];
    amx_GetString(filename, filename_addr, 0, sizeof(filename));
    return g_engfuncs.pfnPrecacheGeneric(filename);
}

static struct DHudParams_s {
    int r, g, b;
    int x, y;
    int effect;
    float fxtime;
    int fadein, fadeout, hold;
} g_dhudParams;

static void SendDHUDToPlayer(edict_t *pEntity, const char *msg)
{
    MESSAGE_BEGIN(MSG_ONE, SVC_TEMPENTITY, nullptr, pEntity);
    WRITE_BYTE(TE_TEXTMESSAGE);
    WRITE_BYTE(1);
    WRITE_SHORT(FixedSigned16(g_dhudParams.x != -1 ? (float)g_dhudParams.x / 1000.0f : -1.0f, (1 << 13)));
    WRITE_SHORT(FixedSigned16(g_dhudParams.y != -1 ? (float)g_dhudParams.y / 1000.0f : 0.3f, (1 << 13)));
    WRITE_BYTE(g_dhudParams.effect);
    WRITE_BYTE(g_dhudParams.r); WRITE_BYTE(g_dhudParams.g); WRITE_BYTE(g_dhudParams.b); WRITE_BYTE(200);
    WRITE_BYTE(255); WRITE_BYTE(255); WRITE_BYTE(255); WRITE_BYTE(200);
    WRITE_SHORT(FixedUnsigned16((float)g_dhudParams.fadein / 1000.0f, (1 << 8)));
    WRITE_SHORT(FixedUnsigned16((float)g_dhudParams.fadeout / 1000.0f, (1 << 8)));
    WRITE_SHORT(FixedUnsigned16((float)g_dhudParams.hold / 1000.0f, (1 << 8)));
    if (g_dhudParams.effect == 2)
        WRITE_SHORT(FixedUnsigned16(g_dhudParams.fxtime, (1 << 8)));
    WRITE_STRING(msg);
    MESSAGE_END();
}

cell AMX_NATIVE_CALL amxx_set_dhudmessage(AMX *amx, cell *params)
{
    (void)amx;
    g_dhudParams.r = (int)params[1];
    g_dhudParams.g = (int)params[2];
    g_dhudParams.b = (int)params[3];
    g_dhudParams.x = (int)amx_ctof(params[4]);
    g_dhudParams.y = (int)amx_ctof(params[5]);
    g_dhudParams.effect = (int)params[6];
    if (params[0] / sizeof(cell) >= 7)
        g_dhudParams.fxtime = amx_ctof(params[7]);
    g_dhudParams.fadein = (int)amx_ctof(params[8]);
    g_dhudParams.fadeout = (int)amx_ctof(params[9]);
    g_dhudParams.hold = (int)amx_ctof(params[10]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_show_dhudmessage(AMX *amx, cell *params)
{
    int index = (int)params[1];
    if (index < 0 || index > gpGlobals->maxClients)
        return 0;

    cell *msg_addr;
    amx_GetAddr(amx, params[2], &msg_addr);
    char msg[1024];
    amx_GetString(msg, msg_addr, 0, sizeof(msg));

    int len = (int)strlen(msg);
    for (int i = 0; i < len; i++)
        if (msg[i] == '\n')
            msg[i] = '\r';

    if (index == 0) {
        for (int i = 1; i <= gpGlobals->maxClients; i++) {
            edict_t *p = INDEXENT(i);
            if (p && !p->free)
                SendDHUDToPlayer(p, msg);
        }
    } else {
        edict_t *pEdict = INDEXENT(index);
        if (!pEdict || pEdict->free)
            return 0;
        SendDHUDToPlayer(pEdict, msg);
    }
    return 1;
}

extern int gmsgMOTD;

static void SendMOTDToPlayer(edict_t *pEntity, int msgId, const char *msg)
{
    MESSAGE_BEGIN(MSG_ONE, msgId, nullptr, pEntity);
    WRITE_BYTE(0);
    WRITE_STRING(msg);
    MESSAGE_END();
}

cell AMX_NATIVE_CALL amxx_show_motd(AMX *amx, cell *params)
{
    int index = (int)params[1];
    if (index < 0 || index > gpGlobals->maxClients)
        return 0;

    cell *msg_addr;
    amx_GetAddr(amx, params[2], &msg_addr);
    char msg[4096];
    amx_GetString(msg, msg_addr, 0, sizeof(msg));

    int motdMsgId = gmsgMOTD;
    if (motdMsgId <= 0)
        motdMsgId = REG_USER_MSG("MOTD", -1);

    if (index == 0) {
        for (int i = 1; i <= gpGlobals->maxClients; i++) {
            edict_t *p = INDEXENT(i);
            if (p && !p->free)
                SendMOTDToPlayer(p, motdMsgId, msg);
        }
    } else {
        edict_t *pEdict = INDEXENT(index);
        if (!pEdict || pEdict->free)
            return 0;
        SendMOTDToPlayer(pEdict, motdMsgId, msg);
    }
    return 1;
}

static int g_msgDest = 0;
static edict_t *g_msgEdict = nullptr;
static int g_msgType = 0;

cell AMX_NATIVE_CALL amxx_message_begin(AMX *amx, cell *params)
{
    (void)amx;
    int numParams = (int)(params[0] / sizeof(cell));
    int dest = (int)params[1];
    int msgType = (int)params[2];

    // msg_type 合法性: >= 0 即可
    if (msgType < 0) {
        AMXX_LOG("[message_begin] Plugin called message_begin with an invalid message id (%d).", msgType);
        return 0;
    }

    g_msgDest = dest;
    g_msgType = msgType;
    g_msgEdict = nullptr;

    switch (dest) {
        case MSG_BROADCAST:
        case MSG_ALL:
        case MSG_SPEC:
        case MSG_INIT:
            g_engfuncs.pfnMessageBegin(dest, msgType, nullptr, nullptr);
            break;
        case MSG_PVS:
        case MSG_PAS:
        case MSG_PVS_R:
        case MSG_PAS_R:
            // 需要 origin 参数
            if (numParams < 3) {
                AMXX_LOG("[message_begin] Invalid number of parameters passed for dest %d", dest);
                return 0;
            }
            {
                cell *org_addr;
                amx_GetAddr(amx, params[3], &org_addr);
                float origin[3];
                origin[0] = (float)org_addr[0];
                origin[1] = (float)org_addr[1];
                origin[2] = (float)org_addr[2];
                g_engfuncs.pfnMessageBegin(dest, msgType, origin, nullptr);
            }
            break;
        case MSG_ONE:
        case MSG_ONE_UNRELIABLE:
            // 需要 player 参数
            if (numParams < 4) {
                AMXX_LOG("[message_begin] Invalid number of parameters passed for dest %d", dest);
                return 0;
            }
            {
                int player = (int)params[4];
                if (player >= 1 && player <= gpGlobals->maxClients) {
                    edict_t *pEdict = INDEXENT(player);
                    if (pEdict && !pEdict->free)
                        g_msgEdict = pEdict;
                }
                g_engfuncs.pfnMessageBegin(dest, msgType, nullptr, g_msgEdict);
            }
            break;
        default:
            g_engfuncs.pfnMessageBegin(MSG_BROADCAST, msgType, nullptr, nullptr);
            break;
    }
    return 1;
}

cell AMX_NATIVE_CALL amxx_message_end(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    g_engfuncs.pfnMessageEnd();
    return 1;
}

cell AMX_NATIVE_CALL amxx_write_byte(AMX *amx, cell *params)
{
    (void)amx;
    g_engfuncs.pfnWriteByte((int)params[1]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_write_short(AMX *amx, cell *params)
{
    (void)amx;
    g_engfuncs.pfnWriteShort((int)params[1]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_write_long(AMX *amx, cell *params)
{
    (void)amx;
    g_engfuncs.pfnWriteLong((int)params[1]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_write_float(AMX *amx, cell *params)
{
    (void)amx;
    float value = amx_ctof(params[1]);
    g_engfuncs.pfnWriteLong(*(int *)&value);
    return 1;
}

cell AMX_NATIVE_CALL amxx_write_string(AMX *amx, cell *params)
{
    cell *str_addr;
    amx_GetAddr(amx, params[1], &str_addr);
    char str[512];
    amx_GetString(str, str_addr, 0, sizeof(str));
    g_engfuncs.pfnWriteString(str);
    return 1;
}

cell AMX_NATIVE_CALL amxx_write_coord(AMX *amx, cell *params)
{
    (void)amx;
    g_engfuncs.pfnWriteCoord(amx_ctof(params[1]));
    return 1;
}

cell AMX_NATIVE_CALL amxx_message_begin_f(AMX *amx, cell *params)
{
    (void)amx;
    g_msgDest = (int)params[1];
    g_msgType = (int)params[2];

    float origin[3] = {0.0f, 0.0f, 0.0f};
    if (params[0] >= 3 * sizeof(cell)) {
        cell *org_addr;
        amx_GetAddr(amx, params[3], &org_addr);
        origin[0] = amx_ctof(org_addr[0]);
        origin[1] = amx_ctof(org_addr[1]);
        origin[2] = amx_ctof(org_addr[2]);
    }

    int player = 0;
    if (params[0] >= 4 * sizeof(cell)) {
        player = (int)params[4];
    }

    g_msgEdict = nullptr;
    if (player >= 1 && player <= gpGlobals->maxClients) {
        edict_t *pEdict = INDEXENT(player);
        if (pEdict && !pEdict->free)
            g_msgEdict = pEdict;
    }

    switch (g_msgDest) {
        case 0: g_engfuncs.pfnMessageBegin(MSG_BROADCAST, g_msgType, origin, nullptr); break;
        case 1: g_engfuncs.pfnMessageBegin(MSG_ONE, g_msgType, origin, g_msgEdict); break;
        case 2: g_engfuncs.pfnMessageBegin(MSG_ALL, g_msgType, origin, nullptr); break;
        case 3: g_engfuncs.pfnMessageBegin(MSG_INIT, g_msgType, origin, nullptr); break;
        default: g_engfuncs.pfnMessageBegin(MSG_BROADCAST, g_msgType, origin, nullptr); break;
    }
    return 1;
}

cell AMX_NATIVE_CALL amxx_write_char(AMX *amx, cell *params)
{
    (void)amx;
    g_engfuncs.pfnWriteChar((char)params[1]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_write_entity(AMX *amx, cell *params)
{
    (void)amx;
    g_engfuncs.pfnWriteEntity((int)params[1]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_write_angle(AMX *amx, cell *params)
{
    (void)amx;
    g_engfuncs.pfnWriteAngle((int)params[1]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_write_angle_f(AMX *amx, cell *params)
{
    (void)amx;
    g_engfuncs.pfnWriteAngle(amx_ctof(params[1]));
    return 1;
}

cell AMX_NATIVE_CALL amxx_write_coord_f(AMX *amx, cell *params)
{
    (void)amx;
    g_engfuncs.pfnWriteCoord(amx_ctof(params[1]));
    return 1;
}

// 颜色标记定义
#define COLOR_TEAM_DEFAULT 0
#define COLOR_TEAM_GREY    1
#define COLOR_TEAM_RED     2
#define COLOR_TEAM_BLUE    3

// CS SayText 消息协议:
// WriteByte(2)  = SayText 消息类型标记
// WriteByte(sender) = 发送者(0 = 服务器)
// WriteString(msg) = 消息内容 (支持 ^1 ^3 ^4 颜色代码)
// WriteByte(1) = 消息可见性

cell AMX_NATIVE_CALL amxx_client_print_color(AMX *amx, cell *params)
{
    // P0-6 修复: 对齐原版 AMXX client_print_color 行为
    // 原版 amxmodx.cpp L377-L460
    int index = (int)params[1];
    int sender = (int)params[2];

    if (index < 0 || index > gpGlobals->maxClients)
        return 0;

    // 原版 print_team 常量: default=-1, grey=-2, red=-3, blue=-4
    // sender < print_team_blue(-4) || sender > maxClients → default(-1)
    // sender < print_team_default(-1) → abs(sender)+32 (对齐 TeamInfo 索引)
    if (sender < -4 || sender > gpGlobals->maxClients)
        sender = -1; // print_team_default
    else if (sender < -1)
        sender = abs(sender) + 32;

    // 格式化消息
    char msg[256];
    amxx_format_string(amx, params, 3, msg, sizeof(msg));
    int len = (int)strlen(msg);

    // 原版: 首字节 > 4 时前缀 \x01 (否则颜色码不生效)
    if ((unsigned char)*msg > 4) {
        memmove(msg + 1, msg, len < 254 ? len + 1 : 255);
        *msg = 1; // \x01
        len++;
    }

    // 原版: 最大 187 字节 + UTF-8 截断防乱码
    if (len > 187) {
        len = 187;
        if (msg[len - 1] & 0x80) {
            // 回退到最后一个完整 UTF-8 字符
            while (len > 0 && (msg[len - 1] & 0xC0) == 0x80)
                len--;
            if (len > 0)
                len--;
        }
    }
    msg[len] = 0;

    int msgSayText = REG_USER_MSG("SayText", -1);

    if (index == 0) {
        for (int i = 1; i <= gpGlobals->maxClients; i++) {
            edict_t *p = INDEXENT(i);
            if (!p || p->free) continue;
            CBasePlayer *pPlayer = GET_PRIVATE<CBasePlayer>(p);
            if (!pPlayer || !pPlayer->IsNetClient() || pPlayer->IsBot())
                continue;

            int actualSender = (sender != -1) ? sender : i;
            g_engfuncs.pfnMessageBegin(MSG_ONE, msgSayText, nullptr, p);
            g_engfuncs.pfnWriteByte(actualSender);
            g_engfuncs.pfnWriteString(msg);
            g_engfuncs.pfnWriteByte(1);
            g_engfuncs.pfnMessageEnd();
        }
    } else {
        edict_t *pEdict = INDEXENT(index);
        if (!pEdict || pEdict->free)
            return 0;

        int actualSender = (sender != -1) ? sender : index;
        g_engfuncs.pfnMessageBegin(MSG_ONE, msgSayText, nullptr, pEdict);
        g_engfuncs.pfnWriteByte(actualSender);
        g_engfuncs.pfnWriteString(msg);
        g_engfuncs.pfnWriteByte(1);
        g_engfuncs.pfnMessageEnd();
    }
    return 1;
}

cell AMX_NATIVE_CALL amxx_server_exec(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    g_engfuncs.pfnServerExecute();
    return 1;
}

cell AMX_NATIVE_CALL amxx_set_task_ex(AMX *amx, cell *params)
{
    AMXXRuntime &runtime = AMXXRuntime::GetInstance();
    AMXXTaskManager &tm = runtime.GetTaskManager();

    float delay = amx_ctof(params[1]);
    cell *funcname_addr;
    amx_GetAddr(amx, params[2], &funcname_addr);
    char funcname[64];
    amx_GetString(funcname, funcname_addr, 0, sizeof(funcname));

    AMXXPlugin *plugin = nullptr;
    for (size_t i = 0; i < runtime.GetPlugins().size(); i++) {
        if (runtime.GetPlugins()[i]->GetAMX() == amx) {
            plugin = runtime.GetPlugins()[i];
            break;
        }
    }

    if (!plugin)
        return -1;

    // 查找函数索引
    int funcIndex = plugin->FindPublic(funcname);
    if (funcIndex < 0) {
        AMXX_LOG("[set_task_ex] Function '%s' not found", funcname);
        return -1;
    }

    cell taskId = params[3];
    bool repeat = false;
    float interval = 0.0f;
    if (params[0] / sizeof(cell) > 4)
        repeat = (params[4] != 0);
    if (params[0] / sizeof(cell) > 5)
        interval = amx_ctof(params[5]);

    // 确定 flags 和 repeat 参数
    int flags = 0;
    int repeatCount = 0;
    float base = delay;
    
    if (repeat && interval > 0) {
        // 无限循环任务
        flags = 2;  // 'b' flag
        repeatCount = -1;
        base = interval;
    }

    // 处理任务参数
    int numTaskParams = (params[0] / sizeof(cell)) - 5;
    int paramLen = 0;
    const cell *paramAddr = nullptr;
    
    if (numTaskParams > 0) {
        paramLen = numTaskParams * sizeof(cell);
        cell *addr;
        amx_GetAddr(amx, params[6], &addr);
        paramAddr = addr;
    }

    tm.CreateTask(plugin, amx, funcIndex, flags, taskId, base, paramLen, paramAddr, repeatCount);
    return 1;
}

cell AMX_NATIVE_CALL amxx_remove_tasks(AMX *amx, cell *params)
{
    AMXXRuntime &runtime = AMXXRuntime::GetInstance();
    AMXXTaskManager &tm = runtime.GetTaskManager();
    
    AMXXPlugin *plugin = nullptr;
    for (size_t i = 0; i < runtime.GetPlugins().size(); i++) {
        if (runtime.GetPlugins()[i]->GetAMX() == amx) {
            plugin = runtime.GetPlugins()[i];
            break;
        }
    }
    
    if (!plugin)
        return 0;
    
    return tm.RemoveTasksByPlugin(plugin) ? 1 : 0;
}

// ===== 字符串操作扩展 =====

cell AMX_NATIVE_CALL amxx_strtoupper(AMX *amx, cell *params)
{
    cell *addr;
    amx_GetAddr(amx, params[1], &addr);
    char str[4096];
    amx_GetString(str, addr, 0, sizeof(str));
    
    for (size_t i = 0; str[i]; i++)
        str[i] = toupper(str[i]);
    
    amx_SetString(addr, str, 0, 0, params[2]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_strtolower(AMX *amx, cell *params)
{
    cell *addr;
    amx_GetAddr(amx, params[1], &addr);
    char str[4096];
    amx_GetString(str, addr, 0, sizeof(str));
    
    for (size_t i = 0; str[i]; i++)
        str[i] = tolower(str[i]);
    
    amx_SetString(addr, str, 0, 0, params[2]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_strreplace(AMX *amx, cell *params)
{
    cell *str_addr, *search_addr, *replace_addr;
    amx_GetAddr(amx, params[1], &str_addr);
    amx_GetAddr(amx, params[2], &search_addr);
    amx_GetAddr(amx, params[3], &replace_addr);
    
    char str[4096], search[256], replace[256];
    amx_GetString(str, str_addr, 0, sizeof(str));
    amx_GetString(search, search_addr, 0, sizeof(search));
    amx_GetString(replace, replace_addr, 0, sizeof(replace));
    
    char result[4096] = "";
    char *ptr = str;
    char *pos = nullptr;
    size_t search_len = strlen(search);
    
    if (search_len == 0) {
        amx_SetString(str_addr, str, 0, 0, params[4]);
        return 1;
    }
    
    while ((pos = strstr(ptr, search)) != nullptr) {
        strncat(result, ptr, pos - ptr);
        strcat(result, replace);
        ptr = pos + search_len;
    }
    strcat(result, ptr);
    
    amx_SetString(str_addr, result, 0, 0, params[4]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_strins(AMX *amx, cell *params)
{
    cell *str_addr, *ins_addr;
    amx_GetAddr(amx, params[1], &str_addr);
    amx_GetAddr(amx, params[3], &ins_addr);
    
    char str[4096], ins[256];
    amx_GetString(str, str_addr, 0, sizeof(str));
    amx_GetString(ins, ins_addr, 0, sizeof(ins));
    
    int pos = params[2];
    size_t str_len = strlen(str);
    
    if (pos < 0) pos = 0;
    if (pos > (int)str_len) pos = str_len;
    
    char result[4096];
    strncpy(result, str, pos);
    result[pos] = '\0';
    strcat(result, ins);
    strcat(result, str + pos);
    
    amx_SetString(str_addr, result, 0, 0, params[4]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_strdel(AMX *amx, cell *params)
{
    cell *str_addr;
    amx_GetAddr(amx, params[1], &str_addr);
    
    char str[4096];
    amx_GetString(str, str_addr, 0, sizeof(str));
    
    int start = params[2];
    int count = params[3];
    size_t str_len = strlen(str);
    
    if (start < 0) start = 0;
    if (start >= (int)str_len) {
        amx_SetString(str_addr, str, 0, 0, params[4]);
        return 1;
    }
    
    if (count < 0 || start + count > (int)str_len)
        count = str_len - start;
    
    memmove(str + start, str + start + count, str_len - start - count + 1);
    
    amx_SetString(str_addr, str, 0, 0, params[4]);
    return 1;
}

// ===== 数学运算 =====

cell AMX_NATIVE_CALL amxx_floatadd(AMX *amx, cell *params)
{
    (void)amx;
    float result = amx_ctof(params[1]) + amx_ctof(params[2]);
    return amx_ftoc(result);
}

cell AMX_NATIVE_CALL amxx_floatmul(AMX *amx, cell *params)
{
    (void)amx;
    float result = amx_ctof(params[1]) * amx_ctof(params[2]);
    return amx_ftoc(result);
}

cell AMX_NATIVE_CALL amxx_floatdiv(AMX *amx, cell *params)
{
    (void)amx;
    float divisor = amx_ctof(params[2]);
    if (divisor == 0.0f) {
        float zero = 0.0f;
        return amx_ftoc(zero);
    }
    float result = amx_ctof(params[1]) / divisor;
    return amx_ftoc(result);
}

cell AMX_NATIVE_CALL amxx_floatsub(AMX *amx, cell *params)
{
    (void)amx;
    float result = amx_ctof(params[1]) - amx_ctof(params[2]);
    return amx_ftoc(result);
}

cell AMX_NATIVE_CALL amxx_floatmod(AMX *amx, cell *params)
{
    (void)amx;
    float divisor = amx_ctof(params[2]);
    if (divisor == 0.0f) {
        float zero = 0.0f;
        return amx_ftoc(zero);
    }
    float result = fmod(amx_ctof(params[1]), divisor);
    return amx_ftoc(result);
}

// ===== 位操作 =====

cell AMX_NATIVE_CALL amxx_bit(AMX *amx, cell *params)
{
    (void)amx;
    return (cell)(1 << params[1]);
}

cell AMX_NATIVE_CALL amxx_bits(AMX *amx, cell *params)
{
    (void)amx;
    int count = 0;
    cell value = params[1];
    while (value) {
        count += value & 1;
        value >>= 1;
    }
    return count;
}

cell AMX_NATIVE_CALL amxx_bit_set(AMX *amx, cell *params)
{
    (void)amx;
    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    *dest |= params[2];
    return 1;
}

cell AMX_NATIVE_CALL amxx_bit_get(AMX *amx, cell *params)
{
    (void)amx;
    cell *src;
    amx_GetAddr(amx, params[1], &src);
    return (*src & params[2]) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_bit_test(AMX *amx, cell *params)
{
    (void)amx;
    return (params[1] & params[2]) ? 1 : 0;
}

// ===== 文件系统 =====

cell AMX_NATIVE_CALL amxx_file_exists(AMX *amx, cell *params)
{
    cell *filename_addr;
    amx_GetAddr(amx, params[1], &filename_addr);
    char filename[512];
    amx_GetString(filename, filename_addr, 0, sizeof(filename));
    
    FILE *fp = fopen(filename, "r");
    if (fp) {
        fclose(fp);
        return 1;
    }
    return 0;
}

cell AMX_NATIVE_CALL amxx_file_delete(AMX *amx, cell *params)
{
    cell *filename_addr;
    amx_GetAddr(amx, params[1], &filename_addr);
    char filename[512];
    amx_GetString(filename, filename_addr, 0, sizeof(filename));
    
    return remove(filename) == 0 ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_mkdir(AMX *amx, cell *params)
{
    cell *dirname_addr;
    amx_GetAddr(amx, params[1], &dirname_addr);
    char dirname[512];
    amx_GetString(dirname, dirname_addr, 0, sizeof(dirname));
    
#ifdef _WIN32
    return mkdir(dirname) == 0 ? 1 : 0;
#else
    return mkdir(dirname, 0755) == 0 ? 1 : 0;
#endif
}

// read_dir(dirname, output[], len, &pos) — 原版 AMXX read_dir 语义:
// 返回 1 表示成功读取一个条目，0 表示无更多条目
// pos 为迭代游标，插件首次传入 0，之后传入返回的 pos 继续遍历
cell AMX_NATIVE_CALL amxx_read_dir(AMX *amx, cell *params)
{
    cell *dirname_addr;
    amx_GetAddr(amx, params[1], &dirname_addr);
    char dirname[512];
    amx_GetString(dirname, dirname_addr, 0, sizeof(dirname));

    cell *out_addr;
    amx_GetAddr(amx, params[2], &out_addr);
    int maxlen = (int)params[3];

    cell *pos_addr;
    amx_GetAddr(amx, params[4], &pos_addr);
    int pos = (int)*pos_addr;

#ifdef _WIN32
    struct _finddata_t fileinfo;
    intptr_t handle;
    char pattern[600];
    snprintf(pattern, sizeof(pattern), "%s\\*", dirname);

    // 首次调用：_findfirst
    if (pos == 0) {
        handle = _findfirst(pattern, &fileinfo);
        if (handle == -1) return 0;
    } else {
        handle = (intptr_t)pos;
        if (_findnext(handle, &fileinfo) != 0) {
            _findclose(handle);
            return 0;
        }
    }

    amx_SetString(out_addr, fileinfo.name, 0, 0, maxlen);
    // 返回 handle 作为下次的 pos（非零），并用 *pos_addr 传回
    *pos_addr = (cell)handle;
    return 1;
#else
    DIR *dir = (DIR *)(intptr_t)pos;
    if (!dir) {
        dir = opendir(dirname);
        if (!dir) return 0;
    }

    struct dirent *entry = readdir(dir);
    if (!entry) {
        closedir(dir);
        return 0;
    }

    amx_SetString(out_addr, entry->d_name, 0, 0, maxlen);
    *pos_addr = (cell)(intptr_t)dir;
    return 1;
#endif
}

// delete_dir(dirname) — 递归删除目录（原版 AMXX 无此 native，但常被插件需要）
cell AMX_NATIVE_CALL amxx_delete_dir(AMX *amx, cell *params)
{
    cell *dirname_addr;
    amx_GetAddr(amx, params[1], &dirname_addr);
    char dirname[512];
    amx_GetString(dirname, dirname_addr, 0, sizeof(dirname));

#ifdef _WIN32
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rmdir /S /Q \"%s\"", dirname);
    return system(cmd) == 0 ? 1 : 0;
#else
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", dirname);
    return system(cmd) == 0 ? 1 : 0;
#endif
}

// rename_file(oldname[], newname[]) — 重命名/移动文件
cell AMX_NATIVE_CALL amxx_rename_file(AMX *amx, cell *params)
{
    cell *old_addr, *new_addr;
    amx_GetAddr(amx, params[1], &old_addr);
    amx_GetAddr(amx, params[2], &new_addr);
    char oldname[512], newname[512];
    amx_GetString(oldname, old_addr, 0, sizeof(oldname));
    amx_GetString(newname, new_addr, 0, sizeof(newname));

    return rename(oldname, newname) == 0 ? 1 : 0;
}

// ===== 字符串操作扩展 =====

cell AMX_NATIVE_CALL amxx_copy(AMX *amx, cell *params)
{
    cell *dest_addr, *src_addr;
    amx_GetAddr(amx, params[1], &dest_addr);
    amx_GetAddr(amx, params[2], &src_addr);
    
    char src[4096];
    amx_GetString(src, src_addr, 0, sizeof(src));
    
    amx_SetString(dest_addr, src, 0, 0, params[3]);
    return strlen(src);
}

cell AMX_NATIVE_CALL amxx_add(AMX *amx, cell *params)
{
    cell *dest_addr, *src_addr;
    amx_GetAddr(amx, params[1], &dest_addr);
    amx_GetAddr(amx, params[2], &src_addr);
    
    char dest[4096], src[4096];
    amx_GetString(dest, dest_addr, 0, sizeof(dest));
    amx_GetString(src, src_addr, 0, sizeof(src));
    
    strncat(dest, src, params[3] - strlen(dest) - 1);
    amx_SetString(dest_addr, dest, 0, 0, params[3]);
    return strlen(dest);
}

cell AMX_NATIVE_CALL amxx_num_to_str(AMX *amx, cell *params)
{
    cell *str_addr;
    amx_GetAddr(amx, params[2], &str_addr);
    
    char str[64];
    snprintf(str, sizeof(str), "%d", params[1]);
    
    amx_SetString(str_addr, str, 0, 0, params[3]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_str_to_num(AMX *amx, cell *params)
{
    cell *str_addr;
    amx_GetAddr(amx, params[1], &str_addr);
    
    char str[64];
    amx_GetString(str, str_addr, 0, sizeof(str));
    
    return atoi(str);
}

cell AMX_NATIVE_CALL amxx_float_to_str(AMX *amx, cell *params)
{
    cell *str_addr;
    amx_GetAddr(amx, params[2], &str_addr);
    
    char str[64];
    snprintf(str, sizeof(str), "%.6f", amx_ctof(params[1]));
    
    amx_SetString(str_addr, str, 0, 0, params[3]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_str_to_float(AMX *amx, cell *params)
{
    cell *str_addr;
    amx_GetAddr(amx, params[1], &str_addr);

    char str[64];
    amx_GetString(str, str_addr, 0, sizeof(str));

    float value = (float)atof(str);
    return amx_ftoc(value);
}

// floatstr(const string[]) — 字符串转浮点 (原版 float.inc 中 str_to_float 的别名)
// 与 amxx_str_to_float 行为完全一致, 仅注册名不同
cell AMX_NATIVE_CALL amxx_floatstr(AMX *amx, cell *params)
{
    cell *str_addr;
    amx_GetAddr(amx, params[1], &str_addr);

    char str[64];
    amx_GetString(str, str_addr, 0, sizeof(str));

    float value = (float)atof(str);
    return amx_ftoc(value);
}

// floatfract(Float:value) — 返回浮点数的小数部分
// 原版 float.inc: floatfract(v) = v - floatfloor(v)
cell AMX_NATIVE_CALL amxx_floatfract(AMX *amx, cell *params)
{
    (void)amx;
    float v = amx_ctof(params[1]);
    float fract = v - (float)floor(v);
    return amx_ftoc(fract);
}

cell AMX_NATIVE_CALL amxx_trim(AMX *amx, cell *params)
{
    cell *str_addr;
    amx_GetAddr(amx, params[1], &str_addr);
    
    char str[4096];
    amx_GetString(str, str_addr, 0, sizeof(str));
    
    char *start = str;
    while (isspace((unsigned char)*start)) start++;
    
    char *end = str + strlen(str) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    
    amx_SetString(str_addr, start, 0, 0, params[2]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_containi(AMX *amx, cell *params)
{
    cell *str_addr, *sub_addr;
    amx_GetAddr(amx, params[1], &str_addr);
    amx_GetAddr(amx, params[2], &sub_addr);
    
    char str[4096], sub[4096];
    amx_GetString(str, str_addr, 0, sizeof(str));
    amx_GetString(sub, sub_addr, 0, sizeof(sub));
    
    size_t str_len = strlen(str);
    size_t sub_len = strlen(sub);
    
    if (sub_len == 0) return 0;
    if (sub_len > str_len) return -1;
    
    for (size_t i = 0; i <= str_len - sub_len; i++) {
        if (_strnicmp(str + i, sub, sub_len) == 0)
            return (cell)i;
    }
    return -1;
}

cell AMX_NATIVE_CALL amxx_equali(AMX *amx, cell *params)
{
    cell *str1_addr, *str2_addr;
    amx_GetAddr(amx, params[1], &str1_addr);
    amx_GetAddr(amx, params[2], &str2_addr);
    
    char str1[4096], str2[4096];
    amx_GetString(str1, str1_addr, 0, sizeof(str1));
    amx_GetString(str2, str2_addr, 0, sizeof(str2));
    
    return _stricmp(str1, str2) == 0 ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_strncmp(AMX *amx, cell *params)
{
    cell *str1_addr, *str2_addr;
    amx_GetAddr(amx, params[1], &str1_addr);
    amx_GetAddr(amx, params[2], &str2_addr);
    
    char str1[4096], str2[4096];
    amx_GetString(str1, str1_addr, 0, sizeof(str1));
    amx_GetString(str2, str2_addr, 0, sizeof(str2));
    
    return strncmp(str1, str2, params[3]);
}

// ===== 数学运算扩展 =====

cell AMX_NATIVE_CALL amxx_abs(AMX *amx, cell *params)
{
    (void)amx;
    return abs(params[1]);
}

// power(value, exponent) - 整数幂 (对齐原版 core.inc)
cell AMX_NATIVE_CALL amxx_power(AMX *amx, cell *params)
{
    (void)amx;
    cell base = params[1];
    cell exp = params[2];
    cell result = 1;
    while (exp > 0) {
        if (exp & 1)
            result *= base;
        base *= base;
        exp >>= 1;
    }
    return result;
}

// sqroot(value) - 整数平方根 (对齐原版 core.inc)
cell AMX_NATIVE_CALL amxx_sqroot(AMX *amx, cell *params)
{
    (void)amx;
    cell value = params[1];
    if (value < 0)
        return 0;
    cell result = (cell)sqrt((double)value);
    return result;
}

// tickcount(&granularity=0) - 引擎 tick 计数 (对齐原版 core.inc)
cell AMX_NATIVE_CALL amxx_tickcount(AMX *amx, cell *params)
{
    cell *granularity = nullptr;
    if (params[0] / sizeof(cell) >= 1)
        amx_GetAddr(amx, params[1], &granularity);
    if (granularity)
        *granularity = 1000; // ~1000 ticks/sec
#ifdef _WIN32
    return (cell)GetTickCount();
#else
    return (cell)(gpGlobals->time * 1000.0f);
#endif
}

// time(&hour=0, &minute=0, &second=0) - 当前时间 (对齐原版 core.inc)
cell AMX_NATIVE_CALL amxx_time(AMX *amx, cell *params)
{
    time_t now = time(nullptr);
    struct tm *lt = localtime(&now);
    if (!lt)
        return 0;
    cell *addr;
    int numParams = (int)(params[0] / sizeof(cell));
    if (numParams >= 1) { amx_GetAddr(amx, params[1], &addr); if (addr) *addr = lt->tm_hour; }
    if (numParams >= 2) { amx_GetAddr(amx, params[2], &addr); if (addr) *addr = lt->tm_min; }
    if (numParams >= 3) { amx_GetAddr(amx, params[3], &addr); if (addr) *addr = lt->tm_sec; }
    return (cell)now;
}

// date(&year=0, &month=0, &day=0) - 当前日期 (对齐原版 core.inc)
cell AMX_NATIVE_CALL amxx_date(AMX *amx, cell *params)
{
    time_t now = time(nullptr);
    struct tm *lt = localtime(&now);
    if (!lt)
        return 0;
    cell *addr;
    int numParams = (int)(params[0] / sizeof(cell));
    if (numParams >= 1) { amx_GetAddr(amx, params[1], &addr); if (addr) *addr = lt->tm_year + 1900; }
    if (numParams >= 2) { amx_GetAddr(amx, params[2], &addr); if (addr) *addr = lt->tm_mon + 1; }
    if (numParams >= 3) { amx_GetAddr(amx, params[3], &addr); if (addr) *addr = lt->tm_mday; }
    return (cell)now;
}

cell AMX_NATIVE_CALL amxx_ceil(AMX *amx, cell *params)
{
    (void)amx;
    return (cell)ceil(amx_ctof(params[1]));
}

cell AMX_NATIVE_CALL amxx_floor(AMX *amx, cell *params)
{
    (void)amx;
    return (cell)floor(amx_ctof(params[1]));
}

cell AMX_NATIVE_CALL amxx_pow(AMX *amx, cell *params)
{
    (void)amx;
    float result = pow(amx_ctof(params[1]), amx_ctof(params[2]));
    return amx_ftoc(result);
}

cell AMX_NATIVE_CALL amxx_sqrt(AMX *amx, cell *params)
{
    (void)amx;
    float result = sqrt(amx_ctof(params[1]));
    return amx_ftoc(result);
}

cell AMX_NATIVE_CALL amxx_log(AMX *amx, cell *params)
{
    (void)amx;
    float val = amx_ctof(params[1]);
    if (val <= 0.0f) {
        float zero = 0.0f;
        return amx_ftoc(zero);
    }
    float result = log(val);
    return amx_ftoc(result);
}

cell AMX_NATIVE_CALL amxx_log10(AMX *amx, cell *params)
{
    (void)amx;
    float val = amx_ctof(params[1]);
    if (val <= 0.0f) {
        float zero = 0.0f;
        return amx_ftoc(zero);
    }
    float result = log10(val);
    return amx_ftoc(result);
}

// ===== 位操作扩展 =====

cell AMX_NATIVE_CALL amxx_bit_reset(AMX *amx, cell *params)
{
    (void)amx;
    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    *dest &= ~params[2];
    return 1;
}

cell AMX_NATIVE_CALL amxx_bit_flip(AMX *amx, cell *params)
{
    (void)amx;
    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    *dest ^= params[2];
    return 1;
}

// ===== AMX参数操作 =====

cell AMX_NATIVE_CALL amxx_set_string(AMX *amx, cell *params)
{
    cell *str_addr;
    amx_GetAddr(amx, params[2], &str_addr);
    
    char str[4096];
    amx_GetString(str, str_addr, 0, sizeof(str));
    
    cell *param_addr = (cell *)((char *)amx->base + amx->stk + params[1] * sizeof(cell));
    amx_SetString(param_addr, str, 0, 0, params[3]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_set_param_byref(AMX *amx, cell *params)
{
    (void)amx;
    cell *param_addr = (cell *)((char *)amx->base + amx->stk + params[1] * sizeof(cell));
    *param_addr = params[2];
    return 1;
}

cell AMX_NATIVE_CALL amxx_get_param_byref(AMX *amx, cell *params)
{
    (void)amx;
    cell *param_addr = (cell *)((char *)amx->base + amx->stk + params[1] * sizeof(cell));
    return *param_addr;
}

cell AMX_NATIVE_CALL amxx_get_float_byref(AMX *amx, cell *params)
{
    (void)amx;
    cell *param_addr = (cell *)((char *)amx->base + amx->stk + params[1] * sizeof(cell));
    return *param_addr;
}

cell AMX_NATIVE_CALL amxx_set_float_byref(AMX *amx, cell *params)
{
    (void)amx;
    cell *param_addr = (cell *)((char *)amx->base + amx->stk + params[1] * sizeof(cell));
    *param_addr = params[2];
    return 1;
}

// ===== 数据结构 =====

cell AMX_NATIVE_CALL amxx_arrayset(AMX *amx, cell *params)
{
    (void)amx;
    cell *array_addr;
    amx_GetAddr(amx, params[1], &array_addr);
    
    cell value = params[2];
    int size = params[3];
    
    for (int i = 0; i < size; i++) {
        array_addr[i] = value;
    }
    return 1;
}

// ===== 函数调用 =====

static cell g_callfunc_result = 0;
static int g_callfunc_params = 0;
static cell *g_callfunc_argv = nullptr;
static AMX *g_callfunc_amx = nullptr;
static int g_callfunc_funcidx = -1;

cell AMX_NATIVE_CALL amxx_callfunc_begin(AMX *amx, cell *params)
{
    (void)amx;
    if (g_callfunc_amx) {
        // 原版: 未先 callfunc_end 又调用 begin，视为脚本错误 (返回 0)
        return 0;
    }

    cell *func_addr, *plugin_addr;
    amx_GetAddr(amx, params[1], &func_addr);
    amx_GetAddr(amx, params[2], &plugin_addr);
    
    char func[256], plugin[256];
    amx_GetString(func, func_addr, 0, sizeof(func));
    amx_GetString(plugin, plugin_addr, 0, sizeof(plugin));
    
    AMXXRuntime &runtime = AMXXRuntime::GetInstance();
    g_callfunc_amx = nullptr;
    
    for (size_t i = 0; i < runtime.GetPlugins().size(); i++) {
        // 插件按基础文件名匹配（amxmisc.inc 的 get_plugin 返回 GetName() 基础名，
        // 不能与 GetFilename() 完整路径比较）
        if (strcmp(runtime.GetPlugins()[i]->GetName(), plugin) == 0) {
            g_callfunc_amx = runtime.GetPlugins()[i]->GetAMX();
            break;
        }
    }
    
    if (!g_callfunc_amx) {
        g_callfunc_funcidx = -1;
        return -1;   // 原版: 插件未找到返回 -1
    }
    
    int index;
    if (amx_FindPublic(g_callfunc_amx, func, &index) == AMX_ERR_NONE) {
        g_callfunc_funcidx = index;
    } else {
        g_callfunc_funcidx = -1;
        g_callfunc_amx = nullptr;
        return -2;   // 原版: 函数未找到返回 -2
    }
    g_callfunc_params = 0;
    
    g_callfunc_argv = new cell[32];
    
    return 1;   // 原版: 成功返回 1
}

cell AMX_NATIVE_CALL amxx_callfunc_begin_i(AMX *amx, cell *params)
{
    (void)amx;
    if (g_callfunc_amx) {
        // 原版: 未先 callfunc_end 又调用 begin，视为脚本错误 (返回 0)
        return 0;
    }

    cell *plugin_addr;
    amx_GetAddr(amx, params[2], &plugin_addr);
    
    char plugin[256];
    amx_GetString(plugin, plugin_addr, 0, sizeof(plugin));
    
    AMXXRuntime &runtime = AMXXRuntime::GetInstance();
    g_callfunc_amx = nullptr;
    
    for (size_t i = 0; i < runtime.GetPlugins().size(); i++) {
        // 插件按基础文件名匹配（amxmisc.inc 的 get_plugin 返回 GetName() 基础名，
        // 不能与 GetFilename() 完整路径比较）
        if (strcmp(runtime.GetPlugins()[i]->GetName(), plugin) == 0) {
            g_callfunc_amx = runtime.GetPlugins()[i]->GetAMX();
            break;
        }
    }
    
    if (!g_callfunc_amx) {
        g_callfunc_funcidx = -1;
        return -1;   // 原版: 插件未找到返回 -1
    }
    
    if (params[1] < 0) {
        g_callfunc_funcidx = -1;
        g_callfunc_amx = nullptr;
        return -1;   // 原版: 无效函数索引返回 -1
    }

    // 校验函数索引确实指向一个 public（不可执行则返回 -2）
    char funcname[64];
    if (amx_GetPublic(g_callfunc_amx, (int)params[1], funcname) != AMX_ERR_NONE) {
        g_callfunc_funcidx = -1;
        g_callfunc_amx = nullptr;
        return -2;   // 原版: 函数不可执行返回 -2
    }

    g_callfunc_funcidx = params[1];
    g_callfunc_params = 0;
    
    g_callfunc_argv = new cell[32];
    
    return 1;   // 原版: 成功返回 1
}

cell AMX_NATIVE_CALL amxx_callfunc_end(AMX *amx, cell *params)
{
    (void)amx;
    (void)params; // 原版 callfunc_end 无参数
    if (g_callfunc_amx && g_callfunc_funcidx != -1 && g_callfunc_argv) {
        // 按反序推送参数到 AMX 栈（最后一个参数先 push）
        for (int i = g_callfunc_params - 1; i >= 0; i--) {
            amx_Push(g_callfunc_amx, g_callfunc_argv[i]);
        }

        cell result;
        amx_Exec(g_callfunc_amx, &result, g_callfunc_funcidx);
        g_callfunc_result = result;

        delete[] g_callfunc_argv;
        g_callfunc_argv = nullptr;
    }

    g_callfunc_amx = nullptr;
    g_callfunc_funcidx = -1;
    g_callfunc_params = 0;

    return g_callfunc_result;
}

cell AMX_NATIVE_CALL amxx_callfunc_push_int(AMX *amx, cell *params)
{
    (void)amx;
    if (g_callfunc_argv && g_callfunc_params < 32) {
        g_callfunc_argv[g_callfunc_params++] = params[1];
    }
    return 1;
}

cell AMX_NATIVE_CALL amxx_callfunc_push_float(AMX *amx, cell *params)
{
    (void)amx;
    if (g_callfunc_argv && g_callfunc_params < 32) {
        g_callfunc_argv[g_callfunc_params++] = params[1];
    }
    return 1;
}

cell AMX_NATIVE_CALL amxx_callfunc_push_str(AMX *amx, cell *params)
{
    (void)amx;
    if (g_callfunc_argv && g_callfunc_params < 32) {
        g_callfunc_argv[g_callfunc_params++] = params[1];
    }
    return 1;
}

cell AMX_NATIVE_CALL amxx_callfunc_push_array(AMX *amx, cell *params)
{
    (void)amx;
    cell *array_addr;
    amx_GetAddr(amx, params[1], &array_addr);
    
    if (g_callfunc_argv && g_callfunc_params < 32) {
        g_callfunc_argv[g_callfunc_params++] = params[3];
        for (int i = 0; i < params[2] && g_callfunc_params < 32; i++) {
            g_callfunc_argv[g_callfunc_params++] = array_addr[i];
        }
    }
    return 1;
}

// ===== P1: 服务器信息 =====

cell AMX_NATIVE_CALL amxx_is_dedicated_server(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return IS_DEDICATED_SERVER() ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_is_linux_server(AMX *amx, cell *params)
{
    (void)amx; (void)params;
#ifdef __linux__
    return 1;
#else
    return 0;
#endif
}

cell AMX_NATIVE_CALL amxx_get_amxx_verstring(AMX *amx, cell *params)
{
    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    const char *version = "1.8.2-dev+regamedll";
    return amx_SetString(dest, version, 0, 0, params[2]);
}

cell AMX_NATIVE_CALL amxx_server_name(AMX *amx, cell *params)
{
    (void)amx;
    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    const char *name = CVAR_GET_STRING("hostname");
    if (!name) name = "";
    return amx_SetString(dest, name, 0, 0, params[2]);
}

// ===== P1: 目录路径 =====

cell AMX_NATIVE_CALL amxx_get_configsdir(AMX *amx, cell *params)
{
    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    // 从 localinfo 读取配置目录 (兼容原版 AMX Mod X)
    char *infobuffer = g_engfuncs.pfnGetInfoKeyBuffer(NULL);
    const char *configsdir = g_engfuncs.pfnInfoKeyValue(infobuffer, "amxx_configsdir");
    if (!configsdir || !*configsdir) configsdir = "addons/amxmodx/configs";
    return amx_SetString(dest, configsdir, 0, 0, params[2]);
}

cell AMX_NATIVE_CALL amxx_get_datadir(AMX *amx, cell *params)
{
    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    // 从 localinfo 读取数据目录
    char *infobuffer = g_engfuncs.pfnGetInfoKeyBuffer(NULL);
    const char *datadir = g_engfuncs.pfnInfoKeyValue(infobuffer, "amxx_datadir");
    if (!datadir || !*datadir) datadir = "addons/amxmodx/data";
    return amx_SetString(dest, datadir, 0, 0, params[2]);
}

// ===== P1: localinfo =====

static char g_localinfo[32][256] = {{0}};
static int g_localinfo_count = 0;

cell AMX_NATIVE_CALL amxx_set_localinfo(AMX *amx, cell *params)
{
    cell *key_addr, *val_addr;
    amx_GetAddr(amx, params[1], &key_addr);
    amx_GetAddr(amx, params[2], &val_addr);
    char key[256], value[256];
    amx_GetString(key, key_addr, 0, sizeof(key));
    amx_GetString(value, val_addr, 0, sizeof(value));

    // 设置服务器 localinfo (NULL infobuffer = 服务器 localinfo)
    char *infobuffer = g_engfuncs.pfnGetInfoKeyBuffer(NULL);
    g_engfuncs.pfnSetKeyValue(infobuffer, key, value);

    return 1;
}

cell AMX_NATIVE_CALL amxx_get_localinfo(AMX *amx, cell *params)
{
    cell *key_addr, *dest;
    amx_GetAddr(amx, params[1], &key_addr);
    amx_GetAddr(amx, params[2], &dest);
    char key[256];
    amx_GetString(key, key_addr, 0, sizeof(key));

    // 读取服务器 localinfo (NULL infobuffer = 服务器 localinfo)
    char *infobuffer = g_engfuncs.pfnGetInfoKeyBuffer(NULL);
    const char *value = g_engfuncs.pfnInfoKeyValue(infobuffer, key);
    if (!value) value = "";
    return amx_SetString(dest, value, 0, 0, params[3]);
}

// ===== P1: 音效 =====

cell AMX_NATIVE_CALL amxx_emit_sound(AMX *amx, cell *params)
{
    int index = (int)params[1];
    int channel = (int)params[2];
    int numParams = (int)(params[0] / sizeof(cell));

    cell *sample_addr;
    amx_GetAddr(amx, params[3], &sample_addr);
    char sample[256];
    amx_GetString(sample, sample_addr, 0, sizeof(sample));

    float volume = 1.0f;
    float attenuation = 1.0f;
    int flags = 0;
    int pitch = 100;

    if (numParams >= 4) volume = amx_ctof(params[4]);
    if (numParams >= 5) attenuation = amx_ctof(params[5]);
    if (numParams >= 6) flags = (int)params[6];
    if (numParams >= 7) pitch = (int)params[7];

    if (index == 0) {
        EMIT_SOUND_DYN2(nullptr, channel, sample, volume, attenuation, flags, pitch);
    } else {
        if (index < 1 || index > gpGlobals->maxClients)
            return 0;
        edict_t *pEdict = INDEXENT(index);
        if (!pEdict || pEdict->free)
            return 0;
        EMIT_SOUND_DYN2(pEdict, channel, sample, volume, attenuation, flags, pitch);
    }
    return 1;
}

// ===== P1: CVar 增强 =====

cell AMX_NATIVE_CALL amxx_get_cvar_flags(AMX *amx, cell *params)
{
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char name[64];
    amx_GetString(name, name_addr, 0, sizeof(name));

    cvar_t *cvar = g_engfuncs.pfnCVarGetPointer(name);
    if (!cvar)
        return -1;
    return (cell)cvar->flags;
}

cell AMX_NATIVE_CALL amxx_set_cvar_flags(AMX *amx, cell *params)
{
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char name[64];
    amx_GetString(name, name_addr, 0, sizeof(name));
    int flags = (int)params[2];

    cvar_t *cvar = g_engfuncs.pfnCVarGetPointer(name);
    if (!cvar)
        return 0;
    cvar->flags = flags;
    return 1;
}

cell AMX_NATIVE_CALL amxx_get_cvar_pointer(AMX *amx, cell *params)
{
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char name[64];
    amx_GetString(name, name_addr, 0, sizeof(name));

    cvar_t *cvar = g_engfuncs.pfnCVarGetPointer(name);
    if (!cvar)
        return 0;
    return (cell)(intptr_t)cvar;
}

cell AMX_NATIVE_CALL amxx_query_client_cvar(AMX *amx, cell *params)
{
    int index = (int)params[1];
    cell *cvar_addr;
    amx_GetAddr(amx, params[2], &cvar_addr);
    char cvar[64];
    amx_GetString(cvar, cvar_addr, 0, sizeof(cvar));

    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free)
        return 0;

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%s\n", cvar);
    CLIENT_COMMAND(pEdict, cmd);
    return 1;
}

// ===== P1: 插件管理 =====

cell AMX_NATIVE_CALL amxx_plugin_flags(AMX *amx, cell *params)
{
    (void)params;
    AMXXRuntime &runtime = AMXXRuntime::GetInstance();
    for (size_t i = 0; i < runtime.GetPlugins().size(); i++) {
        if (runtime.GetPlugins()[i]->GetAMX() == amx) {
            return runtime.GetPlugins()[i]->GetFlags();
        }
    }
    return 0;
}

cell AMX_NATIVE_CALL amxx_module_exists(AMX *amx, cell *params)
{
    (void)amx;
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char name[256];
    amx_GetString(name, name_addr, 0, sizeof(name));

    // P0-9: 补全模块列表（区分大小写匹配，列出所有已静态编译的模块）
    static const char *s_builtInModules[] = {
        "amxmodx", "AMXModX", "amxmod", "AMXMod",
        "cstrike", "CStrike", "csx", "CSX",
        "fun", "Fun",
        "engine", "Engine",
        "fakemeta", "FakeMeta",
        "hamsandwich", "HamSandwich",
        "nvault", "NVault",
        "regex", "Regex",
        "sqlx", "SQLx", "sqlite", "SQLite",
        "dbi", "DBI",
        nullptr
    };
    for (int i = 0; s_builtInModules[i]; i++) {
        if (strcmp(name, s_builtInModules[i]) == 0)
            return 1;
    }
    return 0;
}

cell AMX_NATIVE_CALL amxx_get_plugins(AMX *amx, cell *params)
{
    // P1-6 修复: 对齐原版签名 get_plugins(num, name[], namelen, version[], verlen, author[], authlen, status[], statlen)
    AMXXRuntime &runtime = AMXXRuntime::GetInstance();
    int num = (int)params[1];

    int count = (int)runtime.GetPlugins().size();
    if (num < 0 || num >= count)
        return count; // 返回插件总数

    AMXXPlugin *plugin = runtime.GetPlugins()[num];
    if (!plugin)
        return count;

    // params[2]=name[], params[3]=namelen
    if (params[3] > 0) {
        cell *dest;
        amx_GetAddr(amx, params[2], &dest);
        amx_SetString(dest, plugin->GetTitle() ? plugin->GetTitle() : "", 0, 0, (int)params[3]);
    }
    // params[4]=version[], params[5]=verlen
    if (params[5] > 0) {
        cell *dest;
        amx_GetAddr(amx, params[4], &dest);
        amx_SetString(dest, plugin->GetVersion() ? plugin->GetVersion() : "", 0, 0, (int)params[5]);
    }
    // params[6]=author[], params[7]=authlen
    if (params[7] > 0) {
        cell *dest;
        amx_GetAddr(amx, params[6], &dest);
        amx_SetString(dest, plugin->GetAuthor() ? plugin->GetAuthor() : "", 0, 0, (int)params[7]);
    }
    // params[8]=status[], params[9]=statlen
    if (params[9] > 0) {
        cell *dest;
        amx_GetAddr(amx, params[8], &dest);
        const char *status = plugin->IsLoaded() ? "running" : "stopped";
        amx_SetString(dest, status, 0, 0, (int)params[9]);
    }

    return count;
}

// ===== P1: 字符串工具 =====

cell AMX_NATIVE_CALL amxx_replace_string(AMX *amx, cell *params)
{
    cell *str_addr, *search_addr, *replace_addr;
    amx_GetAddr(amx, params[1], &str_addr);
    amx_GetAddr(amx, params[2], &search_addr);
    amx_GetAddr(amx, params[3], &replace_addr);

    char str[4096], search[256], replace[256];
    amx_GetString(str, str_addr, 0, sizeof(str));
    amx_GetString(search, search_addr, 0, sizeof(search));
    amx_GetString(replace, replace_addr, 0, sizeof(replace));

    int searchLen = (int)params[4];
    if (searchLen <= 0 || searchLen > (int)strlen(search))
        searchLen = (int)strlen(search);
    char searchTerm[257];
    strncpy(searchTerm, search, searchLen);
    searchTerm[searchLen] = '\0';

    bool caseSensitive = true;
    int numParams = (int)(params[0] / sizeof(cell));
    if (numParams >= 5)
        caseSensitive = (params[5] != 0);

    char result[4096] = "";
    char *ptr = str;

    if (caseSensitive) {
        char *pos;
        while ((pos = strstr(ptr, searchTerm)) != nullptr) {
            strncat(result, ptr, pos - ptr);
            strcat(result, replace);
            ptr = pos + searchLen;
        }
    } else {
        // 大小写不敏感的搜索
        int remaining = (int)strlen(ptr);
        while (remaining >= searchLen) {
            if (_strnicmp(ptr, searchTerm, searchLen) == 0) {
                strcat(result, replace);
                ptr += searchLen;
                remaining -= searchLen;
            } else {
                strncat(result, ptr, 1);
                ptr++;
                remaining--;
            }
        }
    }
    strcat(result, ptr);

    return amx_SetString(str_addr, result, 0, 0, params[6]);
}

cell AMX_NATIVE_CALL amxx_replace_all(AMX *amx, cell *params)
{
    // replace_all 是 replace_string 的别名，替换所有匹配
    return amxx_replace_string(amx, params);
}

cell AMX_NATIVE_CALL amxx_parse(AMX *amx, cell *params)
{
    cell *text_addr;
    amx_GetAddr(amx, params[1], &text_addr);
    char text[4096];
    amx_GetString(text, text_addr, 0, sizeof(text));

    cell *output_addr;
    amx_GetAddr(amx, params[2], &output_addr);
    int outputSize = (int)params[3];

    cell *split_addr;
    amx_GetAddr(amx, params[4], &split_addr);
    char split[32];
    amx_GetString(split, split_addr, 0, sizeof(split));

    // P1-7 修复: 支持引号包含的整串、反斜杠转义
    char delimiter = split[0];
    int tokenCount = 0;
    int i = 0;
    int len = (int)strlen(text);

    while (i < len && tokenCount < outputSize) {
        // 跳过前导分隔符
        while (i < len && text[i] == delimiter)
            i++;
        if (i >= len)
            break;

        char token[512];
        int tlen = 0;

        if (text[i] == '"') {
            // 引号包含的整串
            i++; // 跳过开引号
            while (i < len && text[i] != '"' && tlen < 511) {
                if (text[i] == '\\' && i + 1 < len) {
                    // 反斜杠转义
                    i++;
                    switch (text[i]) {
                        case 'n': token[tlen++] = '\n'; break;
                        case 't': token[tlen++] = '\t'; break;
                        case '"': token[tlen++] = '"'; break;
                        case '\\': token[tlen++] = '\\'; break;
                        default: token[tlen++] = text[i]; break;
                    }
                } else {
                    token[tlen++] = text[i];
                }
                i++;
            }
            if (i < len && text[i] == '"')
                i++; // 跳过闭引号
        } else {
            // 普通分隔
            while (i < len && text[i] != delimiter && tlen < 511) {
                token[tlen++] = text[i];
                i++;
            }
        }
        token[tlen] = '\0';
        amx_SetString(&output_addr[tokenCount], token, 0, 0, 511);
        tokenCount++;
    }

    return tokenCount;
}

// 排序比较回调（1D 降序/升序）
static int g_sortDirection = 0; // 0=ascending, 1=descending

static int compare_1d_asc(const void *a, const void *b)
{
    cell va = *(const cell *)a;
    cell vb = *(const cell *)b;
    if (g_sortDirection)
        return (vb > va) ? 1 : (vb < va) ? -1 : 0;
    else
        return (va > vb) ? 1 : (va < vb) ? -1 : 0;
}

cell AMX_NATIVE_CALL amxx_sort_custom_1d(AMX *amx, cell *params)
{
    cell *array_addr;
    amx_GetAddr(amx, params[1], &array_addr);
    int size = (int)params[2];
    g_sortDirection = (int)params[3];

    qsort(array_addr, size, sizeof(cell), compare_1d_asc);
    return 1;
}

// 2D 数组排序
static int g_sortColumn = 0;

static int compare_2d(const void *a, const void *b)
{
    cell *rowA = (cell *)a;
    cell *rowB = (cell *)b;
    cell va = rowA[g_sortColumn];
    cell vb = rowB[g_sortColumn];
    if (g_sortDirection)
        return (vb > va) ? 1 : (vb < va) ? -1 : 0;
    else
        return (va > vb) ? 1 : (va < vb) ? -1 : 0;
}

cell AMX_NATIVE_CALL amxx_sort_custom_2d(AMX *amx, cell *params)
{
    cell *array_addr;
    amx_GetAddr(amx, params[1], &array_addr);
    int rows = (int)params[2];
    int cols = (int)params[3];
    g_sortColumn = (int)params[4];
    g_sortDirection = (int)params[5];

    // 使用冒泡排序对行排序（简单实现，每行 cols 个 cell）
    for (int i = 0; i < rows - 1; i++) {
        for (int j = 0; j < rows - 1 - i; j++) {
            cell *rowA = &array_addr[j * cols];
            cell *rowB = &array_addr[(j + 1) * cols];
            int cmp = compare_2d(rowA, rowB);
            if (cmp > 0) {
                for (int k = 0; k < cols; k++) {
                    cell tmp = rowA[k];
                    rowA[k] = rowB[k];
                    rowB[k] = tmp;
                }
            }
        }
    }
    return 1;
}

// SortCustom2D(array[][], array_size, const comparefunc[], data[]="", data_size=0)
// 原版 AMXX sorting.inc 标准签名: 对 2D 数组按行用 Pawn 回调比较排序。
// Pawn 中 array[][] 存储为行偏移数组 (array[i] 为第 i 行的引用偏移),
// 交换行 = 交换偏移值。回调签名: comparefunc(elem1[], elem2[], const data[], data_size)
cell AMX_NATIVE_CALL amxx_SortCustom2D(AMX *amx, cell *params)
{
    cell *array_addr;
    amx_GetAddr(amx, params[1], &array_addr);
    int size = (int)params[2];
    if (size <= 1) return 1;

    // 获取回调函数名
    cell *funcAddr;
    amx_GetAddr(amx, params[3], &funcAddr);
    char funcName[256];
    amx_GetString(funcName, funcAddr, 0, sizeof(funcName));

    int funcidx;
    if (amx_FindPublic(amx, funcName, &funcidx) != AMX_ERR_NONE)
        return 0;

    // 可选 data[] / data_size 参数
    cell dataAddr = (params[0] / sizeof(cell) >= 4) ? params[4] : 0;
    cell dataSize = (params[0] / sizeof(cell) >= 5) ? params[5] : 0;

    // 冒泡排序: 交换行偏移 (array_addr[j] 与 array_addr[j+1])
    for (int i = 0; i < size - 1; i++) {
        for (int j = 0; j < size - 1 - i; j++) {
            // 回调: comparefunc(elem1[], elem2[], data[], data_size)
            // amx_Push 逆序入栈
            amx_Push(amx, dataSize);
            amx_Push(amx, dataAddr);
            amx_Push(amx, array_addr[j + 1]);  // elem2 = row j+1 引用
            amx_Push(amx, array_addr[j]);      // elem1 = row j 引用
            cell retval = 0;
            amx_Exec(amx, &retval, funcidx);
            if (retval > 0) {
                cell tmp = array_addr[j];
                array_addr[j] = array_addr[j + 1];
                array_addr[j + 1] = tmp;
            }
        }
    }
    return 1;
}

// P3: 排序

// sort_integ(array[], size, order=0) - 整数数组排序 (0=升序, 1=降序)
// 比较函数
static int SortIntAsc(const void *a, const void *b) {
    return (*(int *)a - *(int *)b);
}
static int SortIntDesc(const void *a, const void *b) {
    return (*(int *)b - *(int *)a);
}

cell AMX_NATIVE_CALL amxx_sort_integ(AMX *amx, cell *params)
{
    cell *array_addr;
    amx_GetAddr(amx, params[1], &array_addr);
    int size = (int)params[2];
    int order = (int)params[3];

    if (size <= 0) return 0;

    // Copy to temp array, sort, copy back
    int *temp = new int[size];
    for (int i = 0; i < size; i++)
        temp[i] = (int)array_addr[i];

    qsort(temp, (size_t)size, sizeof(int), order == 1 ? SortIntDesc : SortIntAsc);

    for (int i = 0; i < size; i++)
        array_addr[i] = (cell)temp[i];

    delete[] temp;
    return 1;
}

// 浮点数比较
static int SortFloatAsc(const void *a, const void *b) {
    float fa = *(float *)a, fb = *(float *)b;
    if (fa < fb) return -1;
    if (fa > fb) return 1;
    return 0;
}
static int SortFloatDesc(const void *a, const void *b) {
    float fa = *(float *)a, fb = *(float *)b;
    if (fa > fb) return -1;
    if (fa < fb) return 1;
    return 0;
}

// sort_float(Float:array[], size, order=0)
cell AMX_NATIVE_CALL amxx_sort_float(AMX *amx, cell *params)
{
    cell *array_addr;
    amx_GetAddr(amx, params[1], &array_addr);
    int size = (int)params[2];
    int order = (int)params[3];

    if (size <= 0) return 0;

    float *temp = new float[size];
    for (int i = 0; i < size; i++)
        temp[i] = amx_ctof(array_addr[i]);

    qsort(temp, (size_t)size, sizeof(float), order == 1 ? SortFloatDesc : SortFloatAsc);

    for (int i = 0; i < size; i++)
        array_addr[i] = amx_ftoc(temp[i]);

    delete[] temp;
    return 1;
}

// sort_str(array[], size, order=0)
cell AMX_NATIVE_CALL amxx_sort_str(AMX *amx, cell *params)
{
    cell *array_addr;
    amx_GetAddr(amx, params[1], &array_addr);
    int size = (int)params[2];
    int order = (int)params[3];

    if (size <= 0) return 0;

    // AMX string array - sort by cell value (handle-based strings won't work directly)
    int *temp = new int[size];
    for (int i = 0; i < size; i++)
        temp[i] = (int)array_addr[i];

    qsort(temp, (size_t)size, sizeof(int), order == 1 ? SortIntDesc : SortIntAsc);

    for (int i = 0; i < size; i++)
        array_addr[i] = (cell)temp[i];

    delete[] temp;
    return 1;
}

// ===== P2: 时间处理 =====

#include <sys/timeb.h>

cell AMX_NATIVE_CALL amxx_get_systime(AMX *amx, cell *params)
{
    (void)amx;
    int numParams = (int)(params[0] / sizeof(cell));
    struct timeb tp;
    ftime(&tp);
    if (numParams >= 1) {
        cell *highOut;
        amx_GetAddr(amx, params[1], &highOut);
        if (highOut) *highOut = (cell)(tp.time >> 32);
    }
    if (numParams >= 2) {
        cell *lowOut;
        amx_GetAddr(amx, params[2], &lowOut);
        if (lowOut) *lowOut = (cell)(tp.time & 0xFFFFFFFF);
    }
    return (cell)tp.time;
}

cell AMX_NATIVE_CALL amxx_parse_time(AMX *amx, cell *params)
{
    cell *timeStr_addr;
    amx_GetAddr(amx, params[1], &timeStr_addr);
    char timeStr[256];
    amx_GetString(timeStr, timeStr_addr, 0, sizeof(timeStr));

    cell *fmt_addr;
    amx_GetAddr(amx, params[2], &fmt_addr);
    char fmt[128];
    amx_GetString(fmt, fmt_addr, 0, sizeof(fmt));

    // Windows 不提供 strptime，用简单手动解析
    struct tm tm_val;
    memset(&tm_val, 0, sizeof(tm_val));
#ifdef _WIN32
    sscanf(timeStr, fmt,
        &tm_val.tm_year, &tm_val.tm_mon, &tm_val.tm_mday,
        &tm_val.tm_hour, &tm_val.tm_min, &tm_val.tm_sec);
    tm_val.tm_year -= 1900;
    tm_val.tm_mon -= 1;
#else
    strptime(timeStr, fmt, &tm_val);
#endif
    time_t result = mktime(&tm_val);
    return (cell)result;
}

cell AMX_NATIVE_CALL amxx_format_time(AMX *amx, cell *params)
{
    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    int maxlen = (int)params[2];

    cell *fmt_addr;
    amx_GetAddr(amx, params[3], &fmt_addr);
    char fmt[256];
    amx_GetString(fmt, fmt_addr, 0, sizeof(fmt));

    time_t timestamp = (time_t)params[4];

    struct tm *lt = localtime(&timestamp);
    char date[512];
    strftime(date, sizeof(date) - 1, fmt, lt);
    date[sizeof(date) - 1] = '\0';

    return amx_SetString(dest, date, 0, 0, maxlen);
}

// ===== P2: split =====

cell AMX_NATIVE_CALL amxx_split(AMX *amx, cell *params)
{
    cell *str_addr;
    amx_GetAddr(amx, params[1], &str_addr);
    char str[4096];
    amx_GetString(str, str_addr, 0, sizeof(str));

    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    int maxParts = (int)params[3];

    cell *delim_addr;
    amx_GetAddr(amx, params[4], &delim_addr);
    char delim[32];
    amx_GetString(delim, delim_addr, 0, sizeof(delim));
    char delimiter = delim[0];

    int count = 0;
    char *token = strtok(str, delim);
    while (token && count < maxParts) {
        amx_SetString(&dest[count], token, 0, 0, 511);
        count++;
        token = strtok(nullptr, delim);
    }

    return count;
}

// ===== P2: pcvar API =====

cell AMX_NATIVE_CALL amxx_pcvar_num(AMX *amx, cell *params)
{
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char name[64];
    amx_GetString(name, name_addr, 0, sizeof(name));

    cvar_t *cvar = g_engfuncs.pfnCVarGetPointer(name);
    if (!cvar) return 0;
    return (cell)(intptr_t)cvar;
}

cell AMX_NATIVE_CALL amxx_get_pcvar_num(AMX *amx, cell *params)
{
    (void)amx;
    cvar_t *cvar = (cvar_t *)(intptr_t)params[1];
    if (!cvar) return 0;
    return amx_ftoc(cvar->value);
}

cell AMX_NATIVE_CALL amxx_get_pcvar_string(AMX *amx, cell *params)
{
    cvar_t *cvar = (cvar_t *)(intptr_t)params[1];
    if (!cvar) return 0;

    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    return amx_SetString(dest, cvar->string ? cvar->string : "", 0, 0, (int)params[3]);
}

cell AMX_NATIVE_CALL amxx_set_pcvar_num(AMX *amx, cell *params)
{
    (void)amx;
    cvar_t *cvar = (cvar_t *)(intptr_t)params[1];
    if (!cvar) return 0;
    // P1-11 修复: 直接写 cvar->value, 不经过引擎字符串路径 (避免帧延迟)
    cvar->value = (float)(int)params[2];
    return 1;
}

cell AMX_NATIVE_CALL amxx_set_pcvar_string(AMX *amx, cell *params)
{
    cvar_t *cvar = (cvar_t *)(intptr_t)params[1];
    if (!cvar) return 0;

    cell *str_addr;
    amx_GetAddr(amx, params[2], &str_addr);
    char value[256];
    amx_GetString(value, str_addr, 0, sizeof(value));

    if (cvar->string) {
        cvar_t *cv = const_cast<cvar_t *>(cvar);
        // 使用动态分配避免多个 cvar 共享同一 static buffer
        // 注意：引擎原始 cvar 的 string 不应释放，只有我们自己分配的才需要
        cv->string = _strdup(value);
        cv->value = (float)atof(value);
    }
    return 1;
}

cell AMX_NATIVE_CALL amxx_get_pcvar_flags(AMX *amx, cell *params)
{
    (void)amx;
    cvar_t *cvar = (cvar_t *)(intptr_t)params[1];
    if (!cvar) return 0;
    return (cell)cvar->flags;
}

cell AMX_NATIVE_CALL amxx_set_pcvar_flags(AMX *amx, cell *params)
{
    (void)amx;
    cvar_t *cvar = (cvar_t *)(intptr_t)params[1];
    if (!cvar) return 0;
    cvar->flags = (int)params[2];
    return 1;
}

// ===== cvars.inc 补全: 9 个 native =====

// create_cvar(const name[], const string[], flags = FCVAR_NONE, const description[] = "", ...)
// 与 register_cvar 类似, 但支持 description 等扩展参数, 返回 cvar pointer
// 签名对齐原版 AMXX cvars.inc
cell AMX_NATIVE_CALL amxx_create_cvar(AMX *amx, cell *params)
{
    cell num_params = params[0] / sizeof(cell);

    cell *name_addr, *value_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    amx_GetAddr(amx, params[2], &value_addr);
    char name[64], value[512];
    amx_GetString(name, name_addr, 0, sizeof(name));
    amx_GetString(value, value_addr, 0, sizeof(value));

    int flags = (num_params >= 3) ? (int)params[3] : 0;
    char description[256] = "";
    if (num_params >= 4) {
        cell *desc_addr;
        amx_GetAddr(amx, params[4], &desc_addr);
        if (desc_addr)
            amx_GetString(description, desc_addr, 0, sizeof(description));
    }

    // 如果 cvar 已存在则返回已有指针 (原版行为: 不重复创建)
    cvar_t *existing = g_engfuncs.pfnCVarGetPointer(name);
    if (existing) {
        // 更新插件 cvar 追踪表的 description (如提供)
        if (description[0]) {
            for (auto &entry : g_pluginCvars) {
                if (entry.name == name) {
                    entry.description = description;
                    break;
                }
            }
        }
        return (cell)(intptr_t)existing;
    }

    // 获取插件 ID (1-based)
    int pluginId = -1;
    const auto &plugins = AMXXRuntime::GetInstance().GetPlugins();
    for (size_t i = 0; i < plugins.size(); i++) {
        if (plugins[i] && plugins[i]->GetAMX() == amx) {
            pluginId = (int)(i + 1);
            break;
        }
    }

    // 记录到插件 cvar 追踪表 (避免重复)
    bool alreadyTracked = false;
    for (const auto &entry : g_pluginCvars) {
        if (entry.pluginId == pluginId && entry.name == name) {
            alreadyTracked = true;
            break;
        }
    }
    if (!alreadyTracked) {
        PluginCvarEntry entry;
        entry.pluginId = pluginId;
        entry.name = name;
        entry.flags = flags;
        entry.description = description;
        g_pluginCvars.push_back(entry);
    }

    // 创建新 cvar 并注册到引擎
    cvar_t *cvar = (cvar_t *)malloc(sizeof(cvar_t));
    if (!cvar) return 0;
    memset(cvar, 0, sizeof(cvar_t));
    cvar->name = strdup(name);
    cvar->string = strdup(value);
    cvar->flags = flags;
    cvar->value = (float)atof(value);

    g_engfuncs.pfnCVarRegister(cvar);
    return (cell)(intptr_t)cvar;
}

// cvar_exists(const cvar[]) - 检查指定名称的 cvar 是否存在
cell AMX_NATIVE_CALL amxx_cvar_exists(AMX *amx, cell *params)
{
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char name[64];
    amx_GetString(name, name_addr, 0, sizeof(name));

    cvar_t *cvar = g_engfuncs.pfnCVarGetPointer(name);
    return cvar ? 1 : 0;
}

// hook_cvar_change(pcvar, const callback[]) - 为指定 pcvar 注册变更回调
// 返回 hook handle (>0 成功, 0 失败)
// 回调签名: public cvar_change_callback(pcvar, const old_value[], const new_value[])
cell AMX_NATIVE_CALL amxx_hook_cvar_change(AMX *amx, cell *params)
{
    cvar_t *cvar = (cvar_t *)(intptr_t)params[1];
    if (!cvar) return 0;

    cell *funcname_addr;
    amx_GetAddr(amx, params[2], &funcname_addr);
    char funcname[128];
    amx_GetString(funcname, funcname_addr, 0, sizeof(funcname));

    int funcidx;
    if (amx_FindPublic(amx, funcname, &funcidx) != AMX_ERR_NONE)
        return 0;

    CvarHook hook;
    hook.pcvar = cvar;
    hook.amx = amx;
    hook.funcidx = funcidx;
    hook.disabled = false;
    hook.lastValue = cvar->string ? cvar->string : "";
    g_cvarHooks.push_back(hook);

    // handle = 1-based index
    return (cell)g_cvarHooks.size();
}

// disable_cvar_hook(handle) - 禁用指定 hook (标记为 disabled, 不删除)
cell AMX_NATIVE_CALL amxx_disable_cvar_hook(AMX *amx, cell *params)
{
    (void)amx;
    int handle = (int)params[1];
    if (handle < 1 || handle > (int)g_cvarHooks.size())
        return 0;
    g_cvarHooks[handle - 1].disabled = true;
    return 1;
}

// enable_cvar_hook(handle) - 重新启用指定 hook
cell AMX_NATIVE_CALL amxx_enable_cvar_hook(AMX *amx, cell *params)
{
    (void)amx;
    int handle = (int)params[1];
    if (handle < 1 || handle > (int)g_cvarHooks.size())
        return 0;
    g_cvarHooks[handle - 1].disabled = false;
    return 1;
}

// remove_cvar_flags(const cvar[], flags = -1) - 移除指定 cvar 的标志
// 如果 flags=-1 则移除所有标志
cell AMX_NATIVE_CALL amxx_remove_cvar_flags(AMX *amx, cell *params)
{
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char name[64];
    amx_GetString(name, name_addr, 0, sizeof(name));

    cvar_t *cvar = g_engfuncs.pfnCVarGetPointer(name);
    if (!cvar)
        return 0;

    int flags = (int)params[2];
    if (flags == -1) {
        // 移除所有标志
        cvar->flags = 0;
    } else {
        // 使用 bitwise-and 移除指定标志
        cvar->flags &= ~flags;
    }
    return 1;
}

// set_pcvar_bool(pcvar, bool:value) - 设置 pcvar 的布尔值
cell AMX_NATIVE_CALL amxx_set_pcvar_bool(AMX *amx, cell *params)
{
    (void)amx;
    cvar_t *cvar = (cvar_t *)(intptr_t)params[1];
    if (!cvar) return 0;
    // 内部调用 set_pcvar_num 逻辑: 直接写 cvar->value
    cvar->value = (params[2] != 0) ? 1.0f : 0.0f;
    return 1;
}

// get_pcvar_bounds(pcvar, CvarBounds:type, &Float:value)
// 获取 pcvar 的边界值. 嵌入版: 从内部 map 读取 (引擎层不支持强制执行)
// CvarBound_Upper = 0, CvarBound_Lower = 1
// 返回 1 如果该边界已设置, 0 如果未设置
cell AMX_NATIVE_CALL amxx_get_pcvar_bounds(AMX *amx, cell *params)
{
    cvar_t *cvar = (cvar_t *)(intptr_t)params[1];
    if (!cvar) return 0;

    int type = (int)params[2];

    auto it = g_cvarBounds.find(cvar);
    if (it == g_cvarBounds.end())
        return 0;

    const CvarBoundEntry &entry = it->second;
    if (type == 0) {
        // CvarBound_Upper
        if (!entry.hasUpper)
            return 0;
        cell *valueAddr;
        amx_GetAddr(amx, params[3], &valueAddr);
        if (valueAddr) *valueAddr = amx_ftoc(entry.upperValue);
        return 1;
    } else {
        // CvarBound_Lower
        if (!entry.hasLower)
            return 0;
        cell *valueAddr;
        amx_GetAddr(amx, params[3], &valueAddr);
        if (valueAddr) *valueAddr = amx_ftoc(entry.lowerValue);
        return 1;
    }
}

// set_pcvar_bounds(pcvar, CvarBounds:type, bool:set, Float:value = 0.0)
// 设置 pcvar 的边界值. 嵌入版: 记录到内部 map 但不强制执行 (引擎层不支持)
cell AMX_NATIVE_CALL amxx_set_pcvar_bounds(AMX *amx, cell *params)
{
    (void)amx;
    cvar_t *cvar = (cvar_t *)(intptr_t)params[1];
    if (!cvar) return 0;

    int type = (int)params[2];
    bool set = (params[3] != 0);
    float value = amx_ctof(params[4]);

    CvarBoundEntry &entry = g_cvarBounds[cvar];
    // 确保已初始化 (map 默认构造时所有 bool 为 false, float 为 0)
    if (type == 0) {
        // CvarBound_Upper
        entry.hasUpper = set;
        if (set) entry.upperValue = value;
    } else {
        // CvarBound_Lower
        entry.hasLower = set;
        if (set) entry.lowerValue = value;
    }
    return 1;
}

// ===== P3: argbreak =====

cell AMX_NATIVE_CALL amxx_argbreak(AMX *amx, cell *params)
{
    // 读取完整命令行字符串
    const char *cmdline = CMD_ARGS();
    if (!cmdline) cmdline = "";

    cell *arg1, *arg2;
    amx_GetAddr(amx, params[1], &arg1);
    amx_GetAddr(amx, params[2], &arg2);

    char buffer[4096];
    snprintf(buffer, sizeof(buffer), "%s", cmdline);

    // 找到第一个引号内的参数
    char *p = buffer;
    while (*p == ' ') p++;

    int hasQuotes = 0;
    char saved = 0;
    char *arg1Start = p;

    if (*p == '\"') {
        hasQuotes = 1;
        p++;
        arg1Start = p;
        char *q = p;
        while (*q && *q != '\"') q++;
        if (*q == '\"') {
            saved = *q;
            *q = '\0';
        }
    } else {
        while (*p && *p != ' ') p++;
        if (*p == ' ') {
            saved = *p;
            *p = '\0';
        }
    }

    amx_SetString(arg1, arg1Start, 0, 0, (int)params[3]);
    if (hasQuotes && saved == '\"') {
        *(arg1Start + strlen(arg1Start)) = saved;
    }

    // arg2: 剩余部分
    char *arg2Start = hasQuotes ? (arg1Start + strlen(arg1Start) + 2) : (arg1Start + strlen(arg1Start) + 1);
    while (*arg2Start == ' ') arg2Start++;
    amx_SetString(arg2, arg2Start, 0, 0, (int)params[4]);

    return 1;
}

// ===== P3: string_to_array =====

cell AMX_NATIVE_CALL amxx_string_to_array(AMX *amx, cell *params)
{
    // string_to_array(dest[], size, source[], maxlen)
    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    int size = (int)params[2];

    cell *src_addr;
    amx_GetAddr(amx, params[3], &src_addr);
    char source[4096];
    amx_GetString(source, src_addr, 0, sizeof(source));

    int maxlen = (int)params[4];
    int len = (int)strlen(source);
    if (len > maxlen) len = maxlen;

    for (int i = 0; i < size && i < len; i++) {
        dest[i] = (cell)(unsigned char)source[i];
    }
    // 如果 source 比 size 短，填充 0
    for (int i = len; i < size; i++) {
        dest[i] = 0;
    }

    return len;
}

// ===== P3: array_to_string =====

cell AMX_NATIVE_CALL amxx_array_to_string(AMX *amx, cell *params)
{
    // array_to_string(source[], size, dest[], maxlen)
    cell *src;
    amx_GetAddr(amx, params[1], &src);
    int size = (int)params[2];

    cell *dest;
    amx_GetAddr(amx, params[3], &dest);
    int maxlen = (int)params[4];

    char buffer[4096];
    int len = size;
    if (len >= (int)sizeof(buffer)) len = (int)sizeof(buffer) - 1;

    for (int i = 0; i < len; i++) {
        buffer[i] = (char)(src[i] & 0xFF);
    }
    buffer[len] = '\0';

    return amx_SetString(dest, buffer, 0, 0, maxlen);
}

// ===== P1: get_user_weaponname =====
// get_user_weaponname(weaponId, buffer[], len)
cell AMX_NATIVE_CALL amxx_get_user_weaponname(AMX *amx, cell *params)
{
    int weaponId = (int)params[1];
    const char *name = WeaponIDToAlias(weaponId);
    if (!name) name = "";
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    return amx_SetString(dest, name, 0, 0, (int)params[3]);
}

// ===== P1: get_user_weapons =====
// get_user_weapons(index, weaponsArray[], numWeapons, ammoArray[] = { }, clipArray[] = { })
cell AMX_NATIVE_CALL amxx_get_user_weapons(AMX *amx, cell *params)
{
    int index = (int)params[1];
    CBasePlayer *pPlayer = GetPlayerByIndex(index);
    if (!pPlayer) return 0;

    cell *weaponsArray = nullptr;
    cell *ammoArray = nullptr;
    cell *clipArray = nullptr;
    int maxWeapons = (int)params[3];

    amx_GetAddr(amx, params[2], &weaponsArray);
    if (params[0] / sizeof(cell) >= 4)
        amx_GetAddr(amx, params[4], &ammoArray);
    if (params[0] / sizeof(cell) >= 5)
        amx_GetAddr(amx, params[5], &clipArray);

    int count = 0;
    // Iterate all entities to find weapons owned by this player
    for (int i = 0; i <= gpGlobals->maxEntities && count < maxWeapons; i++) {
        edict_t *pEnt = INDEXENT(i);
        if (!pEnt || pEnt->free) continue;
        if (pEnt->v.owner != pPlayer->edict()) continue;
        
        // Check if this entity has weapon data
        CBasePlayerWeapon *pWeapon = dynamic_cast<CBasePlayerWeapon *>(CBaseEntity::Instance(pEnt));
        if (!pWeapon) continue;
        
        if (weaponsArray) weaponsArray[count] = pWeapon->m_iId;
        if (ammoArray && pWeapon->m_iPrimaryAmmoType >= 0 && pWeapon->m_iPrimaryAmmoType < MAX_AMMO_SLOTS)
            ammoArray[count] = pPlayer->m_rgAmmo[pWeapon->m_iPrimaryAmmoType];
        if (clipArray) clipArray[count] = pWeapon->m_iClip;
        count++;
    }
    return count;
}

// ===== P1: user_has_weapon =====
// user_has_weapon(index, weaponId)
cell AMX_NATIVE_CALL amxx_user_has_weapon(AMX *amx, cell *params)
{
    int index = (int)params[1];
    int weaponId = (int)params[2];
    CBasePlayer *pPlayer = GetPlayerByIndex(index);
    if (!pPlayer) return 0;

    for (int i = 0; i <= gpGlobals->maxEntities; i++) {
        edict_t *pEnt = INDEXENT(i);
        if (!pEnt || pEnt->free) continue;
        if (pEnt->v.owner != pPlayer->edict()) continue;
        
        CBasePlayerWeapon *pWeapon = dynamic_cast<CBasePlayerWeapon *>(CBaseEntity::Instance(pEnt));
        if (!pWeapon) continue;
        if (pWeapon->m_iId == weaponId) return 1;
    }
    return 0;
}

// ===== P1: get_user_model =====
// get_user_model(index, buffer[], len)
cell AMX_NATIVE_CALL amxx_get_user_model(AMX *amx, cell *params)
{
    int index = (int)params[1];
    edict_t *pEdict = GetEdictByIndex(index);
    if (!pEdict) return 0;

    const char *model = STRING(pEdict->v.model);
    if (!model) model = "";
    // Strip "models/player/" prefix and ".mdl" suffix if present
    const char *shortModel = model;
    const char *prefix = "models/player/";
    size_t prefixLen = strlen(prefix);
    if (strncmp(model, prefix, prefixLen) == 0)
        shortModel = model + prefixLen;
    
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    int maxlen = (int)params[3];
    
    // Buffer for stripped name (remove .mdl suffix)
    char stripped[64];
    snprintf(stripped, sizeof(stripped), "%s", shortModel);
    char *dot = strrchr(stripped, '.');
    if (dot) *dot = '\0';
    
    return amx_SetString(dest, stripped, 0, 0, maxlen);
}

// ===========================================
// Additional Core Natives
// ===========================================

// ========== 动态原生 (register_native) 实现 ==========
struct DynamicNative {
    AMX *handlerAmx;
    int funcidx;
    std::string name;
};
static std::vector<DynamicNative> g_dynNatives;

// 当前调用的动态原生上下文（供 get_param/get_string 使用）
static AMX *s_callerAmx = nullptr;
static cell *s_callerParams = nullptr;

// 统一 thunk：通过 amx->usertags[UT_NATIVE] 获取原生名，查找并调用处理函数
cell AMX_NATIVE_CALL amxx_dyn_native_thunk(AMX *amx, cell *params)
{
    int nativeIdx = (int)(intptr_t)amx->usertags[UT_NATIVE];
    char name[sNAMEMAX + 1];
    amx_GetNative(amx, nativeIdx, name);

    for (auto &dn : g_dynNatives) {
        if (dn.name == name) {
            // 保存调用者上下文
            AMX *prevAmx = s_callerAmx;
            cell *prevParams = s_callerParams;
            s_callerAmx = amx;
            s_callerParams = params;

            cell retval = 0;
            amx_Exec(dn.handlerAmx, &retval, dn.funcidx);

            // 恢复
            s_callerAmx = prevAmx;
            s_callerParams = prevParams;
            return retval;
        }
    }
    AMXX_LOG("[AMXX] Dynamic native '%s' called but handler not found", name);
    return 0;
}

// register_native(name[], handler[], type=0)
cell AMX_NATIVE_CALL amxx_register_native(AMX *amx, cell *params)
{
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char name[128];
    amx_GetString(name, name_addr, 0, sizeof(name));

    cell *handler_addr;
    amx_GetAddr(amx, params[2], &handler_addr);
    char handler[128];
    amx_GetString(handler, handler_addr, 0, sizeof(handler));

    int funcidx;
    if (amx_FindPublic(amx, handler, &funcidx) != AMX_ERR_NONE) {
        AMXX_LOG("[AMXX] register_native('%s'): handler '%s' not found", name, handler);
        return 0;
    }

    // 注册到全局表
    DynamicNative dn;
    dn.handlerAmx = amx;
    dn.funcidx = funcidx;
    dn.name = name;
    g_dynNatives.push_back(dn);

    // 向所有已加载的 AMX 注册 thunk
    AMXXRuntime &runtime = AMXXRuntime::GetInstance();
    for (auto &plugin : runtime.GetPlugins()) {
        amx_RegisterFunc(plugin->GetAMX(), name, amxx_dyn_native_thunk);
    }

    AMXX_LOG_DBG("[AMXX] register_native('%s' -> '%s') registered", name, handler);
    return 1;
}

// get_func_id(funcname[])
cell AMX_NATIVE_CALL amxx_get_func_id(AMX *amx, cell *params)
{
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char name[256];
    amx_GetString(name, name_addr, 0, sizeof(name));
    
    int funcidx;
    if (amx_FindPublic(amx, name, &funcidx) == AMX_ERR_NONE)
        return funcidx;
    return -1;
}

// set_fail_state(message[])
cell AMX_NATIVE_CALL amxx_set_fail_state(AMX *amx, cell *params)
{
    cell *msg_addr;
    amx_GetAddr(amx, params[1], &msg_addr);
    char msg[512];
    amx_GetString(msg, msg_addr, 0, sizeof(msg));
    
    AMXX_LOG("[AMXX] Plugin failed: %s", msg);
    return 0;
}

// get_param(num) - 读取动态原生调用的第 num 个参数（整数）
cell AMX_NATIVE_CALL amxx_get_param(AMX *amx, cell *params)
{
    (void)amx;
    int num = (int)params[1];
    if (!s_callerParams || num < 1 || num > (int)(s_callerParams[0] / sizeof(cell)))
        return 0;
    return s_callerParams[num];
}

// get_param_f(num) - 读取动态原生调用的第 num 个参数（浮点）
cell AMX_NATIVE_CALL amxx_get_param_f(AMX *amx, cell *params)
{
    (void)amx;
    int num = (int)params[1];
    if (!s_callerParams || num < 1 || num > (int)(s_callerParams[0] / sizeof(cell)))
        return 0;
    return s_callerParams[num];  // Pawn 浮点以 cell 存储
}

// get_string(num, buffer[], maxlen) - 读取动态原生调用的第 num 个字符串参数
cell AMX_NATIVE_CALL amxx_get_string(AMX *amx, cell *params)
{
    int num = (int)params[1];
    if (!s_callerAmx || !s_callerParams || num < 1 || num > (int)(s_callerParams[0] / sizeof(cell)))
        return 0;

    cell *src_addr;
    if (amx_GetAddr(s_callerAmx, s_callerParams[num], &src_addr) != AMX_ERR_NONE)
        return 0;

    cell *dest_addr;
    amx_GetAddr(amx, params[2], &dest_addr);
    int maxlen = (int)params[3];
    return amx_SetString(dest_addr, (char *)src_addr, 0, 0, maxlen);
}

// get_array(param, buffer[], maxlen) - 读取动态原生调用的数组参数
cell AMX_NATIVE_CALL amxx_get_array_f(AMX *amx, cell *params)
{
    int num = (int)params[1];
    if (!s_callerAmx || !s_callerParams || num < 1 || num > (int)(s_callerParams[0] / sizeof(cell)))
        return 0;

    cell *src_addr;
    if (amx_GetAddr(s_callerAmx, s_callerParams[num], &src_addr) != AMX_ERR_NONE)
        return 0;

    cell *dest_addr;
    amx_GetAddr(amx, params[2], &dest_addr);
    int maxlen = (int)params[3];
    for (int i = 0; i < maxlen; i++)
        dest_addr[i] = src_addr[i];
    return maxlen;
}

// set_array(param, buffer[], size) - 写入动态原生调用的数组参数
cell AMX_NATIVE_CALL amxx_set_array_f(AMX *amx, cell *params)
{
    int num = (int)params[1];
    if (!s_callerAmx || !s_callerParams || num < 1 || num > (int)(s_callerParams[0] / sizeof(cell)))
        return 0;

    cell *dest_addr;
    if (amx_GetAddr(s_callerAmx, s_callerParams[num], &dest_addr) != AMX_ERR_NONE)
        return 0;

    cell *src_addr;
    amx_GetAddr(amx, params[2], &src_addr);
    int size = (int)params[3];
    for (int i = 0; i < size; i++)
        dest_addr[i] = src_addr[i];
    return 1;
}

// get_array(param, dest[], size) - 读取动态原生调用的数组参数 (int 版本)
cell AMX_NATIVE_CALL amxx_get_array(AMX *amx, cell *params)
{
    int num = (int)params[1];
    if (!s_callerAmx || !s_callerParams || num < 1 || num > (int)(s_callerParams[0] / sizeof(cell)))
        return 0;

    cell *src_addr;
    if (amx_GetAddr(s_callerAmx, s_callerParams[num], &src_addr) != AMX_ERR_NONE)
        return 0;

    cell *dest_addr;
    amx_GetAddr(amx, params[2], &dest_addr);
    int size = (int)params[3];
    for (int i = 0; i < size; i++)
        dest_addr[i] = src_addr[i];
    return size;
}

// set_array(param, source[], size) - 写入动态原生调用的数组参数 (int 版本)
cell AMX_NATIVE_CALL amxx_set_array(AMX *amx, cell *params)
{
    int num = (int)params[1];
    if (!s_callerAmx || !s_callerParams || num < 1 || num > (int)(s_callerParams[0] / sizeof(cell)))
        return 0;

    cell *dest_addr;
    if (amx_GetAddr(s_callerAmx, s_callerParams[num], &dest_addr) != AMX_ERR_NONE)
        return 0;

    cell *src_addr;
    amx_GetAddr(amx, params[2], &src_addr);
    int size = (int)params[3];
    for (int i = 0; i < size; i++)
        dest_addr[i] = src_addr[i];
    return 1;
}

// param_convert(num) - 转换参数编号 (在嵌入式实现中为 no-op)
cell AMX_NATIVE_CALL amxx_param_convert(AMX *amx, cell *params)
{
    (void)amx;
    return params[1];
}

// callfunc_push_intrf(&value) - 按引用推送整数参数
cell AMX_NATIVE_CALL amxx_callfunc_push_intrf(AMX *amx, cell *params)
{
    (void)amx;
    if (g_callfunc_argv && g_callfunc_params < 32) {
        g_callfunc_argv[g_callfunc_params++] = params[1];
    }
    return 1;
}

// callfunc_push_floatrf(&Float:value) - 按引用推送浮点参数
cell AMX_NATIVE_CALL amxx_callfunc_push_floatrf(AMX *amx, cell *params)
{
    (void)amx;
    if (g_callfunc_argv && g_callfunc_params < 32) {
        g_callfunc_argv[g_callfunc_params++] = params[1];
    }
    return 1;
}

// register_library(name[]) - 注册插件库
static std::set<std::string> g_registeredLibraries;
cell AMX_NATIVE_CALL amxx_register_library(AMX *amx, cell *params)
{
    char name[256];
    cell *addr;
    amx_GetAddr(amx, params[1], &addr);
    amx_GetString(name, addr, 0, sizeof(name));
    g_registeredLibraries.insert(name);
    return 1;
}

// CreateOneForward(plugin_id, name[], ...) - 创建单插件 forward
cell AMX_NATIVE_CALL amxx_create_one_forward(AMX *amx, cell *params)
{
    int pluginId = (int)params[1];
    char name[256];
    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    amx_GetString(name, addr, 0, sizeof(name));

    AMXXRuntime &rt = AMXXRuntime::GetInstance();
    if (pluginId < 1 || pluginId > (int)rt.GetPlugins().size())
        return 0;

    AMXXPlugin *plugin = rt.GetPlugins()[pluginId - 1];
    if (!plugin || !plugin->IsLoaded())
        return 0;

    int funcIdx = plugin->FindPublic(name);
    if (funcIdx < 0)
        return 0;

    AMXXForwardManager &fm = AMXXForwardManager::GetInstance();
    ForwardParam paramTypes[FORWARD_MAX_PARAMS];
    int numParams = 0;
    int maxArgs = (int)(params[0] / sizeof(cell)) - 2;
    for (int i = 0; i < maxArgs && i < FORWARD_MAX_PARAMS; i++) {
        cell *pType = nullptr;
        int pt = FP_CELL;
        if (amx_GetAddr(amx, params[3 + i], &pType) == AMX_ERR_NONE && pType)
            pt = (int)*pType;
        if (pt == FP_DONE)
            break;
        paramTypes[i] = (ForwardParam)pt;
        numParams++;
    }

    int fwdId = fm.RegisterForward(name, ET_CONTINUE, numParams, paramTypes);
    AMXXForward *fwd = fm.GetForward(fwdId);
    if (fwd) {
        fwd->AddPlugin(plugin, funcIdx);
    }

    return fwdId >= 0 ? fwdId + 1 : 0;
}

// PrepareArray(array[], size, copyback) - 准备数组用于 forward 传递
// 返回数组句柄 (在嵌入式实现中直接返回数组地址)
cell AMX_NATIVE_CALL amxx_prepare_array(AMX *amx, cell *params)
{
    (void)amx;
    return params[1]; // 返回数组地址作为句柄
}

// read_data - 读取事件/消息参数
// read_data(num) -> 返回整数
// read_data(num, data[], len) -> 读取字符串
// read_data(num, &float) -> 读取浮点数
cell AMX_NATIVE_CALL amxx_read_data(AMX *amx, cell *params)
{
    int numParams = params[0] / sizeof(cell);

    // 原版 AMXX: read_data() (0 参) 返回当前消息类型 (g_events.getCurrentMsgType)
    if (numParams == 0)
        return AMXXEventSystem::GetInstance().GetCurrentMsgType();

    int argIndex = (int)params[1];

    // 优先从消息系统获取 (register_message 回调)
    AMXXMessageSystem &msgs = AMXXMessageSystem::GetInstance();
    bool hasMsgArgs = (msgs.GetArgCount() > 0);

    if (numParams == 1) {
        // read_data(num) -> 返回整数
        if (hasMsgArgs)
            return msgs.GetArgInt(argIndex);
        return AMXXEventSystem::GetInstance().GetEventArgInt(argIndex);
    }
    else if (numParams == 2) {
        // read_data(num, &float) -> 读取浮点数
        float val;
        if (hasMsgArgs)
            val = msgs.GetArgFloat(argIndex);
        else
            val = AMXXEventSystem::GetInstance().GetEventArgFloat(argIndex);
        cell *dest;
        amx_GetAddr(amx, params[2], &dest);
        *dest = amx_ftoc(val);
        return 1;
    }
    else if (numParams == 3) {
        // read_data(num, data[], len) -> 读取字符串
        cell *dest;
        amx_GetAddr(amx, params[2], &dest);
        int maxlen = (int)params[3];

        char buf[256];
        if (hasMsgArgs)
            msgs.GetArgString(argIndex, buf, sizeof(buf));
        else {
            const char *str = AMXXEventSystem::GetInstance().GetEventArgString(argIndex);
            strncpy(buf, str, sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
        }
        return amx_SetString(dest, buf, 0, 0, maxlen);
    }

    return 0;
}

// set_kvd(kvd, key[], value[])
cell AMX_NATIVE_CALL amxx_set_kvd(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return 1;
}

// vdformat(buffer[], len, fmt_arg, vararg, ...) - 动态 native 版 vformat
// fmt_arg/vararg 是调用者函数参数中格式串与第一个可变参数的下标（从 1 起算）；
// fmt_arg 为 0 时格式串通过第 5 个参数直接传入（原版 natives.cpp vdformat 语义）
cell AMX_NATIVE_CALL amxx_vdformat(AMX *amx, cell *params)
{
    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    int maxlen = (int)params[2];

    int fargPos = (int)params[3];
    int vargPos = (int)params[4];

    // 通过 amx->frm 访问调用者函数的参数数组（同 vformat）
    AMX_HEADER *hdr = (AMX_HEADER *)amx->base;
    unsigned char *data = amx->data ? amx->data : amx->base + (int)hdr->dat;
    cell *callerParams = (cell *)(data + (int)amx->frm + 2 * sizeof(cell));
    cell callerMax = callerParams[0] / sizeof(cell);

    // 确定格式串地址：fmt_arg=0 时用第 5 个参数，否则用调用者参数
    cell fmtAddr = 0;
    if (fargPos == 0) {
        if (params[0] / sizeof(cell) < 5) {
            amx_SetString(dest, "", 0, 0, maxlen);
            return 0;
        }
        fmtAddr = params[5];
    } else {
        if (fargPos <= callerMax)
            fmtAddr = callerParams[fargPos];
    }
    if (fmtAddr == 0) {
        amx_SetString(dest, "", 0, 0, maxlen);
        return 0;
    }

    // 把调用者的 vararg 参数读入本地参数数组，再复用现有格式化逻辑
    enum { MAX_FORMAT_ARGS = 16 };
    cell localParams[2 + MAX_FORMAT_ARGS];
    int numVarargs = 0;
    int idx = vargPos;
    while (idx <= callerMax && numVarargs < MAX_FORMAT_ARGS) {
        localParams[2 + numVarargs] = callerParams[idx];
        numVarargs++;
        idx++;
    }
    localParams[0] = (cell)((1 + numVarargs) * sizeof(cell)); // 字节数
    localParams[1] = fmtAddr; // 格式串 AMX 地址

    char buffer[4096];
    amxx_format_string(amx, localParams, 1, buffer, sizeof(buffer));

    int written = (int)strlen(buffer);
    if (written >= maxlen)
        written = maxlen - 1;
    amx_SetString(dest, buffer, 0, 0, maxlen);
    return (cell)written;
}

// log_error(message[])
// P2-10: include plugin name and current line number (when debug info available)
cell AMX_NATIVE_CALL amxx_log_error(AMX *amx, cell *params)
{
    cell *msg_addr;
    amx_GetAddr(amx, params[1], &msg_addr);
    char msg[1024];
    amx_GetString(msg, msg_addr, 0, sizeof(msg));

    const char *pluginName = "unknown";
    AMXXPlugin *plugin = AMXXRuntime::GetInstance().FindPluginByAMX(amx);
    if (plugin && plugin->GetName())
        pluginName = plugin->GetName();

    // amx->cip holds the current instruction pointer; line number requires
    // debug symbols which this lightweight AMX build does not expose, so we
    // log the plugin name and cip for triangulation.
    AMXX_LOG("[AMXX] [%s] Error (cip=%d): %s", pluginName, (int)amx->cip, msg);
    return 1;
}

// ExecuteForward(forward, ret)
// P0-10: 实现 CreateMultiForward / ExecuteForward / DestroyForward
// 支持 Pawn: new fwd = CreateMultiForward("myEvent", ET_STOP, FP_CELL, FP_DONE)
//           ExecuteForward(fwd, ret, 42)
//           DestroyForward(fwd)
cell AMX_NATIVE_CALL amxx_create_multi_forward(AMX *amx, cell *params)
{
    char name[256];
    cell *addr;
    amx_GetAddr(amx, params[1], &addr);
    amx_GetString(name, addr, 0, sizeof(name));

    ForwardExecType execType = (ForwardExecType)(int)params[2];

    // 解析参数类型列表 (params[3...] 直到 FP_DONE 或 params[0] 耗尽)
    // 原版 AMXX ABI：变参按引用传参，需先解引用得到实际值
    ForwardParam paramTypes[FORWARD_MAX_PARAMS];
    int numParams = 0;
    int maxArgs = (int)(params[0] / sizeof(cell)) - 2;
    for (int i = 0; i < maxArgs && i < FORWARD_MAX_PARAMS; i++) {
        cell *pType = nullptr;
        int pt = FP_CELL;
        if (amx_GetAddr(amx, params[3 + i], &pType) == AMX_ERR_NONE && pType)
            pt = (int)*pType;
        if (pt == FP_DONE)
            break;
        paramTypes[i] = (ForwardParam)pt;
        numParams++;
    }

    AMXXForwardManager &fm = AMXXForwardManager::GetInstance();
    int fwdId = fm.RegisterForward(name, execType, numParams, paramTypes);

    // 自动扫描所有已加载插件中同名的 public 函数并注册
    AMXXForward *fwd = fm.GetForward(fwdId);
    if (fwd) {
        AMXXRuntime &rt = AMXXRuntime::GetInstance();
        for (auto *plugin : rt.GetPlugins()) {
            if (!plugin || !plugin->IsLoaded())
                continue;
            int funcIdx = plugin->FindPublic(name);
            if (funcIdx >= 0)
                fwd->AddPlugin(plugin, funcIdx);
        }
    }

    return fwdId >= 0 ? fwdId + 1 : 0; // 返回 1-based ID (0 = 无效)
}

cell AMX_NATIVE_CALL amxx_execute_forward(AMX *amx, cell *params)
{
    int fwdId = (int)params[1] - 1; // 转 0-based
    if (fwdId < 0)
        return 0;

    AMXXForwardManager &fm = AMXXForwardManager::GetInstance();
    AMXXForward *fwd = fm.GetForward(fwdId);
    if (!fwd)
        return 0;

    // 构建参数 (params[3...] 是可变参数, params[2] 是 &ret)
    int numArgs = (int)(params[0] / sizeof(cell)) - 2;
    if (numArgs > FORWARD_MAX_PARAMS)
        numArgs = FORWARD_MAX_PARAMS;

    ForwardCallParam fcp[FORWARD_MAX_PARAMS];
    memset(fcp, 0, sizeof(fcp));
    for (int i = 0; i < numArgs; i++) {
        // 原版 AMXX ABI：变参按引用传参，需解引用取得实际值
        fcp[i].type = FP_CELL;
        cell *pArg = nullptr;
        if (amx_GetAddr(amx, params[3 + i], &pArg) == AMX_ERR_NONE && pArg)
            fcp[i].val = *pArg;
        else
            fcp[i].val = 0;
    }

    int ret = fwd->Execute(numArgs, fcp);

    // 存返回值到 &ret
    cell *retAddr;
    amx_GetAddr(amx, params[2], &retAddr);
    if (retAddr)
        *retAddr = ret;

    return 1;
}

cell AMX_NATIVE_CALL amxx_destroy_forward(AMX *amx, cell *params)
{
    int fwdId = (int)params[1] - 1;
    if (fwdId < 0)
        return 0;
    AMXXForwardManager &fm = AMXXForwardManager::GetInstance();
    fm.UnregisterForward(fwdId);
    return 1;
}

cell AMX_NATIVE_CALL amxx_get_forward_func_count(AMX *amx, cell *params)
{
    int fwdId = (int)params[1] - 1;
    if (fwdId < 0)
        return 0;
    AMXXForwardManager &fm = AMXXForwardManager::GetInstance();
    AMXXForward *fwd = fm.GetForward(fwdId);
    if (!fwd)
        return 0;
    // 返回已注册的插件回调数
    return fwd->GetNumParams() >= 0 ? 1 : 0; // 简化: 无法直接获取 calls.size()
}

// ===========================================
// Player State Natives
// ===========================================

// set_user_origin(index, origin[3])
cell AMX_NATIVE_CALL amxx_set_user_origin(AMX *amx, cell *params)
{
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free)
        return 0;
    
    cell *origin_addr;
    amx_GetAddr(amx, params[2], &origin_addr);
    
    pEdict->v.origin[0] = amx_ctof(origin_addr[0]);
    pEdict->v.origin[1] = amx_ctof(origin_addr[1]);
    pEdict->v.origin[2] = amx_ctof(origin_addr[2]);
    
    pEdict->v.absmin = pEdict->v.origin - Vector(16, 16, 0);
    pEdict->v.absmax = pEdict->v.origin + Vector(16, 16, 64);
    
    return 1;
}

// velocity_by_aim(index, speed)
cell AMX_NATIVE_CALL amxx_velocity_by_aim(AMX *amx, cell *params)
{
    int index = (int)params[1];
    float speed = amx_ctof(params[2]);
    
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free)
        return 0;
    
    cell *dest;
    amx_GetAddr(amx, params[3], &dest);
    
    Vector forward, right, up;
    AngleVectors(pEdict->v.v_angle, forward, right, up);
    forward *= speed;
    
    dest[0] = amx_ftoc(forward.x);
    dest[1] = amx_ftoc(forward.y);
    dest[2] = amx_ftoc(forward.z);
    
    return 1;
}

// ===========================================
// Other Natives
// ===========================================

// pause() - 暂停插件或 forward（参考原版 AMXX 实现）
// pause("a", plugin[]) - 暂停指定插件
// pause("c", forwardId) - 暂停指定 forward
// pause() - 暂停调用者自身
cell AMX_NATIVE_CALL amxx_pause(AMX *amx, cell *params)
{
    cell num_params = params[0] / sizeof(cell);
    AMXXRuntime &runtime = AMXXRuntime::GetInstance();

    if (num_params >= 2)
    {
        char flag[8] = {0};
        cell *flag_addr;
        amx_GetAddr(amx, params[1], &flag_addr);
        if (flag_addr)
            amx_GetString(flag, flag_addr, 0, sizeof(flag));

        if (flag[0] == 'c')
        {
            // pause("c", forwardId) - 暂停 forward
            int fwdId = (num_params >= 3) ? (int)params[2] : 0;
            return AMXXForwardManager::GetInstance().PauseForward(fwdId) ? 1 : 0;
        }

        if (flag[0] == 'a' && num_params >= 3)
        {
            // pause("a", pluginName) - 按名查找并暂停指定插件
            char pluginName[256];
            cell *name_addr;
            amx_GetAddr(amx, params[2], &name_addr);
            if (name_addr)
                amx_GetString(pluginName, name_addr, 0, sizeof(pluginName));
            else
                pluginName[0] = '\0';

            for (size_t i = 0; i < runtime.GetPlugins().size(); i++) {
                AMXXPlugin *plugin = runtime.GetPlugins()[i];
                if (plugin && plugin->IsLoaded() && !plugin->IsPaused()) {
                    if (stricmp(plugin->GetName(), pluginName) == 0 ||
                        stricmp(plugin->GetTitle(), pluginName) == 0) {
                        plugin->SetPaused(true);
                        AMXX_LOG("[Pause] Pausing plugin '%s'", plugin->GetName());
                        ForwardCallParam fcp[1];
                        fcp[0].type = FP_CELL;
                        fcp[0].val = (cell)(i + 1);
                        runtime.ExecuteForwardEx("plugin_pause", 1, fcp);
                        return 1;
                    }
                }
            }
            return 0;
        }
    }

    // 默认: 暂停调用者自身
    for (size_t i = 0; i < runtime.GetPlugins().size(); i++) {
        AMXXPlugin *plugin = runtime.GetPlugins()[i];
        if (plugin && plugin->GetAMX() == amx) {
            plugin->SetPaused(true);
            AMXX_LOG("[Pause] Pausing plugin '%s'", plugin->GetName());
            ForwardCallParam fcp[1];
            fcp[0].type = FP_CELL;
            fcp[0].val = (cell)(i + 1);
            runtime.ExecuteForwardEx("plugin_pause", 1, fcp);
            return 1;
        }
    }
    return 0;
}

// xvar storage (cross-plugin variables)
// 嵌入式实现: get_xvar_id 按名查找，不存在则创建 (简化版，原版在插件加载时扫描 AMX 公共变量注册)
struct XvarEntry {
    std::string name;
    cell value;
};
static std::vector<XvarEntry> g_xvars;

// xvar natives (for statscfg)
cell AMX_NATIVE_CALL amxx_get_xvar_id(AMX *amx, cell *params)
{
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char name[256];
    amx_GetString(name, name_addr, 0, sizeof(name));

    for (int i = 0; i < (int)g_xvars.size(); i++) {
        if (g_xvars[i].name == name)
            return i;
    }
    // 嵌入式简化: 不存在则创建，便于跨插件共享
    g_xvars.push_back({name, 0});
    return (cell)(g_xvars.size() - 1);
}

cell AMX_NATIVE_CALL amxx_get_xvar_num(AMX *amx, cell *params)
{
    (void)amx;
    int id = (int)params[1];
    if (id < 0 || id >= (int)g_xvars.size())
        return 0;
    return g_xvars[id].value;
}

cell AMX_NATIVE_CALL amxx_set_xvar_num(AMX *amx, cell *params)
{
    (void)amx;
    int id = (int)params[1];
    if (id < 0 || id >= (int)g_xvars.size())
        return 0;
    g_xvars[id].value = params[2];
    return 1;
}

// get_xvar_float(id) - 读取 xvar 的浮点值 (cell 位模式直接返回, 由 pawn 端解释为 Float)
cell AMX_NATIVE_CALL amxx_get_xvar_float(AMX *amx, cell *params)
{
    (void)amx;
    int id = (int)params[1];
    if (id < 0 || id >= (int)g_xvars.size()) {
        float zero = 0.0f;
        return amx_ftoc(zero);
    }
    return g_xvars[id].value;
}

// set_xvar_float(id, Float:value) - 设置 xvar 的浮点值 (params[2] 已为 cell 位模式)
cell AMX_NATIVE_CALL amxx_set_xvar_float(AMX *amx, cell *params)
{
    (void)amx;
    int id = (int)params[1];
    if (id < 0 || id >= (int)g_xvars.size())
        return 0;
    g_xvars[id].value = params[2];
    return 1;
}

// xvar_exists(const name[]) - 检查 xvar 是否存在
cell AMX_NATIVE_CALL amxx_xvar_exists(AMX *amx, cell *params)
{
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char name[256];
    amx_GetString(name, name_addr, 0, sizeof(name));

    for (int i = 0; i < (int)g_xvars.size(); i++) {
        if (g_xvars[i].name == name)
            return 1;
    }
    return 0;
}

// unpause() - 恢复插件或 forward
// unpause("a", plugin[]) - 恢复指定插件
// unpause("c", forwardId) - 恢复指定 forward
// unpause() - 恢复调用者自身
cell AMX_NATIVE_CALL amxx_unpause(AMX *amx, cell *params)
{
    cell num_params = params[0] / sizeof(cell);
    AMXXRuntime &runtime = AMXXRuntime::GetInstance();

    if (num_params >= 2)
    {
        char flag[8] = {0};
        cell *flag_addr;
        amx_GetAddr(amx, params[1], &flag_addr);
        if (flag_addr)
            amx_GetString(flag, flag_addr, 0, sizeof(flag));

        if (flag[0] == 'c')
        {
            // unpause("c", forwardId) - 恢复 forward
            int fwdId = (num_params >= 3) ? (int)params[2] : 0;
            return AMXXForwardManager::GetInstance().UnpauseForward(fwdId) ? 1 : 0;
        }

        if (flag[0] == 'a' && num_params >= 3)
        {
            // unpause("a", pluginName) - 按名查找并恢复指定插件
            char pluginName[256];
            cell *name_addr;
            amx_GetAddr(amx, params[2], &name_addr);
            if (name_addr)
                amx_GetString(pluginName, name_addr, 0, sizeof(pluginName));
            else
                pluginName[0] = '\0';

            for (size_t i = 0; i < runtime.GetPlugins().size(); i++) {
                AMXXPlugin *plugin = runtime.GetPlugins()[i];
                if (plugin && plugin->IsLoaded() && plugin->IsPaused()) {
                    if (stricmp(plugin->GetName(), pluginName) == 0 ||
                        stricmp(plugin->GetTitle(), pluginName) == 0) {
                        plugin->SetPaused(false);
                        AMXX_LOG("[Unpause] Unpausing plugin '%s'", plugin->GetName());
                        ForwardCallParam fcp[1];
                        fcp[0].type = FP_CELL;
                        fcp[0].val = (cell)(i + 1);
                        runtime.ExecuteForwardEx("plugin_unpause", 1, fcp);
                        return 1;
                    }
                }
            }
            return 0;
        }
    }

    // 默认: 恢复调用者自身
    for (size_t i = 0; i < runtime.GetPlugins().size(); i++) {
        AMXXPlugin *plugin = runtime.GetPlugins()[i];
        if (plugin && plugin->GetAMX() == amx) {
            plugin->SetPaused(false);
            AMXX_LOG("[Unpause] Unpausing plugin '%s'", plugin->GetName());
            ForwardCallParam fcp[1];
            fcp[0].type = FP_CELL;
            fcp[0].val = (cell)(i + 1);
            runtime.ExecuteForwardEx("plugin_unpause", 1, fcp);
            return 1;
        }
    }
    return 0;
}

// isalnum(c)
cell AMX_NATIVE_CALL amxx_isalnum(AMX *amx, cell *params)
{
    (void)amx;
    char c = (char)params[1];
    return (cell)(isalnum((unsigned char)c) ? 1 : 0);
}

// num_to_word(num, output[], len) - 将数字转为英文单词 (0 ~ 999,999)
static void AppendWord(char *buf, size_t size, const char *word)
{
    if (buf[0] != '\0')
        strncat(buf, " ", size - strlen(buf) - 1);
    strncat(buf, word, size - strlen(buf) - 1);
}

static void NumberToWords(int num, char *buffer, size_t size)
{
    static const char *ones[] = {"", "one", "two", "three", "four", "five", "six",
        "seven", "eight", "nine", "ten", "eleven", "twelve", "thirteen", "fourteen",
        "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"};
    static const char *tens[] = {"", "", "twenty", "thirty", "forty", "fifty",
        "sixty", "seventy", "eighty", "ninety"};

    buffer[0] = '\0';

    if (num == 0) {
        AppendWord(buffer, size, "zero");
        return;
    }

    if (num < 0) {
        AppendWord(buffer, size, "negative");
        num = -num;
    }

    if (num >= 1000000) {
        // 超出支持范围，回退为数字字符串
        snprintf(buffer, size, "%d", num);
        return;
    }

    if (num >= 1000) {
        int thousands = num / 1000;
        if (thousands >= 100) {
            AppendWord(buffer, size, ones[thousands / 100]);
            AppendWord(buffer, size, "hundred");
            thousands %= 100;
        }
        if (thousands >= 20) {
            AppendWord(buffer, size, tens[thousands / 10]);
            if (thousands % 10 > 0)
                AppendWord(buffer, size, ones[thousands % 10]);
        } else if (thousands > 0) {
            AppendWord(buffer, size, ones[thousands]);
        }
        AppendWord(buffer, size, "thousand");
        num %= 1000;
    }

    if (num >= 100) {
        AppendWord(buffer, size, ones[num / 100]);
        AppendWord(buffer, size, "hundred");
        num %= 100;
    }

    if (num >= 20) {
        AppendWord(buffer, size, tens[num / 10]);
        if (num % 10 > 0)
            AppendWord(buffer, size, ones[num % 10]);
    } else if (num > 0) {
        AppendWord(buffer, size, ones[num]);
    }
}

cell AMX_NATIVE_CALL amxx_num_to_word(AMX *amx, cell *params)
{
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    int maxlen = (int)params[3];

    int num = (int)params[1];
    char buffer[256];
    NumberToWords(num, buffer, sizeof(buffer));

    return amx_SetString(dest, buffer, 0, 0, maxlen);
}

// set_module_filter(filter)
cell AMX_NATIVE_CALL amxx_set_module_filter(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return 1;
}

// set_native_filter(filter)
cell AMX_NATIVE_CALL amxx_set_native_filter(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return 1;
}

// ============================================================
// 补全核心 natives
// ============================================================

// get_user_index(const name[]) - 按名查找玩家，返回 1-32 或 0
cell AMX_NATIVE_CALL amxx_get_user_index(AMX *amx, cell *params)
{
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char name[256];
    amx_GetString(name, name_addr, 0, sizeof(name));
    if (name[0] == '\0')
        return 0;

    int partialMatch = 0;
    for (int i = 1; i <= gpGlobals->maxClients; i++) {
        edict_t *pEdict = INDEXENT(i);
        if (!pEdict || pEdict->free)
            continue;
        const char *pname = STRING(pEdict->v.netname);
        if (!pname || !pname[0])
            continue;
        if (strcmp(pname, name) == 0)
            return i;  // 精确匹配优先
        if (!partialMatch && strstr(pname, name))
            partialMatch = i;
    }
    return partialMatch;
}

// amxclient_cmd(index, const command[], const arg1[] = "", const arg2[] = "")
// 与 engclient_cmd 相同实现 (模拟管理员级别客户端命令)
cell AMX_NATIVE_CALL amxx_amxclient_cmd(AMX *amx, cell *params)
{
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free)
        return 0;

    cell *cmd_addr, *arg1_addr, *arg2_addr;
    amx_GetAddr(amx, params[2], &cmd_addr);
    amx_GetAddr(amx, params[3], &arg1_addr);
    amx_GetAddr(amx, params[4], &arg2_addr);

    char cmd[256], arg1[256], arg2[256];
    amx_GetString(cmd, cmd_addr, 0, sizeof(cmd));
    amx_GetString(arg1, arg1_addr, 0, sizeof(arg1));
    amx_GetString(arg2, arg2_addr, 0, sizeof(arg2));

    g_engfuncs.pfnClientCommand(pEdict, "%s %s %s", cmd, arg1, arg2);
    return 1;
}

// --- HUD Sync 对象 ---
#define MAX_HUD_SYNC_OBJS 16
struct HudSyncObj {
    int lastChannel;
    bool used;
};
static HudSyncObj g_hudSyncObjs[MAX_HUD_SYNC_OBJS];

// 内部辅助: 用当前 g_hudset 向目标发送 HUD 消息
static void InternalSendHudMessage(int target, const char *msg)
{
    char *splitMessage = UTIL_SplitHudMessage(msg);
    int maxClients = gpGlobals ? gpGlobals->maxClients : 32;

    if (target == 0) {
        for (int i = 1; i <= maxClients; i++) {
            CBaseEntity *pEntity = UTIL_PlayerByIndex(i);
            if (!pEntity || !pEntity->IsNetClient())
                continue;
            hudtextparms_t hp = g_hudset;
            if (hp.a1 == 0) { hp.a1 = 255; if (hp.r1 == 0 && hp.g1 == 0 && hp.b1 == 0) { hp.r1 = 255; hp.g1 = 255; hp.b1 = 255; } }
            if (hp.channel < 0)
                hp.channel = GetNextHudChannel(i);
            else
                hp.channel = abs(hp.channel % 5);
            MESSAGE_BEGIN(MSG_ONE, SVC_TEMPENTITY, nullptr, pEntity->edict());
                WRITE_BYTE(TE_TEXTMESSAGE);
                WRITE_BYTE(hp.channel & 0xFF);
                WRITE_SHORT(FixedSigned16(hp.x, (1<<13)));
                WRITE_SHORT(FixedSigned16(hp.y, (1<<13)));
                WRITE_BYTE(hp.effect);
                WRITE_BYTE(hp.r1); WRITE_BYTE(hp.g1); WRITE_BYTE(hp.b1); WRITE_BYTE(hp.a1);
                WRITE_BYTE(hp.r2); WRITE_BYTE(hp.g2); WRITE_BYTE(hp.b2); WRITE_BYTE(hp.a2);
                WRITE_SHORT(FixedUnsigned16(hp.fadeinTime, (1<<8)));
                WRITE_SHORT(FixedUnsigned16(hp.fadeoutTime, (1<<8)));
                WRITE_SHORT(FixedUnsigned16(hp.holdTime, (1<<8)));
                if (hp.effect == 2)
                    WRITE_SHORT(FixedUnsigned16(hp.fxTime, (1<<8)));
                WRITE_STRING(splitMessage);
            MESSAGE_END();
        }
    } else if (target >= 1 && target <= maxClients) {
        CBaseEntity *pEntity = UTIL_PlayerByIndex(target);
        if (!pEntity || !pEntity->IsNetClient())
            return;
        hudtextparms_t hp = g_hudset;
        if (hp.a1 == 0) { hp.a1 = 255; if (hp.r1 == 0 && hp.g1 == 0 && hp.b1 == 0) { hp.r1 = 255; hp.g1 = 255; hp.b1 = 255; } }
        if (hp.channel < 0)
            hp.channel = GetNextHudChannel(target);
        else
            hp.channel = abs(hp.channel % 5);
        MESSAGE_BEGIN(MSG_ONE, SVC_TEMPENTITY, nullptr, pEntity->edict());
            WRITE_BYTE(TE_TEXTMESSAGE);
            WRITE_BYTE(hp.channel & 0xFF);
            WRITE_SHORT(FixedSigned16(hp.x, (1<<13)));
            WRITE_SHORT(FixedSigned16(hp.y, (1<<13)));
            WRITE_BYTE(hp.effect);
            WRITE_BYTE(hp.r1); WRITE_BYTE(hp.g1); WRITE_BYTE(hp.b1); WRITE_BYTE(hp.a1);
            WRITE_BYTE(hp.r2); WRITE_BYTE(hp.g2); WRITE_BYTE(hp.b2); WRITE_BYTE(hp.a2);
            WRITE_SHORT(FixedUnsigned16(hp.fadeinTime, (1<<8)));
            WRITE_SHORT(FixedUnsigned16(hp.fadeoutTime, (1<<8)));
            WRITE_SHORT(FixedUnsigned16(hp.holdTime, (1<<8)));
            if (hp.effect == 2)
                WRITE_SHORT(FixedUnsigned16(hp.fxTime, (1<<8)));
            WRITE_STRING(splitMessage);
        MESSAGE_END();
    }
}

// CreateHudSyncObj(num = 0, ...) - 创建 HUD 同步对象，返回 1-based 句柄
cell AMX_NATIVE_CALL amxx_CreateHudSyncObj(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    for (int i = 0; i < MAX_HUD_SYNC_OBJS; i++) {
        if (!g_hudSyncObjs[i].used) {
            g_hudSyncObjs[i].used = true;
            g_hudSyncObjs[i].lastChannel = -1;
            return i + 1;  // 1-based handle
        }
    }
    return 0;  // 无可用同步对象
}

// ShowSyncHudMsg(target, syncObj, const fmt[], any:...) - 显示同步 HUD 消息
cell AMX_NATIVE_CALL amxx_ShowSyncHudMsg(AMX *amx, cell *params)
{
    int target = (int)params[1];
    int syncHandle = (int)params[2];

    if (syncHandle < 1 || syncHandle > MAX_HUD_SYNC_OBJS || !g_hudSyncObjs[syncHandle - 1].used)
        return 0;
    HudSyncObj *obj = &g_hudSyncObjs[syncHandle - 1];

    // 格式化消息 (fmt 在 params[3])
    char message[1024];
    amxx_format_string(amx, params, 3, message, sizeof(message));

    // 保存当前 channel，设置同步 channel
    int savedChannel = g_hudset.channel;

    // 首次使用时分配 channel
    if (obj->lastChannel < 0) {
        if (target >= 1 && target <= 32)
            obj->lastChannel = GetNextHudChannel(target);
        else
            obj->lastChannel = 0;
    }
    g_hudset.channel = obj->lastChannel;

    InternalSendHudMessage(target, message);

    g_hudset.channel = savedChannel;
    return 1;
}

// ClearSyncHud(target, syncObj) - 清除同步 HUD 消息
cell AMX_NATIVE_CALL amxx_ClearSyncHud(AMX *amx, cell *params)
{
    (void)amx;
    int target = (int)params[1];
    int syncHandle = (int)params[2];

    if (syncHandle < 1 || syncHandle > MAX_HUD_SYNC_OBJS || !g_hudSyncObjs[syncHandle - 1].used)
        return 0;
    HudSyncObj *obj = &g_hudSyncObjs[syncHandle - 1];

    int savedChannel = g_hudset.channel;
    if (obj->lastChannel < 0) {
        if (target >= 1 && target <= 32)
            obj->lastChannel = GetNextHudChannel(target);
        else
            obj->lastChannel = 0;
    }
    g_hudset.channel = obj->lastChannel;

    InternalSendHudMessage(target, "");

    g_hudset.channel = savedChannel;
    return 1;
}

// get_user_menu(index, &id, &keys) - 获取玩家当前菜单
cell AMX_NATIVE_CALL amxx_get_user_menu(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;

    int menuId = AMXXMenuSystem::GetInstance().GetPlayerMenu(index);

    cell *id_ptr = nullptr, *keys_ptr = nullptr;
    amx_GetAddr(amx, params[2], &id_ptr);
    amx_GetAddr(amx, params[3], &keys_ptr);

    if (menuId > 0) {
        if (id_ptr) *id_ptr = menuId;
        if (keys_ptr) *keys_ptr = 0;
        return menuId;
    }

    if (id_ptr) *id_ptr = 0;
    if (keys_ptr) *keys_ptr = 0;
    return 0;
}

// --- 日志事件数据存储 ---
static std::string g_logEventData;
static std::vector<std::string> g_logEventArgs;

// 由 events.cpp OnLogMessage 调用，存储当前日志事件数据
void AMXX_SetLogEventData(const char *logString)
{
    g_logEventData = logString ? logString : "";
    g_logEventArgs.clear();

    // 解析日志字符串为 token 列表 (与 events.cpp OnLogMessage 一致的解析逻辑)
    const char *b = g_logEventData.c_str();
    while (*b) {
        while (*b == ' ' || *b == '\t') b++;
        if (!*b) break;
        std::string token;
        if (*b == '"') {
            b++;
            while (*b && *b != '"' && token.size() < 127) token += *b++;
            if (*b == '"') b++;
        } else if (*b == '(') {
            b++;
            while (*b && *b != ')' && token.size() < 127) token += *b++;
            if (*b == ')') b++;
        } else {
            while (*b && *b != ' ' && *b != '\t' && *b != '"' && *b != '(' && token.size() < 127)
                token += *b++;
        }
        if (!token.empty())
            g_logEventArgs.push_back(token);
    }
}

// read_logdata(output[], len) - 获取当前日志事件数据字符串
cell AMX_NATIVE_CALL amxx_read_logdata(AMX *amx, cell *params)
{
    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    if (dest)
        amx_SetString(dest, g_logEventData.c_str(), 0, 0, params[2]);
    return 1;
}

// read_logargc() - 获取当前日志事件参数数量
cell AMX_NATIVE_CALL amxx_read_logargc(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return (cell)g_logEventArgs.size();
}

// read_logargv(id, output[], len) - 获取指定日志事件参数
cell AMX_NATIVE_CALL amxx_read_logargv(AMX *amx, cell *params)
{
    int id = (int)params[1];
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    if (!dest)
        return 0;
    if (id < 0 || id >= (int)g_logEventArgs.size()) {
        amx_SetString(dest, "", 0, 0, params[3]);
        return 0;
    }
    amx_SetString(dest, g_logEventArgs[id].c_str(), 0, 0, params[3]);
    return 1;
}

// parse_loguser(const text[], name[], nlen, &userid, authid[], alen, team[], tlen)
// 解析 "Player<1><STEAM_0:1:12345><CT>" 格式的日志用户字符串
cell AMX_NATIVE_CALL amxx_parse_loguser(AMX *amx, cell *params)
{
    cell *text_addr;
    amx_GetAddr(amx, params[1], &text_addr);
    char text[512];
    amx_GetString(text, text_addr, 0, sizeof(text));

    char name[256] = "";
    char authid[64] = "";
    char team[32] = "";
    int userid = -1;

    // 解析 "Name<userid><authid><team>"
    char *lt1 = strchr(text, '<');
    if (!lt1) {
        // 无格式，直接作为 name
        strncpy(name, text, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
    } else {
        // 提取 name
        int nameLen = (int)(lt1 - text);
        if (nameLen >= (int)sizeof(name)) nameLen = (int)sizeof(name) - 1;
        strncpy(name, text, nameLen);
        name[nameLen] = '\0';

        // 提取 userid
        char *gt1 = strchr(lt1 + 1, '>');
        if (gt1) {
            char uidBuf[16];
            int uidLen = (int)(gt1 - (lt1 + 1));
            if (uidLen >= (int)sizeof(uidBuf)) uidLen = (int)sizeof(uidBuf) - 1;
            strncpy(uidBuf, lt1 + 1, uidLen);
            uidBuf[uidLen] = '\0';
            userid = atoi(uidBuf);
        }

        // 提取 authid (第二个 <>)
        char *lt2 = gt1 ? strchr(gt1 + 1, '<') : nullptr;
        char *gt2 = lt2 ? strchr(lt2 + 1, '>') : nullptr;
        if (lt2 && gt2) {
            int authidLen = (int)(gt2 - (lt2 + 1));
            if (authidLen >= (int)sizeof(authid)) authidLen = (int)sizeof(authid) - 1;
            strncpy(authid, lt2 + 1, authidLen);
            authid[authidLen] = '\0';
        }

        // 提取 team (第三个 <>)
        char *lt3 = gt2 ? strchr(gt2 + 1, '<') : nullptr;
        char *gt3 = lt3 ? strchr(lt3 + 1, '>') : nullptr;
        if (lt3 && gt3) {
            int teamLen = (int)(gt3 - (lt3 + 1));
            if (teamLen >= (int)sizeof(team)) teamLen = (int)sizeof(team) - 1;
            strncpy(team, lt3 + 1, teamLen);
            team[teamLen] = '\0';
        }
    }

    int numParams = (int)(params[0] / sizeof(cell));

    // name[], nlen
    if (numParams >= 3) {
        cell *name_dest;
        amx_GetAddr(amx, params[2], &name_dest);
        if (name_dest) amx_SetString(name_dest, name, 0, 0, params[3]);
    }
    // &userid
    if (numParams >= 4) {
        cell *uid_dest;
        amx_GetAddr(amx, params[4], &uid_dest);
        if (uid_dest) *uid_dest = userid;
    }
    // authid[], alen
    if (numParams >= 6) {
        cell *authid_dest;
        amx_GetAddr(amx, params[5], &authid_dest);
        if (authid_dest) amx_SetString(authid_dest, authid, 0, 0, params[6]);
    }
    // team[], tlen
    if (numParams >= 8) {
        cell *team_dest;
        amx_GetAddr(amx, params[7], &team_dest);
        if (team_dest) amx_SetString(team_dest, team, 0, 0, params[8]);
    }

    return 1;
}

// is_user_authorized(index) - 检查玩家是否已授权 (有有效 SteamID)
cell AMX_NATIVE_CALL amxx_is_user_authorized(AMX *amx, cell *params)
{
    (void)amx;
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients)
        return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free)
        return 0;
    const char *authid = GETPLAYERAUTHID(pEdict);
    if (!authid || authid[0] == '\0')
        return 0;
    if (strcmp(authid, "STEAM_ID_PENDING") == 0)
        return 0;
    return 1;
}

// force_unmodified(force_type, const mins[3], const maxs[3], const filename[])
// 简化实现: 始终返回 1 (允许)
cell AMX_NATIVE_CALL amxx_force_unmodified(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return 1;
}

// ============================================================
// P2-9: 补全核心 natives (engine log / module / arch / debug / addr)
// ============================================================

// 错误过滤器回调 (由 set_error_filter 设置)，以及上次错误码 (供 dbg_fmt_error 使用)
static cell g_errorFilterFunc = -1;
static cell g_lastErrorCode = 0;

// elog_message(const message[], any:...) - 写入引擎日志 (ENGINE_LOG)
// 与 log_message 类似，但通过 pfnAlertMessage(at_logged, ...) 输出，可被 register_logevent 捕获
cell AMX_NATIVE_CALL amxx_elog_message(AMX *amx, cell *params)
{
    char msg[1024];
    amxx_format_string(amx, params, 1, msg, sizeof(msg));
    size_t len = strlen(msg);
    if (len < sizeof(msg) - 1) {
        msg[len++] = '\n';
        msg[len] = '\0';
    }
    ALERT(at_logged, "%s", msg);
    return (cell)len;
}

// require_module(const module[]) - 声明模块依赖
// 嵌入版所有功能内置：查找模块名，存在则返回模块 ID (>=0)，不存在则 LogError 并返回 -1
cell AMX_NATIVE_CALL amxx_require_module(AMX *amx, cell *params)
{
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char name[256];
    amx_GetString(name, name_addr, 0, sizeof(name));

    for (int i = 0; i < s_numBuiltinModules; i++) {
        if (strcmp(name, s_builtinModules[i].name) == 0)
            return (cell)i;
    }

    AMXX_LOG_ERR("[AMXX] require_module: module \"%s\" not found", name);
    return -1;
}

// is_amd64_server() - 返回服务器是否为 AMD64 架构
// 嵌入版始终为 32 位，返回 0
cell AMX_NATIVE_CALL amxx_is_amd64_server(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return 0;
}

// set_error_filter(const function[]) - 设置错误过滤器回调
// 嵌入版简化：存储回调 funcidx，在 amx_RaiseError 时可调用
cell AMX_NATIVE_CALL amxx_set_error_filter(AMX *amx, cell *params)
{
    cell *fn_addr;
    amx_GetAddr(amx, params[1], &fn_addr);
    char fnName[64];
    amx_GetString(fnName, fn_addr, 0, sizeof(fnName));

    int index = -1;
    int err = amx_FindPublic(amx, fnName, &index);
    if (err != AMX_ERR_NONE) {
        AMXX_LOG_ERR("[AMXX] set_error_filter: function \"%s\" not found", fnName);
        return 0;
    }
    g_errorFilterFunc = (cell)index;
    return 1;
}

// dbg_trace_begin() - 返回调用栈顶跟踪句柄
// 嵌入版 JIT 不支持完整调试跟踪，返回 0 (无跟踪信息)
cell AMX_NATIVE_CALL amxx_dbg_trace_begin(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return 0;
}

// dbg_trace_next(trace) - 返回下一个跟踪句柄
// 嵌入版无调试跟踪，返回 0
cell AMX_NATIVE_CALL amxx_dbg_trace_next(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return 0;
}

// dbg_trace_info(trace, &line, function[], maxLength1, file[], maxLength2)
// 嵌入版无调试信息，填充 line=0 和空字符串，返回 0
cell AMX_NATIVE_CALL amxx_dbg_trace_info(AMX *amx, cell *params)
{
    cell *line_addr = nullptr;
    amx_GetAddr(amx, params[2], &line_addr);
    if (line_addr)
        *line_addr = 0;

    cell *fn_dest = nullptr;
    amx_GetAddr(amx, params[3], &fn_dest);
    if (fn_dest)
        amx_SetString(fn_dest, "", 0, 0, (int)params[4]);

    cell *file_dest = nullptr;
    amx_GetAddr(amx, params[5], &file_dest);
    if (file_dest)
        amx_SetString(file_dest, "", 0, 0, (int)params[6]);

    return 0;
}

// dbg_fmt_error(buffer[], maxLength) - 格式化当前错误为字符串
// 嵌入版简化为 "Run time error %d: (no debug info)"，使用上次记录的错误码
cell AMX_NATIVE_CALL amxx_dbg_fmt_error(AMX *amx, cell *params)
{
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "Run time error %d: (no debug info)", (int)g_lastErrorCode);

    cell *dest = nullptr;
    amx_GetAddr(amx, params[1], &dest);
    if (dest)
        amx_SetString(dest, buffer, 0, 0, (int)params[2]);
    return 1;
}

// int3() - 触发断点 (仅 DEBUG 模式)
cell AMX_NATIVE_CALL amxx_int3(AMX *amx, cell *params)
{
    (void)amx; (void)params;
#if defined(_DEBUG) || defined(DEBUG)
#  if defined(_WIN32)
    __debugbreak();
#  else
    __asm__ __volatile__("int $3");
#  endif
#endif
    return 0;
}

// get_var_addr(any:...) - 返回变量的 AMX 内存地址
// Pawn 按引用传参时，参数值即为 AMX 相对地址，直接返回 params[1]
cell AMX_NATIVE_CALL amxx_get_var_addr(AMX *amx, cell *params)
{
    (void)amx;
    if ((int)(params[0] / sizeof(cell)) > 0)
        return params[1];
    return 0;
}

// get_addr_val(addr) - 读取指定 AMX 地址的值
// 使用 amx_GetAddr 安全转换为物理指针后读取
cell AMX_NATIVE_CALL amxx_get_addr_val(AMX *amx, cell *params)
{
    cell *addr = nullptr;
    int err = amx_GetAddr(amx, params[1], &addr);
    if (err != AMX_ERR_NONE) {
        g_lastErrorCode = (cell)err;
        AMXX_LOG_ERR("[AMXX] get_addr_val: bad reference %d (err=%d)", (int)params[1], err);
        return 0;
    }
    return addr ? *addr : 0;
}

// set_addr_val(addr, val) - 写入指定 AMX 地址
// 使用 amx_GetAddr 安全转换后写入
cell AMX_NATIVE_CALL amxx_set_addr_val(AMX *amx, cell *params)
{
    cell *addr = nullptr;
    int err = amx_GetAddr(amx, params[1], &addr);
    if (err != AMX_ERR_NONE) {
        g_lastErrorCode = (cell)err;
        AMXX_LOG_ERR("[AMXX] set_addr_val: bad reference %d (err=%d)", (int)params[1], err);
        return 0;
    }
    if (addr)
        *addr = params[2];
    return 1;
}

// ===== string.inc: 补全 21 个缺失 native =====

// UTF-8 辅助: 返回 UTF-8 首字节对应的字符字节数 (1-4)
static int utf8_char_bytes(unsigned char c)
{
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1; // 无效 UTF-8, 按单字节处理
}

// replace_stringex(text[], maxlength, const search[], const replace[], searchLen = -1, replaceLen = -1, bool:caseSensitive = true)
// 替换第一个匹配, 返回替换结束位置索引, 无匹配返回 -1
cell AMX_NATIVE_CALL amxx_replace_stringex(AMX *amx, cell *params)
{
    cell *text_addr, *search_addr, *replace_addr;
    amx_GetAddr(amx, params[1], &text_addr);
    amx_GetAddr(amx, params[3], &search_addr);
    amx_GetAddr(amx, params[4], &replace_addr);

    char text[4096], search[256], replace[256];
    amx_GetString(text, text_addr, 0, sizeof(text));
    amx_GetString(search, search_addr, 0, sizeof(search));
    amx_GetString(replace, replace_addr, 0, sizeof(replace));

    int maxlength = (int)params[2];
    int searchLen = (int)params[5];
    int replaceLen = (int)params[6];
    bool caseSensitive = (params[7] != 0);

    int searchFullLen = (int)strlen(search);
    int replaceFullLen = (int)strlen(replace);
    if (searchLen < 0 || searchLen > searchFullLen)
        searchLen = searchFullLen;
    if (replaceLen < 0 || replaceLen > replaceFullLen)
        replaceLen = replaceFullLen;

    int textLen = (int)strlen(text);
    if (searchLen == 0 || searchLen > textLen) {
        amx_SetString(text_addr, text, 0, 0, maxlength);
        return -1;
    }

    // 查找第一个匹配
    int foundPos = -1;
    for (int i = 0; i <= textLen - searchLen; i++) {
        bool match = caseSensitive
            ? (strncmp(text + i, search, searchLen) == 0)
            : (_strnicmp(text + i, search, searchLen) == 0);
        if (match) { foundPos = i; break; }
    }

    if (foundPos < 0) {
        amx_SetString(text_addr, text, 0, 0, maxlength);
        return -1;
    }

    // 构建结果: text[0:foundPos] + replace[0:replaceLen] + text[foundPos+searchLen:]
    char result[4096];
    size_t outPos = 0;
    memcpy(result + outPos, text, foundPos);
    outPos = (size_t)foundPos;
    if (outPos + (size_t)replaceLen < sizeof(result)) {
        memcpy(result + outPos, replace, replaceLen);
        outPos += replaceLen;
    }
    int afterStart = foundPos + searchLen;
    size_t remaining = strlen(text + afterStart);
    if (outPos + remaining < sizeof(result)) {
        memcpy(result + outPos, text + afterStart, remaining);
        outPos += remaining;
    }
    result[outPos] = '\0';

    amx_SetString(text_addr, result, 0, 0, maxlength);
    return (cell)(foundPos + replaceLen);
}

// fmt(string[], len, const format[], any:...) — format 的别名 (直接调用 format 逻辑)
cell AMX_NATIVE_CALL amxx_fmt(AMX *amx, cell *params)
{
    return amxx_format(amx, params);
}

// format_args(output[], len, pos = 0) — 从调用者栈中读取格式串和变参进行格式化
// pos 是调用者函数参数中格式串的位置 (1-based)
cell AMX_NATIVE_CALL amxx_format_args(AMX *amx, cell *params)
{
    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    int maxlen = (int)params[2];
    int pos = (int)params[3];
    if (pos < 1) pos = 1;

    // 通过 amx->frm 访问调用者函数的参数数组
    AMX_HEADER *hdr = (AMX_HEADER *)amx->base;
    unsigned char *data = amx->data ? amx->data : amx->base + (int)hdr->dat;
    cell *callerParams = (cell *)(data + (int)amx->frm + 2 * sizeof(cell));
    cell callerMax = callerParams[0] / sizeof(cell);

    if (pos > callerMax) {
        amx_SetString(dest, "", 0, 0, maxlen);
        return 0;
    }

    // 把调用者从 pos 开始的参数读入本地数组, 复用 amxx_format_string
    enum { MAX_FORMAT_ARGS = 16 };
    cell localParams[2 + MAX_FORMAT_ARGS];
    int numVarargs = 0;
    int idx = pos + 1; // 变参从格式串之后开始
    while (idx <= callerMax && numVarargs < MAX_FORMAT_ARGS) {
        localParams[2 + numVarargs] = callerParams[idx];
        numVarargs++;
        idx++;
    }
    localParams[0] = (cell)((1 + numVarargs) * sizeof(cell));
    localParams[1] = callerParams[pos]; // 格式串 AMX 地址

    char buffer[4096];
    amxx_format_string(amx, localParams, 1, buffer, sizeof(buffer));

    int written = (int)strlen(buffer);
    if (written >= maxlen)
        written = maxlen - 1;
    amx_SetString(dest, buffer, 0, 0, maxlen);
    return (cell)written;
}

// strtol(const string[], &endPos = 0, base = 0) — 字符串转 long
cell AMX_NATIVE_CALL amxx_strtol(AMX *amx, cell *params)
{
    cell *str_addr;
    amx_GetAddr(amx, params[1], &str_addr);
    char str[256];
    amx_GetString(str, str_addr, 0, sizeof(str));

    int base = (int)params[3];
    char *endptr = nullptr;
    long val = strtol(str, &endptr, base);

    cell *endPosAddr = nullptr;
    amx_GetAddr(amx, params[2], &endPosAddr);
    if (endPosAddr) {
        if (endptr == str)
            *endPosAddr = 0; // 无转换
        else
            *endPosAddr = (cell)(endptr - str);
    }
    return (cell)val;
}

// strtof(const string[], &endPos = 0) — 字符串转 float
cell AMX_NATIVE_CALL amxx_strtof(AMX *amx, cell *params)
{
    cell *str_addr;
    amx_GetAddr(amx, params[1], &str_addr);
    char str[256];
    amx_GetString(str, str_addr, 0, sizeof(str));

    char *endptr = nullptr;
    double val = strtod(str, &endptr);

    cell *endPosAddr = nullptr;
    amx_GetAddr(amx, params[2], &endPosAddr);
    if (endPosAddr) {
        if (endptr == str)
            *endPosAddr = 0;
        else
            *endPosAddr = (cell)(endptr - str);
    }
    return amx_ftoc((float)val);
}

// copyc(dest[], len, const source[], ch) — 复制 source 到 dest 直到遇到 ch 或 len 用完
cell AMX_NATIVE_CALL amxx_copyc(AMX *amx, cell *params)
{
    cell *dest_addr, *src_addr;
    amx_GetAddr(amx, params[1], &dest_addr);
    amx_GetAddr(amx, params[3], &src_addr);

    int len = (int)params[2];
    char ch = (char)(params[4] & 0xFF);

    char src[4096], dest[4096];
    amx_GetString(src, src_addr, 0, sizeof(src));

    size_t written = 0;
    int maxCopy = (len > 1) ? len - 1 : 0;
    for (size_t i = 0; i < strlen(src) && written < (size_t)maxCopy && src[i] != ch; i++)
        dest[written++] = src[i];
    dest[written] = '\0';

    amx_SetString(dest_addr, dest, 0, 0, len);
    return (cell)written;
}

// setc(src[], num, ch) — 设置 src 前 num 个字节为 ch, 返回 num
cell AMX_NATIVE_CALL amxx_setc(AMX *amx, cell *params)
{
    cell *src_addr;
    amx_GetAddr(amx, params[1], &src_addr);
    int num = (int)params[2];
    cell ch = params[3] & 0xFF;

    if (num < 0) num = 0;
    for (int i = 0; i < num; i++)
        src_addr[i] = ch;
    if (num > 0)
        src_addr[num] = 0;
    return (cell)num;
}

// strtok2(const text[], left[], const llen, right[], const rlen, const token = ' ', const trim = 0)
// 增强版 strtok, 支持 trim 标志. 返回 token 在 text 中的位置, 未找到返回 -1
cell AMX_NATIVE_CALL amxx_strtok2(AMX *amx, cell *params)
{
    cell *text_addr;
    amx_GetAddr(amx, params[1], &text_addr);
    char text[4096];
    amx_GetString(text, text_addr, 0, sizeof(text));

    int llen = (int)params[3];
    int rlen = (int)params[5];
    char token = (char)(params[6] & 0xFF);
    int trim = (int)params[7];

    int textLen = (int)strlen(text);
    int tokenPos = -1;
    for (int i = 0; i < textLen; i++) {
        if (text[i] == token) { tokenPos = i; break; }
    }

    char left[4096], right[4096];
    if (tokenPos < 0) {
        strncpy(left, text, sizeof(left) - 1);
        left[sizeof(left) - 1] = '\0';
        right[0] = '\0';
    } else {
        strncpy(left, text, tokenPos);
        left[tokenPos] = '\0';
        strncpy(right, text + tokenPos + 1, sizeof(right) - 1);
        right[sizeof(right) - 1] = '\0';
    }

    // trim 标志 (见 string_const.inc):
    // LTRIM_LEFT=1, RTRIM_LEFT=2, LTRIM_RIGHT=4, RTRIM_RIGHT=8
    auto trimStr = [](char *s, bool ltrim, bool rtrim) {
        if (ltrim) {
            int start = 0;
            while (s[start] && isspace((unsigned char)s[start])) start++;
            if (start > 0) memmove(s, s + start, strlen(s + start) + 1);
        }
        if (rtrim) {
            int len = (int)strlen(s);
            while (len > 0 && isspace((unsigned char)s[len - 1])) len--;
            s[len] = '\0';
        }
    };

    bool ltrimLeft = (trim & 1) != 0;
    bool rtrimLeft = (trim & 2) != 0;
    bool ltrimRight = (trim & 4) != 0;
    bool rtrimRight = (trim & 8) != 0;
    // trim=1 兼容简单模式: 全部 trim
    if (trim == 1) { ltrimLeft = rtrimLeft = ltrimRight = rtrimRight = true; }

    trimStr(left, ltrimLeft, rtrimLeft);
    trimStr(right, ltrimRight, rtrimRight);

    cell *left_addr, *right_addr;
    amx_GetAddr(amx, params[2], &left_addr);
    amx_GetAddr(amx, params[4], &right_addr);
    amx_SetString(left_addr, left, 0, 0, llen);
    amx_SetString(right_addr, right, 0, 0, rlen);

    return (cell)tokenPos;
}

// mb_strtolower(string[], maxlen = 0) — 多字节安全转小写 (UTF-8)
cell AMX_NATIVE_CALL amxx_mb_strtolower(AMX *amx, cell *params)
{
    cell *str_addr;
    amx_GetAddr(amx, params[1], &str_addr);
    int maxlen = (int)params[2];

    char str[4096];
    amx_GetString(str, str_addr, 0, sizeof(str));
    int effectiveMax = (maxlen > 0 && maxlen < (int)sizeof(str)) ? maxlen : (int)sizeof(str);

    for (size_t i = 0; str[i] && i < (size_t)effectiveMax; ) {
        int charLen = utf8_char_bytes((unsigned char)str[i]);
        if (charLen == 1) {
            str[i] = (char)tolower((unsigned char)str[i]);
            i++;
        } else {
            i += charLen; // UTF-8 多字节字符不变
        }
    }
    return amx_SetString(str_addr, str, 0, 0, effectiveMax);
}

// mb_strtoupper(string[], maxlen = 0) — 多字节安全转大写 (UTF-8)
cell AMX_NATIVE_CALL amxx_mb_strtoupper(AMX *amx, cell *params)
{
    cell *str_addr;
    amx_GetAddr(amx, params[1], &str_addr);
    int maxlen = (int)params[2];

    char str[4096];
    amx_GetString(str, str_addr, 0, sizeof(str));
    int effectiveMax = (maxlen > 0 && maxlen < (int)sizeof(str)) ? maxlen : (int)sizeof(str);

    for (size_t i = 0; str[i] && i < (size_t)effectiveMax; ) {
        int charLen = utf8_char_bytes((unsigned char)str[i]);
        if (charLen == 1) {
            str[i] = (char)toupper((unsigned char)str[i]);
            i++;
        } else {
            i += charLen;
        }
    }
    return amx_SetString(str_addr, str, 0, 0, effectiveMax);
}

// mb_ucfirst(string[], maxlen = 0) — 首字母大写 (多字节)
cell AMX_NATIVE_CALL amxx_mb_ucfirst(AMX *amx, cell *params)
{
    cell *str_addr;
    amx_GetAddr(amx, params[1], &str_addr);
    int maxlen = (int)params[2];

    char str[4096];
    amx_GetString(str, str_addr, 0, sizeof(str));
    int effectiveMax = (maxlen > 0 && maxlen < (int)sizeof(str)) ? maxlen : (int)sizeof(str);

    if (str[0]) {
        int charLen = utf8_char_bytes((unsigned char)str[0]);
        if (charLen == 1)
            str[0] = (char)toupper((unsigned char)str[0]);
    }
    return amx_SetString(str_addr, str, 0, 0, effectiveMax);
}

// mb_strtotitle(string[], maxlen = 0) — 每个单词首字母大写 (多字节)
cell AMX_NATIVE_CALL amxx_mb_strtotitle(AMX *amx, cell *params)
{
    cell *str_addr;
    amx_GetAddr(amx, params[1], &str_addr);
    int maxlen = (int)params[2];

    char str[4096];
    amx_GetString(str, str_addr, 0, sizeof(str));
    int effectiveMax = (maxlen > 0 && maxlen < (int)sizeof(str)) ? maxlen : (int)sizeof(str);

    bool newWord = true;
    for (size_t i = 0; str[i] && i < (size_t)effectiveMax; ) {
        int charLen = utf8_char_bytes((unsigned char)str[i]);
        if (charLen == 1) {
            if (newWord && isalpha((unsigned char)str[i])) {
                str[i] = (char)toupper((unsigned char)str[i]);
                newWord = false;
            } else if (!isalpha((unsigned char)str[i])) {
                newWord = true;
            } else {
                str[i] = (char)tolower((unsigned char)str[i]);
            }
            i++;
        } else {
            newWord = false;
            i += charLen;
        }
    }
    return amx_SetString(str_addr, str, 0, 0, effectiveMax);
}

// is_string_category(const input[], input_size, flags, &output_size = 0)
// 检查字符串是否属于指定字符类别 (UTF-8, ASCII 范围精确, 多字节简化处理)
cell AMX_NATIVE_CALL amxx_is_string_category(AMX *amx, cell *params)
{
    cell *input_addr;
    amx_GetAddr(amx, params[1], &input_addr);
    char input[4096];
    amx_GetString(input, input_addr, 0, sizeof(input));

    int input_size = (int)params[2];
    cell flags = params[3];
    cell *output_size_addr = nullptr;
    amx_GetAddr(amx, params[4], &output_size_addr);

    int inputLen = (int)strlen(input);
    if (input_size < 0 || input_size > inputLen)
        input_size = inputLen;

    int conformingBytes = 0;
    for (int i = 0; i < input_size; ) {
        unsigned char c = (unsigned char)input[i];
        int charLen = utf8_char_bytes(c);
        bool matches = false;

        if (charLen == 1) {
            // ASCII 范围
            if (flags & 0x40000000) { // UTF8C_COMPATIBILITY 标志
                cell baseFlag = flags & ~(cell)0x40000000;
                // 各 IS* flag 去掉 COMPAT 后的值 (见 string_const.inc)
                const cell LETTER = 0x0000001F;
                const cell NUMBER = 0x00000700;
                const cell PUNCT = 0x0003F800;
                const cell SYMBOL = 0x003C0000;
                const cell SEP = 0x01C00000;
                if (baseFlag == 0x02000000)                    // ISCNTRL
                    matches = iscntrl(c) != 0;
                else if (baseFlag == LETTER)                    // ISALPHA
                    matches = isalpha(c) != 0;
                else if (baseFlag == NUMBER)                    // ISDIGIT
                    matches = isdigit(c) != 0;
                else if (baseFlag == (LETTER|NUMBER))           // ISALNUM
                    matches = isalnum(c) != 0;
                else if (baseFlag == 0x00400000)                // ISSPACE
                    matches = isspace(c) != 0;
                else if (baseFlag == 0x00000001)                // ISUPPER
                    matches = isupper(c) != 0;
                else if (baseFlag == 0x00000002)                // ISLOWER
                    matches = islower(c) != 0;
                else if (baseFlag == (PUNCT|SYMBOL))            // ISPUNCT
                    matches = ispunct(c) != 0;
                else if (baseFlag == (LETTER|NUMBER|PUNCT|SYMBOL)) // ISGRAPH
                    matches = isgraph(c) != 0;
                else if (baseFlag == (LETTER|NUMBER|PUNCT|SYMBOL|SEP)) // ISPRINT
                    matches = isprint(c) != 0;
                else if (baseFlag == (0x00400000|0x10000000))   // ISBLANK
                    matches = (c == ' ' || c == '\t');
                else if (baseFlag == (NUMBER|0x10000000))       // ISXDIGIT
                    matches = isxdigit(c) != 0;
                else
                    matches = false;
            } else {
                // 非 compatibility flag: 检查具体 Unicode 类别 (ASCII 近似)
                if ((flags & 0x00000001) && isupper(c)) matches = true;       // Uppercase
                if ((flags & 0x00000002) && islower(c)) matches = true;       // Lowercase
                if ((flags & 0x00000010) && isalpha(c) && !isupper(c) && !islower(c)) matches = true; // Other letter
                if ((flags & 0x00000100) && isdigit(c)) matches = true;       // Decimal number
                if ((flags & 0x00400000) && c == ' ') matches = true;         // Space separator
                if ((flags & 0x02000000) && iscntrl(c)) matches = true;       // Control
                if ((flags & 0x00020000) && ispunct(c)) matches = true;       // Other punctuation
                if ((flags & 0x00040000) && (c=='+'||c=='-'||c=='='||c=='<'||c=='>'||c=='|'||c=='~')) matches = true; // Math
                if ((flags & 0x00080000) && c == '$') matches = true;         // Currency
            }
        } else {
            // UTF-8 多字节字符: 简化处理, 视为 "other letter" (Lo)
            if (!(flags & 0x40000000)) {
                if (flags & 0x00000010) matches = true; // UTF8C_LETTER_OTHER
            }
        }

        if (matches) {
            conformingBytes += charLen;
            i += charLen;
        } else {
            break;
        }
    }

    if (output_size_addr)
        *output_size_addr = (cell)conformingBytes;

    return (conformingBytes >= input_size && input_size > 0) ? 1 : 0;
}

// isdigit(ch) — 检查字符是否为数字
cell AMX_NATIVE_CALL amxx_isdigit(AMX *amx, cell *params)
{
    (void)amx;
    int ch = (int)params[1] & 0xFF;
    return isdigit(ch) ? 1 : 0;
}

// isalpha(ch) — 检查字符是否为字母
cell AMX_NATIVE_CALL amxx_isalpha(AMX *amx, cell *params)
{
    (void)amx;
    int ch = (int)params[1] & 0xFF;
    return isalpha(ch) ? 1 : 0;
}

// is_char_mb(ch) — 检查字符是否为多字节字符首字节, 返回字节数(2-4)或 0
cell AMX_NATIVE_CALL amxx_is_char_mb(AMX *amx, cell *params)
{
    (void)amx;
    unsigned char ch = (unsigned char)(params[1] & 0xFF);
    if (ch < 0x80)
        return 0;
    if ((ch & 0xE0) == 0xC0) return 2;
    if ((ch & 0xF0) == 0xE0) return 3;
    if ((ch & 0xF8) == 0xF0) return 4;
    return 0;
}

// is_char_upper(ch) — 检查字符是否为大写 (多字节字符返回 false)
cell AMX_NATIVE_CALL amxx_is_char_upper(AMX *amx, cell *params)
{
    (void)amx;
    int ch = (int)params[1] & 0xFF;
    if (ch >= 0x80) return 0;
    return isupper(ch) ? 1 : 0;
}

// is_char_lower(ch) — 检查字符是否为小写 (多字节字符返回 false)
cell AMX_NATIVE_CALL amxx_is_char_lower(AMX *amx, cell *params)
{
    (void)amx;
    int ch = (int)params[1] & 0xFF;
    if (ch >= 0x80) return 0;
    return islower(ch) ? 1 : 0;
}

// get_char_bytes(const source[]) — 返回第一个字符的字节数 (UTF-8)
cell AMX_NATIVE_CALL amxx_get_char_bytes(AMX *amx, cell *params)
{
    cell *src_addr;
    amx_GetAddr(amx, params[1], &src_addr);
    if (!src_addr || !*src_addr)
        return 1;
    unsigned char ch = (unsigned char)(*src_addr & 0xFF);
    return (cell)utf8_char_bytes(ch);
}

// argparse(const text[], pos, argbuffer[], maxlen)
// 解析参数 (支持引号), 返回下一个位置或 -1
cell AMX_NATIVE_CALL amxx_argparse(AMX *amx, cell *params)
{
    cell *text_addr;
    amx_GetAddr(amx, params[1], &text_addr);
    char text[4096];
    amx_GetString(text, text_addr, 0, sizeof(text));

    int pos = (int)params[2];
    cell *arg_addr;
    amx_GetAddr(amx, params[3], &arg_addr);
    int maxlen = (int)params[4];

    int textLen = (int)strlen(text);
    if (pos < 0) pos = 0;

    // 跳过前导空格
    while (pos < textLen && isspace((unsigned char)text[pos]))
        pos++;

    if (pos >= textLen) {
        amx_SetString(arg_addr, "", 0, 0, maxlen);
        return -1;
    }

    char arg[4096];
    int argPos = 0;

    if (text[pos] == '"') {
        // 引号包含的参数
        pos++; // 跳过开引号
        while (pos < textLen && text[pos] != '"' && argPos < (int)sizeof(arg) - 1)
            arg[argPos++] = text[pos++];
        if (pos < textLen && text[pos] == '"')
            pos++; // 跳过闭引号
    } else {
        // 普通参数: 读到空格为止
        while (pos < textLen && !isspace((unsigned char)text[pos]) && argPos < (int)sizeof(arg) - 1)
            arg[argPos++] = text[pos++];
    }
    arg[argPos] = '\0';
    amx_SetString(arg_addr, arg, 0, 0, maxlen);

    return (cell)pos;
}

// split_string(const source[], const split[], part[], partLen)
// 按 split 分隔符分割字符串, 返回分隔符后位置或 -1
cell AMX_NATIVE_CALL amxx_split_string(AMX *amx, cell *params)
{
    cell *src_addr, *split_addr, *part_addr;
    amx_GetAddr(amx, params[1], &src_addr);
    amx_GetAddr(amx, params[2], &split_addr);
    amx_GetAddr(amx, params[3], &part_addr);
    int partLen = (int)params[4];

    char source[4096], split[256];
    amx_GetString(source, src_addr, 0, sizeof(source));
    amx_GetString(split, split_addr, 0, sizeof(split));

    if (split[0] == '\0') {
        amx_SetString(part_addr, source, 0, 0, partLen);
        return -1;
    }

    char *found = strstr(source, split);
    if (!found) {
        amx_SetString(part_addr, source, 0, 0, partLen);
        return -1;
    }

    int partLength = (int)(found - source);
    char part[4096];
    if (partLength >= (int)sizeof(part))
        partLength = (int)sizeof(part) - 1;
    memcpy(part, source, partLength);
    part[partLength] = '\0';
    amx_SetString(part_addr, part, 0, 0, partLen);

    return (cell)(found - source + (int)strlen(split));
}

// ===========================================
// file.inc 补全: 27 个文件/目录 native
// 文件句柄复用已有的 FILE* 直传约定 (fopen/fclose 等)
// ===========================================

// 目录遍历句柄（堆分配，handle 为指针，0 表示无效）
struct AmxxDirHandle
{
#ifdef _WIN32
    intptr_t findHandle;  // _findfirst 返回值
#else
    DIR *dir;             // opendir 返回值
#endif
};

// ---- 文件 I/O ----

// fread_raw(file, stream[], blocksize, blocks)
// 读取原始字节到 stream（cell 数组），blocksize=元素数, blocks=每元素字节数
// 返回读取的元素数
cell AMX_NATIVE_CALL amxx_fread_raw(AMX *amx, cell *params)
{
    FILE *fp = (FILE *)(intptr_t)params[1];
    if (!fp) return 0;
    cell *stream = nullptr;
    amx_GetAddr(amx, params[2], &stream);
    if (!stream) return 0;
    size_t blocksize = (size_t)params[3];
    size_t blocks = (size_t)params[4];
    if (blocksize == 0 || blocks == 0) return 0;
    return (cell)fread(stream, blocks, blocksize, fp);
}

// fwrite_blocks(file, const data[], blocks, mode)
// 按 blocks 个块写入，mode=BLOCK_CHAR/SHORT/INT/BYTE/BITS（与 fread_blocks 一致）
// 返回写入的块数
cell AMX_NATIVE_CALL amxx_fwrite_blocks(AMX *amx, cell *params)
{
    FILE *fp = (FILE *)(intptr_t)params[1];
    if (!fp) return 0;
    cell *data = nullptr;
    amx_GetAddr(amx, params[2], &data);
    if (!data) return 0;
    cell blocks = params[3];
    int mode = (int)params[4];
    if (blocks <= 0) return 0;

    cell writeCount = 0;
    switch (mode) {
        case BLOCK_CHAR:
        case BLOCK_BYTE: {
            for (cell i = 0; i < blocks; i++) {
                char v = (char)(data[i] & 0xFF);
                if (fwrite(&v, 1, 1, fp) != 1) break;
                writeCount++;
            }
            break;
        }
        case BLOCK_SHORT: {
            for (cell i = 0; i < blocks; i++) {
                short v = (short)(data[i] & 0xFFFF);
                if (fwrite(&v, 1, 2, fp) != 2) break;
                writeCount++;
            }
            break;
        }
        case BLOCK_INT: {
            for (cell i = 0; i < blocks; i++) {
                int v = (int)data[i];
                if (fwrite(&v, 1, 4, fp) != 4) break;
                writeCount++;
            }
            break;
        }
        case BLOCK_BITS: {
            for (cell i = 0; i < blocks; i++) {
                char v = (data[i] & 1) ? 1 : 0;
                if (fwrite(&v, 1, 1, fp) != 1) break;
                writeCount++;
            }
            break;
        }
        default:
            return 0;
    }
    return writeCount;
}

// fwrite_raw(file, const stream[], blocks, mode)
// 写入原始字节, blocks=元素数, mode=每元素字节数
// 返回写入的元素数
cell AMX_NATIVE_CALL amxx_fwrite_raw(AMX *amx, cell *params)
{
    FILE *fp = (FILE *)(intptr_t)params[1];
    if (!fp) return 0;
    cell *stream = nullptr;
    amx_GetAddr(amx, params[2], &stream);
    if (!stream) return 0;
    size_t blocks = (size_t)params[3];
    size_t mode = (size_t)params[4];
    if (blocks == 0 || mode == 0) return 0;
    return (cell)fwrite(stream, mode, blocks, fp);
}

// fputs(file, const text[], bool:null_term = false)
// 写入字符串（不追加换行），返回写入字符数
cell AMX_NATIVE_CALL amxx_fputs(AMX *amx, cell *params)
{
    FILE *fp = (FILE *)(intptr_t)params[1];
    if (!fp) return 0;
    cell *text_addr = nullptr;
    amx_GetAddr(amx, params[2], &text_addr);
    if (!text_addr) return 0;
    char text[4096];
    amx_GetString(text, text_addr, 0, sizeof(text));
    size_t len = strlen(text);
    size_t written = 0;
    if (len > 0)
        written = fwrite(text, 1, len, fp);
    int numParams = (int)(params[0] / sizeof(cell));
    bool null_term = (numParams >= 3) ? (params[3] != 0) : false;
    if (null_term) {
        char nul = '\0';
        if (fwrite(&nul, 1, 1, fp) == 1)
            written++;
    }
    return (cell)written;
}

// fprintf(file, const fmt[], any:...)
// 格式化写入文件，返回写入字符数
cell AMX_NATIVE_CALL amxx_fprintf(AMX *amx, cell *params)
{
    FILE *fp = (FILE *)(intptr_t)params[1];
    if (!fp) return 0;
    char buffer[4096];
    amxx_format_string(amx, params, 2, buffer, sizeof(buffer));
    size_t len = strlen(buffer);
    if (len == 0) return 0;
    size_t written = fwrite(buffer, 1, len, fp);
    return (cell)written;
}

// fgetc(file) - 读取一个字符，返回 ASCII 码，EOF 返回 -1
cell AMX_NATIVE_CALL amxx_fgetc(AMX *amx, cell *params)
{
    (void)amx;
    FILE *fp = (FILE *)(intptr_t)params[1];
    if (!fp) return -1;
    int c = fgetc(fp);
    return (cell)c;
}

// fputc(file, ch) - 写入一个字符，返回 ch 或 -1
cell AMX_NATIVE_CALL amxx_fputc(AMX *amx, cell *params)
{
    (void)amx;
    FILE *fp = (FILE *)(intptr_t)params[1];
    if (!fp) return -1;
    int ch = (int)params[2];
    int c = fputc(ch, fp);
    return (cell)c;
}

// fungetc(file, ch) - 回退一个字符，返回 ch 或 -1
cell AMX_NATIVE_CALL amxx_fungetc(AMX *amx, cell *params)
{
    (void)amx;
    FILE *fp = (FILE *)(intptr_t)params[1];
    if (!fp) return -1;
    int ch = (int)params[2];
    int c = ungetc(ch, fp);
    return (cell)c;
}

// fflush(file) - 刷新文件缓冲区, file=0 刷新所有流
// 返回 0 成功, -1 失败
cell AMX_NATIVE_CALL amxx_fflush(AMX *amx, cell *params)
{
    (void)amx;
    if (params[1] == 0) {
        return fflush(NULL) == 0 ? 0 : -1;
    }
    FILE *fp = (FILE *)(intptr_t)params[1];
    if (!fp) return -1;
    return fflush(fp) == 0 ? 0 : -1;
}

// filesize(const filename[], any:...) - 获取文件大小（支持格式化文件名）
// 返回文件字节数, -1 表示文件不存在
cell AMX_NATIVE_CALL amxx_filesize(AMX *amx, cell *params)
{
    char filename[512];
    amxx_format_string(amx, params, 1, filename, sizeof(filename));
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -1;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fclose(fp);
    return (cell)size;
}

// ---- 文件/目录存在与删除 ----

// dir_exists(const dir[]) - 检查目录是否存在
cell AMX_NATIVE_CALL amxx_dir_exists(AMX *amx, cell *params)
{
    cell *dir_addr = nullptr;
    amx_GetAddr(amx, params[1], &dir_addr);
    if (!dir_addr) return 0;
    char dir[512];
    amx_GetString(dir, dir_addr, 0, sizeof(dir));
    struct stat st;
    if (stat(dir, &st) != 0) return 0;
#ifdef _WIN32
    return (st.st_mode & _S_IFDIR) ? 1 : 0;
#else
    return S_ISDIR(st.st_mode) ? 1 : 0;
#endif
}

// rmdir(const path[]) - 删除空目录
cell AMX_NATIVE_CALL amxx_rmdir(AMX *amx, cell *params)
{
    cell *path_addr = nullptr;
    amx_GetAddr(amx, params[1], &path_addr);
    if (!path_addr) return 0;
    char path[512];
    amx_GetString(path, path_addr, 0, sizeof(path));
#ifdef _WIN32
    return _rmdir(path) == 0 ? 1 : 0;
#else
    return rmdir(path) == 0 ? 1 : 0;
#endif
}

// unlink(const filename[]) - 删除文件（delete_file/file_delete 别名）
cell AMX_NATIVE_CALL amxx_unlink(AMX *amx, cell *params)
{
    cell *filename_addr = nullptr;
    amx_GetAddr(amx, params[1], &filename_addr);
    if (!filename_addr) return 0;
    char filename[512];
    amx_GetString(filename, filename_addr, 0, sizeof(filename));
    return remove(filename) == 0 ? 1 : 0;
}

// ---- 目录遍历 ----

// open_dir(const dir[], firstfile[], length, &FileType:type = ...)
// 打开目录，返回 handle（0 失败），firstfile 填第一个文件名
cell AMX_NATIVE_CALL amxx_open_dir(AMX *amx, cell *params)
{
    cell *dir_addr = nullptr;
    amx_GetAddr(amx, params[1], &dir_addr);
    if (!dir_addr) return 0;
    char dir[512];
    amx_GetString(dir, dir_addr, 0, sizeof(dir));

    cell *firstfile_addr = nullptr;
    amx_GetAddr(amx, params[2], &firstfile_addr);
    int length = (int)params[3];

    int numParams = (int)(params[0] / sizeof(cell));
    cell *type_addr = nullptr;
    if (numParams >= 4) {
        amx_GetAddr(amx, params[4], &type_addr);
    }

#ifdef _WIN32
    char pattern[600];
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);
    struct _finddata_t fileinfo;
    intptr_t fh = _findfirst(pattern, &fileinfo);
    if (fh == -1) {
        if (type_addr) *type_addr = 0; // FileType_Unknown
        return 0;
    }
    if (firstfile_addr)
        amx_SetString(firstfile_addr, fileinfo.name, 0, 0, length);
    if (type_addr) {
        if (fileinfo.attrib & _A_SUBDIR)
            *type_addr = 1; // FileType_Directory
        else
            *type_addr = 2; // FileType_File
    }
    AmxxDirHandle *h = new AmxxDirHandle;
    h->findHandle = fh;
    return (cell)(intptr_t)h;
#else
    DIR *d = opendir(dir);
    if (!d) {
        if (type_addr) *type_addr = 0;
        return 0;
    }
    struct dirent *entry = readdir(d);
    if (!entry) {
        closedir(d);
        if (type_addr) *type_addr = 0;
        return 0;
    }
    if (firstfile_addr)
        amx_SetString(firstfile_addr, entry->d_name, 0, 0, length);
    if (type_addr) {
        if (entry->d_type == DT_DIR)
            *type_addr = 1; // FileType_Directory
        else if (entry->d_type == DT_REG)
            *type_addr = 2; // FileType_File
        else
            *type_addr = 0; // FileType_Unknown
    }
    AmxxDirHandle *h = new AmxxDirHandle;
    h->dir = d;
    return (cell)(intptr_t)h;
#endif
}

// next_file(dirh, buffer[], length, &FileType:type = ...)
// 读取下一个文件，返回 1 成功 0 失败（无更多文件）
cell AMX_NATIVE_CALL amxx_next_file(AMX *amx, cell *params)
{
    AmxxDirHandle *h = (AmxxDirHandle *)(intptr_t)params[1];
    if (!h) return 0;

    cell *buffer_addr = nullptr;
    amx_GetAddr(amx, params[2], &buffer_addr);
    int length = (int)params[3];

    int numParams = (int)(params[0] / sizeof(cell));
    cell *type_addr = nullptr;
    if (numParams >= 4) {
        amx_GetAddr(amx, params[4], &type_addr);
    }

#ifdef _WIN32
    struct _finddata_t fileinfo;
    if (_findnext(h->findHandle, &fileinfo) != 0) {
        if (type_addr) *type_addr = 0;
        return 0;
    }
    if (buffer_addr)
        amx_SetString(buffer_addr, fileinfo.name, 0, 0, length);
    if (type_addr) {
        if (fileinfo.attrib & _A_SUBDIR)
            *type_addr = 1; // FileType_Directory
        else
            *type_addr = 2; // FileType_File
    }
    return 1;
#else
    struct dirent *entry = readdir(h->dir);
    if (!entry) {
        if (type_addr) *type_addr = 0;
        return 0;
    }
    if (buffer_addr)
        amx_SetString(buffer_addr, entry->d_name, 0, 0, length);
    if (type_addr) {
        if (entry->d_type == DT_DIR)
            *type_addr = 1;
        else if (entry->d_type == DT_REG)
            *type_addr = 2;
        else
            *type_addr = 0;
    }
    return 1;
#endif
}

// close_dir(dirh) - 关闭目录句柄
cell AMX_NATIVE_CALL amxx_close_dir(AMX *amx, cell *params)
{
    (void)amx;
    AmxxDirHandle *h = (AmxxDirHandle *)(intptr_t)params[1];
    if (!h) return 0;
#ifdef _WIN32
    if (h->findHandle != -1)
        _findclose(h->findHandle);
#else
    if (h->dir)
        closedir(h->dir);
#endif
    delete h;
    return 1;
}

// ---- 引擎文件操作 ----

// LoadFileForMe(const file[], buffer[], maxlength, &length = 0)
// 用引擎 pfnLoadFileForMe 加载文件到 buffer
// 返回写入的字节数, -1 表示加载失败
cell AMX_NATIVE_CALL amxx_LoadFileForMe(AMX *amx, cell *params)
{
    cell *file_addr = nullptr;
    amx_GetAddr(amx, params[1], &file_addr);
    if (!file_addr) return -1;
    char file[512];
    amx_GetString(file, file_addr, 0, sizeof(file));

    int filelen = 0;
    byte *filedata = g_engfuncs.pfnLoadFileForMe(file, &filelen);
    if (!filedata)
        return -1;

    cell *buffer_addr = nullptr;
    amx_GetAddr(amx, params[2], &buffer_addr);
    if (!buffer_addr) {
        g_engfuncs.pfnFreeFile(filedata);
        return -1;
    }

    int maxlength = (int)params[3];
    int copylen = (filelen < maxlength) ? filelen : maxlength;
    if (copylen > 0)
        memcpy(buffer_addr, filedata, (size_t)copylen);

    int numParams = (int)(params[0] / sizeof(cell));
    if (numParams >= 4) {
        cell *length_addr = nullptr;
        amx_GetAddr(amx, params[4], &length_addr);
        if (length_addr)
            *length_addr = filelen;
    }

    g_engfuncs.pfnFreeFile(filedata);
    return (cell)copylen;
}

// GetFileTime(const file[], FileTimeType:tmode)
// 返回 unix 时间戳, tmode: 0=LastAccess 1=Created 2=LastChange
cell AMX_NATIVE_CALL amxx_GetFileTime(AMX *amx, cell *params)
{
    cell *file_addr = nullptr;
    amx_GetAddr(amx, params[1], &file_addr);
    if (!file_addr) return -1;
    char file[512];
    amx_GetString(file, file_addr, 0, sizeof(file));

    struct stat st;
    if (stat(file, &st) != 0)
        return -1;

    int tmode = (int)params[2];
    time_t t;
    switch (tmode) {
        case 0:  t = st.st_atime; break; // FileTime_LastAccess
        case 1:  t = st.st_ctime; break; // FileTime_Created
        case 2:  t = st.st_mtime; break; // FileTime_LastChange
        default: t = st.st_mtime; break;
    }
    return (cell)t;
}

// SetFilePermissions(const path[], mode)
// 设置文件权限 (Windows 简化: _chmod)
cell AMX_NATIVE_CALL amxx_SetFilePermissions(AMX *amx, cell *params)
{
    cell *path_addr = nullptr;
    amx_GetAddr(amx, params[1], &path_addr);
    if (!path_addr) return 0;
    char path[512];
    amx_GetString(path, path_addr, 0, sizeof(path));
    int mode = (int)params[2];
#ifdef _WIN32
    return _chmod(path, mode) == 0 ? 1 : 0;
#else
    return chmod(path, (mode_t)mode) == 0 ? 1 : 0;
#endif
}

// ---- 类型化读写 (FileRead*/FileWrite*) ----

// FileReadInt8(file, &data) - 读取 1 字节有符号整数（符号扩展到 cell）
cell AMX_NATIVE_CALL amxx_FileReadInt8(AMX *amx, cell *params)
{
    FILE *fp = (FILE *)(intptr_t)params[1];
    if (!fp) return 0;
    cell *data = nullptr;
    amx_GetAddr(amx, params[2], &data);
    if (!data) return 0;
    unsigned char b;
    if (fread(&b, 1, 1, fp) != 1) return 0;
    *data = (cell)(signed char)b;
    return 1;
}

// FileReadUint8(file, &data) - 读取 1 字节无符号整数（零扩展到 cell）
cell AMX_NATIVE_CALL amxx_FileReadUint8(AMX *amx, cell *params)
{
    FILE *fp = (FILE *)(intptr_t)params[1];
    if (!fp) return 0;
    cell *data = nullptr;
    amx_GetAddr(amx, params[2], &data);
    if (!data) return 0;
    unsigned char b;
    if (fread(&b, 1, 1, fp) != 1) return 0;
    *data = (cell)(unsigned char)b;
    return 1;
}

// FileReadInt16(file, &data) - 读取 2 字节有符号整数（小端, 符号扩展）
cell AMX_NATIVE_CALL amxx_FileReadInt16(AMX *amx, cell *params)
{
    FILE *fp = (FILE *)(intptr_t)params[1];
    if (!fp) return 0;
    cell *data = nullptr;
    amx_GetAddr(amx, params[2], &data);
    if (!data) return 0;
    unsigned char b[2];
    if (fread(b, 1, 2, fp) != 2) return 0;
    short v = (short)((unsigned short)b[0] | ((unsigned short)b[1] << 8));
    *data = (cell)v;
    return 1;
}

// FileReadUint16(file, &data) - 读取 2 字节无符号整数（小端, 零扩展）
cell AMX_NATIVE_CALL amxx_FileReadUint16(AMX *amx, cell *params)
{
    FILE *fp = (FILE *)(intptr_t)params[1];
    if (!fp) return 0;
    cell *data = nullptr;
    amx_GetAddr(amx, params[2], &data);
    if (!data) return 0;
    unsigned char b[2];
    if (fread(b, 1, 2, fp) != 2) return 0;
    unsigned short v = (unsigned short)((unsigned short)b[0] | ((unsigned short)b[1] << 8));
    *data = (cell)v;
    return 1;
}

// FileReadInt32(file, &data) - 读取 4 字节有符号整数（小端）
cell AMX_NATIVE_CALL amxx_FileReadInt32(AMX *amx, cell *params)
{
    FILE *fp = (FILE *)(intptr_t)params[1];
    if (!fp) return 0;
    cell *data = nullptr;
    amx_GetAddr(amx, params[2], &data);
    if (!data) return 0;
    unsigned char b[4];
    if (fread(b, 1, 4, fp) != 4) return 0;
    int v = (int)((unsigned int)b[0] | ((unsigned int)b[1] << 8) |
                  ((unsigned int)b[2] << 16) | ((unsigned int)b[3] << 24));
    *data = (cell)v;
    return 1;
}

// FileWriteInt8(file, data) - 写入 1 字节
cell AMX_NATIVE_CALL amxx_FileWriteInt8(AMX *amx, cell *params)
{
    (void)amx;
    FILE *fp = (FILE *)(intptr_t)params[1];
    if (!fp) return 0;
    char v = (char)(params[2] & 0xFF);
    return fwrite(&v, 1, 1, fp) == 1 ? 1 : 0;
}

// FileWriteInt16(file, data) - 写入 2 字节（小端）
cell AMX_NATIVE_CALL amxx_FileWriteInt16(AMX *amx, cell *params)
{
    (void)amx;
    FILE *fp = (FILE *)(intptr_t)params[1];
    if (!fp) return 0;
    short v = (short)(params[2] & 0xFFFF);
    return fwrite(&v, 1, 2, fp) == 2 ? 1 : 0;
}

// FileWriteInt32(file, data) - 写入 4 字节（小端）
cell AMX_NATIVE_CALL amxx_FileWriteInt32(AMX *amx, cell *params)
{
    (void)amx;
    FILE *fp = (FILE *)(intptr_t)params[1];
    if (!fp) return 0;
    int v = (int)params[2];
    return fwrite(&v, 1, 4, fp) == 4 ? 1 : 0;
}

// ========== 跨地图全局状态重置实现 ==========
// 声明在 native_core.h，所有静态变量在此下方都有定义
void ResetCoreNativeGlobals()
{
    // 重置 HUD 参数 (line ~2317)
    extern hudtextparms_t g_hudset;
    memset(&g_hudset, 0, sizeof(g_hudset));

    // 重置 DHUD 参数 (line ~3868)
    extern struct DHudParams_s g_dhudParams;
    memset(&g_dhudParams, 0, sizeof(g_dhudParams));

    // 重置消息状态 (line ~4020-4022)
    extern int g_msgDest, g_msgType;
    extern edict_t *g_msgEdict;
    g_msgDest = 0;
    g_msgType = 0;
    g_msgEdict = nullptr;

    // 清理 callfunc 状态 (line ~4821-4825)
    extern cell g_callfunc_result;
    extern int g_callfunc_params;
    extern cell *g_callfunc_argv;
    extern AMX *g_callfunc_amx;
    extern int g_callfunc_funcidx;
    if (g_callfunc_argv) {
        delete[] g_callfunc_argv;
        g_callfunc_argv = nullptr;
    }
    g_callfunc_result = 0;
    g_callfunc_params = 0;
    g_callfunc_amx = nullptr;
    g_callfunc_funcidx = -1;

    // 重置 localinfo (line ~5035-5036)
    extern char g_localinfo[32][256];
    extern int g_localinfo_count;
    memset(g_localinfo, 0, sizeof(g_localinfo));
    g_localinfo_count = 0;

    // 重置排序状态 (line ~5340, ~5364)
    extern int g_sortDirection, g_sortColumn;
    g_sortDirection = 0;
    g_sortColumn = 0;

    // 重置 CVar 回调 (line ~160)
    extern std::map<std::string, std::vector<CVarChangeCallback>> g_cvarCallbacks;
    g_cvarCallbacks.clear();

    // 重置 hook_cvar_change 存储
    extern std::vector<CvarHook> g_cvarHooks;
    g_cvarHooks.clear();

    // 重置 pcvar bounds 存储
    extern std::map<cvar_t *, CvarBoundEntry> g_cvarBounds;
    g_cvarBounds.clear();

    // 重置插件 CVar 追踪表 (插件会在 plugin_init 中重新注册)
    extern std::vector<PluginCvarEntry> g_pluginCvars;
    g_pluginCvars.clear();

    // 重置 pcvar 绑定 (插件会在 plugin_init 中重新注册)
    ResetCvarBindings();

    // 重置 HUD Sync 对象
    extern HudSyncObj g_hudSyncObjs[];
    for (int i = 0; i < MAX_HUD_SYNC_OBJS; i++) {
        g_hudSyncObjs[i].used = false;
        g_hudSyncObjs[i].lastChannel = -1;
    }

    // 重置日志事件数据
    extern std::string g_logEventData;
    extern std::vector<std::string> g_logEventArgs;
    g_logEventData.clear();
    g_logEventArgs.clear();

    // 重置 xvar 存储
    extern std::vector<XvarEntry> g_xvars;
    g_xvars.clear();

    // 重置错误过滤器与上次错误码 (set_error_filter / dbg_fmt_error)
    extern cell g_errorFilterFunc;
    extern cell g_lastErrorCode;
    g_errorFilterFunc = -1;
    g_lastErrorCode = 0;

    // 清理数据结构
    CleanupDatastructs();
}
// ======================================
