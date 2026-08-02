#include "precompiled.h"

#include "plugin.h"
#include "amx.h"
#include <cstdio>
#include <cstring>

#define AMXX_MAGIC 0x414D5858
#define AMXB_MAGIC 0x414D5842

#ifndef AMXX_HAVE_ZLIB
#define AMXX_HAVE_ZLIB
#endif

#ifdef AMXX_HAVE_ZLIB
#if defined(_WIN32)
#include "zlib_compat.h"
#else
#include <zlib.h>
#endif
#endif

static bool IsAMXX(const unsigned char *data, size_t size)
{
    if (size < sizeof(int))
        return false;

    int magic;
    memcpy(&magic, data, sizeof(magic));
    return magic == AMXX_MAGIC;
}

#ifdef AMXX_HAVE_ZLIB
static bool LoadAMXXFromBuffer(AMX *amx, unsigned char *filebuf, size_t filesize, unsigned char **out_program)
{
    AMXX_LOG_DBG("[Plugin] LoadAMXXFromBuffer: filesize=%zu", filesize);

    if (filesize < 7)
    {
        AMXX_LOG_DBG("[Plugin] LoadAMXXFromBuffer: file too small (<7 bytes)");
        return false;
    }

    int magic;
    memcpy(&magic, filebuf, sizeof(int));
    AMXX_LOG_DBG("[Plugin] LoadAMXXFromBuffer: magic=0x%08X", magic);

    if (magic == AMXX_MAGIC)
    {
        unsigned char numPlugins;
        memcpy(&numPlugins, filebuf + 6, sizeof(unsigned char));
        AMXX_LOG_DBG("[Plugin] LoadAMXXFromBuffer: numPlugins=%d", numPlugins);

        if (numPlugins == 0)
        {
            AMXX_LOG_DBG("[Plugin] LoadAMXXFromBuffer: no plugins in file");
            return false;
        }

        int entryOffset = 7;

        for (int i = 0; i < (int)numPlugins; i++)
        {
            AMXX_LOG_DBG("[Plugin] LoadAMXXFromBuffer: processing entry %d at offset %d", i, entryOffset);

            if ((size_t)(entryOffset + 17) > filesize)
            {
                AMXX_LOG_DBG("[Plugin] LoadAMXXFromBuffer: entry %d out of bounds", i);
                return false;
            }

            unsigned char cellsize;
            int disksize, imagesize, memsize, offs;

            memcpy(&cellsize, filebuf + entryOffset, 1);
            memcpy(&disksize, filebuf + entryOffset + 1, 4);
            memcpy(&imagesize, filebuf + entryOffset + 5, 4);
            memcpy(&memsize, filebuf + entryOffset + 9, 4);
            memcpy(&offs, filebuf + entryOffset + 13, 4);

            AMXX_LOG_DBG("[Plugin] LoadAMXXFromBuffer: cellsize=%d, disksize=%d, imagesize=%d, memsize=%d, offs=%d",
                cellsize, disksize, imagesize, memsize, offs);

            if (cellsize == 4)
            {
                if (disksize <= 0 || imagesize <= 0)
                {
                    AMXX_LOG_DBG("[Plugin] LoadAMXXFromBuffer: invalid size (disksize=%d, imagesize=%d)", disksize, imagesize);
                    entryOffset += 17;
                    continue;
                }

                if ((size_t)(offs + disksize) > filesize)
                {
                    AMXX_LOG_DBG("[Plugin] LoadAMXXFromBuffer: compressed data out of bounds");
                    entryOffset += 17;
                    continue;
                }

                unsigned char *compressed = filebuf + offs;
                size_t allocSize = (imagesize > memsize) ? imagesize : memsize;
                uLongf destLen = (uLongf)imagesize;
                int err = Z_OK;

                if (compressed[0] == 0x78)
                {
                    AMXX_LOG_DBG("[Plugin] LoadAMXXFromBuffer: zlib header detected (0x78)");
                }

                // Dynamic buffer allocation: start with max(imagesize, memsize), retry with larger buffer if needed
                for (int attempt = 0; attempt < 3; attempt++)
                {
                    unsigned char *decompressed = (unsigned char *)malloc(allocSize);
                    if (!decompressed)
                    {
                        AMXX_LOG_DBG("[Plugin] LoadAMXXFromBuffer: failed to allocate decompression buffer (%zu bytes)", allocSize);
                        allocSize *= 2;
                        continue;
                    }

                    memset(decompressed, 0, allocSize);
                    destLen = (uLongf)allocSize;
                    err = uncompress(decompressed, &destLen, compressed, disksize);
                    AMXX_LOG_DBG("[Plugin] LoadAMXXFromBuffer: uncompress result=%d, destLen=%lu (alloc=%zu)", err, destLen, allocSize);

                    if (err == Z_OK)
                    {
                        int amx_err = amx_Init(amx, decompressed);
                        AMXX_LOG_DBG("[Plugin] LoadAMXXFromBuffer: amx_Init result=%d", amx_err);

                        if (amx_err == AMX_ERR_NONE)
                        {
                            *out_program = decompressed;
                            AMXX_LOG_DBG("[Plugin] LoadAMXXFromBuffer: success!");
                            return true;
                        }
                        else
                        {
                            AMXX_LOG_DBG("[Plugin] LoadAMXXFromBuffer: amx_Init failed: %d", amx_err);
                        }

                        free(decompressed);
                        break;
                    }
                    else if (err == Z_BUF_ERROR)
                    {
                        AMXX_LOG_DBG("[Plugin] LoadAMXXFromBuffer: buffer too small (%zu), retrying with %zu", allocSize, allocSize * 2);
                        free(decompressed);
                        allocSize *= 2;
                    }
                    else
                    {
                        AMXX_LOG_DBG("[Plugin] LoadAMXXFromBuffer: zlib decompression failed: %d", err);
                        free(decompressed);
                        break;
                    }
                }
            }
            else
            {
                AMXX_LOG_DBG("[Plugin] LoadAMXXFromBuffer: unsupported cellsize=%d", cellsize);
            }

            entryOffset += 17;
        }

        AMXX_LOG_DBG("[Plugin] LoadAMXXFromBuffer: no valid plugin found");
        return false;
    }
    else if (magic == AMXB_MAGIC)
    {
        AMXX_LOG_DBG("[Plugin] LoadAMXXFromBuffer: AMXB format not supported");
        return false;
    }

    AMXX_LOG_DBG("[Plugin] LoadAMXXFromBuffer: unknown format");
    return false;
}
#else
static bool LoadAMXXFromBuffer(AMX *amx, unsigned char *filebuf, size_t filesize, unsigned char **out_program)
{
    (void)amx;
    (void)filebuf;
    (void)filesize;
    (void)out_program;
    return false;
}
#endif

AMXXPlugin::AMXXPlugin()
    : m_loaded(false), m_program(nullptr), m_programSize(0)
{
    memset(&m_amx, 0, sizeof(m_amx));
}

AMXXPlugin::~AMXXPlugin()
{
    Unload();
}

bool AMXXPlugin::Load(const char *filename)
{
    AMXX_LOG_DBG("[Plugin] Loading plugin: %s", filename);

    if (m_loaded) {
        AMXX_LOG_DBG("[Plugin] Already loaded, unloading first");
        Unload();
    }

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        AMXX_LOG_DBG("[Plugin] Failed to open file: %s", filename);
        return false;
    }

    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (fileSize <= 0) {
        AMXX_LOG_DBG("[Plugin] Empty file: %s", filename);
        fclose(fp);
        return false;
    }

    AMXX_LOG_DBG("[Plugin] File size: %ld bytes", fileSize);

    unsigned char *filebuf = (unsigned char *)malloc(fileSize);
    if (!filebuf) {
        AMXX_LOG_DBG("[Plugin] Failed to allocate memory for file buffer");
        fclose(fp);
        return false;
    }

    if (fread(filebuf, 1, fileSize, fp) != (size_t)fileSize) {
        AMXX_LOG_DBG("[Plugin] Failed to read file: %s", filename);
        free(filebuf);
        fclose(fp);
        return false;
    }
    fclose(fp);

    m_filename = filename;
    m_name = filename;
    {
        // 插件名 = 基础文件名（保留 .amxx 扩展名），与原版 AMXX 一致：
        // get_plugin / is_plugin_loaded / callfunc_begin 等都以带扩展名的
        // 插件文件名（如 "menufront.amxx"）作为匹配键
        size_t pos = m_name.find_last_of("/\\");
        if (pos != std::string::npos)
            m_name = m_name.substr(pos + 1);
    }

    AMXX_LOG_DBG("[Plugin] Plugin name: %s", m_name.c_str());

    if (IsAMXX(filebuf, fileSize)) {
        AMXX_LOG_DBG("[Plugin] Detected AMXX format");
        if (!LoadAMXXFromBuffer(&m_amx, filebuf, fileSize, &m_program)) {
            AMXX_LOG_DBG("[Plugin] Failed to load AMXX file");
            free(filebuf);
            return false;
        }
        free(filebuf);
        m_loaded = true;
        AMXX_LOG_DBG("[Plugin] Successfully loaded: %s", m_name.c_str());
        return true;
    }

    AMXX_LOG_DBG("[Plugin] Detected AMX format");
    int err = amx_Init(&m_amx, filebuf);
    if (err != AMX_ERR_NONE) {
        AMXX_LOG_DBG("[Plugin] amx_Init failed: %d", err);
        free(filebuf);
        return false;
    }

    m_program = filebuf;
    m_loaded = true;
    AMXX_LOG_DBG("[Plugin] Successfully loaded: %s", m_name.c_str());
    return true;
}

void AMXXPlugin::Unload()
{
    if (!m_loaded)
        return;

    AMXX_LOG_DBG("[Plugin] Unloading plugin: %s", m_name.c_str());

    amx_Cleanup(&m_amx);

    if (m_program) {
        free(m_program);
        m_program = nullptr;
    }

    m_programSize = 0;
    m_loaded = false;
    AMXX_LOG_DBG("[Plugin] Successfully unloaded: %s", m_name.c_str());
    m_name.clear();
    m_filename.clear();
}

int AMXXPlugin::FindPublic(const char *funcname)
{
    if (!m_loaded)
        return -1;

    int index;
    int err = amx_FindPublic(&m_amx, funcname, &index);
    if (err != AMX_ERR_NONE)
        return -1;

    return index;
}

int AMXXPlugin::ExecutePublic(int index, cell *retval)
{
    if (!m_loaded)
        return AMX_ERR_NOTFOUND;
    if (index < 0)
        return AMX_ERR_INDEX;

    return amx_Exec(&m_amx, retval, index);
}

int AMXXPlugin::ExecutePublic(const char *funcname, cell *retval)
{
    int index = FindPublic(funcname);
    if (index < 0)
        return AMX_ERR_NOTFOUND;

    return ExecutePublic(index, retval);
}