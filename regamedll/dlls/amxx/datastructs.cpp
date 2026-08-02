#include "precompiled.h"
#include "datastructs.h"

AMXXArray::AMXXArray() : m_blockSize(0) {}

AMXXArray::~AMXXArray() {
    Destroy();
}

bool AMXXArray::Create(int blockSize, int initialSize) {
    m_blockSize = blockSize;
    m_data.reserve(initialSize * blockSize);
    return true;
}

void AMXXArray::Destroy() {
    m_data.clear();
    m_blockSize = 0;
}

int AMXXArray::Push(const cell *data) {
    if (m_blockSize <= 0 || !data) return -1;
    
    int idx = m_data.size() / m_blockSize;
    for (int i = 0; i < m_blockSize; i++) {
        m_data.push_back(data[i]);
    }
    return idx;
}

int AMXXArray::PushCell(cell value) {
    if (m_blockSize != 1) return -1;
    m_data.push_back(value);
    return m_data.size() - 1;
}

int AMXXArray::PushString(const char *str) {
    if (!str) return -1;
    
    int len = strlen(str) + 1;
    int idx = m_data.size() / m_blockSize;
    
    for (int i = 0; i < len; i++) {
        m_data.push_back((cell)(unsigned char)str[i]);
    }
    
    // Pad to blockSize alignment so each element occupies a fixed-size block
    size_t remainder = m_data.size() % m_blockSize;
    if (remainder > 0) {
        for (size_t i = remainder; i < m_blockSize; i++) {
            m_data.push_back(0);
        }
    }
    
    return idx;
}

bool AMXXArray::Get(int index, cell *data) {
    if (m_blockSize <= 0 || index < 0 || !data) return false;
    int start = index * m_blockSize;
    if (start + m_blockSize > (int)m_data.size()) return false;
    
    for (int i = 0; i < m_blockSize; i++) {
        data[i] = m_data[start + i];
    }
    return true;
}

cell AMXXArray::GetCell(int index) {
    if (m_blockSize != 1 || index < 0) return 0;
    if (index >= (int)m_data.size()) return 0;
    return m_data[index];
}

bool AMXXArray::GetString(int index, char *buffer, int maxlen) {
    if (!buffer || maxlen <= 0) return false;
    
    int start = index * m_blockSize;
    if (start >= (int)m_data.size()) return false;
    
    int i = 0;
    while (start + i < (int)m_data.size() && i < maxlen - 1) {
        buffer[i] = (char)(unsigned char)m_data[start + i];
        if (buffer[i] == '\0') break;
        i++;
    }
    buffer[i] = '\0';
    return true;
}

bool AMXXArray::Set(int index, const cell *data) {
    if (m_blockSize <= 0 || index < 0 || !data) return false;
    int start = index * m_blockSize;
    if (start + m_blockSize > (int)m_data.size()) return false;
    
    for (int i = 0; i < m_blockSize; i++) {
        m_data[start + i] = data[i];
    }
    return true;
}

bool AMXXArray::SetCell(int index, cell value) {
    if (m_blockSize != 1 || index < 0) return false;
    if (index >= (int)m_data.size()) return false;
    m_data[index] = value;
    return true;
}

bool AMXXArray::SetString(int index, const char *str) {
    if (!str) return false;
    
    int start = index * m_blockSize;
    if (start >= (int)m_data.size()) return false;
    
    int i = 0;
    while (start + i < (int)m_data.size()) {
        m_data[start + i] = (cell)(unsigned char)str[i];
        if (str[i] == '\0') break;
        i++;
    }
    return true;
}

bool AMXXArray::Remove(int index) {
    if (m_blockSize <= 0 || index < 0) return false;
    int start = index * m_blockSize;
    if (start >= (int)m_data.size()) return false;

    m_data.erase(m_data.begin() + start, m_data.begin() + start + m_blockSize);
    return true;
}

bool AMXXArray::Insert(int index, const cell *data) {
    if (m_blockSize <= 0 || index < 0 || !data) return false;
    int size = Size();
    if (index > size) return false;  // index == size is allowed and acts like push_back
    int start = index * m_blockSize;
    m_data.insert(m_data.begin() + start, m_blockSize, 0);
    for (int i = 0; i < m_blockSize; i++) {
        m_data[start + i] = data[i];
    }
    return true;
}

bool AMXXArray::Swap(int a, int b) {
    if (m_blockSize <= 0) return false;
    int size = Size();
    if (a < 0 || a >= size || b < 0 || b >= size) return false;
    int as = a * m_blockSize;
    int bs = b * m_blockSize;
    for (int i = 0; i < m_blockSize; i++) {
        cell tmp = m_data[as + i];
        m_data[as + i] = m_data[bs + i];
        m_data[bs + i] = tmp;
    }
    return true;
}

bool AMXXArray::Resize(int newSize, cell fill) {
    if (m_blockSize <= 0 || newSize < 0) return false;
    int curSize = Size();
    if (newSize == curSize) return true;
    if (newSize > curSize) {
        int extra = (newSize - curSize) * m_blockSize;
        m_data.reserve(m_data.size() + extra);
        for (int i = 0; i < extra; i++) m_data.push_back(fill);
    } else {
        m_data.resize(newSize * m_blockSize);
    }
    return true;
}

AMXXArray *AMXXArray::Clone() const {
    AMXXArray *copy = new AMXXArray();
    copy->m_blockSize = m_blockSize;
    copy->m_data = m_data;
    return copy;
}

void AMXXArray::Clear() {
    m_data.clear();
}

int AMXXArray::Size() const {
    if (m_blockSize <= 0) return 0;
    return m_data.size() / m_blockSize;
}

int AMXXArray::BlockSize() const {
    return m_blockSize;
}

AMXXTrie::AMXXTrie() : m_root(nullptr), m_count(0) {}

AMXXTrie::~AMXXTrie() {
    Destroy();
}

bool AMXXTrie::Create() {
    if (m_root) return false;
    m_root = new TrieNode();
    m_count = 0;
    return true;
}

void AMXXTrie::Destroy() {
    delete m_root;
    m_root = nullptr;
    m_count = 0;
}

bool AMXXTrie::Insert(const char *key, cell value, int type)
{
    if (!m_root || !key) return false;
    TrieNode *node = m_root;
    for (size_t i = 0; key[i]; i++) {
        char c = key[i];  // case-sensitive
        if (!node->children[(unsigned char)c]) {
            node->children[(unsigned char)c] = new TrieNode();
        }
        node = node->children[(unsigned char)c];
    }
    if (!node->hasValue) m_count++;
    node->value = value;
    node->hasValue = true;
    node->type = type;
    return true;
}

bool AMXXTrie::Delete(const char *key) {
    if (!m_root || !key) return false;
    
    TrieNode *node = m_root;
    for (size_t i = 0; key[i]; i++) {
        char c = tolower(key[i]);
        if (!node->children[c]) return false;
        node = node->children[c];
    }
    
    if (!node->hasValue) return false;
    node->hasValue = false;
    m_count--;
    return true;
}

bool AMXXTrie::Retrieve(const char *key, cell *value)
{
    if (!m_root || !key || !value) return false;
    TrieNode *node = m_root;
    for (size_t i = 0; key[i]; i++) {
        char c = key[i];  // case-sensitive
        if (!node->children[(unsigned char)c]) return false;
        node = node->children[(unsigned char)c];
    }
    if (!node->hasValue) return false;
    *value = node->value;
    return true;
}

bool AMXXTrie::Retrieve(const char *key, cell *value, int *type)
{
    if (!m_root || !key || !value) return false;
    TrieNode *node = m_root;
    for (size_t i = 0; key[i]; i++) {
        char c = key[i];
        if (!node->children[(unsigned char)c]) return false;
        node = node->children[(unsigned char)c];
    }
    if (!node->hasValue) return false;
    *value = node->value;
    if (type) *type = node->type;
    return true;
}

void AMXXTrie::CollectNode(TrieNode *node, const std::string &prefix, std::vector<TrieEntry> &entries) const
{
    if (!node) return;
    if (node->hasValue) {
        TrieEntry entry;
        entry.key = prefix;
        entry.value = node->value;
        entry.type = node->type;
        entries.push_back(entry);
    }
    for (auto &child : node->children) {
        CollectNode(child.second, prefix + std::string(1, child.first), entries);
    }
}

void AMXXTrie::CollectEntries(std::vector<TrieEntry> &entries) const
{
    entries.clear();
    if (!m_root) return;
    CollectNode(m_root, std::string(), entries);
}

bool AMXXTrie::KeyExists(const char *key)
{
    if (!m_root || !key) return false;
    TrieNode *node = m_root;
    for (size_t i = 0; key[i]; i++) {
        char c = key[i];
        if (!node->children[(unsigned char)c]) return false;
        node = node->children[(unsigned char)c];
    }
    return node->hasValue;
}

void AMXXTrie::Clear() {
    Destroy();
    Create();
}

int AMXXTrie::Count() const {
    return m_count;
}

AMXXCellArray::AMXXCellArray() {}

AMXXCellArray::~AMXXCellArray() {
    Destroy();
}

bool AMXXCellArray::Create(int initialSize) {
    m_data.reserve(initialSize);
    return true;
}

void AMXXCellArray::Destroy() {
    m_data.clear();
}

int AMXXCellArray::Push(cell value) {
    m_data.push_back(value);
    return m_data.size() - 1;
}

cell AMXXCellArray::Get(int index) {
    if (index < 0 || index >= (int)m_data.size()) return 0;
    return m_data[index];
}

bool AMXXCellArray::Set(int index, cell value) {
    if (index < 0 || index >= (int)m_data.size()) return false;
    m_data[index] = value;
    return true;
}

bool AMXXCellArray::Remove(int index) {
    if (index < 0 || index >= (int)m_data.size()) return false;
    m_data.erase(m_data.begin() + index);
    return true;
}

void AMXXCellArray::Clear() {
    m_data.clear();
}

int AMXXCellArray::Size() const {
    return m_data.size();
}