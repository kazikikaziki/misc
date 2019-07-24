#ifndef NO_IMGUI
#include "inspector_engine.h"
//
#include "GameHierarchy.h"
#include "GameCamera.h"
#include "Engine.h"
#include "inspector.h"
#include "Screen.h"
#include "KImgui.h"
#include "KNum.h"
#include "KMouse.h"

namespace mo {

CCoreEngineInspector::CCoreEngineInspector() {
	engine_ = NULL;
}
void CCoreEngineInspector::init(Engine *engine){
	engine_ = engine;
}
void CCoreEngineInspector::updatePanel() {
	updateAppPanel();
	updateTimePanel();
	// マウス情報
	if (ImGui::TreeNodeEx(u8"マウス")) {
		updateMousePanel();
		ImGui::TreePop();
	}
	// ウィンドウ設定
	if (ImGui::TreeNodeEx(u8"ウィンドウ")) {
		updateWindowPanel();
		ImGui::TreePop();
	}
}
void CCoreEngineInspector::updateAppPanel() {
	int fpsreq = engine_->getStatus(Engine::ST_FPS_REQUIRED);
	int ac = engine_->getStatus(Engine::ST_FRAME_COUNT_APP);
	int gc = engine_->getStatus(Engine::ST_FRAME_COUNT_GAME);
	ImGui::Text("[App ] %d:%02d:%02d(%d)", ac/fpsreq/60, ac/fpsreq%60, ac%fpsreq, ac);
	if (engine_->isPaused()) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
		ImGui::Text("[Game] %d:%02d:%02d(%d) [PAUSED]", gc/fpsreq/60, gc/fpsreq%60, gc%fpsreq, gc);
		ImGui::PopStyleColor();
	} else {
		ImGui::Text("[Game] %d:%02d:%02d(%d)", gc/fpsreq/60, gc/fpsreq%60, gc%fpsreq, gc);
	}
	//
	int fps_update = engine_->getStatus(Engine::ST_FPS_UPDATE);
	int fps_render = engine_->getStatus(Engine::ST_FPS_RENDER);
	ImGui::Text("GAME FPS: %d (%d)", fps_render, fps_update);

	float guif = ImGui::GetIO().Framerate;
	ImGui::Text("GUI FPS: %.1f (%.3f ms/f)", guif, 1000.0f / guif);
}
void CCoreEngineInspector::updateTimePanel() {
	if (engine_->isPaused()) {
		// 一時停止中
		if (ImGui::Button("Play")) {
			engine_->play();
		}
		ImGui::SameLine();
		if (ImGui_ButtonRepeat("Step")) {
			engine_->playStep();
		}
	} else {
		// 実行中
		if (ImGui::Button("Pause")) {
			engine_->pause();
		}
		ImGui::SameLine();
		if (ImGui_ButtonRepeat("Step")) {
			engine_->pause();
		}
	}
	if (ImGui::TreeNode(u8"フレームスキップ")) {
		int skip = engine_->getStatus(Engine::ST_MAX_SKIP_FRAMES);

		const int N = 5;
		const char *names[N] = {
			"No Skip",
			"Skip1", 
			"Skip2", 
			"Skip3", 
			"Skip4", 
		};
		if (ImGui::Combo("##FrameSkips", &skip, names, N)) {
			engine_->setFps(-1, skip);
		}
		ImGui::TreePop();
	}
}

static Vec3 _WindowToWorldPoint(const Vec3 &point_in_window, const View &view) {
	// 正規化ウィンドウクライアント座標に変換（ウィンドウ左上 0,0 右下 1,1)
	Vec3 cli_nor = g_engine->windowClientToIdentityPoint(point_in_window);

	// カメラ座標に変換
	Vec3 local;
	{
		float w = view.projection_w();
		float h = view.projection_h();
		local.x = Num_lerp(-w / 2, w / 2, Num_normalize(cli_nor.x, -1.0f, 1.0f));
		local.y = Num_lerp(-h / 2, h / 2, Num_normalize(cli_nor.y, -1.0f, 1.0f));
		local.z = cli_nor.z; // <-- この値は point_in_window.z そのまま
	}

	Matrix4 local2world;
	if (view.transform_matrix.computeInverse(&local2world)) {
		return local2world.transform(local);
	}
	else {
		return local; // Camera matrix does not have an inverse
	}
}


void CCoreEngineInspector::updateMousePanel() {
	Screen *screen = engine_->getScreen();

	int mouse_scr_x;
	int mouse_scr_y;
	KMouse::getPosition(&mouse_scr_x, &mouse_scr_y);

	int mouse_cli_x;
	int mouse_cli_y;
	{
		int client_origin_x = engine_->getStatus(Engine::ST_SCREEN_AT_CLIENT_ORIGIN_X);
		int client_origin_y = engine_->getStatus(Engine::ST_SCREEN_AT_CLIENT_ORIGIN_Y);
		mouse_cli_x = mouse_scr_x - client_origin_x;
		mouse_cli_y = mouse_scr_y - client_origin_y;
	}

	{
		RectF viewport = screen->getGameViewportRect();

		// スクリーン座標系
		ImGui::Text("In screen: (%d, %d)", mouse_scr_x, mouse_scr_y);

		// ウィンドウクライアント
		ImGui::Text("In window: (%d, %d)", mouse_cli_x, mouse_cli_y);

		// ビューポート左上を (0, 0) 右下を (w, h) とする座標系
		int mouse_vp_x = (int)(mouse_cli_x - viewport.xmin);
		int mouse_vp_y = (int)(mouse_cli_y - viewport.ymin);
		ImGui::Text("In viewport: (%d, %d)", mouse_vp_x, mouse_vp_y);

		// ビューポート中心を (0, 0) 右下を(w/2, -h/2) とする座標系
		int nx = (int)(mouse_cli_x - viewport.getCenter().x);
		int ny = (int)(mouse_cli_y - viewport.getCenter().y);
		ImGui::Text("In view: (%d, %d)", nx, -ny);
	}
	for (int i=0; i<screen->getViewCount(); i++) {
		const View &view = screen->getView(i);
		Vec3 mouse_point_in_world = _WindowToWorldPoint(Vec3(mouse_cli_x, mouse_cli_y, 0), view);
		ImGui::Text("In world: %.2f, %.2f, %.2f", mouse_point_in_world.x, mouse_point_in_world.y, mouse_point_in_world.z);
	}
}
void CCoreEngineInspector::updateWindowPanel() {
	if (1) {
		int x = engine_->getStatus(Engine::ST_WINDOW_POSITION_X);
		int y = engine_->getStatus(Engine::ST_WINDOW_POSITION_Y);
		ImGui::Text("Pos: %d, %d", x, y);
	}
	if (1) {
		int cw = engine_->getStatus(Engine::ST_WINDOW_CLIENT_SIZE_W);
		int ch = engine_->getStatus(Engine::ST_WINDOW_CLIENT_SIZE_H);
		ImGui::Text("Client size %d x %d", cw, ch);
	}
	if (1) {
		Screen *screen = engine_->getScreen();
		int game_w = screen->getGameResolutionW();
		int game_h = screen->getGameResolutionH();
		if (ImGui::Button("Wnd x1")) {
			engine_->resizeWindow(game_w, game_h);
		}
		ImGui::SameLine();
		if (ImGui::Button("x2")) {
			engine_->resizeWindow(game_w*2, game_h*2);
		}
		ImGui::SameLine();
		if (ImGui::Button("x3")) {
			engine_->resizeWindow(game_w*3, game_h*3);
		}
		ImGui::SameLine();
		if (ImGui::Button("Full")) {
			engine_->setFullscreen();
		}
		ImGui::SameLine();
		if (ImGui::Button("WinFull")) {
			engine_->setWindowedFullscreen();
		}
	}
}

} // namespace

#endif // !NO_IMGUI
