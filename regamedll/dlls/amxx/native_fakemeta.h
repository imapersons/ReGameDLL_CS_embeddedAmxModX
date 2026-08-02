#pragma once
#ifndef AMXX_NATIVE_FAKEMETA_H
#define AMXX_NATIVE_FAKEMETA_H

#include "amx.h"

extern AMX_NATIVE_INFO fakemeta_natives[];

void RegisterFakemetaNatives(AMX *amx);

// Fakemeta utility: radius damage
cell AMX_NATIVE_CALL amxx_fm_radius_damage(AMX *amx, cell *params);

// P1: Fakemeta 补充
cell AMX_NATIVE_CALL amxx_fm_set_rendering(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_fm_entity_set_model(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_fm_remove_entity(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_fm_set_velocity(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_fm_entity_set_size(AMX *amx, cell *params);

// ===== 完整 FieldType / TypeDescription 系统（对齐 IGameConfigs.h）=====
enum class FMFieldType {
    FIELD_NONE = 0,
    FIELD_FLOAT = 1,
    FIELD_STRINGINT = 2,
    FIELD_STRINGPTR = 3,
    FIELD_STRING = 4,
    FIELD_CLASSPTR = 5,
    FIELD_CLASS = 6,
    FIELD_STRUCTURE = 7,
    FIELD_EHANDLE = 8,
    FIELD_ENTVARS = 9,
    FIELD_EDICT = 10,
    FIELD_VECTOR = 11,
    FIELD_POINTER = 12,
    FIELD_INTEGER = 13,
    FIELD_FUNCTION = 14,
    FIELD_BOOLEAN = 15,
    FIELD_SHORT = 16,
    FIELD_CHARACTER = 17,
};

struct FMTypeDescription {
    FMFieldType fieldType;
    int         fieldOffset;
    int         fieldSize;
    bool        fieldUnsigned;

    FMTypeDescription() : fieldType(FMFieldType::FIELD_NONE), fieldOffset(0), fieldSize(0), fieldUnsigned(false) {}
};

enum class FMBaseFieldType { None = 0, Integer = 1, Float = 2, Vector = 3, Entity = 4, String = 5 };

bool FMGameConfig_GetOffsetByClass(const char *className, const char *memberName, FMTypeDescription *out);
bool FMGameConfig_GetOffsetByMember(const char *memberName, FMTypeDescription *out);

namespace FMPvData {
    cell   GetInt(void *pObject, const FMTypeDescription &data, int element);
    void   SetInt(void *pObject, const FMTypeDescription &data, cell value, int element);
    cell   GetFloat(void *pObject, const FMTypeDescription &data, int element);
    void   SetFloat(void *pObject, const FMTypeDescription &data, float value, int element);
    void   GetVector(void *pObject, const FMTypeDescription &data, cell *pVector, int element);
    void   SetVector(void *pObject, const FMTypeDescription &data, const cell *pVector, int element);
    cell   GetEntity(void *pObject, const FMTypeDescription &data, int element);
    void   SetEntity(void *pObject, const FMTypeDescription &data, int value, int element);
    char*  GetString(void *pObject, const FMTypeDescription &data, int element);
    cell   SetString(void *pObject, const FMTypeDescription &data, const char *value, int maxlen, int element);
    FMBaseFieldType GetBaseDataType(FMFieldType t);
    const char* GetBaseTypeName(FMBaseFieldType t);
}

// ===== get_ent_data 系列 natives =====
cell AMX_NATIVE_CALL amxx_get_ent_data(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_ent_data(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_ent_data_float(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_ent_data_float(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_ent_data_vector(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_ent_data_vector(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_ent_data_entity(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_ent_data_entity(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_ent_data_string(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_ent_data_string(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_ent_data_size(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_find_ent_data_info(AMX *amx, cell *params);

// ===== get_gamerules 系列 natives =====
cell AMX_NATIVE_CALL amxx_get_gamerules_int(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_gamerules_int(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_gamerules_float(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_gamerules_float(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_gamerules_vector(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_gamerules_vector(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_gamerules_entity(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_gamerules_entity(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_gamerules_string(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_set_gamerules_string(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_get_gamerules_size(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_find_gamerules_info(AMX *amx, cell *params);

#endif
