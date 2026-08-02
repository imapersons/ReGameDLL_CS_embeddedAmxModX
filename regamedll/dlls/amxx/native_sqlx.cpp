#include "precompiled.h"
#include "native_sqlx.h"
#include "native_dbi.h"
#include "amx.h"
#include "sqlite3/sqlite3.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ===========================================
// SQLx Module - SQLite3 Implementation
// ===========================================

// Handle types
enum SqlHandleType {
    HT_TUPLE = 1,
    HT_CONNECTION,
    HT_QUERY
};

struct SqlHandle {
    int id;
    SqlHandleType type;

    // For TUPLE: database file path
    char dbPath[512];

    // For CONNECTION
    sqlite3 *db;
    char connectError[512];

    // For QUERY
    sqlite3_stmt *stmt;
    char query[8192];
    char error[512];
    int errorCode;
    int affectedRows;
    sqlite3_int64 insertId;
    int numCols;
    bool hasResults;
    bool executed;
    bool atEnd;
    bool ownsDb;  // True if this query handle owns its db (from SQL_ThreadQuery)
};

static std::vector<SqlHandle*> s_handles;
static int s_nextHandleId = 1;

// ===== Handle management =====

static SqlHandle* CreateSqlHandle(SqlHandleType type)
{
    SqlHandle *h = new SqlHandle();
    memset(h, 0, sizeof(*h));
    h->id = s_nextHandleId++;
    h->type = type;
    s_handles.push_back(h);
    return h;
}

static SqlHandle* GetSqlHandle(int id)
{
    if (id <= 0) return nullptr;
    for (size_t i = 0; i < s_handles.size(); i++) {
        if (s_handles[i] && s_handles[i]->id == id)
            return s_handles[i];
    }
    return nullptr;
}

static void FreeSqlHandleData(SqlHandle *h)
{
    if (!h) return;
    if (h->type == HT_QUERY && h->stmt) {
        sqlite3_finalize(h->stmt);
        h->stmt = nullptr;
    }
    // Close db if this handle owns it (from SQL_ThreadQuery) or is a connection
    if (h->ownsDb || h->type == HT_CONNECTION) {
        if (h->db) {
            sqlite3_close(h->db);
            h->db = nullptr;
        }
    }
    delete h;
}

static void FreeSqlHandle(int id)
{
    for (size_t i = 0; i < s_handles.size(); i++) {
        if (s_handles[i] && s_handles[i]->id == id) {
            FreeSqlHandleData(s_handles[i]);
            s_handles.erase(s_handles.begin() + i);
            return;
        }
    }
}

// Resolve database file path relative to game directory
static void ResolveDbPath(const char *db, char *out, size_t outLen)
{
    if (!db || !*db) {
        snprintf(out, outLen, ":memory:");
        return;
    }

    // If it already contains a path separator or is :memory:, use as-is
    if (strchr(db, '/') || strchr(db, '\\') || strcmp(db, ":memory:") == 0) {
        strncpy(out, db, outLen - 1);
        out[outLen - 1] = '\0';
        return;
    }

    // Prepend addons/amxmodx/data/ and append .sq3 if no extension
    char gameDir[256] = {0};
    GET_GAME_DIR(gameDir);
    const char *ext = strrchr(db, '.');
    if (ext) {
        snprintf(out, outLen, "%s/addons/amxmodx/data/%s", gameDir, db);
    } else {
        snprintf(out, outLen, "%s/addons/amxmodx/data/%s.sq3", gameDir, db);
    }
}

// Helper: read string from AMX parameter
static void AmxGetString(AMX *amx, cell param, char *out, size_t outLen)
{
    cell *addr;
    amx_GetAddr(amx, param, &addr);
    if (addr) {
        amx_GetString(out, addr, 0, (int)outLen);
    } else {
        out[0] = '\0';
    }
}

// ============================================
// SQLx_* Native Implementations
// ============================================

// SQLx_Connect(host[], user[], pass[], dbname[], port, maxRetry, retryDelay)
cell AMX_NATIVE_CALL amxx_sqlx_connect(AMX *amx, cell *params)
{
    char dbname[256];
    AmxGetString(amx, params[4], dbname, sizeof(dbname));

    char dbPath[512];
    ResolveDbPath(dbname, dbPath, sizeof(dbPath));

    SqlHandle *h = CreateSqlHandle(HT_CONNECTION);
    int rc = sqlite3_open(dbPath, &h->db);
    if (rc != SQLITE_OK) {
        strncpy(h->connectError, sqlite3_errmsg(h->db), sizeof(h->connectError) - 1);
        h->connectError[sizeof(h->connectError) - 1] = '\0';
        sqlite3_close(h->db);
        h->db = nullptr;
        return 0;
    }

    // Enable WAL mode for better concurrency
    sqlite3_exec(h->db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

    return (cell)h->id;
}

// SQLx_FreeHandle(handle)
cell AMX_NATIVE_CALL amxx_sqlx_free(AMX *amx, cell *params)
{
    (void)amx;
    FreeSqlHandle((int)params[1]);
    return 1;
}

// SQLx_Query(handle, query[], callback[], data)
// Executes a query and returns a query handle
cell AMX_NATIVE_CALL amxx_sqlx_query(AMX *amx, cell *params)
{
    SqlHandle *conn = GetSqlHandle((int)params[1]);
    if (!conn || conn->type != HT_CONNECTION || !conn->db) {
        return 0;
    }

    char query[8192];
    // 原版 SQL_Query/SQL_PrepareQuery 支持变长格式化参数 (query[] 后的 any:... 参数)，
    // 必须用 FormatAmxVarargs 完成 %s/%d 等替换，否则 %s 会原样进入 SQL
    FormatAmxVarargs(amx, params, 2, query, sizeof(query));

    SqlHandle *qh = CreateSqlHandle(HT_QUERY);
    qh->db = nullptr; // query handle doesn't own the db
    strncpy(qh->query, query, sizeof(qh->query) - 1);

    int rc = sqlite3_prepare_v2(conn->db, query, -1, &qh->stmt, nullptr);
    if (rc != SQLITE_OK) {
        strncpy(qh->error, sqlite3_errmsg(conn->db), sizeof(qh->error) - 1);
        qh->errorCode = rc;
        return (cell)qh->id; // return handle so plugin can check error
    }

    qh->numCols = sqlite3_column_count(qh->stmt);
    qh->executed = true;

    // Execute first step
    rc = sqlite3_step(qh->stmt);
    if (rc == SQLITE_ROW) {
        qh->hasResults = true;
        qh->atEnd = false;
    } else if (rc == SQLITE_DONE) {
        qh->hasResults = false;
        qh->atEnd = true;
    } else {
        strncpy(qh->error, sqlite3_errmsg(conn->db), sizeof(qh->error) - 1);
        qh->errorCode = rc;
        qh->hasResults = false;
        qh->atEnd = true;
    }

    qh->affectedRows = sqlite3_changes(conn->db);
    qh->insertId = sqlite3_last_insert_rowid(conn->db);

    return (cell)qh->id;
}

// SQLx_FetchResults(handle) — returns 1 if results available
cell AMX_NATIVE_CALL amxx_sqlx_fetch_results(AMX *amx, cell *params)
{
    (void)amx;
    SqlHandle *qh = GetSqlHandle((int)params[1]);
    if (!qh || qh->type != HT_QUERY) return 0;
    return qh->hasResults ? 1 : 0;
}

// SQLx_NextRow(handle)
cell AMX_NATIVE_CALL amxx_sqlx_next_row(AMX *amx, cell *params)
{
    (void)amx;
    SqlHandle *qh = GetSqlHandle((int)params[1]);
    if (!qh || qh->type != HT_QUERY || !qh->stmt) return 0;

    int rc = sqlite3_step(qh->stmt);
    if (rc == SQLITE_ROW) {
        qh->hasResults = true;
        qh->atEnd = false;
        return 1;
    } else {
        qh->hasResults = false;
        qh->atEnd = true;
        return 0;
    }
}

// SQLx_MoreResults(handle)
cell AMX_NATIVE_CALL amxx_sqlx_more_results(AMX *amx, cell *params)
{
    (void)amx;
    SqlHandle *qh = GetSqlHandle((int)params[1]);
    if (!qh || qh->type != HT_QUERY) return 0;
    return qh->hasResults ? 1 : 0;
}

// SQLx_AffectedRows(handle)
cell AMX_NATIVE_CALL amxx_sqlx_affected_rows(AMX *amx, cell *params)
{
    (void)amx;
    SqlHandle *qh = GetSqlHandle((int)params[1]);
    if (!qh || qh->type != HT_QUERY) return 0;
    return (cell)qh->affectedRows;
}

// SQLx_InsertId(handle)
cell AMX_NATIVE_CALL amxx_sqlx_insert_id(AMX *amx, cell *params)
{
    (void)amx;
    SqlHandle *qh = GetSqlHandle((int)params[1]);
    if (!qh || qh->type != HT_QUERY) return 0;
    return (cell)qh->insertId;
}

// SQLx_GetString(handle, fieldNum, buffer[], len)
cell AMX_NATIVE_CALL amxx_sqlx_get_string(AMX *amx, cell *params)
{
    SqlHandle *qh = GetSqlHandle((int)params[1]);
    if (!qh || qh->type != HT_QUERY || !qh->stmt || !qh->hasResults) {
        cell *dest;
        amx_GetAddr(amx, params[3], &dest);
        return amx_SetString(dest, "", 0, 0, (int)params[4]);
    }

    int col = (int)params[2];
    const char *val = (const char*)sqlite3_column_text(qh->stmt, col);
    if (!val) val = "";

    cell *dest;
    amx_GetAddr(amx, params[3], &dest);
    return amx_SetString(dest, val, 0, 0, (int)params[4]);
}

// SQLx_GetInt(handle, fieldNum)
cell AMX_NATIVE_CALL amxx_sqlx_get_int(AMX *amx, cell *params)
{
    (void)amx;
    SqlHandle *qh = GetSqlHandle((int)params[1]);
    if (!qh || qh->type != HT_QUERY || !qh->stmt || !qh->hasResults) return 0;
    return (cell)sqlite3_column_int(qh->stmt, (int)params[2]);
}

// SQLx_GetFloat(handle, fieldNum)
cell AMX_NATIVE_CALL amxx_sqlx_get_float(AMX *amx, cell *params)
{
    (void)amx;
    SqlHandle *qh = GetSqlHandle((int)params[1]);
    if (!qh || qh->type != HT_QUERY || !qh->stmt || !qh->hasResults) return 0;
    float val = (float)sqlite3_column_double(qh->stmt, (int)params[2]);
    return amx_ftoc(val);
}

// SQLx_GetFieldCount(handle)
cell AMX_NATIVE_CALL amxx_sqlx_get_field_count(AMX *amx, cell *params)
{
    (void)amx;
    SqlHandle *qh = GetSqlHandle((int)params[1]);
    if (!qh || qh->type != HT_QUERY || !qh->stmt) return 0;
    return (cell)qh->numCols;
}

// SQLx_GetFieldName(handle, fieldNum, buffer[], len)
cell AMX_NATIVE_CALL amxx_sqlx_get_field_name(AMX *amx, cell *params)
{
    SqlHandle *qh = GetSqlHandle((int)params[1]);
    if (!qh || qh->type != HT_QUERY || !qh->stmt) {
        cell *dest;
        amx_GetAddr(amx, params[3], &dest);
        return amx_SetString(dest, "", 0, 0, (int)params[4]);
    }

    const char *name = sqlite3_column_name(qh->stmt, (int)params[2]);
    if (!name) name = "";

    cell *dest;
    amx_GetAddr(amx, params[3], &dest);
    return amx_SetString(dest, name, 0, 0, (int)params[4]);
}

// SQLx_Error(handle, buffer[], len)
cell AMX_NATIVE_CALL amxx_sqlx_error(AMX *amx, cell *params)
{
    SqlHandle *h = GetSqlHandle((int)params[1]);
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    int maxlen = (int)params[3];

    const char *err = "Invalid handle";
    if (h) {
        if (h->type == HT_QUERY && h->error[0])
            err = h->error;
        else if (h->type == HT_CONNECTION && h->connectError[0])
            err = h->connectError;
        else
            err = "";
    }
    return amx_SetString(dest, err, 0, 0, maxlen);
}

// ============================================
// SQL_* Native Implementations (standard AMXX API)
// ============================================

// SQL_MakeDbTuple(host[], user[], pass[], db[], port[], err[], errlen)
cell AMX_NATIVE_CALL amxx_sql_make_db_tuple(AMX *amx, cell *params)
{
    char db[256];
    AmxGetString(amx, params[4], db, sizeof(db));

    char dbPath[512];
    ResolveDbPath(db, dbPath, sizeof(dbPath));

    SqlHandle *h = CreateSqlHandle(HT_TUPLE);
    strncpy(h->dbPath, dbPath, sizeof(h->dbPath) - 1);

    // Write empty error string
    cell *err_addr;
    amx_GetAddr(amx, params[6], &err_addr);
    if (err_addr && params[7] > 0) {
        amx_SetString(err_addr, "", 0, 0, (int)params[7]);
    }

    return (cell)h->id;
}

// SQL_Connect(tuple, &error, maxRetries, retryDelay)
// Returns connection handle on success, 0 on failure
cell AMX_NATIVE_CALL amxx_sql_connect(AMX *amx, cell *params)
{
    SqlHandle *tuple = GetSqlHandle((int)params[1]);
    if (!tuple || tuple->type != HT_TUPLE) {
        // Set error code if &error param exists
        if (params[0] >= 2) {
            cell *err_addr;
            amx_GetAddr(amx, params[2], &err_addr);
            if (err_addr) *err_addr = -1;
        }
        return 0;
    }

    SqlHandle *h = CreateSqlHandle(HT_CONNECTION);
    int rc = sqlite3_open(tuple->dbPath, &h->db);
    if (rc != SQLITE_OK) {
        strncpy(h->connectError, sqlite3_errmsg(h->db), sizeof(h->connectError) - 1);
        h->connectError[sizeof(h->connectError) - 1] = '\0';
        sqlite3_close(h->db);
        h->db = nullptr;
        if (params[0] >= 2) {
            cell *err_addr;
            amx_GetAddr(amx, params[2], &err_addr);
            if (err_addr) *err_addr = rc;
        }
        // Free the failed handle
        FreeSqlHandle(h->id);
        return 0;
    }

    // Enable WAL mode
    sqlite3_exec(h->db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

    // Set error code to 0 (success)
    if (params[0] >= 2) {
        cell *err_addr;
        amx_GetAddr(amx, params[2], &err_addr);
        if (err_addr) *err_addr = 0;
    }

    return (cell)h->id;
}

// SQL_PrepareQuery(handle, query[])
// Returns query handle
cell AMX_NATIVE_CALL amxx_sql_prepare_query(AMX *amx, cell *params)
{
    SqlHandle *conn = GetSqlHandle((int)params[1]);
    if (!conn || conn->type != HT_CONNECTION || !conn->db) {
        return 0;
    }

    char query[8192];
    // 原版 SQL_Query/SQL_PrepareQuery 支持变长格式化参数 (query[] 后的 any:... 参数)，
    // 必须用 FormatAmxVarargs 完成 %s/%d 等替换，否则 %s 会原样进入 SQL
    FormatAmxVarargs(amx, params, 2, query, sizeof(query));

    SqlHandle *qh = CreateSqlHandle(HT_QUERY);
    strncpy(qh->query, query, sizeof(qh->query) - 1);

    int rc = sqlite3_prepare_v2(conn->db, query, -1, &qh->stmt, nullptr);
    if (rc != SQLITE_OK) {
        strncpy(qh->error, sqlite3_errmsg(conn->db), sizeof(qh->error) - 1);
        qh->errorCode = rc;
    } else {
        qh->numCols = sqlite3_column_count(qh->stmt);
    }

    return (cell)qh->id;
}

// SQL_Execute(query) — returns 1 on success, 0 on failure
cell AMX_NATIVE_CALL amxx_sql_execute(AMX *amx, cell *params)
{
    (void)amx;
    SqlHandle *qh = GetSqlHandle((int)params[1]);
    if (!qh || qh->type != HT_QUERY || !qh->stmt) return 0;

    int rc = sqlite3_step(qh->stmt);
    qh->executed = true;

    if (rc == SQLITE_ROW) {
        qh->hasResults = true;
        qh->atEnd = false;
    } else if (rc == SQLITE_DONE) {
        qh->hasResults = false;
        qh->atEnd = true;
    } else {
        strncpy(qh->error, sqlite3_errmsg(sqlite3_db_handle(qh->stmt)), sizeof(qh->error) - 1);
        qh->errorCode = rc;
        qh->hasResults = false;
        qh->atEnd = true;
        return 0;
    }

    // Get affected rows and insert ID
    sqlite3 *db = sqlite3_db_handle(qh->stmt);
    qh->affectedRows = sqlite3_changes(db);
    qh->insertId = sqlite3_last_insert_rowid(db);

    return 1;
}

// SQL_NumResults(query) — returns number of result rows (0 if no results)
cell AMX_NATIVE_CALL amxx_sql_num_results(AMX *amx, cell *params)
{
    (void)amx;
    SqlHandle *qh = GetSqlHandle((int)params[1]);
    if (!qh || qh->type != HT_QUERY || !qh->hasResults) return 0;
    // SQLite doesn't pre-count rows; return 1 if we have a current row
    return 1;
}

// SQL_FreeHandle(handle) — frees any type of handle
cell AMX_NATIVE_CALL amxx_sql_free_handle(AMX *amx, cell *params)
{
    (void)amx;
    FreeSqlHandle((int)params[1]);
    return 1;
}

// SQL_QueryAndIgnore(handle, query[], ...) — 执行查询并忽略结果/错误（DDL 常用）
cell AMX_NATIVE_CALL amxx_sql_query_and_ignore(AMX *amx, cell *params)
{
    SqlHandle *conn = GetSqlHandle((int)params[1]);
    if (!conn || conn->type != HT_CONNECTION || !conn->db)
        return 0;

    char query[8192];
    FormatAmxVarargs(amx, params, 2, query, sizeof(query));

    char *errmsg = nullptr;
    int rc = sqlite3_exec(conn->db, query, nullptr, nullptr, &errmsg);
    if (errmsg)
        sqlite3_free(errmsg);
    return (rc == SQLITE_OK) ? 1 : 0;
}

// sqlite_TableExists(handle, table[]) — 检查表是否存在（sqlitex.inc）
cell AMX_NATIVE_CALL amxx_sqlite_table_exists(AMX *amx, cell *params)
{
    SqlHandle *conn = GetSqlHandle((int)params[1]);
    if (!conn || conn->type != HT_CONNECTION || !conn->db)
        return 0;

    char table[64];
    AmxGetString(amx, params[2], table, sizeof(table));
    if (table[0] == 0)
        return 0;

    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT name FROM sqlite_master WHERE type='table' AND name='%s'", table);
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(conn->db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return 0;
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_ROW) ? 1 : 0;
}

// SQL_AffectedRows(query)
cell AMX_NATIVE_CALL amxx_sql_affected_rows(AMX *amx, cell *params)
{
    (void)amx;
    SqlHandle *qh = GetSqlHandle((int)params[1]);
    if (!qh || qh->type != HT_QUERY) return 0;
    return (cell)qh->affectedRows;
}

// SQL_QueryError(query, buffer[], len)
cell AMX_NATIVE_CALL amxx_sql_query_error(AMX *amx, cell *params)
{
    SqlHandle *qh = GetSqlHandle((int)params[1]);
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    int maxlen = (int)params[3];

    const char *err = "";
    if (qh && qh->type == HT_QUERY && qh->error[0])
        err = qh->error;

    return amx_SetString(dest, err, 0, 0, maxlen);
}

// SQL_FieldNameToNum(query, name[]) — returns column index or -1
cell AMX_NATIVE_CALL amxx_sql_field_name_to_num(AMX *amx, cell *params)
{
    SqlHandle *qh = GetSqlHandle((int)params[1]);
    if (!qh || qh->type != HT_QUERY || !qh->stmt) return -1;

    char name[256];
    AmxGetString(amx, params[2], name, sizeof(name));

    for (int i = 0; i < qh->numCols; i++) {
        const char *colName = sqlite3_column_name(qh->stmt, i);
        if (colName && stricmp(colName, name) == 0)
            return i;
    }
    return -1;
}

// SQL_MoreResults(query) — returns 1 if there are more results
cell AMX_NATIVE_CALL amxx_sql_more_results(AMX *amx, cell *params)
{
    (void)amx;
    SqlHandle *qh = GetSqlHandle((int)params[1]);
    if (!qh || qh->type != HT_QUERY) return 0;
    return qh->hasResults ? 1 : 0;
}

// SQL_ReadResult(query, column, {Float,_}:...)
// 2 params: return integer
// 3 params: return float by reference
// 4 params: return string (buffer, len)
cell AMX_NATIVE_CALL amxx_sql_read_result(AMX *amx, cell *params)
{
    SqlHandle *qh = GetSqlHandle((int)params[1]);
    if (!qh || qh->type != HT_QUERY || !qh->stmt || !qh->hasResults)
        return 0;

    int col = (int)params[2];
    int numParams = (int)params[0];

    if (numParams == 2) {
        // Return integer value
        return (cell)sqlite3_column_int(qh->stmt, col);
    } else if (numParams == 3) {
        // Return float by reference
        float val = (float)sqlite3_column_double(qh->stmt, col);
        cell *addr;
        amx_GetAddr(amx, params[3], &addr);
        if (addr) *addr = amx_ftoc(val);
        return 1;
    } else if (numParams >= 4) {
        // Return string
        const char *val = (const char*)sqlite3_column_text(qh->stmt, col);
        if (!val) val = "";
        cell *dest;
        amx_GetAddr(amx, params[3], &dest);
        return amx_SetString(dest, val, 0, 0, (int)params[4]);
    }

    return 0;
}

// SQL_NextRow(query) — returns 1 on success, 0 if no more rows
cell AMX_NATIVE_CALL amxx_sql_next_row(AMX *amx, cell *params)
{
    (void)amx;
    SqlHandle *qh = GetSqlHandle((int)params[1]);
    if (!qh || qh->type != HT_QUERY || !qh->stmt) return 0;

    int rc = sqlite3_step(qh->stmt);
    if (rc == SQLITE_ROW) {
        qh->hasResults = true;
        qh->atEnd = false;
        return 1;
    } else {
        qh->hasResults = false;
        qh->atEnd = true;
        return 0;
    }
}

// SQL_NumColumns(query)
cell AMX_NATIVE_CALL amxx_sql_num_columns(AMX *amx, cell *params)
{
    (void)amx;
    SqlHandle *qh = GetSqlHandle((int)params[1]);
    if (!qh || qh->type != HT_QUERY || !qh->stmt) return 0;
    return (cell)qh->numCols;
}

// SQL_GetInsertId(query)
cell AMX_NATIVE_CALL amxx_sql_get_insert_id(AMX *amx, cell *params)
{
    (void)amx;
    SqlHandle *qh = GetSqlHandle((int)params[1]);
    if (!qh || qh->type != HT_QUERY) return 0;
    return (cell)qh->insertId;
}

// SQL_FieldNumToName(query, column, buffer[], len)
cell AMX_NATIVE_CALL amxx_sql_field_num_to_name(AMX *amx, cell *params)
{
    SqlHandle *qh = GetSqlHandle((int)params[1]);
    if (!qh || qh->type != HT_QUERY || !qh->stmt) {
        cell *dest;
        amx_GetAddr(amx, params[3], &dest);
        return amx_SetString(dest, "", 0, 0, (int)params[4]);
    }

    const char *name = sqlite3_column_name(qh->stmt, (int)params[2]);
    if (!name) name = "";

    cell *dest;
    amx_GetAddr(amx, params[3], &dest);
    return amx_SetString(dest, name, 0, 0, (int)params[4]);
}

// SQL_GetAffinity(output[], len) — 把当前驱动名 "sqlite" 写入输出缓冲并返回 1
// 原版签名: native SQL_GetAffinity(affinity[], len)
cell AMX_NATIVE_CALL amxx_sql_get_affinity(AMX *amx, cell *params)
{
    cell *dest = nullptr;
    if (amx_GetAddr(amx, params[1], &dest) == AMX_ERR_NONE && dest) {
        int maxLen = (int)params[2];
        if (maxLen <= 0) maxLen = 32;
        amx_SetString(dest, "sqlite", 0, 0, maxLen);
    }
    // We only support SQLite
    return 1;
}

// SQL_SetAffinity(affinity[]) — returns 1 if affinity is "sqlite", 0 otherwise
cell AMX_NATIVE_CALL amxx_sql_set_affinity(AMX *amx, cell *params)
{
    (void)amx;
    char affinity[32];
    AmxGetString(amx, params[1], affinity, sizeof(affinity));
    // We only support SQLite
    if (stricmp(affinity, "sqlite") == 0 || affinity[0] == '\0')
        return 1;
    return 0;
}

// SQL_ThreadQuery(Handle:dbTuple, const callback[], const query[], const data[]="", dataSize=0)
// Synchronous implementation: execute query immediately and call callback
cell AMX_NATIVE_CALL amxx_sql_thread_query(AMX *amx, cell *params)
{
    SqlHandle *tuple = GetSqlHandle((int)params[1]);
    if (!tuple || tuple->type != HT_TUPLE) {
        AMXX_LOG("[SQL] SQL_ThreadQuery: invalid tuple handle");
        return 0;
    }

    char callback[64];
    AmxGetString(amx, params[2], callback, sizeof(callback));

    char query[8192];
    AmxGetString(amx, params[3], query, sizeof(query));

    char data[256];
    if (params[0] >= 5) {
        AmxGetString(amx, params[4], data, sizeof(data));
    } else {
        data[0] = '\0';
    }

    int dataSize = (params[0] >= 5) ? (int)params[5] : 0;

    // Open a temporary connection
    sqlite3 *db = nullptr;
    int rc = sqlite3_open(tuple->dbPath, &db);
    if (rc != SQLITE_OK) {
        AMXX_LOG("[SQL] SQL_ThreadQuery: failed to open database: %s", sqlite3_errmsg(db));
        if (db) sqlite3_close(db);
        return 0;
    }

    // Execute the query
    SqlHandle *qh = CreateSqlHandle(HT_QUERY);
    qh->db = db;        // Query handle owns the db so it stays alive
    qh->ownsDb = true;
    strncpy(qh->query, query, sizeof(qh->query) - 1);

    rc = sqlite3_prepare_v2(db, query, -1, &qh->stmt, nullptr);
    if (rc != SQLITE_OK) {
        strncpy(qh->error, sqlite3_errmsg(db), sizeof(qh->error) - 1);
        qh->errorCode = rc;
    } else {
        qh->numCols = sqlite3_column_count(qh->stmt);
        rc = sqlite3_step(qh->stmt);
        if (rc == SQLITE_ROW) {
            qh->hasResults = true;
        } else if (rc == SQLITE_DONE) {
            qh->hasResults = false;
        } else {
            strncpy(qh->error, sqlite3_errmsg(db), sizeof(qh->error) - 1);
            qh->errorCode = rc;
        }
        qh->affectedRows = sqlite3_changes(db);
        qh->insertId = sqlite3_last_insert_rowid(db);
    }

    // Call the PAWN callback synchronously
    int failstate = qh->error[0] ? 1 : 0; // TQUERY_CONNECT_FAILED = 1, TQUERY_SUCCESS = 0
    if (qh->error[0]) failstate = 2; // TQUERY_QUERY_FAILED = 2

    int cbIndex;
    if (amx_FindPublic(amx, callback, &cbIndex) == AMX_ERR_NONE) {
        // Push parameters: failstate, query_handle, error[], data[], datasize, queuetime
        // Parameters are pushed in reverse order
        cell amx_addr_data = 0, amx_addr_err = 0;
        cell *phys_addr_data = nullptr, *phys_addr_err = nullptr;

        // Push queuetime (0.0 - synchronous, no queue delay)
        amx_Push(amx, 0);

        // Push datasize
        amx_Push(amx, dataSize);

        // Push data string
        if (data[0]) {
            amx_PushString(amx, &amx_addr_data, &phys_addr_data, data, 0, 0);
        } else {
            amx_PushString(amx, &amx_addr_data, &phys_addr_data, "", 0, 0);
        }

        // Push error string
        amx_PushString(amx, &amx_addr_err, &phys_addr_err, qh->error, 0, 0);

        // Push query handle
        amx_Push(amx, (cell)qh->id);

        // Push failstate
        amx_Push(amx, (cell)failstate);

        // Execute
        cell ret = 0;
        amx_Exec(amx, &ret, cbIndex);

        // Release allocated strings
        if (amx_addr_err) amx_Release(amx, amx_addr_err);
        if (amx_addr_data) amx_Release(amx, amx_addr_data);
    }

    // db is owned by qh now - it will be closed when SQL_FreeHandle is called
    return 1;
}

// SQL_QuoteString(Handle:dbTuple, error[], errLen, const query[])
// Escapes a string for safe SQL insertion. Returns escaped string length.
cell AMX_NATIVE_CALL amxx_sql_quote_string(AMX *amx, cell *params)
{
    (void)amx;

    cell *err_addr;
    amx_GetAddr(amx, params[2], &err_addr);
    if (err_addr && params[3] > 0) {
        amx_SetString(err_addr, "", 0, 0, (int)params[3]);
    }

    cell *query_addr;
    amx_GetAddr(amx, params[4], &query_addr);
    char input[4096];
    amx_GetString(input, query_addr, 0, sizeof(input));

    // Use sqlite3_mprintf for proper escaping
    char *escaped = sqlite3_mprintf("%q", input);
    if (!escaped) {
        if (err_addr && params[3] > 0) {
            amx_SetString(err_addr, "Out of memory", 0, 0, (int)params[3]);
        }
        return -1;
    }

    int escapedLen = (int)strlen(escaped);

    // Write back to the original buffer
    // The buffer size is the original string length + 1 (PAWN arrays are fixed-size)
    // We need to be careful not to overflow
    int bufMaxLen = (int)strlen(input) + 1;
    // Actually, the buffer size should be passed as a parameter, but in the current
    // AMXX API it's not. We'll write back with the original buffer size.
    // For safety, use a larger write-back size
    amx_SetString(query_addr, escaped, 0, 0, bufMaxLen > escapedLen ? bufMaxLen : escapedLen + 1);

    sqlite3_free(escaped);
    return (cell)escapedLen;
}

// SQL_GetQueryString(Handle:query, buffer[], len)
cell AMX_NATIVE_CALL amxx_sql_get_query_string(AMX *amx, cell *params)
{
    SqlHandle *qh = GetSqlHandle((int)params[1]);
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    int maxlen = (int)params[3];

    const char *q = "";
    if (qh && qh->type == HT_QUERY)
        q = qh->query;

    return amx_SetString(dest, q, 0, 0, maxlen);
}

// SQL_SetCharset(Handle:h, const charset[])
// SQLite: PRAGMA encoding 不支持运行时修改 (只能在建表前设置), 此处为兼容 no-op.
// 始终返回 1 (true) 以匹配 sqlx.inc 文档 "This native does nothing in SQLite."
cell AMX_NATIVE_CALL amxx_sql_set_charset(AMX *amx, cell *params)
{
    SqlHandle *h = GetSqlHandle((int)params[1]);
    if (!h) return 0;

    char charset[64];
    AmxGetString(amx, params[2], charset, sizeof(charset));

    // 仅对连接句柄尝试设置编码 (best-effort, 忽略错误)
    if (h->type == HT_CONNECTION && h->db) {
        char pragma[128];
        snprintf(pragma, sizeof(pragma), "PRAGMA encoding = \"%s\";", charset);
        sqlite3_exec(h->db, pragma, nullptr, nullptr, nullptr);
    }
    // TUPLE 句柄: 简化处理, 记录将由后续连接生效 (此处不持久化, 维持原行为)
    return 1;
}

// SQL_QuoteStringFmt(Handle:db, buffer[], buflen, const fmt[], any:...)
// 先格式化字符串, 再调用 sqlite3_mprintf("%q", ...) 转义, 写入 buffer.
// 返回转义后字符串长度, 失败返回 -1.
cell AMX_NATIVE_CALL amxx_sql_quote_string_fmt(AMX *amx, cell *params)
{
    // params[1] = db handle (SQLite 转义不依赖连接, 仅校验存在性, Empty_Handle=0 也允许)
    // params[2] = buffer
    // params[3] = buflen
    // params[4] = fmt
    // params[5]+ = varargs
    cell *fmt_addr;
    amx_GetAddr(amx, params[4], &fmt_addr);
    if (!fmt_addr) return -1;

    char fmt[4096];
    amx_GetString(fmt, fmt_addr, 0, sizeof(fmt));

    int numParams = (int)(params[0] / sizeof(cell));
    int extraParams = numParams - 4;

    // 格式化字符串 (复用 native_core.cpp formatex 的简单实现思路)
    char formatted[4096];
    if (extraParams <= 0) {
        strncpy(formatted, fmt, sizeof(formatted) - 1);
        formatted[sizeof(formatted) - 1] = '\0';
    } else {
        size_t outPos = 0;
        size_t fmtLen = strlen(fmt);
        int varargIdx = 0;
        for (size_t i = 0; i < fmtLen && outPos < sizeof(formatted) - 1; i++) {
            if (fmt[i] == '%' && i + 1 < fmtLen) {
                size_t start = i;
                i++;
                if (fmt[i] == '%') {
                    formatted[outPos++] = '%';
                    continue;
                }
                // 跳过 flags / width / .precision / length modifier
                while (i < fmtLen && (fmt[i] == '-' || fmt[i] == '+' || fmt[i] == ' ' || fmt[i] == '#' || fmt[i] == '0'))
                    i++;
                if (i < fmtLen && fmt[i] == '*') i++;
                else while (i < fmtLen && fmt[i] >= '0' && fmt[i] <= '9') i++;
                if (i < fmtLen && fmt[i] == '.') {
                    i++;
                    if (i < fmtLen && fmt[i] == '*') i++;
                    else while (i < fmtLen && fmt[i] >= '0' && fmt[i] <= '9') i++;
                }
                if (i < fmtLen && (fmt[i] == 'h' || fmt[i] == 'l' || fmt[i] == 'L' || fmt[i] == 'I' || fmt[i] == 'q'))
                    i++;

                if (i >= fmtLen) {
                    formatted[outPos++] = '%';
                    break;
                }

                char fmtType = fmt[i];
                char fmtSpec[64];
                size_t specLen = (i - start + 1);
                if (specLen >= sizeof(fmtSpec)) specLen = sizeof(fmtSpec) - 1;
                strncpy(fmtSpec, fmt + start, specLen);
                fmtSpec[specLen] = '\0';

                if (varargIdx >= extraParams) break;

                cell currentParam = params[5 + varargIdx];
                varargIdx++;

                switch (fmtType) {
                case 's': {
                    cell *strAddr;
                    amx_GetAddr(amx, currentParam, &strAddr);
                    char strBuf[2048];
                    if (strAddr) amx_GetString(strBuf, strAddr, 0, sizeof(strBuf));
                    else strBuf[0] = '\0';
                    int n = snprintf(formatted + outPos, sizeof(formatted) - outPos, fmtSpec, strBuf);
                    if (n > 0) outPos += (size_t)n;
                    break;
                }
                case 'f':
                case 'g':
                case 'e': {
                    float v = amx_ctof(currentParam);
                    int n = snprintf(formatted + outPos, sizeof(formatted) - outPos, fmtSpec, (double)v);
                    if (n > 0) outPos += (size_t)n;
                    break;
                }
                case 'd':
                case 'i':
                case 'x':
                case 'X':
                case 'o':
                case 'c':
                case 'u':
                default: {
                    int n = snprintf(formatted + outPos, sizeof(formatted) - outPos, fmtSpec, (int)currentParam);
                    if (n > 0) outPos += (size_t)n;
                    break;
                }
                }
            } else {
                formatted[outPos++] = fmt[i];
            }
        }
        formatted[outPos] = '\0';
    }

    // 转义
    char *escaped = sqlite3_mprintf("%q", formatted);
    if (!escaped) return -1;

    int escapedLen = (int)strlen(escaped);
    int maxlen = (int)params[3];
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    if (dest && maxlen > 0) {
        amx_SetString(dest, escaped, 0, 0, maxlen);
    }

    sqlite3_free(escaped);
    return (cell)escapedLen;
}

// SQL_IsNull(Handle:query, column)
// 返回 1 若指定列为 NULL, 否则 0.
cell AMX_NATIVE_CALL amxx_sql_is_null(AMX *amx, cell *params)
{
    (void)amx;
    SqlHandle *qh = GetSqlHandle((int)params[1]);
    if (!qh || qh->type != HT_QUERY || !qh->stmt || !qh->hasResults) return 0;
    int col = (int)params[2];
    if (col < 0 || col >= qh->numCols) return 0;
    return (sqlite3_column_type(qh->stmt, col) == SQLITE_NULL) ? 1 : 0;
}

// SQL_Rewind(Handle:query)
// 重置结果集到第一行. 返回 1 成功, 0 失败.
cell AMX_NATIVE_CALL amxx_sql_rewind(AMX *amx, cell *params)
{
    (void)amx;
    SqlHandle *qh = GetSqlHandle((int)params[1]);
    if (!qh || qh->type != HT_QUERY || !qh->stmt) return 0;

    int rc = sqlite3_reset(qh->stmt);
    if (rc != SQLITE_OK) {
        return 0;
    }

    // 重新 step 到第一行
    rc = sqlite3_step(qh->stmt);
    if (rc == SQLITE_ROW) {
        qh->hasResults = true;
        qh->atEnd = false;
        return 1;
    } else {
        // 没有 row 或出错
        qh->hasResults = false;
        qh->atEnd = true;
        return (rc == SQLITE_DONE) ? 1 : 0;
    }
}

// SQL_NextResultSet(Handle:query)
// SQLite 不支持多结果集, 始终返回 0 (false).
// 同时按 sqlx.inc 语义清除当前结果集.
cell AMX_NATIVE_CALL amxx_sql_next_result_set(AMX *amx, cell *params)
{
    (void)amx;
    SqlHandle *qh = GetSqlHandle((int)params[1]);
    if (!qh || qh->type != HT_QUERY) return 0;
    // 销毁当前结果集状态
    qh->hasResults = false;
    qh->atEnd = true;
    return 0;
}

// ===== Native registration =====

AMX_NATIVE_INFO sqlx_natives[] = {
    {"SQLx_Connect", amxx_sqlx_connect},
    {"SQLx_FreeHandle", amxx_sqlx_free},
    {"SQLx_Query", amxx_sqlx_query},
    {"SQLx_FetchResults", amxx_sqlx_fetch_results},
    {"SQLx_NextRow", amxx_sqlx_next_row},
    {"SQLx_MoreResults", amxx_sqlx_more_results},
    {"SQLx_AffectedRows", amxx_sqlx_affected_rows},
    {"SQLx_InsertId", amxx_sqlx_insert_id},
    {"SQLx_GetString", amxx_sqlx_get_string},
    {"SQLx_GetInt", amxx_sqlx_get_int},
    {"SQLx_GetFloat", amxx_sqlx_get_float},
    {"SQLx_GetFieldCount", amxx_sqlx_get_field_count},
    {"SQLx_GetFieldName", amxx_sqlx_get_field_name},
    {"SQLx_Error", amxx_sqlx_error},

    // 标准 SQL API (与 amxmodx/include/sqlx.inc 一致)
    {"SQL_MakeDbTuple", amxx_sql_make_db_tuple},
    {"SQL_Connect", amxx_sql_connect},
    {"SQL_PrepareQuery", amxx_sql_prepare_query},
    {"SQL_Execute", amxx_sql_execute},
    {"SQL_NumResults", amxx_sql_num_results},
    {"SQL_FreeHandle", amxx_sql_free_handle},
    {"SQL_QueryAndIgnore", amxx_sql_query_and_ignore},
    {"sqlite_TableExists", amxx_sqlite_table_exists},
    {"SQL_AffectedRows", amxx_sql_affected_rows},
    {"SQL_QueryError", amxx_sql_query_error},
    {"SQL_FieldNameToNum", amxx_sql_field_name_to_num},
    {"SQL_MoreResults", amxx_sql_more_results},
    {"SQL_ReadResult", amxx_sql_read_result},
    {"SQL_NextRow", amxx_sql_next_row},
    {"SQL_NumColumns", amxx_sql_num_columns},
    {"SQL_GetInsertId", amxx_sql_get_insert_id},
    {"SQL_FieldNumToName", amxx_sql_field_num_to_name},
    {"SQL_GetAffinity", amxx_sql_get_affinity},
    {"SQL_SetAffinity", amxx_sql_set_affinity},
    {"SQL_ThreadQuery", amxx_sql_thread_query},
    {"SQL_QuoteString", amxx_sql_quote_string},
    {"SQL_GetQueryString", amxx_sql_get_query_string},
    {"SQL_SetCharset", amxx_sql_set_charset},
    {"SQL_QuoteStringFmt", amxx_sql_quote_string_fmt},
    {"SQL_IsNull", amxx_sql_is_null},
    {"SQL_Rewind", amxx_sql_rewind},
    {"SQL_NextResultSet", amxx_sql_next_result_set},
    {nullptr, nullptr}
};

void RegisterSQLxNatives(AMX *amx)
{
    amx_Register(amx, sqlx_natives, -1);
}

// 清理 SQLx 状态 (地图切换时调用)
void ResetSqlxGlobals()
{
    for (size_t i = 0; i < s_handles.size(); i++) {
        FreeSqlHandleData(s_handles[i]);
    }
    s_handles.clear();
    s_nextHandleId = 1;
}
