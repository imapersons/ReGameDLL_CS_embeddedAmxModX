#pragma once
#ifndef AMXX_NATIVE_GEOIP_H
#define AMXX_NATIVE_GEOIP_H

#include "amx.h"

// GeoIP native 实现 (对齐原版 AMX Mod X geoip.inc 的 15 个 native)
cell AMX_NATIVE_CALL amxx_geoip_code2_ex(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_geoip_code3_ex(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_geoip_code2(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_geoip_code3(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_geoip_country(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_geoip_country_ex(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_geoip_city(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_geoip_region_code(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_geoip_region_name(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_geoip_timezone(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_geoip_latitude(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_geoip_longitude(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_geoip_distance(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_geoip_continent_code(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_geoip_continent_name(AMX *amx, cell *params);

extern AMX_NATIVE_INFO geoip_natives[];

void RegisterGeoipNatives(AMX *amx);

// 清理 GeoIP 状态 (地图切换时调用)
void ResetGeoipGlobals();

#endif
