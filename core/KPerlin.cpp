#include "KPerlin.h"
#include "KLog.h"
#include <math.h>

#define STB_PERLIN_IMPLEMENTATION
#include "stb_perlin.h"

namespace mo {

/// パーリンノイズの値を得る
/// @param lower    戻り値の下限。元のノイズ値の -1.0 に対応する
/// @param upper    戻り値の上限。元のノイズ値の +1.0 に対応する
/// @param x        ノイズ座標 (0.0 <= x <= 1.0)
/// @param y        ノイズ座標 (0.0 <= y <= 1.0)
/// @param z        ノイズ座標 (0.0 <= z <= 1.0)
/// @param x_wrap   x 方向の折り畳み回数(1 以上かつ 2 の整数乗)。大きいほど x 方向のノイズが細かくなる
/// @param y_wrap   y 方向の折り畳み回数(1 以上かつ 2 の整数乗)。大きいほど y 方向のノイズが細かくなる
/// @param z_wrap   z 方向の折り畳み回数(1 以上かつ 2 の整数乗)。大きいほど z 方向のノイズが細かくなる
float perlin3D(float lower, float upper, float x, float y, float z, int x_wrap, int y_wrap, int z_wrap) {
	Log_assert(x_wrap > 0);
	Log_assert(y_wrap > 0);
	Log_assert(z_wrap > 0);
	float nx = x_wrap * fmodf(x, 1.0f);
	float ny = y_wrap * fmodf(y, 1.0f);
	float nz = z_wrap * fmodf(z, 1.0f);
	// stb_perlin_noise3(x, y, z, wx, wy, wz)
	// wx, wy, wz に 2^N な値を指定すると、その値を x, y, z の周期としてパターンが繰り返すようになる。
	// この値が大きいほどノイズが相対的に細かくなる
	float a = ::stb_perlin_noise3(nx, ny, nz, x_wrap, y_wrap, z_wrap);
	Log_assert(-1.0f <= a && a <= 1.0f);
	float b = 0.5f + a * 0.5f;
	float c = lower + (upper - lower) * b;
	return c;
}

float perlin2D(float lower, float upper, float x, float y, int x_wrap, int y_wrap) {
	Log_assert(x_wrap > 0);
	Log_assert(y_wrap > 0);
	float nx = x_wrap * fmodf(x, 1.0f);
	float ny = y_wrap * fmodf(y, 1.0f);
	float a = ::stb_perlin_noise3(nx, ny, 1.0f, x_wrap, y_wrap, 0);
	Log_assert(-1.0f <= a && a <= 1.0f);
	float b = 0.5f + a * 0.5f;
	float c = lower + (upper - lower) * b;
	return c;
}

float perlin1D(float lower, float upper, float x, int x_wrap) {
	Log_assert(x_wrap > 0);
	float nx = x_wrap * fmodf(x, 1.0f);
	float a = ::stb_perlin_noise3(nx, 1.0f, 1.0f, x_wrap, 0, 0);
	Log_assert(-1.0f <= a && a <= 1.0f);
	float b = 0.5f + a * 0.5f;
	float c = lower + (upper - lower) * b;
	return c;
}

}
