#include "GameWindowSizer.h"
#include "Engine.h"
#include "Screen.h"
#include "KLog.h"
#include "KNum.h"

namespace mo {

#pragma region WindowSizer
WindowSizer::WindowSizer() {
	basew_ = 0;
	baseh_ = 0;
}
WindowSizer::WindowSizer(int basew, int baseh) {
	init(basew, baseh);
}
void WindowSizer::init(int basew, int baseh) {
	basew_ = basew;
	baseh_ = baseh;
}
float WindowSizer::getMaxScale() const {
	int max_w = g_engine->getStatus(Engine::ST_SCREEN_W);
	int max_h = g_engine->getStatus(Engine::ST_SCREEN_H);
	float max_scale_w = (float)max_w / basew_;
	float max_scale_h = (float)max_h / baseh_;
	return Num_min(max_scale_w, max_scale_h);
}
void WindowSizer::getCurrentScale(float *horz, float *vert) const {
	int wnd_w = g_engine->getStatus(Engine::ST_WINDOW_CLIENT_SIZE_W);
	int wnd_h = g_engine->getStatus(Engine::ST_WINDOW_CLIENT_SIZE_H);
	if (horz) *horz = (float)wnd_w / basew_;
	if (vert) *vert = (float)wnd_h / baseh_;
}
void WindowSizer::restoreWindow() {
	g_engine->restoreWindow();
}
void WindowSizer::applyScale(int scale) {
	int new_w = basew_ * scale;
	int new_h = baseh_ * scale;
	g_engine->resizeWindow(new_w, new_h);
}
void WindowSizer::applyMaximize(bool full) {
	if (full) {
		g_engine->setWindowedFullscreen();
	} else {
		g_engine->maximizeWindow();
	}
}
void WindowSizer::resize(bool zoom) {
	bool full = g_engine->getStatus(Engine::ST_VIDEO_IS_WINDOWED_FULLSCREEN);

	// （本当の）フルスクリーンモードの場合は切り替え要求を無視する
	if (g_engine->getStatus(Engine::ST_VIDEO_IS_FULLSCREEN)) {
		return;
	}

	// 適用可能な最大ウィンドウ倍率
	int maxscale = (int)getMaxScale();
	
	if (g_engine->getStatus(Engine::ST_WINDOW_IS_MAXIMIZED)) {
		// 現在最大化状態になっている。

		if (zoom) {
			// 拡大方向へ切り替える。
			if (full) {
				// すでに最大化状態なので、等倍に戻る
				applyScale(1);
			} else {
				// なんちゃってフルスクリーン
				applyMaximize(true);
			}
		} else {
			// 縮小方向へ切り替える
			if (full) {
				// なんちゃってフルスクリーン
				applyMaximize(false);
			} else {
				applyScale(maxscale);
			}
		}
	
	} else {
		// 現在通常ウィンドウ状態になっている。
		if (zoom) {
			// 拡大方向へ切り替える。

			// ウィンドウサイズはユーザーによって変更されている可能性がある。
			// 現在の実際のウィンドウサイズを元にして、何倍で表示されているかを求める
			float scalex, scaley;
			getCurrentScale(&scalex, &scaley);

			int newscale;
			if (Num_equals(scalex, scaley, 0.01f)) {
				// アスペクト比が（ほとんど）正しい場合はそのまま1段階上の倍率にする
				newscale = (int)floorf(scalex) + 1;

			} else {
				// ぴったりの倍率になっていない場合は、まず長い方の倍率に合わせる
				newscale = (int)ceilf(Num_max(scalex, scaley));
			}

			// 最大倍率を超えた場合は最大化する
			if (newscale <= maxscale) {
				applyScale(newscale);
			} else {
				applyMaximize(false);
			}

		} else {
			// 縮小方向へ切り替える。

			// ウィンドウサイズはユーザーによって変更されている可能性がある。
			// 現在の実際のウィンドウサイズを元にして、何倍で表示されているかを求める
			float scalex, scaley;
			getCurrentScale(&scalex, &scaley);

			int newscale;
			if (Num_equals(scalex, scaley, 0.01f)) {
				// アスペクト比が（ほとんど）正しい場合はそのまま1段階下の倍率にする
				newscale = (int)ceilf(scalex) - 1;

			} else {
				// ぴったりの倍率になっていない場合は、まず短い方の倍率に合わせる
				newscale = (int)Num_min(scalex, scaley);
			}

			// 等倍を下回った場合は最大化する
			if (newscale >= 1) {
				applyScale(newscale);
			} else {
				applyMaximize(true);
			}
		}
	}
}
#pragma endregion // WindowSizer


#pragma region WindowResizingSystem
WindowResizingSystem::WindowResizingSystem() {
	engine_ = NULL;
}
void WindowResizingSystem::init(Engine *engine) {
	Log_trace;
	engine->addSystem(this);
	engine_ = engine;
}
bool WindowResizingSystem::isFullscreenTruly() {
	return engine_->getStatus(Engine::ST_VIDEO_IS_FULLSCREEN);
}
bool WindowResizingSystem::isFullscreenBorderless() {
	return engine_->getStatus(Engine::ST_VIDEO_IS_WINDOWED_FULLSCREEN);
}
bool WindowResizingSystem::isWindowed() {
	return engine_->getStatus(Engine::ST_VIDEO_IS_WINDOWED);
}
void WindowResizingSystem::setFullscreenTruly() {
	engine_->setFullscreen();
}
void WindowResizingSystem::setFullscreenBorderless() {
	engine_->setWindowedFullscreen();
	sizer_.applyMaximize(true);
}
void WindowResizingSystem::setWindowed() {
	sizer_.applyScale(1);
}

void WindowResizingSystem::onStart() {
	Log_assert(engine_);

	Screen *screen = engine_->getScreen();
	Log_assert(screen);

	int basew = screen->getGameResolutionW();
	int baseh = screen->getGameResolutionH();
	sizer_.init(basew, baseh);

	engine_->addWindowCallback(this);
}
void WindowResizingSystem::onEnd() {
	engine_->removeWindowCallback(this);
}
void WindowResizingSystem::onWindowEvent(WindowEvent *e) {
	switch (e->type) {
	case WindowEvent::KEY_DOWN:
		if (e->key_ev.key == KKeyboard::F11) {
			if (e->key_ev.withShift()) {
				// Shift + F11
				sizer_.resize(false); // 縮小

			} else {
				// F11
				sizer_.resize(true); // 拡大
	
			}
		}
		break;
	}
}
#pragma endregion // WindowResizingSystem


} // namespace
