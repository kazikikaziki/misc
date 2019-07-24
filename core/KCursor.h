#pragma once
#include <inttypes.h>

namespace mo {

class KCursor {
public:
	enum Type {
		DEFAULT,
		CROSS,
		HAND,
		HELP,
		IBEAM,
		NO,
		WAIT,
		SIZE_ALL,
		SIZE_NESW,
		SIZE_NWSE,
		SIZE_NS,
		SIZE_WE,
		UP,
		ENUM_MAX
	};

	KCursor();

	/// システムからカーソルをロードする
	/// @param tp ロードするカーソル形状の種類
	void loadFromSystem(Type tp);

	/// 画像データからカーソルをロードする
	/// @param pixels 画像データ。RGBA32ビットフォーマットで入力する
	/// @param w 画像幅
	/// @param h 画像高さ
	/// @param hotspot_x ホットスポット（クリック位置）の X 座標
	/// @param hotspot_Y ホットスポット（クリック位置）の Y 座標
	void loadFromPixels(const uint8_t *pixels, int w, int h, int hotspot_x, int hotspot_y);

	/// 内部で保持しているカーソル識別子を返す。
	/// Windows なら HCURSOR の値になる
	void * getHandle() const; 

private:
	void * hCur_;
};

}