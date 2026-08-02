#pragma once
#ifndef AMXX_PLUGIN_H
#define AMXX_PLUGIN_H

#include "amx.h"
#include <vector>
#include <string>

class AMXXPlugin
{
public:
    AMXXPlugin();
    ~AMXXPlugin();

    bool Load(const char *filename);
    void Unload();

    bool IsLoaded() const { return m_loaded; }

    AMX *GetAMX() { return &m_amx; }
    const char *GetName() const { return m_name.c_str(); }
    const char *GetFilename() const { return m_filename.c_str(); }

    void SetTitle(const char *title) { m_title = title; }
    const char *GetTitle() const { return m_title.c_str(); }
    void SetVersion(const char *version) { m_version = version; }
    const char *GetVersion() const { return m_version.c_str(); }
    void SetAuthor(const char *author) { m_author = author; }
    const char *GetAuthor() const { return m_author.c_str(); }

    int GetFlags() const { return m_flags; }
    void SetFlags(int flags) { m_flags = flags; }

    bool IsPaused() const { return m_paused; }
    void SetPaused(bool paused) { m_paused = paused; }

    int FindPublic(const char *funcname);
    int ExecutePublic(int index, cell *retval);
    int ExecutePublic(const char *funcname, cell *retval);

private:
    bool m_loaded;
    AMX m_amx;
    unsigned char *m_program;
    size_t m_programSize;
    std::string m_name;
    std::string m_filename;
    std::string m_title;
    std::string m_version;
    std::string m_author;
    int m_flags;
    bool m_paused = false;
};

#endif
