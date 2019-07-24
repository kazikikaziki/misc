#include "KBezier.h"

#if 0
#	include "KLog.h"
#	define BEZASSERT(x) Log_assert(x)
#else
#	include <assert.h>
#	define BEZASSERT(x) assert(x)
#endif

namespace mo {

#pragma region Bezier3D functions
static Vec3 _Bezpos(const Vec3 *p4, float t) {
	// http://geom.web.fc2.com/geometry/bezier/cubic.html
	BEZASSERT(p4);
	BEZASSERT(0 <= t && t <= 1);
	float T = 1.0f - t;
	float x = t*t*t*p4[3].x + 3*t*t*T*p4[2].x + 3*t*T*T*p4[1].x + T*T*T*p4[0].x;
	float y = t*t*t*p4[3].y + 3*t*t*T*p4[2].y + 3*t*T*T*p4[1].y + T*T*T*p4[0].y;
	float z = t*t*t*p4[3].z + 3*t*t*T*p4[2].z + 3*t*T*T*p4[1].z + T*T*T*p4[0].z;
	return Vec3(x, y, z);
}
static Vec3 _Beztan(const Vec3 *p4, float t) {
	// http://geom.web.fc2.com/geometry/bezier/cubic.html
	BEZASSERT(p4);
	BEZASSERT(0 <= t && t <= 1);
	float T = 1.0f - t;
	float dx = 3*(t*t*(p4[3].x-p4[2].x)+2*t*T*(p4[2].x-p4[1].x)+T*T*(p4[1].x-p4[0].x));
	float dy = 3*(t*t*(p4[3].y-p4[2].y)+2*t*T*(p4[2].y-p4[1].y)+T*T*(p4[1].y-p4[0].y));
	float dz = 3*(t*t*(p4[3].z-p4[2].z)+2*t*T*(p4[2].z-p4[1].z)+T*T*(p4[1].z-p4[0].z));
	return Vec3(dx, dy, dz);
}
// 3次ベジェ曲線を div 個の直線で分割して、その長さを返す
static float _Bezlen1(const Vec3 *p4, int div) {
	BEZASSERT(p4);
	BEZASSERT(div >= 2);
	float result = 0;
	Vec3 pa = p4[0];
	for (int i=1; i<div; i++) {
		float t = (float)i / (div - 1);
		Vec3 pb = _Bezpos(p4, t);
		result += (pb - pa).getLength();
		pa = pb;
	}
	return result;
}

// ベジェ曲線を2分割する
static void _Bezdiv(const Vec3 *p4, Vec3 *out8) {
	BEZASSERT(p4);
	BEZASSERT(out8);
	// 区間1
	out8[0] = p4[0];
	out8[1] = (p4[0] + p4[1]) / 2;
	out8[2] = (p4[0] + p4[1]*2 + p4[2]) / 4;
	out8[3] = (p4[0] + p4[1]*3 + p4[2]*3 + p4[3]) / 8;
	// 区間2
	out8[4] = (p4[0] + p4[1]*3 + p4[2]*3 + p4[3]) / 8;
	out8[5] = (p4[1] + p4[2]*2 + p4[3]) / 4;
	out8[6] = (p4[2] + p4[3]) / 2;
	out8[7] = p4[3];
}

// 直線 ab で b 方向を見たとき、点(px, py)が左にあるなら正の値を、右にあるなら負の値を返す
// z 値は無視する
static float _IsLeft2D(const Vec3 &p, const Vec3 &a, const Vec3 &b) {
	Vec3 ab = b - a;
	Vec3 ap = p - a;
	return ab.x * ap.y - ab.y * ap.x;
}

// 直線 ab と点 p の符号付距離^2。
// 点 a から b を見た時に、点 p が左右どちらにあるかで符号が変わる。正の値なら左側にある
// z 値は無視する
static float _DistSq2D(const Vec3 &p, const Vec3 &a, const Vec3 &b) {
	Vec3 ab = b - a;
	Vec3 ap = p - a;
	float cross = ab.x * ap.y - ab.y * ap.x;
	float deltaSq = ab.x * ab.x + ab.y * ab.y;
	return cross * cross / deltaSq;
}

// 3次ベジェ曲線を再帰分割し、その長さを返す
// （未検証）
static float _Bezlen2(const Vec3 *p4) {
	// 始点と終点
	Vec3 delta = p4[3] - p4[0];
	float delta_len = delta.getLength();

	// 中間点との許容距離を、始点終点の距離の 1/10 に設定する
	float tolerance = delta_len * 0.1f;

	// 曲線中間点 (t = 0.5)
	Vec3 middle = _Bezpos(p4, 0.5f);

	// 始点＆終点を通る直線と、曲線中間点の距離
	float middle_dist_sq = _DistSq2D(middle, p4[0], p4[3]);

	// 始点＆終点を通る直線と、曲線中間点が一定以上離れているなら再分割する
	if (middle_dist_sq >= tolerance) {
		Vec3 pp8[8];
		_Bezdiv(p4, pp8);
		float len0 = _Bezlen2(pp8+0); // 前半区間の長さ
		float len1 = _Bezlen2(pp8+4); // 後半区間の長さ
		return len0 + len1;
	}

	// 始点＆終点を通る直線と、曲線中間点がすごく近い。
	// この場合、曲線がほとんど直線になっているという場合と、
	// 制御点がアルファベットのＺ字型に配置されていて、
	// たまたま曲線中間点が始点＆終点直線に近づいているだけという場合がある
	float left1 = _IsLeft2D(p4[1], p4[0], p4[3]); // 始点から終点を見たときの、点1の位置
	float left2 = _IsLeft2D(p4[2], p4[0], p4[3]); // 始点から終点を見たときの、点2の位置

	// 直線に対する点1の距離符号と点2の距離符号が異なる場合は
	// 直線を挟んで点1と2が逆方向にある。
	// 制御点がアルファベットのＺ字型に配置されているということなので、再分割する
	if (left1 * left2 < 0) {
		Vec3 pp8[8];
		_Bezdiv(p4, pp8);
		float len0 = _Bezlen2(pp8+0); // 前半区間の長さ
		float len1 = _Bezlen2(pp8+4); // 後半区間の長さ
		return len0 + len1;
	}

	// 曲線がほとんど直線と同じ。
	// 始点と終点の距離をそのまま曲線の距離として返す
	return delta.getLength();
}

#pragma endregion // Bezier3D functions



#pragma region KCubicBezier
KCubicBezier::KCubicBezier() {
	length_ = 0;
	dirty_length_ = true;
}
bool KCubicBezier::empty() const {
	return points_.empty();
}
int KCubicBezier::getSegmentCount() const {
	return (int)points_.size() / 4;
}
void KCubicBezier::setSegmentCount(int count) {
	if (count < 0) return;
	points_.resize(count * 4);
	dirty_length_ = true;
}
float KCubicBezier::getWholeLength() const {
	if (dirty_length_) {
		dirty_length_ = false;
		length_ = 0;
		for (int i=0; i<getSegmentCount(); i++) {
			length_ += getLength(i);
		}
	}
	return length_;
}
Vec3 KCubicBezier::getCoordinates(int seg, float t) const {
	Vec3 ret;
	getCoordinates(seg, t, &ret);
	return ret;
}
bool KCubicBezier::getCoordinates(int seg, float t, Vec3 *pos) const {
	if (0 <= seg && seg < getSegmentCount()) {
		if (pos) *pos = _Bezpos(&points_[seg * 4], t);
		return true;
	}
	return false;
}
Vec3 KCubicBezier::getTangent(int seg, float t) const {
	Vec3 ret;
	getTangent(seg, t, &ret);
	return ret;
}
bool KCubicBezier::getTangent(int seg, float t, Vec3 *tangent) const {
	if (0 <= seg && seg < getSegmentCount()) {
		if (tangent) *tangent = _Beztan(&points_[seg * 4], t);
		return true;
	}
	return false;
}
float KCubicBezier::getLength(int seg) const {
	if (0 <= seg && seg < getSegmentCount()) {
		return getLength_Test1(seg, 16);
	}
	return 0.0f;
}
float KCubicBezier::getLength_Test1(int seg, int numdiv) const {
	// 直線分割による近似
	BEZASSERT(0 <= seg && seg < (int)points_.size());
	return _Bezlen1(&points_[seg * 4], 16);
}
float KCubicBezier::getLength_Test2(int seg) const {
	// ベジェ曲線の再帰分割による近似
	BEZASSERT(0 <= seg && seg < (int)points_.size());
	return _Bezlen2(&points_[seg * 4]);
}
void KCubicBezier::addSegment(const Vec3 &a, const Vec3 &b, const Vec3 &c, const Vec3 &d) {
	points_.push_back(a);
	points_.push_back(b);
	points_.push_back(c);
	points_.push_back(d);
	dirty_length_ = true;
}
void KCubicBezier::setPoint(int seg, int point, const Vec3 &pos) {
	int i = seg*4 + point;
	if (0 <= i && i < (int)points_.size()) {
		points_[seg*4 + point] = pos;
		dirty_length_ = true;
	}
}
void KCubicBezier::setPoint(int serial_index, const Vec3 &pos) {
	int seg = serial_index / 4;
	int idx = serial_index % 4;
	setPoint(seg, idx, pos);
}
Vec3 KCubicBezier::getPoint(int seg, int point) const {
	int i = seg*4 + point;
	if (0 <= i && i < (int)points_.size()) {
		return points_[seg*4 + point];
	}
	return Vec3();
}
Vec3 KCubicBezier::getPoint(int serial_index) const {
	int seg = serial_index / 4;
	int idx = serial_index % 4;
	return getPoint(seg, idx);
}
bool KCubicBezier::getFirstAnchor(int seg, Vec3 *anchor, Vec3 *handle) const {
	if (0 <= seg && seg < getSegmentCount()) {
		if (anchor) *anchor = getPoint(seg, 0);
		if (handle) *handle = getPoint(seg, 1);
		return true;
	}
	return false;
}
bool KCubicBezier::getSecondAnchor(int seg, Vec3 *handle, Vec3 *anchor) const {
	if (0 <= seg && seg < getSegmentCount()) {
		if (handle) *handle = getPoint(seg, 2);
		if (anchor) *anchor = getPoint(seg, 3);
		return true;
	}
	return false;
}
void KCubicBezier::setFirstAnchor(int seg, const Vec3 &anchor, const Vec3 &handle) {
	if (0 <= seg && seg < getSegmentCount()) {
		setPoint(seg, 0, anchor);
		setPoint(seg, 1, handle);
	}
}
void KCubicBezier::setSecondAnchor(int seg, const Vec3 &handle, const Vec3 &anchor) {
	if (0 <= seg && seg < getSegmentCount()) {
		setPoint(seg, 2, handle);
		setPoint(seg, 3, anchor);
	}
}
#pragma endregion // KBezierCurve


#ifdef _DEBUG
#include <Windows.h>
void Test_bezier() {
	#define RND (100 + rand() % 400)
	for (int i=0; i<1000; i++) {
		Vec3 p[] = {
			Vec3(RND, RND, 0),
			Vec3(RND, RND, 0),
			Vec3(RND, RND, 0),
			Vec3(RND, RND, 0)
		};
		KCubicBezier b;
		b.addSegment(p[0], p[1], p[2], p[3]);

		// 多角線による近似
		float len1 = b.getLength_Test1(0, 100);
		// 再帰分割での近似
		float len2 = b.getLength_Test2(0);

		// それぞれ算出した長さ値が一定以上異なっていれば SVG 形式で曲線パラメータを出力する
		// なお、このテキストをそのまま
		// http://www.useragentman.com/tests/textpath/bezier-curve-construction-set.html
		// に貼りつければブラウザで確認できる
		if (fabsf(len1 - len2) >= 1.0f) {
			char s[1024];
			sprintf(s, "M %g, %g C %g, %g, %g, %g, %g, %g\n", p[0].x, p[0].y, p[1].x, p[1].y, p[2].x, p[2].y, p[3].x, p[3].y);
			OutputDebugStringA(s);
		}
	}
	#undef RND
}
#endif

} // namespace
