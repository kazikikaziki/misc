#pragma once
#ifndef NO_IMGUI
#include "GameId.h"
#include "inspector_entity.h"
#include "inspector_engine.h"
#include "inspector_systems.h"
#include "KRect.h"
#include "KVideoDef.h"

namespace mo {

class Engine;
class System;
class KMesh;
class Path;
struct Material;

static const int EDITOR_THUMBNAIL_SIZE = 256;



class Inspector {
public:
	Inspector();
	~Inspector();
	void init(Engine *engine);
	void shutdown();

	void render(TEXID game_display);
	void guiMain(TEXID game_tex);

	int  getSelectedEntityCount() const;
	EID getSelectedEntity(int index);
	bool isEntityVisible(EID e) const;
	bool isEntitySelected(EID e) const;
	bool isEntitySelectedInHierarchy(EID e) const;

	/// インスペクターを表示するかどうか
	void setVisible(bool value);
	bool isVisible() const;

	/// インスペクターで使うフォントを指定する
	/// font_index: フォントインデックス。これは ImGui に登録されているフォント配列のインデックスで、
	/// ImGui::GetIO().Fonts->Fonts[index] のフォントを表す。
	/// フォントの追加には ImGuiVideo_AddFont() を使う
	void setFont(int font_index);

	void setHighlightedEntity(EID e);
	bool isEntityHighlighted(EID e, bool in_hierarchy) const;

	void selectEntity(EID e);
	void unselectAllEntities();

	/// ウィンドウ内で実際にゲーム画面が描画されている領域を得る。
	/// ゲームビューが表示されていれば rectInPixel にクライアントウィンドウ座標系（左上が原点）で矩形をセットして true を返す。
	/// デバッグ用GUIを経由せずにウィンドウに直接描画されている場合は何もせずに false を返す
	RectF getWorldViewport() const;

	bool isStatusTipVisible() const;
	void setStatusTipVisible(bool visible);
	bool isOverwrappingWindowMode() const;
	void setOverwrappingWindowMode(bool value);

	/// システム s のインスペクターが開いているかどうか
	bool isSystemInspectorVisible(System *s);

	/// エンティティを選択状態として描画するべきかどうか
	bool entityShouldRednerAsSelected(EID e) const;

	/// インスペクター上で選択されているオブジェクトを、ゲーム画面内で強調表示する
	bool isHighlightSelectionsEnabled() const;

	SystemEditorParams * getSystemEditorParams(void *s) {
		return engine_system_inspector_.getInfo(s);
	}

private:
	bool getWorldViewSize(int *w, int *h, TEXID game_tex) const;
	void guiFontConfig();
	void guiInspectorPreference();
	void guiStatusTip();
	void guiWorldBrowser(TEXID game_tex);
	void guiSystemBrowser();
	void guiScreenMenu();

	CCoreEngineInspector core_engine_inspector_;
	CEngineSystemInspector engine_system_inspector_;
	CEntityInspector entity_inspector_;
	EID highlighted_entity_;
	RectF world_viewport_in_window_;
	Engine *engine_;
	int system_pane_w_;
	int entity_pane_w_;
	int font_index_;
	bool highlight_selections_enabled_;
	bool overwrap_window_;
	bool status_visible_;
	bool visibled_;
	bool demo_;
};

class VideoBank;
void TextureBank_onInspectorGui(VideoBank *video_bank);
void TextureBank_guiTextureInfo(const Path &asset_path, int box_size);
bool TextureBank_guiTextureSelector(const char *label, TEXID *p_texture);
bool TextureBank_guiTextureSelector(const char *label, const Path &selected, Path *new_selected);
bool TextureBank_guiRenderTextureSelector(const char *label, const Path &selected, Path *new_selected);

void ShaderBank_onInspectorGui();
void ShaderBank_guiShaderInfo(SHADERID shader);
bool ShaderBank_guiShaderSelector(const char *label, SHADERID *p_shader);
bool ShaderBank_guiShaderSelector(const char *label, const Path &selected, Path *new_selected);

void Data_guiMeshInfo(const KMesh *mesh);
bool Data_guiMaterialInfo(Material *mat);

}

#endif // !NO_IMGUI
