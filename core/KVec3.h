#pragma once

namespace mo {


class Vec3 {
public:
	Vec3(): x(0), y(0), z(0) {}
	Vec3(const Vec3 &a): x(a.x), y(a.y), z(a.z) {}
	Vec3(float ax, float ay, float az): x(ax), y(ay), z(az) {}
	Vec3(int ax, int ay, int az): x((float)ax), y((float)ay), z((float)az) {}

	#pragma region operators
	bool operator == (const Vec3 &b) const {
		return x==b.x && y==b.y && z==b.z;
	}
	bool operator != (const Vec3 &b) const {
		return x!=b.x || y!=b.y || z!=b.z;
	}
	Vec3 operator + (const Vec3 &b) const {
		return Vec3(x+b.x, y+b.y, z+b.z);
	}
	Vec3 operator - (const Vec3 &b) const {
		return Vec3(x-b.x, y-b.y, z-b.z);
	}
	Vec3 operator - () const {
		return Vec3(-x, -y, -z);
	}
	Vec3 operator * (float b) const {
		return Vec3(x*b, y*b, z*b);
	}
	Vec3 operator / (float b) const { 
		return (b!=0) ? Vec3(x/b, y/b, z/b) : Vec3();
	}
	Vec3 & operator = (const Vec3 &b) {
		x = b.x;
		y = b.y;
		z = b.z;
		return *this;
	}
	Vec3 & operator += (const Vec3 &b) {
		x += b.x;
		y += b.y;
		z += b.z;
		return *this;
	}
	Vec3 & operator -= (const Vec3 &b) {
		x -= b.x;
		y -= b.y;
		z -= b.z;
		return *this;
	}
	Vec3 & operator *= (float b) {
		x *= b;
		y *= b;
		z *= b;
		return *this;
	}
	Vec3 & operator /= (float b) {
		if (b != 0) {
			x /= b;
			y /= b;
			z /= b;
		} else {
			x = 0;
			y = 0;
			z = 0;
		}
		return *this;
	}
	#pragma endregion // operators
	#pragma region methods
	/// 内積
	float dot(const Vec3 &b) const;

	/// 外積
	Vec3 cross(const Vec3 &b) const;

	/// 長さ
	float getLength() const;

	/// 長さ2乗
	float getLengthSq() const;

	/// 正規化したものを返す
	Vec3 getNormalized() const;

	/// すべての要素が0に等しいか？
	bool isZero() const;

	/// 正規化済みか？
	bool isNormalized() const;

	/// 線形補間した値を返す
	Vec3 lerp(const Vec3 &a, float t) const;

	#pragma endregion // methods
	//
	float x, y, z;
};


} // namespace
