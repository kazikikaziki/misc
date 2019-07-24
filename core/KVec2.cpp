#include "KVec2.h"
#include <assert.h>
#include <math.h>

#define INVALID_MATH_OPERATION  assert(0)


namespace mo {

float Vec2::dot(const Vec2 &b) const {
	return x*b.x + y*b.y;
}
float Vec2::cross(const Vec2 &b) const {
	return x*b.y - y*b.x;
}
float Vec2::getLength() const {
	return sqrtf(getLengthSq());
}
float Vec2::getLengthSq() const {
	const Vec2 &a = *this;
	return a.dot(a);
}
Vec2 Vec2::getNormalized() const {
	float r = getLength();
	if (r > 0) {
		return (*this) / r;
	} else {
		INVALID_MATH_OPERATION;
		return Vec2();
	}
}
bool Vec2::isZero() const {
	return getLengthSq() == 0;
}
bool Vec2::isNormalized() const {
	return getLengthSq() == 1;
}

} // namespace
