#pragma once
#ifndef AMXX_LANG_H
#define AMXX_LANG_H

#include <string>
#include <map>
#include <vector>

// Language target sentinels, matching original AMXX semantics.
#define LANG_SERVER 0
#define LANG_PLAYER  (-1)

// Global current language target used by the %L format handler.
//   0            -> server (use m_serverLang)
//   LANG_PLAYER  -> resolve through the global trans target (per-player)
//   >=1          -> specific player index (per-player)
//
// native_core.cpp's amxx_format_string can read this to resolve per-player
// language when the %L lang parameter is a player id / LANG_PLAYER. The actual
// integration with the %L handler is performed separately; this declaration
// exposes the hook so that work can be done without touching the lang core.
extern int g_currentLangTarget;

// Setters/getters for the global current language target.
void SetCurrentLangTarget(int playerIndex);
int GetCurrentLangTarget();

class AMXXLang
{
public:
    static AMXXLang &GetInstance();

    void Init();
    bool LoadDictionary(const char *filename);

    // Look up a translation. If `lang` is nullptr, the server language is used
    // (current/legacy behaviour). If `lang` is provided, that language is
    // tried first, then "en", then the key itself is copied to dest.
    bool GetString(const char *key, const char *lang, char *dest, int maxlen);

    void SetServerLang(const char *lang) { m_serverLang = lang ? lang : "en"; }
    const char *GetServerLang() const { return m_serverLang.c_str(); }

    // Per-player language support
    void SetPlayerLang(int playerIndex, const char *langCode);
    const char *GetPlayerLang(int playerIndex);

    // Resolve a language target (LANG_SERVER / LANG_PLAYER / player index) to
    // a language code string. LANG_PLAYER resolves through g_currentLangTarget.
    const char *GetLangForTarget(int playerIndex);

    // New methods for dynamic language manipulation
    bool LangKeyExists(const char *key);
    void AddLangKey(const char *key);
    bool AddTranslation(const char *key, const char *lang, const char *value);
    bool LangExists(const char *lang);

    // TransKey handle management (key string <-> integer index).
    // These back the GetLangTransKey / CreateLangKey / AddTranslation natives.
    int  GetKeyEntry(const char *key);          // returns index or -1
    int  AddKeyEntry(const char *key);          // create-or-return existing index
    const char *GetKey(int index);              // returns key string or nullptr

private:
    AMXXLang();

    std::string m_serverLang;
    // outer key = translation key, inner key = language code
    std::map<std::string, std::map<std::string, std::string>> m_entries;
    // per-player language: playerIndex -> language code
    std::map<int, std::string> m_playerLangs;
    // TransKey handle tables
    std::vector<std::string> m_keyList;
    std::map<std::string, int> m_keyTable;
};

#endif
