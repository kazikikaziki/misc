#include "KVec3.h"
#include <assert.h>
#include <math.h>

#define INVALID_MATH_OPERATION  assert(0)


namespace mo {

float Vec3::dot(const Vec3 &b) const {
	return x*b.x + y*b.y + z*b.z;
}
Vec3 Vec3::cross(const Vec3 &b) const {
	return Vec3(
		y*b.z - z*b.y,
		z*b.x - x*b.z,
		x*b.y - y*b.x
	);
}
float Vec3::getLength() const {
	return sqrtf(getLengthSq());
}
float Vec3::getLengthSq() const {
	const Vec3 &a = *this;
	return a.dot(a);
}
Vec3 Vec3::getNormalized() const {
	float r = getLength();
	if (r > 0) {
		return (*this) / r;
	} else {
		INVALID_MATH_OPERATION;
		return Vec3();
	}
}
bool Vec3::isZero() const {
	return getLengthSq() == 0;
}
bool Vec3::isNormalized() const {
	return getLengthSq() == 1;
}
Vec3 Vec3::lerp(const Vec3 &a, float t) const {
	return Vec3(
		x + (a.x - x) * t,
		y + (a.y - y) * t,
		z + (a.z - z) * t);
}

} // namespace
