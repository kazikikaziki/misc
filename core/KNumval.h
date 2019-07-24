#pragma once
namespace mo {

// 「値」として指定できる文字列のパーサ
struct KNumval {
//	char pre[16]; ///< プレフィックス
	char num[16]; ///< 数値部 入力が "50%" だった場合、"50" になる
	char suf[16]; ///< 単位部 入力が "50%" だった場合、"%d" になる
	float numf;   ///< 数値を実数として解釈したときの値。入力が "50%" だった場合、50 になる

	KNumval();
	KNumval(const char *expr);
	void clear();
	void assign(const KNumval &source);
	bool parse(const char *expr);

	/// サフィックスがあるかどうか
	bool has_suffix(const char *suf) const;

	/// 整数値を得る
	/// percent_base: サフィックスが % だった場合、その割合に乗算する値
	/// 入力が "50" だった場合は percent_base を無視し、そのまま 50 を返す。
	/// 入力が "50%" だった場合、percent_base に 256 を指定すると 128 を返す
	int valuei(int percent_base) const;

	/// 実数を得る
	/// percent_base: サフィックスが % だった場合、その割合に乗算する値
	/// 入力が "50" だった場合は percent_base を無視し、そのまま 50.0f を返す。
	/// 入力が "50%" だった場合、percent_base に 256.0f を指定すると 128.0f を返す
	float valuef(float percent_base) const;
};

void Test_numval();

} // namespace
