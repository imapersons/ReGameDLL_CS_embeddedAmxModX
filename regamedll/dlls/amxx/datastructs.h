#pragma once
#ifndef AMXX_DATASTRUCTS_H
#define AMXX_DATASTRUCTS_H

#include "amx.h"
#include <vector>
#include <map>
#include <string>

class AMXXArray {
public:
    AMXXArray();
    ~AMXXArray();

    bool Create(int blockSize, int initialSize = 0);
    void Destroy();

    int Push(const cell *data);
    int PushCell(cell value);
    int PushString(const char *str);

    bool Get(int index, cell *data);
    cell GetCell(int index);
    bool GetString(int index, char *buffer, int maxlen);

    bool Set(int index, const cell *data);
    bool SetCell(int index, cell value);
    bool SetString(int index, const char *str);

    bool Remove(int index);
    bool Insert(int index, const cell *data);   // base for InsertCellArrayBefore
    bool Swap(int a, int b);
    bool Resize(int newSize, cell fill = 0);
    void Clear();
    int Size() const;
    int BlockSize() const;
    AMXXArray *Clone() const;

private:
    std::vector<cell> m_data;
    int m_blockSize;
};

// Trie value types
#define TRIE_VALUE_CELL   0
#define TRIE_VALUE_STRING 1
#define TRIE_VALUE_ARRAY  2

// Trie key/value entry (used by snapshot/iterator)
struct TrieEntry {
    std::string key;
    cell value;
    int type;
};

struct TrieNode {
    std::map<char, TrieNode*> children;
    cell value;
    bool hasValue;
    int type;

    TrieNode() : value(0), hasValue(false), type(TRIE_VALUE_CELL) {}
    ~TrieNode() {
        for (auto &pair : children) {
            delete pair.second;
        }
    }
};

class AMXXTrie {
public:
    AMXXTrie();
    ~AMXXTrie();

    bool Create();
    void Destroy();

    bool Insert(const char *key, cell value, int type = TRIE_VALUE_CELL);
    bool Delete(const char *key);
    bool Retrieve(const char *key, cell *value);
    bool Retrieve(const char *key, cell *value, int *type);
    bool KeyExists(const char *key);

    void Clear();
    int Count() const;

    // Collects all key/value pairs (used by snapshot/iterator)
    void CollectEntries(std::vector<TrieEntry> &entries) const;

private:
    TrieNode *m_root;
    int m_count;

    void CollectNode(TrieNode *node, const std::string &prefix, std::vector<TrieEntry> &entries) const;
};

class AMXXCellArray {
public:
    AMXXCellArray();
    ~AMXXCellArray();
    
    bool Create(int initialSize = 0);
    void Destroy();
    
    int Push(cell value);
    cell Get(int index);
    bool Set(int index, cell value);
    bool Remove(int index);
    void Clear();
    int Size() const;
    
private:
    std::vector<cell> m_data;
};

#endif