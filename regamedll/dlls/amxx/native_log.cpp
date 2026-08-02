#include "precompiled.h"
#include "amxxlog.h"
#include "native_core.h"
#include "amx.h"
#include "../enginecallback.h"
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <cstring>

// 原版 AMXX log_to_file(const file[], const message[], any:...)
// params[1] = filename, params[2] = format string, params[3+] = format args
cell AMX_NATIVE_CALL amxx_log_to_file(AMX *amx, cell *params)
{
    char szFile[256];
    cell *faddr;
    if (amx_GetAddr(amx, params[1], &faddr) != AMX_ERR_NONE || !faddr)
        return 0;
    amx_GetString(szFile, faddr, 0, sizeof(szFile));
    if (!szFile[0])
        return 0;

    char file[512];
    // 若包含路径分隔符则按原路径使用，否则放在 amxmodx/logs 目录下
    if (strchr(szFile, '/') || strchr(szFile, '\\'))
    {
        snprintf(file, sizeof(file), "%s", szFile);
    }
    else
    {
        // 通过引擎获取游戏目录 (gamedir)，Android 下即 /sdcard/xash/cstrike 等实际路径
        char gameDir[256] = {0};
        GET_GAME_DIR(gameDir);
        snprintf(file, sizeof(file), "%s/addons/amxmodx/logs/%s", gameDir, szFile);
    }

    // 检测是否首次写入（文件不存在）
    bool first_time = true;
    {
        FILE *fp = fopen(file, "r");
        if (fp) { first_time = false; fclose(fp); }
    }

    FILE *fp = fopen(file, "a");
    if (!fp)
        return 0;

    // 日期前缀
    char date[32];
    time_t td; time(&td);
    strftime(date, sizeof(date), "%m/%d/%Y - %H:%M:%S", localtime(&td));

    // 格式化消息：从 params[2] 开始 (filename 是 params[1])
    char msg[2048];
    amxx_format_string(amx, params, 2, msg, sizeof(msg));
    size_t mlen = strlen(msg);
    if (mlen < sizeof(msg) - 1)
    {
        msg[mlen++] = '\n';
        msg[mlen] = '\0';
    }

    if (first_time)
    {
        char hdr[512];
        snprintf(hdr, sizeof(hdr), "L %s: Log file started (file \"%s\")\n", date, file);
        fprintf(fp, "%s", hdr);
        SERVER_PRINT(hdr);
    }

    char outMsg[2200];
    snprintf(outMsg, sizeof(outMsg), "L %s: %s", date, msg);
    fprintf(fp, "%s", outMsg);
    SERVER_PRINT(outMsg);
    fclose(fp);

    return 1;
}

cell AMX_NATIVE_CALL amxx_log_amxx_file(AMX *amx, cell *params)
{
    char msg[2048];
    amxx_format_string(amx, params, 1, msg, sizeof(msg));
    AMXXLogSystem::GetInstance().LogToFile("%s", msg);
    return 1;
}

cell AMX_NATIVE_CALL amxx_set_logfile(AMX *amx, cell *params)
{
    cell *filename_addr;
    amx_GetAddr(amx, params[1], &filename_addr);
    char filename[512];
    amx_GetString(filename, filename_addr, 0, sizeof(filename));
    AMXXLogSystem::GetInstance().SetLogFile(filename);
    return 1;
}

AMX_NATIVE_INFO log_natives[] = {
    {"log_to_file", amxx_log_to_file},
    {"log_amx", amxx_log_amxx_file},
    {"set_logfile", amxx_set_logfile},
    {nullptr, nullptr}
};

void RegisterLogNatives(AMX *amx)
{
    amx_Register(amx, log_natives, -1);
}
