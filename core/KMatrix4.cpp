#include "KMatrix4.h"
#include "KQuat.h"
#include <math.h>
#include <memory.h> // memcpy
#include <assert.h>

#if defined MO_NO_SIMD_SSE2 || !defined(_WIN32)
#	define USE_SSE2 0
#else
#	define USE_SSE2 1
#endif

/// SSE2の有無は KSysInfo の mo::getSystemInfo で取得できる
#if USE_SSE2
#	include <intrin.h>
#endif

#define INVALID_MATH_OPERATION  assert(0)
#define MATH_ASSERT(x) assert(x)



namespace mo {


#pragma region Static methods

Matrix4 Matrix4::fromTranslation(float x, float y, float z) {
	return Matrix4(
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		x, y, z, 1);
}
Matrix4 Matrix4::fromTranslation(const Vec3 &vec) {
	return fromTranslation(vec.x, vec.y, vec.z);
}
Matrix4 Matrix4::fromScale(float x, float y, float z) {
	return Matrix4(
		x, 0, 0, 0,
		0, y, 0, 0,
		0, 0, z, 0,
		0, 0, 0, 1);
}
Matrix4 Matrix4::fromScale(const Vec3 &vec) {
	return fromScale(vec.x, vec.y, vec.z);
}
Matrix4 Matrix4::fromRotation(const Vec3 &axis, float degrees) {
	return fromRotation(Quat(axis, degrees));
}
Matrix4 Matrix4::fromRotation(const Quat &q) {
	// http://stackoverflow.com/questions/1556260/convert-quaternion-rotation-to-rotation-matrix
	float x = q.x;
	float y = q.y;
	float z = q.z;
	float w = q.w;
	return Matrix4(
		1-2*y*y-2*z*z,   2*x*y+2*w*z,   2*x*z-2*w*y, 0,
		  2*x*y-2*w*z, 1-2*x*x-2*z*z,   2*y*z+2*w*x, 0,
		  2*x*z+2*w*y,   2*y*z-2*w*x, 1-2*x*x-2*y*y, 0,
		            0,             0,             0, 1);
}
Matrix4 Matrix4::fromSkewX(float deg) {
	float t = tanf(Num_radians(deg));
	return Matrix4(
		1, 0, 0, 0,
		t, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1);
}
Matrix4 Matrix4::fromSkewY(float deg) {
	float t = tanf(Num_radians(deg));
	return Matrix4(
		1, t, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1);
}
Matrix4 Matrix4::fromOrtho(float width, float height, float znear, float zfar) {
	MATH_ASSERT(width > 0);
	MATH_ASSERT(height > 0);
	MATH_ASSERT(znear < zfar);
	// 平行投影行列（左手系）
	// http://www.opengl.org/sdk/docs/man2/xhtml/glOrtho.xml
	// http://msdn.microsoft.com/ja-jp/library/cc372881.aspx
	float x = 2.0f / width;
	float y = 2.0f / height;
	float a = 1.0f / (zfar - znear);
	float b = znear / (znear - zfar);
	return Matrix4(
		x, 0, 0, 0,
		0, y, 0, 0,
		0, 0, a, 0,
		0, 0, b, 1);
}
Matrix4 Matrix4::fromOrtho(float left, float right, float bottom, float top, float znear, float zfar) {
	MATH_ASSERT(left < right);
	MATH_ASSERT(bottom < top);
	MATH_ASSERT(znear < zfar);
	// 平行投影行列（左手系）
	// http://www.opengl.org/sdk/docs/man2/xhtml/glOrtho.xml
	// http://msdn.microsoft.com/ja-jp/library/cc372882.aspx
	float x = 2.0f / (right - left);
	float y = 2.0f / (top - bottom);
	float z = 1.0f / (zfar - znear);
	float a = (right + left) / (right - left);
	float b = (bottom + top) / (bottom - top);
	float c = znear / (znear - zfar);
	return Matrix4(
		x, 0, 0, 0,
		0, y, 0, 0,
		0, 0, z, 0,
		a, b, c, 1);
}
Matrix4 Matrix4::fromFrustumFov(const float fovy, float aspect, float znear, float zfar) {
	MATH_ASSERT(aspect > 0);
	MATH_ASSERT(znear < zfar);
	// 透視投影行列（左手系）
	// http://tech-sketch.jp/2011/12/3d4.html
	// http://msdn.microsoft.com/ja-jp/library/cc372898.aspx
	float h = 1.0f / tanf(fovy/2);
	float w = h / aspect;
	float a = zfar / (zfar - znear);
	float b = zfar * znear / (znear - zfar);
	return Matrix4(
		w, 0, 0, 0,
		0, h, 0, 0,
		0, 0, a, 1,
		0, 0, b, 0);
}
Matrix4 Matrix4::fromFrustum(float width, float height, float znear, float zfar) {
	MATH_ASSERT(width > 0);
	MATH_ASSERT(height > 0);
	MATH_ASSERT(znear < zfar);
	// 透視投影行列（左手系）
	// http://gunload.web.fc2.com/opengl/glfrustum.html
	// http://msdn.microsoft.com/ja-jp/library/cc372887.aspx
	float w = 2 * znear / width;
	float h = 2 * znear / height;
	float a = zfar / (zfar - znear);
	float b = zfar * znear / (znear - zfar);
	return Matrix4(
		w, 0, 0, 0,
		0, h, 0, 0,
		0, 0, a, 1,
		0, 0, b, 0);
}
Matrix4 Matrix4::fromFrustum(float left, float right, float bottom, float top, float znear, float zfar) {
	MATH_ASSERT(left < right);
	MATH_ASSERT(bottom < top);
	MATH_ASSERT(znear < zfar);
	// 透視投影行列（左手系）
	// http://gunload.web.fc2.com/opengl/glfrustum.html
	// http://msdn.microsoft.com/ja-jp/library/cc372889.aspx
	const float x = (2 * znear) / (right - left);
	const float y = (2 * znear) / (top - bottom);
	const float a = (right + left) / (right - left);
	const float b = (top + bottom) / (top - bottom);
	const float c = -(zfar + znear) / (zfar - znear);
	const float d = -(2 * zfar * znear) / (zfar - znear);
	return Matrix4(
		x, 0, 0, 0,
		0, y, 0, 0,
		a, b, c, -1,
		0, 0, d, 0);
}
#pragma endregion // Static methods




#pragma region Methods
Matrix4::Matrix4() {
	_11=1; _12=0; _13=0; _14=0;
	_21=0; _22=1; _23=0; _24=0;
	_31=0; _32=0; _33=1; _34=0;
	_41=0; _42=0; _43=0; _44=1;
}
Matrix4::Matrix4(const Matrix4 &a) {
	memcpy(m, a.m, sizeof(float)*16);
}
Matrix4::Matrix4(
	float e11, float e12, float e13, float e14,
	float e21, float e22, float e23, float e24,
	float e31, float e32, float e33, float e34,
	float e41, float e42, float e43, float e44
) {
	_11=e11; _12=e12; _13=e13; _14=e14;
	_21=e21; _22=e22; _23=e23; _24=e24;
	_31=e31; _32=e32; _33=e33; _34=e34;
	_41=e41; _42=e42; _43=e43; _44=e44;
}
bool Matrix4::operator == (const Matrix4 &a) const {
	return memcmp(m, a.m, sizeof(float)*16) == 0;
}
bool Matrix4::operator != (const Matrix4 &a) const {
	return memcmp(m, a.m, sizeof(float)*16) != 0;
}
Matrix4 Matrix4::operator + (const Matrix4 &a) const {
	return Matrix4(
		_11 + a._11, _21 + a._21, _31 + a._31, _41 + a._41,
		_12 + a._12, _22 + a._22, _32 + a._32, _42 + a._42,
		_13 + a._13, _23 + a._23, _33 + a._33, _43 + a._43,
		_14 + a._14, _24 + a._24, _34 + a._34, _44 + a._44
	);
}
Matrix4 Matrix4::operator - (const Matrix4 &a) const {
	return Matrix4(
		_11 - a._11, _21 - a._21, _31 - a._31, _41 - a._41,
		_12 - a._12, _22 - a._22, _32 - a._32, _42 - a._42,
		_13 - a._13, _23 - a._23, _33 - a._33, _43 - a._43,
		_14 - a._14, _24 - a._24, _34 - a._34, _44 - a._44
	);
}
Matrix4 Matrix4::operator * (const Matrix4 &a) const {
#if USE_SSE2
	Matrix4 out;
	__m128  xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

	xmm4 = _mm_loadu_ps(a.m +  0);
	xmm5 = _mm_loadu_ps(a.m +  4);
	xmm6 = _mm_loadu_ps(a.m +  8);
	xmm7 = _mm_loadu_ps(a.m + 12);

	// column0
	xmm0 = _mm_load1_ps(m+0);
	xmm1 = _mm_load1_ps(m+1);
	xmm2 = _mm_load1_ps(m+2);
	xmm3 = _mm_load1_ps(m+3);

	xmm0 = _mm_mul_ps(xmm0, xmm4);
	xmm1 = _mm_mul_ps(xmm1, xmm5);
	xmm2 = _mm_mul_ps(xmm2, xmm6);
	xmm3 = _mm_mul_ps(xmm3, xmm7);

	xmm0 = _mm_add_ps(xmm0, xmm1);
	xmm2 = _mm_add_ps(xmm2, xmm3);
	xmm0 = _mm_add_ps(xmm0, xmm2);

	_mm_storeu_ps(out.m, xmm0);

	// column1
	xmm0 = _mm_load1_ps(m+4);
	xmm1 = _mm_load1_ps(m+5);
	xmm2 = _mm_load1_ps(m+6);
	xmm3 = _mm_load1_ps(m+7);

	xmm0 = _mm_mul_ps(xmm0, xmm4);
	xmm1 = _mm_mul_ps(xmm1, xmm5);
	xmm2 = _mm_mul_ps(xmm2, xmm6);
	xmm3 = _mm_mul_ps(xmm3, xmm7);

	xmm0 = _mm_add_ps(xmm0, xmm1);
	xmm2 = _mm_add_ps(xmm2, xmm3);
	xmm0 = _mm_add_ps(xmm0, xmm2);

	_mm_storeu_ps(out.m+4, xmm0);

	// column2
	xmm0 = _mm_load1_ps(m+ 8);
	xmm1 = _mm_load1_ps(m+ 9);
	xmm2 = _mm_load1_ps(m+10);
	xmm3 = _mm_load1_ps(m+11);

	xmm0 = _mm_mul_ps(xmm0, xmm4);
	xmm1 = _mm_mul_ps(xmm1, xmm5);
	xmm2 = _mm_mul_ps(xmm2, xmm6);
	xmm3 = _mm_mul_ps(xmm3, xmm7);

	xmm0 = _mm_add_ps(xmm0, xmm1);
	xmm2 = _mm_add_ps(xmm2, xmm3);
	xmm0 = _mm_add_ps(xmm0, xmm2);

	_mm_storeu_ps(out.m+8, xmm0);

	// column3
	xmm0 = _mm_load1_ps(m+12);
	xmm1 = _mm_load1_ps(m+13);
	xmm2 = _mm_load1_ps(m+14);
	xmm3 = _mm_load1_ps(m+15);

	xmm0 = _mm_mul_ps(xmm0, xmm4);
	xmm1 = _mm_mul_ps(xmm1, xmm5);
	xmm2 = _mm_mul_ps(xmm2, xmm6);
	xmm3 = _mm_mul_ps(xmm3, xmm7);

	xmm0 = _mm_add_ps(xmm0, xmm1);
	xmm2 = _mm_add_ps(xmm2, xmm3);
	xmm0 = _mm_add_ps(xmm0, xmm2);

	_mm_storeu_ps(out.m+12, xmm0);
	return out;
#else
	Matrix4 out;
	for (int row=0; row<4; row++) {
		for (int col=0; col<4; col++) {
			float val =
				get(row, 0) * a.get(0, col) +
				get(row, 1) * a.get(1, col) +
				get(row, 2) * a.get(2, col) +
				get(row, 3) * a.get(3, col);
			out.set(row, col, val);
		}
	}
	return out;
#endif
}


float Matrix4::get(int row, int col) const {
	MATH_ASSERT(0 <= row && row < 4);
	MATH_ASSERT(0 <= col && col < 4);
	return m[row * 4 + col];
}
void Matrix4::set(int row, int col, float value) {
	MATH_ASSERT(0 <= row && row < 4);
	MATH_ASSERT(0 <= col && col < 4);
	m[row * 4 + col] = value;
}

bool Matrix4::equals(const Matrix4 &b, float tolerance) const {
	for (int i=0; i<16; i++) {
		if (fabsf(m[i] - b.m[i]) > tolerance) {
			return false;
		}
	}
	return true;
}

Matrix4 Matrix4::transpose() const {
	return Matrix4(
		_11, _21, _31, _41,
		_12, _22, _32, _42,
		_13, _23, _33, _43,
		_14, _24, _34, _44
	);
}
float Matrix4::determinant() const {
	return (
		+ get(0, 0) * subdet(0, 0)
		- get(1, 0) * subdet(1, 0)
		+ get(2, 0) * subdet(2, 0)
		- get(3, 0) * subdet(3, 0)
	);
}
Matrix4 Matrix4::inverse() const {
	Matrix4 m;
	computeInverse(&m);
	return m;
}
bool Matrix4::computeInverse(Matrix4 *out) const {
	float det = determinant();

	// 行列式が 0 でなければ逆行列が存在する。
	// 行列式は非常に小さい値になる可能性があるため、
	// 0 かどうかの判定には誤差付き判定関数を用いずに直接比較する。
	if (det != 0) {
		if (out) {
			*out = Matrix4(
				 subdet(0, 0) / det, -subdet(1, 0) / det,  subdet(2, 0) / det, -subdet(3, 0) / det,
				-subdet(0, 1) / det,  subdet(1, 1) / det, -subdet(2, 1) / det,  subdet(3, 1) / det,
				 subdet(0, 2) / det, -subdet(1, 2) / det,  subdet(2, 2) / det, -subdet(3, 2) / det,
				-subdet(0, 3) / det,  subdet(1, 3) / det, -subdet(2, 3) / det,  subdet(3, 3) / det
			);
		}
		return true;
	}
	return false;
}
/// 4x4行列 m から row 行 col 列を削除した 3x3 小行列の行列式を返す
float Matrix4::subdet(int row, int col) const {
	MATH_ASSERT(0 <= col && col < 4);
	MATH_ASSERT(0 <= row && row < 4);
	// 移動前と移動元の行、列番号を作成する
	int srcrow[3], srccol[3];
	for (int i=0; i<3; i++) {
		srcrow[i] = (i >= row) ? i+1 : i; // srcrow[小行列の行番号] = 元行列の行番号
		srccol[i] = (i >= col) ? i+1 : i; // srccol[小行列の列番号] = 元行列の列番号
	}
	// 3x3小行列を作成
	Matrix4 sub;
	for (int r=0; r<3; r++) {
		for (int c=0; c<3; c++) {
			float v = get(srcrow[r], srccol[c]);
			sub.set(r, c, v);
		}
	}
	// 3x3行列の行列式
	return (
		sub._11 * sub._22 * sub._33 +
		sub._21 * sub._32 * sub._13 +
		sub._31 * sub._12 * sub._23 -
		sub._11 * sub._32 * sub._23 -
		sub._31 * sub._22 * sub._13 -
		sub._21 * sub._12 * sub._33
	);
}




Vec4 Matrix4::transform(const Vec4 &a) const {
	return Vec4(
		a.x * _11 + a.y * _21 + a.z * _31 + a.w * _41,
		a.x * _12 + a.y * _22 + a.z * _32 + a.w * _42,
		a.x * _13 + a.y * _23 + a.z * _33 + a.w * _43,
		a.x * _14 + a.y * _24 + a.z * _34 + a.w * _44
	);
}
Vec3 Matrix4::transform(const Vec3 &a) const {
	#if 0
	Vec4 b = mul(Vec4(a.x, a.y, a.z, 1.0f), m);
	return Vec3(b.x/b.w, b.y/b.w, b.z/b.w);
	#else
	float x = a.x * _11 + a.y * _21 + a.z * _31 + _41;
	float y = a.x * _12 + a.y * _22 + a.z * _32 + _42;
	float z = a.x * _13 + a.y * _23 + a.z * _33 + _43;
	float w = a.x * _14 + a.y * _24 + a.z * _34 + _44;
	return Vec3(x/w, y/w, z/w);
	#endif
}
Vec3 Matrix4::transformZero() const {
	// transform(Vec3()) と同じ
	#if 0
	float x = 0 * _11 + 0 * _21 + 0 * _31 + _41;
	float y = 0 * _12 + 0 * _22 + 0 * _32 + _42;
	float z = 0 * _13 + 0 * _23 + 0 * _33 + _43;
	float w = 0 * _14 + 0 * _24 + 0 * _34 + _44;
	return Vec3(x/w, y/w, z/w);
	#else
	return Vec3(_41/_44, _42/_44, _43/_44);
	#endif
}


#pragma endregion // Methods



} // namespace
