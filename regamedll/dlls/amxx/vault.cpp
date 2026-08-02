#include "precompiled.h"
#include "vault.h"
#include "amx.h"
#include <cstdio>
#include <cstring>

AMXXVault &AMXXVault::GetInstance()
{
    static AMXXVault instance;
    return instance;
}

AMXXVault::AMXXVault()
{
}

void AMXXVault::Init()
{
    AMXX_LOG("[Vault] Initializing vault system...");
    m_data.clear();

    // 通过引擎获取游戏目录 (gamedir)，Android 下即 /sdcard/xash/cstrike 等实际路径
    char gameDir[256] = {0};
    GET_GAME_DIR(gameDir);
    m_defaultFile = gameDir;
    m_defaultFile += "/addons/amxmodx/data/vault.ini";

    Load(m_defaultFile.c_str());
}

bool AMXXVault::Load(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        AMXX_LOG("[Vault] File not found: %s", filename);
        return false;
    }

    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        if (len == 0 || line[0] == ';' || line[0] == '#')
            continue;

        char *eq = strchr(line, '=');
        if (!eq)
            continue;

        *eq = '\0';
        char *key = line;
        char *value = eq + 1;

        char *end = key + strlen(key) - 1;
        while (end > key && isspace((unsigned char)*end)) *end-- = '\0';
        while (*key && isspace((unsigned char)*key)) key++;

        while (*value && isspace((unsigned char)*value)) value++;

        VaultEntry entry;
        entry.value = value;
        entry.timestamp = 0.0;
        m_data[key] = entry;
    }

    fclose(fp);
    AMXX_LOG("[Vault] Loaded %zu entries from %s", m_data.size(), filename);
    return true;
}

bool AMXXVault::Save(const char *filename)
{
    FILE *fp = fopen(filename, "w");
    if (!fp)
        return false;

    fprintf(fp, "; AMXX Vault\n");
    for (auto &kv : m_data) {
        fprintf(fp, "%s=%s\n", kv.first.c_str(), kv.second.value.c_str());
    }

    fclose(fp);
    AMXX_LOG("[Vault] Saved %zu entries to %s", m_data.size(), filename);
    return true;
}

bool AMXXVault::Exists(const char *key)
{
    return m_data.find(key) != m_data.end();
}

bool AMXXVault::Get(const char *key, char *dest, int maxlen)
{
    auto it = m_data.find(key);
    if (it == m_data.end())
        return false;

    strncpy(dest, it->second.value.c_str(), maxlen - 1);
    dest[maxlen - 1] = '\0';
    return true;
}

bool AMXXVault::Set(const char *key, const char *value, double timestamp)
{
    VaultEntry entry;
    entry.value = value ? value : "";
    entry.timestamp = timestamp;
    m_data[key] = entry;
    if (!m_defaultFile.empty())
        Save(m_defaultFile.c_str());
    return true;
}

bool AMXXVault::Remove(const char *key)
{
    auto it = m_data.find(key);
    if (it == m_data.end())
        return false;
    m_data.erase(it);
    if (!m_defaultFile.empty())
        Save(m_defaultFile.c_str());
    return true;
}
