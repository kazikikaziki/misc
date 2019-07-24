#include "Screen.h"
//
#include "GameCamera.h"
#include "GameGizmo.h"
#include "GameRender.h"
#include "VideoBank.h"
#include "Engine.h"
#include "inspector.h"
#include "KFileLoader.h"
#include "KClock.h"
#include "KImgui.h"
#include "KLog.h"
#include "KNum.h"
#include "KVideo.h"

#define DEFAULT_SNAPSHOT_FILENAME  "_ScreenTexture.png"

/// 基本的なゲーム画面のレンダーターゲット
#define mo_RENDERTEX_BASE  "__base__.tex"

#define mo_RENDERTEX_MASK  "__mask__.tex"

#define mo_RENDERTEX_SELECTION "__sel__.tex"

// ゲーム画面用のレンダーターゲットの描画内容をコピーしたテクスチャ。
// ゲーム画面をテクスチャとしてエンティティに適用したい時につかう。
// mo_RENDERTEX_BASE をスプライトレンダラーなどのテクスチャとして使ってしまうと、テクスチャの参照元がゲーム画面の描画先レンダーターゲットと同じになってしまい、
// 予期しない描画結果になる可能性があるため、コピーを使うようにする
#define mo_RENDERTEX_BASECOPY  "__basecopy__.tex"

// 最終的に画面に出力するゲーム画面。システムゲージや画面全体のエフェクトなどをすべて含む
#define mo_RENDERTEX_OUTPUT  "__output__.tex"
#define mo_RENDERTEX_OUTPUT2 "__output2__.tex"

// 画面エフェクト適用のための作業用
#define mo_RENDERTEX_TMP0  "__tmp0__.tex"
#define mo_RENDERTEX_TMP1  "__tmp1__.tex"

namespace mo {

static const char *g_OutlineHLSL =
	"uniform float4x4 mo__MatrixView;											\n"
	"uniform float4x4 mo__MatrixProj;											\n"
	"uniform texture  mo__Texture;												\n"
	"uniform float4   mo__Color;												\n"
	"uniform float2   mo__TextureSize;											\n"
	"sampler samMain = sampler_state {											\n"
	"	Texture = <mo__Texture>;												\n"
	"	MinFilter = Point;														\n"
	"	MagFilter = Point;														\n"
	"	MipFilter = Point;														\n"
	"	AddressU  = Clamp;														\n"
	"	AddressV  = Clamp;														\n"
	"};																			\n"
	"struct RET {																\n"
	"	float4 pos: POSITION;													\n"
	"	float2 uv: TEXCOORD0;													\n"
	"	float4 color: COLOR0;													\n"
	"};																			\n"
	"RET vs(float4 pos: POSITION, float2 uv: TEXCOORD0, float4 color: COLOR0) {	\n"
	"	float4x4 mat = mul(mo__MatrixView, mo__MatrixProj);						\n"
	"	RET ret = (RET)0;														\n"
	"	ret.pos = mul(pos, mat);												\n"
	"	ret.uv = uv;															\n"
	"	ret.color = color;														\n"
	"	return ret;																\n"
	"}																			\n"
	"float4 ps(float2 uv: TEXCOORD0): COLOR {									\n"
	"	float4 color = {0, 0, 0, 0};											\n"
	"	float dx = 1.0 / mo__TextureSize.x;										\n"
	"	float dy = 1.0 / mo__TextureSize.y;										\n"
	"	float a = tex2D(samMain, uv).a;											\n"
	"	if (a == 0) {															\n"
	"		float b = tex2D(samMain, uv + float2( dx, 0)).a;					\n"
	"		float c = tex2D(samMain, uv + float2(-dx, 0)).a;					\n"
	"		float d = tex2D(samMain, uv + float2(0, -dy)).a;					\n"
	"		float e = tex2D(samMain, uv + float2(0,  dx)).a;					\n"
	"		if (b+c+d+e > 0) {													\n"
	"			color = float4(1, 0, 1, 1);										\n"
	"		}																	\n"
	"	}																		\n"
	"	return color;															\n"
	"}																			\n"
	"technique main {															\n"
	"	pass P0 {																\n"
	"		VertexShader = compile vs_2_0 vs();									\n"
	"		PixelShader  = compile ps_2_0 ps();									\n"
	"	}																		\n"
	"}																			\n"
	;





static void _ExportTextureImage(TEXID tex, const Path &filename) {
	KImage img = Video::exportTextureImage(tex);
	if (!img.empty()) {
		std::string png = img.saveToMemory();
		FileOutput(filename).write(png);
	}
}
static void _SetViewportWithAspect(const RectF &nor_vp, int dest_w, int dest_h, float aspect_w_div_h) {
	Log_assert(aspect_w_div_h > 0);
	int x = (int)(dest_w * nor_vp.xmin);
	int y = (int)(dest_h * (1.0f - nor_vp.ymax)); // ｙ軸下向きに
	int w = (int)(dest_w * (nor_vp.xmax - nor_vp.xmin));
	int h = (int)(dest_h * (nor_vp.ymax - nor_vp.ymin));
	float dest_aspect = (float)dest_w / dest_h;
	if (dest_aspect < aspect_w_div_h) {
		// 出力先の方が縦長になっている
		// 横幅はそのままで、画面高さを調節する
		h = (int)(w / aspect_w_div_h);
		y = (dest_h - h) / 2;
	} else if (dest_aspect > aspect_w_div_h) {
		// 出力先の方が横長になっている
		// 高さはそのままで、画面幅を調節する
		w = (int)(h * aspect_w_div_h);
		x = (dest_w - w) / 2;
	}
	Video::setViewport(x, y, w, h);
}
static void _SetViewportFit(const RectF &nor_vp, int dest_w, int dest_h) {
	int x = (int)(dest_w * nor_vp.xmin);
	int y = (int)(dest_h * (1.0f - nor_vp.ymax)); // ｙ軸下向きに
	int w = (int)(dest_w * (nor_vp.xmax - nor_vp.xmin));
	int h = (int)(dest_h * (nor_vp.ymax - nor_vp.ymin));
	Video::setViewport(x, y, w, h);
}
/// src: Source render textrue
/// dst: Target render texture
/// materials: array of materials
/// materials_count: number of materials
static void _Blit(TEXID src, TEXID dst, Material **materials, int materials_count, const char *trace_filename) {
	if (src == NULL) return;
	Log_assert(src != dst);
	TEXID old_src = src;
	if (materials && materials_count > 0) {
		for (int m=0; m<materials_count; m++) {
			Material *mat = materials[m];
			if (mat && mat->cb) {
				Video::blit(dst, src, mat);
			}
			if (m+1 < materials_count) {
				std::swap(src, dst); // src <--> dst
			}
		}
		if (old_src != src) {
			// 奇数回 swap したために、最終結果を保持しているレンダーターゲットが入れ替わっている
			std::swap(src, dst); // src <--> dst
			Video::blit(dst, src, NULL);
		}
	} else {
		Video::blit(dst, src, NULL);
	}
}


#pragma region Screen
Screen::Screen() {
	zeroClear();
}
Screen::~Screen() {
	shutdown();
}
void Screen::zeroClear() {
	engine_ = NULL;
	gizmo_ = NULL;
	resolution_x_ = 0;
	resolution_y_ = 0;
	fullscreen_resolution_x_ = 0;
	fullscreen_resolution_y_ = 0;
	export_screen_texture_ = 0;
	keep_aspect_ = false;
	trace_ = false;
	memset(render_tex_, 0, sizeof(render_tex_));
	outline_shader_ = NULL;
}
void Screen::init(Engine *engine, int w, int h) {
	Log_trace;
	Log_assert(w > 0);
	Log_assert(h > 0);
	zeroClear();
	engine_ = engine;
	resolution_x_ = w;
	resolution_y_ = h;
	fullscreen_resolution_x_ = resolution_x_;
	fullscreen_resolution_y_ = resolution_y_;
	use_render_texture_ = true;
	keep_aspect_ = true;
	final_material_.blend = Blend_One;
	final_material_.color = Color::WHITE;
	final_material_.filter = Filter_None;
	outline_shader_ = NULL;
}
void Screen::start() {
	Log_assert(Video::isInitialized());
	Log_assert(engine_);
	gizmo_ = engine_->getGizmo();
	inspector_ = engine_->getInspector();
	outline_shader_ = Video::createShaderFromHLSL(g_OutlineHLSL, "Engine_OutlineFx");
	// この時点で誰もビューを追加していない場合、
	// デフォルトのビューを作成しておく
	if (views_.empty()) {
		Log_assert(resolution_x_ > 0);
		Log_assert(resolution_y_ > 0);
		View view;
		view.width  = (float)resolution_x_;
		view.height = (float)resolution_y_;
		view.viewport.xmin = 0;
		view.viewport.ymin = 0;
		view.viewport.xmax = (float)resolution_x_;
		view.viewport.ymax = (float)resolution_y_;
		view.projection_type = ProjectionType_Ortho;
		view.projection_matrix = Matrix4::fromOrtho((float)resolution_x_, (float)resolution_y_, -1000, 1000);
		views_.push_back(view);
	}
}
void Screen::shutdown() {
	if (outline_shader_) {
		Video::deleteShader(outline_shader_);
		outline_shader_ = NULL;
	}
	destroyCoreResources();
	zeroClear();
}
void Screen::addCallback(IScreenCallback *cb) {
	auto it = std::find(cblist_.begin(), cblist_.end(), cb);
	if (it == cblist_.end()) {
		cblist_.push_back(cb);
	}
}
void Screen::removeCallback(IScreenCallback *cb) {
	auto it = std::find(cblist_.begin(), cblist_.end(), cb);
	if (it != cblist_.end()) {
		cblist_.erase(it);
	}
}
void Screen::clearView() {
	views_.clear();
}
void Screen::addView(const View &view) {
	views_.push_back(view);
}
int Screen::getViewIndexOfName(const Path &name) const {
	for (size_t i=0; i<views_.size(); i++) {
		if (views_[i].name.compare(name) == 0) {
			return (int)i;
		}
	}
	return -1;
}
const View & Screen::getView(int index) const {
	return views_[index];
}
void Screen::setView(int index, const View &view) {
	views_[index] = view;
}
int Screen::getViewCount() const {
	return (int)views_.size();
}
bool Screen::isViewVisible(int index) const {
	return views_[index].visible;
}
void Screen::setViewVisible(int index, bool val) {
	views_[index].visible = val;
}

TEXID Screen::getRenderTexture(ERenderTexId id) {
	if (! use_render_texture_) return NULL;
	return render_tex_[id];
}
TEXID Screen::raw_render_tex() {
	return getRenderTexture(RT_RAW);
}
TEXID Screen::raw_render_tex_copy() {
	return getRenderTexture(RT_RAW_COPY);
}
TEXID Screen::mask_render_tex() {
	return getRenderTexture(RT_MASK);
}
TEXID Screen::out_render_tex() {
	return getRenderTexture(RT_OUT);
}
TEXID Screen::out_render_tex2() {
	return getRenderTexture(RT_OUT2);
}
TEXID Screen::tmp_render_tex(int i) {
	Log_assert(0 <= i && i<2);
	ERenderTexId id = (ERenderTexId)(RT_TMP_0 + i);
	Log_assert(RT_TMP_0 <= id && id <= RT_TMP_1);
	return getRenderTexture(id);
}
TEXID Screen::sel_render_tex() {
	return getRenderTexture(RT_SEL);
}
VideoBank * Screen::getVideoBank() {
	Log_assert(engine_);
	return engine_->videobank();
}
void Screen::postExportScreenTexture(const Path &filename) {
	export_screen_texture_ = true;
	export_screen_filename_ = filename;
}
void Screen::createCoreResources() {
	if (use_render_texture_) {
		TextureBank *tex_bank = getVideoBank()->getTextureBank();
		render_tex_[RT_RAW_COPY] = tex_bank->addRenderTexture(mo_RENDERTEX_BASECOPY, resolution_x_, resolution_y_);
		render_tex_[RT_RAW]      = tex_bank->addRenderTexture(mo_RENDERTEX_BASE,     resolution_x_, resolution_y_);
		render_tex_[RT_MASK]     = tex_bank->addRenderTexture(mo_RENDERTEX_MASK,     resolution_x_, resolution_y_);
		render_tex_[RT_OUT]      = tex_bank->addRenderTexture(mo_RENDERTEX_OUTPUT,   resolution_x_, resolution_y_);
		render_tex_[RT_OUT2]     = tex_bank->addRenderTexture(mo_RENDERTEX_OUTPUT2,  resolution_x_, resolution_y_);
		render_tex_[RT_SEL]      = tex_bank->addRenderTexture(mo_RENDERTEX_SELECTION,resolution_x_, resolution_y_);
		render_tex_[RT_TMP_0]    = tex_bank->addRenderTexture(mo_RENDERTEX_TMP0,     resolution_x_, resolution_y_);
		render_tex_[RT_TMP_1]    = tex_bank->addRenderTexture(mo_RENDERTEX_TMP1,     resolution_x_, resolution_y_);
	} else {
		for (int i=0; i<RT_ENUM_MAX; i++) {
			render_tex_[i] = NULL;
		}
	}
}
void Screen::destroyCoreResources() {
	for (int i=0; i<RT_ENUM_MAX; i++) {
		render_tex_[i] = NULL;
	}
}
void Screen::render_entities() {
	int src = 0;
	int dst = 1;

	// レンダーターゲットをクリアしておく
	if (use_render_texture_) {
		Video::pushRenderTarget(tmp_render_tex(src));
		Video::clearColor(Color(0, 0, 0, 0));
		Video::popRenderTarget();

		Video::pushRenderTarget(tmp_render_tex(dst));
		Video::clearColor(Color(0, 0, 0, 0));
		Video::popRenderTarget();
	} else {
		Video::clearColor(Color(0, 0, 0, 0));
	}

	bool should_clear = true;

	for (size_t ci=0; ci<views_.size(); ci++) {
		const View &view = views_[ci];

		// カメラの描画先レンダーターゲットが指定されている場合は、そこに描画する
		// 未指定の場合はスクリーン描画用のレンダーターゲットを使用する
		TEXID camera_rentex;
		bool is_private_rentex;
		{
			if (view.render_target) {
				camera_rentex = view.render_target;
				is_private_rentex = true;
				should_clear = false; // カメラ独自のテクスチャ使っている場合は勝手にクリアしないようにする
			} else {
				camera_rentex = tmp_render_tex(src);
				is_private_rentex = false;
			}
		}
		Video::pushRenderTarget(camera_rentex);
		// ビューポートを設定
		_SetViewportFit(view.viewport, resolution_x_, resolution_y_);
		// バッファクリア
		if (should_clear) {
			Video::clearColor(view.bgcolor);
			should_clear = false; // 最初の一枚をクリアしたらOFFに
		}
		// 描画パラメータ準備
		RenderParams render_params;
		render_params.projection_matrix = view.projection_matrix;
		render_params.transform_matrix = view.transform_matrix;
		// 描画
		for (auto it=cblist_.begin(); it!=cblist_.end(); ++it) {
			IScreenCallback *r = *it;
			KClock cl;
			RenderWorldParameters p;
			p.render_params = render_params;
			p.debug_selections_only = false;
			p.view = view;
			r->onRenderWorld(&p);
			engine_->addTimeConsumed(r, cl.getTimeMsec());
		}
		Video::popRenderTarget();

		_Blit(camera_rentex, tmp_render_tex(dst), NULL, 0, NULL);
		if (is_private_rentex) {
			// カメラ固有のレンダーターゲットに描画した。
			// ターゲット入れ替え不要
		} else {
			// 共有レンダーターゲットに描画した。
			// ターゲットを入れ替える
			std::swap(src, dst); // src <--> dst
		}
	}
	// 最終的に tmp_render_tex(curr) に描画された内容をゲーム画面として raw_render_tex() にコピーする
	if (use_render_texture_) {
		Video::blit(raw_render_tex(), tmp_render_tex(src), NULL);
	}
}
void Screen::render_entitiesSelected() {
	Video::clearColor(Color());
	for (size_t i=0; i<views_.size(); i++) {
		const View &view = views_[i];

		// ビューポートを設定
		_SetViewportFit(view.viewport, resolution_x_, resolution_y_);
		// 描画パラメータ準備
		RenderParams render_params;
		render_params.projection_matrix = view.projection_matrix;
		render_params.transform_matrix = view.transform_matrix;
		// 描画
		for (auto it=cblist_.begin(); it!=cblist_.end(); ++it) {
			IScreenCallback *r = *it;
			KClock cl;
			RenderWorldParameters p;
			p.render_params = render_params;
			p.debug_selections_only = true;
			p.view = view;
			r->onRenderWorld(&p);
			engine_->addTimeConsumed(r, cl.getTimeMsec());
		}
	}
}
void Screen::render_debugInfo(Gizmo *gizmo) {
	Log_assert(engine_);
	Log_assert(gizmo);

	// カメラごとに描画する
	for (size_t i=0; i<views_.size(); i++) {
		const View &view = views_[i];

		// 描画リストはカメラごとにリセットする。
		// そうしないと一つのギズモが複数のカメラで描画されてしまう
		gizmo->newFrame();

		// ビューポートを設定
		_SetViewportFit(view.viewport, resolution_x_, resolution_y_);
			
		// 描画パラメータ準備
		RenderParams render_params;
		render_params.projection_matrix = view.projection_matrix;
		render_params.transform_matrix = view.transform_matrix;

		// システムのデバッグ情報を画面に描画する
		DebugRenderParameters debug_params;
		debug_params.gizmo = gizmo;
		debug_params.view = view;
		debug_params.render_params = render_params;

		KClock cl;
		for (auto it=cblist_.begin(); it!=cblist_.end(); ++it) {
			IScreenCallback *r = *it;
			cl.reset();
			r->onDrawDebugInfo(debug_params);
			engine_->addTimeConsumed(r, cl.getTimeMsec());
		}
		gizmo->render(render_params.projection_matrix);
	}
}
void Screen::render_debugInfoSelected(Gizmo *gizmo) {
	Log_assert(engine_);
	Log_assert(gizmo);
	for (size_t i=0; i<views_.size(); i++) {
		const View &view = views_[i];

		// ビューポートを設定
		_SetViewportFit(view.viewport, resolution_x_, resolution_y_);
		// 描画パラメータ準備
		RenderParams render_params;
		render_params.projection_matrix = view.projection_matrix;
		render_params.transform_matrix = view.transform_matrix;
		// 描画
		DebugRenderParameters debug_params;
		debug_params.gizmo = gizmo;
		debug_params.render_params = render_params;
		call_onDrawDebugInfoSelected(debug_params);
		gizmo->render(render_params.projection_matrix);
	}
}
void Screen::renderScene() {
	Log_assert(engine_);

	// 描画順に並べたカメラ配列を用意
	auto cs = engine_->findSystem<CameraSystem>();
	if (cs) {
		views_.clear();
		cs->getViewInOrder(views_);
	}

	Video::clearColor(Color::BLACK);
	Video::clearDepth(1.0f);
	Video::clearStencil(0);

	// カメラ設定に従ってワールドを描画
	render_entities();
	
	if (gizmo_) {
		// オブジェクトのデバッグ情報を描画
		Video::pushRenderTarget(raw_render_tex()); // raw_render_tex() が NULL の場合はバックバッファに直接描画される
		render_debugInfo(gizmo_);
		Video::popRenderTarget();

		// 特定のオブジェクトのデバッグ情報だけを描画する。
		// この処理は、インスペクター上で選択されたオブジェクトだけの情報描画を想定しているが、
		// 実際にどのオブジェクトの情報を描画するかは各システムの onDrawDebugInfoSelected の実装による。
		Video::pushRenderTarget(raw_render_tex()); // raw_render_tex() が NULL の場合はバックバッファに直接描画される
		render_debugInfoSelected(gizmo_);
		Video::popRenderTarget();
	}

	if (raw_render_tex()) {
		// ゲーム画面を raw_render_tex_copy() にコピー
		Video::blit(raw_render_tex_copy(), raw_render_tex(), NULL);

		// ゲーム画面を out_render_tex2() にコピー
		Video::blit(out_render_tex2(), raw_render_tex(), NULL);
	} else {
		// レンダーターゲット無効。
		// ゲーム画面は既にバックバッファに描画されているため、転送不要
	}
#ifndef NO_IMGUI
	if (sel_render_tex() && inspector_ && inspector_->isVisible()) {
		
		// インスペクターで選択されているオブジェクトだけ描画
		Video::pushRenderTarget(sel_render_tex());
		render_entitiesSelected();
		Video::popRenderTarget();

		// 既存のゲーム画面に、選択オブジェクトの輪郭線を合成する
		if (inspector_->isHighlightSelectionsEnabled()) {
			if (sel_material_.shader == NULL) {
				sel_material_.blend = Blend_Alpha;
				sel_material_.shader = outline_shader_;
				sel_material_.color = Color::WHITE;
			}
			Video::blit(out_render_tex2(), sel_render_tex(), &sel_material_);
		}
	}
#endif // !NO_IMGUI

	if (use_render_texture_) {
		Video::blit(out_render_tex(), out_render_tex2(), NULL);

		// 完全不透明にする
		Video::fill(out_render_tex(), Color::BLACK, Video::MASK_A);
	}

	// 描画済みのゲーム画面を重ねる
	if (inspector_ && inspector_->isVisible() && !inspector_->isOverwrappingWindowMode()) {
		// デバッグ用のゲームウィンドウが存在する
		// デバッグ用の GUI を描画する時にゲーム画面も一緒に描画されるため、ここではなにもしない。

	} else {
		// デバッグ用のゲームウィンドウが存在しない。
		// デバッグウィンドウ内ではなく、バックバッファにゲーム画面を描画するため、いますぐに描画する
		if (! views_.empty()) {
			const View &view = views_[0];
			
			// 描画領域を決定
			int wnd_w = engine_->getStatus(Engine::ST_WINDOW_CLIENT_SIZE_W);
			int wnd_h = engine_->getStatus(Engine::ST_WINDOW_CLIENT_SIZE_H);
			if (keep_aspect_) {
				// アスペクト比を保つために適当に余白を入れて描画
				_SetViewportWithAspect(view.viewport, wnd_w, wnd_h, (float)resolution_x_ / resolution_y_);
			}
			else {
				// ウィンドウいっぱいに引き延ばして描画
				_SetViewportFit(view.viewport, wnd_w, wnd_h);
			}

			// 画像を転送する
			Material *mat = getMaterial();
			Video::blit(NULL, out_render_tex(), mat); // mo_RENDERTEX_OUTPUT

			// スクリーンショットが設定されていれば、画面のコピーを取る
			if (export_screen_texture_) {
				Path filename = export_screen_filename_;
				if (filename.empty()) {
					filename = DEFAULT_SNAPSHOT_FILENAME;
				}
				_ExportTextureImage(out_render_tex(), filename);
				Log_infof("Snapshot saved: %s", filename.getFullPath().u8());
				export_screen_texture_ = false;
			}

		}
	}

	// GUIを除く最前面への描画
	for (auto it=cblist_.begin(); it!=cblist_.end(); ++it) {
		IScreenCallback *r = *it;
		r->onRawRender();
	}

#ifndef NO_IMGUI
	if (ImGuiVideo_IsInitialized()) {
		int wnd_w = engine_->getStatus(Engine::ST_WINDOW_CLIENT_SIZE_W);
		int wnd_h = engine_->getStatus(Engine::ST_WINDOW_CLIENT_SIZE_H);
		RectF vp(0, 0, 1, 1);
		_SetViewportFit(vp, wnd_w, wnd_h);
		ImGuiVideo_BeginRender();
		{
			// ゲームGUIを更新
			for (auto it=cblist_.begin(); it!=cblist_.end(); ++it) {
				IScreenCallback *r = *it;
				r->onUpdateGui();
			}
			// デバッグGUIを更新
			if (inspector_) {
				inspector_->render(out_render_tex());
			}
		}
		ImGuiVideo_EndRender();
	} else {
		// ImGuiVideo が初期化されていないため GUI 更新しない
		// （ImGui を使うには事前に ImGuiVideo_Init を呼んでおく）
	}
#endif // !NO_IMGUI
}

bool Screen::render() {
	Log_assert(engine_);

	// 描画する
	if (Video::beginScene()) {
		renderScene();
		if (Video::endScene()) {
			return true;
		}
	}
	return false;
}
int Screen::getGameResolutionW() const {
	return resolution_x_;
}
int Screen::getGameResolutionH() const {
	return resolution_y_;
}
void Screen::setGameResolution(int w, int h) {
	if (raw_render_tex()) {
		Log_error("E_INTERNAL: Screen::setGameResolution() must be called before Screen::init()");
		return;
	}
	resolution_x_ = w;
	resolution_y_ = h;
}
/// フルスクリーンモードにしたときの解像度
/// フルスクリーンにできる解像度には制限があるため、
/// ウィンドウモードでの解像度とは異なる値を設定しなければならない場合がある
void Screen::setFullScreenResolution(int w, int h) {
	fullscreen_resolution_x_ = w;
	fullscreen_resolution_y_ = h;
}
/// ウィンドウクライアント座標（左上が原点、Y軸下向き、ピクセル単位）をスクリーン座標（中央原点、Ｙ軸上向き、右上座標をX,Y共に 1.0 とする）に変換する
Vec3 Screen::windowClientToScreenPoint(const Vec3 &p) const {
	RectF vp = getGameViewportRect();
	Vec3 out;
	float nx = Num_normalize_unclamped(p.x, vp.xmin, vp.xmax);
	float ny = Num_normalize_unclamped(p.y, vp.ymin, vp.ymax);
	out.x = Num_lerp_unclamped(-1.0f,  1.0f, nx);
	out.y = Num_lerp_unclamped( 1.0f, -1.0f, ny); // Ｙ軸反転。符号に注意
	out.z = p.z;
	return out;
}
/// ゲーム画面描画用のビューポートを返す。クライアントウィンドウの座標系で、左上原点、Y軸下向き
RectF Screen::getGameViewportRect() const {
#ifndef NO_IMGUI
	{
		if (inspector_ && !inspector_->isOverwrappingWindowMode() && inspector_->isVisible()) {
			// インスペクターのペイン内にゲーム画面を描画している
			return inspector_->getWorldViewport();
		}
	}
#endif // !NO_IMGUI

	// ウィンドウ内に直接ゲーム画面を描画している
	{
		RectF rect;
		int wnd_w = engine_->getStatus(Engine::ST_WINDOW_CLIENT_SIZE_W);
		int wnd_h = engine_->getStatus(Engine::ST_WINDOW_CLIENT_SIZE_H);

		int src_w = resolution_x_;
		int src_h = resolution_y_;

		int dst_w, dst_h;
		if (keep_aspect_) {
			float wnd_aspect = (float)wnd_w / wnd_h;
			float src_aspect = (float)src_w / src_h;
			if (wnd_aspect < src_aspect) {
				// 縦長の画面
				// 横をウィンドウサイズに合わせる
				dst_w = wnd_w;
				dst_h = src_h * wnd_w / src_w;
			} else {
				// 横長の画面
				// 縦をウィンドウサイズに合わせる
				dst_w = src_w * wnd_h / src_h;
				dst_h = wnd_h;
			}
		} else {
			dst_w = wnd_w;
			dst_h = wnd_h;
		}
		rect.xmin = (float)((wnd_w - dst_w) / 2);
		rect.ymin = (float)((wnd_h - dst_h) / 2);
		rect.xmax = rect.xmin + dst_w;
		rect.ymax = rect.ymin + dst_h;
		return rect;
	}
}
Material * Screen::getMaterial() {
	return &final_material_;
}
void Screen::setRenderTargetEnable(bool b) {
	use_render_texture_ = b;
}
bool Screen::isRenderTargetEnable() const {
	return use_render_texture_;
}
/// ゲーム画面描画用の独自GUIウィンドウがある場合は true を返す。
/// その場合、ウィンドウにゲーム画面を直接描画してはいけない
bool Screen::Engine_hasInternalGameWindow() {
#ifndef NO_IMGUI
	return inspector_ && inspector_->isVisible() && !inspector_->isOverwrappingWindowMode();
#else
	return false;
#endif
}
void Screen::call_onDrawDebugInfoSelected(const DebugRenderParameters &p) {
	Log_assert(engine_);
	KClock cl;
	for (auto it=cblist_.begin(); it!=cblist_.end(); ++it) {
		IScreenCallback *r = *it;
		cl.reset();
		r->onDrawDebugInfoSelected(p);
		engine_->addTimeConsumed(r, cl.getTimeMsec());
	}
}
void Screen::call_onDrawDebugInfo(const DebugRenderParameters &p) {
	Log_assert(engine_);
	KClock cl;
	for (auto it=cblist_.begin(); it!=cblist_.end(); ++it) {
		IScreenCallback *r = *it;
		cl.reset();
		r->onDrawDebugInfo(p);
		engine_->addTimeConsumed(r, cl.getTimeMsec());
	}
}


#pragma endregion // Screen

}