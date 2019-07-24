#pragma once

namespace mo {

class KMouse {
public:
	enum Button {
		INVALID = -1,
		LEFT, 
		RIGHT,
		MIDDLE,
		ENUM_MAX
	};
	/// マウスカーソルのスクリーン内での座標を返す
	static void getPosition(int *x, int *y);

	/// マウスボタンが押されているかどうかを調べる
	static bool isButtonDown(Button btn);

	/// マウスホイールの回転位置を得る。
	/// 初期状態では 0 になっている。1ノッチ動かすごとに +1 または -1 だけ変化する
	static int getWheel();

	/// 水平ホイールの回転位置を得る。
	/// 初期状態では 0 になっている。1ノッチ動かすごとに +1 または -1 だけ変化する
	static int getHorzWheel();

	/// ホイールメッセージを受け取るウィンドウを指定する。
	/// 普通は、現在のスレッドのトップウィンドウが自動的に選択されるので、この関数を呼ぶ必要はない。
	/// ホイール値の取得に失敗する場合は、この関数で明示的にウィンドウを指定する。
	/// @param hWnd ウィンドウハンドル(HWND)
	static void setWheelWindow(void *hWnd);

	/// マウスボタンの文字列表現を返す
	/// 名前が未定義の場合は空文字列 "" を返す。（NULLは返さない）
	static const char * getButtonName(Button btn);

	/// 名前を指定してボタンを得る。大小文字は区別しない。
	/// 対応するボタンが存在しない場合は KMouse::INVALID を返す
	static Button findButtonByName(const char *name);
};

}
