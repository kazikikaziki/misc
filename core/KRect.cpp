#include "KRect.h"
#include "KNum.h"

namespace mo {

#pragma region RectF
RectF::RectF():
	xmin(0),
	ymin(0),
	xmax(0),
	ymax(0)
{
	sort();
}
RectF::RectF(const RectF &rect):
	xmin(rect.xmin),
	ymin(rect.ymin),
	xmax(rect.xmax),
	ymax(rect.ymax)
{
	sort();
}
RectF::RectF(int _xmin, int _ymin, int _xmax, int _ymax):
	xmin((float)_xmin),
	ymin((float)_ymin),
	xmax((float)_xmax),
	ymax((float)_ymax)
{
	sort();
}
RectF::RectF(float _xmin, float _ymin, float _xmax, float _ymax):
	xmin(_xmin),
	ymin(_ymin),
	xmax(_xmax),
	ymax(_ymax) {
	sort();
}
RectF::RectF(const Vec2 &pmin, const Vec2 &pmax):
	xmin(pmin.x),
	ymin(pmin.y),
	xmax(pmax.x),
	ymax(pmax.y) {
	sort();
}
RectF RectF::getOffsetRect(const Vec2 &delta) const {
	return RectF(
		delta.x + xmin,
		delta.y + ymin,
		delta.x + xmax,
		delta.y + ymax);
}
RectF RectF::getOffsetRect(float dx, float dy) const {
	return RectF(
		dx + xmin,
		dy + ymin,
		dx + xmax,
		dy + ymax);
}
void RectF::offset(float dx, float dy) {
	xmin += dx;
	ymin += dy;
	xmax += dx;
	ymax += dy;
}
void RectF::offset(int dx, int dy) {
	xmin += dx;
	ymin += dy;
	xmax += dx;
	ymax += dy;
}
RectF RectF::getInflatedRect(float dx, float dy) const {
	return RectF(xmin - dx, ymin - dy, xmax + dx, ymax + dy);
}
bool RectF::isIntersectedWith(const RectF &rc) const {
	if (xmin > rc.xmax) return false;
	if (xmax < rc.xmin) return false;
	if (ymin > rc.ymax) return false;
	if (ymax < rc.ymin) return false;
	return true;
}
RectF RectF::getIntersectRect(const RectF &rc) const {
	if (isIntersectedWith(rc)) {
		return RectF(
			Num_max(xmin, rc.xmin),
			Num_max(ymin, rc.ymin),
			Num_min(xmax, rc.xmax),
			Num_min(ymax, rc.ymax)
		);
	} else {
		return RectF();
	}
}
RectF RectF::getUnionRect(const Vec2 &p) const {
	return RectF(
		Num_min(xmin, p.x),
		Num_min(ymin, p.y),
		Num_max(xmax, p.x),
		Num_max(ymax, p.y)
	);
}
RectF RectF::getUnionRect(const RectF &rc) const {
	return RectF(
		Num_min(xmin, rc.xmin),
		Num_min(ymin, rc.ymin),
		Num_max(xmax, rc.xmax),
		Num_max(ymax, rc.ymax)
	);
}
bool RectF::isZeroSized() const {
	return Num_equals(xmin, xmax) && Num_equals(ymin, ymax);
}
bool RectF::isEqual(const RectF &rect) const {
	return 
		Num_equals(xmin, rect.xmin) &&
		Num_equals(xmax, rect.xmax) &&
		Num_equals(ymin, rect.ymin) &&
		Num_equals(ymax, rect.ymax);
}
bool RectF::contains(float x, float y) const {
	return (xmin <= x && x < xmax) &&
		   (ymin <= y && y < ymax);
}
bool RectF::contains(int x, int y) const {
	return (xmin <= (float)x && (float)x < xmax) &&
		   (ymin <= (float)y && (float)y < ymax);
}
bool RectF::contains(const Vec2 &p) const {
	return (xmin <= p.x && p.x < xmax) &&
		   (ymin <= p.y && p.y < ymax);
}
Vec2 RectF::getCenter() const {
	return Vec2(
		(xmin + xmax) / 2.0f,
		(ymin + ymax) / 2.0f
	);
}
Vec2 RectF::getMinPoint() const {
	return Vec2(xmin, ymin);
}
Vec2 RectF::getMaxPoint() const {
	return Vec2(xmax, ymax);
}
float RectF::getSizeX() const {
	return xmax - xmin;
}
float RectF::getSizeY() const {
	return ymax - ymin;
}
void RectF::inflate(float dx, float dy) {
	xmin -= dx;
	xmax += dx;
	ymin -= dy;
	ymax += dy;
}
void RectF::unionWith(const RectF &rect, bool unionX, bool unionY) {
	if (unionX) {
		xmin = Num_min(xmin, rect.xmin);
		xmax = Num_max(xmax, rect.xmax);
	}
	if (unionY) {
		ymin = Num_min(ymin, rect.ymin);
		ymax = Num_max(ymax, rect.ymax);
	}
}
void RectF::unionWithX(float x) {
	if (x < xmin) xmin = x;
	if (x > xmax) xmax = x;
}
void RectF::unionWithY(float y) {
	if (y < ymin) ymin = y;
	if (y > ymax) ymax = y;
}
void RectF::sort() {
	const auto x0 = Num_min(xmin, xmax);
	const auto x1 = Num_max(xmin, xmax);
	const auto y0 = Num_min(ymin, ymax);
	const auto y1 = Num_max(ymin, ymax);
	xmin = x0;
	ymin = y0;
	xmax = x1;
	ymax = y1;
}
#pragma endregion // RectF

} // namespace
