#pragma once
#include "KVec2.h"

namespace mo {

class RectI {
public:
	int left, top, right, bottom;

	RectI() {
		left = top = right = bottom = 0;
	}
	RectI(int l, int t, int r, int b) {
		left   = l;
		top    = t;
		right  = r;
		bottom = b;
	}
};

class RectF {
public:
	RectF();
	RectF(const RectF &rect);
	RectF(int _xmin, int _ymin, int _xmax, int _ymax);
	RectF(float _xmin, float _ymin, float _xmax, float _ymax);
	RectF(const Vec2 &pmin, const Vec2 &pmax);
	RectF getOffsetRect(const Vec2 &p) const;
	RectF getOffsetRect(float dx, float dy) const;

	/// 外側に dx, dy だけ広げた矩形を返す（負の値を指定した場合は内側に狭める）
	RectF getInflatedRect(float dx, float dy) const;

	/// rc と交差しているか判定する
	bool isIntersectedWith(const RectF &rc) const;

	/// rc と交差しているならその範囲を返す
	RectF getIntersectRect(const RectF &rc) const;
	RectF getUnionRect(const Vec2 &p) const;
	RectF getUnionRect(const RectF &rc) const;
	bool isZeroSized() const;
	bool isEqual(const RectF &rect) const;
	bool contains(const Vec2 &p) const;
	bool contains(float x, float y) const;
	bool contains(int x, int y) const;
	Vec2 getCenter() const;
	Vec2 getMinPoint() const;
	Vec2 getMaxPoint() const;
	float getSizeX() const;
	float getSizeY() const;

	/// 外側に向けて dx, dy だけ膨らませる
	void inflate(float dx, float dy);

	/// rect 全体を dx, dy だけ移動する
	void offset(float dx, float dy);
	void offset(int dx, int dy);

	/// rect を含むように矩形を拡張する
	/// unionX X方向について拡張する
	/// unionY Y方向について拡張する
	void unionWith(const RectF &rect, bool unionX=true, bool unionY=true);

	/// 数直線上で座標 x を含むように xmin, xmax を拡張する
	void unionWithX(float x);

	/// 数直線上で座標 y を含むように ymin, ymax を拡張する
	void unionWithY(float y);

	/// xmin <= xmax かつ ymin <= ymax であることを保証する
	void sort();
	//
	float xmin;
	float ymin;
	float xmax;
	float ymax;
};

}