#pragma once
#include <vector>
#include <string>

namespace mo {

class KToken {
public:
	KToken();

	/// @see tokenize()
	KToken(const char *text, const char *delims="\n\r\t ", bool condense_delims=true, size_t maxtokens=0);

	/// 文字列 text を文字集合 delim で区切り、トークン配列を得る
	///
	/// 文字列 text から区切り文字集合 delim のいずれかの文字を探し、見つかるたびにトークンとして分割していく。
	/// 得られたトークンは [] 演算子または get() で取得できる
	/// @param text 入力文字列
	/// @param delim 区切り文字の集合。
	/// @param condense_delims 連続する区切り文字を結合するか。
	///        例えば区切り文字が "/" の時に入力文字列 "AA/BB//CC/" を処理した場合、
	///        区切り文字を結合するなら分割結果は {"AA", "BB", "CC"} になる。
	///        区切り文字を結合しない場合、分割結果は {"AA", "BB", "", "CC", ""} になる
	/// @param maxtokens 最大分割数。0 だと無制限。例えば 2 を指定した場合、"AA/BB/CC" の分割結果は {"AA", "BB/CC"} になる
	/// @return 得られたトークンの数。 size() と同じ
	size_t tokenize(const char *text, const char *delims="\n\r\t ", bool condense_delims=true, size_t maxtokens=0);

	/// 文字列 text を行単位で分割する
	/// @param with_empty_line 空白行を含めるかどうか。
	///                        true にすると空白行を無視する。
	///                        false にすると全ての行を含める。
	///                        行番号と対応させたい場合など、行を削除すると困る場合は false にする
	size_t splitLines(const char *text, bool with_empty_line=false);

	/// 文字列 text を文字集合 delim で区切り、その前後の文字列を得る。
	/// 文字列 text が delim を含んでいれば 2 個の文字列に分割されて size() は 2 になる。
	/// 文字列 text が delim を含まない場合は text 全体が一つのトークン扱いになって size() は 1 になる
	/// @return 得られたトークンの数。 size() と同じ
	size_t split(const char *text, const char *delim);

	/// トークンを取得する。index が範囲外の場合は std::vector の例外が発生する
	const char * operator[](size_t index) const;

	/// トークンを取得する。index が範囲外の場合は def を返す
	const char * get(size_t index, const char *def="") const;

	/// トークンと文字列 str を比較する
	int compare(size_t index, const char *str) const;

	/// トークン数を返す
	size_t size() const;

private:
	struct _Range {
		const char *s;
		const char *e;
	};

	// 前後の空白を削除
	static void trim(_Range &str);

	// 素直にトークン分けする
	static void tokenize_raw(const char *text, const char *delims, bool condense_delims, int maxdiv, std::vector<_Range> &ranges);

	std::vector<_Range> tok_;
	std::string s_;
};

} // namespace
