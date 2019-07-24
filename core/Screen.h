#pragma once
#include "KMatrix4.h"
#include "KPath.h"
#include "KRect.h"
#include "KVideoDef.h"

namespace mo {

class Engine;
class Gizmo;
class Inspector;
class ImGuiSystem;
class VideoBank;

enum ProjectionType {
	ProjectionType_Ortho,
	ProjectionType_OrthoCabinet,
	ProjectionType_Frustum,
};

/// 描画順序のソート方法
enum RenderingOrder {
	RenderingOrder_TimeStamp, ///< エンティティを追加した順番で描画
	RenderingOrder_Hierarchy, ///< ヒエラルキーを深さ優先でスキャンした順番で描画
	RenderingOrder_ZDepth,    ///< Z値にしたがって描画
	RenderingOrder_Custom,    ///< ユーザー定義のソート方法。RenderSystem::onCustomSorting2() を実装すること
};

struct View {
	Path name;
	float width;
	float height;
	float znear;
	float zfar;
	float cabinet_dx_dz;
	float cabinet_dy_dz;
	float zoom;
	RectF viewport;
	Color bgcolor;
	Matrix4 projection_matrix;
	Matrix4 transform_matrix;
	ProjectionType projection_type;
	Vec3 rendering_offset;
	RenderingOrder object_order;
	TEXID render_target;
	int object_layer;
	int render_order;
	bool visible;

	View() {
		// Ortho行列が単位行列になるように初期化
		width             = 2.0f;
		height            = 2.0f;
		znear             = 0.0f;
		zfar              = 1.0f;
		zoom              = 1.0f;
		projection_matrix = Matrix4::fromOrtho(width, height, znear, zfar);
		transform_matrix  = Matrix4();
		viewport.xmin     = 0.0f;
		viewport.ymin     = 0.0f;
		viewport.xmax     = 1.0f;
		viewport.ymax     = 1.0f;
		bgcolor           = Color(0, 0, 0, 1);
		projection_type   = ProjectionType_Ortho;
		cabinet_dx_dz     = 0.5f;
		cabinet_dy_dz     = 1.0f;
		object_order      = RenderingOrder_Hierarchy;
		object_layer      = 0;
		render_target     = NULL;
		render_order      = 0;
		visible           = true;
	}
	float projection_w() const { 
		return width / zoom;
	}
	float projection_h() const { 
		return height / zoom;
	}
};

struct RenderParams {
	RenderParams() {
	}
	Matrix4 projection_matrix; ///< シェーダーの ProjectionMatrix に渡す行列
	Matrix4 transform_matrix;  ///< シェーダーの TransformMatrix に渡す行列
};
struct DebugRenderParameters {
	DebugRenderParameters() {
		gizmo = NULL;
	}
	RenderParams render_params;
	Gizmo *gizmo;
	View view;
};
struct RenderWorldParameters {
	RenderWorldParameters() {
		debug_selections_only = false;
	}
	RenderParams render_params;
	View view;
	bool debug_selections_only;
};
class IScreenCallback {
public:
	/// GUI を更新する時に呼ばれる
	virtual void onUpdateGui() {}
	virtual void onRawRender() {}
	virtual void onRenderWorld(RenderWorldParameters *params) {}
	virtual void onDrawDebugInfo(const DebugRenderParameters &p) {}
	virtual void onDrawDebugInfoSelected(const DebugRenderParameters &p) {}
};


/// デバッグ用のウィンドウなどを含め、ゲーム画面内に表示するべきものをすべて描画するためのクラス
class Screen {
public:
	enum ERenderTexId {
		RT_RAW,      ///< エフェクト適用前のゲーム画面
		RT_RAW_COPY, ///< RT_RAW の コピー
		RT_MASK,     ///< ポストエフェクト用マスク
		RT_OUT2,     ///< エフェクト適用後のゲーム画面
		RT_OUT,      ///< 最終出力の画面
		RT_SEL,      ///< 選択オブジェクトの描画用
		RT_TMP_0,    ///< エフェクト適用後のゲーム画面（作業用）
		RT_TMP_1,
		RT_ENUM_MAX
	};

	Screen();
	~Screen();
	void init(Engine *engine, int w, int h);
	void start();

	void shutdown();
	bool render();

	TEXID getRenderTexture(ERenderTexId id);

	/// 内部で使用するテクスチャやマテリアルなどを作成する
	void createCoreResources();

	/// 内部で利用するために作成されたテクスチャやマテリアルなどを削除する
	void destroyCoreResources();

	/// ゲーム画面の解像度を指定する
	void setGameResolution(int w, int h);

	/// フルスクリーン時の解像度を指定する
	void setFullScreenResolution(int w, int h);

	int getGameResolutionW() const;
	int getGameResolutionH() const;
	RectF getGameViewportRect() const;

	/// ウィンドウクライアント座標（左上が原点、Y軸下向き、ピクセル単位）を
	/// スクリーン単位座標（中央原点、Ｙ軸上向き、右上座標をX,Y共に 1.0 とする）に変換する
	Vec3 windowClientToScreenPoint(const Vec3 &p) const;

	Material * getMaterial();

	void setRenderTargetEnable(bool b);
	bool isRenderTargetEnable() const;

	/// 次回描画時の描画結果をファイルにエクスポートする
	void postExportScreenTexture(const Path &filename);

	void setKeepAspect(bool val) { keep_aspect_ = val; }
	bool isKeepAspect() const { return keep_aspect_; }

	/// 通常描画時でもレンダーターゲットを経由する。
	/// レンダーターゲットを全く使わないようにするなら false
	bool use_render_texture_;

	/// 選択オブジェクトを描画するためのマテリアル
	Material sel_material_;

	/// 最終的に画面に描画する時のマテリアル
	Material final_material_;

	void addCallback(IScreenCallback *cb);
	void removeCallback(IScreenCallback *cb);

	void clearView();
	void addView(const View &view);
	int getViewCount() const;
	int getViewIndexOfName(const Path &name) const;
	const View & getView(int index) const;
	void setView(int index, const View &view);
	bool isViewVisible(int index) const;
	void setViewVisible(int index, bool val);

private:
	enum RenderMask_ {
		RenderMask_World = 1,
		RenderMask_DebugLines = 2,
		RenderMask_SelectedDebugLines = 4,
	};
	typedef int RenderMask; // combination of RenderMask_
	void zeroClear();
	void renderScene();
	void render_entities();
	void render_entitiesSelected();
	void render_debugInfo(Gizmo *gizmo);
	void render_debugInfoSelected(Gizmo *gizmo);
	bool Engine_hasInternalGameWindow();
	void call_onDrawDebugInfoSelected(const DebugRenderParameters &p);
	void call_onDrawDebugInfo(const DebugRenderParameters &p);

	TEXID raw_render_tex();
	TEXID raw_render_tex_copy();
	TEXID mask_render_tex();
	TEXID out_render_tex();
	TEXID out_render_tex2();
	TEXID tmp_render_tex(int i);
	TEXID sel_render_tex();
	SHADERID outline_shader_;
	VideoBank * getVideoBank();

	std::vector<IScreenCallback *> cblist_;
	std::vector<View> views_;
	TEXID render_tex_[RT_ENUM_MAX];
	Inspector *inspector_;
	Engine *engine_;
	Gizmo *gizmo_;
	int resolution_x_;
	int resolution_y_;
	int fullscreen_resolution_x_;
	int fullscreen_resolution_y_;
	Path export_screen_filename_;
	bool export_screen_texture_;
	bool keep_aspect_;
	bool trace_;
};


}
