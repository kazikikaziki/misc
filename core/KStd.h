#pragma once
#include <inttypes.h>
#include <stdio.h>
#include <time.h>
#include <string>
#include <vector>

#if 1
#ifndef NO_ASSERT
#  define K_assert(x)     (!(x) ? K_raise_assertion(#x, __FILE__, __LINE__) : (void)0)
#endif
#else
#ifndef NO_ASSERT
#  if defined(_WIN32)
#    define K_assert(x)       (!(x) ? __debugbreak() : (void)0)
#
#  elif defined (ANDROID_NDK)
#    include <android/log.h>
#    define K_assert(x)       (!(x) ? __android_log_assert("assert", "KStd", "ASSERT in '%s' at %d.", __FILE__, __LINE__ ) : (void)0)
#
#  else
#    include <assert.h>
#    define K_assert(x)       assert(x)
#
#  endif
#else
#    define K_assert(x)       {}
#endif
#endif

/// 関数版のアサート
void K_raise_assertion(const char *expr, const char *file, int line);

/// 現在のシステム時刻をナノ秒単位で返す
uint64_t K_gettime_nano();

/// 現在のシステム時刻をミリ秒単位で返す
uint32_t K_gettime_msec();

/// 現在のプロセスIDを返す
/// POSIX関数の getpid と同じ
uint32_t K_getpid();

/// カレントディレクトリを UTF-8 で取得する
void K_getcwd_u8(char *path, size_t size);

/// カレントディレクトリを UTF-8 で指定する
bool K_chdir_u8(const char *path);

/// 標準関数の fopen と同じだが、ファイル名を UTF-8 で指定する
FILE * K_fopen_u8(const char *path, const char *mode);

/// 世界標準時刻をローカル時刻に変換する
/// out_tm ローカル時刻
/// gmt    世界標準時刻。gmt は time() で取得する
void K_localtime_s(struct tm *out_tm, time_t gmt);

/// 現在のローカル時刻をミリ秒単位で取得する
/// out_tm ローカル時刻（秒単位まで）。不要なら NULL でもよい
/// out_ms ミリ秒 (0..999)。不要なら NULL でもよい
void K_current_localtime(struct tm *out_tm, int *out_msec);

/// 標準関数の iswprint と同じだが、ロケールを設定していなくてもよい
bool K_iswprint(wchar_t wc);

/// strcpy_s/strlcpy と似ているが、指定長さを超えた場合は全くコピーを行わない
void K_strcpy(char *dst, int size, const char *src);

/// strcmp と同じだが大小文字を無視する。
/// MSVC の stricmp, POSIX の strcasecmp と同じ
int K_stricmp(const char *s1, const char *s2);

/// strncmp と同じだが大小文字を無視する。
/// MSVC の strnicmp, POSIX の strncasecmp と同じ
int K_strnicmp(const char *s1, const char *s2, int maxsize);

/// 文字列 s を日付書式 fmt にしたがって時刻値に変換する。
/// POSIX の strptime にロケールを追加したものと同じ
char * K_strptime_l(const char *str, const char *fmt, struct tm *out_tm, const char *_locale);

/// vsprintf のワイド版。
/// MSVC の _vsnwprintf, POSIX の vswprintf と同じ
int K_vswprintf(wchar_t *out, size_t outsize, const wchar_t *fmt, va_list args);

/// strtol を int にして簡略化したもの
int K_strtoi(const char *s);

/// strtol を int にして簡略化したもの。
/// 数値への変換が成功すれば val に値をセットして true を返す。
/// 失敗した場合は何もせずに false を返す
bool K_strtoi_try(const char *s, int *val);

/// strtoul を unsigned int にして簡略化したもの
unsigned int K_strtoui(const char *s);

/// strtoul を unsigned int にして簡略化したもの。
/// 数値への変換が成功すれば val に値をセットして true を返す。
/// 失敗した場合は何もせずに false を返す
bool K_strtoui_try(const char *s, unsigned int *val);

/// strtof を簡略化したもの
float K_strtof(const char *s);

/// strtof を簡略化したもの。
/// 数値への変換が成功すれば val に値をセットして true を返す。
/// 失敗した場合は何もせずに false を返す
bool K_strtof_try(const char *s, float *val);

/// start, count で指定された範囲の部分文字列を str で置換する。
/// 置換前と置換後の文字数が変化してもよい。
/// start には 0 以上 size() 未満の開始インデックスを指定する。
/// count には文字数を指定する。-1 を指定した場合、残り全ての文字を対象にする。
/// 不正な引数を指定した場合、関数は失敗し、文字列は変化しない。
void K_strrep(char *s, char oldchar, char newchar);
void K_strrep(std::string &s, int start, int count, const char *newstr);
void K_strrep(std::string &s, const char *oldstr, const char *newstr);
void K_wcsrep(wchar_t *s, wchar_t oldchar, wchar_t newchar);
void K_wcsrep(std::wstring &s, int start, int count, const wchar_t *newstr);
void K_wcsrep(std::wstring &s, const wchar_t *oldstr, const wchar_t *newstr);

/// インデックス start 以降の部分から、文字列 sub に一致する部分を探す。
/// みつかれば、その先頭インデックスを返す。そうでなければ -1 を返す。
/// sub に空文字列を指定した場合は常に検索開始位置 (start) を返す
int K_strpos(const char *s, const char *sub, int start=0);
int K_strpos(const std::string &s, const std::string &sub, int start=0);
int K_wcspos(const wchar_t *s, const wchar_t *sub, int start=0);
int K_wcspos(const std::wstring &s, const std::wstring &sub, int start=0);

/// 指定された文字で始まっている / 終わっているかどうか
bool K_strstarts(const char *s, const char *sub);
bool K_strends(const char *s, const char *sub);


/// s の各文字と key の排他的論理和を取り、簡易暗号化を行う。
/// （復号にも同じ関数を用いる）
/// 文字列 key が s より短い場合は、key を繰り返して参照する
void K_strxor(char *s, size_t len, const char *key);

/// std::to_string と同じだが、std::to_string は android/NDK では定義されていない
std::string K_tostring(int x);
std::string K_tostring(unsigned int x);
std::string K_tostring(float x);
std::string K_tostring(double x);

/// 自動拡張版の sprintf
std::string K_sprintf(const char *fmt, ...);
std::string K_vsprintf(const char *fmt, va_list args);

/// 文字列 s から区切り文字 separator を探し、最初に見つかった場所を境にして左文字列と右文字列に分ける。
/// 分割した場合は left, right に文字列をセットして true を返す
/// 区切り文字が見つからなかった場合は left に s のコピーをセットして false を返す
/// left, right は NULL でも良い
/// @param s[in] 入力文字列
/// @param left[out] 分割後の左文字列。NULLでも良い
/// @param separator[in] 区切り文字
/// @param right[out] 分割後の右文字列。NULLでも良い
/// @param trim[in] 分割後の文字列の前後についた空白文字を取り除くかどうか
bool K_strsplit_char(const char *s, std::string *left, char separator, std::string *right, bool trim=true);

/// K_strsplit_char と同じだが、「区切り文字」ではなく「区切り文字<b>列</b>」を使う
bool K_strsplit_str(const char *s, std::string *left, const char *separator_word, std::string *right, bool trim=true);

/// K_strsplit_char と同じだが、最後に見つかった separator の場所を境にして分割する
bool K_strsplit_rchar(const char *s, std::string *left, char separator, std::string *right, bool trim=true);

/// K_strsplit_rchar と同じだが、「区切り文字」ではなく「区切り文字<b>列</b>」を使う
bool K_strsplit_rstr(const char *s, std::string *left, const char *separator_word, std::string *right, bool trim=true);


struct string_range_t {
	const char *s;
	const char *e;

	string_range_t() {
		s = e = NULL;
	}
	string_range_t(const char *str) {
		s = str;
		e = str + strlen(s);
	}
	string_range_t(const char *start, const char *end) {
		s = start;
		e = end;
	}
	size_t size() const {
		return e - s;
	}
	void get(char *out, size_t maxsize) const {
		strcpy_s(out, maxsize, s);
	}
	std::string get() const {
		return std::string(s, e-s);
	}
};


void K_trim(string_range_t &str);

