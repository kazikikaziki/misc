#pragma once
#include <math.h>
#include "KNum.h"
#include "KVec3.h"
#include "KStd.h"
#include "KQuat.h"
#include <assert.h>

namespace mo {


class Quat {
public:
	Quat();
	Quat(const Quat &q);
	Quat(float qx, float qy, float qz, float qw);

	/// 軸ベクトルと回転角度（度）からクォターニオンを作成
	Quat(const Vec3 &axis, float deg);

	#pragma region operators
	bool operator == (const Quat &b) const {
		return x==b.x && y==b.y && z==b.z && w==b.w;
	}
	bool operator != (const Quat &b) const {
		return x!=b.x || y!=b.y || z!=b.z || w!=b.w;
	}
	Quat operator - () const {
		return Quat(-x, -y, -z, -w);
	}

	/// クォターニオン同士の積
	Quat operator * (const Quat &b) const {
		return Quat(
			w*b.x + x*b.w + y*b.z - z*b.y,
			w*b.y - x*b.z + y*b.w + z*b.x,
			w*b.z + x*b.y - y*b.x + z*b.w,
			w*b.w - x*b.x - y*b.y - z*b.z
		);
	}

	/// クォターニオンとベクトルの積
	Quat operator * (const Vec3 &v) const {
		return Quat(
			 w*v.x + y*v.z - z*v.y,
			 w*v.y + z*v.x - x*v.z,
			 w*v.z + x*v.y - y*v.x,
			-x*v.x - y*v.y - z*v.z
		);
	}

	Quat & operator = (const Quat &b) {
		x = b.x;
		y = b.y;
		z = b.z;
		w = b.w;
		return *this;
	}
	Quat & operator *= (const Quat &b) {
		*this = (*this) * b;
		return *this;
	}

	Quat & operator *= (const Vec3 &v) {
		*this = (*this) * v;
		return *this;
	}


	#pragma endregion // operators

	#pragma region methods
	/// 内積
	float dot(const Quat &b) const;

	/// ノルム2乗
	float getLengthSq() const;

	/// ノルム
	float getLength() const;

	/// 正規化したものを返す
	Quat getNormalized() const;

	/// すべての要素が0に等しいか？
	bool isZero() const;

	/// 正規化済みか？
	bool isNormalized() const;

	/// クォターニオンの逆元（qを打ち消す回転）
	Quat inverse() const;

	/// クォターニオンの軸ベクトル
	Vec3 axis() const;

	/// クォターニオンの回転角度（ラジアン）
	float radians() const;
	float degrees() const;

	/// クォターニオンの線形補間
	Quat lerp(const Quat &b, float t) const;

	/// クォターニオンの球面補間
	Quat slerp(const Quat &b, float t) const;

	/// クォターニオン a から b に補完したい時、実際に補間に使う b を返す。
	/// 符号を反転させたクォターニオンは反転前と全く同じ回転を表すが、
	/// 4次元空間内では原点を挟んで対称な位置にある（符号が逆なので）。
	/// なのでクォターニオン b と -b は回転としては同じだが、
	/// a から b に補完するのと a から -b に補完するのでは途中経過が異なる。
	/// a と -b を比べて、a に近い方の b を返す
	/// https://www.f-sp.com/entry/2017/06/30/221124
	Quat near(const Quat &b) const;

	/// クオータニオン q で ベクトル v を回す
	Vec3 rot(const Vec3 &v) const;
	#pragma endregion // methods

	//
	float x, y, z, w;
};

} // namespace
