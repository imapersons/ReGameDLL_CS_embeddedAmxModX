#pragma once
#ifndef AMXX_TASKMGR_H
#define AMXX_TASKMGR_H

#include "amx.h"
#include <vector>
#include <cstring>

class AMXXPlugin;

struct AMXXTask
{
    AMXXPlugin *plugin;
    AMX *amx;               // 关联的 AMX（用于按插件作用域过滤 remove_task 等）
    int taskId;
    float nextExecTime;
    int funcIndex;

    // 原始 cell 参数数组（与原版 AMXX CTask 一致）
    // set_task 传参时以 array 形式推给 public func(params[], taskId)
    cell *params;
    int paramLen;

    // 原版 AMXX 任务标志位 (set_task flags 参数)
    int flags;
    bool loop;
    int repeat;          // -1 = 无限 'b', 0 = 单次, >0 = 'a' 重复次数
    bool afterStart;     // 'c'
    bool beforeEnd;      // 'd'
    bool mapEnd;         // 'f'
    float base;

    bool inExecute;
    bool isFree;         // 空闲槽标记（原版 m_bFree）

    AMXXTask() : plugin(nullptr), amx(nullptr), taskId(0), nextExecTime(0.0f),
                 funcIndex(-1), params(nullptr), paramLen(0),
                 flags(0), loop(false), repeat(0),
                 afterStart(false), beforeEnd(false), mapEnd(false), base(0.0f),
                 inExecute(false), isFree(true) {}

    void ClearParams()
    {
        if (params) { delete[] params; params = nullptr; }
        paramLen = 0;
    }

    void SetFree()
    {
        ClearParams();
        isFree = true;
        plugin = nullptr;
        amx = nullptr;
        taskId = 0;
        funcIndex = -1;
        base = 0.0f;
        nextExecTime = 0.0f;
        loop = false;
        repeat = 0;
        afterStart = false;
        beforeEnd = false;
        mapEnd = false;
    }
};

class AMXXTaskManager
{
public:
    AMXXTaskManager();
    ~AMXXTaskManager();

    void SetCurrentTime(float time);
    void ProcessTasks();

    void CreateTask(AMXXPlugin *plugin, AMX *amx, int funcIndex, int flags, cell taskId, float base, int paramLen, const cell *params, int repeat);

    // amx=NULL 表示不限插件（原版 params[2] ? 0 : amx 语义）
    bool ChangeTask(int taskId, float newBase, AMX *amx = nullptr);
    int RemoveTask(int taskId, AMX *amx = nullptr);   // P2: 返回移除的任务数量
    bool RemoveTasksByPlugin(AMXXPlugin *plugin);
    bool TaskExists(int taskId, AMX *amx = nullptr);

    void ExecuteMapEndTasks();
    void ClearAllTasks();

private:
    float m_currentTime;
    std::vector<AMXXTask> m_tasks;
    std::vector<AMXXTask> m_mapEndTasks;
};

#endif
