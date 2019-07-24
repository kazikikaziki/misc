#ifndef NO_IMGUI
#include "Engine.h"
#include "VideoBank.h"
#include "inspector.h"
#include "GameInput.h"
#include "GameRender.h"
#include "GameAudio.h"
#include "GameHierarchy.h"
#include "KFile.h"
#include "KPlatform.h"
#include "asset_path.h"
#include "Screen.h"
#include "KImgui.h"
#include "KNum.h"
#include "KStd.h"
#include "KLog.h"
#include "KVideo.h"
#include <algorithm> // std::sort

namespace mo {
#pragma region Inspector

static const int DEFAULT_PANE_WIDTH = 260;
static const int MIN_PANE_SIZE = 32;

struct DrawCallbackData {
	TEXID tex;
	int w, h;
};
static DrawCallbackData s_drawcallbackdata;

static void mo__guiSample() {
	static bool show_test_window = true;
	ImGui::ShowDemoWindow(&show_test_window);
}

static const char * s_primitive_names[Primitive_EnumMax] = {
	"PointList",     // Primitive_PointList
	"LineList",      // Primitive_LineList
	"LineStrip",     // Primitive_LineStrip
	"TriangleList",  // Primitive_TriangleList
	"TriangleStrip", // Primitive_TriangleStrip
	"TriangleFan",   // Primitive_TriangleFan
};
const char * _GetPrimitiveName(Primitive p) {
	Log_assert(0 <= p && p < Primitive_EnumMax);
	return s_primitive_names[p];
}




#pragma region Inspector
Inspector::Inspector() {
	engine_ = NULL;
	highlight_selections_enabled_ = true;
	visibled_ = false;
	demo_ = false;
	overwrap_window_ = true;
	highlighted_entity_ = NULL;
	system_pane_w_ = 0;
	entity_pane_w_ = 0;
	font_index_ = 0;
}
Inspector::~Inspector() {
}
void Inspector::init(Engine *engine) {
	engine_ = engine;
	highlight_selections_enabled_ = true;
	visibled_ = false;
	demo_ = false;
	overwrap_window_ = true;
	highlighted_entity_ = NULL;
	system_pane_w_ = DEFAULT_PANE_WIDTH;
	entity_pane_w_ = DEFAULT_PANE_WIDTH;
	core_engine_inspector_.init(engine_);
	engine_system_inspector_.init(engine_);
	entity_inspector_.init(engine_);
	status_visible_ = false;
	font_index_ = 0;
}
void Inspector::shutdown() {
	engine_ = NULL;
}
void Inspector::setFont(int font_index) {
	font_index_ = font_index;
}
void Inspector::render(TEXID game_display) {
	ImGui_PushFont(font_index_);
	guiMain(game_display);
	ImGui_PopFont();
}
void Inspector::guiFontConfig() {
	ImGui::Text("Font");
	ImGui::Indent();
	ImFontAtlas *fontatlas = ImGui::GetIO().Fonts;
	for (int i=0; i<fontatlas->Fonts.size(); i++) {
		if (ImGui::Button(fontatlas->ConfigData[i].Name)) {
			font_index_ = i;
		}
	}
	ImGui::Unindent();
}
void Inspector::setHighlightedEntity(EID e) {
	highlighted_entity_ = e;
}
bool Inspector::isEntityHighlighted(EID e, bool in_hierarchy) const {
	Log_assert(engine_);
	if (e == NULL) return false;
	if (in_hierarchy) {
		// エンティティ自身または親がハイライトされていればＯＫ
		HierarchySystem *hs = engine_->findSystem<HierarchySystem>();
		while (e) {
			if (e == highlighted_entity_) return true;
			e = hs->getParent(e);
		}
		return false;
		
	} else {
		// エンティティ自身がハイライトされてるならＯＫ
		return (e == highlighted_entity_);
	}
}
void Inspector::selectEntity(EID e) {
	entity_inspector_.select(e);
}
void Inspector::unselectAllEntities() {
	entity_inspector_.unselectAll();
}
void Inspector::setVisible(bool value) {
	Log_assert(engine_);
	visibled_ = value;
	if (visibled_) {
		if (getSelectedEntityCount() == 0) {
			auto mgr = engine_->findSystem<HierarchySystem>();
			EID player_e = mgr->findEntity("Player");
			selectEntity(player_e);
		}
	}
}
bool Inspector::isVisible() const {
	return visibled_;
}
bool Inspector::isEntityVisible(EID e) const {
	return entity_inspector_.isVisible(e);
}
int Inspector::getSelectedEntityCount() const {
	return entity_inspector_.getSelectedCount();
}
EID Inspector::getSelectedEntity(int index) {
	return entity_inspector_.getSelected(index);
}
bool Inspector::isEntitySelected(EID e) const {
	return entity_inspector_.isSelected(e);
}
bool Inspector::isEntitySelectedInHierarchy(EID e) const {
	return entity_inspector_.isSelectedInHierarchy(e);
}
bool Inspector::isStatusTipVisible() const {
	return status_visible_;
}
void Inspector::setStatusTipVisible(bool visible) {
	status_visible_ = visible;
}
bool Inspector::isOverwrappingWindowMode() const {
	return overwrap_window_;
}
void Inspector::setOverwrappingWindowMode(bool value) {
	overwrap_window_ = value;
}
bool Inspector::isSystemInspectorVisible(System *s) {
	return engine_system_inspector_.getInfo(s)->is_open;
}
bool Inspector::isHighlightSelectionsEnabled() const {
	return highlight_selections_enabled_;
}
bool Inspector::entityShouldRednerAsSelected(EID e) const {
	bool a = isEntitySelected(e);
	bool b = isEntityHighlighted(e, true);
	return a || b;
}
void Inspector::guiScreenMenu() {
	Screen *screen = engine_->getScreen();
	{
		bool b = screen->isRenderTargetEnable();
		if (ImGui::Checkbox(u8"レンダーテクスチャを経由して描画", &b)) {
			screen->setRenderTargetEnable(b);
		}
	}
	if (! engine_->getScreen()->use_render_texture_) {
		// ゲーム画面用のレンダーターゲットが生成されていないため、ペインには描画できない
		ImGui::PushStyleColor(ImGuiCol_Text, GuiColor_DeactiveText);
		overwrap_window_ = true;
		ImGui::Checkbox(u8"インスペクターをゲーム画面に重ねる", &overwrap_window_); // 常にtrue
		ImGui::PopStyleColor();
	} else {
		ImGui::Checkbox(u8"インスペクターをゲーム画面に重ねる", &overwrap_window_);
	}
	if (1) {
		ImGui::Checkbox(u8"選択オブジェクトを強調表示する", &highlight_selections_enabled_);
	}
}
void Inspector::guiSystemBrowser() {
	Log_assert(engine_);
	// ゲームエンジンの基本情報
	if (ImGui::CollapsingHeader(u8"一般", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Indent();
		core_engine_inspector_.updatePanel();
		// スクリーン設定
		if (ImGui::TreeNodeEx(u8"表示と描画")) {
			guiScreenMenu();
			ImGui::TreePop();
		}
		guiInspectorPreference();
		ImGui::Unindent();
	}
	// システム
	engine_system_inspector_.updatePanel();

	ImGui::Separator();
	if (ImGui::CollapsingHeader(u8"アセット")) {
		engine_->videobank()->updateInspector();
	}
}
RectF Inspector::getWorldViewport() const {
	return world_viewport_in_window_;
}
bool Inspector::getWorldViewSize(int *w, int *h, TEXID game_tex) const {
	Log_assert(w);
	Log_assert(h);
	Log_assert(engine_);
	if (! visibled_) return false;
	if (game_tex == NULL) return false;
	ImVec2 pane_size = ImGui::GetContentRegionAvail();
	// 元画像サイズ
	int si;
	float sf;
	TexDesc desc;
	Video::getTextureDesc(game_tex, &desc);
	int tex_w = desc.w;
	int tex_h = desc.h;
	switch (2) {
	case 0:
		// 常に原寸
		*w = tex_w;
		*h = tex_h;
		break;
	case 1:
		// 整数倍かつ最大サイズ
		si = (int)Num_min(pane_size.x / tex_w, pane_size.y / tex_h);
		si = (int)Num_max(1, si);
		*w = tex_w * si;
		*h = tex_h * si;
		break;
	case 2:
	default:
		if (engine_->getScreen()->isKeepAspect()) {
			// アスペクト比を保ちながら最大サイズ
			sf = Num_min((float)pane_size.x / tex_w, (float)pane_size.y / tex_h);
			*w = (int)(tex_w * sf);
			*h = (int)(tex_h * sf);
		} else {
			// FIT
			*w = (int)pane_size.x;
			*h = (int)pane_size.y;
		}
		break;
	}
	return true;
}
void Inspector::guiWorldBrowser(TEXID game_tex) {
	if (game_tex) {
		int w, h;
		getWorldViewSize(&w, &h, game_tex);
		ImVec2 p = ImGui::GetCursorScreenPos();
		world_viewport_in_window_.xmin = p.x;
		world_viewport_in_window_.ymin = p.y;
		world_viewport_in_window_.xmax = world_viewport_in_window_.xmin + w;
		world_viewport_in_window_.ymax = world_viewport_in_window_.ymin + h;
		ImGui_Image(game_tex, ImVec2i(w, h));
	}
}
void Inspector::guiStatusTip() {
	Log_assert(engine_);
	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
	ImGui::Begin("##window", NULL, ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings);
	auto color = engine_->isPaused() ? ImColor(1.0f, 0.0f, 0.0f, 1.0f) : ImColor(1.0f, 1.0f, 1.0f, 1.0f);
	int fpsreq = engine_->getStatus(Engine::ST_FPS_REQUIRED);
	int ac = engine_->getStatus(Engine::ST_FRAME_COUNT_APP);
	int gc = engine_->getStatus(Engine::ST_FRAME_COUNT_GAME);
	ImGui::TextColored(color, "Game %d:%02d.%02d", gc/fpsreq/60, gc/fpsreq%60, gc%fpsreq);// ImGui::SameLine();
	ImGui::Text("Time  %d:%02d.%02d", ac/fpsreq/60, ac/fpsreq%60, ac%fpsreq);// ImGui::SameLine();
	ImGui::Text("FPS  %.1f (GUI)", ImGui::GetIO().Framerate);
	//
	int fps_update = engine_->getStatus(Engine::ST_FPS_UPDATE);
	int fps_render = engine_->getStatus(Engine::ST_FPS_RENDER);
	ImGui::Text("FPS  %d/%d (Game render/update)", fps_render, fps_update);
	//
	ImGui::End();
}
void Inspector::guiMain(TEXID game_tex) {
	Log_assert(engine_);
	if (! visibled_) {
		if (status_visible_) {
			guiStatusTip();
		}
		return;
	}
	int wnd_padding_x = (int)ImGui::GetStyle().WindowPadding.x;
	int wnd_padding_y = (int)ImGui::GetStyle().WindowPadding.y;
	int item_spacing_x = (int)ImGui::GetStyle().ItemSpacing.x;
	ImGui::GetStyle().FrameBorderSize = 1.0f;

	const int splitter_thickness = 6;
	if (! overwrap_window_) {
		// ImGuiウィンドウサイズを、常にクライアント領域いっぱいになるようにする
		int window_w = engine_->getStatus(Engine::ST_WINDOW_CLIENT_SIZE_W);
		int window_h = engine_->getStatus(Engine::ST_WINDOW_CLIENT_SIZE_H);
		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2i(window_w, window_h), ImGuiCond_Always);
		//
		ImGui::Begin("##window", NULL, ImVec2i(window_w, window_h), -1.0f, /*ImGuiWindowFlags_ShowBorders|*/ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize);
		ImVec2 maxsize = ImGui::GetContentRegionMax();
		// 中央ペインのサイズは、左右ペインのサイズから自動的に決まる
		int world_pane_w = (int)maxsize.x - system_pane_w_ - entity_pane_w_ - splitter_thickness * 2 - item_spacing_x * 4;
		//
		{
			// 左ペイン
			ImGui::BeginChild("##systems", ImVec2i(system_pane_w_, 0));
			guiSystemBrowser();
			ImGui::EndChild();
		}
		ImGui::SameLine();
		{
			// 左スプリッター
			ImGui::Button("##vsplitter1", ImVec2i(splitter_thickness, (int)(maxsize.y-wnd_padding_y)));
			if (ImGui::IsItemActive()) {
				system_pane_w_ += (int)ImGui::GetIO().MouseDelta.x;
				if (system_pane_w_ < MIN_PANE_SIZE) system_pane_w_ = MIN_PANE_SIZE;
			}
			// マウスカーソル形状を設定する
			if (ImGui::IsItemHovered()) {
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			}
		}
		ImGui::SameLine();
		{
			// 中央ペイン
			ImGui::BeginChild("##view", ImVec2i(world_pane_w, 0));
			guiWorldBrowser(game_tex);
			ImGui::EndChild();
		}
		ImGui::SameLine();
		{
			// 右スプリッター
			ImGui::Button("##vsplitter2", ImVec2i(splitter_thickness, (int)(maxsize.y-wnd_padding_y)));
			if (ImGui::IsItemActive()) {
				entity_pane_w_ -= (int)ImGui::GetIO().MouseDelta.x;
				if (entity_pane_w_ < MIN_PANE_SIZE) entity_pane_w_ = MIN_PANE_SIZE;
			}
			// マウスカーソル形状を設定する
			if (ImGui::IsItemHovered()) {
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			}
		}
		ImGui::SameLine();
		{
			// 右ペイン
			ImGui::BeginChild("##ent", ImVec2i(entity_pane_w_, 0));
			entity_inspector_.guiEntityBrowser();
			ImGui::EndChild();
		}
		ImGui::End();

	} else {
		int window_w = engine_->getStatus(Engine::ST_WINDOW_CLIENT_SIZE_W);
		int window_h = engine_->getStatus(Engine::ST_WINDOW_CLIENT_SIZE_H);
		//
		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
		int lwidth = 
			wnd_padding_x + 
			system_pane_w_ + 
			item_spacing_x +
			splitter_thickness + 
			wnd_padding_x;
		ImGui::SetNextWindowSize(ImVec2i(lwidth, window_h), ImGuiCond_Always);
		ImGui::Begin("##window1", NULL, ImGuiWindowFlags_NoScrollbar|/*ImGuiWindowFlags_ShowBorders|*/ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize);
		{
			// 左ペイン
			ImGui::BeginChild("##systems", ImVec2i(system_pane_w_, 0));
			guiSystemBrowser();
			ImGui::EndChild();
		}
		ImGui::SameLine();
		if (1){
			// 左スプリッター
			ImVec2 maxsize = ImGui::GetContentRegionMax();
			ImGui::Button("##vsplitter1", ImVec2i(splitter_thickness, (int)(maxsize.y-wnd_padding_y*2)));
			if (ImGui::IsItemActive()) {
				system_pane_w_ += (int)ImGui::GetIO().MouseDelta.x;
				if (system_pane_w_ < MIN_PANE_SIZE) system_pane_w_ = MIN_PANE_SIZE;
			}
			// マウスカーソル形状を設定する
			if (ImGui::IsItemHovered()) {
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			}
		}
		ImGui::End();

		int rwidth =
			wnd_padding_x + 
			splitter_thickness + 
			item_spacing_x +
			entity_pane_w_ +
			wnd_padding_x;
		ImGui::SetNextWindowPos(ImVec2i(window_w - rwidth, 0), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2i(rwidth, window_h), ImGuiCond_Always);
		ImGui::Begin("##window2", NULL, ImGuiWindowFlags_NoScrollbar|/*ImGuiWindowFlags_ShowBorders|*/ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize);
		ImVec2 maxsize = ImGui::GetContentRegionMax();
		{
			// 右スプリッター
			ImGui::Button("##vsplitter2", ImVec2i(splitter_thickness, (int)(maxsize.y-wnd_padding_y)));
			if (ImGui::IsItemActive()) {
				entity_pane_w_ -= (int)ImGui::GetIO().MouseDelta.x;
				if (entity_pane_w_ < MIN_PANE_SIZE) entity_pane_w_ = MIN_PANE_SIZE;
			}
			// マウスカーソル形状を設定する
			if (ImGui::IsItemHovered()) {
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			}
		}
		ImGui::SameLine();
		{
			// 右ペイン
			ImGui::BeginChild("##ent", ImVec2i(entity_pane_w_, 0));
			entity_inspector_.guiEntityBrowser();
			ImGui::EndChild();
		}
		ImGui::End();
	}

	if (demo_) {
		mo__guiSample();
	}
}
void Inspector::guiInspectorPreference() {
	Log_assert(engine_);
	if (ImGui::TreeNode(u8"ログ")) {
		{
			bool b = KLog::command("level_info");
			if (ImGui::Checkbox(u8"情報", &b)) {
				KLog::setLevelInfo(b);
			}
		}
		{
			bool b = KLog::command("level_debug");
			if (ImGui::Checkbox(u8"デバッグ", &b)) {
				KLog::setLevelDebug(b);
			}
		}
		{
			bool b = KLog::command("level_verbose");
			if (ImGui::Checkbox(u8"詳細", &b)) {
				KLog::setLevelVerbose(b);
			}
		}
		{
			bool b = KLog::command("output_console");
			if (ImGui::Checkbox(u8"コンソール", &b)) {
				KLog::setOutputConsole(b);
			}
		}
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Font/ImGUI/Color")) {
		guiFontConfig();
		ImGui::Checkbox("ImGui Demo", &demo_);
		if (ImGui::Button("BG0")) { ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = GuiColor_Window; } ImGui::SameLine();
		if (ImGui::Button("BG1")) { ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = ImVec4(0.7f, 0.0f, 0.0f, 1.0f); } ImGui::SameLine();
		if (ImGui::Button("BG2")) { ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.7f, 0.0f, 1.0f); } ImGui::SameLine();
		if (ImGui::Button("BG3")) { ImGui::GetStyle().Colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.7f, 1.0f); }
		ImGui::TreePop();
	}
}
#pragma endregion // Inspector




#pragma region Inspector funcs
void TextureBank_onInspectorGui(VideoBank *video_bank) {
#ifndef NO_IMGUI
	TextureBank *bank = video_bank->getTextureBank();
	if (ImGui::TreeNode(ImGui_ID(0), "Textures (%d)", bank->getCount())) {
		// 表示フィルタ
		static char s_filter[256] = {0};
		ImGui::InputText("Filter", s_filter, sizeof(s_filter));

		PathList allnames = bank->getNames();

		// 名前リスト
		PathList names;
		names.reserve(allnames.size());
		if (s_filter[0]) {
			// フィルターあり。
			// 部分一致したものだけリストに入れる
			for (auto it=allnames.cbegin(); it!=allnames.cend(); ++it) {
				const char *s = it->u8();
				if (K_strpos(it->u8(), s_filter, 0) >= 0) {
					names.push_back(*it);
				}
			}
		} else {
			// フィルターなし
			names.assign(allnames.begin(), allnames.end());
		}

		// ソート
		std::sort(names.begin(), names.end());

		// 表示オプション
		static bool s_realsize = false;
		ImGui::Checkbox("Real size", &s_realsize);

		static bool s_rentex = true;
		static bool s_tex2d = true;
		ImGui::Checkbox("Render texture", &s_rentex);
		ImGui::Checkbox("Normal texture", &s_tex2d);

		// リスト表示
		for (auto it=names.cbegin(); it!=names.cend(); ++it) {
			Path name = *it;

			TEXID tex = bank->findTextureRaw(name, true);

			TexDesc desc;
			Video::getTextureDesc(tex, &desc);
			if (desc.is_render_target && !s_rentex) continue;
			if (!desc.is_render_target && !s_tex2d) continue;

			bool isopen = ImGui::TreeNode(name.u8());
			ImGui::SameLine();
			ImGui::Spacing();

			if (desc.is_render_target) {
				ImGui::SameLine();
				ImGui::TextColored(GuiColor_WarningText, "[R]");
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("[R] Render texture");
				}
			}
			if (!Num_is_pow2(desc.w) || !Num_is_pow2(desc.h)) {
				ImGui::SameLine();
				ImGui::TextColored(GuiColor_WarningText, "[!2]");
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("[!2] Texture size is not power of 2");
				}
			}
			if (desc.w != desc.h) {
			//	ImGui::SameLine();
			//	ImGui::TextColored(GuiColor_WarningText, "[!SQ]");
			//	if (ImGui::IsItemHovered()) {
			//		ImGui::SetTooltip("[!SQ] Texture size is not square");
			//	}
			}
			if (isopen) {
				TextureBank_guiTextureInfo(name, s_realsize ? 0 : EDITOR_THUMBNAIL_SIZE);
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}
#endif // !NO_IMGUI
}

void TextureBank_guiTextureInfo(const Path &tex_path, int box_size) {
	if (tex_path.empty()) {
		ImGui::Text("(NULL)");
		return;
	}
	TextureBank *tex_bank = g_engine->videobank()->getTextureBank();
	TEXID tex = tex_bank->findTextureRaw(tex_path, true);
	if (tex == NULL) {
		ImGui::Text("(NULL)");
		return;
	}
	TexDesc desc;
	Video::getTextureDesc(tex, &desc);
	if (desc.w == 0 || desc.h == 0 || desc.d3dtex9 == 0) {
		ImGui::TextColored(GuiColor_ErrorText, "Texture not loaded");
		return;
	}
	int w = desc.w;
	int h = desc.h;
	bool is_pow2 = Num_is_pow2(w) && Num_is_pow2(h);
	if (!is_pow2) {
		ImGui::TextColored(GuiColor_WarningText, "Not power of 2");
	}

	if (box_size > 0) {
		if (w >= h) {
			w = box_size;
			h = desc.h * w / desc.w;
		} else {
			h = box_size;
			w = desc.w * h / desc.h;
		}
	} else {
		// 原寸で表示
	}
	static const int BORDER_COLORS_SIZE = 4;
	static const Color BORDER_COLORS[BORDER_COLORS_SIZE] = {
		Color(0, 0, 0, 0),
		Color(1, 1, 1, 1),
		Color(0, 1, 0, 1),
		Color(0, 0, 0, 1),
	};
	static int s_border_mode = 0;

	ImGui_SImageParams p;
	p.w = w;
	p.h = h;
	p.border_color = BORDER_COLORS[s_border_mode % BORDER_COLORS_SIZE];
	ImGui_Image(tex, &p);
	ImGui::Text("Is render texture: %s", desc.is_render_target ? "Yes" : "No");
	ImGui::Text("Size: %dx%d", desc.w, desc.h);
	ImGui::Text("TexID: 0x%X", tex);
	ImGui::PushID(tex);
	if (ImGui::Button("Border")) {
		s_border_mode++;
	}
	if (1) {
		// テクスチャ画像を png でエクスポートする
		Path texname = g_engine->videobank()->getTextureName(tex);
		Path relname = Path::fromFormat("__export__%s.png", AssetPath::escapeFileName(texname).u8());
		Path filename = relname.getFullPath();
		if (ImGui::Button("Export")) {
			KImage image = Video::exportTextureImage(tex);
			if (!image.empty()) {
				std::string png = image.saveToMemory();
				FileOutput(filename).write(png);
				Log_infof("Export texture image '%s' ==> %s", g_engine->videobank()->getTextureName(tex).u8(), filename.u8());
			}
		}
		if (Platform::fileExists(filename)) {
			ImGui::SameLine();
			std::string s = K_sprintf("Open: %s", filename.u8());
			if (ImGui::Button(s.c_str())) {
				Platform::openFileInSystem(filename);
			}
		}
	}
	if (1) {
		// テクスチャ画像のアルファマスクを png でエクスポートする
		Path texname = g_engine->videobank()->getTextureName(tex);
		Path relname = Path::fromFormat("__export__%s_a.png", AssetPath::escapeFileName(texname).u8());
		Path filename = relname.getFullPath();
		if (ImGui::Button("Export Alpha")) {
			KImage image = Video::exportTextureImage(tex, 3);
			if (!image.empty()) {
				std::string png = image.saveToMemory();
				FileOutput(filename).write(png);
				Log_infof("Export alpha texture image '%s' ==> %s", texname.u8(), filename.u8());
			}
		}
		if (Platform::fileExists(filename)) {
			ImGui::SameLine();
			std::string s = K_sprintf("Open: %s", filename.u8());
			if (ImGui::Button(s.c_str())) {
				Platform::openFileInSystem(filename);
			}
		}
	}
	ImGui::PopID();
}
bool TextureBank_guiTextureSelector(const char *label, TEXID *p_texture) {
	TextureBank *tex_bank = g_engine->videobank()->getTextureBank();
	Path cur_path = (p_texture && *p_texture) ? g_engine->videobank()->getTextureName(*p_texture) : Path::Empty;
	Path new_path;
	if (TextureBank_guiTextureSelector(label, cur_path, &new_path)) {
		Log_assert(p_texture);
		*p_texture = tex_bank->findTextureRaw(new_path, true);
		return true;
	}
	return false;
}
bool TextureBank_guiTextureSelector(const char *label, const Path &selected, Path *new_selected) {
	// ソート済みのアイテム名リストを得る
	TextureBank *bank = g_engine->videobank()->getTextureBank();
	PathList names = bank->getNames();
	names.insert(names.begin(), "(NULL)"); // NULL を選択肢に含める
	//
	int sel_index = -1;
	if (ImGui_Combo(label, names, selected, &sel_index)) {
		if (sel_index > 0) {
			*new_selected = names[sel_index];
		} else {
			*new_selected = Path::Empty; // NULL が選択された
		}
		return true;
	}
	return false;
}
bool TextureBank_guiRenderTextureSelector(const char *label, const Path &selected, Path *new_selected) {
	// ソート済みのアイテム名リストを得る
	TextureBank *bank = g_engine->videobank()->getTextureBank();
	PathList names = bank->getRenderTexNames();
	names.insert(names.begin(), "(NULL)"); // NULL を選択肢に含める
	//
	int sel_index = -1;
	if (ImGui_Combo(label, names, selected, &sel_index)) {
		if (sel_index > 0) {
			*new_selected = names[sel_index];
		} else {
			*new_selected = Path::Empty; // NULL が選択された
		}
		return true;
	}
	return false;
}

void ShaderBank_onInspectorGui() {
#ifndef NO_IMGUI
	ShaderBank *bank = g_engine->videobank()->getShaderBank();
	if (ImGui::TreeNode("Shader", "Shader(%d)", bank->getCount())) {
		if (ImGui::Button("Clear")) {
			bank->clearAssets();
		}
		PathList names = bank->getNames();
		for (auto it=names.cbegin(); it!=names.cend(); ++it) {
			const Path &name = *it;
			if (ImGui::TreeNode(name.u8())) {
				SHADERID shader = bank->findShader(name);
				ShaderBank_guiShaderInfo(shader);
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}
#endif // !NO_IMGUI
}
void ShaderBank_guiShaderInfo(SHADERID shader) {
#ifndef NO_IMGUI
	ShaderBank *bank = g_engine->videobank()->getShaderBank();
	if (shader == NULL) {
		ImGui::Text("(NULL)");
		return;
	}
	ShaderDesc desc;
	Video::getShaderDesc(shader, &desc);
	ImGui::Text("Name: %s", bank->getName(shader).u8());
	ImGui::Text("Type: %s", desc.type);
#endif // !NO_IMGUI
}
bool ShaderBank_guiShaderSelector(const char *label, SHADERID *p_shader) {
	ShaderBank *bank = g_engine->videobank()->getShaderBank();
	Path cur_path = *p_shader ? bank->getName(*p_shader) : Path::Empty;
	Path new_path;
	if (ShaderBank_guiShaderSelector(label, cur_path, &new_path)) {
		*p_shader = bank->findShader(new_path, false); // 「シェーダーなし」が選択される可能性に注意
		return true;
	}
	return false;
}
bool ShaderBank_guiShaderSelector(const char *label, const Path &selected, Path *new_selected) {
#ifndef NO_IMGUI
	// ソート済みのアイテム名リストを得る
	ShaderBank *bank = g_engine->videobank()->getShaderBank();
	PathList names = bank->getNames();
	names.insert(names.begin(), "(NULL)"); // NULL を選択肢に含める

	int sel_index = -1;
	if (ImGui_Combo(label, names, selected, &sel_index)) {
		if (sel_index > 0) {
			*new_selected = names[sel_index];
		} else {
			*new_selected = Path::Empty; // NULL が選択された
		}
		return true;
	}
#endif // !NO_IMGUI
	return false;
}






void Data_guiMeshInfo(const KMesh *mesh) {
	if (mesh == NULL) {
		ImGui::Text("(NULL)");
		return;
	}
	ImGui::Text("Vertex: %d", mesh->getVertexCount());
	ImGui::Text("Indices: %d", mesh->getIndexCount());
	Vec3 minp, maxp;
	mesh->getAabb(&minp, &maxp);
	ImGui::Text("AABB min(%g, %g, %g)", minp.x, minp.y, minp.z);
	ImGui::Text("AABB max(%g, %g, %g)", maxp.x, maxp.y, maxp.z);
	if (ImGui::TreeNode(ImGui_ID(mesh), "Submesh (%d)", mesh->getSubMeshCount())) {
		for (int i=0; i<mesh->getSubMeshCount(); i++) {
			if (ImGui::TreeNode(ImGui_ID(i), "[%d]", i)) {
				KSubMesh sub;
				if (mesh->getSubMesh(i, &sub)) {
					ImGui::Text("Start %d", sub.start);
					ImGui::Text("Count %d", sub.count);
					ImGui::Text("Primitive %s", _GetPrimitiveName(sub.primitive));
				}
				ImGui::TreePop();
			}
		}
		ImGui::TreePop();
	}
	if (1) {
		// メッシュファイルをエクスポートする時に使うファイル名を作成する
		Path export_filename = Path("__export__mesh.txt").getFullPath();
		if (ImGui::Button("Export")) {
			FileOutput file;
			if (file.open(export_filename)) {
				mo::meshSaveToXml(mesh, file);
				Log_infof("Export mesh '%s'", export_filename.u8());
			}
		}
		if (Platform::fileExists(export_filename)) {
			std::string s = K_sprintf("Open: %s", export_filename.u8());
			ImGui::SameLine();
			if (ImGui::Button(s.c_str())) {
				Platform::openFileInSystem(export_filename);
			}
		}
	}
}

bool Data_guiMaterialInfo(Material *mat) {
#ifndef NO_IMGUI
	if (mat == NULL) {
		ImGui::Text("(NULL)");
		return false;
	}
	bool changed = false;
	// Shader
	if (1) {
		ShaderBank_guiShaderSelector("Shader", &mat->shader);
	}
	// Texture
	if (1) {
		TextureBank_guiTextureSelector("Main texture", &mat->texture);
	}
	// Color
	if (1) {
		ImGui::ColorEdit4("Main color", reinterpret_cast<float*>(&mat->color));
	}
	// Specular
	if (1) {
		ImGui::ColorEdit4("Specular", reinterpret_cast<float*>(&mat->specular));
	}
	// Blend
	if (1) {
		changed |= ImGui_BlendCombo("Blend", &mat->blend);
	}
	// Filter
	Filter filter = mat->filter;
	if (ImGui_FilterCombo("Filter", &filter)) {
		mat->filter = filter;
		changed = true;
	}
	// Shader params
	if (ImGui::TreeNode("Shader parameters")) {
		{
			TEXID tex = mat->texture;
			if (tex) {
				Path tex_path = g_engine->videobank()->getTextureName(tex);
				if (TextureBank_guiTextureSelector(tex_path.u8(), &tex)) {
					mat->texture = tex;
					changed = true;
				}
				ImGui::Indent();
				if (ImGui::TreeNode("Texture info")) {
					TextureBank_guiTextureInfo(tex_path, EDITOR_THUMBNAIL_SIZE);
					ImGui::TreePop();
				}
				ImGui::Unindent();
			}
		}
		{
			if (ImGui::ColorEdit4("MainColor", reinterpret_cast<float*>(&mat->color))) {
				changed = true;
			}
		}
		{
			if (ImGui::ColorEdit4("Specular", reinterpret_cast<float*>(&mat->specular))) {
				changed = true;
			}
		}
		ImGui::TreePop();
	}
	return changed;
#else
	return false;
#endif // NO_IMGUI
}
#pragma endregion // Inspector funcs


} // namespace

#endif // !NO_IMGUI
