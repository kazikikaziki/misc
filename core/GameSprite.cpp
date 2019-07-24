#include "GameSprite.h"
//
#include "Engine.h"
#include "asset_path.h"
#include "inspector.h"
#include "VideoBank.h"
#include "KDrawList.h"
#include "KImgui.h"
#include "KLog.h"
#include "KNum.h"
#include "KStd.h"
#include "KToken.h"
#include "KVideo.h"
#include "mo_cstr.h"


#define MAX_SPRITE_LAYER_COUNT 16

namespace mo {


#pragma region SpriteRendererCo
SpriteRendererCo::SpriteRendererCo() {
	modifier_ = 0;
	gui_max_layers_ = 0;
	layer_count_ = 0;
	use_unpacked_texture_ = false;
}
VideoBank * SpriteRendererCo::getVideoBank() {
	return g_engine->videobank();
}
RenderSystem * SpriteRendererCo::getRS() {
	return g_engine->findSystem<RenderSystem>();
}
void SpriteRendererCo::copyFrom(const SpriteRendererCo *co) {
	Log_assert(co);
	EID this_e = entity();
	int numLayers = co->getLayerCount();

	// スプライトを全レイヤーコピーする
	setLayerCount(numLayers);
	for (int i=0; i<numLayers; i++) {
		setLayerSprite (i, co->sprite_layers_[i].sprite);
		setLayerVisible(i, co->sprite_layers_[i].visible);
		setLayerLabel  (i, co->sprite_layers_[i].label);
		setLayerOffset (i, co->sprite_layers_[i].offset);
		const Material *mat_src = co->getLayerMaterial(i);
		if (mat_src) {
			Material *mat_dst = getLayerMaterialAnimatable(i);
			*mat_dst = *mat_src;
		}
	}
	setModifier(co->getModifier());
	copyLayerVisibleFilters(co);
	Matrix4 m = getRS()->getLocalTransform(co->entity());
	getRS()->setLocalTransform(this_e, m);
}
int SpriteRendererCo::getLayerCount() const {
	return layer_count_;
}
void SpriteRendererCo::setModifier(int modifier) {
	modifier_ = modifier;
}
int SpriteRendererCo::getModifier() const {
	return modifier_;
}
// 指定されたインデックスのレイヤーが存在することを保証する
void SpriteRendererCo::guaranteeLayerIndex(int index) {
	Log_assert(0 <= index && index < MAX_SPRITE_LAYER_COUNT);
	// 一度確保したレイヤーは減らさない。
	// （アニメによってレイヤー枚数が変化する場合、レイヤーに割り当てられたマテリアルを
	// 毎回生成・削除しなくてもいいように維持してくため）
	int n = (int)sprite_layers_.size();
	if (n < index+1) {
		sprite_layers_.resize(index+1);

		// 新しく増えた領域を初期化しておく
		for (int i=n; i<index+1; i++) {
			sprite_layers_[i] = SpriteLayer();
		}
	}
	if (layer_count_ < index+1) {
		layer_count_ = index+1;

		// 無効化された領域を初期化しておく
		for (int i=layer_count_; i<n; i++) {
			sprite_layers_[i] = SpriteLayer();
		}
	}
}
void SpriteRendererCo::setUseUnpackedTexture(bool b) {
	use_unpacked_texture_ = b;
}
void SpriteRendererCo::setLayerCount(int count) {
	if (count > 0) {
		Log_assert(count <= MAX_SPRITE_LAYER_COUNT);
		layer_count_ = count;
		guaranteeLayerIndex(count - 1);
	} else {
		layer_count_ = 0;
	}
}
bool SpriteRendererCo::getLayerAabb(int index, Vec3 *minpoint, Vec3 *maxpoint) {
	const Sprite *sp = getLayerSprite(index);
	if (sp == NULL) return false;
	const KMesh *mesh = sp->getMesh();
	if (mesh == NULL) return false;
	Vec3 p0, p1;
	if (! mesh->getAabb(&p0, &p1)) return false;
	Vec3 sp_offset = sp->offset();

	Vec3 offset = getRS()->getOffset(entity());
	*minpoint = sp_offset + offset+ p0;
	*maxpoint = sp_offset + offset+ p1;
	return true;
}
const Path & SpriteRendererCo::getLayerSpritePath(int index) const {
	if (0 <= index && index < getLayerCount()) {
		return sprite_layers_[index].sprite;
	} else {
		static const Path s_def;
		return s_def;
	}
}
TEXID SpriteRendererCo::getBakedSpriteTexture(int index) {
	if (0 <= index && index < getLayerCount()) {
		return getVideoBank()->getBakedSpriteTexture(sprite_layers_[index].sprite, modifier_, false);
	}
	return NULL;
}
Sprite * SpriteRendererCo::getLayerSprite(int index) {
	const Path &path = getLayerSpritePath(index);
	if (! path.empty()) {
		SpriteBank *bank = getVideoBank()->getSpriteBank();
		bool should_be_found = true;
		return bank->find(path, should_be_found);
	} else {
		return NULL;
	}
}
void SpriteRendererCo::setLayerSprite(int index, Sprite *sprite) {
	guaranteeLayerIndex(index);
	sprite_layers_[index].sprite = sprite ? sprite->name_ : "";
}

void SpriteRendererCo::setLayerSprite(int index, const Path &asset_path) {
	if (asset_path.empty()) {
		// 無色透明画像とみなす
	} else if (asset_path.extension().compare(".sprite") != 0) {
		Log_errorf("E_INVALID_FILEPATH_FOR_SPRITE: %s. (NO FILE EXT .sprite)", asset_path.u8());
	}
	guaranteeLayerIndex(index);
	sprite_layers_[index].sprite = asset_path;
}
void SpriteRendererCo::setLayerLabel(int index, const Path &label) {
	guaranteeLayerIndex(index);
	sprite_layers_[index].label = label;
}
const Path & SpriteRendererCo::getLayerLabel(int index) const {
	if (0 <= index && index < getLayerCount()) {
		return sprite_layers_[index].label;
	}
	return Path::Empty;
}
void SpriteRendererCo::setLayerOffset(int index, const Vec3 &value) {
	guaranteeLayerIndex(index);
	sprite_layers_[index].offset = value;
}
Vec3 SpriteRendererCo::getLayerOffset(int index) const {
	if (0 <= index && index < getLayerCount()) {
		return sprite_layers_[index].offset;
	}
	return Vec3();
}
void SpriteRendererCo::setLayerVisible(int index, bool value) {
	guaranteeLayerIndex(index);
	sprite_layers_[index].visible = value;
}
bool SpriteRendererCo::isLayerVisible(int index) const {
	if (0 <= index && index < getLayerCount()) {
		return sprite_layers_[index].visible;
	}
	return false;
}
void SpriteRendererCo::clearLayerFilters() {
	sprite_filter_layer_labels_.clear();
}
void SpriteRendererCo::copyLayerVisibleFilters(const SpriteRendererCo *src) {
	sprite_filter_layer_labels_ = src->sprite_filter_layer_labels_;
}
/// labelw ラベル名。空文字列の場合は、どのラベル名ともマッチしない場合に適用される
void SpriteRendererCo::setLayerVisibleFilter(const Path &label, bool visible) {
	sprite_filter_layer_labels_[label] = visible;
}
bool SpriteRendererCo::layerWillBeRendered(int index) {
	if (index < 0) return false;
	if (getLayerCount() <= index) return false;
	const SpriteLayer &L = sprite_layers_[index];
	if (! L.visible) return false;
	if (L.sprite.empty()) return false;
	if (! sprite_filter_layer_labels_.empty()) {
		// フィルターが存在する場合は、その設定に従う
		auto it = sprite_filter_layer_labels_.find(L.label);
		if (it != sprite_filter_layer_labels_.end()) {
			if (! it->second) return false; // フィルターによって非表示に設定されている
		} else {
			// フィルターに登録されていないレイヤーの場合は、空文字列 "" をキーとする設定を探し、これをデフォルト値とする
			if (! sprite_filter_layer_labels_[""]) return false;
		}
	}
	return true;
}
/// アニメーション可能なマテリアルを得る
Material * SpriteRendererCo::getLayerMaterialAnimatable(int index) {
	if (index < 0) {
		return NULL;
	}
	if (index < (int)sprite_layers_.size()) {
		return &sprite_layers_[index].material;
	}
	sprite_layers_.resize(index + 1);
	return &sprite_layers_[index].material;
}
Material * SpriteRendererCo::getLayerMaterial(int index, Material *def) {
	if (0 <= index && index < getLayerCount()) {
		return &sprite_layers_[index].material;
	}
	return def;
}
const Material * SpriteRendererCo::getLayerMaterial(int index, const Material *def) const {
	if (0 <= index && index < getLayerCount()) {
		return &sprite_layers_[index].material;
	}
	return def;
}
void SpriteRendererCo::setLayerMaterial(int index, const Material &material) {
	guaranteeLayerIndex(index);
	sprite_layers_[index].material = material;
}
Vec3 SpriteRendererCo::bitmapToLocalPoint(int index, const Vec3 &p) {
	// スプライト情報を取得
	Sprite *sprite = getLayerSprite(index);
	if (sprite == NULL) return Vec3();

	// スプライトのpivot
	Vec2 sprite_pivot = sprite->pivot_;

	// レイヤーのオフセット量
	Vec3 layer_offset = getLayerOffset(0);

	// 元画像サイズ
	int bmp_h = sprite->image_h_;

	// ビットマップ左下基準での座標（Ｙ上向き）
	Vec3 bmp_local(p.x, bmp_h - p.y, p.z);

	// エンティティローカル座標
	Vec3 local(
		layer_offset.x + bmp_local.x - sprite_pivot.x,
		layer_offset.y + bmp_local.y - sprite_pivot.y,
		layer_offset.z + bmp_local.z);
	
	return local;
}
Vec3 SpriteRendererCo::localToBitmapPoint(int index, const Vec3 &p) {
	// スプライト情報を取得
	Sprite *sprite = getLayerSprite(index);
	if (sprite == NULL) return Vec3();

	// スプライトのpivot
	Vec2 sprite_pivot = sprite->pivot_;

	// レイヤーのオフセット量
	Vec3 layer_offset = getLayerOffset(0);

	// 元画像サイズ
	int bmp_h = sprite->image_h_;

	// ビットマップ左下基準での座標（Ｙ上向き）
	Vec3 bmp_local(
		p.x - layer_offset.x + sprite_pivot.x,
		p.y - layer_offset.y + sprite_pivot.y,
		p.z - layer_offset.z);
		
	// ビットマップ左上基準での座標（Ｙ下向き）
	Vec3 pos(
		bmp_local.x, 
		bmp_h - bmp_local.y,
		bmp_local.z);

	return pos;
}
static Vec3 vec3_min(const Vec3 &a, const Vec3 &b) {
	return Vec3(
		Num_min(a.x, b.x),
		Num_min(a.y, b.y),
		Num_min(a.z, b.z));
}
static Vec3 vec3_max(const Vec3 &a, const Vec3 &b) {
	return Vec3(
		Num_max(a.x, b.x),
		Num_max(a.y, b.y),
		Num_max(a.z, b.z));
}
bool SpriteRendererCo::getAabb(Vec3 *min_point, Vec3 *max_point) {
	bool found = false;
	Vec3 m0, m1;
	int n = getLayerCount();
	for (int i=0; i<n; i++) {
		Vec3 off = getLayerOffset(i);
		Sprite *sp = getLayerSprite(i);
		if (sp) {
			Vec3 roff = sp->offset();
			const KMesh *mesh = sp->getMesh(false);
			if (mesh) {
				Vec3 p0, p1;
				if (mesh->getAabb(&p0, &p1)) {
					p0 += roff + off;
					p1 += roff + off;
					m0 = vec3_min(p0, m0);
					m1 = vec3_max(p1, m1);
					found = true;
				}
			}
		}
	}
	if (found) {
		if (min_point) *min_point = m0;
		if (max_point) *max_point = m1;
		return true;
	}
	return false;
}
void SpriteRendererCo::onQueryGroupingImageSize(int *w, int *h, Vec3 *pivot) {
	// スプライトに使われる各レイヤー画像がすべて同じ大きさであると仮定する
	Sprite *sp = getLayerSprite(0);
	if (sp) {
		int img_w = sp->image_w_;
		int img_h = sp->image_h_;
		// スプライトサイズが奇数でない場合は、偶数に補正する。
		// グループ化描画する場合、レンダーターゲットが奇数だとドットバイドットでの描画が歪む
		if (img_w % 2 == 1) img_w = (int)ceilf(img_w / 2.0f) * 2; // img_w/2*2 はダメ。img_w==1で破綻する
		if (img_h % 2 == 1) img_h = (int)ceilf(img_h / 2.0f) * 2; // img_h/2*2 はダメ。img_h==1で破綻する
		if (w) *w = img_w;
		if (h) *h = img_h;
		if (pivot) {
			Vec2 piv2d = sp->pivot_;
			if (sp->pivot_in_pixels) {
				pivot->x = piv2d.x;
				pivot->y = piv2d.y;
				pivot->z = 0;
			} else {
				pivot->x = img_w * piv2d.x;
				pivot->y = img_h * piv2d.y;
				pivot->z = 0;
			}
		}
	}
}
// フィルターの設定を得る
bool SpriteRendererCo::isVisibleLayerLabel(const Path &label) const {

	if (sprite_filter_layer_labels_.empty()) {
		// レイヤー名によるフィルターが指定されていない。
		// 無条件で表示可能とする
		return true;
	}

	// フィルターが存在する場合は、その設定に従う
	{
		auto it = sprite_filter_layer_labels_.find(label);
		if (it != sprite_filter_layer_labels_.end()) {
			return it->second;
		}
	}

	// 指定されたレイヤー名がフィルターに登録されていなかった。
	// 空文字列 "" をキーとする設定がデフォルト値なので、その設定に従う
	{
		auto it = sprite_filter_layer_labels_.find(Path::Empty);
		if (it != sprite_filter_layer_labels_.end()) {
			return it->second;
		}
	}

	// デフォルト値が指定されていない。
	// 無条件で表示可能とする
	return true;
}


// スプライトレイヤーとして描画される画像を、１枚のテクスチャに収める
// target 描画先レンダーテクスチャ
// transform 描画用変形行列。単位行列の場合、レンダーテクスチャ中央を原点とする、ピクセル単位の座標系になる（Ｙ軸上向き）
void SpriteRendererCo::unpackInTexture(TEXID target, const Matrix4 &transform, const KMesh &mesh, TEXID texid) {
	RenderSystem *rs = g_engine->findSystem<RenderSystem>();
	EID this_e = entity();
	bool adj_snap = rs->getFlag(this_e, RenderSystem::ADJ_SNAP);
	bool adj_half = rs->getFlag(this_e, RenderSystem::ADJ_HALF);

	int texw=0, texh=0;
	Video::getTextureSize(target, &texw, &texh);

	// レンダーターゲットのサイズが奇数になっていると、ドットバイドットでの描画が歪む
	Log_assert(texw>0 && texw%2==0);
	Log_assert(texh>0 && texh%2==0);

	RenderParams group_params;
	group_params.projection_matrix = Matrix4::fromOrtho((float)texw, (float)texh, (float)(-1000), (float)1000);
	group_params.transform_matrix = transform;
	
	Video::pushRenderTarget(target);
	Video::clearColor(Color::ZERO);
	Video::clearDepth(1.0f);
	Video::clearStencil(0);
	{
		RenderParams params = group_params; // Copy

		// ピクセルパーフェクト
		if (MO_VIDEO_ENABLE_HALF_PIXEL) {
			// 前提として、半ピクセルずらす前の座標は整数でないといけない
			if (rs->master_adj_snap_ && adj_snap) mo::translateFloorInPlace(params.transform_matrix);
			if (rs->master_adj_half_ && adj_half) mo::translateHalfPixelInPlace(params.transform_matrix);
		}

		// マテリアル
		// テクスチャの内容をそのまま転送するだけ
		Material mat;
		mat.blend = Blend_One;
		mat.color = Color32::WHITE;
		mat.specular = Color32::ZERO;
		mat.texture = texid;

		// 描画
		{
			Video::pushRenderState();
			Video::setProjection(params.projection_matrix);
			Video::setTransform(params.transform_matrix);
			Video::beginMaterial(&mat);
			Video::drawMesh(&mesh, 0/*submesh_index*/);
			Video::endMaterial(&mat);
			Video::popRenderState();
		}
	}
	Video::popRenderTarget();
}

void SpriteRendererCo::drawInTexture(const RenderLayer *layers, int num_nodes, TEXID target, int w, int h, const Matrix4 &transform) const {
	RenderSystem *rs = g_engine->findSystem<RenderSystem>();
	EID this_e = entity();

	// レンダーターゲットのサイズが奇数になっていると、ドットバイドットでの描画が歪む
	Log_assert(w>0 && w%2==0);
	Log_assert(h>0 && h%2==0);

	bool adj_snap = rs->getFlag(this_e, RenderSystem::ADJ_SNAP);
	bool adj_half = rs->getFlag(this_e, RenderSystem::ADJ_HALF);

	RenderParams group_params;
	group_params.projection_matrix = Matrix4::fromOrtho((float)w, (float)h, (float)(-1000), (float)1000);
	group_params.transform_matrix = transform;
	
	Video::pushRenderTarget(target);
	Video::clearColor(Color::ZERO);
	Video::clearDepth(1.0f);
	Video::clearStencil(0);
	for (int i=0; i<num_nodes; i++) {
		const RenderLayer &layer = layers[i];
		RenderParams params = group_params; // Copy

		// オフセット
		// いちどレンダーターゲットを経由するので、どちらの描画時に適用するオフセットなのか注意すること
		if (1) {
			params.transform_matrix = Matrix4::fromTranslation(layer.offset) * params.transform_matrix;
		}

		// ピクセルパーフェクト
		if (MO_VIDEO_ENABLE_HALF_PIXEL) {
			// 前提として、半ピクセルずらす前の座標は整数でないといけない
			if (rs->master_adj_snap_ && adj_snap) mo::translateFloorInPlace(params.transform_matrix);
			if (rs->master_adj_half_ && adj_half) mo::translateHalfPixelInPlace(params.transform_matrix);
		}

		// 描画
		{
			Video::pushRenderState();
			Video::setProjection(params.projection_matrix);
			Video::setTransform(params.transform_matrix);
			Video::beginMaterial(&layer.material);
			Video::drawMesh(&layer.mesh, 0/*submesh_index*/);
			Video::endMaterial(&layer.material);
			Video::popRenderState();
		}
	}
	Video::popRenderTarget();

}
void SpriteRendererCo::drawWithMeshRenderer() {
	RenderSystem *rs = getRS();
	SpriteBank *sbank = getVideoBank()->getSpriteBank();
	TextureBank *tbank = getVideoBank()->getTextureBank();
	EID this_e = entity();

	bool adj_snap = rs->getFlag(this_e, RenderSystem::ADJ_SNAP);
	bool adj_half = rs->getFlag(this_e, RenderSystem::ADJ_HALF);

	// 実際に描画するべきレイヤーを、描画するべき順番で取得する
	render_layers_.clear();

	// レイヤーを番号の大きなほうから順番に描画する
	// なお、sprite_layers は実際に描画する枚数よりも余分に確保されている場合があるため、
	// スプライトレイヤー枚数の取得に sprite_layers_.size() を使ってはいけない。
	for (int i=layer_count_-1; i>=0; i--) {
		const SpriteLayer &spritelayer = sprite_layers_[i];

		// 非表示レイヤーを除外
		if (!spritelayer.visible) {
			continue;
		}
		// スプライト未指定なら除外
		if (spritelayer.sprite.empty()) {
			continue;
		}
		// レイヤー名による表示設定を確認
		if (! isVisibleLayerLabel(spritelayer.label)) {
			continue;
		}
		// スプライトを取得
		Sprite *sprite = sbank->find(spritelayer.sprite, true);
		if (sprite == NULL) {
			continue;
		}

		// 描画に使うテクスチャとメッシュを取得
		RenderLayer renderlayer;
		preparateMeshAndTextureForSprite(sprite, spritelayer.sprite, modifier_, &renderlayer.texid, &renderlayer.mesh);

		if (renderlayer.mesh.getVertexCount() == 0) {
			continue; // メッシュなし
		}

		if (use_unpacked_texture_) {
			// レイヤーごとにレンダーターゲットに展開する
			Path tex_path = AssetPath::makeUnpackedTexturePath(spritelayer.sprite);
			TEXID target_tex = tbank->findTextureRaw(tex_path, false);

			// レンダーテクスチャーを取得
			int texW;
			int texH;
			rs->getGroupingImageSize(this_e, &texW, &texH, NULL);
			if (target_tex == NULL) {
				target_tex = tbank->addRenderTexture(tex_path, texW, texH, TextureBank::F_OVERWRITE_SIZE_NOT_MATCHED);
			}

			// レンダーテクスチャーにレイヤー内容を書き出す

			// スプライトのメッシュはY軸上向きに合わせてある。
			// レンダーテクスチャの左下を原点にする
			Vec3 pos;
			pos.x -= texW / 2;
			pos.y -= texH / 2;
			Matrix4 transform = Matrix4::fromTranslation(pos); // 描画位置
			unpackInTexture(target_tex, transform, renderlayer.mesh, renderlayer.texid);

			// レンダーテクスチャをスプライトとして描画するためのメッシュを作成する
			KMesh newmesh;
			int w = sprite->image_w_;
			int h = sprite->image_h_;
			float u0 = 0.0f;
			float u1 = (float)sprite->image_w_ / texW;
			float v0 = (float)(texH-sprite->image_h_) / texH;
			float v1 = 1.0f;
			MeshShape::makeRect(&newmesh, 0, 0, w, h, u0, v0, u1, v1, Color::WHITE);

			// 得られたレンダーターゲットをスプライトとして描画する
			renderlayer.mesh = newmesh; //<--新しいメッシュに変更
			renderlayer.texid = target_tex; //<-- レイヤーを描画したレンダーテクスチャに変更
			renderlayer.offset = spritelayer.offset + sprite->offset();
			renderlayer.index = i;
			renderlayer.material = spritelayer.material; // COPY
			renderlayer.material.texture = renderlayer.texid;
			render_layers_.push_back(renderlayer);

		} else {
			// 展開なし。描画と同時に展開する
			renderlayer.offset = spritelayer.offset + sprite->offset();
			renderlayer.index = i;
			renderlayer.material = spritelayer.material; // COPY
			renderlayer.material.texture = renderlayer.texid;
			render_layers_.push_back(renderlayer);
		}
	}

	int total_vertices = 0;
	for (size_t i=0; i<render_layers_.size(); i++) {
		const RenderLayer &renderlayer = render_layers_[i];
		total_vertices += renderlayer.mesh.getVertexCount();
	}

	KMesh *mesh = rs->getMesh(this_e);
	mesh->clear();
	Vertex *v = mesh->lock(0, total_vertices);
	int offset = 0;
	int isub = 0;
	for (size_t i=0; i<render_layers_.size(); i++) {
		const RenderLayer &renderlayer = render_layers_[i];
		int count = renderlayer.mesh.getVertexCount();
		Log_assert(offset + count <= total_vertices);
		memcpy(v + offset, renderlayer.mesh.getVertices(), sizeof(Vertex)*count);
		KSubMesh laysub;
		renderlayer.mesh.getSubMesh(0, &laysub);

		KSubMesh sub;
		sub.start = offset + laysub.start;
		sub.count = laysub.count;
		sub.blend = laysub.blend;
		sub.primitive = laysub.primitive;
		mesh->addSubMesh(sub);

		Material *m = rs->getSubMeshMaterial(this_e, isub);
		*m = renderlayer.material;

		Matrix4 tr = Matrix4::fromTranslation(renderlayer.offset);
		rs->setSubMeshTransform(this_e, isub, tr);

		offset += count;
		isub++;
	}
	mesh->unlock();

	if (rs->isGrouping(this_e)) {
		int w=0, h=0;
		Vec3 pivot;
		onQueryGroupingImageSize(&w, &h, &pivot);
		rs->setGroupingImageSize(this_e, w, h, pivot);
	}
}

void SpriteRendererCo::draw(RenderSystem *rs, const RenderParams &render_params, DrawList *drawlist) {
	SpriteBank *sbank = getVideoBank()->getSpriteBank();
	TextureBank *tbank = getVideoBank()->getTextureBank();
	EID this_e = entity();

	bool adj_snap = rs->getFlag(this_e, RenderSystem::ADJ_SNAP);
	bool adj_half = rs->getFlag(this_e, RenderSystem::ADJ_HALF);

	// 実際に描画するべきレイヤーを、描画するべき順番で取得する
	render_layers_.clear();

	// レイヤーを番号の大きなほうから順番に描画する
	// なお、sprite_layers は実際に描画する枚数よりも余分に確保されている場合があるため、
	// スプライトレイヤー枚数の取得に sprite_layers_.size() を使ってはいけない。
	for (int i=layer_count_-1; i>=0; i--) {
		const SpriteLayer &spritelayer = sprite_layers_[i];

		// 非表示レイヤーを除外
		if (!spritelayer.visible) {
			continue;
		}
		// スプライト未指定なら除外
		if (spritelayer.sprite.empty()) {
			continue;
		}
		// レイヤー名による表示設定を確認
		if (! isVisibleLayerLabel(spritelayer.label)) {
			continue;
		}
		// スプライトを取得
		Sprite *sprite = sbank->find(spritelayer.sprite, true);
		if (sprite == NULL) {
			continue;
		}

		// 描画に使うテクスチャとメッシュを取得
		RenderLayer renderlayer;
		preparateMeshAndTextureForSprite(sprite, spritelayer.sprite, modifier_, &renderlayer.texid, &renderlayer.mesh);

		if (renderlayer.mesh.getVertexCount() == 0) {
			continue; // メッシュなし
		}

		if (use_unpacked_texture_) {
			// レイヤーごとにレンダーターゲットに展開する
			Path tex_path = AssetPath::makeUnpackedTexturePath(spritelayer.sprite);
			TEXID target_tex = tbank->findTextureRaw(tex_path, false);

			// レンダーテクスチャーを取得
			int texW;
			int texH;
			rs->getGroupingImageSize(this_e, &texW, &texH, NULL);
			if (target_tex == NULL) {
				target_tex = tbank->addRenderTexture(tex_path, texW, texH, TextureBank::F_OVERWRITE_SIZE_NOT_MATCHED);
			}

			// レンダーテクスチャーにレイヤー内容を書き出す

			// スプライトのメッシュはY軸上向きに合わせてある。
			// レンダーテクスチャの左下を原点にする
			Vec3 pos;
			pos.x -= texW / 2;
			pos.y -= texH / 2;
			Matrix4 transform = Matrix4::fromTranslation(pos); // 描画位置
			unpackInTexture(target_tex, transform, renderlayer.mesh, renderlayer.texid);

			// レンダーテクスチャをスプライトとして描画するためのメッシュを作成する
			KMesh newmesh;
			int w = sprite->image_w_;
			int h = sprite->image_h_;
			float u0 = 0.0f;
			float u1 = (float)sprite->image_w_ / texW;
			float v0 = (float)(texH-sprite->image_h_) / texH;
			float v1 = 1.0f;
			MeshShape::makeRect(&newmesh, 0, 0, w, h, u0, v0, u1, v1, Color::WHITE);

			// 得られたレンダーターゲットをスプライトとして描画する
			renderlayer.mesh = newmesh; //<--新しいメッシュに変更
			renderlayer.texid = target_tex; //<-- レイヤーを描画したレンダーテクスチャに変更
			renderlayer.offset = spritelayer.offset + sprite->offset();
			renderlayer.index = i;
			renderlayer.material = spritelayer.material; // COPY
			renderlayer.material.texture = renderlayer.texid;
			render_layers_.push_back(renderlayer);

		} else {
			// 展開なし。描画と同時に展開する
			renderlayer.offset = spritelayer.offset + sprite->offset();
			renderlayer.index = i;
			renderlayer.material = spritelayer.material; // COPY
			renderlayer.material.texture = renderlayer.texid;
			render_layers_.push_back(renderlayer);
		}
	}

	// マスターカラー
	Color master_col = rs->getMasterColor(this_e);
	Color master_spe = rs->getMasterSpecular(this_e);

	// 親の設定を継承する場合は合成マスターカラーを得る
	if (rs->getFlag(this_e, RenderSystem::INHERIT_COLOR)) {
		master_col = rs->getMasterColorInTree(this_e);
	}
	if (rs->getFlag(this_e, RenderSystem::INHERIT_SPECULAR)) {
		master_spe = rs->getMasterSpecularInTree(this_e);
	}

	if (rs->isGrouping(this_e)) {
		// グループ化する。
		// 一旦別のレンダーテクスチャに書き出し、それをスプライトとして描画する
		int groupW;
		int groupH;
		Vec3 groupPiv;
		rs->getGroupingImageSize(this_e, &groupW, &groupH, &groupPiv);
		TEXID group_tex = rs->getGroupRenderTexture(this_e);
		Matrix4 local_transform = rs->getLocalTransform(this_e); // 画像用の変形

		// groupPivはビットマップ原点（＝レンダーターゲット画像左上）を基準にしていることに注意
		Vec3 pos;
		pos.x -= groupW / 2; // レンダーテクスチャ左上へ
		pos.y += groupH / 2;
		pos.x += groupPiv.x; // さらに groupPivずらす
		pos.y -= groupPiv.y;
		Matrix4 group_transform = Matrix4::fromTranslation(pos) * local_transform; // 描画位置

		// 描画
		drawInTexture(render_layers_.data(), render_layers_.size(), group_tex, groupW, groupH, group_transform);

		// グループ化描画終わり。ここまでで render_target テクスチャにはスプライトの絵が描画されている。
		// これを改めて描画する
		{
			RenderParams params = render_params; // Copy

			// オフセット
			{
				Vec3 off;
				off += rs->getOffset(this_e); // 一括オフセット。すべてのレイヤーが影響を受ける
			//	off += layer.offset; // レイヤーごとに指定されたオフセット
			//	off += sprite->offset(); // スプライトに指定された Pivot 座標によるオフセット
				off.x += -groupPiv.x; // レンダーターゲット内のピヴォットが、通常描画時の原点と一致するように調整する
				off.y +=  groupPiv.y - groupH;
				params.transform_matrix = Matrix4::fromTranslation(off) * params.transform_matrix;
			}

			// ピクセルパーフェクト
			if (MO_VIDEO_ENABLE_HALF_PIXEL) {
				// 前提として、半ピクセルずらす前の座標は整数でないといけない
				if (rs->master_adj_snap_ && adj_snap) mo::translateFloorInPlace(params.transform_matrix);
				if (rs->master_adj_half_ && adj_half) mo::translateHalfPixelInPlace(params.transform_matrix);
			}

			// マテリアル
			Material *p_mat = rs->getGroupingMaterial(this_e);
			Log_assert(p_mat);
			Material mat = *p_mat; // COPY
			mat.texture = group_tex;

			// マスターカラーと合成する
			mat.color    *= master_col; // MUL
			mat.specular += master_spe; // ADD

			// 描画
			KMesh *groupMesh = rs->getGroupRenderMesh(this_e);
			if (drawlist) {
				drawlist->setProjection(params.projection_matrix);
				drawlist->setTransform(params.transform_matrix);
				drawlist->setMaterial(mat);
				drawlist->addMesh(groupMesh, 0);
			} else {
				Video::pushRenderState();
				Video::setProjection(params.projection_matrix);
				Video::setTransform(params.transform_matrix);
				Video::beginMaterial(&mat);
				Video::drawMesh(groupMesh, 0);
				Video::endMaterial(&mat);
				Video::popRenderState();
			}
		}
		return;
	}
	
	// グループ化しない。
	// 通常描画する
	for (size_t i=0; i<render_layers_.size(); i++) {
		const RenderLayer &renderlayer = render_layers_[i];

		RenderParams params = render_params; // Copy

		// 画像用の変形を適用
		params.transform_matrix = rs->getLocalTransform(this_e) * params.transform_matrix;

		// オフセット
		{
			Vec3 off;
			off += rs->getOffset(this_e); // 一括オフセット。すべてのレイヤーが影響を受ける
			off += renderlayer.offset; // レイヤーごとのオフセット
			params.transform_matrix = Matrix4::fromTranslation(off) * params.transform_matrix;
		}

		// ピクセルパーフェクト
		if (MO_VIDEO_ENABLE_HALF_PIXEL) {
			// 前提として、半ピクセルずらす前の座標は整数でないといけない
			if (rs->master_adj_snap_ && adj_snap) mo::translateFloorInPlace(params.transform_matrix);
			if (rs->master_adj_half_ && adj_half) mo::translateHalfPixelInPlace(params.transform_matrix);
		}

		// マテリアル
		Material *p_mat = getLayerMaterialAnimatable(renderlayer.index);
		Log_assert(p_mat);
		Material mat = *p_mat; // COPY
		mat.texture = renderlayer.texid;

		// マスターカラーと合成する
		mat.color    *= master_col; // MUL
		mat.specular += master_spe; // ADD

		// 描画
		if (drawlist) {
			drawlist->setProjection(params.projection_matrix);
			drawlist->setTransform(params.transform_matrix);
			drawlist->setMaterial(mat);
			drawlist->addMesh(&renderlayer.mesh, 0);
		} else {
			Video::pushRenderState();
			Video::setProjection(params.projection_matrix);
			Video::setTransform(params.transform_matrix);
			Video::beginMaterial(&mat);
			Video::drawMesh(&renderlayer.mesh, 0/*submesh_index*/);
			Video::endMaterial(&mat);
			Video::popRenderState();
		}
	}
}
bool SpriteRendererCo::preparateMeshAndTextureForSprite(const Sprite *sprite, const Path &sprite_name, int modifier, TEXID *tex, KMesh *mesh) {
	EID this_e = entity();

	// テクスチャを取得。なければ作成
	TEXID actual_tex = getVideoBank()->getTextureEx(sprite->texture_name_, modifier, true, this_e);

	if (tex) *tex = actual_tex;
	*mesh = sprite->mesh_;
	return true;
}

#ifndef NO_IMGUI
bool ImGui_TreeNodeColored(const void *ptr_id, const ImVec4& col, const char *fmt, ...) {
	ImGui::PushStyleColor(ImGuiCol_Text, col);
	va_list args;
	va_start(args, fmt);
	bool is_open = ImGui::TreeNodeExV(ptr_id, 0, fmt, args);
	va_end(args);
	ImGui::PopStyleColor();
	return is_open;
}

#endif

void SpriteRendererCo::updateInspectorGui() {
#ifndef NO_IMGUI
	EID this_e = entity();

	SpriteBank *sprite_bank = getVideoBank()->getSpriteBank();

	// 同一クリップ内でレイヤー枚数が変化する場合、
	// インスペクターの表示レイヤーがフレーム単位で増えたり減ったりして安定しない。
	// 見やすくするために、これまでの最大レイヤー数の表示領域を確保しておく
	gui_max_layers_ = Num_max(gui_max_layers_, getLayerCount());
	for (int i=0; i<gui_max_layers_; i++) {
		ImGui::PushID(0);
		if (i < getLayerCount()) {
			ImVec4 color = isLayerVisible(i) ? GuiColor_ActiveText : GuiColor_DeactiveText;
			if (ImGui_TreeNodeColored(ImGui_ID(i), color, "Layer[%d]: %s", i, sprite_layers_[i].sprite.u8())) {
				ImGui::Text("Label: %s", sprite_layers_[i].label.u8());
				sprite_bank->guiSpriteSelector("Sprite", &sprite_layers_[i].sprite);

				ImGui::Text("Will be rendered: %s", layerWillBeRendered(i) ? "Yes" : "No");
				bool b = isLayerVisible(i);
				if (ImGui::Checkbox("Visible", &b)) {
					setLayerVisible(i, b);
				}
				Vec3 p = getLayerOffset(i);
				if (ImGui::DragFloat3("Offset", (float*)&p, 1)) {
					setLayerOffset(i, p);
				}
				if (ImGui::TreeNode(ImGui_ID(0), "Sprite: %s", sprite_layers_[i].sprite.u8())) {
					sprite_bank->guiSpriteInfo(sprite_layers_[i].sprite, getVideoBank());
					ImGui::TreePop();
				}
				if (ImGui::TreeNode(ImGui_ID(1), "Material")) {
					Material *mat = getLayerMaterial(i);
					Data_guiMaterialInfo(mat);
					ImGui::TreePop();
				}
				ImGui::TreePop();
			}
		} else {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.2f));
			if (ImGui::TreeNode(ImGui_ID(i), "Layer[%d]: (none)", i)) {
				ImGui::TreePop();
			}
			ImGui::PopStyleColor();
		}
		ImGui::PopID();
	}
	if (ImGui::TreeNode(ImGui_ID(-1), "Layer filters")) {
		for (auto it=sprite_filter_layer_labels_.begin(); it!=sprite_filter_layer_labels_.end(); ++it) {
			if (it->first.empty()) {
				ImGui::Checkbox("(Default)", &it->second);
			} else {
				ImGui::Checkbox(it->first.u8(), &it->second);
			}
		}
		ImGui::TreePop();
	}
	ImGui::Checkbox("Use unpacked texture", &use_unpacked_texture_);
/*
	ImGui::Checkbox(u8"Unpack/アンパック", &use_unpacked_texture_);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			u8"スプライトとして表示するテクスチャーがブロック圧縮されている場合、\n"
			u8"既定の設定では描画と展開を同時に行いますが、この項目にチェックを付けた場合は、\n"
			u8"一度レンダーテクスチャーに画像を展開してから描画します。\n"
			u8"シェーダーやUVアニメが正しく反映されない場合、ここにチェックを入れると改善される場合があります\n"
			u8"ブロック圧縮されていないテクスチャーを利用している場合、このオプションは無視されます"
		);
	}
*/
	if (ImGui::TreeNode(ImGui_ID(-2), "Test")) {
		ImGui::Text("SkewTest");
		if (ImGui::Button("-20")) {
			getRS()->setLocalTransform(this_e, Matrix4::fromSkewX(-20));
		}
		ImGui::SameLine();
		if (ImGui::Button("-10")) {
			getRS()->setLocalTransform(this_e, Matrix4::fromSkewX(-10));
		}
		ImGui::SameLine();
		if (ImGui::Button("0")) {
			getRS()->setLocalTransform(this_e, Matrix4::fromSkewX(0));
		}
		ImGui::SameLine();
		if (ImGui::Button("10")) {
			getRS()->setLocalTransform(this_e, Matrix4::fromSkewX(10));
		}
		ImGui::SameLine();
		if (ImGui::Button("20")) {
			getRS()->setLocalTransform(this_e, Matrix4::fromSkewX(20));
		}
		ImGui::TreePop();
	}
#endif // !NO_IMGUI
}
#pragma endregion // SpriteRendererCo














SpriteSystem::SpriteSystem() {
	rs_ = NULL;
}
void SpriteSystem::init(Engine *engine) {
	engine->addSystem(this);
}
void SpriteSystem::onStart() {
	rs_ = g_engine->findSystem<RenderSystem>();
}
void SpriteSystem::onEnd() {
	rs_ = NULL;
}
void SpriteSystem::attach(EID e) {
	Log_assert(rs_);

#if SS_TEST
	// 必ず MeshRenderer もセットでアタッチする
	rs_->attachMesh(e);
	rs_->attachSprite(e);
	nodes_.insert(e);
#else
	rs_->attachSprite(e);
#endif
}
bool SpriteSystem::onAttach(EID e, const char *type) {
	if (strcmp(type, "SpriteRenderer") == 0) {
		attach(e);
		return true;
	}
	return false;
}
void SpriteSystem::onDetach(EID e) {
	nodes_.erase(e);
}
bool SpriteSystem::onIsAttached(EID e) {
	return getNode(e) != NULL;

}
bool SpriteSystem::setParameter(EID e, const char *key, const char *val) {
	if (getNode(e) == NULL) {
		return false;
	}
	// レイヤーごとのスプライト、マテリアル設定
	// <SpriteRenderer
	//		layer-0.sprite   = "fg.sprite"
	//		layer-0.material = "fg.material"
	//		layer-0.visible  = 1
	//		layer-1.sprite   = "bg.sprite"
	//		layer-1.material = "bg.material"
	//		layer-1.visible  = 1
	//	/>
	// "layer-0.XXX"
	KToken tok(key, "-.");
	if (tok.compare(0, "layer") == 0) {
		int index = K_strtoi(tok.get(1, "-1"));
		if (index < 0) {
			Log_warningf("Invalid layer index '%s'", key);
			return false;
		}
		const char *subkey = tok.get(2, NULL);
		if (subkey == NULL) {
			Log_warningf("Invalid subkey '%s'", key);
			return false;
		}
		// "layer-0.sprite"
		if (strcmp(subkey, "sprite") == 0) {
			setLayerSprite(e, index, val);
			return true;
		}
		// "layer-0.visible"
		if (strcmp(subkey, "visible") == 0) {
			setLayerVisible(e, index, K_strtoi(val));
			return true;
		}
		// "layer-0.name"
		if (strcmp(subkey, "name") == 0) {
			setLayerLabel(e, index, val);
			return true;
		}
	}

	// 互換性。MeshRenderer.XXX は SpriteRenderer.XXX でもアクセスできるように
	// ここは削除したい。
	if (rs_->setParameter(e, key, val)) {
		return true;
	}

	return false;
}
void SpriteSystem::onAppRenderUpdate() {
#if SS_TEST
	for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
		EID e = *it;
		SpriteRendererCo *co = getNode(e);
		if (co) {
			co->drawWithMeshRenderer();
		}
	}
#endif
}
void SpriteSystem::onFrameLateUpdate() {
}


const SpriteRendererCo * SpriteSystem::getNode(EID e) const {
	Log_assert(rs_);
	return rs_->getSpriteRenderer(e);
}
SpriteRendererCo * SpriteSystem::getNode(EID e) {
	Log_assert(rs_);
	return rs_->getSpriteRenderer(e);
}
void SpriteSystem::copyFrom(EID e, const SpriteRendererCo *orhter) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->copyFrom(orhter);
}
void SpriteSystem::setLayerCount(EID e, int count) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->setLayerCount(count);
}
int SpriteSystem::getLayerCount(EID e) const {
	const SpriteRendererCo *co = getNode(e);
	return co ? co->getLayerCount() : 0;
}
bool SpriteSystem::getLayerAabb(EID e, int index, Vec3 *minpoint, Vec3 *maxpoint) {
	SpriteRendererCo *co = getNode(e);
	return co ? co->getLayerAabb(index, minpoint, maxpoint) : false;
}
void SpriteSystem::setLayerLabel(EID e, int index, const Path &label) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->setLayerLabel(index, label);
}
const Path & SpriteSystem::getLayerLabel(EID e, int index) const {
	const SpriteRendererCo *co = getNode(e);
	return co ? co->getLayerLabel(index) : Path::Empty;
}
void SpriteSystem::setLayerOffset(EID e, int index, const Vec3 &value) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->setLayerOffset(index, value);
}
Vec3 SpriteSystem::getLayerOffset(EID e, int index) const {
	const SpriteRendererCo *co = getNode(e);
	return co ? co->getLayerOffset(index) : Vec3();
}
const Path & SpriteSystem::getLayerSpritePath(EID e, int index) const {
	const SpriteRendererCo *co = getNode(e);
	return co ? co->getLayerSpritePath(index) : Path::Empty;
}
Sprite * SpriteSystem::getLayerSprite(EID e, int index) {
	SpriteRendererCo *co = getNode(e);
	return co ? co->getLayerSprite(index) : NULL;
}
void SpriteSystem::setLayerSprite(EID e, int index, Sprite *sprite) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->setLayerSprite(index, sprite);
}
void SpriteSystem::setLayerSprite(EID e, int index, const Path &name) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->setLayerSprite(index, name);
}
void SpriteSystem::setLayerVisible(EID e, int index, bool value) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->setLayerVisible(index, value);
}
bool SpriteSystem::isLayerVisible(EID e, int index) const {
	const SpriteRendererCo *co = getNode(e);
	return co ? co->isLayerVisible(index) : false;
}
void SpriteSystem::setLayerAlpha(EID e, int layer, float alpha) {
	Material *mat = getLayerMaterialAnimatable(e, layer);
	if (mat) mat->color.a = alpha;
}
void SpriteSystem::setLayerFilter(EID e, int layer, Filter f) {
	Material *mat = getLayerMaterialAnimatable(e, layer);
	if (mat) mat->filter = f;
}
void SpriteSystem::setLayerBlend(EID e, int layer, Blend blend) {
	Material *mat = getLayerMaterialAnimatable(e, layer);
	if (mat) mat->blend = blend;
}
void SpriteSystem::setLayerColor(EID e, int layer, const Color &color) {
	Material *mat = getLayerMaterialAnimatable(e, layer);
	if (mat) mat->color = color;
}
void SpriteSystem::setLayerSpecular(EID e, int layer, const Color &specular) {
	Material *mat = getLayerMaterialAnimatable(e, layer);
	if (mat) mat->specular = specular;
}
void SpriteSystem::setLayerCallback(EID e, int layer, IMaterialCallback *cb, void *data) {
	Material *mat = getLayerMaterialAnimatable(e, layer);
	if (mat) {
		mat->cb = cb;
		mat->cb_data = data;
	}
}
void SpriteSystem::clearLayerFilters(EID e) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->clearLayerFilters();
}
void SpriteSystem::setLayerVisibleByLabel(EID e, const Path &label, bool visible) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->setLayerVisibleFilter(label, visible);
}
void SpriteSystem::copyLayerVisibleFilters(EID e, const SpriteRendererCo *src) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->copyLayerVisibleFilters(src);
}
bool SpriteSystem::layerWillBeRendered(EID e, int index) {
	SpriteRendererCo *co = getNode(e);
	return co ? co->layerWillBeRendered(index) : false;
}
Material * SpriteSystem::getLayerMaterial(EID e, int index, Material *def) {
	SpriteRendererCo *co = getNode(e);
	return co ? co->getLayerMaterial(index, def) : NULL;
}
const Material * SpriteSystem::getLayerMaterial(EID e, int index, const Material *def) const {
	const SpriteRendererCo *co = getNode(e);
	return co ? co->getLayerMaterial(index, def) : NULL;
}
Material * SpriteSystem::getLayerMaterialAnimatable(EID e, int index) {
	SpriteRendererCo *co = getNode(e);
	return co ? co->getLayerMaterialAnimatable(index) : NULL;
}
void SpriteSystem::setLayerMaterial(EID e, int index, const Material &material) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->setLayerMaterial(index, material);
}
void SpriteSystem::setColor(EID e, const Color &color) {
	Log_assert(rs_);
	rs_->setMasterColor(e, color);
}
void SpriteSystem::setAlpha(EID e, float alpha) {
	Log_assert(rs_);
	rs_->setMasterAlpha(e, alpha);
}
void SpriteSystem::setSpecular(EID e, const Color &specular) {
	Log_assert(rs_);
	rs_->setMasterSpecular(e, specular);
}
TEXID SpriteSystem::getBakedSpriteTexture(EID e, int layer) {
	SpriteRendererCo *co = getNode(e);
	return co->getBakedSpriteTexture(layer);
}

/// 指定されたインデクスのスプライトの元画像ＢＭＰでの座標を、ローカル座標に変換する（テクスチャ座標ではない）
/// 例えばスプライトが player.png から作成されているなら、player.png 画像内の座標に対応するエンティティローカル座標を返す。
///
/// png などから直接生成したスプライト、アトラスにより切り取ったスプライト、それらをブロック化圧縮したスプライトに対応する。
/// それ以外の方法で作成されたスプライトでは正しく計算できない可能性がある。
///
/// index スプライト番号
/// bmp_x, bmp_y スプライトの作成元ビットマップ上の座標。原点は画像左上でＹ軸下向き。ピクセル単位。
///
Vec3 SpriteSystem::bitmapToLocalPoint(EID e, int index, const Vec3 &p) {
	SpriteRendererCo *co = getNode(e);
	return co ? co->bitmapToLocalPoint(index, p) : Vec3();
}
Vec3 SpriteSystem::localToBitmapPoint(EID e, int index, const Vec3 &p) {
	SpriteRendererCo *co = getNode(e);
	return co ? co->localToBitmapPoint(index, p) : Vec3();
}
bool SpriteSystem::RendererCo_getAabb(EID e, Vec3 *min_point, Vec3 *max_point) {
	SpriteRendererCo *co = getNode(e);
	return co ? co->getAabb(min_point, max_point) : false;
}
void SpriteSystem::RendererCo_onQueryGroupingImageSize(EID e, int *w, int *h, Vec3 *pivot) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->onQueryGroupingImageSize(w, h, pivot);
}
void SpriteSystem::RendererCo_draw(EID e, RenderSystem *rs, const RenderParams &render_params, DrawList *drawlist) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->draw(rs, render_params, drawlist);
}
void SpriteSystem::onEntityInspectorGui(EID e) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->updateInspectorGui();
}
void SpriteSystem::drawInTexture(EID e, const RenderLayer *nodes, int num_nodes, TEXID target, int w, int h, const Matrix4 &transform) const {
	const SpriteRendererCo *co = getNode(e);
	if (co) co->drawInTexture(nodes, num_nodes, target, w, h, transform);
}
void SpriteSystem::unpackInTexture(EID e, TEXID target, const Matrix4 &transform, const KMesh &mesh, TEXID texid) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->unpackInTexture(target, transform, mesh, texid);
}
bool SpriteSystem::preparateMeshAndTextureForSprite(EID e, const Sprite *sprite, const Path &sprite_name, int modifier, TEXID *tex, KMesh *mesh) {
	SpriteRendererCo *co = getNode(e);
	return co ? co->preparateMeshAndTextureForSprite(sprite, sprite_name, modifier, tex, mesh) : false;
}
void SpriteSystem::guaranteeLayerIndex(EID e, int index) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->guaranteeLayerIndex(index);
}
void SpriteSystem::setModifier(EID e, int modifier) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->setModifier(modifier);
}
int SpriteSystem::getModifier(EID e) const {
	const SpriteRendererCo *co = getNode(e);
	return co ? co->getModifier() : 0;
}
void SpriteSystem::setUseUnpackedTexture(EID e, bool b) {
	SpriteRendererCo *co = getNode(e);
	if (co) co->setUseUnpackedTexture(b);
}
bool SpriteSystem::isVisibleLayerLabel(EID e, const Path &label) const {
	const SpriteRendererCo *co = getNode(e);
	return co ? co->isVisibleLayerLabel(label) : false;
}



} // namespace
