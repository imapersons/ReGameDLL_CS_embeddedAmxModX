#include "precompiled.h"
#include "amxxlog.h"
#include "amx.h"
#include "runtime.h"
#include "forwards.h"
#include "../enginecallback.h"
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <ctime>
#include <string>

#ifdef _WIN32
#include <direct.h>
#define mkdir _mkdir
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

AMXXLogSystem &AMXXLogSystem::GetInstance()
{
    static AMXXLogSystem instance;
    return instance;
}

AMXXLogSystem::AMXXLogSystem()
{
    m_logPath[0] = '\0';
}

void AMXXLogSystem::Init()
{
    // 通过引擎获取游戏目录 (gamedir)，Android 下即 /sdcard/xash/cstrike 等实际路径
    char gameDir[256] = {0};
    GET_GAME_DIR(gameDir);
    std::string baseDir = std::string(gameDir) + "/addons/amxmodx";

#ifdef _WIN32
    _mkdir(baseDir.c_str());
    _mkdir((std::string(baseDir) + "/logs").c_str());
#else
    mkdir(baseDir.c_str(), 0755);
    mkdir((std::string(baseDir) + "/logs").c_str(), 0755);
#endif

    time_t td = time(nullptr);
    struct tm *lt = localtime(&td);
    snprintf(m_logPath, sizeof(m_logPath), "%s/logs/L%02d%02d%03d.log",
             baseDir, lt->tm_year % 100, lt->tm_mon + 1, lt->tm_mday);
}

void AMXXLogSystem::SetLogFile(const char *filename)
{
    if (filename)
        strncpy(m_logPath, filename, sizeof(m_logPath) - 1);
    m_logPath[sizeof(m_logPath) - 1] = '\0';
}

void AMXXLogSystem::LogToFile(const char *fmt, ...)
{
    if (!m_logPath[0])
        Init();

    char logBuf[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(logBuf, sizeof(logBuf), fmt, args);
    va_end(args);

    // 触发 plugin_log forward：参数为 (logtag[], message[], loglevel = 0)
    // 注意: 不使用递归防护，因为 plugin_log forward 内部不应再次调用 LogToFile
    {
        ForwardCallParam fcp[3];
        fcp[0].type = FP_STRING; fcp[0].str = "amxx";
        fcp[1].type = FP_STRING; fcp[1].str = logBuf;
        fcp[2].type = FP_CELL;   fcp[2].val = 0;
        AMXXRuntime::GetInstance().ExecuteForwardEx("plugin_log", 3, fcp);
    }

    FILE *fp = fopen(m_logPath, "a");
    if (!fp)
        return;

    time_t td = time(nullptr);
    struct tm *lt = localtime(&td);
    fprintf(fp, "L %02d/%02d/%04d - %02d:%02d:%02d: ",
            lt->tm_mday, lt->tm_mon + 1, lt->tm_year + 1900,
            lt->tm_hour, lt->tm_min, lt->tm_sec);

    fprintf(fp, "%s", logBuf);
    fprintf(fp, "\n");
    fclose(fp);
}

void AMXXLogSystem::LogError(const char *fmt, ...)
{
    if (!m_logPath[0])
        Init();

    FILE *fp = fopen(m_logPath, "a");
    if (!fp)
        return;

    time_t td = time(nullptr);
    struct tm *lt = localtime(&td);
    fprintf(fp, "L %02d/%02d/%04d - %02d:%02d:%02d: [ERROR] ",
            lt->tm_mday, lt->tm_mon + 1, lt->tm_year + 1900,
            lt->tm_hour, lt->tm_min, lt->tm_sec);

    va_list args;
    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);

    fprintf(fp, "\n");
    fclose(fp);
}

// 控制台输出：同时写入 stdout 和引擎控制台（-log 文件捕获）
// 声明在 amx.h 中，由 AMXX_LOG 宏调用
void amxx_console_print(const char *msg)
{
    // 只使用引擎控制台输出，避免重复
    ALERT(at_console, "%s\n", msg);
}
