#include "precompiled.h"
#include "forwards.h"
#include "plugin.h"
#include "amx.h"
#include <cstdarg>
#include <cstring>

// ========== AMXXForward (Normal Forward) ==========

AMXXForward::AMXXForward(const char *name, ForwardExecType execType, int numParams)
    : m_name(name ? name : ""), m_execType(execType), m_numParams(numParams)
{
    for (int i = 0; i < FORWARD_MAX_PARAMS; i++)
        m_paramTypes[i] = FP_DONE;
    AMXX_LOG_DBG("[Forward] Created normal forward: '%s' (execType=%d, numParams=%d)", name, execType, numParams);
}

AMXXForward::~AMXXForward()
{
    AMXX_LOG_DBG("[Forward] Destroyed normal forward: '%s' (%zu plugins)", m_name.c_str(), m_calls.size());
}

void AMXXForward::SetParamType(int index, ForwardParam type)
{
    if (index >= 0 && index < FORWARD_MAX_PARAMS)
        m_paramTypes[index] = type;
}

ForwardParam AMXXForward::GetParamType(int index) const
{
    if (index >= 0 && index < FORWARD_MAX_PARAMS)
        return m_paramTypes[index];
    return FP_DONE;
}

void AMXXForward::AddPlugin(AMXXPlugin *plugin, int funcIndex)
{
    // Avoid duplicates
    for (auto &call : m_calls) {
        if (call.plugin == plugin && call.funcIndex == funcIndex)
            return;
    }
    ForwardCall call;
    call.plugin = plugin;
    call.funcIndex = funcIndex;
    m_calls.push_back(call);
    AMXX_LOG_DBG("[Forward] Added plugin '%s' to forward '%s' (funcIndex=%d)", plugin->GetName(), m_name.c_str(), funcIndex);
}

void AMXXForward::RemovePlugin(AMXXPlugin *plugin)
{
    for (size_t i = 0; i < m_calls.size(); ) {
        if (m_calls[i].plugin == plugin) {
            AMXX_LOG_DBG("[Forward] Removing plugin '%s' from '%s'", plugin->GetName(), m_name.c_str());
            m_calls.erase(m_calls.begin() + i);
        } else {
            i++;
        }
    }
}

int AMXXForward::Execute(int paramCount, ForwardCallParam *params)
{
    if (m_paused || m_calls.empty())
        return PLUGIN_CONTINUE;

    int result = PLUGIN_CONTINUE;

    // For any param that needs to be written back after the call (byref, or
    // opt-in copyBack array/stringex), remember the AMX-side *address* (not
    // the raw pointer amx_Push*() handed back) so we can re-resolve it via
    // amx_GetAddr() after ExecutePublic() returns. The heap can move during
    // execution, so holding on to the original phys_addr would be unsafe.
    cell pushedAddr[FORWARD_MAX_PARAMS];

    for (size_t i = 0; i < m_calls.size(); i++) {
        AMXXPlugin *plugin = m_calls[i].plugin;
        if (!plugin || !plugin->IsLoaded() || plugin->IsPaused())
            continue;

        AMX *amx = plugin->GetAMX();
        if (!amx || m_calls[i].funcIndex < 0)
            continue;

        for (int j = 0; j < paramCount && j < FORWARD_MAX_PARAMS; j++)
            pushedAddr[j] = 0;

        // Push parameters in reverse order
        for (int j = paramCount - 1; j >= 0; j--) {
            switch (params[j].type) {
                case FP_CELL:
                    amx_Push(amx, params[j].val);
                    break;
                case FP_FLOAT:
                    amx_Push(amx, amx_ftoc(params[j].fval));
                    break;
                case FP_STRING:
                case FP_STRINGEX: {
                    cell amx_addr;
                    cell *phys_addr;
                    amx_PushString(amx, &amx_addr, &phys_addr, params[j].str, 0, 0);
                    if (params[j].type == FP_STRINGEX && params[j].copyBack && params[j].outbuf)
                        pushedAddr[j] = amx_addr;
                    break;
                }
                case FP_ARRAY: {
                    cell amx_addr;
                    cell *phys_addr;
                    if (params[j].arrayElemType == Type_Char) {
                        // Char array: expand each byte into a cell
                        int numCells = params[j].size;
                        cell *tempCells = (cell*)alloca(numCells * sizeof(cell));
                        char *src = (char*)params[j].array;
                        for (int ci = 0; ci < numCells; ci++)
                            tempCells[ci] = (cell)(unsigned char)src[ci];
                        amx_PushArray(amx, &amx_addr, &phys_addr, tempCells, numCells);
                    } else {
                        amx_PushArray(amx, &amx_addr, &phys_addr, params[j].array, params[j].size);
                    }
                    if (params[j].copyBack && params[j].array)
                        pushedAddr[j] = amx_addr;
                    break;
                }
                case FP_CELL_BYREF: {
                    cell amx_addr;
                    cell *phys_addr;
                    amx_PushArray(amx, &amx_addr, &phys_addr, params[j].ref, 1);
                    pushedAddr[j] = amx_addr;
                    break;
                }
                case FP_FLOAT_BYREF: {
                    cell amx_addr;
                    cell *phys_addr;
                    amx_PushArray(amx, &amx_addr, &phys_addr, (cell*)&params[j].fval, 1);
                    pushedAddr[j] = amx_addr;
                    break;
                }
                default:
                    amx_Push(amx, params[j].val);
                    break;
            }
        }

        cell retval;
        int err = plugin->ExecutePublic(m_calls[i].funcIndex, &retval);

        // Copy modified byref/stringex/array values back now, while the amx
        // instance and its data segment are still the ones we pushed into.
        for (int j = 0; j < paramCount && j < FORWARD_MAX_PARAMS; j++) {
            if (pushedAddr[j] == 0)
                continue;

            cell *phys_addr = nullptr;
            if (amx_GetAddr(amx, pushedAddr[j], &phys_addr) != AMX_ERR_NONE || !phys_addr)
                continue;

            switch (params[j].type) {
                case FP_CELL_BYREF:
                    *params[j].ref = *phys_addr;
                    break;
                case FP_FLOAT_BYREF:
                    params[j].fval = amx_ctof(*phys_addr);
                    break;
                case FP_STRINGEX:
                    amx_GetString(params[j].outbuf, phys_addr, 0, (size_t)params[j].size);
                    break;
                case FP_ARRAY:
                    if (params[j].arrayElemType == Type_Char) {
                        char *destChar = (char*)params[j].array;
                        for (int ci = 0; ci < params[j].size; ci++)
                            destChar[ci] = (char)(phys_addr[ci] & 0xFF);
                    } else {
                        memcpy(params[j].array, phys_addr, (size_t)params[j].size * sizeof(cell));
                    }
                    break;
                default:
                    break;
            }
        }

        if (err == AMX_ERR_NONE) {
            int ret = (int)retval;

            switch (m_execType) {
                case ET_IGNORE:
                    // Don't update result
                    break;
                case ET_STOP:
                    if (ret == PLUGIN_HANDLED || ret == PLUGIN_STOP) {
                        result = ret;
                        return result;
                    }
                    result = ret;
                    break;
                case ET_STOP2:
                    if (ret == PLUGIN_HANDLED || ret == PLUGIN_STOP) {
                        result = (ret > result) ? ret : result;
                        return result;
                    }
                    result = (ret > result) ? ret : result;
                    break;
                case ET_CONTINUE:
                    result = (ret > result) ? ret : result;
                    break;
            }
        }
    }

    return result;
}

// ========== AMXXSPForward (Single-Plugin Forward) ==========

AMXXSPForward::AMXXSPForward()
    : m_plugin(nullptr), m_funcIndex(-1), m_hasFunc(false), m_numParams(0)
{
    for (int i = 0; i < FORWARD_MAX_PARAMS; i++)
        m_paramTypes[i] = FP_DONE;
}

AMXXSPForward::~AMXXSPForward()
{
    Clear();
}

bool AMXXSPForward::SetByName(AMXXPlugin *plugin, const char *funcName, int numParams, const ForwardParam *paramTypes)
{
    if (!plugin || !funcName)
        return false;

    int funcIdx = plugin->FindPublic(funcName);
    if (funcIdx < 0) {
        AMXX_LOG("[SPForward] Function '%s' not found in plugin '%s'", funcName, plugin->GetName());
        return false;
    }

    m_plugin = plugin;
    m_funcIndex = funcIdx;
    m_hasFunc = true;
    m_numParams = numParams;
    m_name = funcName;

    if (paramTypes) {
        for (int i = 0; i < numParams && i < FORWARD_MAX_PARAMS; i++)
            m_paramTypes[i] = paramTypes[i];
    }

    AMXX_LOG_DBG("[SPForward] Set '%s' to plugin '%s' funcIndex=%d", funcName, plugin->GetName(), funcIdx);
    return true;
}

bool AMXXSPForward::SetByIndex(AMXXPlugin *plugin, int funcIndex, int numParams, const ForwardParam *paramTypes)
{
    if (!plugin || funcIndex < 0)
        return false;

    m_plugin = plugin;
    m_funcIndex = funcIndex;
    m_hasFunc = true;
    m_numParams = numParams;
    m_name = plugin->GetName();

    if (paramTypes) {
        for (int i = 0; i < numParams && i < FORWARD_MAX_PARAMS; i++)
            m_paramTypes[i] = paramTypes[i];
    }

    AMXX_LOG_DBG("[SPForward] Set by index: plugin='%s' funcIndex=%d", plugin->GetName(), funcIndex);
    return true;
}

int AMXXSPForward::Execute(int paramCount, ForwardCallParam *params)
{
    if (m_paused || !m_hasFunc || !m_plugin || !m_plugin->IsLoaded() || m_plugin->IsPaused())
        return PLUGIN_CONTINUE;

    AMX *amx = m_plugin->GetAMX();
    if (!amx || m_funcIndex < 0)
        return PLUGIN_CONTINUE;

    cell pushedAddr[FORWARD_MAX_PARAMS];
    for (int j = 0; j < paramCount && j < FORWARD_MAX_PARAMS; j++)
        pushedAddr[j] = 0;

    // Push parameters in reverse order
    for (int j = paramCount - 1; j >= 0; j--) {
        switch (params[j].type) {
            case FP_CELL:
                amx_Push(amx, params[j].val);
                break;
            case FP_FLOAT:
                amx_Push(amx, amx_ftoc(params[j].fval));
                break;
            case FP_STRING:
            case FP_STRINGEX: {
                cell amx_addr;
                cell *phys_addr;
                amx_PushString(amx, &amx_addr, &phys_addr, params[j].str, 0, 0);
                if (params[j].type == FP_STRINGEX && params[j].copyBack && params[j].outbuf)
                    pushedAddr[j] = amx_addr;
                break;
            }
            case FP_ARRAY: {
                cell amx_addr;
                cell *phys_addr;
                if (params[j].arrayElemType == Type_Char) {
                    int numCells = params[j].size;
                    cell *tempCells = (cell*)alloca(numCells * sizeof(cell));
                    char *src = (char*)params[j].array;
                    for (int ci = 0; ci < numCells; ci++)
                        tempCells[ci] = (cell)(unsigned char)src[ci];
                    amx_PushArray(amx, &amx_addr, &phys_addr, tempCells, numCells);
                } else {
                    amx_PushArray(amx, &amx_addr, &phys_addr, params[j].array, params[j].size);
                }
                if (params[j].copyBack && params[j].array)
                    pushedAddr[j] = amx_addr;
                break;
            }
            case FP_CELL_BYREF: {
                cell amx_addr;
                cell *phys_addr;
                amx_PushArray(amx, &amx_addr, &phys_addr, params[j].ref, 1);
                pushedAddr[j] = amx_addr;
                break;
            }
            case FP_FLOAT_BYREF: {
                cell amx_addr;
                cell *phys_addr;
                amx_PushArray(amx, &amx_addr, &phys_addr, (cell*)&params[j].fval, 1);
                pushedAddr[j] = amx_addr;
                break;
            }
            default:
                amx_Push(amx, params[j].val);
                break;
        }
    }

    cell retval;
    int err = m_plugin->ExecutePublic(m_funcIndex, &retval);

    // Copy modified byref/stringex/array values back now
    for (int j = 0; j < paramCount && j < FORWARD_MAX_PARAMS; j++) {
        if (pushedAddr[j] == 0)
            continue;

        cell *phys_addr = nullptr;
        if (amx_GetAddr(amx, pushedAddr[j], &phys_addr) != AMX_ERR_NONE || !phys_addr)
            continue;

        switch (params[j].type) {
            case FP_CELL_BYREF:
                *params[j].ref = *phys_addr;
                break;
            case FP_FLOAT_BYREF:
                params[j].fval = amx_ctof(*phys_addr);
                break;
            case FP_STRINGEX:
                amx_GetString(params[j].outbuf, phys_addr, 0, (size_t)params[j].size);
                break;
            case FP_ARRAY:
                if (params[j].arrayElemType == Type_Char) {
                    char *destChar = (char*)params[j].array;
                    for (int ci = 0; ci < params[j].size; ci++)
                        destChar[ci] = (char)(phys_addr[ci] & 0xFF);
                } else {
                    memcpy(params[j].array, phys_addr, (size_t)params[j].size * sizeof(cell));
                }
                break;
            default:
                break;
        }
    }

    if (err == AMX_ERR_NONE) {
        return (int)retval;
    }

    AMXX_LOG("[SPForward] Execution failed: plugin='%s' funcIndex=%d error=%d", m_plugin->GetName(), m_funcIndex, err);
    return PLUGIN_CONTINUE;
}

ForwardParam AMXXSPForward::GetParamType(int index) const
{
    if (index >= 0 && index < FORWARD_MAX_PARAMS)
        return m_paramTypes[index];
    return FP_DONE;
}

void AMXXSPForward::Clear()
{
    m_plugin = nullptr;
    m_funcIndex = -1;
    m_hasFunc = false;
    m_numParams = 0;
    m_name.clear();
}

// ========== AMXXForwardManager ==========

AMXXForwardManager &AMXXForwardManager::GetInstance()
{
    static AMXXForwardManager instance;
    return instance;
}

AMXXForwardManager::AMXXForwardManager()
{
}

int AMXXForwardManager::RegisterForward(const char *name, ForwardExecType execType, int numParams, const ForwardParam *paramTypes)
{
    auto *fwd = new AMXXForward(name, execType, numParams);
    if (paramTypes) {
        for (int i = 0; i < numParams && i < FORWARD_MAX_PARAMS; i++)
            fwd->SetParamType(i, paramTypes[i]);
    }
    m_forwards.push_back(fwd);
    int id = MakeNormalId((int)m_forwards.size() - 1);
    AMXX_LOG_DBG("[FwdMgr] Registered normal forward '%s' (id=%d)", name, id);
    return id;
}

void AMXXForwardManager::UnregisterForward(int id)
{
    if (IsSPForwardId(id))
        return;
    int idx = DecodeIndex(id);
    if (idx >= 0 && idx < (int)m_forwards.size() && m_forwards[idx]) {
        AMXX_LOG_DBG("[FwdMgr] Unregistered normal forward '%s' (id=%d)", m_forwards[idx]->GetName(), id);
        delete m_forwards[idx];
        m_forwards[idx] = nullptr;
    }
}

AMXXForward *AMXXForwardManager::GetForward(int id)
{
    if (IsSPForwardId(id))
        return nullptr;
    int idx = DecodeIndex(id);
    if (idx >= 0 && idx < (int)m_forwards.size())
        return m_forwards[idx];
    return nullptr;
}

int AMXXForwardManager::RegisterSPForwardByName(AMXXPlugin *plugin, const char *funcName, int numParams, const ForwardParam *paramTypes)
{
    // Find a free slot
    int slotIdx = -1;
    if (!m_freeSPIndices.empty()) {
        slotIdx = m_freeSPIndices.back();
        m_freeSPIndices.pop_back();
    } else {
        m_spForwards.push_back(nullptr);
        slotIdx = (int)m_spForwards.size() - 1;
    }

    auto *sp = new AMXXSPForward();
    if (!sp->SetByName(plugin, funcName, numParams, paramTypes)) {
        delete sp;
        // Don't consume the slot
        if (slotIdx == (int)m_spForwards.size() - 1)
            m_spForwards.pop_back();
        else
            m_freeSPIndices.push_back(slotIdx);
        return 0;
    }

    m_spForwards[slotIdx] = sp;
    int id = MakeSPId(slotIdx);
    AMXX_LOG_DBG("[FwdMgr] Registered SP forward '%s' for plugin '%s' (id=%d)", funcName, plugin->GetName(), id);
    return id;
}

int AMXXForwardManager::RegisterSPForwardByIndex(AMXXPlugin *plugin, int funcIndex, int numParams, const ForwardParam *paramTypes)
{
    int slotIdx = -1;
    if (!m_freeSPIndices.empty()) {
        slotIdx = m_freeSPIndices.back();
        m_freeSPIndices.pop_back();
    } else {
        m_spForwards.push_back(nullptr);
        slotIdx = (int)m_spForwards.size() - 1;
    }

    auto *sp = new AMXXSPForward();
    if (!sp->SetByIndex(plugin, funcIndex, numParams, paramTypes)) {
        delete sp;
        if (slotIdx == (int)m_spForwards.size() - 1)
            m_spForwards.pop_back();
        else
            m_freeSPIndices.push_back(slotIdx);
        return 0;
    }

    m_spForwards[slotIdx] = sp;
    int id = MakeSPId(slotIdx);
    AMXX_LOG_DBG("[FwdMgr] Registered SP forward by index: plugin='%s' funcIdx=%d (id=%d)", plugin->GetName(), funcIndex, id);
    return id;
}

void AMXXForwardManager::UnregisterSPForward(int id)
{
    if (!IsSPForwardId(id))
        return;
    int slotIdx = DecodeIndex(id);
    if (slotIdx >= 0 && slotIdx < (int)m_spForwards.size() && m_spForwards[slotIdx]) {
        AMXX_LOG_DBG("[FwdMgr] Unregistered SP forward (id=%d)", id);
        m_spForwards[slotIdx]->Clear();
        m_spForwards[slotIdx] = nullptr;
        m_freeSPIndices.push_back(slotIdx);
    }
}

AMXXSPForward *AMXXForwardManager::GetSPForward(int id)
{
    if (!IsSPForwardId(id))
        return nullptr;
    int slotIdx = DecodeIndex(id);
    if (slotIdx >= 0 && slotIdx < (int)m_spForwards.size())
        return m_spForwards[slotIdx];
    return nullptr;
}

int AMXXForwardManager::ExecuteForward(int id, int paramCount, ForwardCallParam *params)
{
    if (IsSPForwardId(id)) {
        auto *spf = GetSPForward(id);
        if (spf)
            return spf->Execute(paramCount, params);
    } else {
        auto *nf = GetForward(id);
        if (nf)
            return nf->Execute(paramCount, params);
    }
    return PLUGIN_CONTINUE;
}

// P2-8: Pause/resume forward by ID (supports both normal and SP forwards)
bool AMXXForwardManager::PauseForward(int id)
{
    if (IsSPForwardId(id)) {
        auto *spf = GetSPForward(id);
        if (spf) { spf->SetPaused(true); return true; }
    } else {
        auto *nf = GetForward(id);
        if (nf) { nf->SetPaused(true); return true; }
    }
    return false;
}

bool AMXXForwardManager::UnpauseForward(int id)
{
    if (IsSPForwardId(id)) {
        auto *spf = GetSPForward(id);
        if (spf) { spf->SetPaused(false); return true; }
    } else {
        auto *nf = GetForward(id);
        if (nf) { nf->SetPaused(false); return true; }
    }
    return false;
}

void AMXXForwardManager::ClearAll()
{
    for (auto *f : m_forwards) {
        delete f;
    }
    m_forwards.clear();  // 清空容器，避免野指针

    for (auto *sp : m_spForwards) {
        delete sp;
    }
    m_spForwards.clear();
    m_freeSPIndices.clear();

    AMXX_LOG_DBG("[FwdMgr] All forwards cleared");
}

void AMXXForwardManager::RemovePluginForwards(AMXXPlugin *plugin)
{
    // Remove plugin from normal forwards
    for (auto *f : m_forwards) {
        if (f)
            f->RemovePlugin(plugin);
    }

    // Clear SP forwards owned by this plugin
    for (auto *sp : m_spForwards) {
        if (sp && sp->GetPlugin() == plugin) {
            sp->Clear();
        }
    }
    AMXX_LOG_DBG("[FwdMgr] Removed all forwards for plugin '%s'", plugin->GetName());
}

// ========== Global convenience functions ==========

int registerForward(const char *funcName, ForwardExecType et, int numParams, ...)
{
    ForwardParam params[FORWARD_MAX_PARAMS];
    va_list args;
    va_start(args, numParams);
    for (int i = 0; i < numParams; i++) {
        int pt = va_arg(args, int);
        params[i] = (ForwardParam)pt;
    }
    va_end(args);
    return AMXXForwardManager::GetInstance().RegisterForward(funcName, et, numParams, params);
}

int registerSPForwardByName(AMXXPlugin *plugin, const char *funcName, int numParams, ...)
{
    ForwardParam params[FORWARD_MAX_PARAMS];
    va_list args;
    va_start(args, numParams);
    for (int i = 0; i < numParams; i++) {
        int pt = va_arg(args, int);
        params[i] = (ForwardParam)pt;
    }
    va_end(args);
    return AMXXForwardManager::GetInstance().RegisterSPForwardByName(plugin, funcName, numParams, params);
}

int registerSPForward(AMXXPlugin *plugin, int funcIndex, int numParams, ...)
{
    ForwardParam params[FORWARD_MAX_PARAMS];
    va_list args;
    va_start(args, numParams);
    for (int i = 0; i < numParams; i++) {
        int pt = va_arg(args, int);
        params[i] = (ForwardParam)pt;
    }
    va_end(args);
    return AMXXForwardManager::GetInstance().RegisterSPForwardByIndex(plugin, funcIndex, numParams, params);
}

void unregisterSPForward(int id)
{
    AMXXForwardManager::GetInstance().UnregisterSPForward(id);
}

cell executeForwards(int id, int paramCount, ForwardCallParam *params)
{
    return (cell)AMXXForwardManager::GetInstance().ExecuteForward(id, paramCount, params);
}