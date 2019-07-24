#include "KQuat.h"
#include <assert.h>
#include <math.h>

#define INVALID_MATH_OPERATION  assert(0)


namespace mo {

Quat::Quat() {
	x = 0;
	y = 0;
	z = 0;
	w = 1;
}
Quat::Quat(const Quat &q) {
	x = q.x;
	y = q.y;
	z = q.z;
	w = q.w;
}
Quat::Quat(float qx, float qy, float qz, float qw) {
	x = qx;
	y = qy;
	z = qz;
	w = qw;
}
Quat::Quat(const Vec3 &axis, float deg) {
	// http://marupeke296.com/DXG_No10_Quaternion.html
	// https://gist.github.com/mattatz/40a91588d5fb38240403f198a938a593
	float len = axis.getLength();
	if (len > 0) {
		float rad = Num_radians(deg);
		float s = sinf(rad / 2.0f) / len;
		x = axis.x * s;
		y = axis.y * s;
		z = axis.z * s;
		w = cosf(rad / 2.0f);
	} else {
		INVALID_MATH_OPERATION;
		x = 0.0f;
		y = 0.0f;
		z = 0.0f;
		w = 1.0f;
	}
}

float Quat::dot(const Quat &b) const {
	return x*b.x + y*b.y + z*b.z + w*b.w;
}
float Quat::getLength() const {
	return sqrtf(getLengthSq());
}
float Quat::getLengthSq() const {
	const Quat &a = *this;
	return a.dot(a);
}
Quat Quat::inverse() const {
	return Quat(-x, -y, -z, w); // 複素共益を取る
}
Vec3 Quat::axis() const {
	return Vec3(x, y, z).getNormalized();
}
float Quat::radians() const {
	if (-1.0f <= w && w <= 1.0f) {
		return 2 * acosf(w);
	} else {
		INVALID_MATH_OPERATION;
		return 0;
	}
}
float Quat::degrees() const {
	return Num_degrees(radians());
}
Quat Quat::getNormalized() const {
	float r = getLength();
	if (r > 0) {
		return Quat(x/r, y/r, z/r, w/r);
	} else {
		INVALID_MATH_OPERATION;
		return Quat();
	}
}
/// クォターニオンの線形補間
Quat Quat::lerp(const Quat &b, float t) const {
	// https://www.f-sp.com/entry/2017/06/30/221124
	// http://marupeke296.com/DXG_No58_RotQuaternionTrans.html
	float d =  dot(b);
	float rad;
	if (-1.0f <= d && d <= 1.0f) {
		rad = acosf(d);
	} else {
		INVALID_MATH_OPERATION;
		rad = 0;
	}
	float sin_rad = sinf(rad);

	if (sin_rad != 0) {
		float k1 = sinf((1.0f - t) * rad) / sin_rad;
		float k2 = sinf((       t) * rad) / sin_rad;
		return Quat(
			k1 * x + k2 * b.x,
			k1 * y + k2 * b.y,
			k1 * z + k2 * b.z,
			k1 * w + k2 * b.w).getNormalized();
	} else {
		INVALID_MATH_OPERATION;
		return Quat();
	}
}

/// クォターニオンの球面補間
Quat Quat::slerp(const Quat &b, float t) const {
	float magnitude = sqrtf(getLengthSq() * b.getLengthSq());
	if (magnitude == 0) {
		INVALID_MATH_OPERATION;
		return Quat();
	}
	float product = dot(b) / magnitude;
	if (fabsf(product) < 1.0f) {
		float sig = (product >= 0) ? 1.0f : -1.0f;
		float rad = acosf(sig * product);
		float s1  = sinf(sig * t * rad);
		float s0  = sinf((1.0f - t) * rad);
		float d   = 1.0f / sinf(rad);
		return Quat(
			(x * s0 + b.x * s1) * d,
			(y * s0 + b.y * s1) * d,
			(z * s0 + b.z * s1) * d,
			(w * s0 + b.w * s1) * d);
	} else {
		return *this;
	}
}
/// クォターニオン a から b に補完したい時、実際に補間に使う b を返す。
/// 符号を反転させたクォターニオンは反転前と全く同じ回転を表すが、
/// 4次元空間内では原点を挟んで対称な位置にある（符号が逆なので）。
/// なのでクォターニオン b と -b は回転としては同じだが、
/// a から b に補完するのと a から -b に補完するのでは途中経過が異なる。
/// a と -b を比べて、a に近い方の b を返す
/// https://www.f-sp.com/entry/2017/06/30/221124
Quat Quat::near(const Quat &b) const {
	float d = dot(b);
	if (d >= 0) {
		// a と b のなす角度 <= 180度なので a --> b で補間する
		return b;
	} else {
		// a と b のなす角度 > 180度なので a --> -b で補間する
		return -b;
	}
}

/// クオータニオン q で ベクトル v を回す
Vec3 Quat::rot(const Vec3 &v) const {
	// 回転済みベクトル = q * v * q^-1
	const Quat &q = *this;
	Quat rot = q * v * q.inverse();
	return Vec3(rot.x, rot.y, rot.z);
}
bool Quat::isZero() const {
	return getLengthSq() == 0;
}
bool Quat::isNormalized() const {
	return getLengthSq() == 1;
}

} // namespace
