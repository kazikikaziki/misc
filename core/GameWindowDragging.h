#pragma once
#include "system.h"
#include "KVec3.h"
#include "KWindow.h"

namespace mo {

class Engine;

/// ウィンドウを直接ドラッグして移動できるようにする
class WindowDraggingSystem: public System, public IWindowCallback {
public:
	WindowDraggingSystem();
	void init(Engine *engine);
	virtual void onWindowEvent(WindowEvent *e) override;
	virtual void onAppFrameUpdate() override;
private:
	bool canDrag();
	Vec3 last_mouse_pos_;
	Engine *engine_;
};

} // namespace
