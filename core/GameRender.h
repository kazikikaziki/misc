#pragma once
#include "GameInput.h"
#include "GameId.h"
#include "KMesh.h"
#include "KRect.h"
#include "KFont.h"
#include "KPath.h"
#include "Screen.h"


namespace mo {

struct Material;
class KMesh;
class RendererCo;
class HierarchySystem;
class Engine;
class VideoBank;
class RenderSystem;
class Gizmo;
class SpriteRendererCo;

Blend strToBlend(const char *s, Blend def);
bool strToBlend(const char *s, Blend *blend);


class DrawList;

class RendererCo {
public:
	RendererCo();
	virtual ~RendererCo();
	void setEnabled(bool b) { enabled_ = b; }
	
	/// グループ化を設定する。
	/// グループ化を有効にすると、一度内部レンダーテクスチャに描画してから
	/// その結果をあらためて描画するようになる。
	/// サブメッシュやスプライトレイヤーごとに個別のマテリアルを設定するのではなく、
	/// 描画後の画像に対してマテリアルを適用したい時に使う（アルファ設定など）
	/// ※ここでグループ化されるのは、他のエンティティの描画ではなく、このレンダーコンポーネント内で描画される複数のメッシュである
	///   複数のサブメッシュ、複数のレイヤーなど。
	void setGrouping(bool value);
	bool isGrouping() const;

	/// グループ化されている場合、一時レンダーテクスチャへの描画結果を
	/// 改めて出力するときに使うマテリアルを得る
	Material * getGroupingMaterial();

	/// グループ化されているときに使う一時レンダーテクスチャのサイズを指定する。
	/// w, h どちらかに 0 に指定すると、自動的にサイズを計算する
	/// pivot メッシュの原点に対応する、一時レンダーテクスチャ上の座標（テクセル座標ではない）
	void setGroupingImageSize(int w, int h, const Vec3 &pivot);
	void getGroupingImageSize(int *w, int *h, Vec3 *pivot);

	/// グループ化されているときに使う一時レンダーテクスチャのサイズを自動計算に設定してある場合、
	/// サイズ取得するために呼ばれる
	/// pivot 一時レンダーテクスチャ「を」描画する時に使う原点座標
	virtual void onQueryGroupingImageSize(int *w, int *h, Vec3 *pivot) = 0;

	/// 描画先をレンダーテクスチャに設定する
	/// 解除する場合は空文字列 "" を渡す
	void setRenderTexture(const Path &name);
	TEXID getRenderTexture();

	/// レンダーテクスチャーに描画するときのトランスフォーム行列。
	/// getRenderTexture() が NULL でないときにだけ有効
	void setTransformInRenderTexture(const Matrix4 &matrix);
	const Matrix4 & getTransformInRenderTexture() const;

	Color master_color_;
	Color master_specular_;
	bool inherit_color_;
	bool inherit_specular_;
	
	// このレンダラーにだけ適用させる変形行列。
	// 子の Transform には影響しない
	Matrix4 local_transform_;

	/// 自分を、子よりも後に描画する
	/// レンダリングオーダーを RenderingOrder_Hierarchy にしてある場合のみ有効。
	bool render_after_children_;

	/// 描画を実行する
	virtual void onRendererDrawInfoSelected(const DebugRenderParameters &p) {}
	//
	virtual void draw(RenderSystem *rs, const RenderParams &render_params, DrawList *drawlist) = 0;
	virtual void updateInspectorGui() = 0;

	virtual void onInspector();

	/// 描画オブジェクトのAABB
	/// 描画するべきものが存在しない場合は false を返す
	virtual bool getAabb(Vec3 *minpoint, Vec3 *maxpoint) = 0;

	/// オフセット量も考慮した AABB を得る
	bool getOffsetAabb(Vec3 *minpoint, Vec3 *maxpoint) {
		if (getAabb(minpoint, maxpoint)) {
			if (minpoint) *minpoint += offset_;
			if (maxpoint) *maxpoint += offset_;
			return true;
		}
		return false;
	}

	TEXID getGroupRenderTexture();
	void setGroupRenderTexture(TEXID rentex);

	KMesh * getGroupRenderMesh();
	void setGroupRenderMesh(const KMesh &mesh);

	const Vec3 & getGroupRenderPivot() const;
	void setGroupRenderPivot(const Vec3 &p);

	void updateGroupRenderTextureAndMesh();

	VideoBank * getVideoBank();
	Path rentex_path_;
	Path group_rentex_name_;
	Material group_material_;
	Vec3 offset_;
	EID entity_;
	int world_layer_;
	bool enabled_;
	bool adj_snap_; // 座標を整数化する。ただし、微妙な伸縮アニメを伴う場合はアニメがガタガタになってしまうことがある。
	bool adj_half_;

protected:
	std::vector<int> subset_rendering_order_;
	Matrix4 transform_in_render_texture_; // レンダーターゲットに描画する場合のみ有効
	Vec3 tmp_group_rentex_pivot_;
	Vec3 group_rentex_pivot_;
	bool group_rentex_autosize_;
	int group_rentex_w_;
	int group_rentex_h_;

private:
	TEXID tmp_group_rentex_;
	KMesh group_mesh_;
	bool group_enabled_;
};

class MeshRendererCo: public RendererCo {
public:
	MeshRendererCo();

	KMesh * getMesh();
	const KMesh * getMesh() const;
	Material * getSubMeshMaterial(int index);
	Material * getSubMeshMaterialAnimatable(int index);
	const Material * getSubMeshMaterialAnimatable(int index) const;
	const Matrix4 & getSubMeshTransform(int index) const;
	void setSubMeshTransform(int index, const Matrix4 &m);

	int getSubMeshMaterialCount();
	void setMesh(const KMesh &mesh);
	void setSubMeshMaterial(int index, const Material &material);
	//
	virtual void updateInspectorGui() override;
	virtual void draw(RenderSystem *rs, const RenderParams &render_params, DrawList *drawlist) override;
	virtual void onQueryGroupingImageSize(int *w, int *h, Vec3 *pivot) override;
	virtual bool getAabb(Vec3 *min_point, Vec3 *max_point) override;

	void drawSubmesh(const KMesh *mesh, int index, const RenderParams &params, const Color &mul, const Color &spe, DrawList *drawlist) const;

	// 描画順を逆順にする
	// （末尾のサブメッシュを最初に描画し、先頭のサブメッシュを最後に描画する）
	bool invert_order_;

private:
	enum _Attr {
		A_NONE,
		A_ENABLED,
		A_MESH,
		A_COLOR_ARGB,
		A_COLOR_RGB,
		A_COLOR_R,
		A_COLOR_G,
		A_COLOR_B,
		A_COLOR_A,
		A_SPECLAR_RGB,
		A_SPECLAR_R,
		A_SPECLAR_G,
		A_SPECLAR_B,
		A_BLEND,
		A_MATERIAL_FLOAT,
		A_WORLD_LAYER,
	};
	enum _Err {
		E_NONE,
		E_INVALID_MESH,
	};
	KMesh mesh_;
	mutable std::vector<Material> submesh_materials_;
	mutable std::vector<Matrix4> submesh_transform_;
	mutable int bound_grouping_rentex_w_;
	mutable int bound_grouping_rentex_h_;
};



class RenderSystem: public System, public IScreenCallback {
public:
	struct _Node {
		_Node() {
			entity = NULL;
			renderer = NULL;
			sprite_renderer = NULL;
			mesh_renderer = NULL;
			debug_time_spent_msec = 0.0f;
		}
		EID entity;
		RendererCo *renderer;
		SpriteRendererCo *sprite_renderer;
		MeshRendererCo *mesh_renderer;
		float debug_time_spent_msec;
	};


	RenderSystem();
	virtual ~RenderSystem();
	void init(Engine *engine);
	virtual void onEntityInspectorGui(EID e) override;
	virtual bool setParameter(EID e, const char *key, const char *val) override;
	virtual bool onAttach(EID e, const char *type) override;
	virtual void onStart() override;
	virtual void onEnd() override;
	virtual void onDetach(EID e) override;
	virtual bool onIsAttached(EID e) override { return _getNode(e) != NULL; }
	virtual void onFrameEnd() override;
	virtual void onInspectorGui() override;
	virtual void onRenderWorld(RenderWorldParameters *params) override;

	// ソート方法を RenderingOrder_Custom にしたときに呼ばれる
	virtual void onCustomSorting2(std::vector<_Node*> &nodes) {}

	void onRenderWorld_make_tmp_nodes(RenderWorldParameters *params);

	bool master_adj_snap_;
	bool master_adj_half_;

	// point_in_screen スクリーン座標 (スクリーン中央原点、左下(-1, -1) 右上(1, 1) とする座標
	EID getEntityByScreenPoint(const Vec3 &point_in_screen, EID camera_e, EntityList *entities);

	// 描画しないレンダラーをアタッチする。
	// inherit color など、描画のためのパラメータをツリー管理するためだけにレンダラーが必要な場合に使う
	void attachNull(EID e);
	void attachMesh(EID e);
	void attachSprite(EID e);
	//
	SpriteRendererCo * getSpriteRenderer(EID e);

	bool copySpriteRenderer(EID e, EID source);

	Color getMasterColor(EID e) const;
	Color getMasterColorInTree(EID e) const;
	Color getMasterSpecular(EID e) const;
	Color getMasterSpecularInTree(EID e) const;
	void setMasterColor(EID e, const Color &color);
	void setMasterAlpha(EID e, float a);
	void setMasterSpecular(EID e, const Color &specular);
	void setInheritColor(EID e, bool b);
	void setInheritSpecular(EID e, bool b);
	void setWorldLayer(EID e, int layer);
	int getWorldLayer(EID e) const;

	/// グループ化を設定する。
	/// グループ化を有効にすると、一度内部レンダーテクスチャに描画してから
	/// その結果をあらためて描画するようになる。
	/// サブメッシュやスプライトレイヤーごとに個別のマテリアルを設定するのではなく、
	/// 描画後の画像に対してマテリアルを適用したい時に使う（アルファ設定など）
	/// ※ここでグループ化されるのは、他のエンティティの描画ではなく、このレンダーコンポーネント内で描画される複数のメッシュである
	///   複数のサブメッシュ、複数のレイヤーなど。
	void setGrouping(EID e, bool value);
	bool isGrouping(EID e) const;

	/// グループ化されている場合、一時レンダーテクスチャへの描画結果を
	/// 改めて出力するときに使うマテリアルを得る
	Material * getGroupingMaterial(EID e);

	/// グループ化されているときに使う一時レンダーテクスチャのサイズを指定する。
	/// w, h どちらかに 0 に指定すると、自動的にサイズを計算する
	/// pivot メッシュの原点に対応する、一時レンダーテクスチャ上の座標（テクセル座標ではない）
	void setGroupingImageSize(EID e, int w, int h, const Vec3 &pivot);
	void getGroupingImageSize(EID e, int *w, int *h, Vec3 *pivot);

	/// 描画先をレンダーテクスチャに設定する
	/// 解除する場合は空文字列 "" を渡す
	void setRenderTexture(EID e, const Path &name);
	TEXID getRenderTexture(EID e);

	/// レンダーテクスチャーに描画するときのトランスフォーム行列。
	/// getRenderTexture() が NULL でないときにだけ有効
	void setTransformInRenderTexture(EID e, const Matrix4 &matrix);
	const Matrix4 & getTransformInRenderTexture(EID e) const;

	/// オフセット量も考慮した AABB を得る
	bool getOffsetAabb(EID e, Vec3 *minpoint, Vec3 *maxpoint);

	void setOffset(EID e, const Vec3 &offset);
	const Vec3 & getOffset(EID e) const;

	TEXID getGroupRenderTexture(EID e);
	void setGroupRenderTexture(EID e, TEXID rentex);

	KMesh * getGroupRenderMesh(EID e);
	void setGroupRenderMesh(EID e, const KMesh &mesh);

	const Vec3 & getGroupRenderPivot(EID e) const;
	void setGroupRenderPivot(EID e, const Vec3 &p);

	void updateGroupRenderTextureAndMesh(EID e);

	// このレンダラーにだけ適用させる変形行列。
	// 子の Transform には影響しない
	void setLocalTransform(EID e, const Matrix4 &matrix);
	const Matrix4 & getLocalTransform(EID e) const;

	void setSubMeshBlend(EID e, int submesh, Blend blend);
	void setSubMeshTexture(EID e, int submesh, TEXID texid);
	void setSubMeshTexture(EID e, int submesh, const Path &texname);
	void setSubMeshFilter(EID e, int submesh, Filter filter);
	void setSubMeshColor(EID e, int submesh, const Color &color);
	void setSubMeshSpecular(EID e, int submesh, const Color &specular);
	Material * getSubMeshMaterial(EID e, int submesh);
	const Material * getSubMeshMaterial(EID e, int submesh) const;
	int getSubMeshMaterialCount(EID e) const;
	const Matrix4 & getSubMeshTransform(EID e, int index) const;
	void setSubMeshTransform(EID e, int index, const Matrix4 &m);
	KMesh * getMesh(EID e);
	const KMesh * getMesh(EID e) const;

	enum Flag {
		NONE,
		ADJ_SNAP,
		ADJ_HALF,
		INHERIT_COLOR,
		INHERIT_SPECULAR,
	};

	void setFlag(EID e, Flag flag, bool value);
	bool getFlag(EID e, Flag flag) const;

private:
	_Node * _getNode(EID e);
	const _Node * _getNode(EID e) const;
	void _dirtyMeshTree(EID e);

	std::unordered_map<EID , _Node> nodes_;
	std::vector<_Node*> tmp_nodes_;
	std::vector<_Node*> tmp_nodes2_;

	Engine *engine_;
	HierarchySystem *hs_;
	int num_draw_nodes_;
	int num_drawlist_;
};


}
