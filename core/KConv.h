#pragma once
#include <string>

class KConv {
public:
	/// u8の先頭が utf8 bom で始まっているなら、その次の文字アドレスを返す。
	/// utf8 bom で始まっていない場合はそのまま u8 を返す
	static const char * skipBOM(const char *u8);

	/// UTF8文字列をワイド文字列に変換する
	static std::wstring utf8ToWide(const char *u8);
	static std::wstring utf8ToWide(const std::string &u8);

	/// UTF8文字列をANSI文字列(ロケール依存マルチバイト文字列）に変換する。
	/// ロケールに空文字列 "" を指定した場合は、現在のシステムのロケールを使う。
	/// それ以外のロケール引数については標準関数の setlocale の解説を参照すること
	/// 日本語向けなら "JPN" や "Japanese_Japan.932" などと指定する
	static std::string utf8ToAnsi(const char *u8, const char *_locale="");
	static std::string utf8ToAnsi(const std::string &u8, const char *_locale="");

	/// ワイド文字列をUTF8文字列に変換する
	static std::string wideToUtf8(const wchar_t *ws);
	static std::string wideToUtf8(const std::wstring &ws);

	/// ワイド文字列をANSI文字列(ロケール依存マルチバイト文字列）に変換する
	/// ロケールに空文字列 "" を指定した場合は、現在のシステムのロケールを使う。
	/// それ以外のロケール引数については標準関数の setlocale の解説を参照すること
	static std::string wideToAnsi(const wchar_t *ws, const char *_locale="");
	static std::string wideToAnsi(const std::wstring &ws, const char *_locale="");

	/// ANSI文字列(ロケール依存マルチバイト文字列）をワイド文字列に変換する
	/// ロケールに空文字列 "" を指定した場合は、現在のシステムのロケールを使う。
	/// それ以外のロケール引数については標準関数の setlocale の解説を参照すること
	static std::wstring ansiToWide(const char *ansi, const char *_locale="");
	static std::wstring ansiToWide(const std::string &ansi, const char *_locale="");

	/// ANSI文字列(ロケール依存マルチバイト文字列）をUTF8文字列に変換する。
	/// ロケールに空文字列 "" を指定した場合は、現在のシステムのロケールを使う。
	/// それ以外のロケール引数については標準関数の setlocale の解説を参照すること
	static std::string ansiToUtf8(const char *ansi, const char *_locale="");
	static std::string ansiToUtf8(const std::string &ansi, const char *_locale="");


	/// エンコード方式の不明なバイト列 data を文字列に変換する。
	/// 現ロケールでのマルチバイト文字列 --> SJIS --> UTF8 の順で変換を試みる。
	/// 成功すれば変換結果を out にセットして true を返す。
	/// どの方法でも変換できなかった場合は false を返す
	static bool toWideTry(std::wstring &out, const void *data, int size);

	/// toWideTry と同じだが、変換結果を文字列として返す。
	/// どの方法でも変換できなかった場合、入力データをそのまま std::wstring に拡張したものを返す
	/// (data が char 配列であるとみなして、それぞれの要素を wchar_t に置き換えたもの）
	static std::wstring toWide(const void *data, int size);
	static std::wstring toWide(const std::string &data);
	static std::string  toUtf8(const void *data, int size);
	static std::string  toUtf8(const std::string &data);

};

