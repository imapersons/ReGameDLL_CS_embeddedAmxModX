// ===========================================
// DBI Module - AMX Mod X dbi.inc API
// 13 natives, implemented on top of SQLite3:
//   dbi_connect, dbi_query, dbi_query2, dbi_nextrow,
//   dbi_field, dbi_result, dbi_num_rows, dbi_free_result,
//   dbi_close, dbi_error, dbi_type, dbi_num_fields, dbi_field_name
// ===========================================

#include "precompiled.h"
#include "native_dbi.h"
#include "amx.h"
#include "sqlite3/sqlite3.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ===========================================
// Constants
// ===========================================
#define SQL_FAILED    0
#define SQL_OK        1

#define RESULT_FAILED (-1)
#define RESULT_NONE   0
#define RESULT_OK     1

// ===========================================
// Handle pools
//   g_dbiConnections: vector<sqlite3*>, handle = index (1-based; index 0 is a placeholder)
//   g_dbiResults:     vector<DbiResultData>, handle = index (1-based; index 0 is a placeholder)
//   Free slots are marked by nullptr connections and results with stmt == nullptr (reused on release)
// ===========================================
struct DbiResultData
{
    sqlite3 *db;          // owning connection
    sqlite3_stmt *stmt;   // prepared statement (set to nullptr after release, marks the slot as free)
    int affected;         // affected row count (dbi_query2)
    std::string error;    // last error message
};

static std::vector<sqlite3 *> g_dbiConnections;
static std::vector<DbiResultData> g_dbiResults;

// ===========================================
// Handle management - connections
// ===========================================
static sqlite3 *GetConn(int handle)
{
    if (handle <= 0 || (size_t)handle >= g_dbiConnections.size())
        return nullptr;
    return g_dbiConnections[handle];
}

static int AllocConnHandle(sqlite3 *db)
{
    if (g_dbiConnections.empty())
        g_dbiConnections.push_back(nullptr);  // index 0 placeholder

    for (size_t i = 1; i < g_dbiConnections.size(); i++) {
        if (g_dbiConnections[i] == nullptr) {
            g_dbiConnections[i] = db;
            return (int)i;
        }
    }

    g_dbiConnections.push_back(db);
    return (int)(g_dbiConnections.size() - 1);
}

// ===========================================
// Handle management - results
// ===========================================
static DbiResultData *GetResult(int handle)
{
    if (handle <= 0 || (size_t)handle >= g_dbiResults.size())
        return nullptr;
    DbiResultData *r = &g_dbiResults[handle];
    if (r->stmt == nullptr)
        return nullptr;
    return r;
}

static int AllocResultHandle(sqlite3 *db, sqlite3_stmt *stmt)
{
    if (g_dbiResults.empty())
        g_dbiResults.push_back(DbiResultData());  // index 0 placeholder

    for (size_t i = 1; i < g_dbiResults.size(); i++) {
        DbiResultData &r = g_dbiResults[i];
        if (r.stmt == nullptr) {
            r.db = db;
            r.stmt = stmt;
            r.affected = 0;
            r.error.clear();
            return (int)i;
        }
    }

    g_dbiResults.push_back(DbiResultData());
    DbiResultData &nr = g_dbiResults.back();
    nr.db = db;
    nr.stmt = stmt;
    nr.affected = 0;
    nr.error.clear();
    return (int)(g_dbiResults.size() - 1);
}

static void FreeResultHandle(int handle)
{
    if (handle <= 0 || (size_t)handle >= g_dbiResults.size())
        return;

    DbiResultData &r = g_dbiResults[handle];
    if (r.stmt) {
        sqlite3_finalize(r.stmt);
        r.stmt = nullptr;
    }
    r.db = nullptr;
    r.affected = 0;
    r.error.clear();
}

// Frees all results of a connection (must be done before dbi_close, otherwise sqlite3_close fails)
static void FreeResultsForConn(sqlite3 *db)
{
    for (size_t i = 1; i < g_dbiResults.size(); i++) {
        DbiResultData &r = g_dbiResults[i];
        if (r.stmt && r.db == db) {
            sqlite3_finalize(r.stmt);
            r.stmt = nullptr;
            r.db = nullptr;
            r.affected = 0;
            r.error.clear();
        }
    }
}

// ===========================================
// Helper functions
// ===========================================

// Reads an AMX string parameter
static void AmxGetString(AMX *amx, cell param, char *out, size_t outLen)
{
    if (outLen == 0) {
        return;
    }
    out[0] = '\0';

    cell *addr = nullptr;
    if (amx_GetAddr(amx, param, &addr) == AMX_ERR_NONE && addr) {
        amx_GetString(out, addr, 0, outLen);
    }
}

// Writes an AMX string; if maxlength <= 0, writes the full string length
static void SetAmxStringSafe(AMX *amx, cell param, const char *src, int maxlength)
{
    cell *dest = nullptr;
    if (amx_GetAddr(amx, param, &dest) != AMX_ERR_NONE || !dest)
        return;

    if (!src)
        src = "";

    if (maxlength <= 0)
        maxlength = (int)strlen(src) + 1;

    amx_SetString(dest, src, 0, 0, (size_t)maxlength);
}

// Resolves the database path; relative paths are placed under addons/amxmodx/data/ in the game directory
static void ResolveDbPath(const char *db, char *out, size_t outLen)
{
    if (outLen == 0) {
        return;
    }
    out[0] = '\0';

    if (!db || !*db) {
        snprintf(out, outLen, ":memory:");
        return;
    }

    if (strchr(db, '/') || strchr(db, '\\') || strcmp(db, ":memory:") == 0) {
        strncpy(out, db, outLen - 1);
        out[outLen - 1] = '\0';
        return;
    }

    char gameDir[256] = {0};
    GET_GAME_DIR(gameDir);
    if (strrchr(db, '.')) {
        snprintf(out, outLen, "%s/addons/amxmodx/data/%s", gameDir, db);
    } else {
        snprintf(out, outLen, "%s/addons/amxmodx/data/%s.sq3", gameDir, db);
    }
}

// Formats a query string from varargs (dbi_query/dbi_query2/SQL_PrepareQuery any:... parameters)
// Outputs to the out buffer and returns the formatted length
size_t FormatAmxVarargs(AMX *amx, cell *params, int fixedParams, char *out, size_t outLen)
{
    if (!params || outLen == 0)
        return 0;

    char format[4096];
    AmxGetString(amx, params[fixedParams], format, sizeof(format));

    int numParams = (int)(params[0] / sizeof(cell));
    int extraParams = numParams - fixedParams;

    if (extraParams <= 0) {
        size_t n = strlen(format);
        if (n >= outLen) n = outLen - 1;
        memcpy(out, format, n);
        out[n] = '\0';
        return n;
    }

    size_t outPos = 0;
    size_t len = strlen(format);
    int varargIdx = 0;  // varargs start at params[fixedParams+1]

    for (size_t i = 0; i < len && outPos < outLen - 1; i++) {
        if (format[i] == '%' && i + 1 < len) {
            size_t start = i;
            i++;
            if (format[i] == '%') {
                out[outPos++] = '%';
                continue;
            }

            // Skip flags
            while (i < len && (format[i] == '-' || format[i] == '+' || format[i] == ' ' || format[i] == '#' || format[i] == '0'))
                i++;

            // Skip width
            if (i < len && format[i] == '*') {
                i++;
            } else {
                while (i < len && format[i] >= '0' && format[i] <= '9')
                    i++;
            }

            // Skip .precision
            if (i < len && format[i] == '.') {
                i++;
                if (i < len && format[i] == '*') {
                    i++;
                } else {
                    while (i < len && format[i] >= '0' && format[i] <= '9')
                        i++;
                }
            }

            // Skip length modifier
            if (i < len && (format[i] == 'h' || format[i] == 'l' || format[i] == 'L' || format[i] == 'I' || format[i] == 'q'))
                i++;

            if (i >= len) {
                out[outPos++] = '%';
                break;
            }

            char fmtType = format[i];
            char fmtSpec[64];
            size_t specLen = (i - start + 1);
            if (specLen >= sizeof(fmtSpec)) specLen = sizeof(fmtSpec) - 1;
            strncpy(fmtSpec, format + start, specLen);
            fmtSpec[specLen] = '\0';

            if (varargIdx >= extraParams)
                break;

            cell currentParam = params[fixedParams + 1 + varargIdx];
            varargIdx++;

            switch (fmtType) {
            case 's': {
                cell *strAddr = nullptr;
                char strBuf[2048];
                if (amx_GetAddr(amx, currentParam, &strAddr) == AMX_ERR_NONE && strAddr)
                    amx_GetString(strBuf, strAddr, 0, sizeof(strBuf));
                else
                    strBuf[0] = '\0';
                int n = snprintf(out + outPos, outLen - outPos, fmtSpec, strBuf);
                if (n > 0) outPos += (size_t)n;
                break;
            }
            case 'd':
            case 'i':
            case 'c': {
                int val = (fmtType == 'c') ? ((int)currentParam & 0xFF) : (int)currentParam;
                int n = snprintf(out + outPos, outLen - outPos, fmtSpec, val);
                if (n > 0) outPos += (size_t)n;
                break;
            }
            case 'f':
            case 'F':
            case 'e':
            case 'E':
            case 'g':
            case 'G':
            case 'a':
            case 'A': {
                int n = snprintf(out + outPos, outLen - outPos, fmtSpec, amx_ctof(currentParam));
                if (n > 0) outPos += (size_t)n;
                break;
            }
            case 'u':
            case 'o':
            case 'x':
            case 'X':
            case 'p': {
                int n = snprintf(out + outPos, outLen - outPos, fmtSpec, (unsigned int)currentParam);
                if (n > 0) outPos += (size_t)n;
                break;
            }
            default:
                if (outPos < outLen - 1) out[outPos++] = '%';
                if (outPos < outLen - 1) out[outPos++] = fmtType;
                break;
            }
        } else {
            out[outPos++] = format[i];
        }
    }
    if (outPos >= outLen) outPos = outLen - 1;
    out[outPos] = '\0';
    return outPos;
}

// Finds a column index by name (SQLite has no by-name lookup; iterates column names, case-sensitive)
static int FindColumnByName(sqlite3_stmt *stmt, const char *name)
{
    if (!stmt || !name)
        return -1;

    int numCols = sqlite3_column_count(stmt);
    for (int i = 0; i < numCols; i++) {
        const char *colName = sqlite3_column_name(stmt, i);
        if (colName && strcmp(colName, name) == 0)
            return i;
    }
    return -1;
}

// ============================================
// Native implementations
// ============================================

// dbi_connect(_host[], _user[], _pass[], _dbname[], _error[]="", _maxlength=0)
// SQLite: _dbname is used as the file path (relative paths resolve against the server directory); host/user/pass are ignored
// Returns a Sql handle (>0) on success, or SQL_FAILED (0) with the error written to _error on failure
cell AMX_NATIVE_CALL amxx_dbi_connect(AMX *amx, cell *params)
{
    int numParams = (int)(params[0] / sizeof(cell));

    char dbname[256];
    AmxGetString(amx, params[4], dbname, sizeof(dbname));

    char dbPath[512];
    ResolveDbPath(dbname, dbPath, sizeof(dbPath));

    sqlite3 *db = nullptr;
    int rc = sqlite3_open(dbPath, &db);
    if (rc != SQLITE_OK) {
        const char *errMsg = db ? sqlite3_errmsg(db) : "unknown error";
        if (numParams >= 5) {
            int maxlen = (numParams >= 6) ? (int)params[6] : 0;
            SetAmxStringSafe(amx, params[5], errMsg, maxlen);
        }
        if (db) {
            sqlite3_close(db);
            db = nullptr;
        }
        return SQL_FAILED;
    }

    // WAL mode (best-effort, improves concurrent read/write performance)
    sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

    if (numParams >= 5) {
        SetAmxStringSafe(amx, params[5], "", (numParams >= 6) ? (int)params[6] : 0);
    }

    return (cell)AllocConnHandle(db);
}

// dbi_query(Sql:_sql, _query[], any:...)
// On success with a result set: returns a result handle (>0, requires dbi_free_result)
// On success without a result set (DDL/DML): returns RESULT_NONE (0)
// On failure: returns RESULT_FAILED (-1); the error is stored on the connection (readable via dbi_error)
cell AMX_NATIVE_CALL amxx_dbi_query(AMX *amx, cell *params)
{
    sqlite3 *db = GetConn((int)params[1]);
    if (!db)
        return RESULT_FAILED;

    char query[8192];
    FormatAmxVarargs(amx, params, 2, query, sizeof(query));

    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
        return RESULT_FAILED;  // sqlite3_errmsg(db) already recorded the error

    // Statements without a result set (DDL/DML): execute and return RESULT_NONE
    if (sqlite3_column_count(stmt) == 0) {
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE)
            return RESULT_FAILED;
        return RESULT_NONE;
    }

    // Statements with a result set (SELECT): return a result handle consumed by dbi_nextrow/dbi_field
    return (cell)AllocResultHandle(db, stmt);
}

// dbi_query2(Sql:_sql, &rows, _query[], any:...)
// Same as dbi_query, but also returns the affected row count (sqlite3_changes) by reference; rows = -1 on failure
cell AMX_NATIVE_CALL amxx_dbi_query2(AMX *amx, cell *params)
{
    cell *rows_addr = nullptr;
    amx_GetAddr(amx, params[2], &rows_addr);

    sqlite3 *db = GetConn((int)params[1]);
    if (!db) {
        if (rows_addr) *rows_addr = -1;
        return RESULT_FAILED;
    }

    char query[8192];
    FormatAmxVarargs(amx, params, 3, query, sizeof(query));

    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        if (rows_addr) *rows_addr = -1;
        return RESULT_FAILED;
    }

    // Statements without a result set (DDL/DML): execute and return the affected row count
    if (sqlite3_column_count(stmt) == 0) {
        rc = sqlite3_step(stmt);
        int affected = (rc == SQLITE_DONE) ? sqlite3_changes(db) : -1;
        sqlite3_finalize(stmt);
        if (rows_addr) *rows_addr = (cell)affected;
        if (rc != SQLITE_DONE)
            return RESULT_FAILED;
        return RESULT_NONE;
    }

    // Statements with a result set (SELECT): affected rows are meaningless, set to 0
    if (rows_addr) *rows_addr = 0;

    return (cell)AllocResultHandle(db, stmt);
}

// Advances one row; returns 1 if a row is available, 0 on failure or end
cell AMX_NATIVE_CALL amxx_dbi_nextrow(AMX *amx, cell *params)
{
    (void)amx;
    DbiResultData *r = GetResult((int)params[1]);
    if (!r || !r->stmt)
        return 0;

    int rc = sqlite3_step(r->stmt);
    if (rc == SQLITE_ROW)
        return 1;

    if (rc != SQLITE_DONE && r->db)
        r->error = sqlite3_errmsg(r->db) ? sqlite3_errmsg(r->db) : "";

    return 0;
}

// dbi_field(Result:_result, _fieldnum, any:...) - reads a field by number (1-based)
// 2 params: returns int; 3 params: Float byref; 4 params: string buffer + len
cell AMX_NATIVE_CALL amxx_dbi_field(AMX *amx, cell *params)
{
    DbiResultData *r = GetResult((int)params[1]);
    if (!r || !r->stmt)
        return 0;

    int numParams = (int)(params[0] / sizeof(cell));

    int col = (int)params[2] - 1;  // 1-based -> 0-based
    if (col < 0 || col >= sqlite3_column_count(r->stmt))
        return 0;

    // No current row (dbi_nextrow not called yet or end of result set)
    if (sqlite3_data_count(r->stmt) == 0)
        return 0;

    if (numParams == 2) {
        return (cell)sqlite3_column_int(r->stmt, col);
    } else if (numParams == 3) {
        float val = (float)sqlite3_column_double(r->stmt, col);
        cell *addr = nullptr;
        if (amx_GetAddr(amx, params[3], &addr) != AMX_ERR_NONE || !addr)
            return 0;
        *addr = amx_ftoc(val);
        return 1;
    } else {
        const char *val = (const char *)sqlite3_column_text(r->stmt, col);
        if (!val) val = "";
        SetAmxStringSafe(amx, params[3], val, (int)params[4]);
        return (cell)strlen(val);
    }
}

// dbi_result(Result:_result, _field[], any:...) - reads a field by name
// 2 params: returns int; 3 params: Float byref; 4 params: string buffer + len
cell AMX_NATIVE_CALL amxx_dbi_result(AMX *amx, cell *params)
{
    DbiResultData *r = GetResult((int)params[1]);
    if (!r || !r->stmt)
        return 0;

    int numParams = (int)(params[0] / sizeof(cell));

    char fieldName[256];
    AmxGetString(amx, params[2], fieldName, sizeof(fieldName));

    // Look up the column by name
    int col = FindColumnByName(r->stmt, fieldName);
    if (col < 0)
        return 0;

    if (sqlite3_data_count(r->stmt) == 0)
        return 0;

    if (numParams == 2) {
        return (cell)sqlite3_column_int(r->stmt, col);
    } else if (numParams == 3) {
        float val = (float)sqlite3_column_double(r->stmt, col);
        cell *addr = nullptr;
        if (amx_GetAddr(amx, params[3], &addr) != AMX_ERR_NONE || !addr)
            return 0;
        *addr = amx_ftoc(val);
        return 1;
    } else {
        const char *val = (const char *)sqlite3_column_text(r->stmt, col);
        if (!val) val = "";
        SetAmxStringSafe(amx, params[3], val, (int)params[4]);
        return (cell)strlen(val);
    }
}

// dbi_num_rows(Result:_result) - returns the row count
// Resets the cursor, counts all rows, then rewinds so subsequent dbi_nextrow calls start from the first row
cell AMX_NATIVE_CALL amxx_dbi_num_rows(AMX *amx, cell *params)
{
    (void)amx;
    DbiResultData *r = GetResult((int)params[1]);
    if (!r || !r->stmt)
        return 0;

    sqlite3_reset(r->stmt);
    int count = 0;
    int rc;
    while ((rc = sqlite3_step(r->stmt)) == SQLITE_ROW)
        count++;

    if (rc != SQLITE_DONE && r->db)
        r->error = sqlite3_errmsg(r->db) ? sqlite3_errmsg(r->db) : "";

    // Rewind: reset the cursor so subsequent dbi_nextrow calls start from the first row
    sqlite3_reset(r->stmt);

    return (cell)count;
}

// Frees a result handle and sets it to 0
cell AMX_NATIVE_CALL amxx_dbi_free_result(AMX *amx, cell *params)
{
    cell *addr = nullptr;
    if (amx_GetAddr(amx, params[1], &addr) != AMX_ERR_NONE || !addr)
        return 0;

    cell id = *addr;
    if (id == 0)
        return 1;

    if (!GetResult((int)id))
        return 0;

    FreeResultHandle((int)id);
    *addr = 0;
    return 1;
}

// Closes a connection and sets the handle to 0
cell AMX_NATIVE_CALL amxx_dbi_close(AMX *amx, cell *params)
{
    cell *addr = nullptr;
    if (amx_GetAddr(amx, params[1], &addr) != AMX_ERR_NONE || !addr)
        return 0;

    int id = (int)*addr;
    sqlite3 *db = GetConn(id);
    if (!db)
        return 0;

    // Free all results of the connection first, otherwise sqlite3_close fails
    FreeResultsForConn(db);

    sqlite3_close(db);
    g_dbiConnections[id] = nullptr;
    *addr = 0;
    return 1;
}

// Writes the error message and returns the last error code
cell AMX_NATIVE_CALL amxx_dbi_error(AMX *amx, cell *params)
{
    sqlite3 *db = GetConn((int)params[1]);
    if (!db)
        return -1;

    const char *err = sqlite3_errmsg(db);
    if (!err) err = "";

    SetAmxStringSafe(amx, params[2], err, (int)params[3]);
    return (cell)sqlite3_errcode(db);
}

// Returns the database type "sqlite"
cell AMX_NATIVE_CALL amxx_dbi_type(AMX *amx, cell *params)
{
    SetAmxStringSafe(amx, params[1], "sqlite", (int)params[2]);
    return 6;
}

// Returns the field count
cell AMX_NATIVE_CALL amxx_dbi_num_fields(AMX *amx, cell *params)
{
    (void)amx;
    DbiResultData *r = GetResult((int)params[1]);
    if (!r || !r->stmt)
        return 0;
    return (cell)sqlite3_column_count(r->stmt);
}

// Returns the field name (1-based)
cell AMX_NATIVE_CALL amxx_dbi_field_name(AMX *amx, cell *params)
{
    DbiResultData *r = GetResult((int)params[1]);
    if (!r || !r->stmt)
        return 0;

    int col = (int)params[2] - 1;  // 1-based -> 0-based
    if (col < 0 || col >= sqlite3_column_count(r->stmt))
        return 0;

    const char *name = sqlite3_column_name(r->stmt, col);
    if (!name)
        return 0;

    SetAmxStringSafe(amx, params[3], name, (int)params[4]);
    return 1;
}

// ===== Native registration =====

AMX_NATIVE_INFO dbi_natives[] = {
    {"dbi_connect", amxx_dbi_connect},
    {"dbi_query", amxx_dbi_query},
    {"dbi_query2", amxx_dbi_query2},
    {"dbi_nextrow", amxx_dbi_nextrow},
    {"dbi_field", amxx_dbi_field},
    {"dbi_result", amxx_dbi_result},
    {"dbi_num_rows", amxx_dbi_num_rows},
    {"dbi_free_result", amxx_dbi_free_result},
    {"dbi_close", amxx_dbi_close},
    {"dbi_error", amxx_dbi_error},
    {"dbi_type", amxx_dbi_type},
    {"dbi_num_fields", amxx_dbi_num_fields},
    {"dbi_field_name", amxx_dbi_field_name},
    {nullptr, nullptr}
};

void RegisterDbiNatives(AMX *amx)
{
    amx_Register(amx, dbi_natives, -1);
}

// Cleans up DBI state (called on map change)
void ResetDbiGlobals()
{
    for (size_t i = 1; i < g_dbiResults.size(); i++) {
        DbiResultData &r = g_dbiResults[i];
        if (r.stmt) {
            sqlite3_finalize(r.stmt);
            r.stmt = nullptr;
        }
        r.db = nullptr;
        r.affected = 0;
        r.error.clear();
    }
    g_dbiResults.clear();

    for (size_t i = 1; i < g_dbiConnections.size(); i++) {
        if (g_dbiConnections[i]) {
            sqlite3_close(g_dbiConnections[i]);
            g_dbiConnections[i] = nullptr;
        }
    }
    g_dbiConnections.clear();
}
