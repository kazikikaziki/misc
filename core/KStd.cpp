#include "KStd.h"
#include <assert.h>
#include <locale.h>
#include <sstream> // std::istringstream
#include <iomanip> // std::get_time
#ifdef _WIN32
#	include <Windows.h>
#else
#	include <unistd.h> // getcwd, access, getpid, chdir
#endif

#ifdef ANDROID_NDK
#	include <android/log.h>
#endif

void K_raise_assertion(const char *expr, const char *file, int line) {
#if defined (_WIN32)
	__debugbreak();
#elif defined (ANDROID_NDK)
	__android_log_assert("assert", "KStd", "ASSERT in %s(%d) : %s", file, line, expr);
#else
	printf("ASSERT in %s(%d) : %s", file, line, expr);
#endif
}

uint64_t K_gettime_nano() {
#ifdef _WIN32
	static LARGE_INTEGER freq = {0, 0};
	if (freq.QuadPart == 0) {
		QueryPerformanceFrequency(&freq);
	}

	LARGE_INTEGER time;
	{
		// https://msdn.microsoft.com/ja-jp/library/cc410966.aspx
		// マルチプロセッサ環境の場合、QueryPerformanceCounter が必ず同じプロセッサで実行されるように固定する必要がある
		// SetThreadAffinityMask を使い、指定したスレッドを実行可能なCPUを限定する
		HANDLE hThread = GetCurrentThread();
		DWORD_PTR oldmask = SetThreadAffinityMask(hThread, 1);
		QueryPerformanceCounter(&time);
		SetThreadAffinityMask(hThread, oldmask);
	}
	return (uint64_t)((double)time.QuadPart / freq.QuadPart * 1000000000.0);
#else
	struct timespec time;
	clock_gettime(CLOCK_MONOTONIC, &time);
	return (uint64_t)time.tv_sec * 1000000000 + time.tv_nsec;
#endif
}
uint32_t K_gettime_msec() {
	return (uint32_t)(K_gettime_nano() / 1000 / 1000);
}

uint32_t K_getpid() {
#ifdef _WIN32
	return GetCurrentProcessId();
#else
	return getpid();
#endif
}

void K_getcwd_u8(char *path, size_t size) {
	K_assert(path);
	#ifdef _WIN32
	{
		wchar_t wpath[MAX_PATH] = {0};
		GetCurrentDirectoryW(MAX_PATH, wpath);
		WideCharToMultiByte(CP_UTF8, 0, wpath, -1, path, size, NULL, NULL);
	}
	#else
	{
		return getcwd(path, size);
	}
	#endif
}
bool K_chdir_u8(const char *path) {
	#ifdef _WIN32
	{
		wchar_t wpath[MAX_PATH] = {0};
		MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, MAX_PATH);
		if (SetCurrentDirectoryW(wpath)) {
			OutputDebugStringW(L"cdir: ");
			OutputDebugStringW(wpath);
			OutputDebugStringW(L"\n");
			return true;
		}
		return false;
	}
	#else
	{
		return chdir(path) == 0;
	}
	#endif
}

FILE * K_fopen_u8(const char *path, const char *mode) {
	K_assert(path);
	K_assert(mode);
	#ifdef _WIN32
	{
		wchar_t wpath[MAX_PATH] = {0};
		wchar_t wmode[MAX_PATH] = {0};
		MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, MAX_PATH);
		MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, MAX_PATH);
		return _wfopen(wpath, wmode);
	}
	#else
	{
		return fopen(file, mode);
	}
	#endif
}

size_t K_wcstombs_l(char *mb, const wchar_t *ws, size_t max_mb_bytes, const char *_locale) {
#ifdef _WIN32
	{
		size_t retval;
		_locale_t loc = _create_locale(LC_CTYPE, _locale);
		retval = _wcstombs_l(mb, ws, max_mb_bytes, loc);
		_free_locale(loc);
		return retval;
	}
#else
	{
		static std::mutex s_mutex;
		size_t retval;
		s_mutex.lock();
		{
			char loc[64];
			strcpy(loc, setlocale(LC_CTYPE, NULL));
			setlocale(LC_CTYPE, _locale);
			retval = wcstombs(mb, ws, max_mb_bytes);
			setlocale(LC_CTYPE, loc);
		}
		s_mutex.unlock();
		return retval;
	}
#endif
}

void K_localtime_s(struct tm *out_tm, time_t gmt) {
#ifdef _WIN32
	localtime_s(out_tm, &gmt);
#else
	localtime_r(&gmt, out_tm);
#endif
}

void K_current_localtime(struct tm *out_tm, int *out_msec) {
#ifdef _WIN32
	{
		SYSTEMTIME st;
		GetLocalTime(&st);
		if (out_tm) {
			memset(out_tm, 0, sizeof(struct tm));
			out_tm->tm_year = st.wYear - 1900; // years since 1900
			out_tm->tm_mon  = st.wMonth - 1;   // tm_mon  [0, 11]
			out_tm->tm_mday = st.wDay;         // tm_mday [1, 31]
			out_tm->tm_hour = st.wHour;        // tm_hour [0, 23]
			out_tm->tm_min  = st.wMinute;      // tm_min  [0, 59]
			out_tm->tm_sec  = st.wSecond;      // tm_sec  [0, 60]
		}
		if (out_msec) {
			*out_msec = st.wMilliseconds;       // msec [0, 999]
		}
	}
#else
	{
		// https://www.mm2d.net/main/prog/c/time-01.html
		if (out_tm) {
			// 秒単位までは普通に time, localtime で取得する
			time_t gmt = ::time(NULL);
			localtime_r(&gmt, out_tm);
		}
		if (out_msec) {
			// ミリ秒単位まで要求された場合
			struct timespec ts;
			clock_gettime(CLOCK_REALTIME, &ts);
			// CLOCK_REALTIME で取得した時刻の「分」「秒」は time() で取得した値と同じになるので、
			// おそらくミリ秒の値も time_t の秒数と同期しているハズ
			*out_msec = ts.tv_nsec / 1000 / 1000;
		}
	}
#endif
}

bool K_iswprint(wchar_t wc) {
#ifdef _WIN32
	WORD t = 0;
	GetStringTypeW(CT_CTYPE1, &wc, 1, &t);
	return (t & C1_DEFINED) && !(t & C1_CNTRL);
#else
	// MSVC以外の場合、locale がちゃんと設定されていれば iswprint は期待通りに動作するが、
	// locale 未設定だと ascii と重複する範囲でしか正常動作しないので注意
	K_assert(0 && "ここのコードは未検証であり、正常動作する保証はない");
	// return iswprint(wc);
	return !iswcntrl(wc);
#endif
}

int K_iswgrapg(wchar_t wc) {
#ifdef _WIN32
	WORD t = 0;
	GetStringTypeW(CT_CTYPE1, &wc, 1, &t);
	return (t & C1_DEFINED) && !(t & C1_CNTRL) && !(t & C1_SPACE);
#else
	// MSVC以外の場合、locale がちゃんと設定されていれば iswprint は期待通りに動作するが、
	// locale 未設定だと ascii と重複する範囲でしか正常動作しないので注意
	K_assert(0 && "ここのコードは未検証であり、正常動作する保証はない");
	// return iswgraph(wc);
	return !iswcntrl(wc) && !iswspace(wc);
#endif
}

void K_strcpy(char *dst, int size, const char *src) {
	// 文字列長さが指定長さを超えた続行不可能なエラー扱いにする。
	// strcpy_s や strlcpy は「黙って」「勝手に」文字列を切り捨ててしまうため、
	// 気づきにくいバグの原因になる可能性がある。
	if ((int)strlen(src) >= size) {
		K_assert(0);
		exit(-1); // 続行不可能により構成停止
		return;
	}
	strcpy(dst, src);
}

int K_stricmp(const char *s1, const char *s2) {
	K_assert(s1);
	K_assert(s2);
#ifdef _WIN32
	return _stricmp(s1, s2);
#else
	return strcasecmp(s1, s2);
#endif
}

int K_strnicmp(const char *s1, const char *s2, int maxsize) {
	K_assert(s1);
	K_assert(s2);
#ifdef _WIN32
	return _strnicmp(s1, s2, maxsize);
#else
	return strncasecmp(s1, s2, maxsize);
#endif
}

char * K_strptime_l(const char *str, const char *fmt, struct tm *out_tm, const char *_locale) {
	K_assert(str);
	K_assert(fmt);
	K_assert(out_tm);
	K_assert(_locale);
#ifdef _WIN32
	// strptime は Visual Studio では使えないので代替関数を用意する
	// https://code.i-harness.com/ja/q/4e939
	std::istringstream input(str);
	input.imbue(std::locale(_locale));
	input >> std::get_time(out_tm, fmt);
	if (input.fail()) return NULL;
	return (char*)(str + (int)input.tellg()); // 戻り値は解析済み文字列の次の文字
#else
	return strptime(str, fmt, out_tm);
#endif
}

int K_vswprintf(wchar_t *out, size_t outsize, const wchar_t *fmt, va_list args) {
	K_assert(out);
	K_assert(fmt);
#ifdef _WIN32
	return _vsnwprintf(out, outsize, fmt, args);
#else
	return vswprintf(out, outsize, fmt, args);
#endif
}

bool K_strtoi_try(const char *s, int *val) {
	K_assert(s);
	// strtol 系関数は変換に失敗した場合、変換できなかった文字へのポインタをセットする
	// 空文字列を渡した場合は '\0' がセットされているが、それでは正常終了し時と区別がつかないため事前に弾いておく
	if (s[0]) {
		char *err = NULL;
		int value = (int)strtol(s, &err, 0);
		if (err[0] == '\0') {
			if (val) *val = value;
			return true;
		}
	}
	return false;
}
int K_strtoi(const char *s) {
	int val = 0;
	K_strtoi_try(s, &val);
	return val;
}

bool K_strtoui_try(const char *s, unsigned int *val) {
	K_assert(s);
	// strtol 系関数は変換に失敗した場合、変換できなかった文字へのポインタをセットする
	// 空文字列を渡した場合は '\0' がセットされているが、それでは正常終了し時と区別がつかないため事前に弾いておく
	if (s[0]) {
		char *err = NULL;
		unsigned int value = (unsigned int)strtoul(s, &err, 0);
		if (err[0] == '\0') {
			if (val) *val = value;
			return true;
		}
	}
	return false;
}
unsigned int K_strtoui(const char *s) {
	unsigned int val = 0;
	K_strtoui_try(s, &val);
	return val;
}

bool K_strtof_try(const char *s, float *val) {
	K_assert(s);
	// strtol 系関数は変換に失敗した場合、変換できなかった文字へのポインタをセットする
	// 空文字列を渡した場合は '\0' がセットされているが、それでは正常終了し時と区別がつかないため事前に弾いておく
	if (s[0]) {
		char *err = NULL;
		float value = strtof(s, &err);
		if (err[0] == '\0') {
			if (val) *val = value;
			return true;
		}
	}
	return false;
}
float K_strtof(const char *s) {
	float val = 0;
	K_strtof_try(s, &val);
	return val;
}




std::string K_tostring(int x) {
#ifdef ANDROID_NDK
	char s[64];
	sprintf(s, "%d", x);
	return std::string(s);
#else
	return std::to_string(x);
#endif
}

std::string K_tostring(unsigned int x) {
#ifdef ANDROID_NDK
	char s[64];
	sprintf(s, "%u", x);
	return std::string(s);
#else
	return std::to_string(x);
#endif
}

std::string K_tostring(float x) {
#ifdef ANDROID_NDK
	char s[64];
	sprintf(s, "%f", x);
	return std::string(s);
#else
	return std::to_string(x);
#endif
}
std::string K_tostring(double x) {
#ifdef ANDROID_NDK
	char s[64];
	sprintf(s, "%lf", x);
	return std::string(s);
#else
	return std::to_string(x);
#endif
}
std::string K_sprintf(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	std::string result = K_vsprintf(fmt, args);
	va_end(args);
	return result;
}
std::string K_vsprintf(const char *fmt, va_list args) {
	// 長さ取得
	int len = 0;
	{
		// vsnprintfを args に対して複数回呼び出すときには注意が必要。
		// args はストリームのように動作するため、args の位置をリセットしなければならない
		// args に対しては va_start することができないので、毎回コピーを取り、それを使うようにする
		va_list ap;
		va_copy(ap, args);
		len = vsnprintf(NULL, 0, fmt, ap);
		va_end(ap);
	}
	if (len > 0) {
		std::string s(len+1, 0); // vsprintf はヌル文字も書き込むため、len+1 確保しないといけない
		int n = vsnprintf(&s[0], s.size(), fmt, args);
		s.resize(n);
		return s;
	} else {
		return "";
	}
}

int K_strpos(const char *s, const char *sub, int start) {
	K_assert(s);
	K_assert(sub);
	if (start < 0) start = 0;
	if ((int)strlen(s) < start) return -1; // 範囲外
	if ((int)strlen(s) == start) return sub[0]=='\0' ? start : -1; // 検索開始位置が文字列末尾の場合、空文字列だけがマッチする
	if (sub[0]=='\0') return start; // 空文字列はどの場所であっても必ずマッチする。常に検索開始位置にマッチする
	const char *p = strstr(s + start, sub);
	return p ? (p - s) : -1;
}
int K_strpos(const std::string &s, const std::string &sub, int start) {
	return K_strpos(s.c_str(), sub.c_str(), start);
}
int K_wcspos(const wchar_t *s, const wchar_t *sub, int start) {
	K_assert(s);
	K_assert(sub);
	if (start < 0) start = 0;
	if ((int)wcslen(s) < start) return -1; // 範囲外
	if ((int)wcslen(s) == start) return sub[0]=='\0' ? start : -1; // 検索開始位置が文字列末尾の場合、空文字列だけがマッチする
	if (sub[0]=='\0') return start; // 空文字列はどの場所であっても必ずマッチする。常に検索開始位置にマッチする
	const wchar_t *p = wcsstr(s + start, sub);
	return p ? (p - s) : -1;
}
int K_wcspos(const std::wstring &s, const std::wstring &sub, int start) {
	return K_wcspos(s.c_str(), sub.c_str(), start);
}

void K_strrep(std::string &s, int start, int count, const char *newstr) {
	K_assert(newstr);
	s.erase(start, count);
	s.insert(start, newstr);
}
void K_strrep(std::string &s, const char *oldstr, const char *newstr) {
	K_assert(oldstr);
	K_assert(newstr);
	if (oldstr[0]==L'\0') return;
	int pos = K_strpos(s.c_str(), oldstr, 0);
	while (pos >= 0) {
		K_strrep(s, pos, strlen(oldstr), newstr);
		pos += strlen(newstr);
		pos = K_strpos(s.c_str(), oldstr, pos);
	}
}
void K_strrep(char *s, char oldchar, char newchar) {
	K_assert(s);
	for (size_t i=0; s[i]; i++) {
		if (s[i] == oldchar) {
			s[i] = newchar;
		}
	}
}
void K_wcsrep(std::wstring &s, int start, int count, const wchar_t *newstr) {
	K_assert(newstr);
	s.erase(start, count);
	s.insert(start, newstr);
}
void K_wcsrep(std::wstring &s, const wchar_t *oldstr, const wchar_t *newstr) {
	K_assert(oldstr);
	K_assert(newstr);
	if (oldstr[0]==L'\0') return;
	int pos = K_wcspos(s.c_str(), oldstr, 0);
	while (pos >= 0) {
		K_wcsrep(s, pos, wcslen(oldstr), newstr);
		pos += wcslen(newstr);
		pos = K_wcspos(s.c_str(), oldstr, pos);
	}
}
void K_wcsrep(wchar_t *s, wchar_t oldchar, wchar_t newchar) {
	K_assert(s);
	for (size_t i=0; s[i]; i++) {
		if (s[i] == oldchar) {
			s[i] = newchar;
		}
	}
}
bool K_strstarts(const char *s, const char *sub) {
	K_assert(s);
	K_assert(sub);
	return strncmp(s, sub, strlen(sub)) == 0;
}
bool K_strends(const char *s, const char *sub) {
	K_assert(s);
	K_assert(sub);
	size_t slen = strlen(s);
	size_t sublen = strlen(sub);
	if (slen >= sublen) {
		return strncmp(s + slen - sublen, sub, sublen) == 0;
	}
	return false;
}
void K_strxor(char *s, size_t len, const char *key) {
	K_assert(s);
	K_assert(key && key[0]); // 長さ１以上の文字列であること
	size_t n = strlen(key);
	for (size_t i=0; i<len; i++) {
		s[i] = s[i] ^ key[i % n];
	}
}

// 前後の空白を削除
void K_trim(string_range_t &str) {
	K_assert(str.s);
	K_assert(str.e);
	while (isascii(*str.s) && isblank(*str.s)) {
		str.s++;
	}
	while (str.s < str.e && isascii(str.e[-1]) && isblank(str.e[-1])) {
		str.e--;
	}
}

// 文字列 s を pos を境にして分割する
static bool K_strsplit_pchar(const string_range_t &str, string_range_t &left, const char *pos, size_t poslen, string_range_t &right, bool trim) {
	if (pos) {
		left.s = str.s;
		left.e = pos;
		right.s = pos + poslen; // 区切り文字の次からコピー
		right.e = str.e;
		if (trim) K_trim(left);
		if (trim) K_trim(right);
		return true;

	} else {
		left = str;
		if (trim) K_trim(left);
		return false;
	}
}

bool K_strsplit_char(const char *s, std::string *left, char separator, std::string *right, bool trim) {
	K_assert(s);
	const char *pos = strchr(s, separator);
	string_range_t lstr, rstr;
	if (K_strsplit_pchar(string_range_t(s), lstr, pos, 1, rstr, trim)) {
		if (left) left->assign(lstr.s, lstr.e - lstr.s);
		if (right) right->assign(rstr.s, rstr.e - rstr.s);
		return true;
	} else {
		if (left) left->assign(lstr.s, lstr.e - lstr.s);
		return false;
	}
}

bool K_strsplit_str(const char *s, std::string *left, const char *separator_word, std::string *right, bool trim) {
	K_assert(s);
	const char *pos = strstr(s, separator_word);
	string_range_t lstr, rstr;
	if (K_strsplit_pchar(string_range_t(s), lstr, pos, strlen(separator_word), rstr, trim)) {
		if (left) left->assign(lstr.s, lstr.e - lstr.s);
		if (right) right->assign(rstr.s, rstr.e - rstr.s);
		return true;
	} else {
		if (left) left->assign(lstr.s, lstr.e - lstr.s);
		return false;
	}
}

bool K_strsplit_rchar(const char *s, std::string *left, char separator, std::string *right, bool trim) {
	K_assert(s);
	const char *pos = strrchr(s, separator);
	string_range_t lstr, rstr;
	if (K_strsplit_pchar(string_range_t(s), lstr, pos, 1, rstr, trim)) {
		if (left) left->assign(lstr.s, lstr.e - lstr.s);
		if (right) right->assign(rstr.s, rstr.e - rstr.s);
		return true;
	} else {
		if (left) left->assign(lstr.s, lstr.e - lstr.s);
		return false;
	}
}
bool K_strsplit_rstr(const char *s, std::string *left, const char *separator_word, std::string *right, bool trim) {
	K_assert(s);
	const char *pos = NULL;
	size_t seplen = strlen(separator_word);
	{
		size_t slen = strlen(s);
		if (slen >= seplen) {
			for (size_t i=slen-seplen; i>=0; i--) {
				if (strncmp(s + i, separator_word, seplen) == 0) {
					pos = s + i;
					break;
				}
			}
		}
	}
	string_range_t lstr, rstr;
	if (K_strsplit_pchar(string_range_t(s), lstr, pos, seplen, rstr, trim)) {
		if (left) left->assign(lstr.s, lstr.e - lstr.s);
		if (right) right->assign(rstr.s, rstr.e - rstr.s);
		return true;
	} else {
		if (left) left->assign(lstr.s, lstr.e - lstr.s);
		return false;
	}
}


