#pragma once
#ifndef AMXX_VAULT_H
#define AMXX_VAULT_H

#include <string>
#include <map>
#include <vector>

class AMXXVault
{
public:
    static AMXXVault &GetInstance();

    void Init();

    bool Load(const char *filename);
    bool Save(const char *filename);

    bool Exists(const char *key);
    bool Get(const char *key, char *dest, int maxlen);
    bool Set(const char *key, const char *value, double timestamp = 0.0);
    bool Remove(const char *key);

    void SetDefaultFile(const char *filename) { m_defaultFile = filename ? filename : ""; }
    const char *GetDefaultFile() const { return m_defaultFile.c_str(); }

private:
    AMXXVault();

    struct VaultEntry {
        std::string value;
        double timestamp;
    };

    std::string m_defaultFile;
    std::map<std::string, VaultEntry> m_data;
};

#endif
