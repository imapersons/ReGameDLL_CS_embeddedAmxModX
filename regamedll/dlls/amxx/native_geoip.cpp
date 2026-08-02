// ===========================================
// GeoIP Module - 对齐原版 AMX Mod X geoip.inc (15 个 native)
// 嵌入版简化实现: 内置极简国家表 (IP 首段 -> 国家),
// 城市/区域/时区等需要 GeoIP City 数据库的接口返回 0 占位。
// ===========================================

#include "precompiled.h"
#include "native_geoip.h"
#include "amx.h"

#include <cstdio>
#include <cstring>
#include <cmath>

// ===========================================
// 常量 (对齐 geoip.inc)
// ===========================================
#define SYSTEM_METRIC   0 // kilometers
#define SYSTEM_IMPERIAL 1 // statute miles

// Continent enum (对齐 geoip.inc)
#define CONTINENT_UNKNOWN       0
#define CONTINENT_AFRICA        1
#define CONTINENT_ANTARCTICA    2
#define CONTINENT_ASIA          3
#define CONTINENT_EUROPE        4
#define CONTINENT_NORTH_AMERICA 5
#define CONTINENT_OCEANIA       6
#define CONTINENT_SOUTH_AMERICA 7

struct GeoCountry {
    const char *code2;
    const char *code3;
    const char *name;
    const char *continent;  // 2 字符大洲码
    float lat;
    float lon;
    const char *timezone;   // 预留 (原版需要 City 库, 本简化实现不使用)
};

// ===========================================
// 内置国家表 (61 国)
// ===========================================
static const GeoCountry g_countries[] = {
    { "CN", "CHN", "China",                 "AS", 35.86f,  104.20f, "Asia/Shanghai" },            // 0
    { "US", "USA", "United States",         "NA", 39.83f,  -98.58f, "America/Chicago" },          // 1
    { "JP", "JPN", "Japan",                 "AS", 36.20f,  138.25f, "Asia/Tokyo" },               // 2
    { "KR", "KOR", "South Korea",           "AS", 35.91f,  127.77f, "Asia/Seoul" },               // 3
    { "RU", "RUS", "Russia",                "EU", 61.52f,  105.32f, "Europe/Moscow" },            // 4
    { "DE", "DEU", "Germany",               "EU", 51.16f,  10.45f,  "Europe/Berlin" },            // 5
    { "FR", "FRA", "France",                "EU", 46.23f,  2.21f,   "Europe/Paris" },             // 6
    { "GB", "GBR", "United Kingdom",        "EU", 55.38f,  -3.44f,  "Europe/London" },            // 7
    { "CA", "CAN", "Canada",                "NA", 56.13f,  -106.35f,"America/Toronto" },          // 8
    { "AU", "AUS", "Australia",             "OC", -25.27f, 133.78f, "Australia/Sydney" },         // 9
    { "BR", "BRA", "Brazil",                "SA", -14.24f, -51.93f, "America/Sao_Paulo" },        // 10
    { "ZA", "ZAF", "South Africa",          "AF", -30.56f, 22.94f,  "Africa/Johannesburg" },      // 11
    { "IN", "IND", "India",                 "AS", 20.59f,  78.96f,  "Asia/Kolkata" },             // 12
    { "IT", "ITA", "Italy",                 "EU", 41.87f,  12.57f,  "Europe/Rome" },              // 13
    { "ES", "ESP", "Spain",                 "EU", 40.46f,  -3.75f,  "Europe/Madrid" },            // 14
    { "NL", "NLD", "Netherlands",           "EU", 52.13f,  5.29f,   "Europe/Amsterdam" },         // 15
    { "SE", "SWE", "Sweden",                "EU", 60.13f,  18.64f,  "Europe/Stockholm" },         // 16
    { "PL", "POL", "Poland",                "EU", 51.92f,  19.15f,  "Europe/Warsaw" },            // 17
    { "UA", "UKR", "Ukraine",               "EU", 48.38f,  31.17f,  "Europe/Kyiv" },              // 18
    { "TW", "TWN", "Taiwan",                "AS", 23.70f,  121.00f, "Asia/Taipei" },              // 19
    { "HK", "HKG", "Hong Kong",             "AS", 22.32f,  114.17f, "Asia/Hong_Kong" },           // 20
    { "SG", "SGP", "Singapore",             "AS", 1.35f,   103.82f, "Asia/Singapore" },           // 21
    { "ID", "IDN", "Indonesia",             "AS", -0.79f,  113.92f, "Asia/Jakarta" },             // 22
    { "TH", "THA", "Thailand",              "AS", 15.87f,  100.99f, "Asia/Bangkok" },             // 23
    { "VN", "VNM", "Vietnam",               "AS", 14.06f,  108.28f, "Asia/Ho_Chi_Minh" },         // 24
    { "MX", "MEX", "Mexico",                "NA", 23.63f,  -102.55f,"America/Mexico_City" },      // 25
    { "AR", "ARG", "Argentina",             "SA", -38.42f, -63.62f, "America/Argentina/Buenos_Aires" }, // 26
    { "CL", "CHL", "Chile",                 "SA", -35.68f, -71.54f, "America/Santiago" },         // 27
    { "TR", "TUR", "Turkey",                "EU", 38.96f,  35.24f,  "Europe/Istanbul" },          // 28
    { "IL", "ISR", "Israel",                "AS", 31.05f,  34.85f,  "Asia/Jerusalem" },           // 29
    { "SA", "SAU", "Saudi Arabia",          "AS", 23.89f,  45.08f,  "Asia/Riyadh" },              // 30
    { "AE", "ARE", "United Arab Emirates",  "AS", 23.42f,  53.85f,  "Asia/Dubai" },               // 31
    { "EG", "EGY", "Egypt",                 "AF", 26.82f,  30.80f,  "Africa/Cairo" },             // 32
    { "NG", "NGA", "Nigeria",               "AF", 9.08f,   8.68f,   "Africa/Lagos" },             // 33
    { "NZ", "NZL", "New Zealand",           "OC", -40.90f, 174.89f, "Pacific/Auckland" },         // 34
    { "FI", "FIN", "Finland",               "EU", 61.92f,  25.75f,  "Europe/Helsinki" },          // 35
    { "NO", "NOR", "Norway",                "EU", 60.47f,  8.47f,   "Europe/Oslo" },              // 36
    { "DK", "DNK", "Denmark",               "EU", 56.26f,  9.50f,   "Europe/Copenhagen" },        // 37
    { "CH", "CHE", "Switzerland",           "EU", 46.82f,  8.23f,   "Europe/Zurich" },            // 38
    { "AT", "AUT", "Austria",               "EU", 47.52f,  14.55f,  "Europe/Vienna" },            // 39
    { "BE", "BEL", "Belgium",               "EU", 50.50f,  4.47f,   "Europe/Brussels" },          // 40
    { "CZ", "CZE", "Czech Republic",        "EU", 49.82f,  15.47f,  "Europe/Prague" },            // 41
    { "GR", "GRC", "Greece",                "EU", 39.07f,  21.82f,  "Europe/Athens" },            // 42
    { "PT", "PRT", "Portugal",              "EU", 39.40f,  -8.22f,  "Europe/Lisbon" },            // 43
    { "IE", "IRL", "Ireland",               "EU", 53.41f,  -8.24f,  "Europe/Dublin" },            // 44
    { "HU", "HUN", "Hungary",               "EU", 47.16f,  19.50f,  "Europe/Budapest" },          // 45
    { "RO", "ROU", "Romania",               "EU", 45.94f,  24.97f,  "Europe/Bucharest" },         // 46
    { "BG", "BGR", "Bulgaria",              "EU", 42.73f,  25.49f,  "Europe/Sofia" },             // 47
    { "KZ", "KAZ", "Kazakhstan",            "AS", 48.02f,  66.92f,  "Asia/Almaty" },              // 48
    { "PH", "PHL", "Philippines",           "AS", 12.88f,  121.77f, "Asia/Manila" },              // 49
    { "MY", "MYS", "Malaysia",              "AS", 4.21f,   101.98f, "Asia/Kuala_Lumpur" },        // 50
    { "PK", "PAK", "Pakistan",              "AS", 30.38f,  69.35f,  "Asia/Karachi" },             // 51
    { "BD", "BGD", "Bangladesh",            "AS", 23.68f,  90.36f,  "Asia/Dhaka" },               // 52
    { "IR", "IRN", "Iran",                  "AS", 32.43f,  53.69f,  "Asia/Tehran" },              // 53
    { "QA", "QAT", "Qatar",                 "AS", 25.35f,  51.18f,  "Asia/Qatar" },               // 54
    { "CO", "COL", "Colombia",              "SA", 4.57f,   -74.30f, "America/Bogota" },           // 55
    { "PE", "PER", "Peru",                  "SA", -9.19f,  -75.02f, "America/Lima" },             // 56
    { "KE", "KEN", "Kenya",                 "AF", -0.02f,  37.91f,  "Africa/Nairobi" },           // 57
    { "MA", "MAR", "Morocco",               "AF", 31.79f,  -7.09f,  "Africa/Casablanca" },        // 58
    { "DZ", "DZA", "Algeria",               "AF", 28.03f,  1.66f,   "Africa/Algiers" },           // 59
    { "TN", "TUN", "Tunisia",               "AF", 33.89f,  9.56f,   "Africa/Tunis" },             // 60
};

static const int g_countryCount = (int)(sizeof(g_countries) / sizeof(g_countries[0]));

// ===========================================
// IP 首段 -> 国家表索引 (简化映射), -1 = 未知
// ===========================================
static const short g_octetMap[256] = {
    /*   0 */ -1,  2, -1,  1,  1,  5,  1,  1,  1,  1, -1,  1,  1,  1,  0,  1,
    /*  16 */  1,  1,  1, -1,  1,  1,  1,  1,  1,  7,  1,  0, 28, -1,  1,  7,
    /*  32 */  1, -1,  1,  1,  0,  0,  5,  1,  0, 11, -1,  2,  0, -1,  4,  8,
    /*  48 */  1,  3,  1,  1,  1,  1,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0,
    /*  64 */  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    /*  80 */  5,  7,  7,  7,  7,  7,  0,  7,  7,  7,  7,  7,  4,  4,  4,  4,
    /*  96 */  1,  1,  1,  1,  1,  0,  0,  0,  1,  4,  0,  0,  0,  0,  0,  0,
    /* 112 */  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1,
    /* 128 */  1,  1,  1,  2,  0,  1,  1,  0,  0,  1,  0,  0,  0,  0,  0,  5,
    /* 144 */  0,  0,  0,  1,  1,  0,  0,  0,  0,  0,  0, 13,  0,  0,  0,  0,
    /* 160 */  0,  0,  1,  0,  0,  1,  1,  1,  0,  1,  1,  0,  1,  1,  1,  0,
    /* 176 */  6, 10,  4, 10,  0, 10,  0,  0,  1,  5, 10, 10,  4, 10, 10, 10,
    /* 192 */  1,  5,  7,  5,  7,  7, 11, 11,  1,  1, 10, 10,  0,  0,  1,  1,
    /* 208 */  1,  1,  0,  0,  1,  1,  1,  1,  0,  0,  0,  0,  5,  5,  1,  1,
    /* 224 */  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    /* 240 */  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, -1,
};

// 解析 "a.b.c.d", 返回 IP 首段 (0-255), 失败返回 -1
static int ParseIpFirstOctet(const char *ip)
{
    if (!ip || !*ip)
        return -1;
    int a = 0;
    const char *p = ip;
    while (*p >= '0' && *p <= '9') {
        a = a * 10 + (*p - '0');
        if (a > 255)
            return -1;
        p++;
    }
    if (*p != '.')
        return -1;
    return a;
}

// 查找国家: 支持 "a.b.c.d" 或 "a.b.c.d:port" (geoip.inc: 带端口时端口被忽略)
static const GeoCountry *LookupCountry(const char *ip)
{
    if (!ip || !*ip)
        return nullptr;

    char buf[64];
    size_t ipLen = strlen(ip);
    if (ipLen >= sizeof(buf))
        ipLen = sizeof(buf) - 1;
    memcpy(buf, ip, ipLen);
    buf[ipLen] = '\0';

    // 先剥离端口
    char *colon = strchr(buf, ':');
    if (colon)
        *colon = '\0';

    int octet = ParseIpFirstOctet(buf);
    if (octet < 0 || octet > 255)
        return nullptr;
    short idx = g_octetMap[octet];
    if (idx < 0 || idx >= g_countryCount)
        return nullptr;
    return &g_countries[idx];
}

// 2 字符大洲码 -> 大洲 id (对齐 geoip.inc Continent enum)
static int ContinentToId(const char *code)
{
    if (!code) return CONTINENT_UNKNOWN;
    if (code[0] == 'A' && code[1] == 'F') return CONTINENT_AFRICA;
    if (code[0] == 'A' && code[1] == 'N') return CONTINENT_ANTARCTICA;
    if (code[0] == 'A' && code[1] == 'S') return CONTINENT_ASIA;
    if (code[0] == 'E' && code[1] == 'U') return CONTINENT_EUROPE;
    if (code[0] == 'N' && code[1] == 'A') return CONTINENT_NORTH_AMERICA;
    if (code[0] == 'O' && code[1] == 'C') return CONTINENT_OCEANIA;
    if (code[0] == 'S' && code[1] == 'A') return CONTINENT_SOUTH_AMERICA;
    return CONTINENT_UNKNOWN;
}

// 大洲码 -> 全名
static const char *ContinentName(const char *code)
{
    if (!code) return "";
    if (code[0] == 'A' && code[1] == 'F') return "Africa";
    if (code[0] == 'A' && code[1] == 'N') return "Antarctica";
    if (code[0] == 'A' && code[1] == 'S') return "Asia";
    if (code[0] == 'E' && code[1] == 'U') return "Europe";
    if (code[0] == 'N' && code[1] == 'A') return "North America";
    if (code[0] == 'O' && code[1] == 'C') return "Oceania";
    if (code[0] == 'S' && code[1] == 'A') return "South America";
    return "";
}

static void SetAmxString(AMX *amx, cell param, const char *str, int maxLen)
{
    cell *dest;
    amx_GetAddr(amx, param, &dest);
    if (dest)
        amx_SetString(dest, str, 0, 0, (size_t)maxLen);
}

static void GetIpParam(AMX *amx, cell param, char *out, size_t outLen)
{
    cell *addr;
    amx_GetAddr(amx, param, &addr);
    if (addr) {
        amx_GetString(out, addr, 0, (int)outLen);
    } else {
        out[0] = '\0';
    }
}

// Haversine 距离 (对齐原版: metric=6371.0 km, imperial=3956.0 miles)
static float GeoDistance(float lat1, float lon1, float lat2, float lon2, int system)
{
    const float kEarthRadiusKm = 6371.0f;
    const float kEarthRadiusMi = 3956.0f;
    const float kPi = 3.14159265358979323846f;

    float dLat = (lat2 - lat1) * kPi / 180.0f;
    float dLon = (lon2 - lon1) * kPi / 180.0f;

    float sLat = sinf(dLat / 2.0f);
    float sLon = sinf(dLon / 2.0f);
    float a = sLat * sLat +
              cosf(lat1 * kPi / 180.0f) * cosf(lat2 * kPi / 180.0f) * sLon * sLon;
    float c = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));

    float radius = (system == SYSTEM_IMPERIAL) ? kEarthRadiusMi : kEarthRadiusKm;
    return radius * c;
}

// ============================================
// Native 实现
// ============================================

// geoip_code2_ex(const ip[], result[3]) - 成功填 2 位国家码返回 true, 失败不改 buffer 返回 false
cell AMX_NATIVE_CALL amxx_geoip_code2_ex(AMX *amx, cell *params)
{
    char ip[64];
    GetIpParam(amx, params[1], ip, sizeof(ip));

    const GeoCountry *c = LookupCountry(ip);
    if (c) {
        SetAmxString(amx, params[2], c->code2, 3);
        return 1;
    }
    return 0;
}

// geoip_code3_ex(const ip[], result[4]) - 成功填 3 位国家码返回 true, 失败不改 buffer 返回 false
cell AMX_NATIVE_CALL amxx_geoip_code3_ex(AMX *amx, cell *params)
{
    char ip[64];
    GetIpParam(amx, params[1], ip, sizeof(ip));

    const GeoCountry *c = LookupCountry(ip);
    if (c) {
        SetAmxString(amx, params[2], c->code3, 4);
        return 1;
    }
    return 0;
}

// geoip_code2(const ip[], ccode[3]) - deprecated: 成功返回码长度, 失败写 "error" 返回 0
cell AMX_NATIVE_CALL amxx_geoip_code2(AMX *amx, cell *params)
{
    char ip[64];
    GetIpParam(amx, params[1], ip, sizeof(ip));

    const GeoCountry *c = LookupCountry(ip);
    if (c) {
        SetAmxString(amx, params[2], c->code2, 3);
        return 2;
    }
    SetAmxString(amx, params[2], "error", 3);
    return 0;
}

// geoip_code3(const ip[], result[4]) - deprecated: 成功返回码长度, 失败写 "error" 返回 0
cell AMX_NATIVE_CALL amxx_geoip_code3(AMX *amx, cell *params)
{
    char ip[64];
    GetIpParam(amx, params[1], ip, sizeof(ip));

    const GeoCountry *c = LookupCountry(ip);
    if (c) {
        SetAmxString(amx, params[2], c->code3, 4);
        return 3;
    }
    SetAmxString(amx, params[2], "error", 4);
    return 0;
}

// geoip_country(const ip[], result[], len = 45) - deprecated: 成功返回国家名长度, 失败写 "error" 返回 0
cell AMX_NATIVE_CALL amxx_geoip_country(AMX *amx, cell *params)
{
    char ip[64];
    GetIpParam(amx, params[1], ip, sizeof(ip));
    int len = (int)params[3];

    const GeoCountry *c = LookupCountry(ip);
    if (c) {
        SetAmxString(amx, params[2], c->name, len);
        return (cell)strlen(c->name);
    }
    int errLen = (len < 6) ? len : 6;
    SetAmxString(amx, params[2], "error", errLen);
    return 0;
}

// geoip_country_ex(const ip[], result[], len, id = -1) - 返回结果长度, 失败 0 (id 语言支持: 简化统一返回英文名)
cell AMX_NATIVE_CALL amxx_geoip_country_ex(AMX *amx, cell *params)
{
    char ip[64];
    GetIpParam(amx, params[1], ip, sizeof(ip));
    int len = (int)params[3];

    const GeoCountry *c = LookupCountry(ip);
    if (c) {
        SetAmxString(amx, params[2], c->name, len);
        return (cell)strlen(c->name);
    }
    return 0;
}

// geoip_city(const ip[], result[], len, id = -1) - 需要 City 库, 内置表无城市数据, 返回 0 不改 buffer
cell AMX_NATIVE_CALL amxx_geoip_city(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return 0;
}

// geoip_region_code(const ip[], result[], len) - 需要 City 库, 返回 0 不改 buffer
cell AMX_NATIVE_CALL amxx_geoip_region_code(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return 0;
}

// geoip_region_name(const ip[], result[], len, id = -1) - 需要 City 库, 返回 0 不改 buffer
cell AMX_NATIVE_CALL amxx_geoip_region_name(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return 0;
}

// geoip_timezone(const ip[], result[], len) - 原版需要 City 库, 内置表无城市数据, 返回 0 不改 buffer
cell AMX_NATIVE_CALL amxx_geoip_timezone(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    return 0;
}

// geoip_latitude(const ip[]) - 返回国家代表纬度, 未找到返回 0.0
cell AMX_NATIVE_CALL amxx_geoip_latitude(AMX *amx, cell *params)
{
    char ip[64];
    GetIpParam(amx, params[1], ip, sizeof(ip));

    const GeoCountry *c = LookupCountry(ip);
    if (c) {
        float lat = c->lat;
        return amx_ftoc(lat);
    }
    float zero = 0.0f;
    return amx_ftoc(zero);
}

// geoip_longitude(const ip[]) - 返回国家代表经度, 未找到返回 0.0
cell AMX_NATIVE_CALL amxx_geoip_longitude(AMX *amx, cell *params)
{
    char ip[64];
    GetIpParam(amx, params[1], ip, sizeof(ip));

    const GeoCountry *c = LookupCountry(ip);
    if (c) {
        float lon = c->lon;
        return amx_ftoc(lon);
    }
    float zero = 0.0f;
    return amx_ftoc(zero);
}

// geoip_distance(Float:lat1, Float:lon1, Float:lat2, Float:lon2, system = SYSTEM_METRIC)
// Haversine 公式, metric=km (半径 6371.0), imperial=miles (半径 3956.0)
cell AMX_NATIVE_CALL amxx_geoip_distance(AMX *amx, cell *params)
{
    (void)amx;
    float lat1 = amx_ctof(params[1]);
    float lon1 = amx_ctof(params[2]);
    float lat2 = amx_ctof(params[3]);
    float lon2 = amx_ctof(params[4]);
    int system = (int)params[5];

    float dist = GeoDistance(lat1, lon1, lat2, lon2, system);
    return amx_ftoc(dist);
}

// geoip_continent_code(const ip[], result[3]) - 返回 continent id, result 填 2 位大洲码; 失败返回 0
cell AMX_NATIVE_CALL amxx_geoip_continent_code(AMX *amx, cell *params)
{
    char ip[64];
    GetIpParam(amx, params[1], ip, sizeof(ip));

    const GeoCountry *c = LookupCountry(ip);
    if (c) {
        SetAmxString(amx, params[2], c->continent, 3);
        return (cell)ContinentToId(c->continent);
    }
    return CONTINENT_UNKNOWN;
}

// geoip_continent_name(const ip[], result[], len, id = -1) - 返回大洲全名长度, 失败 0
cell AMX_NATIVE_CALL amxx_geoip_continent_name(AMX *amx, cell *params)
{
    char ip[64];
    GetIpParam(amx, params[1], ip, sizeof(ip));
    int len = (int)params[3];

    const GeoCountry *c = LookupCountry(ip);
    if (c) {
        const char *name = ContinentName(c->continent);
        SetAmxString(amx, params[2], name, len);
        return (cell)strlen(name);
    }
    return 0;
}

// ===== Native registration =====

AMX_NATIVE_INFO geoip_natives[] = {
    {"geoip_code2_ex", amxx_geoip_code2_ex},
    {"geoip_code3_ex", amxx_geoip_code3_ex},
    {"geoip_code2", amxx_geoip_code2},
    {"geoip_code3", amxx_geoip_code3},
    {"geoip_country", amxx_geoip_country},
    {"geoip_country_ex", amxx_geoip_country_ex},
    {"geoip_city", amxx_geoip_city},
    {"geoip_region_code", amxx_geoip_region_code},
    {"geoip_region_name", amxx_geoip_region_name},
    {"geoip_timezone", amxx_geoip_timezone},
    {"geoip_latitude", amxx_geoip_latitude},
    {"geoip_longitude", amxx_geoip_longitude},
    {"geoip_distance", amxx_geoip_distance},
    {"geoip_continent_code", amxx_geoip_continent_code},
    {"geoip_continent_name", amxx_geoip_continent_name},
    {nullptr, nullptr}
};

void RegisterGeoipNatives(AMX *amx)
{
    amx_Register(amx, geoip_natives, -1);
}

// 无运行时状态需要清理 (内置静态表)
void ResetGeoipGlobals()
{
}
