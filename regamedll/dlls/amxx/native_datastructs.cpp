#include "precompiled.h"
#include "datastructs.h"
#include "amx.h"
#include <cstdio>

static std::vector<AMXXArray*> g_arrays;
static std::vector<AMXXTrie*> g_tries;
static std::vector<AMXXCellArray*> g_cellArrays;

cell AMX_NATIVE_CALL amxx_array_create(AMX *amx, cell *params)
{
    int blockSize = params[1];
    int initialSize = params[2];
    
    AMXXArray *arr = new AMXXArray();
    arr->Create(blockSize, initialSize);
    
    int idx = g_arrays.size();
    g_arrays.push_back(arr);
    
    return idx + 1;
}

cell AMX_NATIVE_CALL amxx_array_destroy(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size()) return 0;
    
    delete g_arrays[idx];
    g_arrays[idx] = nullptr;
    
    return 1;
}

cell AMX_NATIVE_CALL amxx_array_push(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return -1;
    
    cell *data;
    amx_GetAddr(amx, params[2], &data);
    
    return g_arrays[idx]->Push(data);
}

cell AMX_NATIVE_CALL amxx_array_push_cell(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return -1;
    
    return g_arrays[idx]->PushCell(params[2]);
}

cell AMX_NATIVE_CALL amxx_array_push_string(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return -1;
    
    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    char str[512];
    amx_GetString(str, addr, 0, sizeof(str));
    
    int ret = g_arrays[idx]->PushString(str);
    return ret;
}

cell AMX_NATIVE_CALL amxx_array_get(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return 0;
    
    cell *dest;
    amx_GetAddr(amx, params[3], &dest);
    
    return g_arrays[idx]->Get(params[2], dest) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_array_get_cell(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return 0;
    
    return g_arrays[idx]->GetCell(params[2]);
}

cell AMX_NATIVE_CALL amxx_array_get_string(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return 0;
    
    cell *dest;
    amx_GetAddr(amx, params[3], &dest);
    
    char buffer[512];
    if (!g_arrays[idx]->GetString(params[2], buffer, sizeof(buffer))) return 0;
    
    amx_SetString(dest, buffer, 0, 0, params[4]);
    return 1;
}

cell AMX_NATIVE_CALL amxx_array_set(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return 0;
    
    cell *data;
    amx_GetAddr(amx, params[3], &data);
    
    return g_arrays[idx]->Set(params[2], data) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_array_set_cell(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return 0;
    
    return g_arrays[idx]->SetCell(params[2], params[3]) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_array_set_string(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return 0;
    
    cell *addr;
    amx_GetAddr(amx, params[3], &addr);
    char str[512];
    amx_GetString(str, addr, 0, sizeof(str));
    
    return g_arrays[idx]->SetString(params[2], str) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_array_remove(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return 0;
    
    return g_arrays[idx]->Remove(params[2]) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_array_clear(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return 0;
    
    g_arrays[idx]->Clear();
    return 1;
}

cell AMX_NATIVE_CALL amxx_array_size(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return 0;
    
    return g_arrays[idx]->Size();
}

cell AMX_NATIVE_CALL amxx_array_blocksize(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return 0;
    
    return g_arrays[idx]->BlockSize();
}

// ArrayGetArray(array, index, dest[], dest_size = sizeof(dest))
// Copies the block data of array[index] into the dest[] buffer
cell AMX_NATIVE_CALL amxx_array_get_array(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return 0;
    
    cell *dest;
    amx_GetAddr(amx, params[3], &dest);
    
    return g_arrays[idx]->Get(params[2], dest) ? 1 : 0;
}

// ArrayPushArray(destArray, srcArray[])
// Pushes blockSize cells from srcArray[] (a PAWN array address) into destArray
cell AMX_NATIVE_CALL amxx_array_push_array(AMX *amx, cell *params)
{
    int destIdx = params[1] - 1;
    if (destIdx < 0 || destIdx >= (int)g_arrays.size() || !g_arrays[destIdx]) return 0;

    cell *src;
    amx_GetAddr(amx, params[2], &src);

    return g_arrays[destIdx]->Push(src);
}

// ArraySetArray(Array:which, any:item[], index = -1)
// Replaces the block at index in which with item[]; index = -1 means the last item
cell AMX_NATIVE_CALL amxx_array_set_array(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return 0;

    AMXXArray *arr = g_arrays[idx];

    // index is optional; -1 defaults to the last item
    int numParams = (int)(params[0] / sizeof(cell));
    int targetIdx = (numParams >= 3) ? (int)params[3] : -1;

    if (targetIdx < 0) {
        targetIdx = arr->Size() - 1;
    }

    if (targetIdx < 0 || targetIdx >= arr->Size()) return 0;

    cell *src;
    amx_GetAddr(amx, params[2], &src);

    return arr->Set(targetIdx, src) ? 1 : 0;
}

// ArrayGetStringHandle(array, index) - returns a handle to the string at index
cell AMX_NATIVE_CALL amxx_array_get_string_handle(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return 0;
    
    AMXXArray *arr = g_arrays[idx];
    int strIndex = params[2];
    
    char buffer[512];
    if (!arr->GetString(strIndex, buffer, sizeof(buffer))) return 0;
    
    // Create a new array to hold this string as a handle
    AMXXArray *strHandle = new AMXXArray();
    strHandle->Create((int)strlen(buffer) + 1, 1);
    strHandle->PushString(buffer);
    
    int newIdx = g_arrays.size();
    g_arrays.push_back(strHandle);
    
    return newIdx + 1;
}

cell AMX_NATIVE_CALL amxx_trie_create(AMX *amx, cell *params)
{
    AMXXTrie *trie = new AMXXTrie();
    trie->Create();
    
    int idx = g_tries.size();
    g_tries.push_back(trie);
    
    return idx + 1;
}

cell AMX_NATIVE_CALL amxx_trie_destroy(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_tries.size()) return 0;
    
    delete g_tries[idx];
    g_tries[idx] = nullptr;
    
    return 1;
}

cell AMX_NATIVE_CALL amxx_trie_insert(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_tries.size() || !g_tries[idx]) return 0;
    
    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    char key[512];
    amx_GetString(key, addr, 0, sizeof(key));
    
    return g_tries[idx]->Insert(key, params[3]) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_trie_delete(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_tries.size() || !g_tries[idx]) return 0;
    
    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    char key[512];
    amx_GetString(key, addr, 0, sizeof(key));
    
    return g_tries[idx]->Delete(key) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_trie_retrieve(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_tries.size() || !g_tries[idx]) return 0;
    
    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    char key[512];
    amx_GetString(key, addr, 0, sizeof(key));
    
    cell value;
    if (!g_tries[idx]->Retrieve(key, &value)) return 0;
    
    cell *dest;
    amx_GetAddr(amx, params[3], &dest);
    *dest = value;
    
    return 1;
}

cell AMX_NATIVE_CALL amxx_trie_key_exists(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_tries.size() || !g_tries[idx]) return 0;
    
    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    char key[512];
    amx_GetString(key, addr, 0, sizeof(key));
    
    return g_tries[idx]->KeyExists(key) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_trie_clear(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_tries.size() || !g_tries[idx]) return 0;
    
    g_tries[idx]->Clear();
    return 1;
}

cell AMX_NATIVE_CALL amxx_trie_count(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_tries.size() || !g_tries[idx]) return 0;

    return g_tries[idx]->Count();
}

// ==============================================
// Additional advanced APIs
// ==============================================

// ---- Array advanced operations ----

// ArrayInsertArrayBefore(array, index, source[])
cell AMX_NATIVE_CALL amxx_array_insert_before(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return 0;
    cell *src;
    amx_GetAddr(amx, params[3], &src);
    return g_arrays[idx]->Insert(params[2], src) ? 1 : 0;
}

// ArrayInsertArrayAfter(array, index, source[])
cell AMX_NATIVE_CALL amxx_array_insert_after(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return 0;
    cell *src;
    amx_GetAddr(amx, params[3], &src);
    return g_arrays[idx]->Insert(params[2] + 1, src) ? 1 : 0;
}

// ArrayInsertCellBefore(Array:which, item, any:value)
// Inserts a cell before item (value written as a single-element block)
cell AMX_NATIVE_CALL amxx_array_insert_cell_before(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return 0;
    AMXXArray *arr = g_arrays[idx];
    int bs = arr->BlockSize();
    if (bs <= 0) return 0;
    std::vector<cell> blk(bs, 0);
    blk[0] = params[3];
    return arr->Insert((int)params[2], blk.data()) ? 1 : 0;
}

// ArrayInsertCellAfter(Array:which, item, any:value)
// Inserts a cell after item
cell AMX_NATIVE_CALL amxx_array_insert_cell_after(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return 0;
    AMXXArray *arr = g_arrays[idx];
    int bs = arr->BlockSize();
    if (bs <= 0) return 0;
    std::vector<cell> blk(bs, 0);
    blk[0] = params[3];
    return arr->Insert((int)params[2] + 1, blk.data()) ? 1 : 0;
}

// ArrayInsertStringBefore(Array:which, item, const value[])
// Inserts a string before item (packed into a block)
cell AMX_NATIVE_CALL amxx_array_insert_string_before(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return 0;
    AMXXArray *arr = g_arrays[idx];
    int bs = arr->BlockSize();
    if (bs <= 0) return 0;
    cell *addr;
    amx_GetAddr(amx, params[3], &addr);
    char str[512];
    amx_GetString(str, addr, 0, sizeof(str));
    std::vector<cell> blk(bs, 0);
    for (int i = 0; i < bs; i++) {
        blk[i] = (cell)(unsigned char)str[i];
        if (str[i] == '\0') break;
    }
    return arr->Insert((int)params[2], blk.data()) ? 1 : 0;
}

// ArrayInsertStringAfter(Array:which, item, const value[])
// Inserts a string after item
cell AMX_NATIVE_CALL amxx_array_insert_string_after(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return 0;
    AMXXArray *arr = g_arrays[idx];
    int bs = arr->BlockSize();
    if (bs <= 0) return 0;
    cell *addr;
    amx_GetAddr(amx, params[3], &addr);
    char str[512];
    amx_GetString(str, addr, 0, sizeof(str));
    std::vector<cell> blk(bs, 0);
    for (int i = 0; i < bs; i++) {
        blk[i] = (cell)(unsigned char)str[i];
        if (str[i] == '\0') break;
    }
    return arr->Insert((int)params[2] + 1, blk.data()) ? 1 : 0;
}

// ArraySwap(array, itemA, itemB)
cell AMX_NATIVE_CALL amxx_array_swap(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return 0;
    return g_arrays[idx]->Swap(params[2], params[3]) ? 1 : 0;
}

// ArrayResize(array, newSize, fill=0)
cell AMX_NATIVE_CALL amxx_array_resize(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return 0;
    cell fill = (params[0] / sizeof(cell) >= 3) ? params[3] : 0;
    return g_arrays[idx]->Resize(params[2], fill) ? 1 : 0;
}

// ArrayClone(array)
cell AMX_NATIVE_CALL amxx_array_clone(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return 0;
    AMXXArray *copy = g_arrays[idx]->Clone();
    if (!copy) return 0;
    int newIdx = (int)g_arrays.size();
    g_arrays.push_back(copy);
    return newIdx + 1;
}

// Standard alias of ArrayRemove
cell AMX_NATIVE_CALL amxx_array_delete_item(AMX *amx, cell *params)
{
    return amxx_array_remove(amx, params);
}

// Standard alias with capital S
cell AMX_NATIVE_CALL amxx_array_get_size(AMX *amx, cell *params)
{
    return amxx_array_size(amx, params);
}

// ---- Trie typed API ----

// TrieSetCell(handle, const key[], any:value, replace=true)
cell AMX_NATIVE_CALL amxx_trie_set_cell(AMX *amx, cell *params)
{
    (void)amx;
    return amxx_trie_insert(amx, params);  // Insert always overwrites
}

// TrieGetCell(handle, const key[], &any:value)
cell AMX_NATIVE_CALL amxx_trie_get_cell(AMX *amx, cell *params)
{
    return amxx_trie_retrieve(amx, params);
}

// TrieSetString(handle, const key[], const value[])
cell AMX_NATIVE_CALL amxx_trie_set_string(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_tries.size() || !g_tries[idx]) return 0;
    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    char key[512];
    amx_GetString(key, addr, 0, sizeof(key));
    cell *valAddr;
    amx_GetAddr(amx, params[3], &valAddr);
    char value[1024];
    amx_GetString(value, valAddr, 0, sizeof(value));
    // Store the duplicated string pointer as the cell value (note: leaks)
    cell encoded = (cell)(uintptr_t)strdup(value);
    return g_tries[idx]->Insert(key, encoded, TRIE_VALUE_STRING) ? 1 : 0;
}

// TrieGetString(handle, const key[], buffer[], maxlen)
cell AMX_NATIVE_CALL amxx_trie_get_string(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_tries.size() || !g_tries[idx]) return 0;
    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    char key[512];
    amx_GetString(key, addr, 0, sizeof(key));
    cell value;
    if (!g_tries[idx]->Retrieve(key, &value)) return 0;
    cell *dest;
    amx_GetAddr(amx, params[3], &dest);
    const char *str = (const char *)(uintptr_t)value;
    if (!str) return 0;
    amx_SetString(dest, str, 0, 0, (int)params[4]);
    return 1;
}

// TrieSetArray(handle, const key[], const any:source[], size=sizeof(source))
cell AMX_NATIVE_CALL amxx_trie_set_array(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_tries.size() || !g_tries[idx]) return 0;
    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    char key[512];
    amx_GetString(key, addr, 0, sizeof(key));
    cell *src;
    amx_GetAddr(amx, params[3], &src);
    int size = (int)params[4];
    // Store a pointer to the copied array as the cell value
    cell *copy = new cell[size];
    for (int i = 0; i < size; i++) copy[i] = src[i];
    return g_tries[idx]->Insert(key, (cell)(uintptr_t)copy, TRIE_VALUE_ARRAY) ? 1 : 0;
}

// TrieGetArray(handle, const key[], any:dest[], size=sizeof(dest))
cell AMX_NATIVE_CALL amxx_trie_get_array(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_tries.size() || !g_tries[idx]) return 0;
    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    char key[512];
    amx_GetString(key, addr, 0, sizeof(key));
    cell value;
    if (!g_tries[idx]->Retrieve(key, &value)) return 0;
    cell *src = (cell *)(uintptr_t)value;
    if (!src) return 0;
    cell *dest;
    amx_GetAddr(amx, params[3], &dest);
    int size = (int)params[4];
    for (int i = 0; i < size; i++) dest[i] = src[i];
    return 1;
}

// Standard alias
cell AMX_NATIVE_CALL amxx_trie_get_size(AMX *amx, cell *params)
{
    return amxx_trie_count(amx, params);
}

// Standard name (alias of TrieDelete)
// Deletes the key; returns 1 if it existed, 0 otherwise
cell AMX_NATIVE_CALL amxx_trie_delete_key(AMX *amx, cell *params)
{
    return amxx_trie_delete(amx, params);
}

// ---- Simple Stack implementation (backed by AMXXArray, supports blocksize) ----
// g_stacks serves both the legacy StackPush/StackPop (blocksize=1) and the modern CellStack API

static std::vector<AMXXArray*> g_stacks;

// Legacy Stack API; blocksize is fixed at 1
cell AMX_NATIVE_CALL amxx_stack_create(AMX *amx, cell *params)
{
    (void)amx; (void)params;
    AMXXArray *st = new AMXXArray();
    st->Create(1, 0);
    int idx = (int)g_stacks.size();
    g_stacks.push_back(st);
    return idx + 1;
}

// StackDestroy(handle, free=false)
cell AMX_NATIVE_CALL amxx_stack_destroy(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_stacks.size() || !g_stacks[idx]) return 0;
    delete g_stacks[idx];
    g_stacks[idx] = nullptr;
    return 1;
}

// StackPush(handle, any:value)
cell AMX_NATIVE_CALL amxx_stack_push(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_stacks.size() || !g_stacks[idx]) return 0;
    return g_stacks[idx]->PushCell(params[2]);
}

// StackPop(handle, &any:value)
cell AMX_NATIVE_CALL amxx_stack_pop(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_stacks.size() || !g_stacks[idx]) return 0;
    AMXXArray *st = g_stacks[idx];
    int sz = st->Size();
    if (sz == 0) return 0;
    cell val = st->GetCell(sz - 1);
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    *dest = val;
    st->Remove(sz - 1);
    return 1;
}

// StackTop(handle, &any:value)
cell AMX_NATIVE_CALL amxx_stack_top(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_stacks.size() || !g_stacks[idx]) return 0;
    AMXXArray *st = g_stacks[idx];
    int sz = st->Size();
    if (sz == 0) return 0;
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    *dest = st->GetCell(sz - 1);
    return 1;
}

// ==============================================
// Modern CellStack API (cellstack.inc)
// Reuses the g_stacks handle pool and supports a configurable blocksize
// ==============================================

// Stack:CreateStack(blocksize = 1)
cell AMX_NATIVE_CALL amxx_create_stack(AMX *amx, cell *params)
{
    (void)amx;
    int blocksize = (int)params[1];
    if (blocksize <= 0) blocksize = 1;
    AMXXArray *st = new AMXXArray();
    st->Create(blocksize, 0);
    int idx = (int)g_stacks.size();
    g_stacks.push_back(st);
    return idx + 1;
}

// PushStackCell(Stack:handle, any:value)
cell AMX_NATIVE_CALL amxx_push_stack_cell(AMX *amx, cell *params)
{
    (void)amx;
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_stacks.size() || !g_stacks[idx]) return 0;
    AMXXArray *st = g_stacks[idx];
    int bs = st->BlockSize();
    if (bs <= 0) return 0;
    if (bs == 1) {
        st->PushCell(params[2]);
        return 1;
    }
    std::vector<cell> blk(bs, 0);
    blk[0] = params[2];
    st->Push(blk.data());
    return 1;
}

// PushStackString(Stack:handle, const value[])
cell AMX_NATIVE_CALL amxx_push_stack_string(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_stacks.size() || !g_stacks[idx]) return 0;
    AMXXArray *st = g_stacks[idx];
    int bs = st->BlockSize();
    if (bs <= 0) return 0;
    cell *addr;
    amx_GetAddr(amx, params[2], &addr);
    std::vector<char> strbuf(bs + 1, 0);
    amx_GetString(strbuf.data(), addr, 0, bs + 1);
    std::vector<cell> blk(bs, 0);
    for (int i = 0; i < bs; i++) {
        blk[i] = (cell)(unsigned char)strbuf[i];
        if (strbuf[i] == '\0') break;
    }
    st->Push(blk.data());
    return 1;
}

// PushStackArray(Stack:handle, const any:values[], size = -1)
cell AMX_NATIVE_CALL amxx_push_stack_array(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_stacks.size() || !g_stacks[idx]) return 0;
    AMXXArray *st = g_stacks[idx];
    int bs = st->BlockSize();
    if (bs <= 0) return 0;
    cell *src;
    amx_GetAddr(amx, params[2], &src);
    int size = (int)params[3];
    int count = bs;
    if (size != -1 && size <= bs) count = size;
    if (count < 0) count = 0;
    std::vector<cell> blk(bs, 0);
    for (int i = 0; i < count; i++) blk[i] = src[i];
    st->Push(blk.data());
    return 1;
}

// bool:PopStackCell(Stack:handle, &any:value, block = 0, bool:asChar = false)
cell AMX_NATIVE_CALL amxx_pop_stack_cell(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_stacks.size() || !g_stacks[idx]) return 0;
    AMXXArray *st = g_stacks[idx];
    int sz = st->Size();
    if (sz == 0) return 0;
    int bs = st->BlockSize();
    if (bs <= 0) return 0;
    int block = (int)params[3];
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    std::vector<cell> blk(bs, 0);
    st->Get(sz - 1, blk.data());
    if (params[4] == 0) {
        if (block < 0 || block >= bs) return 0;
        *dest = blk[block];
    } else {
        if (block < 0 || block >= bs * (int)sizeof(cell)) return 0;
        *dest = (cell)*((char *)blk.data() + block);
    }
    st->Remove(sz - 1);
    return 1;
}

// bool:PopStackString(Stack:handle, buffer[], maxlength, &written = 0)
cell AMX_NATIVE_CALL amxx_pop_stack_string(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_stacks.size() || !g_stacks[idx]) return 0;
    AMXXArray *st = g_stacks[idx];
    int sz = st->Size();
    if (sz == 0) return 0;
    int bs = st->BlockSize();
    if (bs <= 0) return 0;
    std::vector<cell> blk(bs, 0);
    st->Get(sz - 1, blk.data());
    std::string s;
    for (int i = 0; i < bs; i++) {
        char c = (char)(unsigned char)blk[i];
        if (c == '\0') break;
        s += c;
    }
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    int maxlength = (int)params[3];
    amx_SetString(dest, s.c_str(), 0, 0, maxlength);
    int numParams = (int)(params[0] / sizeof(cell));
    if (numParams >= 4) {
        cell *written;
        amx_GetAddr(amx, params[4], &written);
        if (written) *written = (cell)s.length();
    }
    st->Remove(sz - 1);
    return 1;
}

// bool:PopStackArray(Stack:handle, any:buffer[], size = -1)
cell AMX_NATIVE_CALL amxx_pop_stack_array(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_stacks.size() || !g_stacks[idx]) return 0;
    AMXXArray *st = g_stacks[idx];
    int sz = st->Size();
    if (sz == 0) return 0;
    int bs = st->BlockSize();
    if (bs <= 0) return 0;
    std::vector<cell> blk(bs, 0);
    st->Get(sz - 1, blk.data());
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    int size = (int)params[3];
    int count = bs;
    if (size != -1 && size <= bs) count = size;
    for (int i = 0; i < count; i++) dest[i] = blk[i];
    st->Remove(sz - 1);
    return 1;
}

// bool:IsStackEmpty(Stack:handle)
cell AMX_NATIVE_CALL amxx_is_stack_empty(AMX *amx, cell *params)
{
    (void)amx;
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_stacks.size() || !g_stacks[idx]) return 1;
    return g_stacks[idx]->Size() == 0 ? 1 : 0;
}

// DestroyStack(&Stack:handle)
cell AMX_NATIVE_CALL amxx_destroy_stack(AMX *amx, cell *params)
{
    cell *ptr;
    amx_GetAddr(amx, params[1], &ptr);
    if (!ptr) return 0;
    int idx = (*ptr) - 1;
    if (idx < 0 || idx >= (int)g_stacks.size() || !g_stacks[idx]) return 0;
    delete g_stacks[idx];
    g_stacks[idx] = nullptr;
    *ptr = 0;
    return 1;
}

// ---- Sort standard aliases ----

// SortIntegers(array[], size, SortFunc:compare=Sort_Ascending)
// Uses std::sort directly (ascending or descending)
cell AMX_NATIVE_CALL amxx_sort_integers(AMX *amx, cell *params)
{
    cell *arr;
    amx_GetAddr(amx, params[1], &arr);
    int size = (int)params[2];
    cell compare = (params[0] / sizeof(cell) >= 3) ? params[3] : 0;
    if (size <= 0) return 0;
    if (compare == 0) {
        std::sort(arr, arr + size);
    } else {
        std::sort(arr, arr + size, std::greater<cell>());
    }
    return 1;
}

// SortFloats(Float:array[], size, SortFunc:compare=Sort_Ascending)
cell AMX_NATIVE_CALL amxx_sort_floats(AMX *amx, cell *params)
{
    cell *arr;
    amx_GetAddr(amx, params[1], &arr);
    int size = (int)params[2];
    cell compare = (params[0] / sizeof(cell) >= 3) ? params[3] : 0;
    if (size <= 0) return 0;
    if (compare == 0) {
        std::sort(arr, arr + size, [](cell a, cell b){ return amx_ctof(a) < amx_ctof(b); });
    } else {
        std::sort(arr, arr + size, [](cell a, cell b){ return amx_ctof(a) > amx_ctof(b); });
    }
    return 1;
}

// SortStrings(array[][], size, SortFunc:compare=Sort_Ascending, caseSensitive=true)
cell AMX_NATIVE_CALL amxx_sort_strings(AMX *amx, cell *params)
{
    // Not implemented: full 2D array sorting is skipped to avoid out-of-bounds access
    (void)amx; (void)params;
    return 0;
}

// Standard alias
cell AMX_NATIVE_CALL amxx_sort_custom_1d_alias(AMX *amx, cell *params)
{
    // Delegates to the actual sort_custom_1d implementation
    extern cell AMX_NATIVE_CALL amxx_sort_custom_1d(AMX *, cell *);
    return amxx_sort_custom_1d(amx, params);
}

cell AMX_NATIVE_CALL amxx_cellarray_create(AMX *amx, cell *params)
{
    int initialSize = params[1];
    
    AMXXCellArray *arr = new AMXXCellArray();
    arr->Create(initialSize);
    
    int idx = g_cellArrays.size();
    g_cellArrays.push_back(arr);
    
    return idx + 1;
}

cell AMX_NATIVE_CALL amxx_cellarray_destroy(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_cellArrays.size()) return 0;
    
    delete g_cellArrays[idx];
    g_cellArrays[idx] = nullptr;
    
    return 1;
}

cell AMX_NATIVE_CALL amxx_cellarray_push(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_cellArrays.size() || !g_cellArrays[idx]) return -1;
    
    return g_cellArrays[idx]->Push(params[2]);
}

cell AMX_NATIVE_CALL amxx_cellarray_get(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_cellArrays.size() || !g_cellArrays[idx]) return 0;
    
    return g_cellArrays[idx]->Get(params[2]);
}

cell AMX_NATIVE_CALL amxx_cellarray_set(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_cellArrays.size() || !g_cellArrays[idx]) return 0;
    
    return g_cellArrays[idx]->Set(params[2], params[3]) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_cellarray_remove(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_cellArrays.size() || !g_cellArrays[idx]) return 0;
    
    return g_cellArrays[idx]->Remove(params[2]) ? 1 : 0;
}

cell AMX_NATIVE_CALL amxx_cellarray_clear(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_cellArrays.size() || !g_cellArrays[idx]) return 0;
    
    g_cellArrays[idx]->Clear();
    return 1;
}

cell AMX_NATIVE_CALL amxx_cellarray_size(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_cellArrays.size() || !g_cellArrays[idx]) return 0;
    
    return g_cellArrays[idx]->Size();
}

// ==============================================
// Modern datastructs API
// ==============================================

// ---- Array sorting and searching ----

// ArraySort(Array:array, const comparefunc[], data[]="", data_size=0)
// Uses bubble sort; callback signature: sortcmp(Array:array, elem1, elem2, const data[], data_size)
cell AMX_NATIVE_CALL amxx_array_sort(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return 0;
    AMXXArray *arr = g_arrays[idx];

    cell *funcAddr;
    amx_GetAddr(amx, params[2], &funcAddr);
    char funcName[256];
    amx_GetString(funcName, funcAddr, 0, sizeof(funcName));

    int funcidx;
    if (amx_FindPublic(amx, funcName, &funcidx) != AMX_ERR_NONE)
        return 0;

    cell dataAddr = params[3];
    cell dataSize = (params[0] / sizeof(cell) >= 4) ? params[4] : 0;

    int size = arr->Size();
    for (int i = 0; i < size - 1; i++) {
        for (int j = 0; j < size - 1 - i; j++) {
            // Callback: sortcmp(array, elem1, elem2, data, data_size)
            // amx_Push is LIFO: push the last argument first
            amx_Push(amx, dataSize);
            amx_Push(amx, dataAddr);
            amx_Push(amx, j + 1);
            amx_Push(amx, j);
            amx_Push(amx, params[1]);  // array handle
            cell retval = 0;
            amx_Exec(amx, &retval, funcidx);
            if (retval > 0) {
                arr->Swap(j, j + 1);
            }
        }
    }
    return 1;
}

// ArraySortEx(Array:array, const comparefunc[], data[]="", data_size=0)
// Callback signature: sortcmp(Array:array, elem1, elem2)
cell AMX_NATIVE_CALL amxx_array_sortex(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return 0;
    AMXXArray *arr = g_arrays[idx];

    cell *funcAddr;
    amx_GetAddr(amx, params[2], &funcAddr);
    char funcName[256];
    amx_GetString(funcName, funcAddr, 0, sizeof(funcName));

    int funcidx;
    if (amx_FindPublic(amx, funcName, &funcidx) != AMX_ERR_NONE)
        return 0;

    int size = arr->Size();
    for (int i = 0; i < size - 1; i++) {
        for (int j = 0; j < size - 1 - i; j++) {
            amx_Push(amx, j + 1);
            amx_Push(amx, j);
            amx_Push(amx, params[1]);
            cell retval = 0;
            amx_Exec(amx, &retval, funcidx);
            if (retval > 0) {
                arr->Swap(j, j + 1);
            }
        }
    }
    return 1;
}

// Returns the index or -1
cell AMX_NATIVE_CALL amxx_array_find_string(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return -1;
    AMXXArray *arr = g_arrays[idx];

    cell *searchAddr;
    amx_GetAddr(amx, params[2], &searchAddr);
    char search[512];
    amx_GetString(search, searchAddr, 0, sizeof(search));

    int size = arr->Size();
    for (int i = 0; i < size; i++) {
        char buffer[512];
        if (arr->GetString(i, buffer, sizeof(buffer)) && strcmp(buffer, search) == 0)
            return i;
    }
    return -1;
}

// Returns the index or -1
cell AMX_NATIVE_CALL amxx_array_find_value(AMX *amx, cell *params)
{
    (void)amx;
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return -1;
    AMXXArray *arr = g_arrays[idx];

    cell value = params[2];
    int size = arr->Size();
    for (int i = 0; i < size; i++) {
        if (arr->GetCell(i) == value)
            return i;
    }
    return -1;
}

// SortADTArray(Array:array, SortOrder:order, SortType:type)
// Sorts an Array container in place
//   order: 0=Sort_Ascending, 1=Sort_Descending
//   type:  0=Sort_Integer, 1=Sort_Float, 2=Sort_String
// Compares by the block's first element and swaps whole blocks
cell AMX_NATIVE_CALL amxx_array_sort_adt(AMX *amx, cell *params)
{
    (void)amx;
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_arrays.size() || !g_arrays[idx]) return 0;
    AMXXArray *arr = g_arrays[idx];

    int order = (int)params[2];  // 0=asc, 1=desc
    int type = (int)params[3];   // 0=int, 1=float, 2=string

    int size = arr->Size();
    if (size <= 1) return 1;

    int bs = arr->BlockSize();
    if (bs <= 0) return 0;

    // Bubble sort with Swap, preserving block structure
    for (int i = 0; i < size - 1; i++) {
        for (int j = 0; j < size - 1 - i; j++) {
            bool swap = false;
            if (type == 1) {
                // float: compare each block's first element as float
                std::vector<cell> blkA(bs, 0), blkB(bs, 0);
                arr->Get(j, blkA.data());
                arr->Get(j + 1, blkB.data());
                float a = amx_ctof(blkA[0]);
                float b = amx_ctof(blkB[0]);
                swap = (order == 0) ? (a > b) : (a < b);
            } else if (type == 2) {
                // string: compare block contents as strings
                char bufA[512], bufB[512];
                arr->GetString(j, bufA, sizeof(bufA));
                arr->GetString(j + 1, bufB, sizeof(bufB));
                int cmp = strcmp(bufA, bufB);
                swap = (order == 0) ? (cmp > 0) : (cmp < 0);
            } else {
                // integer (default): compare each block's first element as int
                std::vector<cell> blkA(bs, 0), blkB(bs, 0);
                arr->Get(j, blkA.data());
                arr->Get(j + 1, blkB.data());
                cell a = blkA[0];
                cell b = blkB[0];
                swap = (order == 0) ? (a > b) : (a < b);
            }
            if (swap) {
                arr->Swap(j, j + 1);
            }
        }
    }
    return 1;
}

// ---- Trie Snapshot ----

struct TrieSnapshotData {
    std::vector<std::string> keys;
};

static std::vector<TrieSnapshotData*> g_trieSnapshots;

// Creates a snapshot and returns its handle
cell AMX_NATIVE_CALL amxx_trie_snapshot_create(AMX *amx, cell *params)
{
    (void)amx;
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_tries.size() || !g_tries[idx]) return 0;

    std::vector<TrieEntry> entries;
    g_tries[idx]->CollectEntries(entries);

    TrieSnapshotData *snap = new TrieSnapshotData();
    snap->keys.reserve(entries.size());
    for (auto &e : entries)
        snap->keys.push_back(e.key);

    g_trieSnapshots.push_back(snap);
    return (cell)g_trieSnapshots.size();
}

// Returns the number of keys
cell AMX_NATIVE_CALL amxx_trie_snapshot_length(AMX *amx, cell *params)
{
    (void)amx;
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_trieSnapshots.size() || !g_trieSnapshots[idx]) return 0;
    return (cell)g_trieSnapshots[idx]->keys.size();
}

// Returns the buffer size required for the key
cell AMX_NATIVE_CALL amxx_trie_snapshot_key_buffer_size(AMX *amx, cell *params)
{
    (void)amx;
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_trieSnapshots.size() || !g_trieSnapshots[idx]) return 0;
    auto &keys = g_trieSnapshots[idx]->keys;
    int keyIdx = (int)params[2];
    if (keyIdx < 0 || keyIdx >= (int)keys.size()) return 0;
    return (cell)(keys[keyIdx].length() + 1);
}

// TrieSnapshotGetKey(Snapshot:handle, index, buffer[], maxlength)
cell AMX_NATIVE_CALL amxx_trie_snapshot_get_key(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_trieSnapshots.size() || !g_trieSnapshots[idx]) return 0;
    auto &keys = g_trieSnapshots[idx]->keys;
    int keyIdx = (int)params[2];
    if (keyIdx < 0 || keyIdx >= (int)keys.size()) return 0;

    cell *dest;
    amx_GetAddr(amx, params[3], &dest);
    amx_SetString(dest, keys[keyIdx].c_str(), 0, 0, (int)params[4]);
    return 1;
}

// TrieSnapshotDestroy(&Snapshot:handle)
cell AMX_NATIVE_CALL amxx_trie_snapshot_destroy(AMX *amx, cell *params)
{
    cell *ptr;
    amx_GetAddr(amx, params[1], &ptr);
    if (!ptr) return 0;
    int idx = (*ptr) - 1;
    if (idx < 0 || idx >= (int)g_trieSnapshots.size() || !g_trieSnapshots[idx]) return 0;
    delete g_trieSnapshots[idx];
    g_trieSnapshots[idx] = nullptr;
    *ptr = 0;
    return 1;
}

// ---- Trie Iterator ----

struct TrieIterData {
    std::vector<TrieEntry> entries;
    size_t index;
    int trieSize;
};

static std::vector<TrieIterData*> g_trieIters;

// Creates an iterator
cell AMX_NATIVE_CALL amxx_trie_iter_create(AMX *amx, cell *params)
{
    (void)amx;
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_tries.size() || !g_tries[idx]) return 0;

    TrieIterData *iter = new TrieIterData();
    iter->index = 0;
    iter->trieSize = g_tries[idx]->Count();
    g_tries[idx]->CollectEntries(iter->entries);

    g_trieIters.push_back(iter);
    return (cell)g_trieIters.size();
}

// Returns whether iteration has finished
cell AMX_NATIVE_CALL amxx_trie_iter_ended(AMX *amx, cell *params)
{
    (void)amx;
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_trieIters.size() || !g_trieIters[idx]) return 1;
    return g_trieIters[idx]->index >= g_trieIters[idx]->entries.size() ? 1 : 0;
}

// Advances to the next entry
cell AMX_NATIVE_CALL amxx_trie_iter_next(AMX *amx, cell *params)
{
    (void)amx;
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_trieIters.size() || !g_trieIters[idx]) return 0;
    TrieIterData *iter = g_trieIters[idx];
    if (iter->index >= iter->entries.size()) return 0;
    iter->index++;
    return 1;
}

// TrieIterGetKey(TrieIter:handle, key[], outputsize)
cell AMX_NATIVE_CALL amxx_trie_iter_get_key(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_trieIters.size() || !g_trieIters[idx]) return 0;
    TrieIterData *iter = g_trieIters[idx];
    if (iter->index >= iter->entries.size()) {
        cell *dest;
        amx_GetAddr(amx, params[2], &dest);
        if (dest) amx_SetString(dest, "", 0, 0, (int)params[3]);
        return 0;
    }
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    amx_SetString(dest, iter->entries[iter->index].key.c_str(), 0, 0, (int)params[3]);
    return 1;
}

// Returns the trie size
cell AMX_NATIVE_CALL amxx_trie_iter_get_size(AMX *amx, cell *params)
{
    (void)amx;
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_trieIters.size() || !g_trieIters[idx]) return 0;
    return g_trieIters[idx]->trieSize;
}

// TrieIterGetCell(TrieIter:handle, &any:value)
cell AMX_NATIVE_CALL amxx_trie_iter_get_cell(AMX *amx, cell *params)
{
    (void)amx;
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_trieIters.size() || !g_trieIters[idx]) return 0;
    TrieIterData *iter = g_trieIters[idx];
    if (iter->index >= iter->entries.size()) return 0;
    if (iter->entries[iter->index].type != TRIE_VALUE_CELL) return 0;
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    if (dest) *dest = iter->entries[iter->index].value;
    return 1;
}

// TrieIterGetString(TrieIter:handle, buffer[], outputsize, &size=0)
cell AMX_NATIVE_CALL amxx_trie_iter_get_string(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_trieIters.size() || !g_trieIters[idx]) return 0;
    TrieIterData *iter = g_trieIters[idx];
    if (iter->index >= iter->entries.size()) return 0;
    if (iter->entries[iter->index].type != TRIE_VALUE_STRING) return 0;

    const char *str = (const char *)(uintptr_t)iter->entries[iter->index].value;
    if (!str) return 0;
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    amx_SetString(dest, str, 0, 0, (int)params[3]);

    int numParams = (int)(params[0] / sizeof(cell));
    if (numParams >= 4) {
        cell *sizeOut;
        amx_GetAddr(amx, params[4], &sizeOut);
        if (sizeOut) *sizeOut = (cell)strlen(str);
    }
    return 1;
}

// TrieIterGetArray(TrieIter:handle, array[], outputsize, &size=0)
cell AMX_NATIVE_CALL amxx_trie_iter_get_array(AMX *amx, cell *params)
{
    int idx = params[1] - 1;
    if (idx < 0 || idx >= (int)g_trieIters.size() || !g_trieIters[idx]) return 0;
    TrieIterData *iter = g_trieIters[idx];
    if (iter->index >= iter->entries.size()) return 0;
    if (iter->entries[iter->index].type != TRIE_VALUE_ARRAY) return 0;

    cell *src = (cell *)(uintptr_t)iter->entries[iter->index].value;
    if (!src) return 0;
    cell *dest;
    amx_GetAddr(amx, params[2], &dest);
    int outputSize = (int)params[3];
    if (outputSize < 0) return 0;
    for (int i = 0; i < outputSize; i++)
        dest[i] = src[i];

    int numParams = (int)(params[0] / sizeof(cell));
    if (numParams >= 4) {
        cell *sizeOut;
        amx_GetAddr(amx, params[4], &sizeOut);
        if (sizeOut) *sizeOut = outputSize;
    }
    return 1;
}

// TrieIterDestroy(&TrieIter:handle)
cell AMX_NATIVE_CALL amxx_trie_iter_destroy(AMX *amx, cell *params)
{
    cell *ptr;
    amx_GetAddr(amx, params[1], &ptr);
    if (!ptr) return 0;
    int idx = (*ptr) - 1;
    if (idx < 0 || idx >= (int)g_trieIters.size() || !g_trieIters[idx]) return 0;
    delete g_trieIters[idx];
    g_trieIters[idx] = nullptr;
    *ptr = 0;
    return 1;
}

AMX_NATIVE_INFO datastruct_natives[] = {
    // Original API
    {"ArrayCreate", amxx_array_create},
    {"ArrayDestroy", amxx_array_destroy},
    {"ArrayPush", amxx_array_push},
    {"ArrayPushCell", amxx_array_push_cell},
    {"ArrayPushString", amxx_array_push_string},
    {"ArrayGet", amxx_array_get},
    {"ArrayGetCell", amxx_array_get_cell},
    {"ArrayGetString", amxx_array_get_string},
    {"ArraySet", amxx_array_set},
    {"ArraySetCell", amxx_array_set_cell},
    {"ArraySetString", amxx_array_set_string},
    {"ArrayRemove", amxx_array_remove},
    {"ArrayClear", amxx_array_clear},
    {"ArraySize", amxx_array_size},
    {"ArrayBlockSize", amxx_array_blocksize},
    {"ArrayGetArray", amxx_array_get_array},
    {"ArrayPushArray", amxx_array_push_array},
    {"ArraySetArray", amxx_array_set_array},
    {"ArrayGetStringHandle", amxx_array_get_string_handle},
    {"TrieCreate", amxx_trie_create},
    {"TrieDestroy", amxx_trie_destroy},
    {"TrieInsert", amxx_trie_insert},
    {"TrieDelete", amxx_trie_delete},
    {"TrieRetrieve", amxx_trie_retrieve},
    {"TrieKeyExists", amxx_trie_key_exists},
    {"TrieClear", amxx_trie_clear},
    {"TrieCount", amxx_trie_count},
    {"CellArrayCreate", amxx_cellarray_create},
    {"CellArrayDestroy", amxx_cellarray_destroy},
    {"CellArrayPush", amxx_cellarray_push},
    {"CellArrayGet", amxx_cellarray_get},
    {"CellArraySet", amxx_cellarray_set},
    {"CellArrayRemove", amxx_cellarray_remove},
    {"CellArrayClear", amxx_cellarray_clear},
    {"CellArraySize", amxx_cellarray_size},

    // Additional: standard names for advanced Array operations
    {"ArrayInsertArrayBefore", amxx_array_insert_before},
    {"ArrayInsertArrayAfter",  amxx_array_insert_after},
    {"ArrayInsertCellBefore",  amxx_array_insert_cell_before},
    {"ArrayInsertCellAfter",   amxx_array_insert_cell_after},
    {"ArrayInsertStringBefore", amxx_array_insert_string_before},
    {"ArrayInsertStringAfter",  amxx_array_insert_string_after},
    {"ArraySwap", amxx_array_swap},
    {"ArrayResize", amxx_array_resize},
    {"ArrayClone", amxx_array_clone},
    {"ArrayDeleteItem", amxx_array_delete_item},  // standard name (alias of ArrayRemove)
    {"ArrayGetSize", amxx_array_get_size},        // alias with capital S

    // Additional: Trie typed API
    {"TrieSetCell", amxx_trie_set_cell},
    {"TrieGetCell", amxx_trie_get_cell},
    {"TrieSetString", amxx_trie_set_string},
    {"TrieGetString", amxx_trie_get_string},
    {"TrieSetArray", amxx_trie_set_array},
    {"TrieGetArray", amxx_trie_get_array},
    {"TrieGetSize", amxx_trie_get_size},          // standard name (alias of TrieCount)
    {"TrieDeleteKey", amxx_trie_delete_key},      // standard name (alias of TrieDelete)

    // Additional: Stack ADT
    {"StackCreate", amxx_stack_create},
    {"StackDestroy", amxx_stack_destroy},
    {"StackPush", amxx_stack_push},
    {"StackPop", amxx_stack_pop},
    {"StackTop", amxx_stack_top},

    // Modern CellStack API (cellstack.inc)
    {"CreateStack",     amxx_create_stack},
    {"PushStackCell",   amxx_push_stack_cell},
    {"PushStackString", amxx_push_stack_string},
    {"PushStackArray",  amxx_push_stack_array},
    {"PopStackCell",    amxx_pop_stack_cell},
    {"PopStackString",  amxx_pop_stack_string},
    {"PopStackArray",   amxx_pop_stack_array},
    {"IsStackEmpty",    amxx_is_stack_empty},
    {"DestroyStack",    amxx_destroy_stack},

    // Additional: Sort standard names
    {"SortIntegers", amxx_sort_integers},
    {"SortFloats", amxx_sort_floats},
    {"SortStrings", amxx_sort_strings},
    {"SortCustom1D", amxx_sort_custom_1d_alias},

    // Modern Array API
    {"ArraySort", amxx_array_sort},
    {"ArraySortEx", amxx_array_sortex},
    {"ArrayFindString", amxx_array_find_string},
    {"ArrayFindValue", amxx_array_find_value},

    // sorting.inc: SortADTArray — in-place Array container sorting
    {"SortADTArray", amxx_array_sort_adt},

    // Trie Snapshot
    {"TrieSnapshotCreate", amxx_trie_snapshot_create},
    {"TrieSnapshotLength", amxx_trie_snapshot_length},
    {"TrieSnapshotKeyBufferSize", amxx_trie_snapshot_key_buffer_size},
    {"TrieSnapshotGetKey", amxx_trie_snapshot_get_key},
    {"TrieSnapshotDestroy", amxx_trie_snapshot_destroy},

    // Trie Iterator
    {"TrieIterCreate", amxx_trie_iter_create},
    {"TrieIterEnded", amxx_trie_iter_ended},
    {"TrieIterNext", amxx_trie_iter_next},
    {"TrieIterGetKey", amxx_trie_iter_get_key},
    {"TrieIterGetSize", amxx_trie_iter_get_size},
    {"TrieIterGetCell", amxx_trie_iter_get_cell},
    {"TrieIterGetString", amxx_trie_iter_get_string},
    {"TrieIterGetArray", amxx_trie_iter_get_array},
    {"TrieIterDestroy", amxx_trie_iter_destroy},

    {nullptr, nullptr}
};

void RegisterDatastructNatives(AMX *amx)
{
    amx_Register(amx, datastruct_natives, -1);
}

void CleanupDatastructs()
{
    for (auto arr : g_arrays) {
        delete arr;
    }
    g_arrays.clear();
    for (auto trie : g_tries) {
        delete trie;
    }
    g_tries.clear();
    for (auto ca : g_cellArrays) {
        delete ca;
    }
    g_cellArrays.clear();
    for (auto st : g_stacks) {
        delete st;
    }
    g_stacks.clear();
    for (auto snap : g_trieSnapshots) {
        delete snap;
    }
    g_trieSnapshots.clear();
    for (auto iter : g_trieIters) {
        delete iter;
    }
    g_trieIters.clear();
}
