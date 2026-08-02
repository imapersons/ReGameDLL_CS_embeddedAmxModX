#include "precompiled.h"
#include "native_regex.h"
#include "amx.h"
#include <regex>
#include <string>
#include <map>

// Return value semantics (per AMXX regex.inc):
//   REGEX_MATCH_FAIL   = -2
//   REGEX_PATTERN_FAIL = -1
//   REGEX_NO_MATCH     =  0
//   REGEX_OK / handle  = >0
#define REGEX_MATCH_FAIL   (-2)
#define REGEX_PATTERN_FAIL (-1)
#define REGEX_NO_MATCH      0

// PCRE numeric flag bits (subset supported by std::regex backend).
#define REGEX_PCRE_CASELESS  0x00000001
#define REGEX_PCRE_MULTILINE 0x00000002

// std::regex_constants::multiline is a C++17 feature.
#if defined(_MSVC_LANG)
#  define REGEX_CXX_STD _MSVC_LANG
#else
#  define REGEX_CXX_STD __cplusplus
#endif
#if REGEX_CXX_STD >= 201703L
#  define REGEX_HAVE_MULTILINE 1
#endif

struct RegexData {
    std::regex regex;
    std::smatch match;       // last match results (for regex_substr)
    std::string lastString;  // last string matched against
    bool isValid;
    RegexData() : isValid(true) {}
};

// Handle table: 1-based, 0 = invalid.
static std::map<int, RegexData*> g_regexHandles;
static int g_nextRegexId = 1;

static int AllocRegexHandle(RegexData *data)
{
    int id = g_nextRegexId++;
    g_regexHandles[id] = data;
    return id;
}

static RegexData *GetRegexData(int id)
{
    if (id <= 0) return nullptr;
    auto it = g_regexHandles.find(id);
    if (it == g_regexHandles.end()) return nullptr;
    return it->second;
}

void ResetRegexHandles()
{
    for (auto &kv : g_regexHandles) {
        delete kv.second;
    }
    g_regexHandles.clear();
    g_nextRegexId = 1;
}

// ---- Pawn string helpers ----

static std::string ReadAmxString(AMX *amx, cell param)
{
    cell *addr = nullptr;
    amx_GetAddr(amx, param, &addr);
    if (!addr) return std::string();
    char buffer[4096];
    amx_GetString(buffer, addr, 0, sizeof(buffer));
    return std::string(buffer);
}

static void WriteAmxString(AMX *amx, cell param, const char *str, int maxLen)
{
    cell *dest = nullptr;
    amx_GetAddr(amx, param, &dest);
    if (!dest) return;
    amx_SetString(dest, str, 0, 0, (size_t)maxLen);
}

static void SetCellRef(AMX *amx, cell param, cell value)
{
    cell *addr = nullptr;
    amx_GetAddr(amx, param, &addr);
    if (addr) *addr = value;
}

// ---- Flag conversion ----

static std::regex::flag_type ConvertStringFlags(const char *flags)
{
    std::regex::flag_type f = std::regex_constants::ECMAScript;
    if (!flags) return f;
    for (const char *p = flags; *p; p++) {
        switch (*p) {
            case 'i':
                f |= std::regex_constants::icase;
                break;
            case 'm':
#ifdef REGEX_HAVE_MULTILINE
                f |= std::regex_constants::multiline;
#endif
                break;
            // 's' (PCRE_DOTALL) and 'x' (PCRE_EXTENDED) have no direct
            // std::regex equivalent; they are silently ignored.
            default:
                break;
        }
    }
    return f;
}

static std::regex::flag_type ConvertNumericFlags(cell flags)
{
    std::regex::flag_type f = std::regex_constants::ECMAScript;
    if (flags & REGEX_PCRE_CASELESS)
        f |= std::regex_constants::icase;
#ifdef REGEX_HAVE_MULTILINE
    if (flags & REGEX_PCRE_MULTILINE)
        f |= std::regex_constants::multiline;
#endif
    // Other PCRE flags (DOTALL, EXTENDED, ...) are not directly supported
    // by std::regex and are ignored.
    return f;
}

// ---- Natives ----

// native Regex:regex_compile(const pattern[], &ret = 0, error[] = "", maxLen = 0, const flags[]="");
cell AMX_NATIVE_CALL amxx_regex_compile(AMX *amx, cell *params)
{
    std::string pattern = ReadAmxString(amx, params[1]);
    std::string flagsStr = ReadAmxString(amx, params[5]);
    int maxLen = (int)params[4];

    std::regex::flag_type flags = ConvertStringFlags(flagsStr.c_str());

    RegexData *data = new RegexData();
    try {
        data->regex = std::regex(pattern, flags);
    } catch (const std::regex_error &e) {
        SetCellRef(amx, params[2], 0);
        WriteAmxString(amx, params[3], e.what(), maxLen);
        delete data;
        return REGEX_PATTERN_FAIL;
    }

    int handle = AllocRegexHandle(data);
    SetCellRef(amx, params[2], 0);
    WriteAmxString(amx, params[3], "", maxLen);
    return handle;
}

// native Regex:regex_compile_ex(const pattern[], flags = 0, error[]= "", maxLen = 0, &errcode = 0);
cell AMX_NATIVE_CALL amxx_regex_compile_ex(AMX *amx, cell *params)
{
    std::string pattern = ReadAmxString(amx, params[1]);
    cell flagsNum = params[2];
    int maxLen = (int)params[4];

    std::regex::flag_type flags = ConvertNumericFlags(flagsNum);

    RegexData *data = new RegexData();
    try {
        data->regex = std::regex(pattern, flags);
    } catch (const std::regex_error &e) {
        SetCellRef(amx, params[5], 1);
        WriteAmxString(amx, params[3], e.what(), maxLen);
        delete data;
        return REGEX_PATTERN_FAIL;
    }

    int handle = AllocRegexHandle(data);
    SetCellRef(amx, params[5], 0);
    WriteAmxString(amx, params[3], "", maxLen);
    return handle;
}

// native Regex:regex_match(const string[], const pattern[], &ret = 0, error[] = "", maxLen = 0, const flags[] = "");
cell AMX_NATIVE_CALL amxx_regex_match(AMX *amx, cell *params)
{
    std::string str = ReadAmxString(amx, params[1]);
    std::string pattern = ReadAmxString(amx, params[2]);
    std::string flagsStr = ReadAmxString(amx, params[6]);
    int maxLen = (int)params[5];

    std::regex::flag_type flags = ConvertStringFlags(flagsStr.c_str());

    RegexData *data = new RegexData();
    try {
        data->regex = std::regex(pattern, flags);
    } catch (const std::regex_error &e) {
        SetCellRef(amx, params[3], 0);
        WriteAmxString(amx, params[4], e.what(), maxLen);
        delete data;
        return REGEX_PATTERN_FAIL;
    }

    data->lastString = str;
    try {
        if (std::regex_search(str, data->match, data->regex)) {
            int handle = AllocRegexHandle(data);
            SetCellRef(amx, params[3], (cell)data->match.size());
            return handle;
        }
    } catch (const std::regex_error &e) {
        SetCellRef(amx, params[3], 0);
        WriteAmxString(amx, params[4], e.what(), maxLen);
        delete data;
        return REGEX_MATCH_FAIL;
    }

    delete data;
    SetCellRef(amx, params[3], 0);
    return REGEX_NO_MATCH;
}

// native regex_match_c(const string[], Regex:pattern, &ret = 0);
cell AMX_NATIVE_CALL amxx_regex_match_c(AMX *amx, cell *params)
{
    int handle = (int)params[2];
    RegexData *data = GetRegexData(handle);
    if (!data) {
        SetCellRef(amx, params[3], 0);
        return REGEX_MATCH_FAIL;
    }

    std::string str = ReadAmxString(amx, params[1]);
    data->lastString = str;

    try {
        if (std::regex_search(str, data->match, data->regex)) {
            cell results = (cell)data->match.size();
            SetCellRef(amx, params[3], results);
            return results;
        }
    } catch (const std::regex_error &) {
        SetCellRef(amx, params[3], 0);
        return REGEX_MATCH_FAIL;
    }

    SetCellRef(amx, params[3], 0);
    return REGEX_NO_MATCH;
}

// native regex_match_all_c(const string[], Regex:id, &ret = 0);
// 原版 AMXX regex.inc: 用预编译的 Regex 句柄对 string 执行全局匹配。
// 与 regex_match_c 的区别: 返回匹配总数 (遍历所有匹配), &ret 保存首个匹配的捕获组数。
// 实现: 用 std::sregex_iterator 遍历全部匹配, 同时保存首个 match 到 data->match
//       (供 regex_substr 使用), 与原版行为一致。
cell AMX_NATIVE_CALL amxx_regex_match_all_c(AMX *amx, cell *params)
{
    int handle = (int)params[2];
    RegexData *data = GetRegexData(handle);
    if (!data) {
        SetCellRef(amx, params[3], 0);
        return REGEX_MATCH_FAIL;
    }

    std::string str = ReadAmxString(amx, params[1]);
    data->lastString = str;

    try {
        std::sregex_iterator begin(str.begin(), str.end(), data->regex);
        std::sregex_iterator end;
        int matchCount = 0;
        bool first = true;
        for (std::sregex_iterator it = begin; it != end; ++it) {
            if (first) {
                data->match = *it;  // 保存首个匹配供 regex_substr 使用
                first = false;
            }
            matchCount++;
        }
        if (matchCount > 0) {
            cell captures = first ? 0 : (cell)data->match.size();
            SetCellRef(amx, params[3], captures);
            return (cell)matchCount;
        }
    } catch (const std::regex_error &) {
        SetCellRef(amx, params[3], 0);
        return REGEX_MATCH_FAIL;
    }

    SetCellRef(amx, params[3], 0);
    return REGEX_NO_MATCH;
}

// native Regex:regex_match_all(const string[], const pattern[], flags = 0, error[]= "", maxLen = 0, &errcode = 0);
cell AMX_NATIVE_CALL amxx_regex_match_all(AMX *amx, cell *params)
{
    std::string str = ReadAmxString(amx, params[1]);
    std::string pattern = ReadAmxString(amx, params[2]);
    cell flagsNum = params[3];
    int maxLen = (int)params[5];

    std::regex::flag_type flags = ConvertNumericFlags(flagsNum);

    RegexData *data = new RegexData();
    try {
        data->regex = std::regex(pattern, flags);
    } catch (const std::regex_error &e) {
        SetCellRef(amx, params[6], 1);
        WriteAmxString(amx, params[4], e.what(), maxLen);
        delete data;
        return REGEX_PATTERN_FAIL;
    }

    data->lastString = str;
    try {
        if (std::regex_search(str, data->match, data->regex)) {
            int handle = AllocRegexHandle(data);
            SetCellRef(amx, params[6], 0);
            return handle;
        }
    } catch (const std::regex_error &e) {
        SetCellRef(amx, params[6], 1);
        WriteAmxString(amx, params[4], e.what(), maxLen);
        delete data;
        return REGEX_MATCH_FAIL;
    }

    delete data;
    SetCellRef(amx, params[6], 0);
    return REGEX_NO_MATCH;
}

// native regex_substr(Regex:id, str_id, buffer[], maxLen);
cell AMX_NATIVE_CALL amxx_regex_substr(AMX *amx, cell *params)
{
    int handle = (int)params[1];
    int strId = (int)params[2];
    int maxLen = (int)params[4];

    RegexData *data = GetRegexData(handle);
    if (!data) return 0;

    if (strId < 0 || strId >= (int)data->match.size())
        return 0;

    std::string sub = data->match[strId].str();
    WriteAmxString(amx, params[3], sub.c_str(), maxLen);
    return 1;
}

// native regex_free(&Regex:id);
cell AMX_NATIVE_CALL amxx_regex_free(AMX *amx, cell *params)
{
    cell *addr = nullptr;
    amx_GetAddr(amx, params[1], &addr);
    if (!addr) return 0;

    int handle = (int)*addr;
    if (handle <= 0) return 0;

    auto it = g_regexHandles.find(handle);
    if (it == g_regexHandles.end()) {
        *addr = 0;
        return 0;
    }

    delete it->second;
    g_regexHandles.erase(it);
    *addr = 0;
    return 1;
}

// native regex_replace(Regex:pattern, string[], maxLen, const replace[], flags = REGEX_FORMAT_DEFAULT, &errcode = 0);
cell AMX_NATIVE_CALL amxx_regex_replace(AMX *amx, cell *params)
{
    int handle = (int)params[1];
    int maxLen = (int)params[3];
    // cell formatFlags = params[5];  // std::regex always uses format_default
    (void)params[5];

    RegexData *data = GetRegexData(handle);
    if (!data) {
        SetCellRef(amx, params[6], 1);
        return 0;
    }

    std::string str = ReadAmxString(amx, params[2]);
    std::string replace = ReadAmxString(amx, params[4]);

    int count = 0;
    try {
        // Count matches first so the return value reflects replacements made.
        std::sregex_iterator begin(str.begin(), str.end(), data->regex);
        std::sregex_iterator end;
        for (std::sregex_iterator it = begin; it != end; ++it)
            count++;

        std::string result = std::regex_replace(str, data->regex, replace);

        WriteAmxString(amx, params[2], result.c_str(), maxLen);
        SetCellRef(amx, params[6], 0);
        return count;
    } catch (const std::regex_error &) {
        SetCellRef(amx, params[6], 1);
        return 0;
    }
}

// ---- Registration ----

AMX_NATIVE_INFO regex_natives[] = {
    {"regex_compile",    amxx_regex_compile},
    {"regex_compile_ex", amxx_regex_compile_ex},
    {"regex_match",      amxx_regex_match},
    {"regex_match_c",    amxx_regex_match_c},
    {"regex_match_all",  amxx_regex_match_all},
    {"regex_match_all_c", amxx_regex_match_all_c},
    {"regex_substr",     amxx_regex_substr},
    {"regex_free",       amxx_regex_free},
    {"regex_replace",    amxx_regex_replace},
    {nullptr, nullptr}
};

void RegisterRegexNatives(AMX *amx)
{
    amx_Register(amx, regex_natives, -1);
}
