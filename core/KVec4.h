#pragma once

namespace mo {

	
class Vec4 {
public:
	Vec4(): x(0), y(0), z(0), w(0) {}
	Vec4(const Vec4 &a): x(a.x), y(a.y), z(a.z), w(a.w) {}
	Vec4(float ax, float ay, float az, float aw): x(ax), y(ay), z(az), w(aw) {}
	Vec4(int ax, int ay, int az, int aw): x((float)ax), y((float)ay), z((float)az), w((float)aw) {}

	#pragma region operators
	bool operator == (const Vec4 &b) const {
		return x==b.x && y==b.y && z==b.z && w==b.w;
	}
	bool operator != (const Vec4 &b) const {
		return x!=b.x || y!=b.y || z!=b.z || w!=b.w;
	}
	Vec4 operator + (const Vec4 &b) const {
		return Vec4(x+b.x, y+b.y, z+b.z, w+b.w);
	}
	Vec4 operator - (const Vec4 &b) const {
		return Vec4(x-b.x, y-b.y, z-b.z, w-b.w);
	}
	Vec4 operator - () const {
		return Vec4(-x, -y, -z, -w);
	}
	Vec4 operator * (float b) const {
		return Vec4(x*b, y*b, z*b, w*b);
	}
	Vec4 operator / (float b) const {
		return (b!=0) ? Vec4(x/b, y/b, z/b, w/b): Vec4();
	}
	Vec4 & operator = (const Vec4 &b) {
		x = b.x;
		y = b.y;
		z = b.z;
		w = b.w;
		return *this;
	}
	Vec4 & operator += (const Vec4 &b) {
		x += b.x;
		y += b.y;
		z += b.z;
		w += b.w;
		return *this;
	}
	Vec4 & operator -= (const Vec4 &b) {
		x -= b.x;
		y -= b.y;
		z -= b.z;
		w -= b.w;
		return *this;
	}
	Vec4 & operator *= (float b) {
		x *= b;
		y *= b;
		z *= b;
		w *= b;
		return *this;
	}
	Vec4 & operator /= (float b) {
		if (b != 0) {
			x /= b;
			y /= b;
			z /= b;
			w /= b;
		} else {
			x = 0;
			y = 0;
			z = 0;
			w = 0;
		}
		return *this;
	}
	#pragma endregion // operators
	#pragma region methods
	/// 内積
	float dot(const Vec4 &b) const;

	/// 長さ2乗
	float getLengthSq() const;

	/// 長さ
	float getLength() const;

	/// 正規化したものを返す
	Vec4 getNormalized() const;

	/// すべての要素が0に等しいか？
	bool isZero() const;

	/// 正規化済みか？
	bool isNormalized() const;
	#pragma endregion // methods
	//
	float x, y, z, w;
};


} // namespace
 