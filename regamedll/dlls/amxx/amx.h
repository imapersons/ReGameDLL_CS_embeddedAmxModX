/*  Pawn Abstract Machine (for the Pawn language)
 *
 *  Copyright (c) ITB CompuPhase, 1997-2005
 *
 *  This software is provided "as-is", without any express or implied warranty.
 *  In no event will the authors be held liable for any damages arising from
 *  the use of this software.
 *
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 *
 *  1.  The origin of this software must not be misrepresented; you must not
 *      claim that you wrote the original software. If you use this software in
 *      a product, an acknowledgment in the product documentation would be
 *      appreciated but is not required.
 *  2.  Altered source versions must be plainly marked as such, and must not be
 *      misrepresented as being the original software.
 *  3.  This notice may not be removed or altered from any source distribution.
 */

#if defined FREEBSD && !defined __FreeBSD__
  #define __FreeBSD__
#endif
#if defined LINUX || defined __FreeBSD__ || defined __OpenBSD__ || defined __APPLE__
  #include <sclinux.h>
#endif

#ifndef AMX_H_INCLUDED
#define AMX_H_INCLUDED

#if defined HAVE_STDINT_H
  #include <stdint.h>
#else
  #include <stdint.h>
  #define HAVE_STDINT_H
#endif
#if defined _LP64 || defined WIN64 || defined _WIN64
  #if !defined __64BIT__
    #define __64BIT__
  #endif
#endif

#if !defined arraysize
  #define arraysize(array)  (sizeof(array) / sizeof((array)[0]))
#endif

#ifdef  __cplusplus
extern  "C" {
#endif

#if defined PAWN_DLL
  #if !defined AMX_NATIVE_CALL
    #define AMX_NATIVE_CALL __stdcall
  #endif
  #if !defined AMXAPI
    #define AMXAPI          __stdcall
  #endif
#endif

#if !defined AMX_NATIVE_CALL
  #define AMX_NATIVE_CALL
#endif
#if !defined AMXAPI
  #if defined STDECL
    #define AMXAPI      __stdcall
  #elif defined CDECL
    #define AMXAPI      __cdecl
  #elif defined GCC_HASCLASSVISIBILITY
    #define AMXAPI __attribute__ ((visibility("default")))
  #else
    #define AMXAPI
  #endif
#endif
#if !defined AMXEXPORT
  #define AMXEXPORT
#endif

#define CUR_FILE_VERSION  8
#define MIN_FILE_VERSION  6
#define MIN_AMX_VERSION   8

#if !defined PAWN_CELL_SIZE
  #define PAWN_CELL_SIZE 32
#endif
#if PAWN_CELL_SIZE==16
  typedef uint16_t  ucell;
  typedef int16_t   cell;
#elif PAWN_CELL_SIZE==32
  typedef uint32_t  ucell;
  typedef int32_t   cell;
#define REAL	float
#elif PAWN_CELL_SIZE==64
  typedef uint64_t  ucell;
  typedef int64_t   cell;
#define REAL	double
#else
  #error Unsupported cell size (PAWN_CELL_SIZE)
#endif

#define UNPACKEDMAX   ((1L << (sizeof(cell)-1)*8) - 1)
#define UNLIMITED     (~1u >> 1)

struct tagAMX;
typedef cell (AMX_NATIVE_CALL *AMX_NATIVE)(struct tagAMX *amx, cell *params);
typedef int (AMXAPI *AMX_CALLBACK)(struct tagAMX *amx, cell index,
                                   cell *result, cell *params);
typedef int (AMXAPI *AMX_DEBUG)(struct tagAMX *amx);
typedef int (AMXAPI *AMX_NATIVE_FILTER)(struct tagAMX *amx, int index);
#if !defined _FAR
  #define _FAR
#endif

#if defined _MSC_VER
  #pragma warning(disable:4103)
  #pragma warning(disable:4100)

	#if _MSC_VER >= 1400
		#define stricmp _stricmp
		#pragma warning (disable : 4996)
	#endif
#endif

#if (defined SN_TARGET_PS2 || defined __GNUC__) && !defined AMX_NO_ALIGN
  #define AMX_NO_ALIGN
#endif

#if defined __GNUC__
  #define PACKED        __attribute__((packed))
#else
  #define PACKED
#endif

#if !defined AMX_NO_ALIGN
  #if defined LINUX || defined __FreeBSD__ || defined __APPLE__
    #pragma pack(1)
  #elif defined MACOS && defined __MWERKS__
	#pragma options align=mac68k
  #else
    #pragma pack(push)
    #pragma pack(1)
    #if defined __TURBOC__
      #pragma option -a-
    #endif
  #endif
#endif

typedef struct tagAMX_NATIVE_INFO {
  const char _FAR *name PACKED;
  AMX_NATIVE func       PACKED;
} PACKED AMX_NATIVE_INFO;

#define AMX_USERNUM     4
#define sEXPMAX         19
#define sNAMEMAX        63

#ifdef  __cplusplus
}
#endif

// ===== AMXX 日志系统（按编译平台差异化输出） =====
// Android 平台输出到 logcat (__android_log_print)，方便 APK 调试时查看
// 其他平台（Windows/Linux PC）输出到 stdout + ALERT 引擎日志通道
#if defined(ANDROID) || defined(__ANDROID__)
    #include <android/log.h>
    #define AMXX_LOG_TAG  "AMXX_Runtime"
    #define AMXX_LOG(msg, ...) \
        do { \
            char amxx_log_buf[1024]; \
            snprintf(amxx_log_buf, sizeof(amxx_log_buf), "[AMXX] " msg, ##__VA_ARGS__); \
            __android_log_print(ANDROID_LOG_INFO, AMXX_LOG_TAG, "%s", amxx_log_buf); \
        } while (0)
    #define AMXX_LOG_ERR(msg, ...) \
        do { \
            char amxx_log_buf[1024]; \
            snprintf(amxx_log_buf, sizeof(amxx_log_buf), "[AMXX] " msg, ##__VA_ARGS__); \
            __android_log_print(ANDROID_LOG_ERROR, AMXX_LOG_TAG, "%s", amxx_log_buf); \
        } while (0)
#else
    #define AMXX_LOG(msg, ...) \
        do { \
            char amxx_log_buf[1024]; \
            snprintf(amxx_log_buf, sizeof(amxx_log_buf), "[AMXX] " msg, ##__VA_ARGS__); \
            amxx_console_print(amxx_log_buf); \
        } while (0)
    #define AMXX_LOG_ERR(msg, ...)  AMXX_LOG(msg, ##__VA_ARGS__)
#endif
// AMXX_LOG_DBG: 仅在 DEBUG 编译下输出，用于跟踪/调试信息
// AMXX_LOG/AMXX_LOG_ERR: 始终输出，用于错误/警告
#if defined AMXX_DEBUG
    #define AMXX_LOG_DBG(msg, ...)  AMXX_LOG(msg, ##__VA_ARGS__)
#else
    #define AMXX_LOG_DBG(msg, ...)  ((void)0)
#endif

#define AMXX_LOG_FUNC() \
    AMXX_LOG_DBG("[%s] Enter", __FUNCTION__)
#define AMXX_LOG_FUNC_RET(ret) \
    AMXX_LOG_DBG("[%s] Exit, ret=%d", __FUNCTION__, ret)

#ifdef  __cplusplus
extern "C" {
#endif

typedef struct tagAMX_FUNCSTUB {
  ucell address         PACKED;
  char name[sEXPMAX+1];
} PACKED AMX_FUNCSTUB;

typedef struct tagAMX_FUNCSTUBNT {
  ucell address         PACKED;
  ucell nameofs      PACKED;
} PACKED AMX_FUNCSTUBNT;

typedef struct tagAMX {
  unsigned char _FAR *base PACKED;
  unsigned char _FAR *data PACKED;
  AMX_CALLBACK callback PACKED;
  AMX_DEBUG debug       PACKED;
  cell cip              PACKED;
  cell frm              PACKED;
  cell hea              PACKED;
  cell hlw              PACKED;
  cell stk              PACKED;
  cell stp              PACKED;
  int flags             PACKED;
  void _FAR *usertags[AMX_USERNUM] PACKED;
  void _FAR *userdata[AMX_USERNUM] PACKED;
  int error             PACKED;
  int paramcount;
  cell pri              PACKED;
  cell alt              PACKED;
  cell reset_stk        PACKED;
  cell reset_hea        PACKED;
  cell sysreq_d         PACKED;
  int reloc_size      PACKED;
  long code_size      PACKED;
} PACKED AMX;

typedef struct tagAMX_HEADER {
  int32_t size          PACKED;
  uint16_t magic        PACKED;
  char    file_version;
  char    amx_version;
  int16_t flags         PACKED;
  int16_t defsize       PACKED;
  int32_t cod           PACKED;
  int32_t dat           PACKED;
  int32_t hea           PACKED;
  int32_t stp           PACKED;
  int32_t cip           PACKED;
  int32_t publics       PACKED;
  int32_t natives       PACKED;
  int32_t libraries     PACKED;
  int32_t pubvars       PACKED;
  int32_t tags          PACKED;
  int32_t nametable     PACKED;
} PACKED AMX_HEADER;

#define AMX_MAGIC     0xf1e0

enum {
  AMX_ERR_NONE,
  AMX_ERR_EXIT,
  AMX_ERR_ASSERT,
  AMX_ERR_STACKERR,
  AMX_ERR_BOUNDS,
  AMX_ERR_MEMACCESS,
  AMX_ERR_INVINSTR,
  AMX_ERR_STACKLOW,
  AMX_ERR_HEAPLOW,
  AMX_ERR_CALLBACK,
  AMX_ERR_NATIVE,
  AMX_ERR_DIVIDE,
  AMX_ERR_SLEEP,
  AMX_ERR_INVSTATE,
  AMX_ERR_INVNATIVE,

  AMX_ERR_MEMORY = 16,
  AMX_ERR_FORMAT,
  AMX_ERR_VERSION,
  AMX_ERR_NOTFOUND,
  AMX_ERR_INDEX,
  AMX_ERR_DEBUG,
  AMX_ERR_INIT,
  AMX_ERR_USERDATA,
  AMX_ERR_INIT_JIT,
  AMX_ERR_PARAMS,
  AMX_ERR_DOMAIN,
  AMX_ERR_GENERAL,
};

#define AMX_FLAG_DEBUG    0x02
#define AMX_FLAG_COMPACT  0x04
#define AMX_FLAG_BYTEOPC  0x08
#define AMX_FLAG_NOCHECKS 0x10
#define AMX_FLAG_PRENIT	 0x100
#define AMX_FLAG_NTVREG 0x1000
#define AMX_FLAG_JITC   0x2000
#define AMX_FLAG_BROWSE 0x4000
#define AMX_FLAG_RELOC  0x8000

#define AMX_EXEC_MAIN   -1
#define AMX_EXEC_CONT   -2

#define AMX_USERTAG(a,b,c,d)    ((a) | ((b)<<8) | ((long)(c)<<16) | ((long)(d)<<24))

#if !defined AMX_COMPACTMARGIN
  #define AMX_COMPACTMARGIN 64
#endif

#define UD_FINDPLUGIN	3
#define UD_DEBUGGER		2
#define UD_OPCODELIST	1
#define	UD_HANDLER		0
#define	UT_NATIVE		3
#define UT_OPTIMIZER	2
#define UT_BROWSEHOOK	1
#define UT_BINLOGS		0

typedef void (*BROWSEHOOK)(AMX *amx, cell *oplist, cell *cip);

#if PAWN_CELL_SIZE==32
  #define amx_ftoc(f)   ( * ((cell*)&f) )
  #define amx_ctof(c)   ( * ((float*)&c) )
#elif PAWN_CELL_SIZE==64
  #define amx_ftoc(f)   ( * ((cell*)&f) )
  #define amx_ctof(c)   ( * ((double*)&c) )
#else
  #error Unsupported cell size
#endif

#define amx_StrParam(amx,param,result)                                      \
    do {                                                                    \
      cell *amx_cstr_; int amx_length_;                                     \
      amx_GetAddr((amx), (param), &amx_cstr_);                              \
      amx_StrLen(amx_cstr_, &amx_length_);                                  \
      if (amx_length_ > 0 &&                                                \
          ((result) = (void*)alloca((amx_length_ + 1) * sizeof(*(result)))) != NULL) \
        amx_GetString((char*)(result), amx_cstr_, sizeof(*(result))>1, amx_length_); \
      else (result) = NULL;                                                 \
    } while (0)

uint16_t * AMXAPI amx_Align16(uint16_t *v);
uint32_t * AMXAPI amx_Align32(uint32_t *v);
#if defined _I64_MAX || defined HAVE_I64
  uint64_t * AMXAPI amx_Align64(uint64_t *v);
#endif
int AMXAPI amx_Allot(AMX *amx, int cells, cell *amx_addr, cell **phys_addr);
int AMXAPI amx_Callback(AMX *amx, cell index, cell *result, cell *params);
int AMXAPI amx_CheckNatives(AMX *amx, AMX_NATIVE_FILTER nf);
int AMXAPI amx_Cleanup(AMX *amx);
int AMXAPI amx_Clone(AMX *amxClone, AMX *amxSource, void *data);
int AMXAPI amx_Exec(AMX *amx, cell *retval, int index);
int AMXAPI amx_FindNative(AMX *amx, const char *name, int *index);
int AMXAPI amx_FindPublic(AMX *amx, const char *funcname, int *index);
int AMXAPI amx_FindPubVar(AMX *amx, const char *varname, cell *amx_addr);
int AMXAPI amx_FindTagId(AMX *amx, cell tag_id, char *tagname);
int AMXAPI amx_Flags(AMX *amx,uint16_t *flags);
int AMXAPI amx_GetAddr(AMX *amx,cell amx_addr,cell **phys_addr);
int AMXAPI amx_GetNative(AMX *amx, int index, char *funcname);
int AMXAPI amx_GetPublic(AMX *amx, int index, char *funcname);
int AMXAPI amx_GetPubVar(AMX *amx, int index, char *varname, cell *amx_addr);
int AMXAPI amx_GetString(char *dest,const cell *source, int use_wchar, size_t size);
int AMXAPI amx_GetTag(AMX *amx, int index, char *tagname, cell *tag_id);
int AMXAPI amx_GetUserData(AMX *amx, long tag, void **ptr);
int AMXAPI amx_Init(AMX *amx, void *program);
int AMXAPI amx_InitJIT(AMX *amx, void *reloc_table, void *native_code);
int AMXAPI amx_MemInfo(AMX *amx, long *codesize, long *datasize, long *stackheap);
int AMXAPI amx_NameLength(AMX *amx, int *length);
AMX_NATIVE_INFO * AMXAPI amx_NativeInfo(const char *name, AMX_NATIVE func);
int AMXAPI amx_NumNatives(AMX *amx, int *number);
int AMXAPI amx_NumPublics(AMX *amx, int *number);
int AMXAPI amx_NumPubVars(AMX *amx, int *number);
int AMXAPI amx_NumTags(AMX *amx, int *number);
int AMXAPI amx_Push(AMX *amx, cell value);
int AMXAPI amx_PushArray(AMX *amx, cell *amx_addr, cell **phys_addr, const cell array[], int numcells);
int AMXAPI amx_PushString(AMX *amx, cell *amx_addr, cell **phys_addr, const char *string, int pack, int use_wchar);
int AMXAPI amx_RaiseError(AMX *amx, int error);
int AMXAPI amx_Register(AMX *amx, const AMX_NATIVE_INFO *nativelist, int number);
int AMXAPI amx_Reregister(AMX *amx, const AMX_NATIVE_INFO *nativelist, int number);
int AMXAPI amx_RegisterToAny(AMX *amx, AMX_NATIVE f);
int AMXAPI amx_Release(AMX *amx, cell amx_addr);
int AMXAPI amx_SetCallback(AMX *amx, AMX_CALLBACK callback);
int AMXAPI amx_SetDebugHook(AMX *amx, AMX_DEBUG debug);
int AMXAPI amx_SetString(cell *dest, const char *source, int pack, int use_wchar, size_t size);
int AMXAPI amx_SetUserData(AMX *amx, long tag, void *ptr);
int AMXAPI amx_StrLen(const cell *cstring, int *length);
int AMXAPI amx_UTF8Check(const char *string, int *length);
int AMXAPI amx_UTF8Get(const char *string, const char **endptr, cell *value);
int AMXAPI amx_UTF8Len(const cell *cstr, int *length);
int AMXAPI amx_UTF8Put(char *string, char **endptr, int maxchars, cell value);

// AMXX extensions (defined in amx.cpp, used by other modules)
int AMXAPI amx_GetLibraries(AMX *amx);
const char *AMXAPI amx_GetLibrary(AMX *amx, int index, char *buffer, int len);
int AMXAPI amx_SetStringOld(cell *dest, const char *source, int pack, int use_wchar);
int AMXAPI amx_GetStringOld(char *dest, const cell *source, int use_wchar);
// AMXX 控制台输出：同时写入 stdout 和引擎控制台（-log 文件）
void amxx_console_print(const char *msg);

#if PAWN_CELL_SIZE==16
  #define amx_AlignCell(v) amx_Align16(v)
#elif PAWN_CELL_SIZE==32
  #define amx_AlignCell(v) amx_Align32(v)
#elif PAWN_CELL_SIZE==64 && (defined _I64_MAX || defined HAVE_I64)
  #define amx_AlignCell(v) amx_Align64(v)
#else
  #error Unsupported cell size
#endif

#define amx_RegisterFunc(amx, name, func) \
  amx_Register((amx), amx_NativeInfo((name),(func)), 1);

#if !defined AMX_NO_ALIGN
  #if defined LINUX || defined __FreeBSD__ || defined __APPLE__
    #pragma pack()
  #elif defined MACOS && defined __MWERKS__
    #pragma options align=reset
  #else
    #pragma pack(pop)
  #endif
#endif

#ifdef  __cplusplus
}
#endif

#endif /* AMX_H_INCLUDED */