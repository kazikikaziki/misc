#pragma once
#include <inttypes.h>

namespace mo {

/// 乱数発生クラス
/// http://ja.wikipedia.org/wiki/Xorshift
class Random {
public:
	Random();
	/// 乱数を初期化する
	///
	/// 与えられたシード値を元にして乱数系列を初期化する。
	/// シードに 0 を指定すると、デフォルトの初期化値を使う
	void setSeed(uint32_t seed);
	
	/// 一様な32ビット整数乱数を返す
	/// 0 以上 getMax() 以下（未満ではない）の符号なし整数を返す
	uint32_t getNext();
	
	/// getNext() が返す最大値を返す
	uint32_t getMax();
	
	///　0 以上 upper 未満の整数乱数を返す。
	/// ただし range <= 0 だった場合は lower を返す
	int getNextInt(int upper);

	/// lower 以上 upper 未満(以下)の整数乱数を返す。
	/// ただし upper <= upper だった場合は lower を返す
	int getNextInt(int lower, int upper, bool inclusive=false);

	/// 0.0 以上 1.0 未満(以下)の実数乱数を返す。
	float getNextFloat(bool inclusive=false);

	/// 0.0 以上 upper 未満(以下)の乱数を返す。
	/// ただし upper <= 0 だった場合は lower を返す
	float getNextFloat(float upper, bool inclusive=false);

	/// lower 以上 upper 未満(以下)の乱数を返す。
	/// ただし upper <= lower だった場合は lower を返す
	float getNextFloat(float lower, float upper, bool inclusive=false);

	/// 疑似正規分布に従い、lower 以上 upper 未満の乱数を返す。
	/// ただし upper <= lower だった場合は lower を返す
	float getNextFloatGaussian(float lower, float upper, bool inclusive=false);

	/// 0 以上 nPI 未満の実数乱数を返す。
	/// n <= 0 だった場合は 0 を返す
	float getNextFloatPI(float n=1.0f);

	bool getNextBool() { return getNextInt(2) == 0; }

private:
	uint32_t x_, y_, z_, w_;
};


/// Returns a random number between 0 and n [exclusive]
int Rand_geti(int n);

/// Returns a random number between _min [inclusive] and _max [inclusive]
int Rand_rangei(int _min, int _max);

/// Returns a random number between 0.0 [inclusive] and 1.0 [inclusive]
float Rand_getf();

/// Returns a random number between 0.0 [inclusive] and n [inclusive]
float Rand_getf(float n);

/// Returns a random number between _min [inclusive] and _max [inclusive]
float Rand_rangef(float _min, float _max);
float Rand_rangef(int _min, int _max);


}