#include "precompiled.h"
#include "lang.h"
#include "amx.h"
#include "../enginecallback.h"
#include <cstring>

cell AMX_NATIVE_CALL amxx_register_dictionary(AMX *amx, cell *params)
{
    (void)amx;
    cell *filename_addr;
    amx_GetAddr(amx, params[1], &filename_addr);
    char filename[256];
    amx_GetString(filename, filename_addr, 0, sizeof(filename));

    // 构建完整路径: <gamedir>/addons/amxmodx/data/lang/<filename>
    // 原版 AMXX: build_pathname_r("%s/lang/%s", amxx_datadir, filename)
    char gameDir[256] = {0};
    GET_GAME_DIR(gameDir);
    char fullpath[512];
    snprintf(fullpath, sizeof(fullpath), "%s/addons/amxmodx/data/lang/%s", gameDir, filename);

    AMXXLang::GetInstance().LoadDictionary(fullpath);
    return 1;
}

cell AMX_NATIVE_CALL amxx_get_lang(AMX *amx, cell *params)
{
    cell *key_addr, *lang_addr;
    amx_GetAddr(amx, params[1], &key_addr);
    amx_GetAddr(amx, params[2], &lang_addr);

    char key[256], lang[64];
    amx_GetString(key, key_addr, 0, sizeof(key));
    amx_GetString(lang, lang_addr, 0, sizeof(lang));

    cell *dest;
    amx_GetAddr(amx, params[3], &dest);
    int maxlen = params[4];

    char buffer[2048];
    bool found = AMXXLang::GetInstance().GetString(key, lang, buffer, sizeof(buffer));
    amx_SetString(dest, buffer, 0, 0, maxlen);
    return found ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_get_svlang(AMX *amx, cell *params)
{
    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    amx_SetString(dest, AMXXLang::GetInstance().GetServerLang(), 0, 0, params[2]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_get_langsnum(AMX *amx, cell *params)
{
    (void)amx;
    (void)params;
    return 2;
}

// CreateLangKey(const key[])
// Creates a new translation key (or returns the existing one's index).
cell AMX_NATIVE_CALL amxx_create_lang_key(AMX *amx, cell *params)
{
    cell *key_addr;
    amx_GetAddr(amx, params[1], &key_addr);
    char key[256];
    amx_GetString(key, key_addr, 0, sizeof(key));

    // AddKeyEntry returns the existing index if the key already exists,
    // otherwise creates a new entry and returns its index.
    return (cell)AMXXLang::GetInstance().AddKeyEntry(key);
}

// GetLangTransKey(const key[])
// Returns the translation key handle (index) for the given key string,
// or -1 if the key is not registered.
cell AMX_NATIVE_CALL amxx_GetLangTransKey(AMX *amx, cell *params)
{
    cell *key_addr;
    amx_GetAddr(amx, params[1], &key_addr);
    char key[256];
    amx_GetString(key, key_addr, 0, sizeof(key));

    return (cell)AMXXLang::GetInstance().GetKeyEntry(key);
}

// LookupLangKey(dest[], len, const key[], &id = 0)
// Looks up a translation for `key` using the language resolved from `id`
// (LANG_SERVER / LANG_PLAYER / player index). Writes the result to `dest`.
// Returns 1 on success, 0 on failure.
cell AMX_NATIVE_CALL amxx_LookupLangKey(AMX *amx, cell *params)
{
    // params[1] = dest buffer
    // params[2] = dest length
    // params[3] = key string
    // params[4] = by-ref id (lang target: LANG_SERVER / LANG_PLAYER / player)
    cell *key_addr;
    amx_GetAddr(amx, params[3], &key_addr);
    char key[256];
    amx_GetString(key, key_addr, 0, sizeof(key));

    cell *id_addr;
    amx_GetAddr(amx, params[4], &id_addr);
    int langTarget = (int)(*id_addr);

    const char *langCode = AMXXLang::GetInstance().GetLangForTarget(langTarget);

    cell *dest;
    amx_GetAddr(amx, params[1], &dest);
    int outlen = params[2];

    char buffer[2048];
    bool found = AMXXLang::GetInstance().GetString(key, langCode, buffer, sizeof(buffer));

    amx_SetString(dest, buffer, 0, 0, outlen);
    return found ? 1 : 0;
}

// AddTranslation(const lang[2], TransKey:key, const phrase[])
// Adds a translation for a specific language and key handle.
// Returns 1 on success, 0 on failure (invalid key handle).
cell AMX_NATIVE_CALL amxx_add_translation(AMX *amx, cell *params)
{
    cell *lang_addr;
    amx_GetAddr(amx, params[1], &lang_addr);
    char lang[8];
    amx_GetString(lang, lang_addr, 0, sizeof(lang));

    int keyIndex = (int)params[2];

    cell *phrase_addr;
    amx_GetAddr(amx, params[3], &phrase_addr);
    char phrase[2048];
    amx_GetString(phrase, phrase_addr, 0, sizeof(phrase));

    const char *keyStr = AMXXLang::GetInstance().GetKey(keyIndex);
    if (!keyStr)
        return 0;

    return AMXXLang::GetInstance().AddTranslation(keyStr, lang, phrase) ? 1 : 0;
}

// SetGlobalTransTarget(player)
// Sets the player index used as the language target for subsequent %L
// translations in the current format context (LANG_PLAYER resolution).
cell AMX_NATIVE_CALL amxx_set_global_trans_target(AMX *amx, cell *params)
{
    (void)amx;
    SetCurrentLangTarget((int)params[1]);
    return 1;
}

// get_user_langid(index) — 返回用户偏好语言的索引（0-based，对应 get_langsnum 列表）
// 原版语义：读取客户端 info_keyvalue "_language"（"en", "zh_cn", ...）后在语言表查找。
// 找不到或语言未注册 -> 返回 0。
cell AMX_NATIVE_CALL amxx_get_user_langid(AMX *amx, cell *params)
{
    int index = (int)params[1];
    if (index < 1 || index > gpGlobals->maxClients) return 0;
    edict_t *pEdict = INDEXENT(index);
    if (!pEdict || pEdict->free || pEdict->pvPrivateData == nullptr) return 0;

    const char *lang = g_engfuncs.pfnGetInfoKeyBuffer(pEdict)
        ? g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(pEdict), "_language")
        : nullptr;
    if (!lang || !*lang) lang = "en";

    if (strcmp(lang, "en") == 0) return 0;
    static const char *kExtra[] = {"zh","de","fr","es","ru","pt","it","pl","nl","tr","cs","hu","ko","ja","bg","sv"};
    for (int i = 0; i < (int)(sizeof(kExtra)/sizeof(kExtra[0])); i++) {
        if (strncmp(lang, kExtra[i], 64) == 0) return 1 + i;
    }
    return 0;
}

// lang_exists(lang[])
cell AMX_NATIVE_CALL amxx_lang_exists(AMX *amx, cell *params)
{
    cell *lang_addr;
    amx_GetAddr(amx, params[1], &lang_addr);
    char lang[64];
    amx_GetString(lang, lang_addr, 0, sizeof(lang));
    return AMXXLang::GetInstance().LangExists(lang) ? 1 : 0;
}

AMX_NATIVE_INFO lang_natives[] = {
    {"register_dictionary",    amxx_register_dictionary},
    {"get_lang",               amxx_get_lang},
    {"get_svlang",             amxx_get_svlang},
    {"get_langsnum",           amxx_get_langsnum},
    {"LookupLangKey",          amxx_LookupLangKey},
    {"GetLangTransKey",        amxx_GetLangTransKey},
    {"CreateLangKey",          amxx_create_lang_key},
    {"AddTranslation",         amxx_add_translation},
    {"SetGlobalTransTarget",   amxx_set_global_trans_target},
    {"lang_exists",            amxx_lang_exists},
    {"get_user_langid", amxx_get_user_langid},
    {nullptr, nullptr}
};

void RegisterLangNatives(AMX *amx)
{
    amx_Register(amx, lang_natives, -1);
}
