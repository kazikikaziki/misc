#include "GameWindowDragging.h"
#include "KLog.h"
#include "KMouse.h"
#include "KImgui.h"
#include "KWindow.h"
#include "Engine.h"

namespace mo {

WindowDraggingSystem::WindowDraggingSystem() {
	engine_ = NULL;
}
void WindowDraggingSystem::init(Engine *engine) {
	Log_trace;
	engine->addSystem(this);
	engine->addWindowCallback(this);
	engine_ = engine;
}
void WindowDraggingSystem::onWindowEvent(WindowEvent *e) {
	switch (e->type) {
	case WindowEvent::MOUSE_MOVE:
		if (canDrag()) {
			// ウィンドウ移動
			int x = engine_->getStatus(Engine::ST_WINDOW_POSITION_X);
			int y = engine_->getStatus(Engine::ST_WINDOW_POSITION_Y);
			x += (int)e->mouse_ev.delta_x;
			y += (int)e->mouse_ev.delta_y;
			engine_->moveWindow(x, y);
		}
		break;
	}
}
void WindowDraggingSystem::onAppFrameUpdate() {
	Log_assert(engine_);
#if 0
	// Mouse position on screen
	int mx, my;
	KMouse::getPosition(&mx, &my);

	Vec3 mouse_pos(mx, my, 0);
	if (canDrag()) {
		// マウス移動量
		Vec3 delta = mouse_pos - last_mouse_pos_;
		// ウィンドウ移動
		int x = engine_->getStatus(Engine::ST_WINDOW_POSITION_X);
		int y = engine_->getStatus(Engine::ST_WINDOW_POSITION_Y);
		x += (int)delta.x;
		y += (int)delta.y;
		engine_->moveWindow(x, y);
	}
	last_mouse_pos_ = mouse_pos;
#endif
}
bool WindowDraggingSystem::canDrag() {
	Log_assert(engine_);

	// フルスクリーンだったらダメ
	if (!engine_->getStatus(Engine::ST_VIDEO_IS_WINDOWED)) return false;

	// 最大化されていたらダメ
	if (engine_->getStatus(Engine::ST_WINDOW_IS_MAXIMIZED)) return false;

	// 左マウスボタンが押されていないとダメ
	if (! KMouse::isButtonDown(KMouse::LEFT)) return false;

	// 修飾キーを伴なっていたらダメ
	if (KKeyboard::getModifiers()) return false;

	// マウスカーソルが ImGUI のコントロール上にあるか、入力中だったらダメ
	if (ImGui::IsAnyWindowHovered()) return false;
	if (ImGui::IsAnyItemHovered()) return false;
	if (ImGui::IsAnyItemActive() || ImGui::IsAnyItemFocused()) return false;

	return true;
}


} // namespace
