#include "precompiled.h"

#include "../extdll.h"
#include "../enginecallback.h"
#include "taskmgr.h"
#include "amx.h"
#include "runtime.h"
#include "amxxlog.h"

AMXXTaskManager::AMXXTaskManager()
{
    m_currentTime = 0.0f;
}

AMXXTaskManager::~AMXXTaskManager()
{
    ClearAllTasks();
}

void AMXXTaskManager::SetCurrentTime(float time)
{
    m_currentTime = time;
}

void AMXXTaskManager::ProcessTasks()
{
    float timeLimit = g_engfuncs.pfnCVarGetFloat("mp_timelimit");
    float timeLeft = (timeLimit > 0.0f) ? (timeLimit * 60.0f - m_currentTime) : 0.0f;

    for (size_t i = 0; i < m_tasks.size(); ) {
        AMXXTask &task = m_tasks[i];

        if (task.isFree || !task.plugin || !task.plugin->IsLoaded() || task.plugin->IsPaused()) {
            i++;
            continue;
        }

        bool execute = false;

        // 对齐原版 AMXX CTask::executeIfRequired 的时间判断 (P0-4 修复)
        if (task.afterStart) {
            // 'c' 标志: fCurrentTime - fTimeLeft + 1.0 >= m_fBase
            if (m_currentTime - timeLeft + 1.0f >= task.base)
                execute = true;
        } else if (task.beforeEnd) {
            // 'd' 标志: fTimeLimit != 0 && (fTimeLeft + fTimeLimit*60) - fCurrentTime - 1.0 <= m_fBase
            if (timeLimit != 0.0f && (timeLeft + timeLimit * 60.0f) - m_currentTime - 1.0f <= task.base)
                execute = true;
        } else if (task.nextExecTime <= m_currentTime) {
            execute = true;
        }

        if (execute) {
            // 备份关键状态
            bool wasLoop = task.loop;
            int wasRepeat = task.repeat;
            cell wasTaskId = task.taskId;
            float wasNextExecTime = task.nextExecTime;

            task.inExecute = true;
            AMX *amx = task.plugin->GetAMX();

            // P0-5 修复: 对齐原版参数推送
            // 原版: 有参数时 executeForwards(m_iFunc, arr, m_iId) → public func(params[], taskId)
            //        无参数时 executeForwards(m_iFunc, m_iId) → public func(taskId)
            // amx_Push 反序: 后推的在前
            cell amx_addr = 0;
            if (task.paramLen > 0) {
                cell *phys_addr;
                amx_Allot(amx, task.paramLen, &amx_addr, &phys_addr);
                memcpy(phys_addr, task.params, task.paramLen * sizeof(cell));
                amx_Push(amx, task.taskId);   // 最后参数: taskId
                amx_Push(amx, amx_addr);       // 第一参数: params[]
            } else {
                amx_Push(amx, task.taskId);   // 唯一参数: taskId
            }

            AMXX_LOG("[TaskMgr] EXEC id=%d plugin='%s' funcIdx=%d time=%.2f",
                task.taskId, task.plugin->GetName(), task.funcIndex, m_currentTime);

            cell retval;
            task.plugin->ExecutePublic(task.funcIndex, &retval);

            if (amx_addr)
                amx_Release(amx, amx_addr);

            task.inExecute = false;

            // 检查任务是否在执行期间被替换/删除
            if (i >= m_tasks.size() || m_tasks[i].taskId != wasTaskId || m_tasks[i].isFree) {
                i++;
                continue;
            }

            // 检测 nextExecTime 是否被 set_task 改变
            bool selfReplaced = (m_tasks[i].nextExecTime != wasNextExecTime);
            if (selfReplaced) {
                i++;
                continue;
            }

            // 设置新的执行时间或移除任务
            bool done = false;
            if (wasLoop) {
                if (wasRepeat != -1 && --m_tasks[i].repeat <= 0)
                    done = true;
            } else {
                done = true;
            }

            if (done) {
                m_tasks[i].SetFree();
                // 不 erase，保留为空闲槽供复用 (P1-3)
            } else {
                // 重新调度
                if (m_tasks[i].afterStart || m_tasks[i].beforeEnd) {
                    if (!wasLoop) {
                        m_tasks[i].SetFree();
                    } else {
                        m_tasks[i].nextExecTime = m_currentTime + m_tasks[i].base;
                    }
                } else {
                    m_tasks[i].nextExecTime += m_tasks[i].base;
                    if (m_tasks[i].nextExecTime <= m_currentTime)
                        m_tasks[i].nextExecTime = m_currentTime + m_tasks[i].base;
                }
            }
            i++;
        } else {
            i++;
        }
    }
}

void AMXXTaskManager::CreateTask(AMXXPlugin *plugin, AMX *amx, int funcIndex, int flags, cell taskId, float base, int paramLen, const cell *params, int repeat)
{
    if (!plugin || !plugin->IsLoaded())
        return;

    // 'e' 标志: 若同 ID 任务已存在则不创建
    if (flags & 16) {
        if (TaskExists(taskId, amx)) {
            AMXX_LOG("[TaskMgr] CreateTask: 'e' flag, task %d exists, skipping", taskId);
            return;
        }
    }

    // 'f' 标志: 地图结束任务
    if (flags & 32) {
        // P1-3: 先找空闲槽
        size_t slot = (size_t)-1;
        for (size_t i = 0; i < m_mapEndTasks.size(); i++) {
            if (m_mapEndTasks[i].isFree) { slot = i; break; }
        }

        AMXXTask *pTask;
        if (slot != (size_t)-1) {
            pTask = &m_mapEndTasks[slot];
        } else {
            m_mapEndTasks.push_back(AMXXTask());
            pTask = &m_mapEndTasks.back();
        }

        pTask->plugin = plugin;
        pTask->amx = amx;
        pTask->taskId = taskId;
        pTask->funcIndex = funcIndex;
        pTask->base = base;
        pTask->flags = flags;
        pTask->loop = false;
        pTask->repeat = 0;
        pTask->afterStart = false;
        pTask->beforeEnd = false;
        pTask->mapEnd = true;
        pTask->nextExecTime = 0.0f;
        pTask->inExecute = false;
        pTask->isFree = false;
        pTask->ClearParams();
        if (paramLen > 0 && params) {
            pTask->params = new cell[paramLen];
            memcpy(pTask->params, params, paramLen * sizeof(cell));
            pTask->paramLen = paramLen;
        }
        return;
    }

    // P1-3: 先找空闲槽复用（原版 registerTask 行为）
    size_t slot = (size_t)-1;
    for (size_t i = 0; i < m_tasks.size(); i++) {
        if (m_tasks[i].isFree && !m_tasks[i].inExecute) { slot = i; break; }
    }

    AMXXTask *pTask;
    if (slot != (size_t)-1) {
        pTask = &m_tasks[slot];
    } else {
        m_tasks.push_back(AMXXTask());
        pTask = &m_tasks.back();
    }

    pTask->plugin = plugin;
    pTask->amx = amx;
    pTask->taskId = taskId;
    pTask->funcIndex = funcIndex;
    pTask->base = base;
    pTask->flags = flags;
    pTask->loop = false;
    pTask->repeat = 0;
    pTask->isFree = false;
    pTask->inExecute = false;
    pTask->mapEnd = false;

    if (flags & 2) {
        pTask->loop = true;
        pTask->repeat = -1;
    } else if (flags & 1) {
        pTask->loop = true;
        pTask->repeat = repeat;
    }
    pTask->afterStart = (flags & 4) ? true : false;
    pTask->beforeEnd = (flags & 8) ? true : false;

    float currentTime = m_currentTime;
    if (currentTime == 0.0f)
        currentTime = gpGlobals->time;

    if (pTask->afterStart) {
        pTask->nextExecTime = base;
    } else if (pTask->beforeEnd) {
        float tl = g_engfuncs.pfnCVarGetFloat("mp_timelimit");
        pTask->nextExecTime = (tl > 0.0f) ? (tl * 60.0f - base) : 0.0f;
    } else {
        pTask->nextExecTime = currentTime + base;
    }

    pTask->ClearParams();
    if (paramLen > 0 && params) {
        pTask->params = new cell[paramLen];
        memcpy(pTask->params, params, paramLen * sizeof(cell));
        pTask->paramLen = paramLen;
    }

    AMXX_LOG("[TaskMgr] Created task %d (plugin='%s', funcIdx=%d, flags=%d, base=%.2f, repeat=%d, nextExec=%.2f, tasks=%zu)",
        taskId, plugin->GetName(), funcIndex, flags, base, repeat, pTask->nextExecTime, m_tasks.size());
}

bool AMXXTaskManager::ChangeTask(int taskId, float newBase, AMX *amx)
{
    bool found = false;
    for (size_t i = 0; i < m_tasks.size(); i++) {
        if (!m_tasks[i].isFree && m_tasks[i].taskId == taskId) {
            if (amx && m_tasks[i].amx != amx)
                continue;
            m_tasks[i].base = newBase;
            if (!m_tasks[i].inExecute)
                m_tasks[i].nextExecTime = m_currentTime + newBase;
            found = true;
        }
    }
    return found;
}

// P2: 返回移除的任务数量 (原版 remove_task 返回值语义)
int AMXXTaskManager::RemoveTask(int taskId, AMX *amx)
{
    int removed = 0;
    for (size_t i = 0; i < m_tasks.size(); i++) {
        if (!m_tasks[i].isFree && m_tasks[i].taskId == taskId) {
            if (amx && m_tasks[i].amx != amx)
                continue;
            m_tasks[i].SetFree();
            removed++;
        }
    }
    for (size_t i = 0; i < m_mapEndTasks.size(); i++) {
        if (!m_mapEndTasks[i].isFree && m_mapEndTasks[i].taskId == taskId) {
            if (amx && m_mapEndTasks[i].amx != amx)
                continue;
            m_mapEndTasks[i].SetFree();
            removed++;
        }
    }
    return removed;
}

bool AMXXTaskManager::RemoveTasksByPlugin(AMXXPlugin *plugin)
{
    bool removed = false;
    for (size_t i = 0; i < m_tasks.size(); i++) {
        if (!m_tasks[i].isFree && m_tasks[i].plugin == plugin) {
            m_tasks[i].SetFree();
            removed = true;
        }
    }
    for (size_t i = 0; i < m_mapEndTasks.size(); i++) {
        if (!m_mapEndTasks[i].isFree && m_mapEndTasks[i].plugin == plugin) {
            m_mapEndTasks[i].SetFree();
            removed = true;
        }
    }
    return removed;
}

bool AMXXTaskManager::TaskExists(int taskId, AMX *amx)
{
    for (size_t i = 0; i < m_tasks.size(); i++) {
        if (!m_tasks[i].isFree && m_tasks[i].taskId == taskId) {
            if (amx && m_tasks[i].amx != amx)
                continue;
            return true;
        }
    }
    for (size_t i = 0; i < m_mapEndTasks.size(); i++) {
        if (!m_mapEndTasks[i].isFree && m_mapEndTasks[i].taskId == taskId) {
            if (amx && m_mapEndTasks[i].amx != amx)
                continue;
            return true;
        }
    }
    return false;
}

void AMXXTaskManager::ExecuteMapEndTasks()
{
    for (auto &task : m_mapEndTasks) {
        if (task.isFree || !task.plugin || !task.plugin->IsLoaded())
            continue;

        AMX *amx = task.plugin->GetAMX();
        cell amx_addr = 0;
        if (task.paramLen > 0) {
            cell *phys_addr;
            amx_Allot(amx, task.paramLen, &amx_addr, &phys_addr);
            memcpy(phys_addr, task.params, task.paramLen * sizeof(cell));
            amx_Push(amx, task.taskId);
            amx_Push(amx, amx_addr);
        } else {
            amx_Push(amx, task.taskId);
        }

        cell retval;
        task.plugin->ExecutePublic(task.funcIndex, &retval);

        if (amx_addr)
            amx_Release(amx, amx_addr);
    }
    // 清空 map-end 任务
    for (auto &t : m_mapEndTasks)
        t.SetFree();
}

void AMXXTaskManager::ClearAllTasks()
{
    for (auto &t : m_tasks)
        t.SetFree();
    for (auto &t : m_mapEndTasks)
        t.SetFree();
}
