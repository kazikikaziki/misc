#include "KCollisionMath.h"
#include "KVec3.h"
#include <math.h>
#include <assert.h>

#define COLASSERT assert

namespace mo {

inline float _max(float a, float b) {
	return (a > b) ? a : b;
}
inline float _min(float a, float b) {
	return (a < b) ? a : b;
}

/// 三角形の法線ベクトルを返す
///
/// 三角形法線ベクトル（正規化済み）を得る
/// 時計回りに並んでいるほうを表とする
bool KCollisionMath::getTriangleNormal(Vec3 *result, const Vec3 &o, const Vec3 &a, const Vec3 &b) {
	const Vec3 ao = a - o;
	const Vec3 bo = b - o;
	const Vec3 normal = ao.cross(bo);
	if (!normal.isZero()) {
		if (result) *result = normal.getNormalized();
		return true;
	}
	// 三角形が線分または点に縮退している。
	// 法線は定義できない
	return false;
}

bool KCollisionMath::isAabbIntersected(const Vec3 &pos1, const Vec3 &halfsize1, const Vec3 &pos2, const Vec3 &halfsize2, Vec3 *intersect_center, Vec3 *intersect_halfsize) {
	const Vec3 span = pos1 - pos2;
	const Vec3 maxspan = halfsize1 + halfsize2;
	if (fabsf(span.x) > maxspan.x) return false;
	if (fabsf(span.y) > maxspan.y) return false;
	if (fabsf(span.z) > maxspan.z) return false;
	const Vec3 min1 = pos1 - halfsize1;
	const Vec3 max1 = pos1 + halfsize1;
	const Vec3 min2 = pos2 - halfsize2;
	const Vec3 max2 = pos2 + halfsize2;
	const Vec3 intersect_min(
		_max(min1.x, min2.x),
		_max(min1.y, min2.y),
		_max(min1.z, min2.z));
	const Vec3 intersect_max(
		_min(max1.x, max2.x),
		_min(max1.y, max2.y),
		_min(max1.z, max2.z));
	if (intersect_center) {
		*intersect_center = (intersect_max + intersect_min) / 2;
	}
	if (intersect_halfsize) {
		*intersect_halfsize = (intersect_max - intersect_min) / 2;
	}
	return true;
}
float KCollisionMath::getPointInLeft2D(float px, float py, float ax, float ay, float bx, float by) {
	float ab_x = bx - ax;
	float ab_y = by - ay;
	float ap_x = px - ax;
	float ap_y = py - ay;
	return ab_x * ap_y - ab_y * ap_x;
}
float KCollisionMath::getSignedDistanceOfLinePoint2D(float px, float py, float ax, float ay, float bx, float by) {
	float ab_x = bx - ax;
	float ab_y = by - ay;
	float ap_x = px - ax;
	float ap_y = py - ay;
	float cross = ab_x * ap_y - ab_y * ap_x;
	float delta = sqrtf(ab_x * ab_x + ab_y * ab_y);
	return cross / delta;
}
float KCollisionMath::getPerpendicularLinePoint(const Vec3 &p, const Vec3 &a, const Vec3 &b) {
	Vec3 ab = b - a;
	Vec3 ap = p - a;
	float k = ab.dot(ap) / ab.getLengthSq();
	return k;
}
float KCollisionMath::getPerpendicularLinePoint2D(float px, float py, float ax, float ay, float bx, float by) {
	Vec3 ab(bx - ax, by - ay, 0.0f);
	Vec3 ap(px - ax, py - ay, 0.0f);
	float k = ab.dot(ap) / ab.getLengthSq();
	return k;
}
bool KCollisionMath::isPointInRect2D(float px, float py, float cx, float cy, float xHalf, float yHalf) {
	if (px < cx - xHalf) return false;
	if (px > cx + xHalf) return false;
	if (py < cy - yHalf) return false;
	if (py > cy + yHalf) return false;
	return true;
}
bool KCollisionMath::isPointInXShearedRect2D(float px, float py, float cx, float cy, float xHalf, float yHalf, float xShear) {
	float ymin = cy - yHalf;
	float ymax = cy + yHalf;
	if (py < ymin) return false; // 下辺より下側にいる
	if (py > ymax) return false; // 上辺より上側にいる
	if (getPointInLeft2D(px, py, cx-xHalf-xShear, ymin, cx-xHalf+xShear, ymax) > 0) return false; // 左下から左上を見たとき、左側にいる
	if (getPointInLeft2D(px, py, cx+xHalf-xShear, ymin, cx+xHalf+xShear, ymax) < 0) return false; // 右下から右上を見た時に右側にいる
	return true;
}
bool KCollisionMath::collisionCircleWithPoint2D(float cx, float cy, float cr, float px, float py, float skin, float *xAdj, float *yAdj) {
//	COLASSERT(cr > 0);
	COLASSERT(skin >= 0);
	float dx = cx - px;
	float dy = cy - py;
	float dist = ::hypotf(dx, dy);
	if (fabsf(dist) < cr+skin) {
		if (xAdj && yAdj) {
			// めり込み量
			float penetration_depth = cr - dist;
			if (penetration_depth <= 0) {
				// 押し戻し不要（実際には接触していないが、skin による拡大判定が接触した）
				if (xAdj) *xAdj = 0;
				if (yAdj) *yAdj = 0;
			} else {
				// 押し戻し方向の単位ベクトル
				float normal_x = (dist > 0) ? dx / dist : 1.0f;
				float normal_y = (dist > 0) ? dy / dist : 0.0f;
				// 押し戻し量
				if (xAdj) *xAdj = normal_x * penetration_depth;
				if (yAdj) *yAdj = normal_y * penetration_depth;
			}
		}
		return true;
	}
	return false;
}
bool KCollisionMath::collisionCircleWithLine2D(float cx, float cy, float cr, float x0, float y0, float x1, float y1, float skin, float *xAdj, float *yAdj) {
//	COLASSERT(cr > 0);
	COLASSERT(skin >= 0);
	float dist = getSignedDistanceOfLinePoint2D(cx, cy, x0, y0, x1, y1);
	if (fabsf(dist) < cr+skin) {
		if (xAdj && yAdj) {
			// めり込み量
			float penetration_depth = cr - fabsf(dist);
			if (penetration_depth < 0) {
				// 実際には接触していないが、skin による拡大判定が接触した。押し戻し不要
				if (xAdj) *xAdj = 0;
				if (yAdj) *yAdj = 0;
			} else {
				// 押し戻し方向の単位ベクトル
				// (x0, y0) から (x1, y1) を見たときに、左側を法線方向とする 
				float dx = x1 - x0;
				float dy = y1 - y0;
				float dr = hypotf(dx, dy);
				float normal_x = -dy / dr;
				float normal_y =  dx / dr;
				// 押し戻し量
				if (xAdj) *xAdj = normal_x * penetration_depth;
				if (yAdj) *yAdj = normal_y * penetration_depth;
			}
		}
		return true;
	}
	return false;
}
bool KCollisionMath::collisionCircleWithLinearArea2D(float cx, float cy, float cr, float x0, float y0, float x1, float y1, float skin, float *xAdj, float *yAdj) {
//	COLASSERT(cr > 0);
	COLASSERT(skin >= 0);
	float dist = getSignedDistanceOfLinePoint2D(cx, cy, x0, y0, x1, y1);

	if (dist < cr+skin) {
		if (xAdj && yAdj) {
			// めり込み量
			float penetration_depth = cr - dist;
			if (penetration_depth < 0) {
				// 実際には接触していないが、skin による拡大判定が接触した。押し戻し不要
				if (xAdj) *xAdj = 0;
				if (yAdj) *yAdj = 0;
			} else {
				// 押し戻し方向の単位ベクトル
				// (x0, y0) から (x1, y1) を見たときに、左側を法線方向とする 
				float dx = x1 - x0;
				float dy = y1 - y0;
				float dr = hypotf(dx, dy);
				float normal_x = -dy / dr;
				float normal_y =  dx / dr;
				// 押し戻し量
				if (xAdj) *xAdj = normal_x * penetration_depth;
				if (yAdj) *yAdj = normal_y * penetration_depth;
			}
		}
		return true;
	}
	return false;
}
bool KCollisionMath::collisionCircleWithSegment2D(float cx, float cy, float cr, float x0, float y0, float x1, float y1, float skin, float *xAdj, float *yAdj) {
	float k = getPerpendicularLinePoint2D(cx, cy, x0, y0, x1, y1);
	if (k < 0.0f) {
		if (collisionCircleWithPoint2D(cx, cy, cr, x0, y0, skin, xAdj, yAdj))
			return true;
	} else if (k < 1.0f) {
		if (collisionCircleWithLine2D(cx, cy, cr, x0, y0, x1, y1, skin, xAdj, yAdj))
			return true; 
	} else {
		if (collisionCircleWithPoint2D(cx, cy, cr, x1, y1, skin, xAdj, yAdj))
			return true;
	}
	return false;
}
bool KCollisionMath::collisionCircleWithCircle2D(float x1, float y1, float r1, float x2, float y2, float r2, float skin, float *xAdj, float *yAdj) {
	float deltax = x2 - x1;
	float deltay = y2 - y1;
	float dist = hypotf(deltax, deltay);
	float penetration_depth = r1 + r2 - dist; // めり込み深さ
	if (penetration_depth <= -skin) {
		return false; // 非接触
	}
	if (xAdj && yAdj) {
		float _cos = (dist > 0) ? deltax / dist : 1;
		float _sin = (dist > 0) ? deltay / dist : 0;
		*xAdj = -penetration_depth * _cos;
		*yAdj = -penetration_depth * _sin;
	}
	return true;
}
int KCollisionMath::collisionCircleWithRect2D(float cx, float cy, float cr, float xBox, float yBox, float xHalf, float yHalf, float skin, float *xAdj, float *yAdj) {
	return collisionCircleWithXShearedRect2D(cx, cy, cr, xBox, yBox, xHalf, yHalf, skin, 0, xAdj, yAdj);
}
int KCollisionMath::collisionCircleWithXShearedRect2D(float cx, float cy, float cr, float xBox, float yBox, float xHalf, float yHalf, float xShear, float skin, float *xAdj, float *yAdj) {
	float x0 = xBox - xHalf; // 左
	float y0 = yBox - yHalf; // 下
	float x1 = xBox + xHalf; // 右
	float y1 = yBox + yHalf; // 上
	// 多角形は時計回りにチェックする。角に重なった場合など複数の偏と衝突する場合があるので必ず全ての面の判定を行う
	float xOld = cx;
	float yOld = cy;
	// 左下からスタート

	// 1 左(X負）
	// 2 上(Y正）
	// 4 右(X正）
	// 8 下(Y負）
	int hit = 0;

	float adjx, adjy;
	if (KCollisionMath::collisionCircleWithSegment2D(cx, cy, cr, x0-xShear, y0, x0+xShear, y1, skin, &adjx, &adjy)) {
		cx += adjx; cy += adjy;
		hit |= 1; // 左側
	}
	if (KCollisionMath::collisionCircleWithSegment2D(cx, cy, cr, x0+xShear, y1, x1+xShear, y1, skin, &adjx, &adjy)) {
		cx += adjx; cy += adjy;
		hit |= 2; // 上側
	}
	if (KCollisionMath::collisionCircleWithSegment2D(cx, cy, cr, x1+xShear, y1, x1-xShear, y0, skin, &adjx, &adjy)) {
		cx += adjx; cy += adjy;
		hit |= 4; // 右側
	}
	if (KCollisionMath::collisionCircleWithSegment2D(cx, cy, cr, x1-xShear, y0, x0-xShear, y0, skin, &adjx, &adjy)) {
		cx += adjx; cy += adjy;
		hit |= 8; // 下側
	}
	if (xAdj) *xAdj = cx - xOld;
	if (yAdj) *yAdj = cy - yOld;
	return hit;
}
// r1, r2 -- radius
// k1, k2 -- penetratin response
bool KCollisionMath::resolveYAxisCylinderCollision(Vec3 *p1, float r1, float k1, Vec3 *p2, float r2, float k2, float skin) {
	float xAdj, zAdj;
	if (collisionCircleWithCircle2D(p1->x, p1->z, r1, p2->x, p2->z, r2, skin, &xAdj, &zAdj)) {
		if (k1 + k2 > 0) {
			p1->x += xAdj * k1;
			p1->z += zAdj * k1;
			p2->x -= xAdj * k2;
			p2->z -= zAdj * k2;
		}
		return true;
	}
	return false;
}



} // namespace
