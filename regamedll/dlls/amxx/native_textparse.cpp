#include "precompiled.h"
// ===========================================
// TextParse 模块 (textparse_ini.inc / textparse_smc.inc)
// 事件驱动 API: INI 解析器 (7 natives) + SMC 解析器 (8 natives)
// 移植自 AMX Mod X 原版 CTextParsers.cpp / textparse.cpp
// ===========================================

#include "amx.h"
#include "native_textparse.h"
#include <cstring>
#include <cstdio>
#include <cctype>
#include <string>
#include <vector>

// ========== 常量 ==========

// SMCResult 枚举 (textparse_smc.inc)
enum
{
    SMCRes_Continue = 0,   /* Continue parsing */
    SMCRes_Halt,           /* Stop parsing here */
    SMCRes_HaltFail        /* Stop parsing and return failure */
};

// SMCError 枚举 (textparse_smc.inc)
enum
{
    SMCErr_Okay = 0,       /* No error */
    SMCErr_StreamOpen,     /* Stream failed to open */
    SMCErr_StreamError,    /* The stream died... somehow */
    SMCErr_Custom,         /* A custom handler threw an error */
    SMCErr_InvalidSection1,/* A section was declared without quotes, and had extra tokens */
    SMCErr_InvalidSection2,/* A section was declared without any header */
    SMCErr_InvalidSection3,/* A section ending was declared with too many unknown tokens */
    SMCErr_InvalidSection4,/* A section ending has no matching beginning */
    SMCErr_InvalidSection5,/* A section beginning has no matching ending */
    SMCErr_InvalidTokens,  /* There were too many unidentifiable strings on one line */
    SMCErr_TokenOverflow,  /* The token buffer overflowed */
    SMCErr_InvalidProperty1/* A property was declared outside of any section */
};

// ========== 通用工具 ==========

// fopen 打开文件; Android 下相对路径回退到游戏目录 (gamedir) 前缀
static FILE *OpenFileStream(const char *path)
{
    FILE *fp = fopen(path, "rt");
#if defined(ANDROID) || defined(__ANDROID__)
    if (!fp && path[0] != '/' && path[0] != '\\') {
        char gameDir[256] = {0};
        GET_GAME_DIR(gameDir);
        char buf[1024];
        snprintf(buf, sizeof(buf), "%s/%s", gameDir, path);
        fp = fopen(buf, "rt");
    }
#endif
    return fp;
}

// 从 FILE* 读取整个文件内容, 失败返回 false
static bool ReadFileStream(FILE *fp, std::string &out)
{
    if (!fp) return false;
    long old = ftell(fp);
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size > 0) {
        out.resize((size_t)size);
        size_t rd = fread(&out[0], 1, (size_t)size, fp);
        out.resize(rd);
        if (rd != (size_t)size && ferror(fp))
            return false;
    }
    fseek(fp, old, SEEK_SET);
    return true;
}

// 空白字符 (与原版 g_ws_chartable 一致)
static bool WSChar(char c)
{
    return c == '\n' || c == '\v' || c == '\r' || c == '\t' || c == '\f' || c == ' ';
}

// INI 标识符合法字符 (与原版 g_ini_chartable1 + isalnum 一致)
static bool INIIsValidChar(unsigned char c)
{
    if (isalnum(c))
        return true;
    switch (c) {
        case '_': case '-': case ',': case '+': case '.':
        case '$': case '?': case '/':
            return true;
    }
    return false;
}

// ========== 句柄管理 ==========

// INI 解析器句柄数据 (回调 funcidx 在 Set* 时通过 amx_FindPublic 解析, -1 表示未设置)
struct INIParserData
{
    cell kvFuncIdx;          // OnKeyValue
    cell nsFuncIdx;          // OnNewSection
    cell rawLineFuncIdx;     // OnRawLine
    cell parseStartFuncIdx;  // OnParseStart
    cell parseEndFuncIdx;    // OnParseEnd
    INIParserData()
        : kvFuncIdx(-1), nsFuncIdx(-1), rawLineFuncIdx(-1),
          parseStartFuncIdx(-1), parseEndFuncIdx(-1) {}
};

// SMC 解析器句柄数据
struct SMCParserData
{
    cell kvFuncIdx;          // OnKeyValue -> SMCResult
    cell nsFuncIdx;          // OnNewSection -> SMCResult
    cell esFuncIdx;          // OnEndSection -> SMCResult
    cell rawLineFuncIdx;     // SMC_RawLine -> SMCResult
    cell parseStartFuncIdx;  // OnParseStart
    cell parseEndFuncIdx;    // OnParseEnd(handle, halted, failed, data)
    SMCParserData()
        : kvFuncIdx(-1), nsFuncIdx(-1), esFuncIdx(-1), rawLineFuncIdx(-1),
          parseStartFuncIdx(-1), parseEndFuncIdx(-1) {}
};

static std::vector<INIParserData *> g_iniParsers;
static std::vector<SMCParserData *> g_smcParsers;

static INIParserData *GetINIParser(cell handle)
{
    if (handle <= 0 || handle > (cell)g_iniParsers.size())
        return nullptr;
    return g_iniParsers[handle - 1];
}

static SMCParserData *GetSMCParser(cell handle)
{
    if (handle <= 0 || handle > (cell)g_smcParsers.size())
        return nullptr;
    return g_smcParsers[handle - 1];
}

// ========== INI 回调调用 ==========
// 参数按反序 amx_Push -> amx_Exec -> amx_Release; 回调返回 cell 作为 bool

static void ExecINIParseStart(AMX *amx, INIParserData *p, cell handle, cell data)
{
    if (p->parseStartFuncIdx == -1)
        return;
    // OnParseStart(handle, any:data)
    amx_Push(amx, data);
    amx_Push(amx, handle);
    cell retval;
    amx_Exec(amx, &retval, (int)p->parseStartFuncIdx);
}

static void ExecINIParseEnd(AMX *amx, INIParserData *p, cell handle, bool halted, cell data)
{
    if (p->parseEndFuncIdx == -1)
        return;
    // OnParseEnd(handle, bool:halted, any:data)
    amx_Push(amx, data);
    amx_Push(amx, halted ? 1 : 0);
    amx_Push(amx, handle);
    cell retval;
    amx_Exec(amx, &retval, (int)p->parseEndFuncIdx);
}

static bool ExecININewSection(AMX *amx, INIParserData *p, cell handle,
                              const char *section, bool invalid_tokens, bool close_bracket,
                              bool extra_tokens, unsigned int curtok, cell data)
{
    if (p->nsFuncIdx == -1)
        return true;
    // OnNewSection(handle, const section[], invalid_tokens, close_bracket, extra_tokens, curtok, data)
    amx_Push(amx, data);
    amx_Push(amx, (cell)curtok);
    amx_Push(amx, extra_tokens ? 1 : 0);
    amx_Push(amx, close_bracket ? 1 : 0);
    amx_Push(amx, invalid_tokens ? 1 : 0);
    cell addr;
    amx_PushString(amx, &addr, nullptr, section ? section : "", 0, 0);
    amx_Push(amx, handle);
    cell retval = 0;
    amx_Exec(amx, &retval, (int)p->nsFuncIdx);
    amx_Release(amx, addr);
    return retval != 0;
}

static bool ExecINIKeyValue(AMX *amx, INIParserData *p, cell handle,
                            const char *key, const char *value, bool invalid_tokens,
                            bool equal_token, bool quotes, unsigned int curtok, cell data)
{
    if (p->kvFuncIdx == -1)
        return true;
    // OnKeyValue(handle, const key[], const value[], invalid_tokens, equal_token, quotes, curtok, data)
    amx_Push(amx, data);
    amx_Push(amx, (cell)curtok);
    amx_Push(amx, quotes ? 1 : 0);
    amx_Push(amx, equal_token ? 1 : 0);
    amx_Push(amx, invalid_tokens ? 1 : 0);
    cell addr_val, addr_key;
    amx_PushString(amx, &addr_val, nullptr, value ? value : "", 0, 0);
    amx_PushString(amx, &addr_key, nullptr, key ? key : "", 0, 0);
    amx_Push(amx, handle);
    cell retval = 0;
    amx_Exec(amx, &retval, (int)p->kvFuncIdx);
    amx_Release(amx, addr_key);
    amx_Release(amx, addr_val);
    return retval != 0;
}

static bool ExecINIRawLine(AMX *amx, INIParserData *p, cell handle,
                           const char *line, int lineno, unsigned int curtok, cell data)
{
    if (p->rawLineFuncIdx == -1)
        return true;
    // OnRawLine(handle, const line[], lineno, curtok, data)
    amx_Push(amx, data);
    amx_Push(amx, (cell)curtok);
    amx_Push(amx, (cell)lineno);
    cell addr;
    amx_PushString(amx, &addr, nullptr, line ? line : "", 0, 0);
    amx_Push(amx, handle);
    cell retval = 0;
    amx_Exec(amx, &retval, (int)p->rawLineFuncIdx);
    amx_Release(amx, addr);
    return retval != 0;
}

// ========== INI 解析器 ==========

// INI_CreateParser()
cell AMX_NATIVE_CALL amxx_ini_create_parser(AMX *amx, cell *params)
{
    (void)amx;
    (void)params;
    INIParserData *p = new INIParserData();
    g_iniParsers.push_back(p);
    return (cell)g_iniParsers.size();
}

// INI_DestroyParser(&handle) - 销毁, 句柄置 0, 返回 true/false
cell AMX_NATIVE_CALL amxx_ini_destroy_parser(AMX *amx, cell *params)
{
    cell *handle_addr;
    amx_GetAddr(amx, params[1], &handle_addr);
    if (!handle_addr) return 0;
    cell handle = *handle_addr;
    INIParserData *p = GetINIParser(handle);
    if (!p) return 0;
    delete p;
    g_iniParsers[handle - 1] = nullptr;
    *handle_addr = 0;
    return 1;
}

// INI_ParseFile(handle, file[], &line=0, &col=0, any:data=0) - 解析文件, 返回 true/false
cell AMX_NATIVE_CALL amxx_ini_parse_file(AMX *amx, cell *params)
{
    INIParserData *p = GetINIParser(params[1]);
    if (!p) return 0;

    cell *file_addr;
    amx_GetAddr(amx, params[2], &file_addr);
    char file[512];
    amx_GetString(file, file_addr, 0, sizeof(file));

    cell data = 0;
    if (params[0] / (int)sizeof(cell) >= 5)
        data = params[5];

    FILE *fp = OpenFileStream(file);
    unsigned int curline = 0;
    unsigned int curtok = 0;
    bool result = false;

    if (fp) {
        char buffer[2048];
        char *ptr, *save_ptr;
        bool in_quote;
        bool eventFailed = false;

        ExecINIParseStart(amx, p, params[1], data);

        while (!feof(fp) && !eventFailed) {
            curline++;
            curtok = 0;
            buffer[0] = '\0';
            if (fgets(buffer, sizeof(buffer), fp) == NULL)
                break;

            ptr = buffer;

            // 第一行跳过 UTF-8 BOM
            if (curline == 1 &&
                (unsigned char)buffer[0] == 0xEF &&
                (unsigned char)buffer[1] == 0xBB &&
                (unsigned char)buffer[2] == 0xBF) {
                ptr = &buffer[3];
            }

            // 去除行首空白
            while (*ptr != '\0' && WSChar(*ptr))
                ptr++;

            size_t len = strlen(ptr);
            if (!len)
                continue;

            // 剥离 ; 注释 (引号内不剥离)
            in_quote = false;
            save_ptr = ptr;
            for (size_t i = 0; i < len; i++, ptr++) {
                if (!in_quote) {
                    switch (*ptr) {
                        case '"':
                            in_quote = true;
                            break;
                        case ';':
                            len = i;
                            *ptr = '\0';
                            break;
                    }
                } else {
                    if (*ptr == '"')
                        in_quote = false;
                }
            }
            if (!len)
                continue;
            ptr = save_ptr;

            // 去除行尾空白
            for (size_t i = len - 1; i < len; i--) {
                if (WSChar(ptr[i])) {
                    ptr[i] = '\0';
                    len--;
                } else {
                    break;
                }
            }
            if (!len)
                continue;

            // RawLine 回调 (若返回 false 则停止, halted=true)
            if (!ExecINIRawLine(amx, p, params[1], ptr, (int)curline, curtok, data)) {
                eventFailed = true;
                break;
            }

            if (*ptr == '[') {
                // 节头: [SECTION] 后可有附加 token
                bool invalid_tokens = false;
                bool got_bracket = false;
                bool extra_tokens = false;
                for (size_t i = 1; i < len; i++) {
                    char c = ptr[i];
                    if (!INIIsValidChar((unsigned char)c)) {
                        if (c == ']') {
                            got_bracket = true;
                            if (i != len - 1)
                                extra_tokens = true;
                            ptr[i] = '\0';
                            break;
                        } else {
                            invalid_tokens = true;
                        }
                    }
                }
                if (!ExecININewSection(amx, p, params[1], &ptr[1], invalid_tokens,
                                       got_bracket, extra_tokens, curtok, data)) {
                    eventFailed = true;
                    break;
                }
            } else {
                // 键值: KEY = "VALUE" / KEY = VALUE / 裸 KEY
                char *key_ptr = ptr;
                char *val_ptr = NULL;
                size_t first_space = 0;
                bool invalid_tokens = false;
                bool equal_token = false;
                bool quotes = false;

                for (size_t i = 0; i < len; i++) {
                    char c = ptr[i];
                    if (!INIIsValidChar((unsigned char)c)) {
                        if (WSChar(c)) {
                            if (!first_space)
                                first_space = i;
                        } else if (c == '=') {
                            if (first_space)
                                key_ptr[first_space] = '\0';
                            else
                                key_ptr[i] = '\0';
                            if (ptr[++i] != '\0')
                                val_ptr = &ptr[i];
                            equal_token = true;
                            break;
                        } else {
                            invalid_tokens = true;
                            first_space = 0;
                        }
                    }
                }

                if (val_ptr) {
                    // 吃掉 '=' 后的空白
                    while (*val_ptr != '\0' && WSChar(*val_ptr))
                        val_ptr++;
                    if (*val_ptr == '\0') {
                        val_ptr = NULL;
                    } else if (*val_ptr == '"') {
                        // 首尾引号剥离
                        size_t vlen = strlen(val_ptr);
                        if (val_ptr[vlen - 1] == '"') {
                            val_ptr[--vlen] = '\0';
                            val_ptr++;
                            quotes = true;
                        }
                    }
                }

                if (val_ptr)
                    curtok = (unsigned int)(val_ptr - buffer);
                else
                    curtok = 0;

                if (!ExecINIKeyValue(amx, p, params[1], key_ptr, val_ptr, invalid_tokens,
                                     equal_token, quotes, curtok, data)) {
                    curtok = 0;
                    eventFailed = true;
                    break;
                }
            }
        }

        fclose(fp);
        result = !eventFailed;
        ExecINIParseEnd(amx, p, params[1], eventFailed, data);
    }

    // &line / &col 可选参数 (by-ref 输出最后位置)
    if (params[0] / (int)sizeof(cell) >= 4) {
        cell *line_addr;
        amx_GetAddr(amx, params[3], &line_addr);
        if (line_addr) *line_addr = (cell)curline;
    }
    if (params[0] / (int)sizeof(cell) >= 5) {
        cell *col_addr;
        amx_GetAddr(amx, params[4], &col_addr);
        if (col_addr) *col_addr = (cell)curtok;
    }

    return result ? 1 : 0;
}

// 解析回调函数名 -> funcidx (空名保留原值, 解析失败返回 false)
static bool ResolvePublic(AMX *amx, const char *name, cell *outIdx)
{
    if (name[0] != '\0') {
        int idx = -1;
        if (amx_FindPublic(amx, name, &idx) == AMX_ERR_NONE) {
            *outIdx = (cell)idx;
            return true;
        }
        *outIdx = -1;
        return false;
    }
    return true;   // 空名: 保留原值
}

// INI_SetParseStart(handle, func[]) - 设置 OnParseStart(handle, data) 回调
cell AMX_NATIVE_CALL amxx_ini_set_parse_start(AMX *amx, cell *params)
{
    INIParserData *p = GetINIParser(params[1]);
    if (!p) return 0;

    cell *func_addr;
    amx_GetAddr(amx, params[2], &func_addr);
    char func[256];
    amx_GetString(func, func_addr, 0, sizeof(func));

    if (!ResolvePublic(amx, func, &p->parseStartFuncIdx)) {
        AMXX_LOG_ERR("[TextParse] INI_SetParseStart: function is not present (\"%s\")", func);
        return 0;
    }
    return 1;
}

// INI_SetParseEnd(handle, func[]) - 设置 OnParseEnd(handle, bool:halted, data) 回调
cell AMX_NATIVE_CALL amxx_ini_set_parse_end(AMX *amx, cell *params)
{
    INIParserData *p = GetINIParser(params[1]);
    if (!p) return 0;

    cell *func_addr;
    amx_GetAddr(amx, params[2], &func_addr);
    char func[256];
    amx_GetString(func, func_addr, 0, sizeof(func));

    if (!ResolvePublic(amx, func, &p->parseEndFuncIdx)) {
        AMXX_LOG_ERR("[TextParse] INI_SetParseEnd: function is not present (\"%s\")", func);
        return 0;
    }
    return 1;
}

// INI_SetReaders(handle, kvFunc[], nsFunc[]="") - 设置 KeyValue + NewSection 回调
cell AMX_NATIVE_CALL amxx_ini_set_readers(AMX *amx, cell *params)
{
    INIParserData *p = GetINIParser(params[1]);
    if (!p) return 0;

    cell *func_addr;
    char kv[256], ns[256];
    amx_GetAddr(amx, params[2], &func_addr);
    amx_GetString(kv, func_addr, 0, sizeof(kv));
    amx_GetAddr(amx, params[3], &func_addr);
    amx_GetString(ns, func_addr, 0, sizeof(ns));

    bool kvProvided = kv[0] != '\0';
    bool nsProvided = ns[0] != '\0';

    if (kvProvided)
        ResolvePublic(amx, kv, &p->kvFuncIdx);
    if (kvProvided && nsProvided)
        ResolvePublic(amx, ns, &p->nsFuncIdx);

    if (p->kvFuncIdx == -1 || (nsProvided && p->nsFuncIdx == -1)) {
        AMXX_LOG_ERR("[TextParse] INI_SetReaders: function is not present (kv=\"%s\", ns=\"%s\")", kv, ns);
        return 0;
    }
    return 1;
}

// INI_SetRawLine(handle, func[]) - 设置 OnRawLine(handle, line[], lineno, curtok, data) 回调
cell AMX_NATIVE_CALL amxx_ini_set_raw_line(AMX *amx, cell *params)
{
    INIParserData *p = GetINIParser(params[1]);
    if (!p) return 0;

    cell *func_addr;
    amx_GetAddr(amx, params[2], &func_addr);
    char func[256];
    amx_GetString(func, func_addr, 0, sizeof(func));

    if (!ResolvePublic(amx, func, &p->rawLineFuncIdx)) {
        AMXX_LOG_ERR("[TextParse] INI_SetRawLine: function is not present (\"%s\")", func);
        return 0;
    }
    return 1;
}

// ========== SMC 回调调用 ==========
// 参数按反序 amx_Push -> amx_Exec -> amx_Release; 回调返回 cell 作为 SMCResult

static void ExecSMCParseStart(AMX *amx, SMCParserData *p, cell handle, cell data)
{
    if (p->parseStartFuncIdx == -1)
        return;
    // OnParseStart(handle, any:data)
    amx_Push(amx, data);
    amx_Push(amx, handle);
    cell retval;
    amx_Exec(amx, &retval, (int)p->parseStartFuncIdx);
}

static void ExecSMCParseEnd(AMX *amx, SMCParserData *p, cell handle, bool halted, bool failed, cell data)
{
    if (p->parseEndFuncIdx == -1)
        return;
    // OnParseEnd(handle, bool:halted, bool:failed, any:data)
    amx_Push(amx, data);
    amx_Push(amx, failed ? 1 : 0);
    amx_Push(amx, halted ? 1 : 0);
    amx_Push(amx, handle);
    cell retval;
    amx_Exec(amx, &retval, (int)p->parseEndFuncIdx);
}

static cell ExecSMCNewSection(AMX *amx, SMCParserData *p, cell handle, const char *name, cell data)
{
    if (p->nsFuncIdx == -1)
        return SMCRes_Continue;
    // OnNewSection(handle, const name[], any:data) -> SMCResult
    amx_Push(amx, data);
    cell addr;
    amx_PushString(amx, &addr, nullptr, name ? name : "", 0, 0);
    amx_Push(amx, handle);
    cell retval = 0;
    amx_Exec(amx, &retval, (int)p->nsFuncIdx);
    amx_Release(amx, addr);
    return retval;
}

static cell ExecSMCKeyValue(AMX *amx, SMCParserData *p, cell handle, const char *key, const char *value, cell data)
{
    if (p->kvFuncIdx == -1)
        return SMCRes_Continue;
    // OnKeyValue(handle, const key[], const value[], any:data) -> SMCResult
    amx_Push(amx, data);
    cell addr_val, addr_key;
    amx_PushString(amx, &addr_val, nullptr, value ? value : "", 0, 0);
    amx_PushString(amx, &addr_key, nullptr, key ? key : "", 0, 0);
    amx_Push(amx, handle);
    cell retval = 0;
    amx_Exec(amx, &retval, (int)p->kvFuncIdx);
    amx_Release(amx, addr_key);
    amx_Release(amx, addr_val);
    return retval;
}

static cell ExecSMCEndSection(AMX *amx, SMCParserData *p, cell handle, cell data)
{
    if (p->esFuncIdx == -1)
        return SMCRes_Continue;
    // OnEndSection(handle, any:data) -> SMCResult
    amx_Push(amx, data);
    amx_Push(amx, handle);
    cell retval = 0;
    amx_Exec(amx, &retval, (int)p->esFuncIdx);
    return retval;
}

static cell ExecSMCRawLine(AMX *amx, SMCParserData *p, cell handle, const char *line, int lineno, cell data)
{
    if (p->rawLineFuncIdx == -1)
        return SMCRes_Continue;
    // SMC_RawLine(handle, const line[], lineno, any:data) -> SMCResult
    amx_Push(amx, data);
    amx_Push(amx, (cell)lineno);
    cell addr;
    amx_PushString(amx, &addr, nullptr, line ? line : "", 0, 0);
    amx_Push(amx, handle);
    cell retval = 0;
    amx_Exec(amx, &retval, (int)p->rawLineFuncIdx);
    amx_Release(amx, addr);
    return retval;
}

// ========== SMC 解析器 ==========
// 状态机移植自原版 TextParsers::ParseStream_SMC
// 格式: "KEY" "VALUE" / SECTION 后跟 { 块 / } 结束
//   引号字符串支持 \" 转义; 无引号标识符到空白结束;
//   ; // /* */ 注释剥离 (字符串内不剥离)

struct SMCState
{
    int line;
    int col;
};

struct SMCStr
{
    int ptr;        // 内容起始偏移, -1 表示空
    int end;        // 内容结束偏移 (不含), -1 表示到文件尾
    bool quoted;
    bool special;
    SMCStr() : ptr(-1), end(-1), quoted(false), special(false) {}
    void reset() { ptr = -1; end = -1; quoted = false; special = false; }
    bool isNull() const { return ptr == -1; }
};

// 下移暂存字符串; 返回 true 表示 strings[2] 已有内容 (溢出)
static bool SMCRotate(SMCStr info[3])
{
    if (!info[2].isNull())
        return true;
    if (!info[0].isNull()) {
        info[2] = info[1];
        info[1] = info[0];
        info[0] = SMCStr();
    }
    return false;
}

static void SMCScrap(SMCStr info[3])
{
    info[0].reset();
    info[1].reset();
    info[2].reset();
}

// 提取字符串内容 (剥离引号, 解析转义), 空串返回 ""
static std::string SMCFixup(const SMCStr &s, const std::string &content)
{
    if (s.isNull())
        return std::string();
    int start = s.ptr + (s.quoted ? 1 : 0);
    int end = (s.end == -1) ? (int)content.size() : s.end;
    std::string out;
    if (end > start)
        out.assign(content, (size_t)start, (size_t)(end - start));
    if (s.special) {
        std::string res;
        res.reserve(out.size());
        for (size_t i = 0; i < out.size(); i++) {
            if (out[i] == '\\' && i + 1 < out.size()) {
                i++;
                char c2 = out[i];
                if (c2 == 'n') res += '\n';
                else if (c2 == 't') res += '\t';
                else if (c2 == 'r') res += '\r';
                else if (c2 == '\\' || c2 == '"') res += c2;
                else { res += '\\'; i--; }   // 非法转义: 保留反斜杠
            } else {
                res += out[i];
            }
        }
        out = res;
    }
    return out;
}

// 核心状态机; 返回 SMCError 码, states 输出最后 line/col
static int ParseSMCStream(AMX *amx, SMCParserData *p, cell handle, cell data,
                          const std::string &content, SMCState &states)
{
    int i = 0;
    int length = (int)content.size();
    int line_begin = 0;
    unsigned int curlevel = 0;
    bool in_quote = false;
    bool ignoring = false;
    bool ml_comment = false;
    int err = SMCErr_Okay;
    cell res;
    SMCStr strings[3];

    states.line = 1;
    states.col = 0;

    // 第一行跳过 UTF-8 BOM
    if (length >= 3 && (unsigned char)content[0] == 0xEF &&
        (unsigned char)content[1] == 0xBB && (unsigned char)content[2] == 0xBF) {
        i = 3;
        line_begin = 3;
    }

    ExecSMCParseStart(amx, p, handle, data);

    while (i < length) {
        char c = content[i];

        // 单行超过 4095 字节且无换行 -> TokenOverflow (对应原版 4096 字节缓冲)
        if (i - line_begin >= 4095) {
            err = SMCErr_TokenOverflow;
            goto failed;
        }

        if (c == '\n') {
            /* 换行: 先轮转暂存的字符串 */
            if (!strings[0].isNull()) {
                strings[0].end = i;
                if (SMCRotate(strings)) {
                    err = SMCErr_InvalidTokens;
                    goto failed;
                }
            }

            in_quote = false;
            if (ignoring && !ml_comment)
                ignoring = false;

            /* RawLine 回调 (整行原始内容, 不含换行符) */
            {
                std::string line(content.substr((size_t)line_begin, (size_t)(i - line_begin)));
                res = ExecSMCRawLine(amx, p, handle, line.c_str(), states.line, data);
                if (res != SMCRes_Continue) {
                    err = (res == SMCRes_HaltFail) ? SMCErr_Custom : SMCErr_Okay;
                    goto failed;
                }
            }

            /* 行尾若已暂存 key/value 对则触发 KeyValue 回调 */
            if (!strings[2].isNull()) {
                if (!curlevel) {
                    err = SMCErr_InvalidProperty1;
                    goto failed;
                }
                res = ExecSMCKeyValue(amx, p, handle,
                                      SMCFixup(strings[2], content).c_str(),
                                      SMCFixup(strings[1], content).c_str(), data);
                if (res != SMCRes_Continue) {
                    err = (res == SMCRes_HaltFail) ? SMCErr_Custom : SMCErr_Okay;
                    goto failed;
                }
                SMCScrap(strings);
            }

            states.col = 0;
            states.line++;
            line_begin = i + 1;
        } else if (ignoring) {
            if (in_quote) {
                /* 引号字符串内部: 找结束引号 (\" 转义) */
                if (i > 0 && c == '"' && content[i - 1] != '\\') {
                    in_quote = false;
                    ignoring = false;
                    strings[0].end = i;
                    strings[0].quoted = true;
                    if (SMCRotate(strings)) {
                        err = SMCErr_InvalidTokens;
                        goto failed;
                    }
                } else if (c == '\\') {
                    strings[0].special = true;
                    if (i == length - 1)
                        break;   // 文件尾反斜杠: 引号保持未闭合 (与原版缓冲行为一致)
                }
            } else if (ml_comment) {
                if (c == '*') {
                    if (i == length - 1)
                        break;
                    if (content[i + 1] == '/') {
                        ml_comment = false;
                        ignoring = false;
                        i++;          // 跳过 '/'
                        states.col++;
                    }
                }
            }
        } else {
            if (!WSChar(c)) {
                bool restage = false;
                /* 注释: 分号 ; 与双斜杠 // 为行注释, 斜杠星号与星号斜杠为块注释 (字符串内由 ignoring 分支处理) */
                if (c == ';' || c == '/') {
                    if (c == '/') {
                        if (i == length - 1)
                            break;   // 文件尾孤立 '/': 忽略 (与原版缓冲行为一致)
                        if (content[i + 1] == '/') {
                            ignoring = true;
                            restage = true;
                        } else if (content[i + 1] == '*') {
                            ignoring = true;
                            ml_comment = true;
                            restage = true;
                        }
                    } else {
                        ignoring = true;
                        restage = true;
                    }
                } else if (c == '{') {
                    /* 节头 { : 前面必须恰好有一个暂存字符串 */
                    if (!strings[0].isNull()) {
                        if (SMCRotate(strings)) {
                            err = SMCErr_InvalidSection1;
                            goto failed;
                        }
                    }
                    if (!strings[2].isNull()) {
                        err = SMCErr_InvalidSection1;
                        goto failed;
                    } else if (strings[1].isNull()) {
                        err = SMCErr_InvalidSection2;
                        goto failed;
                    }
                    std::string name = SMCFixup(strings[1], content);
                    res = ExecSMCNewSection(amx, p, handle, name.c_str(), data);
                    if (res != SMCRes_Continue) {
                        err = (res == SMCRes_HaltFail) ? SMCErr_Custom : SMCErr_Okay;
                        goto failed;
                    }
                    strings[1].reset();
                    curlevel++;
                } else if (c == '}') {
                    /* 节尾 } : 前面可有一个 key/value 对 */
                    if (SMCRotate(strings)) {
                        err = SMCErr_InvalidSection3;
                        goto failed;
                    }
                    if (!strings[2].isNull()) {
                        if (!curlevel) {
                            err = SMCErr_InvalidProperty1;
                            goto failed;
                        }
                        res = ExecSMCKeyValue(amx, p, handle,
                                              SMCFixup(strings[2], content).c_str(),
                                              SMCFixup(strings[1], content).c_str(), data);
                        if (res != SMCRes_Continue) {
                            err = (res == SMCRes_HaltFail) ? SMCErr_Custom : SMCErr_Okay;
                            goto failed;
                        }
                    } else if (!strings[1].isNull()) {
                        err = SMCErr_InvalidSection3;
                        goto failed;
                    } else if (!curlevel) {
                        err = SMCErr_InvalidSection4;
                        goto failed;
                    }
                    SMCScrap(strings);
                    res = ExecSMCEndSection(amx, p, handle, data);
                    if (res != SMCRes_Continue) {
                        err = (res == SMCRes_HaltFail) ? SMCErr_Custom : SMCErr_Okay;
                        goto failed;
                    }
                    curlevel--;
                } else if (c == '"') {
                    /* 引号字符串开始 */
                    if (!strings[0].isNull()) {
                        strings[0].end = i;
                        if (SMCRotate(strings)) {
                            err = SMCErr_InvalidTokens;
                            goto failed;
                        }
                    }
                    strings[0].ptr = i;
                    in_quote = true;
                    ignoring = true;
                } else if (strings[0].isNull()) {
                    /* 无引号标识符开始 */
                    strings[0].ptr = i;
                }
                /* 注释开始处若有暂存字符串则轮转 (restage) */
                if (restage && !strings[0].isNull()) {
                    strings[0].end = i;
                    if (SMCRotate(strings)) {
                        err = SMCErr_InvalidTokens;
                        goto failed;
                    }
                }
            } else {
                /* 空白: 结束无引号标识符 */
                if (!strings[0].isNull()) {
                    if (strings[1].isNull()) {
                        strings[0].end = i;
                        SMCRotate(strings);
                    } else if (!strings[1].quoted) {
                        err = SMCErr_InvalidTokens;
                        goto failed;
                    }
                }
            }
        }

        states.col++;
        i++;
    }

    /* EOF: 节未闭合 -> InvalidSection5; 残留 token -> InvalidTokens */
    if (curlevel) {
        err = SMCErr_InvalidSection5;
        goto failed;
    } else if (!strings[0].isNull() || !strings[1].isNull()) {
        err = SMCErr_InvalidTokens;
        goto failed;
    }

    ExecSMCParseEnd(amx, p, handle, false, false, data);

    return SMCErr_Okay;

failed:
    ExecSMCParseEnd(amx, p, handle, true, (err == SMCErr_Custom), data);
    return err;
}

// SMC_CreateParser()
cell AMX_NATIVE_CALL amxx_smc_create_parser(AMX *amx, cell *params)
{
    (void)amx;
    (void)params;
    SMCParserData *p = new SMCParserData();
    g_smcParsers.push_back(p);
    return (cell)g_smcParsers.size();
}

// SMC_DestroyParser(&handle) - 销毁, 句柄置 0, 返回 true/false
cell AMX_NATIVE_CALL amxx_smc_destroy_parser(AMX *amx, cell *params)
{
    cell *handle_addr;
    amx_GetAddr(amx, params[1], &handle_addr);
    if (!handle_addr) return 0;
    cell handle = *handle_addr;
    SMCParserData *p = GetSMCParser(handle);
    if (!p) return 0;
    delete p;
    g_smcParsers[handle - 1] = nullptr;
    *handle_addr = 0;
    return 1;
}

// SMC_ParseFile(handle, file[], &line=0, &col=0, any:data=0) - 返回 SMCError 码
cell AMX_NATIVE_CALL amxx_smc_parse_file(AMX *amx, cell *params)
{
    SMCParserData *p = GetSMCParser(params[1]);
    if (!p) return 0;

    cell *file_addr;
    amx_GetAddr(amx, params[2], &file_addr);
    char file[512];
    amx_GetString(file, file_addr, 0, sizeof(file));

    cell data = 0;
    if (params[0] / (int)sizeof(cell) >= 5)
        data = params[5];

    SMCState states;
    states.line = 0;
    states.col = 0;

    int result = SMCErr_StreamOpen;

    FILE *fp = OpenFileStream(file);
    if (fp) {
        std::string content;
        if (ReadFileStream(fp, content)) {
            result = ParseSMCStream(amx, p, params[1], data, content, states);
        } else {
            states.line = 0;
            states.col = 0;
            result = SMCErr_StreamError;
        }
        fclose(fp);
    }

    // &line / &col 可选参数 (by-ref 输出最后位置)
    if (params[0] / (int)sizeof(cell) >= 4) {
        cell *line_addr;
        amx_GetAddr(amx, params[3], &line_addr);
        if (line_addr) *line_addr = (cell)states.line;
    }
    if (params[0] / (int)sizeof(cell) >= 5) {
        cell *col_addr;
        amx_GetAddr(amx, params[4], &col_addr);
        if (col_addr) *col_addr = (cell)states.col;
    }

    return (cell)result;
}

// SMC_SetParseStart(handle, func[]) - 设置 OnParseStart(handle, data) 回调
cell AMX_NATIVE_CALL amxx_smc_set_parse_start(AMX *amx, cell *params)
{
    SMCParserData *p = GetSMCParser(params[1]);
    if (!p) return 0;

    cell *func_addr;
    amx_GetAddr(amx, params[2], &func_addr);
    char func[256];
    amx_GetString(func, func_addr, 0, sizeof(func));

    if (!ResolvePublic(amx, func, &p->parseStartFuncIdx)) {
        AMXX_LOG_ERR("[TextParse] SMC_SetParseStart: function is not present (\"%s\")", func);
        return 0;
    }
    return 1;
}

// SMC_SetParseEnd(handle, func[]) - 设置 OnParseEnd(handle, bool:halted, bool:failed, data) 回调
cell AMX_NATIVE_CALL amxx_smc_set_parse_end(AMX *amx, cell *params)
{
    SMCParserData *p = GetSMCParser(params[1]);
    if (!p) return 0;

    cell *func_addr;
    amx_GetAddr(amx, params[2], &func_addr);
    char func[256];
    amx_GetString(func, func_addr, 0, sizeof(func));

    if (!ResolvePublic(amx, func, &p->parseEndFuncIdx)) {
        AMXX_LOG_ERR("[TextParse] SMC_SetParseEnd: function is not present (\"%s\")", func);
        return 0;
    }
    return 1;
}

// SMC_SetReaders(handle, kvFunc[], nsFunc[]="", esFunc[]="")
// 设置 OnKeyValue / OnNewSection / OnEndSection 回调 (均返回 SMCResult)
cell AMX_NATIVE_CALL amxx_smc_set_readers(AMX *amx, cell *params)
{
    SMCParserData *p = GetSMCParser(params[1]);
    if (!p) return 0;

    cell *func_addr;
    char kv[256], ns[256], es[256];
    amx_GetAddr(amx, params[2], &func_addr);
    amx_GetString(kv, func_addr, 0, sizeof(kv));
    amx_GetAddr(amx, params[3], &func_addr);
    amx_GetString(ns, func_addr, 0, sizeof(ns));
    amx_GetAddr(amx, params[4], &func_addr);
    amx_GetString(es, func_addr, 0, sizeof(es));

    bool kvProvided = kv[0] != '\0';
    bool nsProvided = ns[0] != '\0';
    bool esProvided = es[0] != '\0';

    if (kvProvided)
        ResolvePublic(amx, kv, &p->kvFuncIdx);
    if (kvProvided && nsProvided)
        ResolvePublic(amx, ns, &p->nsFuncIdx);
    if (kvProvided && esProvided)
        ResolvePublic(amx, es, &p->esFuncIdx);

    if (p->kvFuncIdx == -1 || (nsProvided && p->nsFuncIdx == -1) || (esProvided && p->esFuncIdx == -1)) {
        AMXX_LOG_ERR("[TextParse] SMC_SetReaders: function is not present (kv=\"%s\", ns=\"%s\", es=\"%s\")", kv, ns, es);
        return 0;
    }
    return 1;
}

// SMC_SetRawLine(handle, func[]) - 设置 SMC_RawLine(handle, line[], lineno, data) 回调
cell AMX_NATIVE_CALL amxx_smc_set_raw_line(AMX *amx, cell *params)
{
    SMCParserData *p = GetSMCParser(params[1]);
    if (!p) return 0;

    cell *func_addr;
    amx_GetAddr(amx, params[2], &func_addr);
    char func[256];
    amx_GetString(func, func_addr, 0, sizeof(func));

    if (!ResolvePublic(amx, func, &p->rawLineFuncIdx)) {
        AMXX_LOG_ERR("[TextParse] SMC_SetRawLine: function is not present (\"%s\")", func);
        return 0;
    }
    return 1;
}

// SMC_GetErrorString(error, buffer[], buf_max)
// SMCError_Okay 和 SMCError_Custom 返回 false
cell AMX_NATIVE_CALL amxx_smc_get_error_string(AMX *amx, cell *params)
{
    int err = (int)params[1];
    const char *str = nullptr;
    switch (err) {
        case SMCErr_Okay:            break;                    /* 无错误 */
        case SMCErr_StreamOpen:      str = "Stream failed to open"; break;
        case SMCErr_StreamError:     str = "Stream returned read error"; break;
        case SMCErr_Custom:          break;                    /* 自定义错误 */
        case SMCErr_InvalidSection1: str = "Un-quoted section has invalid tokens"; break;
        case SMCErr_InvalidSection2: str = "Section declared without header"; break;
        case SMCErr_InvalidSection3: str = "Section declared with unknown tokens"; break;
        case SMCErr_InvalidSection4: str = "Section ending without a matching section beginning"; break;
        case SMCErr_InvalidSection5: str = "Section beginning without a matching ending"; break;
        case SMCErr_InvalidTokens:   str = "Line contained too many invalid tokens"; break;
        case SMCErr_TokenOverflow:   str = "Token buffer overflowed"; break;
        case SMCErr_InvalidProperty1:str = "A property was declared outside of a section"; break;
        default:                     break;                    /* 非法错误码 */
    }
    if (!str)
        return 0;

    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    if (!dest)
        return 0;
    return amx_SetString(dest, str, 0, 0, (size_t)params[3]);
}

// ========== 注册 ==========

AMX_NATIVE_INFO textparse_natives[] = {
    // INI
    {"INI_CreateParser", amxx_ini_create_parser},
    {"INI_DestroyParser", amxx_ini_destroy_parser},
    {"INI_ParseFile", amxx_ini_parse_file},
    {"INI_SetParseStart", amxx_ini_set_parse_start},
    {"INI_SetParseEnd", amxx_ini_set_parse_end},
    {"INI_SetReaders", amxx_ini_set_readers},
    {"INI_SetRawLine", amxx_ini_set_raw_line},
    // SMC
    {"SMC_CreateParser", amxx_smc_create_parser},
    {"SMC_DestroyParser", amxx_smc_destroy_parser},
    {"SMC_ParseFile", amxx_smc_parse_file},
    {"SMC_SetParseStart", amxx_smc_set_parse_start},
    {"SMC_SetParseEnd", amxx_smc_set_parse_end},
    {"SMC_SetReaders", amxx_smc_set_readers},
    {"SMC_SetRawLine", amxx_smc_set_raw_line},
    {"SMC_GetErrorString", amxx_smc_get_error_string},
    {nullptr, nullptr}
};

// 清理所有 INI/SMC 句柄 (地图切换时调用, 与 ResetGameConfigGlobals 一致)
void ResetTextparseGlobals()
{
    for (size_t i = 0; i < g_iniParsers.size(); i++) {
        delete g_iniParsers[i];
    }
    g_iniParsers.clear();
    for (size_t i = 0; i < g_smcParsers.size(); i++) {
        delete g_smcParsers[i];
    }
    g_smcParsers.clear();
}
