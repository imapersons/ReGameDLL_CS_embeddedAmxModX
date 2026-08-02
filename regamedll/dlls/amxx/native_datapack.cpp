#include "precompiled.h"
#include "native_datapack.h"
#include "amx.h"
#include <map>
#include <vector>
#include <string>
#include <cstring>

// ==============================================
// DataPack module
// ==============================================
//
// Provides a sequential buffer that can pack and read arbitrary data types
// (cells, floats, strings). Each datapack is identified by a 1-based handle
// (0 is invalid). Strings are stored as: [length+1 cell][char cells...][0 cell].
//
// Handle management uses a static std::map<int, DataPackData*>. ResetDataPackHandles
// frees all datapacks and is intended to be called on map change.

struct DataPackData {
    std::vector<cell> data;
    size_t readPos;
};

static std::map<int, DataPackData*> g_dataPacks;
static int g_nextDataPackHandle = 1;

static DataPackData *GetDataPack(int handle)
{
    auto it = g_dataPacks.find(handle);
    if (it == g_dataPacks.end() || !it->second) return nullptr;
    return it->second;
}

// native DataPack:CreateDataPack();
cell AMX_NATIVE_CALL amxx_create_datapack(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    DataPackData *dp = new DataPackData();
    dp->readPos = 0;
    int handle = g_nextDataPackHandle++;
    g_dataPacks[handle] = dp;
    return (cell)handle;
}

// native WritePackCell(DataPack:pack, any:cell);
cell AMX_NATIVE_CALL amxx_write_pack_cell(AMX *amx, cell *params)
{
    (void)amx;
    DataPackData *dp = GetDataPack((int)params[1]);
    if (!dp) return 0;
    dp->data.push_back(params[2]);
    return 1;
}

// native WritePackFloat(DataPack:pack, Float:val);
cell AMX_NATIVE_CALL amxx_write_pack_float(AMX *amx, cell *params)
{
    (void)amx;
    DataPackData *dp = GetDataPack((int)params[1]);
    if (!dp) return 0;
    float val = amx_ctof(params[2]);
    dp->data.push_back(amx_ftoc(val));
    return 1;
}

// native WritePackString(DataPack:pack, const str[]);
cell AMX_NATIVE_CALL amxx_write_pack_string(AMX *amx, cell *params)
{
    (void)amx;
    DataPackData *dp = GetDataPack((int)params[1]);
    if (!dp) return 0;

    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    if (!addr) return 0;

    char str[512];
    amx_GetString(str, addr, 0, sizeof(str));

    // Pack: length (including null) as a cell, then each char as a cell,
    // followed by a null terminator cell.
    size_t len = strlen(str);
    dp->data.push_back((cell)(len + 1));
    for (size_t i = 0; i < len; i++)
        dp->data.push_back((cell)(unsigned char)str[i]);
    dp->data.push_back(0);
    return 1;
}

// native any:ReadPackCell(DataPack:pack);
cell AMX_NATIVE_CALL amxx_read_pack_cell(AMX *amx, cell *params)
{
    (void)amx;
    DataPackData *dp = GetDataPack((int)params[1]);
    if (!dp) return 0;
    if (dp->readPos >= dp->data.size()) return 0;
    return dp->data[dp->readPos++];
}

// native Float:ReadPackFloat(DataPack:pack);
cell AMX_NATIVE_CALL amxx_read_pack_float(AMX *amx, cell *params)
{
    (void)amx;
    DataPackData *dp = GetDataPack((int)params[1]);
    if (!dp) return 0;
    if (dp->readPos >= dp->data.size()) return 0;
    cell c = dp->data[dp->readPos++];
    float val = amx_ctof(c);
    return amx_ftoc(val);
}

// native ReadPackString(DataPack:pack, buffer[], maxlen);
cell AMX_NATIVE_CALL amxx_read_pack_string(AMX *amx, cell *params)
{
    DataPackData *dp = GetDataPack((int)params[1]);
    if (!dp) return 0;
    if (dp->readPos >= dp->data.size()) return 0;

    cell lenCell = dp->data[dp->readPos++];
    int len = (int)lenCell;
    if (len < 0) return 0;
    if (dp->readPos + (size_t)len > dp->data.size())
        len = (int)(dp->data.size() - dp->readPos);

    std::string buf;
    buf.reserve(len);
    for (int i = 0; i < len; i++)
        buf.push_back((char)(unsigned char)dp->data[dp->readPos++]);

    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    if (!dest) return 0;
    amx_SetString(dest, buf.c_str(), 0, 0, (size_t)params[3]);
    return 1;
}

// native ResetPack(DataPack:pack, bool:clear = false);
cell AMX_NATIVE_CALL amxx_reset_pack(AMX *amx, cell *params)
{
    (void)amx;
    DataPackData *dp = GetDataPack((int)params[1]);
    if (!dp) return 0;
    dp->readPos = 0;

    int numParams = (int)(params[0] / sizeof(cell));
    bool clear = (numParams >= 2) ? (params[2] != 0) : false;
    if (clear)
        dp->data.clear();
    return 1;
}

// native DataPackPos:GetPackPosition(DataPack:pack);
cell AMX_NATIVE_CALL amxx_get_pack_position(AMX *amx, cell *params)
{
    (void)amx;
    DataPackData *dp = GetDataPack((int)params[1]);
    if (!dp) return 0;
    return (cell)dp->readPos;
}

// native SetPackPosition(DataPack:pack, DataPackPos:position);
cell AMX_NATIVE_CALL amxx_set_pack_position(AMX *amx, cell *params)
{
    (void)amx;
    DataPackData *dp = GetDataPack((int)params[1]);
    if (!dp) return 0;
    dp->readPos = (size_t)params[2];
    return 1;
}

// native bool:IsPackEnded(DataPack:pack);
cell AMX_NATIVE_CALL amxx_is_pack_ended(AMX *amx, cell *params)
{
    (void)amx;
    DataPackData *dp = GetDataPack((int)params[1]);
    if (!dp) return 1;
    return dp->readPos >= dp->data.size() ? 1 : 0;
}

// native DestroyDataPack(&DataPack:pack);
cell AMX_NATIVE_CALL amxx_destroy_datapack(AMX *amx, cell *params)
{
    cell *ptr;
    amx_GetAddr(amx, params[1], &ptr);
    if (!ptr) return 0;

    int handle = (int)*ptr;
    auto it = g_dataPacks.find(handle);
    if (it == g_dataPacks.end()) return 0;

    delete it->second;
    g_dataPacks.erase(it);
    *ptr = 0;
    return 1;
}

AMX_NATIVE_INFO datapack_natives[] = {
    {"CreateDataPack",    amxx_create_datapack},
    {"WritePackCell",     amxx_write_pack_cell},
    {"WritePackFloat",    amxx_write_pack_float},
    {"WritePackString",   amxx_write_pack_string},
    {"ReadPackCell",      amxx_read_pack_cell},
    {"ReadPackFloat",     amxx_read_pack_float},
    {"ReadPackString",    amxx_read_pack_string},
    {"ResetPack",         amxx_reset_pack},
    {"GetPackPosition",   amxx_get_pack_position},
    {"SetPackPosition",   amxx_set_pack_position},
    {"IsPackEnded",       amxx_is_pack_ended},
    {"DestroyDataPack",   amxx_destroy_datapack},
    {nullptr, nullptr}
};

void RegisterDataPackNatives(AMX *amx)
{
    amx_Register(amx, datapack_natives, -1);
}

void ResetDataPackHandles()
{
    for (auto &pair : g_dataPacks) {
        delete pair.second;
    }
    g_dataPacks.clear();
    g_nextDataPackHandle = 1;
}
