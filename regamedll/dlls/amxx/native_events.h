#pragma once
#ifndef AMXX_NATIVE_EVENTS_H
#define AMXX_NATIVE_EVENTS_H

#include "amx.h"

extern AMX_NATIVE_INFO event_natives[];

cell AMX_NATIVE_CALL amxx_register_event(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_unregister_event(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_register_message(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_unregister_message(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_fire_event(AMX *amx, cell *params);

cell AMX_NATIVE_CALL amxx_unregister_event_by_handle(AMX *amx, cell *params);

void RegisterEventNatives(AMX *amx);

#endif