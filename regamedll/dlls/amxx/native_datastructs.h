#pragma once
#ifndef AMXX_NATIVE_DATASTRUCTS_H
#define AMXX_NATIVE_DATASTRUCTS_H

#include "amx.h"

extern AMX_NATIVE_INFO datastruct_natives[];

cell AMX_NATIVE_CALL amxx_array_create(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_array_destroy(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_array_push(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_array_push_cell(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_array_push_string(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_array_get(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_array_get_cell(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_array_get_string(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_array_set(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_array_set_cell(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_array_set_string(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_array_remove(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_array_clear(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_array_size(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_array_blocksize(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_array_get_array(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_array_push_array(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_array_set_array(AMX *amx, cell *params);

// cellarray.inc: insert a cell/string before/after the given item
cell AMX_NATIVE_CALL amxx_array_insert_cell_before(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_array_insert_cell_after(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_array_insert_string_before(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_array_insert_string_after(AMX *amx, cell *params);

cell AMX_NATIVE_CALL amxx_trie_create(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_trie_destroy(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_trie_insert(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_trie_delete(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_trie_delete_key(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_trie_retrieve(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_trie_key_exists(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_trie_clear(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_trie_count(AMX *amx, cell *params);

cell AMX_NATIVE_CALL amxx_cellarray_create(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_cellarray_destroy(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_cellarray_push(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_cellarray_get(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_cellarray_set(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_cellarray_remove(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_cellarray_clear(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_cellarray_size(AMX *amx, cell *params);

// Modern Array API
cell AMX_NATIVE_CALL amxx_array_sort(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_array_sortex(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_array_find_string(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_array_find_value(AMX *amx, cell *params);

// sorting.inc: SortADTArray — in-place Array container sorting
cell AMX_NATIVE_CALL amxx_array_sort_adt(AMX *amx, cell *params);

// Trie Snapshot
cell AMX_NATIVE_CALL amxx_trie_snapshot_create(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_trie_snapshot_length(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_trie_snapshot_key_buffer_size(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_trie_snapshot_get_key(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_trie_snapshot_destroy(AMX *amx, cell *params);

// Trie Iterator
cell AMX_NATIVE_CALL amxx_trie_iter_create(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_trie_iter_ended(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_trie_iter_next(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_trie_iter_get_key(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_trie_iter_get_size(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_trie_iter_get_cell(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_trie_iter_get_string(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_trie_iter_get_array(AMX *amx, cell *params);
cell AMX_NATIVE_CALL amxx_trie_iter_destroy(AMX *amx, cell *params);

void RegisterDatastructNatives(AMX *amx);

void CleanupDatastructs();

#endif