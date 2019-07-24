#include "KConv.h"
#include <locale.h>
#ifdef _WIN32
#include <Windows.h> // MultiByteToWideChar, WideCharToMultiByte
#endif

#define FULL_HEADER 1

#if FULL_HEADER
#include "KLog.h"
#define CONVASSERT(expr)        Log_assert(expr)
#define CONVERRORF(fmt, ...)    Log_errorf(fmt, ##__VA_ARGS__)
#define CONVWARNINGF(fmt, ...)  Log_warningf(fmt, ##__VA_ARGS__)
#else
#include <assert.h>
#define CONVASSERT(expr)        assert(expr)
#define CONVERRORF(fmt, ...)    printf(fmt, ##__VA_ARGS__), putchar('\n')
#define CONVWARNINGF(fmt, ...)  printf(fmt, ##__VA_ARGS__), putchar('\n')
#endif

namespace {

static const size_t UTF8_BOMSIZE = 3;
static const char * UTF8_BOM = "\xEF\xBB\xBF";

static void _Error(const char *msg, const void *data, int size) {
#if FULL_HEADER
	KLog::print("STRING ENCODING FAILED:");
	KLog::printBinary(data, size);
	KLog::print("<--");
	CONVERRORF("%s", msg);
#else
	CONVERRORF("STRING ENCODING FAILED:");
#endif
}

#ifdef _WIN32
static bool _Win32WideCharToMultiByte(std::string &out_mb, UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar, LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar) {
	// codepage が UTF7 または UTF8 の場合、lpDefaultChar と lpUsedDefaultChar は NULL でないとエラーになる
	// 注）Win7, Win8 ではエラーになり 0 が返ってきたが、Win10 だと普通に実行できる
	if (CodePage == CP_UTF7 || CodePage == CP_UTF8) { CONVASSERT(lpDefaultChar==NULL && lpUsedDefaultChar==NULL); }
	int estimated_len = WideCharToMultiByte(CodePage, dwFlags, lpWideCharStr, cchWideChar, NULL, 0, lpDefaultChar, lpUsedDefaultChar); // 変換後の文字数（終端文字含む）
	if (estimated_len > 0) {
		out_mb.resize(estimated_len+16); // 念のため、少し大きめに確保しておく http://blog.livedoor.jp/blackwingcat/archives/976097.html
		WideCharToMultiByte(CodePage, dwFlags, lpWideCharStr, cchWideChar, &out_mb[0], out_mb.size(), lpDefaultChar, lpUsedDefaultChar);
		size_t real_len = strlen(out_mb.c_str());
		out_mb.resize(real_len); // string::size と strlen を一致させる
		return true;
	} else {
		int err = GetLastError();
		if (err) {
			CONVERRORF("WideCharToMultiByte FAILED: ERR=%d (0x%X)", err, err);
		}
	}
	return false;
}

static bool _Win32MultiByteToWideChar(std::wstring &out_ws,UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte) {
	// codepage が UTF8 の場合、flags は 0 または MB_ERR_INVALID_CHARS でないとエラーになる
	// 注）Win7, Win8 ではエラーになり 0 が返ってきたが、Win10 だと普通に実行できる
	// https://docs.microsoft.com/en-us/windows/desktop/api/stringapiset/nf-stringapiset-multibytetowidechar
	// によれば、UTF8の場合は書いてあるがUTF7については言及されていない
	if (CodePage == CP_UTF8) { CONVASSERT(dwFlags==0 || dwFlags==MB_ERR_INVALID_CHARS); }
	int estimated_len = MultiByteToWideChar(CodePage, dwFlags, lpMultiByteStr, cbMultiByte, NULL, NULL); // 変換後の文字数（終端文字含む）
	if (estimated_len > 0) {
		out_ws.resize(estimated_len+8);
		MultiByteToWideChar(CodePage, dwFlags, lpMultiByteStr, cbMultiByte, &out_ws[0], out_ws.size());
		size_t real_len = wcslen(out_ws.c_str()); // MultiByteToWideChar の戻り値が信用できない（末尾のヌル文字を含むときと含まない時がある）ので、実際に文字列長さを調べる
		out_ws.resize(real_len); // wstring::size と wcslen を一致させる
		return true;
	} else {
		int err = GetLastError();
		if (err) {
			CONVERRORF("WideCharToMultiByte FAILED: ERR=%d (0x%X)", err, err);
		}
	}
	return false;
}
#endif // _WIN32


static bool _IsAsciiString(const char *a) {
	for (int i=0; a[i]; i++) {
		if (!isascii(a[i])) {
			return false;
		}
	}
	return true;
}
static bool _WideToUtf8(std::string &out, const wchar_t *ws) {
#ifdef _WIN32
	CONVASSERT(ws && ws[0]);
	return _Win32WideCharToMultiByte(out, CP_UTF8, WC_ERR_INVALID_CHARS, ws, -1, NULL, NULL);
#else
	CONVASSERT(0 && "NOT IMPLEMENT");
	return false;
#endif
}
static bool _Utf8ToWide(std::wstring &out, const char *u8, int size) {
#ifdef _WIN32
	CONVASSERT(u8 && u8[0]);
	return _Win32MultiByteToWideChar(out, CP_UTF8, MB_ERR_INVALID_CHARS, u8, size);
#else
	CONVASSERT(0 && "NOT IMPLEMENT");
	return false;
#endif
}
static bool _WideToAnsi(std::string &out, const wchar_t *ws, const char *_locale) {
	CONVASSERT(ws);
	CONVASSERT(_locale);
	bool ret = false;
#ifdef _WIN32
	{
		_locale_t loc = _create_locale(LC_CTYPE, _locale);
		if (loc == NULL) {
			CONVWARNINGF("W_STRING_CONV: Invalid locale: '%s' for _WideToAnsi", _locale);
			CONVASSERT(0);
		}
		int len = (int)_wcstombs_l(NULL, ws, 0, loc); // 終端文字含まず
		if (len > 0) {
			out.resize(len+1);
			_wcstombs_l(&out[0], ws, out.size(), loc);
			CONVASSERT(out.back() == L'\0');
			out.pop_back(); // 末尾の余計な終端文字を取る。こうしないと string::size と strlen の結果が一致しない
			ret = true;
		}
		_free_locale(loc);
	}
#else
	{
		out.resize(wcslen(ws)*2 + 1); // 1ワイド文字につき2バイト確保しておく
		int len = wcstombs(&out[0], ws, out.size()); // len は終端文字を含まない
		if (len >= 0) {
			out.resize(len);
			ret = true;
		} else {
			out.clear();
		}
	}
#endif
	return ret;
}
static bool _AnsiToWide(std::wstring &out, const char *mb, int bytes, const char *_locale) {
	CONVASSERT(mb);
	CONVASSERT(_locale);
	bool ret = false;
#ifdef _WIN32
	{
		_locale_t loc = _create_locale(LC_CTYPE, _locale);
		if (loc == NULL) {
			CONVWARNINGF("W_STRING_CONV: Invalid locale: '%s' for _AnsiToWide", _locale);
			CONVASSERT(0);
		}
		int len = (int)_mbstowcs_l(NULL, mb, 0, loc); // 終端文字含まず
		if (len > 0) {
			out.resize(len+1);
			_mbstowcs_l(&out[0], mb, out.size(), loc);
			CONVASSERT(out.back() == L'\0');
			out.pop_back(); // 末尾の余計な終端文字を取る。こうしないと string::size と strlen の結果が一致しない
			ret = true;
		}
		_free_locale(loc);
	}
#else
	{
		out.resize(strlen(mb) + 1);
		int len = mbstowcs(&out[0], mb, out.size()); // len は終端文字を含まない
		if (len >= 0) {
			out.resize(len);
			ret = true;
		} else {
			out.clear();
		}
	}
#endif
	return ret;
}

}




const char * KConv::skipBOM(const char *s) {
	if (s) {
		if (strncmp(s, UTF8_BOM, 3) == 0) {
			return s + 3;
		}
	}
	return s;
}
std::wstring KConv::utf8ToWide(const char *_u8) {
	std::wstring ws;
	if (_u8 && _u8[0]) {
		const char *u8 = skipBOM(_u8); // BOMをスキップ
		if (!_Utf8ToWide(ws, u8, -1)) {
			_Error("E_CONV: utf8ToWide: Failed", _u8, strlen(_u8));
		}
	}
	return ws;
}
std::wstring KConv::utf8ToWide(const std::string &u8) {
	return utf8ToWide(u8.c_str());
}
std::string KConv::utf8ToAnsi(const char *u8, const char *_locale) {
	// UTF8 から ANSI は直接変換できない。WIDE を経由する
	std::wstring ws = utf8ToWide(u8);
	return wideToAnsi(ws, _locale);
}
std::string KConv::utf8ToAnsi(const std::string &u8, const char *_locale) {
	// UTF8 から ANSI は直接変換できない。WIDE を経由する
	std::wstring ws = utf8ToWide(u8);
	return wideToAnsi(ws, _locale);
}
std::string KConv::wideToUtf8(const wchar_t *wide) {
	std::string u8;
	if (wide && wide[0]) {
		if (!_WideToUtf8(u8, wide)) {
			_Error("E_CONV: utf8ToWide: Failed", wide, wcslen(wide));
		}
	}
	return u8;
}
std::string KConv::wideToUtf8(const std::wstring &ws) {
	return wideToUtf8(ws.c_str());
}
std::string KConv::wideToAnsi(const wchar_t *wide, const char *_locale) {
	std::string mb;
	if (wide && wide[0]) {
		if (!_WideToAnsi(mb, wide, _locale)) {
			_Error("E_CONV: wideToAnsi: Failed", wide, wcslen(wide));
		}
	}
	return mb;
}
std::string KConv::wideToAnsi(const std::wstring &ws, const char *_locale) {
	return wideToAnsi(ws.c_str(), _locale);
}
std::wstring KConv::ansiToWide(const char *mb, const char *_locale) {
	std::wstring ws;
	if (mb && mb[0]) {
		if (!_AnsiToWide(ws, mb, -1, _locale)) {
			_Error("E_CONV: ansiToWide: Failed", mb, strlen(mb));
		}
	}
	return ws;
}
std::wstring KConv::ansiToWide(const std::string &ansi, const char *_locale) {
	return ansiToWide(ansi.c_str(), _locale);
}
std::string KConv::ansiToUtf8(const char *ansi, const char *_locale) {
	if (_IsAsciiString(ansi)) {
		// すべてが ASCII 文字で構成されているなら無変換で良い
		// （現在のロケールにおける 0x00 - 0x7F が ASCII と完全に重複していることを前提とする）
		return ansi;
	} else {
		std::wstring ws = ansiToWide(ansi, _locale);
		return wideToUtf8(ws);
	}
}
std::string KConv::ansiToUtf8(const std::string &ansi, const char *_locale) {
	if (_IsAsciiString(ansi.c_str())) {
		// すべてが ASCII 文字で構成されているなら無変換で良い
		// （現在のロケールにおける 0x00 - 0x7F が ASCII と完全に重複していることを前提とする）
		return ansi;
	} else {
		std::wstring ws = ansiToWide(ansi, _locale);
		return wideToUtf8(ws);
	}
}
bool KConv::toWideTry(std::wstring &out, const void *data, int size) {
	const char *s = (const char *)data;

	if (s == NULL || size == 0) {
		out = std::wstring();
		return true;
	}
	// BOM で始まるデータなら UTF8 で確定させる
	if (size >= UTF8_BOMSIZE && memcmp(data, UTF8_BOM, UTF8_BOMSIZE) == 0) {
		const char *nobom_data = s + UTF8_BOMSIZE;
		const int nobom_size = size - UTF8_BOMSIZE;
		if (_Utf8ToWide(out, nobom_data, nobom_size)) {
			return true;
		}
	}

	// 現在のロケールにおけるマルチバイト文字列として変換
	if (_AnsiToWide(out, s, size, "")) {
		return true;
	}
	// SJIS であると仮定して変換
	// （非日本語環境でゲームを実行しているとき、SJIS保存されたファイルをロードしようとしている場合など）
	// SJISをUTF8として解釈できる事が多いが、UTF8をSJISとして解釈できることは少ないため、
	// 先にSJISへの変換を試みる。入力がUTF8の場合は大体失敗してくれるはず。
	if (_AnsiToWide(out, s, size, "JPN")) {
		return true;
	}
	// UTF8 であると仮定して変換
	if (_Utf8ToWide(out, s, size)) {
		return true;
	}
	// どの方法でも変換できなかった
	return false;
}
std::wstring KConv::toWide(const void *data, int size) {
	// ワイド文字への変換を試みる
	std::wstring out;
	if (toWideTry(out, data, size)) {
		return out;
	}
	_Error("E_CONV: toWide: Failed", data, size);
	// 入力バイナリを wchar_t 配列に拡張したものを返す
	if (data && size > 0) {
		out.resize(size, L'\0');
		for (int i=0; i<size; i++) {
			out[i] = ((const char *)data)[i]; // char --> wchar_t
		}
	}
	return out;
}
std::wstring KConv::toWide(const std::string &data) {
	return toWide(data.data(), data.size());
}
std::string KConv::toUtf8(const void *data, int size) {
	// ワイド文字への変換を試みる。成功したらさらにUTF8に変換したものを返す
	std::wstring ws;
	if (toWideTry(ws, data, size)) {
		return wideToUtf8(ws);
	}
	// 入力バイナリをそのまま返す
	return std::string((const char*)data, size);
}
std::string KConv::toUtf8(const std::string &data) {
	return toUtf8(data.data(), data.size());
}
