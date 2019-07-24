#pragma once
#include "KVec3.h"
#include <vector>

namespace mo {

/// 3次のベジェ曲線を扱う
///
/// 3次ベジェ曲線は、4個の制御点から成る。
/// 制御点0 : 始点 (= アンカー)
/// 制御点1 : 始点側ハンドル
/// 制御点2 : 終点側ハンドル
/// 制御点3 : 終点
/// <pre>
/// handle            handle
///  (1)                (2)
///   |    segment       |
///   |  +------------+  |
///   | /              \ |
///   |/                \|
///  [0]                [3]
/// anchor             anchor
/// </pre>
/// セグメントを連続してつなげていき、1本の連続する曲線を表す。
/// そのため、セグメント接続部分での座標と傾きは一致している必要がある
/// 次の図で言うと、セグメントAの終端点[3]と、セグメントBの始点[0]は同一座標に無いといけない。
/// また、滑らかにつながるには、セグメントAの終端傾き (2)→[3] とセグメントBの始点傾き [0]→(1)
/// が一致していないといけない
/// <pre>
///  (1)                (2)
///   |    segment A     |
///   |  +------------+  |
///   | /              \ |
///   |/                \|
///  [0]                [3]
///                     [0]                [3]
///                      |\                /|
///                      | \              / |
///                      |  +------------+  |
///                     (1)    segment B   (2)
/// </pre>
/// @see https://ja.javascript.info/bezier-curve
/// @see https://postd.cc/bezier-curves/
class KCubicBezier {
public:
	KCubicBezier();

	bool empty() const;

	/// 区間インデックス seg における３次ベジェ曲線の、係数 t における座標を得る。
	/// 成功した場合は pos に座標をセットして true を返す。
	/// 範囲外のインデックスを指定した場合は何もせずに false を返す
	/// @param seg   区間インデックス
	/// @param t     区間内の位置を表す値。区間を 0.0 以上 1.0 以下で正規化したもの
	/// @param pos[out] 曲線上の座標
	bool getCoordinates(int seg, float t, Vec3 *pos) const;
	Vec3 getCoordinates(int seg, float t) const;

	/// 区間インデックス seg における３次ベジェ曲線の、係数 t における傾きを得る。
	/// 成功した場合は tangent に傾きをセットして true を返す。
	/// 範囲外のインデックスを指定した場合は何もせずに false を返す
	/// @param seg   区間インデックス
	/// @param t     区間内の位置を表す値。区間を 0.0 以上 1.0 以下で正規化したもの
	/// @param tangent[out] 傾きの値
	bool getTangent(int seg, float t, Vec3 *tangent) const;

	/// getTangent(int, float, Vec3*) と同じだが、傾きを直接返す
	Vec3 getTangent(int seg, float t) const;

	/// 区間インデックス seg における３次ベジェ曲線の長さを得る
	/// @param seg 区間インデックス
	float getLength(int seg) const;
	
	/// テスト用。曲線を numdiv 個の直線に分割して近似長さを求める
	float getLength_Test1(int seg, int numdiv) const;
	
	/// テスト用。曲線を等価なベジェ曲線に再帰分割して近似長さを求める
	float getLength_Test2(int seg) const;

	/// 全体の長さを返す
	float getWholeLength() const;

	/// 区間数を返す
	int getSegmentCount() const;
	
	/// 制御点の数を変更する
	void setSegmentCount(int count);

	/// 区間を追加する。
	/// １区間には４個の制御点が必要になる。
	/// @param a 始点アンカー
	/// @param b 始点ハンドル。始点での傾きを決定する
	/// @param c 終点ハンドル。終点での傾きを決定する
	/// @param d 終点アンカー
	void addSegment(const Vec3 &a, const Vec3 &b, const Vec3 &c, const Vec3 &d);

	/// 区間インデックス seg の index 番目にある制御点の座標を設定する
	/// @param seg    区間番号。0 以上 getSegmentCount() 未満
	/// @param point  制御点番号。制御点は4個で、始点から順番に 0, 1, 2, 3
	void setPoint(int seg, int point, const Vec3 &pos);

	/// コントロールポイントの通し番号を指定して座標を指定する。
	/// 例えば 0 は区間[0]の始点を、4は区間[1]の始点を、15は区間[3]の終点を表す
	void setPoint(int serial_index, const Vec3 &pos);

	/// 区間インデックス seg の index 番目にある制御点の座標を返す
	/// @param seg    区間番号。0 以上 getSegmentCount() 未満
	/// @param point  制御点番号。制御点は4個で、始点から順番に 0, 1, 2, 3
	Vec3 getPoint(int seg, int point) const;

	/// コントロールポイントの通し番号を指定して座標を取得する
	Vec3 getPoint(int serial_index) const;

	/// 区間インデックス seg における３次ベジェ曲線の、
	/// 始点側のアンカーとハンドルポイントを得る
	bool getFirstAnchor(int seg, Vec3 *anchor, Vec3 *handle) const;

	/// 区間インデックス seg における３次ベジェ曲線の、
	/// 終点側のアンカーとハンドルポイントを得る
	bool getSecondAnchor(int seg, Vec3 *handle, Vec3 *anchor) const;

	void setFirstAnchor(int seg, const Vec3 &anchor, const Vec3 &handle);
	void setSecondAnchor(int seg, const Vec3 &handle, const Vec3 &anchor);

private:
	std::vector<Vec3> points_; /// 制御点の配列
	mutable float length_; /// 曲線の始点から終点までの経路長さ（ループしている場合は一周の長さ）
	mutable bool dirty_length_;
};


} // namespace
