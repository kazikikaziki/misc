#pragma once
#include <stdio.h> // NULL

namespace mo {

class Vec3;

struct CIRCLE2D {
	float x;
	float y;
	float r;
};
struct RECT2D_XSHEAR {
	float x;
	float y;
	float rx;
	float ry;
	float shear;
};

class KCollisionMath {
public:
	/// 三角形の法線ベクトルを返す
	///
	/// 三角形法線ベクトル（正規化済み）を得る
	/// 時計回りに並んでいるほうを表とする
	static bool getTriangleNormal(Vec3 *result, const Vec3 &o, const Vec3 &a, const Vec3 &b);

	static bool isAabbIntersected(const Vec3 &pos1, const Vec3 &halfsize1, const Vec3 &pos2, const Vec3 &halfsize2, Vec3 *intersect_center=NULL, Vec3 *intersect_halfsize=NULL);

	/// 直線 ab で b 方向を見たとき、点(px, py)が左にあるなら正の値を、右にあるなら負の値を返す
	static float getPointInLeft2D(float px, float py, float ax, float ay, float bx, float by);

	/// 直線と点の有向距離（点AからBに向かって左側が負、右側が正）
	static float getSignedDistanceOfLinePoint2D(float px, float py, float ax, float ay, float bx, float by);

	/// 点 p から直線 ab へ垂線を引いたときの、垂線と直線の交点 h を求める。
	/// 戻り値は座標ではなく、点 a b に対する点 h の内分比を返す（点 h は必ず直線 ab 上にある）。
	/// 戻り値を k としたとき、交点座標は b * k + a * (1 - k) で求まる。
	/// 戻り値が 0 未満ならば交点は a の外側にあり、0 以上 1 未満ならば交点は ab 間にあり、1 以上ならば交点は b の外側にある。
	static float getPerpendicularLinePoint(const Vec3 &p, const Vec3 &a, const Vec3 &b);
	static float getPerpendicularLinePoint2D(float px, float py, float ax, float ay, float bx, float by);

	/// 点が矩形の内部であれば true を返す
	static bool isPointInRect2D(float px, float py, float cx, float cy, float xHalf, float yHalf);

	/// 点が剪断矩形（X方向）の内部であれば true を返す
	static bool isPointInXShearedRect2D(float px, float py, float cx, float cy, float xHalf, float yHalf, float xShear);

	/// 円が点に衝突しているか確認し、衝突している場合は円の補正移動量を (xAdj, yAdj) にセットして true を返す。
	/// 衝突していない場合は false を返す。
	/// 衝突判定には、長さ skin だけの余裕を持たせることができる（厳密に触れていなくても、skin 以内の距離ならば接触しているとみなす）
	/// 衝突を解消するためには、円を (xAdj, yAdj) だけ移動させるか、点を (-xAdj, -yAdj) だけ移動させる。
	static bool collisionCircleWithPoint2D(float cx, float cy, float cr, float px, float py, float skin=1.0f, float *xAdj=NULL, float *yAdj=NULL);

	/// 円が直線に衝突しているか確認し、衝突している場合は、円を直線の左側に押し出すための移動量を (xAdj, yAdj) にセットして true を返す。
	/// 衝突していない場合は false を返す。
	/// 衝突判定には、長さ skin だけの余裕を持たせることができる（厳密に触れていなくても、skin 以内の距離ならば接触しているとみなす）
	/// 衝突を解消するためには、円を (xAdj, yAdj) だけ移動させるか、直線を (-xAdj, -yAdj) だけ移動させる。
	static bool collisionCircleWithLine2D(float cx, float cy, float cr, float x0, float y0, float x1, float y1, float skin=1.0f, float *xAdj=NULL, float *yAdj=NULL);
	
	/// collisionCircleWithLine2D と似ているが、円が直線よりも右側にある場合は常に接触しているとみなし、直線の左側に押し出す
	static bool collisionCircleWithLinearArea2D(float cx, float cy, float cr, float x0, float y0, float x1, float y1, float skin=1.0f, float *xAdj=NULL, float *yAdj=NULL);

	/// 円が線分に衝突しているか確認し、衝突している場合は円の補正移動量を (xAdj, yAdj) にセットして true を返す。
	/// 衝突していない場合は false を返す。
	/// 衝突判定には、長さ skin だけの余裕を持たせることができる（厳密に触れていなくても、skin 以内の距離ならば接触しているとみなす）
	/// 衝突を解消するためには、円を (xAdj, yAdj) だけ移動させるか、直線を (-xAdj, -yAdj) だけ移動させる。
	static bool collisionCircleWithSegment2D(float cx, float cy, float cr, float x0, float y0, float x1, float y1, float skin=1.0f, float *xAdj=NULL, float *yAdj=NULL);

	/// 円同士が衝突しているか確認し、衝突している場合は円の補正移動量を (xAdj, yAdj) にセットして true を返す。
	/// 衝突していない場合は false を返す。
	/// 衝突判定には、長さ skin だけの余裕を持たせることができる（厳密に触れていなくても、skin 以内の距離ならば接触しているとみなす）
	/// 衝突を解消するためには、円1を (xAdj, yAdj) だけ移動させるか、円2を (-xAdj, -yAdj) だけ移動させる。
	/// 双方の円を平均的に動かして解消するなら、円1を (xAdj/2, yAdj/2) 動かし、円2を(-xAdj/2, -yAdj/2) だけ動かす
	static bool collisionCircleWithCircle2D(float x1, float y1, float r1, float x2, float y2, float r2, float skin=1.0f, float *xAdj=NULL, float *yAdj=NULL);

	/// 円と矩形が衝突しているなら、矩形のどの辺と衝突したかの値をフラグ組汗で返す
	/// 1 左(X負）
	/// 2 上(Y正）
	/// 4 右(X正）
	/// 8 下(Y負）
	static int collisionCircleWithRect2D(float cx, float cy, float cr, float xBox, float yBox, float xHalf, float yHalf, float skin=1.0f, float *xAdj=NULL, float *yAdj=NULL);
	static int collisionCircleWithXShearedRect2D(float cx, float cy, float cr, float xBox, float yBox, float xHalf, float yHalf, float xShear, float skin=1.0f, float *xAdj=NULL, float *yAdj=NULL);

	/// p1 を通り Y 軸に平行な半径 r1 の円柱と、
	/// p2 を通り Y 軸に平行な半径 r2 の円柱の衝突判定と移動を行う。
	/// 衝突しているなら p1 と p2 の双方を X-Z 平面上で移動させ、移動後の座標を p1 と p2 にセットして true を返す
	/// 衝突していなければ何もせずに false を返す
	/// k1, k2 には衝突した場合のめり込み解決感度を指定する。
	///     0.0 を指定すると全く移動しない。1.0 だと絶対にめり込まず、硬い感触になる
	///     0.2 だとめり込み量を 20% だけ解決する。めり込みの解決に時間がかかり、やわらかい印象になる。
	///     0.8 だとめり込み量を 80% だけ解決する。めり込みの解決が早く、硬めの印象になる
	/// skin 衝突判定用の許容距離を指定する。0.0 だと振動しやすくなる
	static bool resolveYAxisCylinderCollision(Vec3 *p1, float r1, float k1, Vec3 *p2, float r2, float k2, float skin);
};

} // namespace
