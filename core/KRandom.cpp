#define _USE_MATH_DEFINES // M_PI
#include <math.h>
#include "KStd.h"
#include "KRandom.h"


namespace mo {

#pragma region Random
Random::Random() {
	x_ = y_ = z_ = w_ = 0;
	setSeed((uint32_t)(-1));
}
void Random::setSeed(uint32_t seed) {
	x_ = 123456789;
	y_ = 362436069;
	z_ = 521288629;
	w_ = (seed != 0) ? seed : 88675123;
	for (int i=0; i<256; i++) {
		getNext();
	}
}
uint32_t Random::getMax() {
	return 0xFFFFFFFF;
}
uint32_t Random::getNext() {
	uint32_t t = (x_^(x_<<11));
	x_=y_; y_=z_; z_=w_;
	w_ = (w_^(w_>>19)) ^ (t^(t>>8));
	return w_;
}
int Random::getNextInt(int range) {
	if (range <= 0) return 0;
	uint32_t _range = (uint32_t)range;
	uint32_t upper = getMax();
	uint32_t mod = upper % _range;
	uint32_t lim = upper - mod;
	// 元の乱数範囲が range の倍数でない場合、そのまま計算すると確率が均等にならない。
	// lim を超えた場合は再生成する必要がある
	uint32_t n;
	do {
		n = getNext();
	} while (n >= lim);
	int val = (int)n % _range;
	K_assert(0 <= val && val < range);
	return val;
}
int Random::getNextInt(int lower, int upper, bool inclusive) {
	if (lower >= upper) return lower;
	int adj = inclusive ? 1 : 0;
	int val = lower + getNextInt(upper - lower + adj);
	return val;
}
float Random::getNextFloat(bool inclusive) {
	uint32_t a = getNext();
	uint32_t b = getMax();
	if (! inclusive) {
		// 1.0を除外する
		while (a == b) {
			a = getNext();
		}
	}
	return (float)a / b;
}
float Random::getNextFloat(float upper, bool inclusive) {
	if (upper <= 0) return 0;
	return getNextFloat(inclusive) * upper;
}
float Random::getNextFloat(float lower, float upper, bool inclusive) {
	if (lower >= upper) return lower;
	return lower + getNextFloat(upper - lower, inclusive);
}
float Random::getNextFloatPI(float n) {
	if (n < 0) return 0;
	return getNextFloat((float)M_PI * n);
}
float Random::getNextFloatGaussian(float lower, float upper, bool inclusive) {
	if (lower >= upper) return lower;
	// 乱数の平均を計算して中央値の確率を上げ、
	// 疑似的に正規分布になるようにする
	const int N = 5;
	float gauss = 0;
	for (int i=0; i<N; i++) {
		gauss += getNextFloat(inclusive);
	}
	gauss /= N;
	return lower + (upper - lower) * gauss;
}
#pragma endregion // Random





static Random s_random;

int Rand_geti(int n) {
	return s_random.getNextInt(n);
}
int Rand_rangei(int _min, int _max) {
	return s_random.getNextInt(_min, _max, true);
}
float Rand_getf() {
	return s_random.getNextFloat(1.0f, true);
}
float Rand_getf(float n) {
	return s_random.getNextFloat(n, true);
}
float Rand_rangef(float _min, float _max) {
	return s_random.getNextFloat(_min, _max, true);
}
float Rand_rangef(int _min, int _max) {
	return s_random.getNextFloat((float)_min, (float)_max, true);
}

} // namespace
