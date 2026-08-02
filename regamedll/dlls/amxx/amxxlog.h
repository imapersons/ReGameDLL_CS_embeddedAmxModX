#pragma once
#ifndef AMXX_LOGFILE_H
#define AMXX_LOGFILE_H

class AMXXLogSystem
{
public:
    static AMXXLogSystem &GetInstance();

    void Init();
    void LogToFile(const char *fmt, ...);
    void LogError(const char *fmt, ...);
    void SetLogFile(const char *filename);

private:
    AMXXLogSystem();

    char m_logPath[512];
};

#endif
