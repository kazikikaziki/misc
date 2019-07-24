#pragma once
#include "GameRender.h"

namespace mo {

class Sprite;
class RenderSystem;

struct SpriteLayer {
	SpriteLayer() {
		visible = true;
	}
	Path sprite;
	Material material;
	Path label;
	Vec3 offset;
	bool visible;
};

struct RenderLayer {
	RenderLayer() {
		texid = NULL;
		index = 0;
	}
	int index;
	Vec3  offset;
	KMesh mesh;
	TEXID texid;
	Material material;
};


#define SS_TEST 0

class SpriteRendererCo
#if SS_TEST
	{
#else
	: public RendererCo {
#endif
public:
	SpriteRendererCo();
	void copyFrom(const SpriteRendererCo *co);
	void setLayerCount(int count);
	int  getLayerCount() const;
	bool getLayerAabb(int index, Vec3 *minpoint, Vec3 *maxpoint);
	void setLayerLabel(int index, const Path &label);
	const Path & getLayerLabel(int index) const;
	void setLayerOffset(int index, const Vec3 &value);
	Vec3 getLayerOffset(int index) const;
	const Path & getLayerSpritePath(int index) const;
	Sprite * getLayerSprite(int index=0);
	void setLayerSprite(int index, Sprite *sprite);
	void setLayerSprite(int index, const Path &asset_path);
	void setLayerVisible(int index, bool value);
	bool isLayerVisible(int index) const;
	void clearLayerFilters();
	void setLayerVisibleFilter(const Path &label, bool visible);
	void copyLayerVisibleFilters(const SpriteRendererCo *src);
	bool layerWillBeRendered(int index);
	Material * getLayerMaterial(int index=0, Material *def=NULL);
	const Material * getLayerMaterial(int index=0, const Material *def=NULL) const;
	Material * getLayerMaterialAnimatable(int index);
	void setLayerMaterial(int index, const Material &material);

	TEXID getBakedSpriteTexture(int layer);

	/// 指定されたインデクスのスプライトの元画像ＢＭＰでの座標を、ローカル座標に変換する（テクスチャ座標ではない）
	/// 例えばスプライトが player.png から作成されているなら、player.png 画像内の座標に対応するエンティティローカル座標を返す。
	///
	/// png などから直接生成したスプライト、アトラスにより切り取ったスプライト、それらをブロック化圧縮したスプライトに対応する。
	/// それ以外の方法で作成されたスプライトでは正しく計算できない可能性がある。
	///
	/// index スプライト番号
	/// bmp_x, bmp_y スプライトの作成元ビットマップ上の座標。原点は画像左上でＹ軸下向き。ピクセル単位。
	///
	Vec3 bitmapToLocalPoint(int index, const Vec3 &p);
	Vec3 localToBitmapPoint(int index, const Vec3 &p);

	void updateInspectorGui();

#if SS_TEST
	bool getAabb(Vec3 *min_point, Vec3 *max_point);
	void onQueryGroupingImageSize(int *w, int *h, Vec3 *pivot);
	void draw(RenderSystem *rs, const RenderParams &render_params, DrawList *drawlist);
#else
	virtual bool getAabb(Vec3 *min_point, Vec3 *max_point) override;
	virtual void onQueryGroupingImageSize(int *w, int *h, Vec3 *pivot) override;
	virtual void draw(RenderSystem *rs, const RenderParams &render_params, DrawList *drawlist) override;
#endif
	void drawWithMeshRenderer();

	void drawInTexture(const RenderLayer *nodes, int num_nodes, TEXID target, int w, int h, const Matrix4 &transform) const;
	void unpackInTexture(TEXID target, const Matrix4 &transform, const KMesh &mesh, TEXID texid);

	/// スプライト sprite を、この SpriteRendererCo の設定で描画するときに必要になるテクスチャやメッシュを準備する。
	/// 新しいアセットが必要なら構築し、アセットが未ロード状態ならばロードする。
	/// テクスチャアトラス、ブロック化圧縮、パレットの有無などが影響する
	///
	/// tex: NULL でない値を指定した場合、スプライトを描画するために実際に使用するテクスチャを取得できる
	/// mesh: NULL でない値をを指定した場合、スプライトを描画するために実際に使用するメッシュを取得できる
	///
	bool preparateMeshAndTextureForSprite(const Sprite *sprite, const Path &sprite_name, int modifier, TEXID *tex, KMesh *mesh);

	/// インデックス index のレイヤーが存在することを保証する
	/// 存在しなければ index+1 枚のレイヤーになるようレイヤーを追加する。
	/// 存在する場合は何もしない
	void guaranteeLayerIndex(int index);

	void setModifier(int index);
	int getModifier() const;

	/// ブロック化テクスチャーを使っている場合、復元したテクスチャーに対してマテリアルを適用するかどうか
	/// false の場合、画像復元とマテリアル適用を同時に行うため、正しく描画されない場合がある。
	/// true の場合、いったん元画像を復元してからマテリアルを適用する。
	/// 特に、処理対象外のピクセルを参照するようなエフェクトを使う場合は、復元されたテクスチャーに対して適用しないといけない
	void setUseUnpackedTexture(bool b);

	bool isVisibleLayerLabel(const Path &label) const;

	VideoBank * getVideoBank();
	RenderSystem * getRS();
	EID entity() const { return entity_; }

#if SS_TEST
	EID entity_;
#endif

private:
	std::vector<SpriteLayer> sprite_layers_;
	std::unordered_map<Path, bool> sprite_filter_layer_labels_;
	std::vector<RenderLayer> render_layers_;
	int layer_count_;
	int modifier_;
	int gui_max_layers_; // インスペクターのための変数
	bool use_unpacked_texture_;
};













class SpriteSystem: public System {
public:
	SpriteSystem();
	void init(Engine *engine);
	void attach(EID e);
	
	virtual bool onAttach(EID e, const char *type) override;
	virtual void onDetach(EID e) override;
	virtual bool onIsAttached(EID e) override;
	virtual void onStart() override;
	virtual void onEnd() override;
	virtual void onAppRenderUpdate() override;
	virtual void onFrameLateUpdate() override;
	virtual void onEntityInspectorGui(EID e) override;
	virtual bool setParameter(EID e, const char *key, const char *val) override;

	void copyFrom(EID e, const SpriteRendererCo *co);
	int  getLayerCount(EID e) const;
	bool getLayerAabb(EID e, int index, Vec3 *minpoint, Vec3 *maxpoint);
	const Path & getLayerLabel(EID e, int index) const;
	Vec3 getLayerOffset(EID e, int index) const;
	const Path & getLayerSpritePath(EID e, int index) const;
	Sprite * getLayerSprite(EID e, int index);

	void setLayerCount(EID e, int count);
	void setLayerLabel(EID e, int index, const Path &label);
	void setLayerOffset(EID e, int index, const Vec3 &value);
	void setLayerSprite(EID e, int index, Sprite *sprite);
	void setLayerSprite(EID e, int index, const Path &name);
	void setLayerAlpha(EID e, int layer, float alpha);
	void setLayerFilter(EID e, int layer, Filter f);
	void setLayerBlend(EID e, int layer, Blend blend);
	void setLayerColor(EID e, int layer, const Color &color);
	void setLayerSpecular(EID e, int layer, const Color &specular);
	void setLayerCallback(EID e, int layer, IMaterialCallback *cb, void *data);
	void setLayerMaterial(EID e, int index, const Material &material);
	void setLayerVisible(EID e, int index, bool value);

	void setColor(EID e, const Color &color);
	void setSpecular(EID e, const Color &specular);
	void setAlpha(EID e, float alpha);

	bool isLayerVisible(EID e, int index) const;
	void clearLayerFilters(EID e);
	void setLayerVisibleByLabel(EID e, const Path &label, bool visible);
	void copyLayerVisibleFilters(EID e, const SpriteRendererCo *src);
	bool layerWillBeRendered(EID e, int index);

	Material * getLayerMaterial(EID e, int index, Material *def=NULL);
	const Material * getLayerMaterial(EID e, int index, const Material *def=NULL) const;
	Material * getLayerMaterialAnimatable(EID e, int index);


	TEXID getBakedSpriteTexture(EID e, int layer);

	/// 指定されたインデクスのスプライトの元画像ＢＭＰでの座標を、ローカル座標に変換する（テクスチャ座標ではない）
	/// 例えばスプライトが player.png から作成されているなら、player.png 画像内の座標に対応するエンティティローカル座標を返す。
	///
	/// png などから直接生成したスプライト、アトラスにより切り取ったスプライト、それらをブロック化圧縮したスプライトに対応する。
	/// それ以外の方法で作成されたスプライトでは正しく計算できない可能性がある。
	///
	/// index スプライト番号
	/// bmp_x, bmp_y スプライトの作成元ビットマップ上の座標。原点は画像左上でＹ軸下向き。ピクセル単位。
	///
	Vec3 bitmapToLocalPoint(EID e, int index, const Vec3 &p);
	Vec3 localToBitmapPoint(EID e, int index, const Vec3 &p);

	bool RendererCo_getAabb(EID e, Vec3 *min_point, Vec3 *max_point);
	void RendererCo_onQueryGroupingImageSize(EID e, int *w, int *h, Vec3 *pivot);
	void RendererCo_draw(EID e, RenderSystem *rs, const RenderParams &render_params, DrawList *drawlist);
	void drawInTexture(EID e, const RenderLayer *nodes, int num_nodes, TEXID target, int w, int h, const Matrix4 &transform) const;
	void unpackInTexture(EID e, TEXID target, const Matrix4 &transform, const KMesh &mesh, TEXID texid);

	/// スプライト sprite を、この SpriteRendererCo の設定で描画するときに必要になるテクスチャやメッシュを準備する。
	/// 新しいアセットが必要なら構築し、アセットが未ロード状態ならばロードする。
	/// テクスチャアトラス、ブロック化圧縮、パレットの有無などが影響する
	///
	/// tex: NULL でない値を指定した場合、スプライトを描画するために実際に使用するテクスチャを取得できる
	/// mesh: NULL でない値をを指定した場合、スプライトを描画するために実際に使用するメッシュを取得できる
	///
	bool preparateMeshAndTextureForSprite(EID e, const Sprite *sprite, const Path &sprite_name, int modifier, TEXID *tex, KMesh *mesh);

	/// インデックス index のレイヤーが存在することを保証する
	/// 存在しなければ index+1 枚のレイヤーになるようレイヤーを追加する。
	/// 存在する場合は何もしない
	void guaranteeLayerIndex(EID e, int index);

	void setModifier(EID e, int index);
	int getModifier(EID e) const;

	/// ブロック化テクスチャーを使っている場合、復元したテクスチャーに対してマテリアルを適用するかどうか
	/// false の場合、画像復元とマテリアル適用を同時に行うため、正しく描画されない場合がある。
	/// true の場合、いったん元画像を復元してからマテリアルを適用する。
	/// 特に、処理対象外のピクセルを参照するようなエフェクトを使う場合は、復元されたテクスチャーに対して適用しないといけない
	void setUseUnpackedTexture(EID e, bool b);

	bool isVisibleLayerLabel(EID e, const Path &label) const;

private:
	const SpriteRendererCo * getNode(EID e) const;
	SpriteRendererCo * getNode(EID e);
	RenderSystem *rs_;
	std::unordered_set<EID> nodes_;
};

} // namespace
