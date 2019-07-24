#pragma once

namespace mo {


class Vec2 {
public:
	Vec2(): x(0), y(0) {}
	Vec2(const Vec2 &a): x(a.x), y(a.y) {}
	Vec2(float ax, float ay): x(ax), y(ay) {}
	Vec2(int ax, int ay): x((float)ax), y((float)ay) {}

	#pragma region operators
	bool operator == (const Vec2 &b) const {
		return x==b.x && y==b.y;
	}
	bool operator != (const Vec2 &b) const {
		return x!=b.x || y!=b.y;
	}
	Vec2 operator + (const Vec2 &b) const {
		return Vec2(x+b.x, y+b.y);
	}
	Vec2 operator - (const Vec2 &b) const {
		return Vec2(x-b.x, y-b.y);
	}
	Vec2 operator - () const {
		return Vec2(-x, -y);
	}
	Vec2 operator * (float b) const {
		return Vec2(x*b, y*b);
	}
	Vec2 operator / (float b) const {
		return (b!=0) ? Vec2(x/b, y/b) : Vec2();
	}
	Vec2 & operator = (const Vec2 &b) {
		x = b.x;
		y = b.y;
		return *this;
	}
	Vec2 & operator += (const Vec2 &b) {
		x += b.x;
		y += b.y;
		return *this;
	}
	Vec2 & operator -= (const Vec2 &b) {
		x -= b.x;
		y -= b.y;
		return *this;
	}
	Vec2 & operator *= (float b) {
		x *= b;
		y *= b;
		return *this;
	}
	Vec2 & operator /= (float b) {
		if (b != 0) {
			x /= b;
			y /= b;
		} else {
			x = 0;
			y = 0;
		}
		return *this;
	}
	#pragma endregion // operators
	#pragma region methods
	/// 内積
	float dot(const Vec2 &b) const;

	/// 外積
	float cross(const Vec2 &b) const;

	/// 長さ
	float getLength() const;

	/// 長さ2乗
	float getLengthSq() const;

	/// 正規化したものを返す
	Vec2 getNormalized() const;

	/// すべての要素が0に等しいか？
	bool isZero() const;

	/// 正規化済みか？
	bool isNormalized() const;

	#pragma endregion // methods
	//
	float x, y;
};


} // namespace
