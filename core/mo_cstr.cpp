#include "mo_cstr.h"
//
#include "KStd.h"
#include <string.h>
#include <stdarg.h>
#include <stdlib.h> // strtof
#ifdef _WIN32
#	include <Windows.h> // GetStringTypeW
#endif

namespace mo {


std::string c_sprintf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	std::string result = c_sprintfv(fmt, args);
	va_end(args);
	return result;
}
std::string c_sprintfv(const char *fmt, va_list args) {
#if defined(_WIN32) && 0
	std::string s(64, '\0');
	int n = vsnprintf(&s[0], s.size(), fmt, args);
	if (n < 0 || (int)s.length() <= n) {
		n = _vscprintf(fmt, args) + 1;
		s.resize(n + 1);
		n = vsnprintf(&s[0], s.size(), fmt, args);
	}
	s.resize(n); // trim trailing null characters
	return s;
#endif
	std::string s(64, '\0');
	int n = 0;
	while (1) {
		// vsnprintfを args に対して複数回呼び出すときには注意が必要。
		// args はストリームのように動作するため、args の位置をリセットしなければならない
		// args に対しては va_start することができないので、毎回コピーを取り、それを使うようにする
		va_list ap;
		va_copy(ap, args);
		n = vsnprintf(&s[0], s.size(), fmt, ap);
		va_end(ap);
		
		// vsnprintfは書き込んだ文字数を返す（null文字含めない）
		// ただし、バッファが足りなかった場合ｍMicrosoft版では -1 を返す事に注意
		// null含めた文字数がバッファサイズぴったりだった場合は、サイズが足りなかったものとする
		// （必ず、最低１バイトの余裕がないと安心できない）
		if (n >= 0 && n+1 < (int)s.size()) {
			break;
		}
		s.resize(s.size() + 256);
	}

	// この時点で、std::string のサイズが実際の文字列長さよりも大きくなっている可能性に注意。
	// std::string::size は文字列末尾の '\0' なども含めたバッファサイズを返すため、
	// バッファサイズと文字列長さを明示的に同じ値にしておく
	if (n > 0) s.resize(n);

	return s;
}

bool c_streq(const char *a, const char *b) {
	return strcmp(a, b) == 0;
}
bool c_strempty(const char *a) {
	return a==NULL || a[0]==0;
}
bool c_strempty(const wchar_t *a) {
	return a==NULL || a[0]==0;
}
void c_strcpy(char *dst, int dst_size, const char *src) {
	if (dst == NULL || dst_size < 1 || src == NULL) {
		K_assert(0 && "invalid arguments for c_strcpy");
		return;
	}
	for (int i=0; i<dst_size; i++) {
		dst[i] = src[i];
		if (src[i] == '\0') return;
	}
	// overrun
	dst[dst_size-1] = '\0';
	K_assert(0 && "Destination buffer is not enough: c_strcpy");
}
void c_strcat(char *dst, int dst_size, const char *add) {
	if (dst == NULL || dst_size < 1 || add == NULL) {
		K_assert(0 && "invalid arguments for c_strcat");
		return;
	}
	int len = (int)strlen(dst);
	if (len >= dst_size) {
		K_assert(0 && "invalid string length for c_strcat");
		return;
	}
	for (int i=0; len+i < dst_size; i++) {
		dst[len+i] = add[i];
		if (add[i] == '\0') return;
	}
	// overrun
	dst[dst_size-1] = '\0';
	K_assert(0 && "Destination buffer is not enough: c_strcat");
}
/// 文字列 A + B が C と等しいとき、 c_startswith(C, A) は true を返す。
/// すなわち、 c_startswith(C, "") は常に true を返す。
bool c_startswith(const char *str, const char *sub, bool ignore_case) {
	if (str == NULL || sub == NULL) {
		return false;
	}
	if (sub[0] == '\0') {
		return true;
	}
	const char *s1 = str;
	const char *s2 = sub;
	size_t len1 = strlen(s1);
	size_t len2 = strlen(s2);
	if (len1 < len2) {
		return false;
	}
	if (ignore_case) {
		for (size_t i=0; i<len2; i++) {
			char a = s1[i];
			char b = s2[i];
			if (tolower(a) != tolower(b)) {
				return false;
			}
		}
	} else {
		for (size_t i=0; i<len2; i++) {
			char a = s1[i];
			char b = s2[i];
			if (a != b) {
				return false;
			}
		}
	}
	return true;
}
bool c_startswith(const std::string &str, const std::string &substr, bool ignore_case) {
	return c_startswith(str.c_str(), substr.c_str(), ignore_case);
}

/// 文字列 A + B が C と等しいとき、 c_endswith(C, B) は true を返す。
/// すなわち、 c_endswith(C, "") は常に true を返す。
bool c_endswith(const char *str, const char *sub, bool ignore_case) {
	if (str == NULL || sub == NULL) {
		return false;
	}
	if (sub[0] == '\0') {
		return true;
	}
	const char *s1 = str;
	const char *s2 = sub;
	size_t len1 = strlen(s1);
	size_t len2 = strlen(s2);
	if (len1 < len2) {
		return false;
	}
	size_t off = len1 - len2;
	if (ignore_case) {
		for (size_t i=0; i<len2; i++) {
			char a = s1[off+i];
			char b = s2[i];
			if (tolower(a) != tolower(b)) {
				return false;
			}
		}
	} else {
		for (size_t i=0; i<len2; i++) {
			char a = s1[off+i];
			char b = s2[i];
			if (a != b) {
				return false;
			}
		}
	}
	return true;
}
bool c_endswith(const std::string &str, const std::string &substr, bool ignore_case) {
	return c_endswith(str.c_str(), substr.c_str(), ignore_case);
}

bool c_strtof_try(const char *s, float *val) {
	// 空文字列または空白文字だけで構成されている場合、
	// strtol系関数はエラーにならない。事前にチェックが必要
	if (s == NULL || s[0] == '\0' || isblank(s[0])) {
		return false;
	}
	char *err = NULL;
	float value = strtof(s, &err);
	if (err && *err) {
		return false;
	}
	if (val) *val = value;
	return true;
}
float c_strtof(const char *s, float def) {
	float val = def;
	c_strtof_try(s, &val);
	return val;
}
float c_wstrtof(const wchar_t *ws, float def) {
	char s[32];
	wcstombs(s, ws, 32);
	return c_strtof(s, def);
}
bool c_strtoi_try(const char *s, int *val) {
	// 空文字列または空白文字だけで構成されている場合、
	// strtol系関数はエラーにならない。事前にチェックが必要
	if (s == NULL || s[0] == '\0' || isblank(s[0])) {
		return false;
	}
	char *err = NULL;
	int value = 0;
	if (c_startswith(s, "0x")) {
		// 16進数の場合は符号なしで変換する
		// stdtol では、例えば 0xFF0000FF などは正しく変換できない
		value = (int)strtoul(s, &err, 0);
	} else {
		value = (int)strtol(s, &err, 0);
	}
	if (err && *err) {
		return false;
	}
	if (val) *val = value;
	return true;
}
int c_strtoi(const char *s, int def) {
	int val = def;
	c_strtoi_try(s, &val);
	return val;
}
int c_wstrtoi(const wchar_t *ws, int def) {
	char s[32];
	wcstombs(s, ws, 32);
	return c_strtoi(s, def);
}
bool c_strtoui_try(const char *s, unsigned int *val) {
	// 空文字列または空白文字だけで構成されている場合、
	// strtol系関数はエラーにならない。事前にチェックが必要
	if (s == NULL || s[0] == '\0' || isblank(s[0])) {
		return false;
	}
	char *err = NULL;
	unsigned int value = (unsigned int)strtoul(s, &err, 0);
	if (err && *err) {
		return false;
	}
	if (val) *val = value;
	return true;
}
unsigned int c_strtoui(const char *s, unsigned int def) {
	unsigned int val = def;
	c_strtoui_try(s, &val);
	return val;
}

void c_strcatf(char *s, const char *fmt, ...) {
	K_assert(s);
	K_assert(fmt);
	va_list args;
	va_start(args, fmt);
	c_strcatv(s, fmt, args);
	va_end(args);
}
void c_strcatv(char *s, const char *fmt, va_list args) {
	K_assert(s);
	K_assert(fmt);
	size_t n = strlen(s);
	vsprintf(s + n, fmt, args);
}
void c_wstrcatf(wchar_t *s, size_t size, const wchar_t *fmt, ...) {
	K_assert(s);
	K_assert(fmt);
	va_list args;
	va_start(args, fmt);
	c_wstrcatv(s, size, fmt, args);
	va_end(args);
}
void c_wstrcatv(wchar_t *s, size_t size, const wchar_t *fmt, va_list args) {
	K_assert(s);
	K_assert(fmt);
	size_t n = wcslen(s);
#ifdef _WIN32
	_vswprintf(s + n, fmt, args);
#else
	vswprintf(s + n, size, fmt, args);
#endif
}

// ★★time_t が 64 ビットである可能性に注意★★
// time_t を %d で文字列化してしまうと、time_t の上位32ビットだけが %d で処理され、
// 残りの 32 ビットが次の書式化符号によって処理されてしまう
void c_timetostr64(char *s, time_t t) {
	uint32_t hi = (uint32_t)(t >> 32);
	uint32_t lo = (uint32_t)(t & 0xFFFFFFFF);
	if (1) {
		if (hi > 0) {
			sprintf(s, "0x%x%08x", hi, lo);
		} else {
			sprintf(s, "0x%x", lo);
		}
	} else {
		sprintf(s, "0x%08x%08x", hi, lo);
	}
}
time_t c_strtotime64(const char *s) {
	time_t t = 0;
	if (mo::c_startswith(s, "0x")) {
		for (const char *c=s+2; *c; c++) {
			int i;
			if (isdigit(*c)) {
				i = *c - '0';
			} else if ('a' <= *c && *c <= 'f') {
				i = *c - 'a' + 10;
			} else if ('A' <= *c && *c <= 'F') {
				i = *c - 'A' + 10;
			} else {
				break;
			}
			t = (t << 4) | i;
		}
	} else if (isdigit(s[0])) {
		for (const char *c=s; *c; c++) {
			int i;
			if (isdigit(*c)) {
				i = *c - '0';
			} else {
				break;
			}
			t = t * 10 + i;
		}
	}
	return t;
}
void c_strreplace(std::string &s, int start, int count, const char *str) {
	s.erase(start, count);
	s.insert(start, str);
}
void c_strreplace(std::string &s, const char *before, const char *after) {
	if (c_strempty(before)) return;
	int pos = c_strfind(s.c_str(), before, 0);
	while (pos >= 0) {
		c_strreplace(s, pos, strlen(before), after);
		pos += strlen(after);
		pos = c_strfind(s.c_str(), before, pos);
	}
}
void c_strreplace(std::wstring &s, int start, int count, const wchar_t *str) {
	s.erase(start, count);
	s.insert(start, str);
}
void c_strreplace(std::wstring &s, const wchar_t *before, const wchar_t *after) {
	if (c_strempty(before)) return;
	int pos = c_strfind(s.c_str(), before, 0);
	while (pos >= 0) {
		c_strreplace(s, pos, wcslen(before), after);
		pos += wcslen(after);
		pos = c_strfind(s.c_str(), before, pos);
	}
}
void c_strreplace(char *s, char before, char after) {
	for (size_t i=0; s[i]; i++) {
		if (s[i] == before) {
			s[i] = after;
		}
	}
}
void c_strreplace(wchar_t *s, wchar_t before, wchar_t after) {
	for (size_t i=0; s[i]; i++) {
		if (s[i] == before) {
			s[i] = after;
		}
	}
}


int c_strfind(const char *s, const char *sub, int start) {
	if (start < 0) start = 0;
	if ((int)strlen(s) < start) return -1; // 範囲外
	if ((int)strlen(s) == start) return c_strempty(sub) ? start : -1; // 検索開始位置が文字列末尾の場合、空文字列だけがマッチする
	if (c_strempty(sub)) return start; // 空文字列はどの場所であっても必ずマッチする。常に検索開始位置にマッチする
	const char *p = strstr(s + start, sub);
	return p ? (p - s) : -1;
}
int c_strfind(const std::string &s, const std::string &sub, int start) {
	return c_strfind(s.c_str(), sub.c_str(), start);
}
int c_strfind(const wchar_t *s, const wchar_t *sub, int start) {
	if (start < 0) start = 0;
	if ((int)wcslen(s) < start) return -1; // 範囲外
	if ((int)wcslen(s) == start) return c_strempty(sub) ? start : -1; // 検索開始位置が文字列末尾の場合、空文字列だけがマッチする
	if (c_strempty(sub)) return start; // 空文字列はどの場所であっても必ずマッチする。常に検索開始位置にマッチする
	const wchar_t *p = wcsstr(s + start, sub);
	return p ? (p - s) : -1;
}
int c_strfind(const std::wstring &s, const std::wstring &sub, int start) {
	return c_strfind(s.c_str(), sub.c_str(), start);
}


int c_strfindchar(const char *s, char chr, int start) {
	if (start < 0) start = 0;
	if ((int)strlen(s) <= start) return -1;
	const char *p = strchr(s, chr);
	return p ? (p - s) : -1;
}
int c_strfindchar(const std::string &s, char chr, int start) {
	return c_strfindchar(s.c_str(), chr, start);
}
int c_strfindchar(const wchar_t *s, wchar_t chr, int start) {
	if (start < 0) start = 0;
	if ((int)wcslen(s) <= start) return -1;
	const wchar_t *p = wcschr(s, chr);
	return p ? (p - s) : -1;
}
int c_strfindchar(const std::wstring &s, wchar_t chr, int start) {
	return c_strfindchar(s.c_str(), chr, start);
}
void c_strtrim(std::string &text) {
	if (text.empty()) return;

	int len = text.size();
	int s = 0;
	int e = len - 1;
	const char *str = text.c_str();
	for (;           isspace(str[s]); s++) {}
	for (; e >= s && isspace(str[e]); e--) {}
	e++; // 最後の非空白文字の次を指すように調整
	K_assert(s >= 0);
	K_assert(e - s >= 0);
	if (s == 0 && e == len) {
		return; // No trim
	} else {
		text.erase(0, s);
		text.erase((size_t)(e-s), (size_t)(-1));
		return;
	}
}

void Test_cstr() {
//	const char *text = "あのイーハトーヴォのすきとおった風";
//	char s[256];
//	strcpy(s, text);
//	size_t len = strlen(s);
//	K_strxor(s, len, "PASS"); // 暗号
//	K_assert(strcmp(s, text) != 0);
//	K_strxor(s, len, "PASS"); // 復号
//	K_assert(strcmp(s, text) == 0);
}


}
