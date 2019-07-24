#pragma once
#include <string>
#include <vector>

namespace mo {

std::string c_sprintf(const char *fmt, ...);
std::string c_sprintfv(const char *fmt, va_list args);

bool c_streq(const char *a, const char *b);
bool c_strempty(const char *a);
bool c_strempty(const wchar_t *a);

/// 安全な strcpy。strcpy_s とはちがい、NULLを許容する
void c_strcpy(char *dst, int dst_size, const char *src);

/// 安全な strcat
void c_strcat(char *dst, int dst_size, const char *add);

/// str が文字列 substr で始まっているなら true を返す
bool c_startswith(const char *str, const char *substr, bool ignore_case=false);
bool c_startswith(const std::string &str, const std::string &substr, bool ignore_case=false);

/// str が文字列 substr で終わっているなら true を返す
bool c_endswith(const char *str, const char *substr, bool ignore_case=false);
bool c_endswith(const std::string &str, const std::string &substr, bool ignore_case=false);

/// s に sprintf 文字列を追加する
void c_strcatf(char *s, const char *fmt, ...);
void c_strcatv(char *s, const char *fmt, va_list args);
void c_wstrcatf(wchar_t *s, size_t size, const wchar_t *fmt, ...);
void c_wstrcatv(wchar_t *s, size_t size, const wchar_t *fmt, va_list args);

/// お手軽な strtof
float c_strtof(const char *s, float def=0.0f);
float c_wstrtof(const wchar_t *s, float def=0.0f);
bool c_strtof_try(const char *s, float *val);

/// お手軽な strtoi
int c_strtoi(const char *s, int def=0);
int c_wstrtoi(const wchar_t *s, int def=0);
bool c_strtoi_try(const char *s, int *val);

unsigned int c_strtoui(const char *s, unsigned int def=0);
bool c_strtoui_try(const char *s, unsigned int *val);

// time_t の数値を文字列化する。time_t が 64 ビットの場合でも正しく動作する
void c_timetostr64(char *s, time_t t);

// 64ビット16進数で記録された time_t を復元する
time_t c_strtotime64(const char *s);

/// start, count で指定された範囲の部分文字列を str で置換する。
/// 置換前と置換後の文字数が変化してもよい。
/// start には 0 以上 size() 未満の開始インデックスを指定する。
/// count には文字数を指定する。-1 を指定した場合、残り全ての文字を対象にする。
/// 不正な引数を指定した場合、関数は失敗し、文字列は変化しない。
void c_strreplace(std::string &s, int start, int count, const char *str);
void c_strreplace(std::string &s, const char *before, const char *after);
void c_strreplace(std::wstring &s, int start, int count, const wchar_t *str);
void c_strreplace(std::wstring &s, const wchar_t *before, const wchar_t *after);
void c_strreplace(char *s, char before, char after);
void c_strreplace(wchar_t *s, wchar_t before, wchar_t after);

/// インデックス start 以降の部分から、文字列 sub に一致する部分を探す。
/// みつかれば、その先頭インデックスを返す。そうでなければ -1 を返す。
/// sub に空文字列を指定した場合は常に検索開始位置 (start) を返す
int c_strfind(const char *s, const char *sub, int start=0);
int c_strfind(const std::string &s, const std::string &sub, int start=0);
int c_strfind(const wchar_t *s, const wchar_t *sub, int start=0);
int c_strfind(const std::wstring &s, const std::wstring &sub, int start=0);

/// インデックス start 以降の部分から文字 chr に一致する部分を探す
/// みつかれば、そのインデックスを返す。そうでなければ -1 を返す。
int c_strfindchar(const char *s, char chr, int start=0);
int c_strfindchar(const std::string &s, char chr, int start=0);
int c_strfindchar(const wchar_t *s, wchar_t chr, int start=0);
int c_strfindchar(const std::wstring &s, wchar_t chr, int start=0);

/// 前後の空白を取り除く
void c_strtrim(std::string &s);

void Test_cstr();

}