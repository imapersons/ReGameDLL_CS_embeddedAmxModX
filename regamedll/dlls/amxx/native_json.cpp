// native_json.cpp
//
// 轻量 JSON 存储 API (102 个 native = 58 旧版 + 44 新版 json.inc) —— 为嵌入式环境实现的 AMXX JSON 模块。
// 不依赖第三方 JSON 库, 内置递归下降解析器与序列化器。
//
// 句柄模型 (copy-on-view):
//   - json_parse / json_create / json_copy / json_load_file 创建根句柄 (isRoot=true, 无父节点)。
//   - json_object_get_member / json_array_get_item 等返回"视图句柄" (isRoot=false),
//     视图是成员/元素值的深拷贝, 修改视图后需用
//     json_object_set_member / json_array_push_item 等写回父节点。
//   - 句柄表 1-based: 0 恒为无效句柄。
//   - json_delete 只允许作用于根句柄 (会级联释放其全部后代视图)。

#include "precompiled.h"
#include "native_json.h"
#include "amx.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cfloat>
#include <cmath>
#include <string>
#include <vector>
#include <utility>

// JSON 值类型 (与脚本侧 JSONType 常量一致)
enum JsonType
{
	JSON_NULL   = 0,
	JSON_BOOL   = 1,
	JSON_INT    = 2,
	JSON_FLOAT  = 3,
	JSON_STRING = 4,
	JSON_ARRAY  = 5,
	JSON_OBJECT = 6,
};

// JSON 值节点 (值树)
struct JsonValue
{
	JsonType type = JSON_NULL;
	bool boolVal = false;
	int intVal = 0;
	double floatVal = 0.0;
	std::string stringVal;
	std::vector<JsonValue> arrayItems;                                   // 数组元素
	std::vector<std::pair<std::string, JsonValue>> objectMembers;        // 对象成员
};

// JSON 句柄管理
struct JsonHandle
{
	JsonValue value;
	bool isRoot;         // 是否为根节点 (可删除)
	JsonHandle *parent;  // 父节点指针 (句柄树)
	std::string path;    // 调试用, 如 /root/foo/0

	JsonHandle()
		: isRoot(true), parent(nullptr), path("/root")
	{
	}
};

// 脚本侧新版 JSONType 枚举 (与 amxmodx-master json.inc 一致)
enum ScriptJsonType
{
	SCRIPT_JSON_ERROR  = -1,
	SCRIPT_JSON_NULL   = 1,
	SCRIPT_JSON_STRING = 2,
	SCRIPT_JSON_NUMBER = 3,
	SCRIPT_JSON_OBJECT = 4,
	SCRIPT_JSON_ARRAY  = 5,
	SCRIPT_JSON_BOOL   = 6,
};

// 内部类型 -> 脚本侧新版 JSONType
static int ScriptTypeOf(const JsonValue &v)
{
	switch (v.type)
	{
		case JSON_NULL:   return SCRIPT_JSON_NULL;
		case JSON_STRING: return SCRIPT_JSON_STRING;
		case JSON_BOOL:   return SCRIPT_JSON_BOOL;
		case JSON_INT:
		case JSON_FLOAT:  return SCRIPT_JSON_NUMBER;
		case JSON_OBJECT: return SCRIPT_JSON_OBJECT;
		case JSON_ARRAY:  return SCRIPT_JSON_ARRAY;
	}
	return SCRIPT_JSON_ERROR;
}

// 句柄表: 1-based, slot 0 恒为 nullptr (无效句柄)
static std::vector<JsonHandle*> g_jsonHandles;
static std::vector<int> g_freeSlots;

// ---------------------------------------------------------------- 句柄表管理

static void EnsureJsonTable()
{
	if (g_jsonHandles.empty())
		g_jsonHandles.push_back(nullptr);
}

static int AllocJsonHandle(JsonHandle *h)
{
	EnsureJsonTable();
	int id;
	if (!g_freeSlots.empty())
	{
		id = g_freeSlots.back();
		g_freeSlots.pop_back();
		g_jsonHandles[id] = h;
	}
	else
	{
		id = (int)g_jsonHandles.size();
		g_jsonHandles.push_back(h);
	}
	return id;
}

static JsonHandle *GetJsonHandle(int id)
{
	if (g_jsonHandles.empty() || id <= 0 || id >= (int)g_jsonHandles.size())
		return nullptr;
	return g_jsonHandles[id];
}

static int FindHandleId(JsonHandle *h)
{
	EnsureJsonTable();
	for (size_t i = 1; i < g_jsonHandles.size(); i++)
	{
		if (g_jsonHandles[i] == h)
			return (int)i;
	}
	return 0;
}

// 沿 parent 链上溯到根句柄
static JsonHandle *GetRootHandle(JsonHandle *h)
{
	JsonHandle *cur = h;
	while (cur->parent)
		cur = cur->parent;
	return cur;
}

void ResetJsonHandles()
{
	EnsureJsonTable();
	for (size_t i = 0; i < g_jsonHandles.size(); i++)
		delete g_jsonHandles[i];
	g_jsonHandles.clear();
	g_freeSlots.clear();
	g_jsonHandles.push_back(nullptr);
}

// 释放指定句柄及其全部后代视图, 返回是否成功 (0/-1 均视为无效句柄)
static bool FreeJsonHandleTree(int id)
{
	JsonHandle *h = GetJsonHandle(id);
	if (!h)
		return false;
	EnsureJsonTable();
	std::vector<int> toFree;
	toFree.push_back(id);
	for (size_t i = 1; i < g_jsonHandles.size(); i++)
	{
		JsonHandle *cur = g_jsonHandles[i];
		if (!cur || cur == h)
			continue;
		if (GetRootHandle(cur) == h)
			toFree.push_back((int)i);
	}
	for (size_t i = 0; i < toFree.size(); i++)
	{
		int fid = toFree[i];
		delete g_jsonHandles[fid];
		g_jsonHandles[fid] = nullptr;
		g_freeSlots.push_back(fid);
	}
	return true;
}

// ---------------------------------------------------------------- Pawn 字符串助手

static std::string ReadAmxString(AMX *amx, cell param)
{
	cell *addr = nullptr;
	amx_GetAddr(amx, param, &addr);
	if (!addr)
		return std::string();
	int len = 0;
	amx_StrLen(addr, &len);
	if (len <= 0)
		return std::string();
	std::string result;
	result.resize((size_t)len);
	amx_GetString(&result[0], addr, 0, (size_t)len + 1);
	return result;
}

static int WriteAmxString(AMX *amx, cell param, const char *str, int maxLen)
{
	cell *dest = nullptr;
	amx_GetAddr(amx, param, &dest);
	if (!dest)
		return 0;
	if (maxLen > 1)
	{
		size_t len = strlen(str);
		amx_SetString(dest, str, 0, 0, (size_t)maxLen);
		return (int)(len < (size_t)maxLen ? len : (size_t)maxLen - 1);
	}
	if (maxLen > 0)
		*dest = 0;
	return 0;
}

// ---------------------------------------------------------------- 值树深拷贝

static JsonValue CloneValue(const JsonValue &src)
{
	JsonValue v;
	v.type = src.type;
	v.boolVal = src.boolVal;
	v.intVal = src.intVal;
	v.floatVal = src.floatVal;
	v.stringVal = src.stringVal;
	v.arrayItems.reserve(src.arrayItems.size());
	for (size_t i = 0; i < src.arrayItems.size(); i++)
		v.arrayItems.push_back(CloneValue(src.arrayItems[i]));
	v.objectMembers.reserve(src.objectMembers.size());
	for (size_t i = 0; i < src.objectMembers.size(); i++)
		v.objectMembers.push_back(std::make_pair(src.objectMembers[i].first, CloneValue(src.objectMembers[i].second)));
	return v;
}

// ---------------------------------------------------------------- JSON 解析器

struct JsonParser
{
	const char *s;
	size_t pos;
	size_t len;
};

static bool ParseWhitespace(JsonParser &p)
{
	while (p.pos < p.len)
	{
		char c = p.s[p.pos];
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
			p.pos++;
		else
			break;
	}
	return p.pos < p.len;
}

static void AppendUtf8(std::string &out, unsigned int cp)
{
	if (cp < 0x80)
	{
		out += (char)cp;
	}
	else if (cp < 0x800)
	{
		out += (char)(0xC0 | (cp >> 6));
		out += (char)(0x80 | (cp & 0x3F));
	}
	else if (cp < 0x10000)
	{
		out += (char)(0xE0 | (cp >> 12));
		out += (char)(0x80 | ((cp >> 6) & 0x3F));
		out += (char)(0x80 | (cp & 0x3F));
	}
	else
	{
		out += (char)(0xF0 | (cp >> 18));
		out += (char)(0x80 | ((cp >> 12) & 0x3F));
		out += (char)(0x80 | ((cp >> 6) & 0x3F));
		out += (char)(0x80 | (cp & 0x3F));
	}
}

static bool ParseHex4(JsonParser &p, unsigned int &cp)
{
	if (p.pos + 4 > p.len)
		return false;
	cp = 0;
	for (int i = 0; i < 4; i++)
	{
		char c = p.s[p.pos++];
		cp <<= 4;
		if (c >= '0' && c <= '9')
			cp |= (unsigned int)(c - '0');
		else if (c >= 'a' && c <= 'f')
			cp |= (unsigned int)(c - 'a' + 10);
		else if (c >= 'A' && c <= 'F')
			cp |= (unsigned int)(c - 'A' + 10);
		else
			return false;
	}
	return true;
}

static bool ParseString(JsonParser &p, std::string &out)
{
	if (p.pos >= p.len || p.s[p.pos] != '"')
		return false;
	p.pos++;
	out.clear();
	while (p.pos < p.len)
	{
		unsigned char c = (unsigned char)p.s[p.pos++];
		if (c == '"')
			return true;
		if (c == '\\')
		{
			if (p.pos >= p.len)
				return false;
			char e = p.s[p.pos++];
			switch (e)
			{
				case '"':  out += '"';  break;
				case '\\': out += '\\'; break;
				case '/':  out += '/';  break;
				case 'b':  out += '\b'; break;
				case 'f':  out += '\f'; break;
				case 'n':  out += '\n'; break;
				case 'r':  out += '\r'; break;
				case 't':  out += '\t'; break;
				case 'u':
				{
					unsigned int cp;
					if (!ParseHex4(p, cp))
						return false;
					// 代理对: 高代理项后紧跟 \uXXXX 低代理项
					if (cp >= 0xD800 && cp <= 0xDBFF &&
						p.pos + 2 <= p.len && p.s[p.pos] == '\\' && p.s[p.pos + 1] == 'u')
					{
						p.pos += 2;
						unsigned int low;
						if (ParseHex4(p, low) && low >= 0xDC00 && low <= 0xDFFF)
							cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
					}
					AppendUtf8(out, cp);
					break;
				}
				default:
					return false; // 非法转义
			}
		}
		else if (c < 0x20)
		{
			return false; // JSON 字符串不允许裸控制字符
		}
		else
		{
			out += (char)c;
		}
	}
	return false; // 字符串未闭合
}

static bool ParseNumber(JsonParser &p, JsonValue &v)
{
	size_t start = p.pos;
	if (p.pos < p.len && p.s[p.pos] == '-')
		p.pos++;
	// 整数部分
	if (p.pos >= p.len)
		return false;
	if (p.s[p.pos] == '0')
	{
		p.pos++;
	}
	else if (p.s[p.pos] >= '1' && p.s[p.pos] <= '9')
	{
		while (p.pos < p.len && p.s[p.pos] >= '0' && p.s[p.pos] <= '9')
			p.pos++;
	}
	else
	{
		return false;
	}
	bool isFloat = false;
	if (p.pos < p.len && p.s[p.pos] == '.')
	{
		isFloat = true;
		p.pos++;
		if (p.pos >= p.len || p.s[p.pos] < '0' || p.s[p.pos] > '9')
			return false;
		while (p.pos < p.len && p.s[p.pos] >= '0' && p.s[p.pos] <= '9')
			p.pos++;
	}
	if (p.pos < p.len && (p.s[p.pos] == 'e' || p.s[p.pos] == 'E'))
	{
		isFloat = true;
		p.pos++;
		if (p.pos < p.len && (p.s[p.pos] == '+' || p.s[p.pos] == '-'))
			p.pos++;
		if (p.pos >= p.len || p.s[p.pos] < '0' || p.s[p.pos] > '9')
			return false;
		while (p.pos < p.len && p.s[p.pos] >= '0' && p.s[p.pos] <= '9')
			p.pos++;
	}
	std::string token(p.s + start, p.pos - start);
	if (isFloat)
	{
		v.type = JSON_FLOAT;
		v.floatVal = strtod(token.c_str(), nullptr);
	}
	else
	{
		v.type = JSON_INT;
		v.intVal = (int)strtol(token.c_str(), nullptr, 10);
		v.floatVal = (double)v.intVal;
	}
	return true;
}

static bool ParseValue(JsonParser &p, JsonValue &out);

static bool ParseObject(JsonParser &p, JsonValue &out)
{
	p.pos++; // '{'
	out.type = JSON_OBJECT;
	while (true)
	{
		ParseWhitespace(p);
		if (p.pos >= p.len)
			return false;
		if (p.s[p.pos] == '}')
		{
			p.pos++;
			return true;
		}
		if (p.s[p.pos] != '"')
			return false;
		std::string key;
		if (!ParseString(p, key))
			return false;
		ParseWhitespace(p);
		if (p.pos >= p.len || p.s[p.pos] != ':')
			return false;
		p.pos++;
		ParseWhitespace(p);
		JsonValue val;
		if (!ParseValue(p, val))
			return false;
		out.objectMembers.push_back(std::make_pair(key, std::move(val)));
		ParseWhitespace(p);
		if (p.pos >= p.len)
			return false;
		char c = p.s[p.pos];
		if (c == ',')
		{
			p.pos++;
			continue;
		}
		if (c == '}')
		{
			p.pos++;
			return true;
		}
		return false;
	}
}

static bool ParseArray(JsonParser &p, JsonValue &out)
{
	p.pos++; // '['
	out.type = JSON_ARRAY;
	while (true)
	{
		ParseWhitespace(p);
		if (p.pos >= p.len)
			return false;
		if (p.s[p.pos] == ']')
		{
			p.pos++;
			return true;
		}
		JsonValue val;
		if (!ParseValue(p, val))
			return false;
		out.arrayItems.push_back(std::move(val));
		ParseWhitespace(p);
		if (p.pos >= p.len)
			return false;
		char c = p.s[p.pos];
		if (c == ',')
		{
			p.pos++;
			continue;
		}
		if (c == ']')
		{
			p.pos++;
			return true;
		}
		return false;
	}
}

static bool ParseValue(JsonParser &p, JsonValue &out)
{
	ParseWhitespace(p);
	if (p.pos >= p.len)
		return false;
	char c = p.s[p.pos];
	switch (c)
	{
		case '{':
			return ParseObject(p, out);
		case '[':
			return ParseArray(p, out);
		case '"':
		{
			if (!ParseString(p, out.stringVal))
				return false;
			out.type = JSON_STRING;
			return true;
		}
		case 't':
			if (p.len - p.pos >= 4 && strncmp(p.s + p.pos, "true", 4) == 0)
			{
				p.pos += 4;
				out.type = JSON_BOOL;
				out.boolVal = true;
				return true;
			}
			return false;
		case 'f':
			if (p.len - p.pos >= 5 && strncmp(p.s + p.pos, "false", 5) == 0)
			{
				p.pos += 5;
				out.type = JSON_BOOL;
				out.boolVal = false;
				return true;
			}
			return false;
		case 'n':
			if (p.len - p.pos >= 4 && strncmp(p.s + p.pos, "null", 4) == 0)
			{
				p.pos += 4;
				out.type = JSON_NULL;
				return true;
			}
			return false;
		default:
			if (c == '-' || (c >= '0' && c <= '9'))
				return ParseNumber(p, out);
			return false;
	}
}

// 剥离 JSON 中的 // 行注释与 /* */ 块注释 (字符串内的内容不受影响)
static std::string StripJsonComments(const std::string &input)
{
	std::string out;
	out.reserve(input.size());
	size_t i = 0, n = input.size();
	bool inString = false;
	while (i < n)
	{
		char c = input[i];
		if (inString)
		{
			out += c;
			if (c == '\\' && i + 1 < n)
			{
				out += input[i + 1];
				i += 2;
				continue;
			}
			if (c == '"')
				inString = false;
			i++;
			continue;
		}
		if (c == '"')
		{
			inString = true;
			out += c;
			i++;
			continue;
		}
		if (c == '/' && i + 1 < n && input[i + 1] == '/')
		{
			while (i < n && input[i] != '\n')
				i++;
			continue;
		}
		if (c == '/' && i + 1 < n && input[i + 1] == '*')
		{
			i += 2;
			while (i + 1 < n && !(input[i] == '*' && input[i + 1] == '/'))
				i++;
			i += 2;
			continue;
		}
		out += c;
		i++;
	}
	return out;
}

static bool ParseJson(const std::string &input, JsonValue &out)
{
	JsonParser p = { input.c_str(), 0, input.size() };
	if (!ParseValue(p, out))
		return false;
	ParseWhitespace(p);
	return p.pos == p.len; // 必须完整消费输入
}

// ---------------------------------------------------------------- JSON 序列化器

static void EscapeString(const std::string &s, std::string &out)
{
	out += '"';
	for (size_t i = 0; i < s.size(); i++)
	{
		unsigned char c = (unsigned char)s[i];
		switch (c)
		{
			case '"':  out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\b': out += "\\b";  break;
			case '\f': out += "\\f";  break;
			case '\n': out += "\\n";  break;
			case '\r': out += "\\r";  break;
			case '\t': out += "\\t";  break;
			default:
				if (c < 0x20)
				{
					char buf[8];
					snprintf(buf, sizeof(buf), "\\u%04x", (unsigned int)c);
					out += buf;
				}
				else
				{
					out += (char)c;
				}
		}
	}
	out += '"';
}

static void AppendIndent(std::string &out, int depth)
{
	for (int i = 0; i < depth; i++)
		out += "    ";
}

static void SerializeValue(const JsonValue &v, std::string &out, bool pretty, int depth)
{
	switch (v.type)
	{
		case JSON_NULL:
			out += "null";
			break;
		case JSON_BOOL:
			out += v.boolVal ? "true" : "false";
			break;
		case JSON_INT:
		{
			char buf[32];
			snprintf(buf, sizeof(buf), "%d", v.intVal);
			out += buf;
			break;
		}
		case JSON_FLOAT:
		{
			double d = v.floatVal;
			if (d != d || d > DBL_MAX || d < -DBL_MAX)
			{
				// NaN / Inf 不是合法 JSON 数字, 输出 null
				out += "null";
				break;
			}
			char buf[64];
			snprintf(buf, sizeof(buf), "%.15g", d);
			if (!strchr(buf, '.') && !strchr(buf, 'e') && !strchr(buf, 'E'))
				strcat(buf, ".0");
			out += buf;
			break;
		}
		case JSON_STRING:
			EscapeString(v.stringVal, out);
			break;
		case JSON_ARRAY:
		{
			out += '[';
			for (size_t i = 0; i < v.arrayItems.size(); i++)
			{
				if (i)
					out += ',';
				if (pretty)
				{
					out += '\n';
					AppendIndent(out, depth + 1);
				}
				SerializeValue(v.arrayItems[i], out, pretty, depth + 1);
			}
			if (pretty && !v.arrayItems.empty())
			{
				out += '\n';
				AppendIndent(out, depth);
			}
			out += ']';
			break;
		}
		case JSON_OBJECT:
		{
			out += '{';
			for (size_t i = 0; i < v.objectMembers.size(); i++)
			{
				if (i)
					out += ',';
				if (pretty)
				{
					out += '\n';
					AppendIndent(out, depth + 1);
				}
				EscapeString(v.objectMembers[i].first, out);
				out += pretty ? ": " : ":";
				SerializeValue(v.objectMembers[i].second, out, pretty, depth + 1);
			}
			if (pretty && !v.objectMembers.empty())
			{
				out += '\n';
				AppendIndent(out, depth);
			}
			out += '}';
			break;
		}
	}
}

static std::string SerializeJson(const JsonValue &v, bool pretty)
{
	std::string out;
	SerializeValue(v, out, pretty, 0);
	return out;
}

// ---------------------------------------------------------------- 文件读取

static std::string ReadFileToString(const char *path)
{
	FILE *fp = fopen(path, "rb");
	if (!fp)
		return std::string();
	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	std::string data;
	if (size > 0)
	{
		data.resize((size_t)size);
		size_t rd = fread(&data[0], 1, (size_t)size, fp);
		data.resize(rd);
	}
	fclose(fp);
	return data;
}

static bool WriteStringToFile(const char *path, const std::string &data)
{
	FILE *fp = fopen(path, "wb");
	if (!fp)
		return false;
	bool ok = fwrite(data.data(), 1, data.size(), fp) == data.size();
	fclose(fp);
	return ok;
}

// ---------------------------------------------------------------- 视图句柄

static cell MakeViewHandle(JsonHandle *parent, const JsonValue &child, const std::string &path)
{
	JsonHandle *view = new JsonHandle();
	view->value = CloneValue(child);
	view->isRoot = false;
	view->parent = parent;
	view->path = path;
	return (cell)AllocJsonHandle(view);
}

// ---------------------------------------------------------------- 句柄管理 natives

// native json_init()
static cell AMX_NATIVE_CALL amxx_json_init(AMX *amx, cell *params)
{
	ResetJsonHandles();
	return 1;
}

// native json_free() / native bool:json_free(&JSON:handle)
// 旧版 0 参: 释放全部句柄; 新版 1 参 (byref): 释放指定句柄并把 handle 写回 -1 (Invalid_JSON)
static cell AMX_NATIVE_CALL amxx_json_free(AMX *amx, cell *params)
{
	int nparams = params[0] / (int)sizeof(cell);
	if (nparams == 0)
	{
		ResetJsonHandles();
		return 1;
	}
	cell *addr = nullptr;
	amx_GetAddr(amx, params[1], &addr);
	if (!addr)
		return 0;
	int id = (int)*addr;
	if (!FreeJsonHandleTree(id))
		return 0;
	*addr = -1; // Invalid_JSON
	return 1;
}

// native JSON:json_parse(const string[], bool:is_file = false, bool:with_comments = false)
// 旧版只传 1 参; 新版传 3 参 (编译期默认值已展开)。失败返回 -1 (Invalid_JSON)。
static cell AMX_NATIVE_CALL amxx_json_parse(AMX *amx, cell *params)
{
	int nparams = params[0] / (int)sizeof(cell);
	std::string input = ReadAmxString(amx, params[1]);
	bool isFile = (nparams >= 3) && (params[2] != 0);
	bool withComments = (nparams >= 4) && (params[3] != 0);

	std::string data;
	if (isFile)
	{
		data = ReadFileToString(input.c_str());
		if (data.empty())
			return -1;
	}
	else
	{
		data = input;
	}
	if (withComments)
		data = StripJsonComments(data);

	JsonValue root;
	if (!ParseJson(data, root))
		return -1;
	JsonHandle *h = new JsonHandle();
	h->value = std::move(root);
	h->isRoot = true;
	h->parent = nullptr;
	h->path = "/root";
	return (cell)AllocJsonHandle(h);
}

// native JSON:json_create(type)
static cell AMX_NATIVE_CALL amxx_json_create(AMX *amx, cell *params)
{
	int type = (int)params[1];
	if (type < JSON_NULL || type > JSON_OBJECT)
		return 0;
	JsonHandle *h = new JsonHandle();
	h->value.type = (JsonType)type;
	h->isRoot = true;
	h->parent = nullptr;
	h->path = "/root";
	return (cell)AllocJsonHandle(h);
}

// native JSON:json_copy(handle)
static cell AMX_NATIVE_CALL amxx_json_copy(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h)
		return 0;
	JsonHandle *copy = new JsonHandle();
	copy->value = CloneValue(h->value);
	copy->isRoot = true;
	copy->parent = nullptr;
	copy->path = "/root";
	return (cell)AllocJsonHandle(copy);
}

// native json_delete(handle)
static cell AMX_NATIVE_CALL amxx_json_delete(AMX *amx, cell *params)
{
	return FreeJsonHandleTree((int)params[1]) ? 1 : 0;
}

// native bool:json_is_valid(handle)
static cell AMX_NATIVE_CALL amxx_json_is_valid(AMX *amx, cell *params)
{
	return GetJsonHandle((int)params[1]) ? 1 : 0;
}

// ---------------------------------------------------------------- 类型查询 natives

// native JSONType:json_get_type(handle) — 返回新版 JSONType 枚举, 无效句柄返回 -1
static cell AMX_NATIVE_CALL amxx_json_get_type(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h)
		return SCRIPT_JSON_ERROR;
	return (cell)ScriptTypeOf(h->value);
}

static cell JsonIsType(cell *params, JsonType t)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h)
		return 0;
	return h->value.type == t ? 1 : 0;
}

static cell JsonIsNumberType(cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h)
		return 0;
	return (h->value.type == JSON_INT || h->value.type == JSON_FLOAT) ? 1 : 0;
}

// native bool:json_is_object(handle)
static cell AMX_NATIVE_CALL amxx_json_is_object(AMX *amx, cell *params)  { return JsonIsType(params, JSON_OBJECT); }
// native bool:json_is_array(handle)
static cell AMX_NATIVE_CALL amxx_json_is_array(AMX *amx, cell *params)   { return JsonIsType(params, JSON_ARRAY); }
// native bool:json_is_string(handle)
static cell AMX_NATIVE_CALL amxx_json_is_string(AMX *amx, cell *params)  { return JsonIsType(params, JSON_STRING); }
// native bool:json_is_number(handle)
static cell AMX_NATIVE_CALL amxx_json_is_number(AMX *amx, cell *params)  { return JsonIsNumberType(params); }
// native bool:json_is_integer(handle)
static cell AMX_NATIVE_CALL amxx_json_is_integer(AMX *amx, cell *params) { return JsonIsType(params, JSON_INT); }
// native bool:json_is_float(handle)
static cell AMX_NATIVE_CALL amxx_json_is_float(AMX *amx, cell *params)   { return JsonIsType(params, JSON_FLOAT); }
// native bool:json_is_boolean(handle)
static cell AMX_NATIVE_CALL amxx_json_is_boolean(AMX *amx, cell *params) { return JsonIsType(params, JSON_BOOL); }
// native bool:json_is_null(handle)
static cell AMX_NATIVE_CALL amxx_json_is_null(AMX *amx, cell *params)    { return JsonIsType(params, JSON_NULL); }

// native json_object_size(handle)
static cell AMX_NATIVE_CALL amxx_json_object_size(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_OBJECT)
		return 0;
	return (cell)h->value.objectMembers.size();
}

// native json_array_size(handle)
static cell AMX_NATIVE_CALL amxx_json_array_size(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_ARRAY)
		return 0;
	return (cell)h->value.arrayItems.size();
}

// ---------------------------------------------------------------- 对象操作 natives

static JsonValue *FindMember(JsonValue &obj, const std::string &key)
{
	for (size_t i = obj.objectMembers.size(); i > 0; i--)
	{
		if (obj.objectMembers[i - 1].first == key)
			return &obj.objectMembers[i - 1].second;
	}
	return nullptr;
}

// native bool:json_object_has_member(handle, const key[])
static cell AMX_NATIVE_CALL amxx_json_object_has_member(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_OBJECT)
		return 0;
	std::string key = ReadAmxString(amx, params[2]);
	return FindMember(h->value, key) ? 1 : 0;
}

// native JSON:json_object_get_member(handle, const key[])
static cell AMX_NATIVE_CALL amxx_json_object_get_member(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_OBJECT)
		return 0;
	std::string key = ReadAmxString(amx, params[2]);
	JsonValue *member = FindMember(h->value, key);
	if (!member)
		return 0;
	return MakeViewHandle(h, *member, h->path + "/" + key);
}

// native JSON:json_object_get_member_at(handle, index)
static cell AMX_NATIVE_CALL amxx_json_object_get_member_at(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_OBJECT)
		return 0;
	int index = (int)params[2];
	if (index < 0 || index >= (int)h->value.objectMembers.size())
		return 0;
	const std::string &key = h->value.objectMembers[index].first;
	return MakeViewHandle(h, h->value.objectMembers[index].second, h->path + "/" + key);
}

// native bool:json_object_set_member(handle, const key[], value_handle)
static cell AMX_NATIVE_CALL amxx_json_object_set_member(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	JsonHandle *value = GetJsonHandle((int)params[3]);
	if (!h || !value || h->value.type != JSON_OBJECT)
		return 0;
	std::string key = ReadAmxString(amx, params[2]);
	JsonValue *member = FindMember(h->value, key);
	if (member)
	{
		*member = CloneValue(value->value);
	}
	else
	{
		h->value.objectMembers.push_back(std::make_pair(key, CloneValue(value->value)));
	}
	return 1;
}

// native bool:json_object_set_value(JSON:object, const name[], const JSON:value, bool:dot_not = false)
// 新版语义: 第 3 参是 JSON 句柄 (旧版 type+vararg 形式不再支持)。dot_not 内嵌版无点号导航, 忽略。
static cell AMX_NATIVE_CALL amxx_json_object_set_value(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	JsonHandle *value = GetJsonHandle((int)params[3]);
	if (!h || !value || h->value.type != JSON_OBJECT)
		return 0;
	std::string key = ReadAmxString(amx, params[2]);
	JsonValue *member = FindMember(h->value, key);
	if (member)
	{
		*member = CloneValue(value->value);
	}
	else
	{
		h->value.objectMembers.push_back(std::make_pair(key, CloneValue(value->value)));
	}
	return 1;
}

// native bool:json_object_set_string(JSON:object, const name[], const string[], bool:dot_not = false)
// 兼容旧版 3 参调用; 新版第 4 参 dot_not 内嵌版无点号导航, 忽略。
static cell AMX_NATIVE_CALL amxx_json_object_set_string(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_OBJECT)
		return 0;
	std::string key = ReadAmxString(amx, params[2]);
	JsonValue v;
	v.type = JSON_STRING;
	v.stringVal = ReadAmxString(amx, params[3]);
	JsonValue *member = FindMember(h->value, key);
	if (member)
	{
		*member = std::move(v);
	}
	else
	{
		h->value.objectMembers.push_back(std::make_pair(key, std::move(v)));
	}
	return 1;
}

// native bool:json_object_set_number(JSON:object, const name[], number, bool:dot_not = false)
// 兼容旧版 3 参 (Float:value) 调用, 值仍以浮点存储; 新版第 4 参 dot_not 忽略。
static cell AMX_NATIVE_CALL amxx_json_object_set_number(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_OBJECT)
		return 0;
	std::string key = ReadAmxString(amx, params[2]);
	JsonValue v;
	v.type = JSON_FLOAT;
	v.floatVal = (double)amx_ctof(params[3]);
	JsonValue *member = FindMember(h->value, key);
	if (member)
	{
		*member = std::move(v);
	}
	else
	{
		h->value.objectMembers.push_back(std::make_pair(key, std::move(v)));
	}
	return 1;
}

// native bool:json_object_set_bool(JSON:object, const name[], bool:boolean, bool:dot_not = false)
// 兼容旧版 3 参调用; 新版第 4 参 dot_not 忽略。
static cell AMX_NATIVE_CALL amxx_json_object_set_bool(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_OBJECT)
		return 0;
	std::string key = ReadAmxString(amx, params[2]);
	JsonValue v;
	v.type = JSON_BOOL;
	v.boolVal = params[3] != 0;
	JsonValue *member = FindMember(h->value, key);
	if (member)
	{
		*member = std::move(v);
	}
	else
	{
		h->value.objectMembers.push_back(std::make_pair(key, std::move(v)));
	}
	return 1;
}

// native bool:json_object_set_null(JSON:object, const name[], bool:dot_not = false)
// 兼容旧版 2 参调用; 新版第 3 参 dot_not 忽略。
static cell AMX_NATIVE_CALL amxx_json_object_set_null(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_OBJECT)
		return 0;
	std::string key = ReadAmxString(amx, params[2]);
	JsonValue v;
	v.type = JSON_NULL;
	JsonValue *member = FindMember(h->value, key);
	if (member)
	{
		*member = std::move(v);
	}
	else
	{
		h->value.objectMembers.push_back(std::make_pair(key, std::move(v)));
	}
	return 1;
}

// native bool:json_object_remove_member(handle, const key[])
static cell AMX_NATIVE_CALL amxx_json_object_remove_member(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_OBJECT)
		return 0;
	std::string key = ReadAmxString(amx, params[2]);
	for (size_t i = 0; i < h->value.objectMembers.size(); i++)
	{
		if (h->value.objectMembers[i].first == key)
		{
			h->value.objectMembers.erase(h->value.objectMembers.begin() + i);
			return 1;
		}
	}
	return 0;
}

// native bool:json_object_remove_member_at(handle, index)
static cell AMX_NATIVE_CALL amxx_json_object_remove_member_at(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_OBJECT)
		return 0;
	int index = (int)params[2];
	if (index < 0 || index >= (int)h->value.objectMembers.size())
		return 0;
	h->value.objectMembers.erase(h->value.objectMembers.begin() + index);
	return 1;
}

// native bool:json_object_clear(handle)
static cell AMX_NATIVE_CALL amxx_json_object_clear(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_OBJECT)
		return 0;
	h->value.objectMembers.clear();
	return 1;
}

// native json_object_get_key(handle, index, buffer[], maxlen)
static cell AMX_NATIVE_CALL amxx_json_object_get_key(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_OBJECT)
		return 0;
	int index = (int)params[2];
	if (index < 0 || index >= (int)h->value.objectMembers.size())
		return 0;
	return (cell)WriteAmxString(amx, params[3], h->value.objectMembers[index].first.c_str(), (int)params[4]);
}

// ---------------------------------------------------------------- 数组操作 natives

// native JSON:json_array_get_item(handle, index)
static cell AMX_NATIVE_CALL amxx_json_array_get_item(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_ARRAY)
		return 0;
	int index = (int)params[2];
	if (index < 0 || index >= (int)h->value.arrayItems.size())
		return 0;
	char buf[24];
	snprintf(buf, sizeof(buf), "/%d", index);
	return MakeViewHandle(h, h->value.arrayItems[index], h->path + buf);
}

// native bool:json_array_push_item(handle, value_handle)
static cell AMX_NATIVE_CALL amxx_json_array_push_item(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	JsonHandle *value = GetJsonHandle((int)params[2]);
	if (!h || !value || h->value.type != JSON_ARRAY)
		return 0;
	h->value.arrayItems.push_back(CloneValue(value->value));
	return 1;
}

// native bool:json_array_push_string(handle, const value[])
static cell AMX_NATIVE_CALL amxx_json_array_push_string(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_ARRAY)
		return 0;
	JsonValue v;
	v.type = JSON_STRING;
	v.stringVal = ReadAmxString(amx, params[2]);
	h->value.arrayItems.push_back(std::move(v));
	return 1;
}

// native bool:json_array_push_number(handle, Float:value)
static cell AMX_NATIVE_CALL amxx_json_array_push_number(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_ARRAY)
		return 0;
	JsonValue v;
	v.type = JSON_FLOAT;
	v.floatVal = (double)amx_ctof(params[2]);
	h->value.arrayItems.push_back(std::move(v));
	return 1;
}

// native bool:json_array_push_bool(handle, bool:value)
static cell AMX_NATIVE_CALL amxx_json_array_push_bool(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_ARRAY)
		return 0;
	JsonValue v;
	v.type = JSON_BOOL;
	v.boolVal = params[2] != 0;
	h->value.arrayItems.push_back(std::move(v));
	return 1;
}

// native bool:json_array_push_null(handle)
static cell AMX_NATIVE_CALL amxx_json_array_push_null(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_ARRAY)
		return 0;
	JsonValue v;
	v.type = JSON_NULL;
	h->value.arrayItems.push_back(std::move(v));
	return 1;
}

// native bool:json_array_remove_item(handle, index)
static cell AMX_NATIVE_CALL amxx_json_array_remove_item(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_ARRAY)
		return 0;
	int index = (int)params[2];
	if (index < 0 || index >= (int)h->value.arrayItems.size())
		return 0;
	h->value.arrayItems.erase(h->value.arrayItems.begin() + index);
	return 1;
}

// native bool:json_array_clear(handle)
static cell AMX_NATIVE_CALL amxx_json_array_clear(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_ARRAY)
		return 0;
	h->value.arrayItems.clear();
	return 1;
}

// ---------------------------------------------------------------- 值操作 natives

// native json_get_string(handle, buffer[], maxlen)
static cell AMX_NATIVE_CALL amxx_json_get_string(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_STRING)
		return 0;
	return (cell)WriteAmxString(amx, params[2], h->value.stringVal.c_str(), (int)params[3]);
}

// native bool:json_set_string(handle, const value[])
static cell AMX_NATIVE_CALL amxx_json_set_string(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h)
		return 0;
	h->value.type = JSON_STRING;
	h->value.stringVal = ReadAmxString(amx, params[2]);
	return 1;
}

// native Float:json_get_number(handle)
static cell AMX_NATIVE_CALL amxx_json_get_number(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h)
		return 0;
	if (h->value.type == JSON_INT)
	{
		float f = (float)h->value.intVal;
		return amx_ftoc(f);
	}
	if (h->value.type == JSON_FLOAT)
	{
		float f = (float)h->value.floatVal;
		return amx_ftoc(f);
	}
	return 0;
}

// native bool:json_set_number(handle, Float:value)
static cell AMX_NATIVE_CALL amxx_json_set_number(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h)
		return 0;
	h->value.type = JSON_FLOAT;
	h->value.floatVal = (double)amx_ctof(params[2]);
	return 1;
}

// native bool:json_get_bool(handle)
static cell AMX_NATIVE_CALL amxx_json_get_bool(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_BOOL)
		return 0;
	return h->value.boolVal ? 1 : 0;
}

// native bool:json_set_bool(handle, bool:value)
static cell AMX_NATIVE_CALL amxx_json_set_bool(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h)
		return 0;
	h->value.type = JSON_BOOL;
	h->value.boolVal = params[2] != 0;
	return 1;
}

// native bool:json_set_null(handle)
static cell AMX_NATIVE_CALL amxx_json_set_null(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h)
		return 0;
	h->value.type = JSON_NULL;
	return 1;
}

// native json_get_int(handle)
static cell AMX_NATIVE_CALL amxx_json_get_int(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h)
		return 0;
	if (h->value.type == JSON_INT)
		return (cell)h->value.intVal;
	if (h->value.type == JSON_FLOAT)
		return (cell)(int)h->value.floatVal;
	return 0;
}

// native bool:json_set_int(handle, value)
static cell AMX_NATIVE_CALL amxx_json_set_int(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h)
		return 0;
	h->value.type = JSON_INT;
	h->value.intVal = (int)params[2];
	h->value.floatVal = (double)h->value.intVal;
	return 1;
}

// ---------------------------------------------------------------- 序列化 natives

// native json_dump(handle, buffer[], maxlen) — 格式化输出
static cell AMX_NATIVE_CALL amxx_json_dump(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h)
		return 0;
	std::string out = SerializeJson(h->value, true);
	return (cell)WriteAmxString(amx, params[2], out.c_str(), (int)params[3]);
}

// native json_stringify(handle, buffer[], maxlen) — 紧凑输出
static cell AMX_NATIVE_CALL amxx_json_stringify(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h)
		return 0;
	std::string out = SerializeJson(h->value, false);
	return (cell)WriteAmxString(amx, params[2], out.c_str(), (int)params[3]);
}

// native json_print(handle) — 打印到控制台
static cell AMX_NATIVE_CALL amxx_json_print(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h)
		return 0;
	std::string out = SerializeJson(h->value, true);
	ALERT(at_console, "%s\n", out.c_str());
	return 1;
}

// native bool:json_dump_file(handle, const path[])
static cell AMX_NATIVE_CALL amxx_json_dump_file(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h)
		return 0;
	std::string path = ReadAmxString(amx, params[2]);
	std::string out = SerializeJson(h->value, true);
	return WriteStringToFile(path.c_str(), out) ? 1 : 0;
}

// native JSON:json_load_file(const path[])
static cell AMX_NATIVE_CALL amxx_json_load_file(AMX *amx, cell *params)
{
	std::string path = ReadAmxString(amx, params[1]);
	std::string data = ReadFileToString(path.c_str());
	if (data.empty())
		return 0;
	JsonValue root;
	if (!ParseJson(data, root))
		return 0;
	JsonHandle *h = new JsonHandle();
	h->value = std::move(root);
	h->isRoot = true;
	h->parent = nullptr;
	h->path = "/root";
	return (cell)AllocJsonHandle(h);
}

// ---------------------------------------------------------------- 路径/节点操作 natives

// native json_get_path(handle, buffer[], maxlen)
static cell AMX_NATIVE_CALL amxx_json_get_path(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h)
		return 0;
	return (cell)WriteAmxString(amx, params[2], h->path.c_str(), (int)params[3]);
}

// native JSON:json_get_root(handle)
static cell AMX_NATIVE_CALL amxx_json_get_root(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h)
		return 0;
	return (cell)FindHandleId(GetRootHandle(h));
}

// native bool:json_is_root(handle)
static cell AMX_NATIVE_CALL amxx_json_is_root(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h)
		return 0;
	return h->isRoot ? 1 : 0;
}

// native JSON:json_get_parent(handle) — 无效/无父返回 -1 (Invalid_JSON)
static cell AMX_NATIVE_CALL amxx_json_get_parent(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || !h->parent)
		return -1;
	return (cell)FindHandleId(h->parent);
}

// native bool:json_has_parent(handle)
static cell AMX_NATIVE_CALL amxx_json_has_parent(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h)
		return 0;
	return h->parent ? 1 : 0;
}

// ---------------------------------------------------------------- 新版 json.inc API (44 个 native)

// 创建根句柄 (isRoot=true)
static cell MakeRootValue(JsonValue &&v)
{
	JsonHandle *h = new JsonHandle();
	h->value = std::move(v);
	h->isRoot = true;
	h->parent = nullptr;
	h->path = "/root";
	return (cell)AllocJsonHandle(h);
}

// 内部类型是否匹配脚本侧新版 JSONType (用于 json_object_has_value 等)
static bool TypeMatchesScript(JsonType internal, int scriptType)
{
	switch (scriptType)
	{
		case SCRIPT_JSON_NULL:   return internal == JSON_NULL;
		case SCRIPT_JSON_STRING: return internal == JSON_STRING;
		case SCRIPT_JSON_NUMBER: return internal == JSON_INT || internal == JSON_FLOAT;
		case SCRIPT_JSON_OBJECT: return internal == JSON_OBJECT;
		case SCRIPT_JSON_ARRAY:  return internal == JSON_ARRAY;
		case SCRIPT_JSON_BOOL:   return internal == JSON_BOOL;
	}
	return false;
}

// 标量类型是否匹配 (int/float 同属新版 JSONNumber)
static bool SchemaScalarMatch(JsonType schema, JsonType value)
{
	if (schema == value)
		return true;
	return (schema == JSON_INT || schema == JSON_FLOAT) && (value == JSON_INT || value == JSON_FLOAT);
}

static const JsonValue *FindMemberConst(const JsonValue &obj, const std::string &key)
{
	return FindMember(const_cast<JsonValue&>(obj), key);
}

// 递归比较两个 JsonValue (对齐 parson json_value_equals)
static bool JsonValuesEqual(const JsonValue &a, const JsonValue &b)
{
	bool aNum = (a.type == JSON_INT || a.type == JSON_FLOAT);
	bool bNum = (b.type == JSON_INT || b.type == JSON_FLOAT);
	if (aNum && bNum)
	{
		double da = (a.type == JSON_INT) ? (double)a.intVal : a.floatVal;
		double db = (b.type == JSON_INT) ? (double)b.intVal : b.floatVal;
		double diff = da - db;
		if (diff < 0.0)
			diff = -diff;
		return diff < 0.000001;
	}
	if (a.type != b.type)
		return false;
	switch (a.type)
	{
		case JSON_STRING:
			return a.stringVal == b.stringVal;
		case JSON_BOOL:
			return a.boolVal == b.boolVal;
		case JSON_NULL:
			return true;
		case JSON_ARRAY:
		{
			if (a.arrayItems.size() != b.arrayItems.size())
				return false;
			for (size_t i = 0; i < a.arrayItems.size(); i++)
			{
				if (!JsonValuesEqual(a.arrayItems[i], b.arrayItems[i]))
					return false;
			}
			return true;
		}
		case JSON_OBJECT:
		{
			if (a.objectMembers.size() != b.objectMembers.size())
				return false;
			for (size_t i = 0; i < a.objectMembers.size(); i++)
			{
				const JsonValue *bm = FindMemberConst(b, a.objectMembers[i].first);
				if (!bm)
					return false;
				if (!JsonValuesEqual(a.objectMembers[i].second, *bm))
					return false;
			}
			return true;
		}
	}
	return true;
}

// 按 schema 递归校验 (对齐 parson json_validate)
static bool ValidateValue(const JsonValue &schema, const JsonValue &value)
{
	if (schema.type == JSON_NULL)
		return true; // null 匹配所有类型
	if (schema.type == JSON_OBJECT)
	{
		if (value.type != JSON_OBJECT)
			return false;
		if (schema.objectMembers.empty())
			return true; // 空对象验证所有对象
		if (value.objectMembers.size() < schema.objectMembers.size())
			return false;
		for (size_t i = 0; i < schema.objectMembers.size(); i++)
		{
			const JsonValue *vm = FindMemberConst(value, schema.objectMembers[i].first);
			if (!vm)
				return false;
			if (!ValidateValue(schema.objectMembers[i].second, *vm))
				return false;
		}
		return true;
	}
	if (schema.type == JSON_ARRAY)
	{
		if (value.type != JSON_ARRAY)
			return false;
		if (schema.arrayItems.empty())
			return true; // 空数组验证所有数组
		// 只检查 schema 首个元素对全部元素
		for (size_t i = 0; i < value.arrayItems.size(); i++)
		{
			if (!ValidateValue(schema.arrayItems[0], value.arrayItems[i]))
				return false;
		}
		return true;
	}
	return SchemaScalarMatch(schema.type, value.type);
}

static JsonValue *ArrayElementAt(JsonValue &arr, int index)
{
	if (index < 0 || index >= (int)arr.arrayItems.size())
		return nullptr;
	return &arr.arrayItems[index];
}

// native bool:json_equals(const JSON:value1, const JSON:value2)
static cell AMX_NATIVE_CALL amxx_json_equals(AMX *amx, cell *params)
{
	int v1 = (int)params[1], v2 = (int)params[2];
	if (v1 < 1 || v2 < 1)
		return (v1 == v2) ? 1 : 0;
	JsonHandle *h1 = GetJsonHandle(v1);
	JsonHandle *h2 = GetJsonHandle(v2);
	if (!h1 || !h2)
		return 0;
	return JsonValuesEqual(h1->value, h2->value) ? 1 : 0;
}

// native bool:json_validate(const JSON:schema, const JSON:value)
static cell AMX_NATIVE_CALL amxx_json_validate(AMX *amx, cell *params)
{
	JsonHandle *schema = GetJsonHandle((int)params[1]);
	JsonHandle *value = GetJsonHandle((int)params[2]);
	if (!schema || !value)
		return 0;
	return ValidateValue(schema->value, value->value) ? 1 : 0;
}

// native JSON:json_init_object()
static cell AMX_NATIVE_CALL amxx_json_init_object(AMX *amx, cell *params)
{
	JsonValue v;
	v.type = JSON_OBJECT;
	return MakeRootValue(std::move(v));
}

// native JSON:json_init_array()
static cell AMX_NATIVE_CALL amxx_json_init_array(AMX *amx, cell *params)
{
	JsonValue v;
	v.type = JSON_ARRAY;
	return MakeRootValue(std::move(v));
}

// native JSON:json_init_string(const value[])
static cell AMX_NATIVE_CALL amxx_json_init_string(AMX *amx, cell *params)
{
	JsonValue v;
	v.type = JSON_STRING;
	v.stringVal = ReadAmxString(amx, params[1]);
	return MakeRootValue(std::move(v));
}

// native JSON:json_init_number(value)
static cell AMX_NATIVE_CALL amxx_json_init_number(AMX *amx, cell *params)
{
	JsonValue v;
	v.type = JSON_INT;
	v.intVal = (int)params[1];
	v.floatVal = (double)v.intVal;
	return MakeRootValue(std::move(v));
}

// native JSON:json_init_real(Float:value)
static cell AMX_NATIVE_CALL amxx_json_init_real(AMX *amx, cell *params)
{
	JsonValue v;
	v.type = JSON_FLOAT;
	v.floatVal = (double)amx_ctof(params[1]);
	return MakeRootValue(std::move(v));
}

// native JSON:json_init_bool(bool:value)
static cell AMX_NATIVE_CALL amxx_json_init_bool(AMX *amx, cell *params)
{
	JsonValue v;
	v.type = JSON_BOOL;
	v.boolVal = params[1] != 0;
	return MakeRootValue(std::move(v));
}

// native JSON:json_init_null()
static cell AMX_NATIVE_CALL amxx_json_init_null(AMX *amx, cell *params)
{
	JsonValue v;
	v.type = JSON_NULL;
	return MakeRootValue(std::move(v));
}

// native JSON:json_deep_copy(const JSON:value)
static cell AMX_NATIVE_CALL amxx_json_deep_copy(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h)
		return -1;
	return MakeRootValue(CloneValue(h->value));
}

// native Float:json_get_real(const JSON:value)
static cell AMX_NATIVE_CALL amxx_json_get_real(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h)
		return 0;
	float f = 0.0f;
	if (h->value.type == JSON_INT)
		f = (float)h->value.intVal;
	else if (h->value.type == JSON_FLOAT)
		f = (float)h->value.floatVal;
	return amx_ftoc(f);
}

// native JSON:json_array_get_value(const JSON:array, index)
static cell AMX_NATIVE_CALL amxx_json_array_get_value(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_ARRAY)
		return -1;
	int index = (int)params[2];
	if (index < 0 || index >= (int)h->value.arrayItems.size())
		return -1;
	char buf[24];
	snprintf(buf, sizeof(buf), "/%d", index);
	return MakeViewHandle(h, h->value.arrayItems[index], h->path + buf);
}

// native json_array_get_string(const JSON:array, index, buffer[], maxlen)
static cell AMX_NATIVE_CALL amxx_json_array_get_string(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_ARRAY)
		return 0;
	JsonValue *elem = ArrayElementAt(h->value, (int)params[2]);
	const char *str = (elem && elem->type == JSON_STRING) ? elem->stringVal.c_str() : "";
	return (cell)WriteAmxString(amx, params[3], str, (int)params[4]);
}

// native json_array_get_number(const JSON:array, index)
static cell AMX_NATIVE_CALL amxx_json_array_get_number(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_ARRAY)
		return 0;
	JsonValue *elem = ArrayElementAt(h->value, (int)params[2]);
	if (!elem)
		return 0;
	if (elem->type == JSON_INT)
		return (cell)elem->intVal;
	if (elem->type == JSON_FLOAT)
		return (cell)(int)elem->floatVal;
	return 0;
}

// native Float:json_array_get_real(const JSON:array, index)
static cell AMX_NATIVE_CALL amxx_json_array_get_real(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_ARRAY)
		return 0;
	JsonValue *elem = ArrayElementAt(h->value, (int)params[2]);
	if (!elem) {
		float zero = 0.0f;
		return amx_ftoc(zero);
	}
	float f = 0.0f;
	if (elem->type == JSON_INT)
		f = (float)elem->intVal;
	else if (elem->type == JSON_FLOAT)
		f = (float)elem->floatVal;
	return amx_ftoc(f);
}

// native bool:json_array_get_bool(const JSON:array, index)
static cell AMX_NATIVE_CALL amxx_json_array_get_bool(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_ARRAY)
		return 0;
	JsonValue *elem = ArrayElementAt(h->value, (int)params[2]);
	if (!elem || elem->type != JSON_BOOL)
		return 0;
	return elem->boolVal ? 1 : 0;
}

// native json_array_get_count(const JSON:array)
static cell AMX_NATIVE_CALL amxx_json_array_get_count(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_ARRAY)
		return 0;
	return (cell)h->value.arrayItems.size();
}

// native bool:json_array_replace_value(JSON:array, index, const JSON:value)
static cell AMX_NATIVE_CALL amxx_json_array_replace_value(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	JsonHandle *value = GetJsonHandle((int)params[3]);
	if (!h || !value || h->value.type != JSON_ARRAY)
		return 0;
	JsonValue *elem = ArrayElementAt(h->value, (int)params[2]);
	if (!elem)
		return 0;
	*elem = CloneValue(value->value);
	return 1;
}

// native bool:json_array_replace_string(JSON:array, index, const string[])
static cell AMX_NATIVE_CALL amxx_json_array_replace_string(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_ARRAY)
		return 0;
	JsonValue *elem = ArrayElementAt(h->value, (int)params[2]);
	if (!elem)
		return 0;
	elem->type = JSON_STRING;
	elem->stringVal = ReadAmxString(amx, params[3]);
	return 1;
}

// native bool:json_array_replace_number(JSON:array, index, number)
static cell AMX_NATIVE_CALL amxx_json_array_replace_number(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_ARRAY)
		return 0;
	JsonValue *elem = ArrayElementAt(h->value, (int)params[2]);
	if (!elem)
		return 0;
	elem->type = JSON_INT;
	elem->intVal = (int)params[3];
	elem->floatVal = (double)elem->intVal;
	return 1;
}

// native bool:json_array_replace_real(JSON:array, index, Float:number)
static cell AMX_NATIVE_CALL amxx_json_array_replace_real(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_ARRAY)
		return 0;
	JsonValue *elem = ArrayElementAt(h->value, (int)params[2]);
	if (!elem)
		return 0;
	elem->type = JSON_FLOAT;
	elem->floatVal = (double)amx_ctof(params[3]);
	return 1;
}

// native bool:json_array_replace_bool(JSON:array, index, bool:boolean)
static cell AMX_NATIVE_CALL amxx_json_array_replace_bool(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_ARRAY)
		return 0;
	JsonValue *elem = ArrayElementAt(h->value, (int)params[2]);
	if (!elem)
		return 0;
	elem->type = JSON_BOOL;
	elem->boolVal = params[3] != 0;
	return 1;
}

// native bool:json_array_replace_null(JSON:array, index)
static cell AMX_NATIVE_CALL amxx_json_array_replace_null(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_ARRAY)
		return 0;
	JsonValue *elem = ArrayElementAt(h->value, (int)params[2]);
	if (!elem)
		return 0;
	elem->type = JSON_NULL;
	return 1;
}

// native bool:json_array_append_value(JSON:array, const JSON:value)
static cell AMX_NATIVE_CALL amxx_json_array_append_value(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	JsonHandle *value = GetJsonHandle((int)params[2]);
	if (!h || !value || h->value.type != JSON_ARRAY)
		return 0;
	h->value.arrayItems.push_back(CloneValue(value->value));
	return 1;
}

// native bool:json_array_append_string(JSON:array, const string[])
static cell AMX_NATIVE_CALL amxx_json_array_append_string(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_ARRAY)
		return 0;
	JsonValue v;
	v.type = JSON_STRING;
	v.stringVal = ReadAmxString(amx, params[2]);
	h->value.arrayItems.push_back(std::move(v));
	return 1;
}

// native bool:json_array_append_number(JSON:array, number)
static cell AMX_NATIVE_CALL amxx_json_array_append_number(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_ARRAY)
		return 0;
	JsonValue v;
	v.type = JSON_INT;
	v.intVal = (int)params[2];
	v.floatVal = (double)v.intVal;
	h->value.arrayItems.push_back(std::move(v));
	return 1;
}

// native bool:json_array_append_real(JSON:array, Float:number)
static cell AMX_NATIVE_CALL amxx_json_array_append_real(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_ARRAY)
		return 0;
	JsonValue v;
	v.type = JSON_FLOAT;
	v.floatVal = (double)amx_ctof(params[2]);
	h->value.arrayItems.push_back(std::move(v));
	return 1;
}

// native bool:json_array_append_bool(JSON:array, bool:boolean)
static cell AMX_NATIVE_CALL amxx_json_array_append_bool(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_ARRAY)
		return 0;
	JsonValue v;
	v.type = JSON_BOOL;
	v.boolVal = params[2] != 0;
	h->value.arrayItems.push_back(std::move(v));
	return 1;
}

// native bool:json_array_append_null(JSON:array)
static cell AMX_NATIVE_CALL amxx_json_array_append_null(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_ARRAY)
		return 0;
	JsonValue v;
	v.type = JSON_NULL;
	h->value.arrayItems.push_back(std::move(v));
	return 1;
}

// native bool:json_array_remove(JSON:array, index)
static cell AMX_NATIVE_CALL amxx_json_array_remove(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_ARRAY)
		return 0;
	int index = (int)params[2];
	if (index < 0 || index >= (int)h->value.arrayItems.size())
		return 0;
	h->value.arrayItems.erase(h->value.arrayItems.begin() + index);
	return 1;
}

// native JSON:json_object_get_value(const JSON:object, const name[], bool:dot_not = false)
static cell AMX_NATIVE_CALL amxx_json_object_get_value(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_OBJECT)
		return -1;
	std::string key = ReadAmxString(amx, params[2]);
	JsonValue *member = FindMember(h->value, key);
	if (!member)
		return -1;
	return MakeViewHandle(h, *member, h->path + "/" + key);
}

// native json_object_get_string(const JSON:object, const name[], buffer[], maxlen, bool:dot_not = false)
static cell AMX_NATIVE_CALL amxx_json_object_get_string(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_OBJECT)
		return 0;
	std::string key = ReadAmxString(amx, params[2]);
	JsonValue *member = FindMember(h->value, key);
	const char *str = (member && member->type == JSON_STRING) ? member->stringVal.c_str() : "";
	return (cell)WriteAmxString(amx, params[3], str, (int)params[4]);
}

// native json_object_get_number(const JSON:object, const name[], bool:dot_not = false)
static cell AMX_NATIVE_CALL amxx_json_object_get_number(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_OBJECT)
		return 0;
	std::string key = ReadAmxString(amx, params[2]);
	JsonValue *member = FindMember(h->value, key);
	if (!member)
		return 0;
	if (member->type == JSON_INT)
		return (cell)member->intVal;
	if (member->type == JSON_FLOAT)
		return (cell)(int)member->floatVal;
	return 0;
}

// native Float:json_object_get_real(const JSON:object, const name[], bool:dot_not = false)
static cell AMX_NATIVE_CALL amxx_json_object_get_real(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_OBJECT)
		return 0;
	std::string key = ReadAmxString(amx, params[2]);
	JsonValue *member = FindMember(h->value, key);
	if (!member) {
		float zero = 0.0f;
		return amx_ftoc(zero);
	}
	float f = 0.0f;
	if (member->type == JSON_INT)
		f = (float)member->intVal;
	else if (member->type == JSON_FLOAT)
		f = (float)member->floatVal;
	return amx_ftoc(f);
}

// native bool:json_object_get_bool(const JSON:object, const name[], bool:dot_not = false)
static cell AMX_NATIVE_CALL amxx_json_object_get_bool(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_OBJECT)
		return 0;
	std::string key = ReadAmxString(amx, params[2]);
	JsonValue *member = FindMember(h->value, key);
	if (!member || member->type != JSON_BOOL)
		return 0;
	return member->boolVal ? 1 : 0;
}

// native json_object_get_count(const JSON:object)
static cell AMX_NATIVE_CALL amxx_json_object_get_count(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_OBJECT)
		return 0;
	return (cell)h->value.objectMembers.size();
}

// native json_object_get_name(const JSON:object, index, buffer[], maxlen)
static cell AMX_NATIVE_CALL amxx_json_object_get_name(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_OBJECT)
		return 0;
	int index = (int)params[2];
	if (index < 0 || index >= (int)h->value.objectMembers.size())
		return 0;
	return (cell)WriteAmxString(amx, params[3], h->value.objectMembers[index].first.c_str(), (int)params[4]);
}

// native JSON:json_object_get_value_at(const JSON:object, index)
static cell AMX_NATIVE_CALL amxx_json_object_get_value_at(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_OBJECT)
		return -1;
	int index = (int)params[2];
	if (index < 0 || index >= (int)h->value.objectMembers.size())
		return -1;
	const std::string &key = h->value.objectMembers[index].first;
	return MakeViewHandle(h, h->value.objectMembers[index].second, h->path + "/" + key);
}

// native bool:json_object_has_value(const JSON:object, const name[], JSONType:type = JSONError, bool:dot_not = false)
static cell AMX_NATIVE_CALL amxx_json_object_has_value(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_OBJECT)
		return 0;
	std::string key = ReadAmxString(amx, params[2]);
	JsonValue *member = FindMember(h->value, key);
	if (!member)
		return 0;
	int nparams = params[0] / (int)sizeof(cell);
	if (nparams >= 3 && (int)params[3] != SCRIPT_JSON_ERROR)
		return TypeMatchesScript(member->type, (int)params[3]) ? 1 : 0;
	return 1;
}

// native bool:json_object_set_real(JSON:object, const name[], Float:number, bool:dot_not = false)
static cell AMX_NATIVE_CALL amxx_json_object_set_real(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h || h->value.type != JSON_OBJECT)
		return 0;
	std::string key = ReadAmxString(amx, params[2]);
	JsonValue v;
	v.type = JSON_FLOAT;
	v.floatVal = (double)amx_ctof(params[3]);
	JsonValue *member = FindMember(h->value, key);
	if (member)
		*member = std::move(v);
	else
		h->value.objectMembers.push_back(std::make_pair(key, std::move(v)));
	return 1;
}

// native bool:json_object_remove(JSON:object, const name[], bool:dot_not = false)
static cell AMX_NATIVE_CALL amxx_json_object_remove(AMX *amx, cell *params)
{
	return amxx_json_object_remove_member(amx, params);
}

// native json_serial_size(const JSON:value, bool:pretty = false, bool:null_byte = false)
static cell AMX_NATIVE_CALL amxx_json_serial_size(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h)
		return 0;
	int nparams = params[0] / (int)sizeof(cell);
	bool pretty = (nparams >= 2) && (params[2] != 0);
	bool nullByte = (nparams >= 3) && (params[3] != 0);
	std::string out = SerializeJson(h->value, pretty);
	size_t size = out.size(); // 不含终止符
	return (cell)(nullByte ? (size + 1) : size);
}

// native json_serial_to_string(const JSON:value, buffer[], maxlen, bool:pretty = false)
static cell AMX_NATIVE_CALL amxx_json_serial_to_string(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h)
		return 0;
	int nparams = params[0] / (int)sizeof(cell);
	bool pretty = (nparams >= 4) && (params[4] != 0);
	std::string out = SerializeJson(h->value, pretty);
	return (cell)WriteAmxString(amx, params[2], out.c_str(), (int)params[3]);
}

// native bool:json_serial_to_file(const JSON:value, const file[], bool:pretty = false)
static cell AMX_NATIVE_CALL amxx_json_serial_to_file(AMX *amx, cell *params)
{
	JsonHandle *h = GetJsonHandle((int)params[1]);
	if (!h)
		return 0;
	std::string path = ReadAmxString(amx, params[2]);
	int nparams = params[0] / (int)sizeof(cell);
	bool pretty = (nparams >= 3) && (params[3] != 0);
	std::string out = SerializeJson(h->value, pretty);
	return WriteStringToFile(path.c_str(), out) ? 1 : 0;
}

// ---------------------------------------------------------------- 注册

AMX_NATIVE_INFO json_natives[] = {
	// 句柄管理
	{"json_init",            amxx_json_init},
	{"json_free",            amxx_json_free},
	{"json_parse",           amxx_json_parse},
	{"json_create",          amxx_json_create},
	{"json_copy",            amxx_json_copy},
	{"json_delete",          amxx_json_delete},
	{"json_is_valid",        amxx_json_is_valid},
	// 类型查询
	{"json_get_type",        amxx_json_get_type},
	{"json_is_object",       amxx_json_is_object},
	{"json_is_array",        amxx_json_is_array},
	{"json_is_string",       amxx_json_is_string},
	{"json_is_number",       amxx_json_is_number},
	{"json_is_integer",      amxx_json_is_integer},
	{"json_is_float",        amxx_json_is_float},
	{"json_is_boolean",      amxx_json_is_boolean},
	{"json_is_null",         amxx_json_is_null},
	{"json_object_size",     amxx_json_object_size},
	{"json_array_size",      amxx_json_array_size},
	// 对象操作
	{"json_object_has_member",    amxx_json_object_has_member},
	{"json_object_get_member",    amxx_json_object_get_member},
	{"json_object_get_member_at", amxx_json_object_get_member_at},
	{"json_object_set_member",    amxx_json_object_set_member},
	{"json_object_set_value",     amxx_json_object_set_value},
	{"json_object_set_string",    amxx_json_object_set_string},
	{"json_object_set_number",    amxx_json_object_set_number},
	{"json_object_set_bool",      amxx_json_object_set_bool},
	{"json_object_set_null",      amxx_json_object_set_null},
	{"json_object_remove_member",    amxx_json_object_remove_member},
	{"json_object_remove_member_at", amxx_json_object_remove_member_at},
	{"json_object_clear",         amxx_json_object_clear},
	{"json_object_get_key",       amxx_json_object_get_key},
	// 数组操作
	{"json_array_get_item",    amxx_json_array_get_item},
	{"json_array_push_item",   amxx_json_array_push_item},
	{"json_array_push_string", amxx_json_array_push_string},
	{"json_array_push_number", amxx_json_array_push_number},
	{"json_array_push_bool",   amxx_json_array_push_bool},
	{"json_array_push_null",   amxx_json_array_push_null},
	{"json_array_remove_item", amxx_json_array_remove_item},
	{"json_array_clear",       amxx_json_array_clear},
	// 值操作
	{"json_get_string",  amxx_json_get_string},
	{"json_set_string",  amxx_json_set_string},
	{"json_get_number",  amxx_json_get_number},
	{"json_set_number",  amxx_json_set_number},
	{"json_get_bool",    amxx_json_get_bool},
	{"json_set_bool",    amxx_json_set_bool},
	{"json_set_null",    amxx_json_set_null},
	{"json_get_int",     amxx_json_get_int},
	{"json_set_int",     amxx_json_set_int},
	// 序列化
	{"json_dump",        amxx_json_dump},
	{"json_stringify",   amxx_json_stringify},
	{"json_print",       amxx_json_print},
	{"json_dump_file",   amxx_json_dump_file},
	{"json_load_file",   amxx_json_load_file},
	// 路径/节点操作
	{"json_get_path",    amxx_json_get_path},
	{"json_get_root",    amxx_json_get_root},
	{"json_is_root",     amxx_json_is_root},
	{"json_get_parent",  amxx_json_get_parent},
	{"json_has_parent",  amxx_json_has_parent},
	// 新版 json.inc API
	{"json_equals",              amxx_json_equals},
	{"json_validate",            amxx_json_validate},
	{"json_init_object",         amxx_json_init_object},
	{"json_init_array",          amxx_json_init_array},
	{"json_init_string",         amxx_json_init_string},
	{"json_init_number",         amxx_json_init_number},
	{"json_init_real",           amxx_json_init_real},
	{"json_init_bool",           amxx_json_init_bool},
	{"json_init_null",           amxx_json_init_null},
	{"json_deep_copy",           amxx_json_deep_copy},
	{"json_get_real",            amxx_json_get_real},
	{"json_array_get_value",     amxx_json_array_get_value},
	{"json_array_get_string",    amxx_json_array_get_string},
	{"json_array_get_number",    amxx_json_array_get_number},
	{"json_array_get_real",      amxx_json_array_get_real},
	{"json_array_get_bool",      amxx_json_array_get_bool},
	{"json_array_get_count",     amxx_json_array_get_count},
	{"json_array_replace_value", amxx_json_array_replace_value},
	{"json_array_replace_string", amxx_json_array_replace_string},
	{"json_array_replace_number", amxx_json_array_replace_number},
	{"json_array_replace_real",   amxx_json_array_replace_real},
	{"json_array_replace_bool",   amxx_json_array_replace_bool},
	{"json_array_replace_null",   amxx_json_array_replace_null},
	{"json_array_append_value",   amxx_json_array_append_value},
	{"json_array_append_string",  amxx_json_array_append_string},
	{"json_array_append_number",  amxx_json_array_append_number},
	{"json_array_append_real",    amxx_json_array_append_real},
	{"json_array_append_bool",    amxx_json_array_append_bool},
	{"json_array_append_null",    amxx_json_array_append_null},
	{"json_array_remove",         amxx_json_array_remove},
	{"json_object_get_value",     amxx_json_object_get_value},
	{"json_object_get_string",    amxx_json_object_get_string},
	{"json_object_get_number",    amxx_json_object_get_number},
	{"json_object_get_real",      amxx_json_object_get_real},
	{"json_object_get_bool",      amxx_json_object_get_bool},
	{"json_object_get_count",     amxx_json_object_get_count},
	{"json_object_get_name",      amxx_json_object_get_name},
	{"json_object_get_value_at",  amxx_json_object_get_value_at},
	{"json_object_has_value",     amxx_json_object_has_value},
	{"json_object_set_real",      amxx_json_object_set_real},
	{"json_object_remove",        amxx_json_object_remove},
	{"json_serial_size",          amxx_json_serial_size},
	{"json_serial_to_string",     amxx_json_serial_to_string},
	{"json_serial_to_file",       amxx_json_serial_to_file},
	{nullptr, nullptr}
};

void RegisterJSONNatives(AMX *amx)
{
	amx_Register(amx, json_natives, -1);
}
