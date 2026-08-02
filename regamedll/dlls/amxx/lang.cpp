#include "precompiled.h"
#include "lang.h"
#include "amx.h"
#include <cstdio>
#include <cstring>
#include <cctype>

// Global current language target for the %L format handler.
//   0            -> server
//   LANG_PLAYER  -> per-player (resolved through this same variable)
//   >=1          -> specific player index
int g_currentLangTarget = LANG_SERVER;

void SetCurrentLangTarget(int playerIndex)
{
    g_currentLangTarget = playerIndex;
}

int GetCurrentLangTarget()
{
    return g_currentLangTarget;
}

AMXXLang &AMXXLang::GetInstance()
{
    static AMXXLang instance;
    return instance;
}

AMXXLang::AMXXLang()
{
    m_serverLang = "en";
}

void AMXXLang::Init()
{
    AMXX_LOG_DBG("[Lang] Initializing language system...");
    m_entries.clear();
    m_playerLangs.clear();
    m_keyList.clear();
    m_keyTable.clear();
    m_serverLang = "en";
    g_currentLangTarget = LANG_SERVER;
}

bool AMXXLang::LoadDictionary(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("[AMXX] [Lang] Dictionary NOT FOUND: %s\n", filename);
        AMXX_LOG("[Lang] Dictionary not found: %s", filename);
        return false;
    }

    printf("[AMXX] [Lang] Loading dictionary: %s\n", filename);

    char line[2048];
    std::string currentLang;

    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        // strip trailing newlines
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        // skip UTF-8 BOM at start of file
        if (len >= 3 && (unsigned char)line[0] == 0xEF &&
            (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF) {
            memmove(line, line + 3, len - 3 + 1);
            len -= 3;
        }

        if (len == 0 || line[0] == ';' || line[0] == '#')
            continue;

        // language section header, e.g. [en]
        if (line[0] == '[' && len >= 2 && line[len - 1] == ']') {
            line[len - 1] = '\0';
            currentLang = line + 1;
            // trim surrounding whitespace from the language code
            size_t s = currentLang.find_first_not_of(" \t");
            size_t e = currentLang.find_last_not_of(" \t");
            if (s == std::string::npos) {
                currentLang.clear();
            } else {
                currentLang = currentLang.substr(s, e - s + 1);
            }
            continue;
        }

        char *eq = strchr(line, '=');
        if (!eq || currentLang.empty())
            continue;

        *eq = '\0';
        char *key = line;        // translation key, e.g. PRINT_ALL
        char *value = eq + 1;    // translation text

        // trim trailing/leading whitespace from key
        char *end = key + strlen(key) - 1;
        while (end > key && isspace((unsigned char)*end)) *end-- = '\0';
        while (*key && isspace((unsigned char)*key)) key++;

        // strip a trailing ':' on the key (AMXX multi-line colon form)
        size_t klen = strlen(key);
        if (klen > 0 && key[klen - 1] == ':')
            key[klen - 1] = '\0';

        while (*value && isspace((unsigned char)*value)) value++;

        if (!key[0])
            continue;

        // Register the translation key in the TransKey tables so that
        // GetLangTransKey/CreateLangKey can resolve keys loaded from files.
        AddKeyEntry(key);

        // Store: m_entries[translationKey][languageCode] = translation
        m_entries[key][currentLang] = value;
    }

    fclose(fp);
    AMXX_LOG_DBG("[Lang] Loaded dictionary: %s", filename);
    return true;
}

bool AMXXLang::GetString(const char *key, const char *lang, char *dest, int maxlen)
{
    if (!key || !dest || maxlen <= 0)
        return false;

    auto it = m_entries.find(key);
    if (it == m_entries.end()) {
        strncpy(dest, key, maxlen - 1);
        dest[maxlen - 1] = '\0';
        return false;
    }

    // nullptr/empty lang -> server language (legacy behaviour).
    const char *useLang = (lang && lang[0]) ? lang : m_serverLang.c_str();

    auto langIt = it->second.find(useLang);
    if (langIt == it->second.end()) {
        // fall back to english
        langIt = it->second.find("en");
    }

    if (langIt == it->second.end()) {
        // not found: return the key itself
        strncpy(dest, key, maxlen - 1);
        dest[maxlen - 1] = '\0';
        return false;
    }

    strncpy(dest, langIt->second.c_str(), maxlen - 1);
    dest[maxlen - 1] = '\0';
    return true;
}

void AMXXLang::SetPlayerLang(int playerIndex, const char *langCode)
{
    if (playerIndex <= 0)
        return;
    if (!langCode || !langCode[0]) {
        m_playerLangs.erase(playerIndex);
        return;
    }
    m_playerLangs[playerIndex] = langCode;
}

const char *AMXXLang::GetPlayerLang(int playerIndex)
{
    if (playerIndex <= 0)
        return m_serverLang.c_str();

    auto it = m_playerLangs.find(playerIndex);
    if (it == m_playerLangs.end())
        return "en";

    return it->second.c_str();
}

const char *AMXXLang::GetLangForTarget(int playerIndex)
{
    if (playerIndex == LANG_SERVER || playerIndex == 0)
        return m_serverLang.c_str();

    if (playerIndex == LANG_PLAYER) {
        // LANG_PLAYER resolves through the global trans target, which is set
        // by SetGlobalTransTarget before invoking format functions.
        int target = g_currentLangTarget;
        if (target == LANG_SERVER || target == 0)
            return m_serverLang.c_str();
        return GetPlayerLang(target);
    }

    return GetPlayerLang(playerIndex);
}

bool AMXXLang::LangKeyExists(const char *key)
{
    if (!key) return false;
    return m_entries.find(key) != m_entries.end();
}

void AMXXLang::AddLangKey(const char *key)
{
    if (!key || !key[0]) return;
    if (m_entries.find(key) == m_entries.end()) {
        m_entries[key] = std::map<std::string, std::string>();
    }
    AddKeyEntry(key);
}

bool AMXXLang::AddTranslation(const char *key, const char *lang, const char *value)
{
    if (!key || !lang || !value) return false;
    AddKeyEntry(key);
    m_entries[key][lang] = value;
    return true;
}

bool AMXXLang::LangExists(const char *lang)
{
    if (!lang || !lang[0]) return false;
    for (auto &entry : m_entries) {
        if (entry.second.find(lang) != entry.second.end()) {
            return true;
        }
    }
    return false;
}

int AMXXLang::GetKeyEntry(const char *key)
{
    if (!key || !key[0]) return -1;
    auto it = m_keyTable.find(key);
    if (it == m_keyTable.end())
        return -1;
    return it->second;
}

int AMXXLang::AddKeyEntry(const char *key)
{
    if (!key || !key[0]) return -1;
    auto it = m_keyTable.find(key);
    if (it != m_keyTable.end())
        return it->second;

    int index = (int)m_keyList.size();
    m_keyList.push_back(key);
    m_keyTable[key] = index;
    return index;
}

const char *AMXXLang::GetKey(int index)
{
    if (index < 0 || index >= (int)m_keyList.size())
        return nullptr;
    return m_keyList[index].c_str();
}
