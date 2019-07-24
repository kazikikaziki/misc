#pragma once
#include <inttypes.h>

namespace mo {

class KIcon {
public:
	KIcon();

	/// アプリケーションに埋め込まれたリソースからカーソルをロードする
	/// @param index ロードするアイコンの番号。リソースを先頭から検索し、index番目に見つかったアイコンをロードする
	/// @attention index はリソースの順序値ではなく「何番目のアイコンか」を示す
	bool loadFromResource(int index);

	/// 画像データからアイコンをロードする
	/// @param pixels 画像データ。RGBA32ビットフォーマットで入力する
	/// @param w 画像幅
	/// @param h 画像高さ
	bool loadFromPixels(const uint8_t *pixels, int w, int h);

	/// 内部で保持しているアイコン識別子を返す。
	/// Windows なら HICON の値になる
	void * getHandle() const;

private:
	void * hIcon_;
};

}
