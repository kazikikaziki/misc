#include "KVec4.h"
#include <assert.h>
#include <math.h>

#define INVALID_MATH_OPERATION  assert(0)


namespace mo {

float Vec4::dot(const Vec4 &b) const {
	return x*b.x + y*b.y + z*b.z + w*b.w;
}
float Vec4::getLengthSq() const {
	const Vec4 &a = *this;
	return a.dot(a);
}
float Vec4::getLength() const {
	return sqrtf(getLengthSq());
}

Vec4 Vec4::getNormalized() const {
	float r = getLength();
	if (r > 0) {
		return (*this) / r;
	} else {
		INVALID_MATH_OPERATION;
		return Vec4();
	}
}
bool Vec4::isZero() const {
	return getLengthSq() == 0;
}
bool Vec4::isNormalized() const {
	return getLengthSq() == 1;
}

} // namespace
