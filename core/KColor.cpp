#include "KColor.h"
#include <math.h>

namespace mo {


inline float _clampf(float t) {
	return (t < 0.0f) ? 0.0f : (t < 1.0f) ? t : 1.0f;
}
inline int _clamp255(int t) {
	return (t < 0) ? 0 : (t < 255) ? t : 255;
}
inline int _lerpi(int a, int b, float t) {
	return (int)(a + (b - a) * t);
}
inline float _lerpf(float a, float b, float t) {
	return a + (b - a) * t;
}
inline bool _equals(float a, float b) {
	// 実数同士の比較を行うが、最終的には色を 256 段階で表すことがほとんどのため、
	// 1.0 / 255.0 未満の差は誤差とみなして比較する。
	// 実際には 1.0 / 255.0 より若干小さい値 1.0 / 256 を使い、
	// さらに 1/2 の閾値にしておく
	const float maxerr = 0.5f / 256;
	return ::fabsf(a - b) <= maxerr;
}
template <typename T> T _max(T a, T b) {
	return (a > b) ? a : b;
}
template <typename T> T _min(T a, T b) {
	return (a < b) ? a : b;
}


#pragma region Color static members
const Color Color::WHITE = Color(1.0f, 1.0f, 1.0f, 1.0f);
const Color Color::BLACK = Color(0.0f, 0.0f, 0.0f, 1.0f);
const Color Color::ZERO  = Color(0.0f, 0.0f, 0.0f, 0.0f);

Color Color::lerp(const Color &color0, const Color &color1, float t) {
	return Color(
		_lerpf(color0.r, color1.r, t),
		_lerpf(color0.g, color1.g, t),
		_lerpf(color0.b, color1.b, t),
		_lerpf(color0.a, color1.a, t)
	);
}
Color Color::getmax(const Color &dst, const Color &src) {
	return Color(
		_max(dst.r, src.r),
		_max(dst.g, src.g),
		_max(dst.b, src.b),
		_max(dst.a, src.a)
	);
}
Color Color::getmin(const Color &dst, const Color &src) {
	return Color(
		_min(dst.r, src.r),
		_min(dst.g, src.g),
		_min(dst.b, src.b),
		_min(dst.a, src.a)
	);
}
Color Color::add(const Color &dst, const Color &src) {
	return Color(
		_min(dst.r + src.r, 1.0f),
		_min(dst.g + src.g, 1.0f),
		_min(dst.b + src.b, 1.0f),
		_min(dst.a + src.a, 1.0f)
	);
}
Color Color::addAlpha(const Color &dst, const Color &src) {
	return Color(
		_min(dst.r + src.r * src.a, 1.0f),
		_min(dst.g + src.g * src.a, 1.0f),
		_min(dst.b + src.b * src.a, 1.0f),
		_min(dst.a + src.a,         1.0f)
	);
}
Color Color::addAlpha(const Color &dst, const Color &src, const Color &factor) {
	return Color(
		_min(dst.r + src.r * src.a * factor.r * factor.a, 1.0f),
		_min(dst.g + src.g * src.a * factor.g * factor.a, 1.0f),
		_min(dst.b + src.b * src.a * factor.b * factor.a, 1.0f),
		_min(dst.a + src.a,                               1.0f)
	);
}
Color Color::sub(const Color &dst, const Color &src) {
	return Color(
		_max(dst.r - src.r, 0.0f),
		_max(dst.g - src.g, 0.0f),
		_max(dst.b - src.b, 0.0f),
		_max(dst.a - src.a, 0.0f)
	);
}
Color Color::subAlpha(const Color &dst, const Color &src) {
	return Color(
		_max(dst.r - src.r * src.a, 0.0f),
		_max(dst.g - src.g * src.a, 0.0f),
		_max(dst.b - src.b * src.a, 0.0f),
		_max(dst.a - src.a,     0.0f)
	);
}
Color Color::subAlpha(const Color &dst, const Color &src, const Color &factor) {
	return Color(
		_max(dst.r - src.r * src.a * factor.r * factor.a, 0.0f),
		_max(dst.g - src.g * src.a * factor.g * factor.a, 0.0f),
		_max(dst.b - src.b * src.a * factor.b * factor.a, 0.0f),
		_max(dst.a - src.a,                               0.0f)
	);
}
Color Color::mul(const Color &dst, const Color &src) {
	return Color(
		_min(dst.r * src.r, 1.0f),
		_min(dst.g * src.g, 1.0f),
		_min(dst.b * src.b, 1.0f),
		_min(dst.a * src.a, 1.0f)
	);
}
Color Color::blendAlpha(const Color &dst, const Color &src) {
	float a = src.a;
	return Color(
		_min(dst.r * (1.0f - a) + src.r * a, 1.0f),
		_min(dst.g * (1.0f - a) + src.g * a, 1.0f),
		_min(dst.b * (1.0f - a) + src.b * a, 1.0f),
		_min(dst.a * (1.0f - a) +         a, 1.0f)
	);
}
Color Color::blendAlpha(const Color &dst, const Color &src, const Color &factor) {
	float a = src.a * factor.a;
	return Color(
		_min(dst.r * (1.0f - a) + src.r * factor.r * a, 1.0f),
		_min(dst.g * (1.0f - a) + src.g * factor.g * a, 1.0f),
		_min(dst.b * (1.0f - a) + src.b * factor.b * a, 1.0f),
		_min(dst.a * (1.0f - a) +                    a, 1.0f)
	);
}
#pragma endregion Color static members



#pragma region Color
Color::Color() {
	r = 0;
	g = 0;
	b = 0;
	a = 0;
}
Color::Color(float _r, float _g, float _b, float _a) {
	r = _r;
	g = _g;
	b = _b;
	a = _a;
}
Color::Color(const Color &rgb, float _a) {
	r = rgb.r;
	g = rgb.g;
	b = rgb.b;
	a = _a;
}
Color::Color(const Color32 &argb32) {
	r = (float)argb32.r / 255.0f;
	g = (float)argb32.g / 255.0f;
	b = (float)argb32.b / 255.0f;
	a = (float)argb32.a / 255.0f;
}
Color32 Color::toColor32() const {
	int ur = (int)(_clampf(r) * 255.0f);
	int ug = (int)(_clampf(g) * 255.0f);
	int ub = (int)(_clampf(b) * 255.0f);
	int ua = (int)(_clampf(a) * 255.0f);
	return Color32(ur, ug, ub, ua);
}

float * Color::floats() const {
	return (float *)this;
}
float Color::grayscale() const {
	// R: 0.298912
	// G: 0.586611
	// B: 0.114478
	return r * 0.3f + g * 0.6f + b * 0.1f;
}
Color Color::clamp() const {
	return Color(
		_clampf(r),
		_clampf(g),
		_clampf(b),
		_clampf(a));
}
Color Color::operator + (const Color &op) const {
	return Color(
		r + op.r,
		g + op.g,
		b + op.b,
		a + op.a);
}
Color Color::operator + (float k) const {
	return Color(r+k, g+k, b+k, a+k);
}
Color Color::operator - (const Color &op) const {
	return Color(
		r - op.r,
		g - op.g,
		b - op.b,
		a - op.a);
}
Color Color::operator - (float k) const {
	return Color(r-k, g-k, b-k, a-k);
}
Color Color::operator - () const {
	return Color(-r, -g, -b, -a);
}
Color Color::operator * (const Color &op) const {
	return Color(
		r * op.r,
		g * op.g,
		b * op.b,
		a * op.a);
}
Color Color::operator * (float k) const {
	return Color(r*k, g*k, b*k, a*k);
}
Color Color::operator / (float k) const {
	return Color(r/k, g/k, b/k, a/k);
}
Color Color::operator / (const Color &op) const {
	return Color(
		r / op.r,
		g / op.g,
		b / op.b,
		a / op.a);
}
bool Color::equals(const Color &other, float tolerance) const {
	float dr = fabsf(other.r - r);
	float dg = fabsf(other.g - g);
	float db = fabsf(other.b - b);
	float da = fabsf(other.a - a);
	return (dr <= tolerance) && (dg <= tolerance) && (db <= tolerance) && (da <= tolerance);
}
bool Color::operator == (const Color &op) const {
	return 
		_equals(this->r, op.r) &&
		_equals(this->g, op.g) &&
		_equals(this->b, op.b) &&
		_equals(this->a, op.a);
}
bool Color::operator != (const Color &op) const {
	return !(*this == op);
}
Color & Color::operator += (const Color &op) {
	r += op.r;
	g += op.g;
	b += op.b;
	a += op.a;
	return *this;
}
Color & Color::operator += (float k) {
	r += k;
	g += k;
	b += k;
	a += k;
	return *this;
}
Color & Color::operator -= (const Color &op) {
	r -= op.r;
	g -= op.g;
	b -= op.b;
	a -= op.a;
	return *this;
}
Color & Color::operator -= (float k) {
	r -= k;
	g -= k;
	b -= k;
	a -= k;
	return *this;
}
Color & Color::operator *= (const Color &op) {
	r *= op.r;
	g *= op.g;
	b *= op.b;
	a *= op.a;
	return *this;
}
Color & Color::operator *= (float k) {
	r *= k;
	g *= k;
	b *= k;
	a *= k;
	return *this;
}
Color & Color::operator /= (const Color &op) {
	r /= op.r;
	g /= op.g;
	b /= op.b;
	a /= op.a;
	return *this;
}
Color & Color::operator /= (float k) {
	r /= k;
	g /= k;
	b /= k;
	a /= k;
	return *this;
}
#pragma endregion // Color




#pragma region Color32
const Color32 Color32::WHITE = Color32(0xFF, 0xFF, 0xFF, 0xFF);
const Color32 Color32::BLACK = Color32(0x00, 0x00, 0x00, 0xFF);
const Color32 Color32::ZERO  = Color32(0x00, 0x00, 0x00, 0x00);

Color32 Color32::getmax(const Color32 &dst, const Color32 &src) {
	return Color32(
		_max(dst.r, src.r),
		_max(dst.g, src.g),
		_max(dst.b, src.b),
		_max(dst.a, src.a)
	);
}
Color32 Color32::getmin(const Color32 &dst, const Color32 &src) {
	return Color32(
		_min(dst.r, src.r),
		_min(dst.g, src.g),
		_min(dst.b, src.b),
		_min(dst.a, src.a)
	);
}
Color32 Color32::add(const Color32 &dst, const Color32 &src) {
	return Color32(
		_min((int)dst.r + src.r, 255),
		_min((int)dst.g + src.g, 255),
		_min((int)dst.b + src.b, 255),
		_min((int)dst.a + src.a, 255)
	);
}
Color32 Color32::addAlpha(const Color32 &dst, const Color32 &src) {
	int a = src.a;
	return Color32(
		(int)dst.r + src.r * a / 255,
		(int)dst.g + src.g * a / 255,
		(int)dst.b + src.b * a / 255,
		(int)dst.a + src.a
	);
}
Color32 Color32::addAlpha(const Color32 &dst, const Color32 &src, const Color32 &factor) {
	int a = src.a * factor.a / 255;
	return Color32(
		(int)dst.r + src.r * a * factor.r / 255 / 255,
		(int)dst.g + src.g * a * factor.g / 255 / 255,
		(int)dst.b + src.b * a * factor.b / 255 / 255,
		(int)dst.a + src.a
	);
}
Color32 Color32::sub(const Color32 &dst, const Color32 &src) {
	return Color32(
		_max((int)dst.r - src.r, 0),
		_max((int)dst.g - src.g, 0),
		_max((int)dst.b - src.b, 0),
		_max((int)dst.a - src.a, 0)
	);
}
Color32 Color32::subAlpha(const Color32 &dst, const Color32 &src) {
	int a = src.a;
	return Color32(
		(int)dst.r - src.r * a / 255,
		(int)dst.g - src.g * a / 255,
		(int)dst.b - src.b * a / 255,
		(int)dst.a - src.a
	);
}
Color32 Color32::subAlpha(const Color32 &dst, const Color32 &src, const Color32 &factor) {
	int a = (int)src.a * (int)factor.a / 255;
	return Color32(
		(int)dst.r - src.r * a * factor.r / 255 / 255,
		(int)dst.g - src.g * a * factor.g / 255 / 255,
		(int)dst.b - src.b * a * factor.b / 255 / 255,
		(int)dst.a - src.a
	);
}
Color32 Color32::mul(const Color32 &dst, const Color32 &src) {
	// src の各要素の値 0 .. 255 を 0.0 .. 1.0 にマッピングして乗算する
	int r = _clamp255(dst.r * src.r / 255);
	int g = _clamp255(dst.g * src.g / 255);
	int b = _clamp255(dst.b * src.b / 255);
	int a = _clamp255(dst.a * src.a / 255);
	return Color32(r, g, b, a);
}
Color32 Color32::mul(const Color32 &dst, float factor) {
	int r = _clamp255(dst.r * factor);
	int g = _clamp255(dst.g * factor);
	int b = _clamp255(dst.b * factor);
	int a = _clamp255(dst.a * factor);
	return Color32(r, g, b, a);
}
Color32 Color32::blendAlpha(const Color32 &dst, const Color32 &src) {
	int a = src.a;
	return Color32(
		((int)dst.r * (255 - a) + src.r * a) / 255,
		((int)dst.g * (255 - a) + src.g * a) / 255,
		((int)dst.b * (255 - a) + src.b * a) / 255,
		((int)dst.a * (255 - a) + 255   * a) / 255
	);
}
Color32 Color32::blendAlpha(const Color32 &dst, const Color32 &src, const Color32 &factor) {
	int a = src.a * factor.a / 255;
	return Color32(
		((int)dst.r * (255 - a) + src.r * a) * factor.r / 255 / 255,
		((int)dst.g * (255 - a) + src.g * a) * factor.g / 255 / 255,
		((int)dst.b * (255 - a) + src.b * a) * factor.b / 255 / 255,
		((int)dst.a * (255 - a) + 255   * a) / 255
	);
}
Color32 Color32::lerp(const Color32 &x, const Color32 &y, float t) {
	t = _clampf(t);
	return Color32(
		_lerpi(x.r, y.r, t),
		_lerpi(x.g, y.g, t),
		_lerpi(x.b, y.b, t),
		_lerpi(x.a, y.a, t)
	);
}
Color32 Color32::fromARGB32(uint32_t argb32) {
	int a = (argb32 >> 24) & 0xFF;
	int r = (argb32 >> 16) & 0xFF;
	int g = (argb32 >>  8) & 0xFF;
	int b = (argb32 >>  0) & 0xFF;
	return Color32(r, g, b, a);
}
Color32 Color32::fromXRGB32(uint32_t argb32) {
	int r = (argb32 >> 16) & 0xFF;
	int g = (argb32 >>  8) & 0xFF;
	int b = (argb32 >>  0) & 0xFF;
	return Color32(r, g, b, 0xFF);
}
Color32::Color32() {
	a = 0;
	r = 0;
	g = 0;
	b = 0;
}
Color32::Color32(const Color32 &other) {
	r = other.r;
	g = other.g;
	b = other.b;
	a = other.a;
}
Color32::Color32(int _r, int _g, int _b, int _a) {
	a = (uint8_t)(_a & 0xFF);
	r = (uint8_t)(_r & 0xFF);
	g = (uint8_t)(_g & 0xFF);
	b = (uint8_t)(_b & 0xFF);
}
Color32::Color32(uint32_t argb) { 
	a = (uint8_t)((argb >> 24) & 0xFF);
	r = (uint8_t)((argb >> 16) & 0xFF);
	g = (uint8_t)((argb >>  8) & 0xFF);
	b = (uint8_t)((argb >>  0) & 0xFF);
}
Color32::Color32(const Color &color) {
	a = (uint8_t)(_clampf(color.a) * 255.0f);
	r = (uint8_t)(_clampf(color.r) * 255.0f);
	g = (uint8_t)(_clampf(color.g) * 255.0f);
	b = (uint8_t)(_clampf(color.b) * 255.0f);
}
bool Color32::operator == (const Color32 &other) const {
	return (
		r==other.r && 
		g==other.g &&
		b==other.b &&
		a==other.a
	);
}
bool Color32::operator != (const Color32 &other) const {
	return !(*this==other);
}
Color Color32::toColor() const {
	return Color(
		_clampf(r/255.0f),
		_clampf(g/255.0f),
		_clampf(b/255.0f),
		_clampf(a/255.0f)
	);
}
uint32_t Color32::toUInt32() const {
	return (a << 24) | (r << 16) | (g << 8) | b;
}
uint8_t Color32::grayscale() const {
	// NTSC 加重平均でのグレースケール値を計算する。
	// 入力値も出力値も整数なので、簡易版の係数を使う。
	// 途中式での桁あふれに注意
	//return (uint8_t)(((int)r * 3 + g * 6 + b * 1) / 10);
	return (uint8_t)((306 * r + 601 * g + 117 + b) >> 10); // <--　306とか601が既にuint8の範囲外なので、勝手にintに昇格する
}


#pragma endregion // Color32




} // namespace
