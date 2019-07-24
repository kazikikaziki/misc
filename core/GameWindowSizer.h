#pragma once
#include "system.h"
#include "KWindow.h"

namespace mo {

class Engine;
class Window;
class InputSystem;

class WindowSizer {
public:
	WindowSizer();
	WindowSizer(int basew, int baseh);

	void init(int basew, int baseh);

	/// ウィンドウに適用可能な最大倍率を返す。
	/// 横方向と縦方向の倍率のうち小さい方を返す。
	/// このメソッドはマルチモニタをサポートするため、
	/// モニタごとに解像度が異なっていても正しく動作する
	float getMaxScale() const;

	/// 現在のウィンドウ倍率を、横方向と縦方向それぞれ返す
	void getCurrentScale(float *horz, float *vert) const;

	/// 倍率を指定してウィンドウサイズを変更する。
	/// 最大化状態だった場合はこれを解除する
	void applyScale(int scale);

	/// ウィンドウを最大化する
	void applyMaximize(bool full);

	/// ウィンドウモードに戻す。
	/// mo::Window::restoreWindow とは異なり、
	/// ボーダレスウィンドウからの復帰も含む
	void restoreWindow();

	/// ウィンドウサイズを逐次切り替えする
	/// zoom 拡大方向に切り替えるなら true、縮小方向に切り替えるなら false
	void resize(bool zoom);

private:
	int basew_;
	int baseh_;
};



/// F11 を押すことで、ウィンドウサイズを x1, x2, x3 ... と切り替えられるようにする
/// Shift + F11 だと逆順でウィンドウサイズが変化する
class WindowResizingSystem: public System, public IWindowCallback {
public:
	WindowResizingSystem();
	void init(Engine *engine);
	bool isWindowed();
	bool isFullscreenTruly();
	bool isFullscreenBorderless();
	void setWindowed();
	void setFullscreenTruly();
	void setFullscreenBorderless();

	virtual void onStart() override;
	virtual void onEnd() override;

	// IWindowCallback
	virtual void onWindowEvent(WindowEvent *e) override;

private:
	WindowSizer sizer_;
	Engine *engine_;
};


}
