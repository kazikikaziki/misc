#include "KSysInfo.h"
#include "KConv.h"
#include "KStd.h"
//
#if defined (_WIN32)
#	include <intrin.h> // __cpuid
#	include <Shlwapi.h>
#	include <Shlobj.h> // SHGetFolderPath
#	include <Windows.h>
#elif (__x86_64__ || __i386__)
#	include <cpuid.h>
#endif

#include <locale.h>
#include <time.h>

static void K_cpuid(int *int4, int id) {
#ifdef _WIN32
	__cpuid(int4, id); // <intrin.h>
#elif (__x86_64__ || __i386__)
	__get_cpuid(id, 
		(unsigned int*)(int4 + 0),
		(unsigned int*)(int4 + 1),
		(unsigned int*)(int4 + 2),
		(unsigned int*)(int4 + 3)
	); // <cpuid.h>
#endif
}


namespace mo {
static void _GetLocalTimeString(char *s, int size) {
	time_t gmt = time(NULL);
	struct tm lt;
	K_localtime_s(&lt, gmt);
	strftime(s, size, "%y/%m/%d-%H:%M:%S", &lt);
}
static void _GetCpuName(char *s) {
	K_assert(s);
	// https://msdn.microsoft.com/ja-jp/library/hskdteyh.aspx
	// https://msdn.microsoft.com/ja-jp/library/hskdteyh(v=vs.110).aspx
	// https://gist.github.com/t-mat/3769328
	// http://caspar.hazymoon.jp/OpenBSD/annex/cpuid.html
	// https://www.timbreofprogram.info/blog/archives/951
	int x[12] = {0};
	K_cpuid(x+0, 0x80000002);
	K_cpuid(x+4, 0x80000003);
	K_cpuid(x+8, 0x80000004);
	const char *ss = (const char *)x;
	while (isspace(*ss)) ss++; // skip space
	strcpy(s, ss);
}
static void _GetCpuVendor(char *s) {
	K_assert(s);
	char x[sizeof(int) * 4 + 1]; // 終端文字を入れるために+1しておく
	memset(x, 0, sizeof(x));
	K_cpuid((int*)x, 0);
	// cpu vendoer は ebx, ecx, edx に入っている。
	// つまり x[4..16] に char として12文字文ある。さらに↑の memsetで入れた 0 が終端文字として残っている
	strcpy(s, x+4);
}
static bool _HasSSE2() {
	int x[4] = {0};
	K_cpuid(x, 1);
	return (x[3] & (1<<26)) != 0;

}


static void _GetMemorySize(int *total_kb, int *avail_kb) {
	K_assert(total_kb);
	K_assert(avail_kb);
#ifdef _WIN32
	MEMORYSTATUSEX ms = {sizeof(MEMORYSTATUSEX)};
	if (GlobalMemoryStatusEx(&ms)) {
		*total_kb = (int)(ms.ullTotalPhys / 1024);
		*avail_kb = (int)(ms.ullAvailPhys / 1024);
	}
#else
	*total_kb = 0;
	*avail_kb = 0;
#endif
}
static void _GetProductName(char *name_u8) {
#ifdef _WIN32
	K_assert(name_u8);
	// バージョン情報を取得したいが、GetVersionInfo は Win8で使えない。
	// かといって VersionHelpers.h を使おうとすると WinXPが未対応になる。
	// ならば VerifyVersionInfo を使えばよいように思えるがこれにも罠があって、
	// Win8.1以降の場合、マニフェストで動作対象に Win8なりWin10なりを明示的に指定しておかないと
	// 常に Win8 扱いになってしまう。
	// もう面倒なのでレジストリを直接見る。
	//
	// Windows 8.1 でバージョン判別するときの注意点
	// http://grabacr.net/archives/1175
	//
	HKEY hKey = NULL;
	RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &hKey);
	WCHAR data[MAX_PATH] = {0};
	DWORD dataBytes = sizeof(data);
	DWORD type = REG_SZ;
	RegQueryValueExW(hKey, L"ProductName", NULL, &type, (LPBYTE)data, &dataBytes);
	RegCloseKey(hKey);

	std::string u8 = KConv::wideToUtf8(data);
	strcpy(name_u8, u8.c_str());
#else
	strcpy(name_u8, "");
#endif
}
static void _GetSelfFileName(char *name_u8) {
#ifdef _WIN32
	wchar_t ws[MAX_PATH];
	GetModuleFileNameW(NULL, ws, MAX_PATH);
	std::string u8 = KConv::wideToUtf8(ws);
	strcpy(name_u8, u8.c_str());
#else
	strcpy(name_u8, "");
#endif
}
static void _GetFontDir(char *dir_u8) {
#ifdef _WIN32
	wchar_t ws[MAX_PATH];
	SHGetFolderPathW(NULL, CSIDL_FONTS, NULL, SHGFP_TYPE_CURRENT, ws);
	std::string u8 = KConv::wideToUtf8(ws);
	strcpy(dir_u8, u8.c_str());
#else
	strcpy(dir_u8, "");
#endif
}
static int _GetLangId() {
#ifdef _WIN32
	return GetUserDefaultUILanguage();
#else
	return 0;
#endif
}
void getSystemInfo(SystemInfo *info) {
	K_assert(info);
	memset(info, 0, sizeof(SystemInfo));
	_GetCpuName(info->cpuname);
	_GetCpuVendor(info->cpuvendor);
	_GetLocalTimeString(info->localtime, SystemInfo::MAXSIZE);
	_GetMemorySize(&info->mem_total_kb, &info->mem_avail_kb);
	_GetProductName(info->productname);
	_GetSelfFileName(info->exepath);
	K_getcwd_u8(info->curdir, sizeof(info->curdir));
	_GetFontDir(info->fontdir);
	info->langid = _GetLangId();
	info->has_sse2 = _HasSSE2();
}


} // namesapce
