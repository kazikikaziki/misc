#pragma once
#include <string>
#include <vector>

namespace mo {

class Path;
typedef std::vector<Path> PathList;

class Path {
public:
#ifdef _WIN32
	static const char NATIVE_DELIM = '\\';
#else
	static const char NATIVE_DELIM = '/';
#endif
	static const int SIZE = 260;
	static const Path Empty; // 空のパス。Path("") や Path() と等しい
	static Path fromAnsi(const char *mb, const char *locale);
	static Path fromUtf8(const char *u8);
	static Path fromFormat(const char *fmt, ...);
	
	Path();
	Path(const char *u8);
	Path(const std::string &s);
	Path(const wchar_t *s);
	Path(const std::wstring &s);
	Path(const Path &s);

	void clear();
	bool empty() const;

	bool operator ==(const Path &p) const;
	bool operator !=(const Path &p) const;
	bool operator < (const Path &p) const;

	/// 内部文字列のハッシュを返す
	size_t hash() const;

	/// ファイルパスのディレクトリ名部分を得る
	Path directory() const;

	/// ファイルパスのファイル名部分を得る（ディレクトリを含まない）
	Path filename() const;

	/// ファイルパスの拡張子部分を得る
	/// デフォルト状態ではピリオドを含んだ拡張子を返すが、
	/// strip_period を true にするとピリオドを取り除いた拡張子を返す
	Path extension(bool strip_period=false) const;

	/// 区切り文字を挟んで追加する
	Path join(const Path &more) const;
	Path join(const PathList &more) const;
	
	/// 単なる文字列として s を連結する
	Path joinString(const Path &s) const;
	Path joinString(const char *s) const;

	/// ファイルパスの拡張子部分を別の拡張子で置換したものを返す。
	/// ext には新しい拡張子をピリオドを含めて指定する
	Path changeExtension(const Path &ext) const;
	Path changeExtension(const char *ext) const;

	bool hasExtension(const Path &ext) const;
	bool hasExtension(const char *ext) const;

	/// 自分が相対パスであれば、カレントディレクトリを元にして得られた絶対パスを返す
	Path getFullPath() const;

	/// 共通パス部分を返す
	Path getCommonPath(const Path &orther) const;
	int getCommonPath(const Path &other, PathList *list) const;

	/// basedir からの相対パスを返す
	/// @code
	/// Path("q/aaa/bbb.c").getRelativePathFrom("q/aaa"    ) ===> "bbb.c"
	/// Path("q/aaa/bbb.c").getRelativePathFrom("q/aaa/ccc") ===> "../bbb.c"
	/// Path("q/aaa/bbb.c").getRelativePathFrom("q/bbb"    ) ===> "../aaa/bbb.c"
	/// Path("q/aaa/bbb.c").getRelativePathFrom("q/bbb/ccc") ===> "../../aaa/bbb.c"
	/// @endcode
	Path getRelativePathFrom(const Path &basedir) const;

	/// 要素ごとに分解したリストを返す
	/// 要素ではなく、文字単位で分割したい場合は普通の文字列関数 KToken を使う
	size_t split(PathList *list) const;

	/// ファイルパスが相対パス形式かどうか
	bool isRelative() const;

	/// 現在のロケールにしたがってマルチバイト文字列に変換したものを返す
	std::string toAnsiString(char sep, const char *_locale) const;

	/// utf8 文字列を返す。BOM (バイトオーダーマーク) は付かない
	std::string toUtf8(char sep='/') const;

	/// ワイド文字列に変換したものを返す
	std::wstring toWideString(char sep='/') const;

	/// パスが sub パスで始まっているかどうかを、パス単位で調べる
	bool startsWithPath(const Path &sub) const;

	/// パスが sub パスで終わっているかどうかを、パス単位で調べる
	bool endsWithPath(const Path &sub) const;

	/// パス文字列が s で始まっているかどうかを調べる
	bool startsWithString(const char *sub) const;

	/// パス文字列が s で終わっているかどうかを調べる
	bool endsWithString(const char *sub) const;

	/// パス同士を比較する
	int compare(const Path &path) const;
	int compare(const Path &path, bool ignore_case, bool ignore_path) const;

	/// 内部文字列の表現と直接比較する。
	/// str がパス区切りを含む場合は注意。パス区切りは内部で自動変換されている可能性がある
	int compare_str(const char *str) const;
	int compare_str(const char *str, bool ignore_case, bool ignore_path) const;

	/// 簡易版の glob テストを行い、ファイル名がパターンと一致しているかどうかを調べる。
	/// ワイルドカードは * のみ利用可能。
	/// ワイルドカード * はパス区切り記号 / とは一致しない。
	/// @code
	/// Path("abc").glob("*") ===> true
	/// Path("abc").glob("a*") ===> true
	/// Path("abc").glob("ab*") ===> true
	/// Path("abc").glob("abc*") ===> true
	/// Path("abc").glob("a*c") ===> true
	/// Path("abc").glob("*abc") ===> true
	/// Path("abc").glob("*bc") ===> true
	/// Path("abc").glob("*c") ===> true
	/// Path("abc").glob("*a*b*c*") ===> true
	/// Path("abc").glob("*bc*") ===> true
	/// Path("abc").glob("*c*") ===> true
	/// Path("aaa/bbb.ext").glob("a*.ext") ===> false
	/// Path("aaa/bbb.ext").glob("a*/*.ext") ===> true
	/// Path("aaa/bbb.ext").glob("a*/*.*t") ===> true
	/// Path("aaa/bbb.ext").glob("aaa/*.ext") ===> true
	/// Path("aaa/bbb.ext").glob("aaa/bbb*ext") ===> true
	/// Path("aaa/bbb.ext").glob("aaa*bbb*ext") ===> false
	/// Path("aaa/bbb/ccc.ext").glob("aaa/*/ccc.ext") ===> true
	/// Path("aaa/bbb/ccc.ext").glob("aaa/*.ext") ===> false
	/// Path("aaa/bbb.ext").glob("*.aaa") ===> false
	/// Path("aaa/bbb.ext").glob("aaa*bbb") ===> false
	/// @endcode
	bool glob(const Path &pattern) const;

	const char * u8() const;

private:
	char path_[SIZE];
	mutable uint32_t hash_;
};


void Test_pathstring();


} // mo

namespace std {
	// mo::Path を std コンテナのキーとして使えるようにする
	template <> struct hash<mo::Path> {
		std::size_t operator()(mo::Path const &key) const {
			return key.hash();
		}
	};
}



