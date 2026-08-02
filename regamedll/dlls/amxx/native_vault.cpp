#include "precompiled.h"
#include "vault.h"
#include "amx.h"
#include <cstring>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>
#include <map>

cell AMX_NATIVE_CALL amxx_vault_get(AMX *amx, cell *params)
{
    cell *key_addr;
    amx_GetAddr(amx, params[1], &key_addr);
    char key[256];
    amx_GetString(key, key_addr, 0, sizeof(key));

    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    int maxlen = params[3];

    char buffer[4096];
    bool found = AMXXVault::GetInstance().Get(key, buffer, sizeof(buffer));
    amx_SetString(dest, found ? buffer : "", 0, 0, maxlen);
    return found ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_vault_set(AMX *amx, cell *params)
{
    cell *key_addr, *value_addr;
    amx_GetAddr(amx, params[1], &key_addr);
    amx_GetAddr(amx, params[2], &value_addr);
    char key[256], value[4096];
    amx_GetString(key, key_addr, 0, sizeof(key));
    amx_GetString(value, value_addr, 0, sizeof(value));

    AMXXVault::GetInstance().Set(key, value);
    return 1;
}

cell AMX_NATIVE_CALL amxx_vault_exists(AMX *amx, cell *params)
{
    cell *key_addr;
    amx_GetAddr(amx, params[1], &key_addr);
    char key[256];
    amx_GetString(key, key_addr, 0, sizeof(key));
    return AMXXVault::GetInstance().Exists(key) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_vault_remove(AMX *amx, cell *params)
{
    cell *key_addr;
    amx_GetAddr(amx, params[1], &key_addr);
    char key[256];
    amx_GetString(key, key_addr, 0, sizeof(key));
    return AMXXVault::GetInstance().Remove(key) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_vault_load(AMX *amx, cell *params)
{
    cell *filename_addr;
    amx_GetAddr(amx, params[1], &filename_addr);
    char filename[512];
    amx_GetString(filename, filename_addr, 0, sizeof(filename));
    return AMXXVault::GetInstance().Load(filename) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_vault_save(AMX *amx, cell *params)
{
    cell *filename_addr;
    amx_GetAddr(amx, params[1], &filename_addr);
    char filename[512];
    amx_GetString(filename, filename_addr, 0, sizeof(filename));
    return AMXXVault::GetInstance().Save(filename) ? 1 : 0;
}

// ===== P1: Vault 增强 =====

// vault_get_array(key, buffer[], maxlen, section, maxSectionLen)
cell AMX_NATIVE_CALL amxx_vault_get_array(AMX *amx, cell *params)
{
    cell *key_addr;
    amx_GetAddr(amx, params[1], &key_addr);
    char key[256];
    amx_GetString(key, key_addr, 0, sizeof(key));

    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    int maxlen = params[3];

    // Build full key: "key"
    char buffer[4096];
    bool found = AMXXVault::GetInstance().Get(key, buffer, sizeof(buffer));
    if (!found) {
        amx_SetString(dest, "", 0, 0, maxlen);
        return 0;
    }

    // Try to parse as comma-separated array values
    char valueCopy[4096];
    strncpy(valueCopy, buffer, sizeof(valueCopy));
    valueCopy[sizeof(valueCopy) - 1] = '\0';

    amx_SetString(dest, valueCopy, 0, 0, maxlen);
    return 1;
}

// vault_set_array(key, array[], size)
cell AMX_NATIVE_CALL amxx_vault_set_array(AMX *amx, cell *params)
{
    cell *key_addr;
    amx_GetAddr(amx, params[1], &key_addr);
    char key[256];
    amx_GetString(key, key_addr, 0, sizeof(key));

    cell *array_addr;
    amx_GetAddr(amx, params[2], &array_addr);
    int size = params[3];

    // Build comma-separated string from array
    char value[4096];
    int pos = 0;
    for (int i = 0; i < size && pos < (int)sizeof(value) - 32; i++) {
        if (i > 0) value[pos++] = ',';
        pos += snprintf(value + pos, sizeof(value) - pos, "%d", (int)array_addr[i]);
    }
    value[pos] = '\0';

    AMXXVault::GetInstance().Set(key, value);
    return 1;
}

// vaultdata_exists(key[])
cell AMX_NATIVE_CALL amxx_vaultdata_exists(AMX *amx, cell *params)
{
    cell *key_addr;
    amx_GetAddr(amx, params[1], &key_addr);
    char key[256];
    amx_GetString(key, key_addr, 0, sizeof(key));
    return AMXXVault::GetInstance().Exists(key) ? 1 : 0;
}

// get_vaultdata(key[], buffer[], maxlen)
cell AMX_NATIVE_CALL amxx_get_vaultdata(AMX *amx, cell *params)
{
    cell *key_addr;
    amx_GetAddr(amx, params[1], &key_addr);
    char key[256];
    amx_GetString(key, key_addr, 0, sizeof(key));

    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    int maxlen = params[3];

    char buffer[4096];
    bool found = AMXXVault::GetInstance().Get(key, buffer, sizeof(buffer));
    amx_SetString(dest, found ? buffer : "", 0, 0, maxlen);
    return found ? 1 : 0;
}

// set_vaultdata(key[], value[])
cell AMX_NATIVE_CALL amxx_set_vaultdata(AMX *amx, cell *params)
{
    cell *key_addr, *value_addr;
    amx_GetAddr(amx, params[1], &key_addr);
    amx_GetAddr(amx, params[2], &value_addr);
    char key[256], value[4096];
    amx_GetString(key, key_addr, 0, sizeof(key));
    amx_GetString(value, value_addr, 0, sizeof(value));

    AMXXVault::GetInstance().Set(key, value);
    return 1;
}

// ===== P2-4: nVault persistence =====
// Per-handle vault data with disk persistence.
// On nvault_open  : load <name>.vault from addons/amxmodx/data/vault/
// On nvault_close : serialize all key-value pairs back to that file.

struct NVaultEntry {
    std::string value;
    time_t timestamp;
    NVaultEntry() : timestamp(0) {}
};

struct NVaultData {
    std::string name;
    std::string filepath;
    std::map<std::string, NVaultEntry> data;
};

static std::vector<NVaultData *> g_nvaultHandles;

static void amxx_nvault_buildpath(const char *name, char *out, size_t outlen)
{
#ifdef __ANDROID__
    snprintf(out, outlen, "/sdcard/xash/cstrike/addons/amxmodx/data/vault/%s.vault", name);
#else
    snprintf(out, outlen, "cstrike/addons/amxmodx/data/vault/%s.vault", name);
#endif
}

static NVaultData *amxx_nvault_get(int handle)
{
    if (handle < 1 || handle > (int)g_nvaultHandles.size())
        return nullptr;
    return g_nvaultHandles[handle - 1];
}

// nvault_open(name[]) - returns 1-based handle or -1 on failure
cell AMX_NATIVE_CALL amxx_nvault_open(AMX *amx, cell *params)
{
    cell *name_addr;
    amx_GetAddr(amx, params[1], &name_addr);
    char name[256];
    amx_GetString(name, name_addr, 0, sizeof(name));
    if (!name[0]) return -1;

    NVaultData *vault = new NVaultData();
    vault->name = name;
    char filepath[512];
    amxx_nvault_buildpath(name, filepath, sizeof(filepath));
    vault->filepath = filepath;

    // Read existing key-value pairs from disk (one "key=value[\ttimestamp]" per line).
    FILE *fp = fopen(filepath, "r");
    if (fp) {
        char line[4096];
        while (fgets(line, sizeof(line), fp)) {
            size_t len = strlen(line);
            while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
                line[--len] = '\0';
            if (len == 0 || line[0] == ';' || line[0] == '#')
                continue;
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = '\0';
            char *valStart = eq + 1;
            // 查找可选的 \t<timestamp>
            char *tab = strchr(valStart, '\t');
            NVaultEntry entry;
            if (tab) {
                *tab = '\0';
                entry.value = valStart;
                entry.timestamp = (time_t)strtoll(tab + 1, nullptr, 10);
            } else {
                entry.value = valStart;
                entry.timestamp = 0;
            }
            vault->data[line] = entry;
        }
        fclose(fp);
    }

    g_nvaultHandles.push_back(vault);
    return (cell)g_nvaultHandles.size();
}

// nvault_close(handle) - flush to disk and release the handle
cell AMX_NATIVE_CALL amxx_nvault_close(AMX *amx, cell *params)
{
    (void)amx;
    int handle = (int)params[1];
    NVaultData *vault = amxx_nvault_get(handle);
    if (!vault) return 0;

    FILE *fp = fopen(vault->filepath.c_str(), "w");
    if (fp) {
        fprintf(fp, "; nVault: %s\n", vault->name.c_str());
        for (auto &kv : vault->data)
            fprintf(fp, "%s=%s\t%lld\n", kv.first.c_str(), kv.second.value.c_str(), (long long)kv.second.timestamp);
        fclose(fp);
    }

    delete vault;
    g_nvaultHandles[handle - 1] = nullptr;
    return 1;
}

// nvault_get(handle, key[], buffer[], maxlen) - returns value length or -1
cell AMX_NATIVE_CALL amxx_nvault_get(AMX *amx, cell *params)
{
    NVaultData *vault = amxx_nvault_get((int)params[1]);
    if (!vault) return -1;

    cell *key_addr;
    amx_GetAddr(amx, params[2], &key_addr);
    char key[256];
    amx_GetString(key, key_addr, 0, sizeof(key));

    cell *dest;
    amx_GetAddr(amx, params[3], &dest);
    int maxlen = (int)params[4];

    auto it = vault->data.find(key);
    if (it == vault->data.end()) {
        if (dest) amx_SetString(dest, "", 0, 0, maxlen);
        return -1;
    }
    if (dest) amx_SetString(dest, it->second.value.c_str(), 0, 0, maxlen);
    return (cell)it->second.value.length();
}

// nvault_set(handle, key[], value[])
cell AMX_NATIVE_CALL amxx_nvault_set(AMX *amx, cell *params)
{
    NVaultData *vault = amxx_nvault_get((int)params[1]);
    if (!vault) return 0;

    cell *key_addr, *value_addr;
    amx_GetAddr(amx, params[2], &key_addr);
    amx_GetAddr(amx, params[3], &value_addr);
    char key[256], value[4096];
    amx_GetString(key, key_addr, 0, sizeof(key));
    amx_GetString(value, value_addr, 0, sizeof(value));

    NVaultEntry entry;
    entry.value = value;
    entry.timestamp = time(nullptr);
    vault->data[key] = entry;
    return 1;
}

// nvault_remove(handle, key[])
cell AMX_NATIVE_CALL amxx_nvault_remove(AMX *amx, cell *params)
{
    NVaultData *vault = amxx_nvault_get((int)params[1]);
    if (!vault) return 0;

    cell *key_addr;
    amx_GetAddr(amx, params[2], &key_addr);
    char key[256];
    amx_GetString(key, key_addr, 0, sizeof(key));

    auto it = vault->data.find(key);
    if (it == vault->data.end()) return 0;
    vault->data.erase(it);
    return 1;
}

// ===== P2-3: nvault 高级 API =====

// nvault_pset(handle, key[], value[]) - 设置值但不更新时间戳（永久）
cell AMX_NATIVE_CALL amxx_nvault_pset(AMX *amx, cell *params)
{
    NVaultData *vault = amxx_nvault_get((int)params[1]);
    if (!vault) return 0;

    cell *key_addr, *value_addr;
    amx_GetAddr(amx, params[2], &key_addr);
    amx_GetAddr(amx, params[3], &value_addr);
    char key[256], value[4096];
    amx_GetString(key, key_addr, 0, sizeof(key));
    amx_GetString(value, value_addr, 0, sizeof(value));

    NVaultEntry entry;
    entry.value = value;
    entry.timestamp = 0;  // 永久：不更新时间戳
    vault->data[key] = entry;
    return 1;
}

// nvault_touch(handle, key[], timestamp=-1) - 更新时间戳，不改变值
cell AMX_NATIVE_CALL amxx_nvault_touch(AMX *amx, cell *params)
{
    (void)amx;
    NVaultData *vault = amxx_nvault_get((int)params[1]);
    if (!vault) return 0;

    cell *key_addr;
    amx_GetAddr(amx, params[2], &key_addr);
    char key[256];
    amx_GetString(key, key_addr, 0, sizeof(key));

    auto it = vault->data.find(key);
    if (it == vault->data.end()) return 0;

    time_t ts = (params[0] / sizeof(cell) >= 3 && params[3] != -1)
                    ? (time_t)params[3]
                    : time(nullptr);
    it->second.timestamp = ts;
    return 1;
}

// nvault_prune(handle, start, end) - 删除时间戳在 [start, end] 范围内的条目，返回删除数
cell AMX_NATIVE_CALL amxx_nvault_prune(AMX *amx, cell *params)
{
    (void)amx;
    NVaultData *vault = amxx_nvault_get((int)params[1]);
    if (!vault) return 0;

    time_t start = (time_t)params[2];
    time_t end = (time_t)params[3];
    int removed = 0;

    for (auto it = vault->data.begin(); it != vault->data.end(); ) {
        if (it->second.timestamp >= start && it->second.timestamp <= end) {
            it = vault->data.erase(it);
            removed++;
        } else {
            ++it;
        }
    }
    return removed;
}

// nvault_lookup(handle, key[], value[], len, &timestamp) - 查找键，返回值和时间戳
cell AMX_NATIVE_CALL amxx_nvault_lookup(AMX *amx, cell *params)
{
    NVaultData *vault = amxx_nvault_get((int)params[1]);
    if (!vault) return 0;

    cell *key_addr;
    amx_GetAddr(amx, params[2], &key_addr);
    char key[256];
    amx_GetString(key, key_addr, 0, sizeof(key));

    auto it = vault->data.find(key);
    if (it == vault->data.end()) return 0;

    cell *dest;
    amx_GetAddr(amx, params[3], &dest);
    if (dest) amx_SetString(dest, it->second.value.c_str(), 0, 0, (int)params[4]);

    cell *tsAddr;
    amx_GetAddr(amx, params[5], &tsAddr);
    if (tsAddr) *tsAddr = (cell)it->second.timestamp;

    return 1;
}

// remove_vaultdata(key[]) / delete_vaultdata(key[]) - 旧版 vault API 别名
cell AMX_NATIVE_CALL amxx_remove_vaultdata(AMX *amx, cell *params)
{
    cell *key_addr;
    amx_GetAddr(amx, params[1], &key_addr);
    char key[256];
    amx_GetString(key, key_addr, 0, sizeof(key));
    return AMXXVault::GetInstance().Remove(key) ? 1 : 0;
}

AMX_NATIVE_INFO vault_natives[] = {
    {"vault_get", amxx_vault_get},
    {"vault_set", amxx_vault_set},
    {"vault_exists", amxx_vault_exists},
    {"vault_remove", amxx_vault_remove},
    {"vault_load", amxx_vault_load},
    {"vault_save", amxx_vault_save},
    {"vault_get_array", amxx_vault_get_array},
    {"vault_set_array", amxx_vault_set_array},
    {"vaultdata_exists", amxx_vaultdata_exists},
    {"get_vaultdata", amxx_get_vaultdata},
    {"set_vaultdata", amxx_set_vaultdata},

    // P2-4: nVault persistence
    {"nvault_open", amxx_nvault_open},
    {"nvault_close", amxx_nvault_close},
    {"nvault_get", amxx_nvault_get},
    {"nvault_set", amxx_nvault_set},
    {"nvault_remove", amxx_nvault_remove},
    // P2-3: nvault 高级 API
    {"nvault_pset", amxx_nvault_pset},
    {"nvault_touch", amxx_nvault_touch},
    {"nvault_prune", amxx_nvault_prune},
    {"nvault_lookup", amxx_nvault_lookup},
    // P2-3: 旧版 vault API 别名
    {"remove_vaultdata", amxx_remove_vaultdata},
    {"delete_vaultdata", amxx_remove_vaultdata},
    {nullptr, nullptr}
};

void RegisterVaultNatives(AMX *amx)
{
    amx_Register(amx, vault_natives, -1);
}
