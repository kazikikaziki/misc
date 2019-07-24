#pragma once
#include "KVec2.h"
#include "KVec3.h"
#include "KVec4.h"
#pragma warning (disable: 4201) // 非標準の拡張機能: 無名の構造体

namespace mo {

class Quat;

class Matrix4 {
public:
	/// 平行移動行列
	static Matrix4 fromTranslation(float x, float y, float z);
	static Matrix4 fromTranslation(const Vec3 &vec);
	
	/// スケーリング行列
	static Matrix4 fromScale(float x, float y, float z);
	static Matrix4 fromScale(const Vec3 &vec);
	
	/// 回転行列（左手系）
	static Matrix4 fromRotation(const Vec3 &axis, float degrees);
	static Matrix4 fromRotation(const Quat &q);
	
	/// せん断行列
	/// X軸に平行にdeg度傾斜させる（Y軸が時計回りにdeg度傾く）
	static Matrix4 fromSkewX(float deg);

	/// せん断行列
	/// Y軸に平行にdeg度傾斜させる（X軸が時計回りにdeg度傾く）
	static Matrix4 fromSkewY(float deg);

	/// 平行投影行列（左手系）
	static Matrix4 fromOrtho(float width, float height, float znear, float zfar);
	static Matrix4 fromOrtho(float left, float right, float bottom, float top, float znear, float zfar);

	/// 透視投影行列（左手系）
	/// @param fovy   上下方向の視野角（ラジアン）
	/// @param aspect アスペクト比（H/W）
	/// @param znear  近クリップ面
	/// @param zfar   遠クリップ面
	static Matrix4 fromFrustumFov(const float fovy, float aspect, float znear, float zfar);
	static Matrix4 fromFrustum(float width, float height, float znear, float zfar);
	static Matrix4 fromFrustum(float left, float right, float bottom, float top, float znear, float zfar);

public:
	/// コンストラクタ（単位行列で初期化）
	Matrix4();
	
	/// コンストラクタ（コピー）
	Matrix4(const Matrix4 &a);

	/// コンストラクタ（成分を与えて初期化）
	Matrix4(float e11, float e12, float e13, float e14,
	        float e21, float e22, float e23, float e24,
	        float e31, float e32, float e33, float e34,
	        float e41, float e42, float e43, float e44);

	bool operator == (const Matrix4 &a) const;
	bool operator != (const Matrix4 &a) const;
	Matrix4 operator + (const Matrix4 &a) const;
	Matrix4 operator - (const Matrix4 &a) const;
	Matrix4 operator * (const Matrix4 &a) const;

	float get(int row, int col) const;
	void set(int row, int col, float value);

	bool equals(const Matrix4 &b, float tolerance) const;
	Matrix4 transpose() const;
	float determinant() const;
	Matrix4 inverse() const;

	Vec4 transform(const Vec4 &v) const;
	Vec3 transform(const Vec3 &v) const;
	Vec3 transformZero() const;

	bool computeInverse(Matrix4 *out) const;

	/// row 行 col 列を削除した 3x3 小行列の行列式を返す
	float subdet(int row, int col) const;

	union {
		struct {
			float _11, _12, _13, _14;
			float _21, _22, _23, _24;
			float _31, _32, _33, _34;
			float _41, _42, _43, _44;
		};
		float m[16];
	};
};



} // namespace
