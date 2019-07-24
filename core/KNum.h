#pragma once

#define _USE_MATH_DEFINES // M_PI
#include <math.h>
#include <stdlib.h> // abs


/// @var Num_EPS
/// 実数比較の時の最大許容誤差
///
/// #MO_STRICT_EPSILON が 1 の場合は標準ヘッダ float.h の FLT_EPSILON をそのまま使う。
/// この場合 Vec3::is_normalized() などの判定がかなりシビアになるので注意すること。
///
/// #MO_STRICT_EPSILON が 0 の場合は適当な精度の機械イプシロンを使う。
/// FLT_EPSILON の精度が高すぎて比較に失敗する場合は #MO_STRICT_EPSILON を 0 にしておく。
/// @see MO_STRICT_EPSILON
#if MO_STRICT_EPSILON
const float Num_EPS = FLT_EPSILON;
#else
const float Num_EPS = 1.0f / 8192; // 0.00012207031
#endif

#define Num_PI ((float)M_PI)


/// a の符号を -1, 0, +1 のどれかで返す
inline int Num_signi(int a) {
	return (a < 0) ? -1 : (a > 0) ? 1 : 0;
}

/// a の符号を -1.0f, 0.0f, +1.0f のどれかで返す
inline float Num_signf(float a) {
	return (a < 0) ? -1.0f : (a > 0) ? 1.0f : 0.0f;
}

/// 整数の範囲を lower と upper の範囲内に収めた値を返す
inline int Num_clampi(int a, int lower, int upper) {
	return (a < lower) ? lower : (a < upper) ? a : upper;
}

/// 実数の範囲を lower と upper の範囲内に収めた値を返す
inline float Num_clampf(float a, float lower, float upper) {
	return (a < lower) ? lower : (a < upper) ? a : upper;
}
inline float Num_clampf(float a) {
	return (a < 0) ? 0 : (a < 1) ? a : 1;
}

/// 四捨五入した値を返す
inline int Num_round(float a) {
	return (a >= 0) ? (int)(a + 0.5f) : -Num_round(fabsf(a));
}

/// a を b で割ったあまりを 0 以上 b 未満の値で求める
/// a >= 0 の場合は a % b と全く同じ結果になる。
/// a < 0 での挙動が a % b とは異なり、a がどんな値でも戻り値は必ず正の値になる
inline int Num_repeati(int a, int b) {
	if (b == 0) return 0;
	if (a >= 0) return a % b;
	int B = (-a / b + 1) * b; // b の倍数のうち、-a を超えるもの (B+a>=0を満たすB)
	return (B + a) % b;
}

/// a と b の剰余を 0 以上 b 未満の範囲にして返す
/// a >= 0 の場合は fmodf(a, b) と全く同じ結果になる。
/// a < 0 での挙動が fmodf(a, b) とは異なり、a がどんな値でも戻り値は必ず正の値になる
inline float Num_repeatf(float a, float b) {
	if (b == 0) return 0;
	if (a >= 0) return fmodf(a, b);
	float B = ceilf(-a / b + 1.0f) * b; // b の倍数のうち、-a を超えるもの (B+a>=0を満たすB)
	return fmodf(B + a, b);
}

/// 値が 0..n-1 の範囲で繰り返されるように定義されている場合に、
/// 値 a を基準としたときの b までの符号付距離を返す。
/// 値 a, b は範囲 [0..n-1] 内に収まるように転写され、絶対値の小さいほうの符号付距離を返す。
/// たとえば n=10, a=4, b=6 としたとき、a から b までの距離は 2 と -8 の二種類ある。この場合 2 を返す
/// a=6, b=4 だった場合、距離は -2 と 8 の二種類で、この場合は -2 を返す。
inline int Num_delta_repeati(int a, int b, int n) {
	int aa = Num_repeati(a, n);
	int bb = Num_repeati(b, n);
	if (aa == bb) return 0;
	int d1 = bb - aa;
	int d2 = bb - aa + n;
	return (abs(d1) < abs(d2)) ? d1 : d2;
}

/// 範囲 [a..b] である値 t を、範囲 [0..1] でスケール変換した値を返す。
/// t が範囲外だった場合、戻り値も範囲外になる
/// @param a 開始値
/// @param b 終了値
/// @param t 補間係数
/// @return  補間結果
inline float Num_normalize_unclamped(float t, float a, float b) {
	return (t - a) / (b - a);
}
inline float Num_normalize_unclamped(int t, int a, int b) {
	return (float)(t - a) / (b - a);
}

/// Num_normalize_unclamped と同じだが、
/// 戻り値が 0 以上 1 以下になるようにクランプする
inline float Num_normalize(float t, float a, float b) {
	return Num_clampf(Num_normalize_unclamped(t, a, b));
}
inline float Num_normalize(int t, int a, int b) {
	return Num_clampf(Num_normalize_unclamped(t, a, b));
}

/// 範囲 [a..b] である値 t を範囲 [a..b] でスケール変換した値を返す。
/// t が範囲外だった場合、戻り値も範囲外になる
/// @param a 開始値
/// @param b 終了値
/// @param t 補間係数
/// @return  補間結果
inline float Num_lerp_unclamped(float a, float b, float t) {
	return a + (b - a) * t;
}

/// Num_lerp_unclamped と同じだが、
/// 戻り値が a 以上 b 以下になるようにクランプする
inline float Num_lerp(float a, float b, float t) {
	return a + (b - a) * Num_clampf(t);
}

/// 値 a と b をパラメータ t によって補完し、0.0 から 1.0 の間に収める。
/// パラメータt が [a..b] を超えていた場合、戻り値は 0.0 または 1.0 にランプされる
inline float Num_smoothstep(float a, float b, float t) {
	float x = Num_normalize(t, a, b);
	return x * x * (3 - 2 * x);
}

/// 角度（度）からラジアンを求める
inline float Num_radians(float deg) {
	return Num_PI * deg / 180.0f;
}

/// 角度（ラジアン）から度を求める
inline float Num_degrees(float rad) {
	return 180.0f * rad / Num_PI;
}

/// 角度（度）を -180 度以上 +180 未満に正規化する
inline float Num_normalize_deg(float deg) {
	return Num_repeatf(deg + 180.0f, 360.0f) - 180.0f;
}

/// 角度（ラジアン）を -PI 以上 +PI 未満に正規化する
inline float Num_normalize_rad(float rad) {
	return Num_repeatf(rad + Num_PI, Num_PI*2) - Num_PI;
}

/// 二つの実数が等しいとみなせるならば true を返す
/// @param a   比較値1
/// @param b   比較値2
/// @param eps 同値判定の精度
inline bool Num_equals(float a, float b, float eps=Num_EPS) {
	return ::fabsf(a - b) <= eps;
}

/// 実数が範囲 lower .. upper と等しいかその内側にあるなら true を返す
inline bool Num_in_range(float a, float lower=0.0f, float upper=1.0f) {
	return lower <= a && a <= upper;
}
inline bool Num_in_range(int a, int lower, int upper) {
	return lower <= a && a <= upper;
}

/// 実数がゼロとみなせるならば true を返す
/// @param a   比較値
/// @param eps 同値判定の精度
inline bool Num_is_zero(float a, float eps=Num_EPS) {
	return Num_equals(a, 0, eps);
}

/// x が 2 の整数乗であれば true を返す
inline bool Num_is_pow2(int x) {
#if 1
	return fmodf(log2f((float)x), 1.0f) == 0.0f;
#else
	if (x >= 1) {
		while (x % 2 == 0) {
			x /= 2;
		}
		return x==1;
	} else {
		return false;
	}
#endif
}

/// x 未満でない最大の 2^n を返す
inline int Num_ceil_pow2(int x) {
	int b = 1;
	while (b < x) b *= 2;
	return b;
}

/// 三角波を生成する
/// cycle で与えられた周期で 0 .. 1 の間を線形に往復する。
/// t=0       ---> 0.0
/// t=cycle/2 ---> 1.0
/// t=cycle   ---> 0.0
inline float Num_triangle_wave(float t, float cycle) {
	float m = fmodf(t, cycle);
	m /= cycle;
	if (m < 0.5f) {
		return m / 0.5f;
	} else {
		return (1.0f - m) / 0.5f;
	}
}
inline float Num_triangle_wave(int t, int cycle) {
	return Num_triangle_wave((float)t, (float)cycle);
}


/// t = [0..1] の範囲で ab 間を往復する値を返す（線形＝三角波）
/// t が [0..1] の範囲を超えた場合でも、0..1間にあるとみなす
/// t = 0.0 で a
/// t = 0.5 で b
/// t = 1.0 で a になる
inline float Num_repeat_lineare(float t, float a, float b) {
	float T = fmodf(t, 1.0f) * 2.0f; // 0.0 <= T <= 2.0
	if (T < 1.0f) {
		return Num_lerp(a, b, T);
	} else {
		return Num_lerp(a, b, 2.0f - T);
	}
}

inline float Num_repeat_sin(float t, float a, float b) {
	// t = 0.0 で T = 0.0 になるよう、sin ではなく -cos を使う
	float T = 0.5f + 0.5f * -cosf(t * 2.0f * Num_PI); // 0.0 <= T <= 1.0
	return a + (b - a) * T;
}





template <typename T> T Num_max(T a, T b) {
	return (a > b) ? a : b;
}
template <typename T> T Num_min(T a, T b) {
	return (a < b) ? a : b;
}

