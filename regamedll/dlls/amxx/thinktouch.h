#pragma once
#ifndef AMXX_THINKTOUCH_H
#define AMXX_THINKTOUCH_H

#include "amx.h"
#include <map>
#include <string>

struct ThinkCallback
{
    AMX *amx;
    cell funcidx;
};

class AMXXThinkTouch
{
public:
    static AMXXThinkTouch &GetInstance();

    void Init();
    void Shutdown();

    void RegisterThink(const char *classname, AMX *amx, cell funcidx);
    void RegisterTouch(const char *toucherClass, const char *touchedClass, AMX *amx, cell funcidx);

    void UnregisterThink(const char *classname);
    void UnregisterTouch(const char *toucherClass, const char *touchedClass);

    void UnregisterByAMX(AMX *amx);

    const ThinkCallback *FindThink(const char *classname) const;
    const ThinkCallback *FindTouch(const char *toucherClass, const char *touchedClass) const;

private:
    AMXXThinkTouch() : m_hooked(false) {}

    void HookDispatch();
    void UnhookDispatch();

    bool m_hooked;

    std::map<std::string, ThinkCallback> m_thinks;

    struct TouchKey
    {
        std::string toucher;
        std::string touched;
        bool operator<(const TouchKey &o) const {
            int c = toucher.compare(o.toucher);
            return c < 0 || (c == 0 && touched < o.touched);
        }
    };
    std::map<TouchKey, ThinkCallback> m_touches;
};

#endif
