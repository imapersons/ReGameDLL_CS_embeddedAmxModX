#pragma once
#ifndef AMXX_FORWARDS_H
#define AMXX_FORWARDS_H

#include "amx.h"
#include <vector>
#include <string>

class AMXXPlugin;

#define PLUGIN_CONTINUE     0
#define PLUGIN_HANDLED      1
#define PLUGIN_STOP         2

const int FORWARD_MAX_PARAMS = 32;

// Forward execution types (matching original AMXX)
enum ForwardExecType
{
    ET_IGNORE = 0,     // Ignore return value
    ET_STOP,           // Stop on PLUGIN_HANDLED
    ET_STOP2,          // Stop on PLUGIN_HANDLED, return biggest value
    ET_CONTINUE        // Continue; return biggest return value
};

// Forward parameter types (matching original AMXX)
enum ForwardParam
{
    FP_DONE = -1,       // Specify as last argument
    FP_CELL,            // Normal cell
    FP_FLOAT,           // Float (stored as cell via amx_ftoc)
    FP_STRING,          // String
    FP_STRINGEX,        // String; updated to last function's value
    FP_ARRAY,           // Array (use prepareArray return value)
    FP_CELL_BYREF,      // Cell pass-by-reference
    FP_FLOAT_BYREF      // Float pass-by-reference
};

// Forward array element types (matching original AMXX ForwardPreparedArray.type)
enum ForwardArrayElemType
{
    Type_Cell = 0,
    Type_Char = 1
};

struct ForwardPreparedArray
{
    void *ptr;
    int type;   // 0 = cell, 1 = char
    unsigned int size;
    bool copyBack;
};

struct ForwardCallParam
{
    ForwardParam type;
    int size;           // FP_ARRAY: element count. FP_STRINGEX: capacity (in bytes) of outbuf.

    // Opt-in copy-back support (additive; defaults keep old push-only behavior).
    // FP_CELL_BYREF / FP_FLOAT_BYREF are always copied back (that is their whole point).
    // FP_ARRAY / FP_STRINGEX only copy back when copyBack is set and a destination is given.
    bool copyBack = false;
    char *outbuf = nullptr;    // FP_STRINGEX only: destination buffer for the post-call string value
    ForwardArrayElemType arrayElemType = Type_Cell;  // FP_ARRAY only: element type for Push/CopyBack

    union
    {
        int val;
        const char *str;
        float fval;
        cell *array;
        cell *ref;
    };
};

// ========== Normal Forward (multi-plugin broadcast) ==========
class AMXXForward
{
public:
    struct ForwardCall
    {
        AMXXPlugin *plugin;
        int funcIndex;
    };

    AMXXForward(const char *name, ForwardExecType execType = ET_CONTINUE, int numParams = 0);
    ~AMXXForward();

    const char *GetName() const { return m_name.c_str(); }
    ForwardExecType GetExecType() const { return m_execType; }
    int GetNumParams() const { return m_numParams; }
    void SetParamType(int index, ForwardParam type);
    ForwardParam GetParamType(int index) const;

    void AddPlugin(AMXXPlugin *plugin, int funcIndex);
    void RemovePlugin(AMXXPlugin *plugin);

    int Execute(int paramCount, ForwardCallParam *params);

    // P2-8: forward pause/resume
    void SetPaused(bool paused) { m_paused = paused; }
    bool IsPaused() const { return m_paused; }

private:
    std::string m_name;
    ForwardExecType m_execType;
    int m_numParams;
    ForwardParam m_paramTypes[FORWARD_MAX_PARAMS];
    std::vector<ForwardCall> m_calls;
    bool m_paused = false;
};

// ========== SP Forward (single-plugin, handle-based) ==========
// Used by register_event, register_message, set_task, etc.
class AMXXSPForward
{
public:
    AMXXSPForward();
    ~AMXXSPForward();

    // Set by function name (searches plugin publics)
    bool SetByName(AMXXPlugin *plugin, const char *funcName, int numParams, const ForwardParam *paramTypes = nullptr);
    // Set by function index
    bool SetByIndex(AMXXPlugin *plugin, int funcIndex, int numParams, const ForwardParam *paramTypes = nullptr);

    int Execute(int paramCount, ForwardCallParam *params);

    bool IsFree() const { return !m_hasFunc; }
    int GetNumParams() const { return m_numParams; }
    ForwardParam GetParamType(int index) const;
    const char *GetName() const { return m_name.c_str(); }
    AMXXPlugin *GetPlugin() const { return m_plugin; }

    // P2-8: forward pause/resume
    void SetPaused(bool paused) { m_paused = paused; }
    bool IsPaused() const { return m_paused; }

    void Clear();

private:
    AMXXPlugin *m_plugin;
    int m_funcIndex;
    bool m_hasFunc;
    int m_numParams;
    ForwardParam m_paramTypes[FORWARD_MAX_PARAMS];
    std::string m_name;
    bool m_paused = false;
};

// ========== Forward Manager ==========
class AMXXForwardManager
{
public:
    static AMXXForwardManager &GetInstance();

    // Normal forward management
    int RegisterForward(const char *name, ForwardExecType execType, int numParams, const ForwardParam *paramTypes);
    void UnregisterForward(int id);
    AMXXForward *GetForward(int id);

    // SP Forward management
    int RegisterSPForwardByName(AMXXPlugin *plugin, const char *funcName, int numParams, const ForwardParam *paramTypes = nullptr);
    int RegisterSPForwardByIndex(AMXXPlugin *plugin, int funcIndex, int numParams, const ForwardParam *paramTypes = nullptr);
    void UnregisterSPForward(int id);
    AMXXSPForward *GetSPForward(int id);

    // Batch execute
    int ExecuteForward(int id, int paramCount, ForwardCallParam *params);

    // P2-8: Pause/resume forward by ID
    bool PauseForward(int id);
    bool UnpauseForward(int id);

    // Cleanup
    void ClearAll();
    void RemovePluginForwards(AMXXPlugin *plugin);

    // ID helpers
    static bool IsSPForwardId(int id) { return (id & 1) != 0; }
    static int MakeSPId(int index) { return (index << 1) | 1; }
    static int MakeNormalId(int index) { return index << 1; }
    static int DecodeIndex(int id) { return id >> 1; }

private:
    AMXXForwardManager();

    std::vector<AMXXForward*> m_forwards;      // Normal forwards
    std::vector<AMXXSPForward*> m_spForwards;   // SP forwards
    std::vector<int> m_freeSPIndices;          // Free list for SP forward slots
};

// ========== Helper functions ==========
// Global convenience functions (matching original AMXX pattern)
int registerForward(const char *funcName, ForwardExecType et, int numParams, ...);
int registerSPForwardByName(AMXXPlugin *plugin, const char *funcName, int numParams, ...);
int registerSPForward(AMXXPlugin *plugin, int funcIndex, int numParams, ...);
void unregisterSPForward(int id);
cell executeForwards(int id, int paramCount, ForwardCallParam *params);

#endif