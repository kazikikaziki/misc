#include "GameRender.h"
//
#include "GameCamera.h"
#include "GameHierarchy.h"
#include "GameSprite.h"
#include "Engine.h"
#include "inspector.h"
#include "VideoBank.h"
#include "KDrawList.h"
#include "KClock.h"
#include "KImgui.h"
#include "KNum.h"
#include "KStd.h"
#include "KLog.h"
#include "KVideo.h"
#include <algorithm>

// 描画リストに一度書き出してから描画する
#define USING_DRAWLIST 1


namespace mo {



static DrawList s_drawlist;

#pragma region Functions



static const char * s_blend_name_table[Blend_EnumMax+1] = {
	"one",    // Blend_One
	"add",    // Blend_Add
	"alpha",  // Blend_Alpha
	"sub",    // Blend_Sub
	"mul",    // Blend_Mul
	"screen", // Blend_Screen
	"max",    // Blend_Max
	NULL,     // (sentinel)
};
Blend strToBlend(const char *s, Blend def) {
	if (s == NULL || !s[0]) return def;
	for (int i=0; i<Blend_EnumMax; i++) {
		if (strcmp(s, s_blend_name_table[i]) == 0) {
			return (Blend)i;
		}
	}
	Log_errorf("Invalid blend name '%s'", s);
	return def;
}
bool strToBlend(const char *s, Blend *blend) {
	if (s && s[0]) {
		for (int i=0; i<Blend_EnumMax; i++) {
			if (strcmp(s, s_blend_name_table[i]) == 0) {
				if (blend) *blend = (Blend)i;
				return true;
			}
		}
	}
	return false;
}
#pragma endregion // Functions


/// ヒエラルキー順に並び替える
/// 親は子よりも前に、先に追加されたものは後に追加されたモノよりも前に来る。
/// 一般的な GUI ツリーに基づく描画順序のルールと。
class RendererHierarchySortPred2 {
public:
	HierarchySystem *hs_;
	explicit RendererHierarchySortPred2(HierarchySystem *hi) {
		hs_ = hi;
	}
	bool operator()(const RenderSystem::_Node *pnode1, const RenderSystem::_Node *pnode2) const {
		const RenderSystem::_Node &node1 = *pnode1;
		const RenderSystem::_Node &node2 = *pnode2;
		int worldlayer1 = node1.renderer ? node1.renderer->world_layer_ : 0;
		int worldlayer2 = node2.renderer ? node2.renderer->world_layer_ : 0;
		// レイヤー番号が大きいものから並べる
		if (worldlayer1 != worldlayer2) {
			return worldlayer1 > worldlayer2;
		}
		// 巡回順で並べる
		// 通常は親→子の順番で描画するが、render_after_children_ が設定されているものは子よりも後に描画する
		int idx1[2], idx2[2];
		hs_->getTraversalIndex(node1.entity, NULL, &idx1[0], &idx1[1]);
		hs_->getTraversalIndex(node2.entity, NULL, &idx2[0], &idx2[1]);

		int ord1 = idx1[0]; // 巡回（行き）の値
		int ord2 = idx2[0]; // 巡回（行き）の値
		if (node1.renderer && node1.renderer->render_after_children_) {
			ord1 = idx1[1]; // 巡回（帰り）の値を使う
		}
		if (node2.renderer && node2.renderer->render_after_children_) {
			ord2 = idx2[1]; // 巡回（帰り）の値を使う
		}
		return ord1 < ord2;
	}
};

/// ワールドＺ座標順で並び替える。
/// ただしワールドレイヤーの値はＺ値よりも優先される
class RendererDepthSortPred2 {
public:
	HierarchySystem *hs_;
	
	RendererDepthSortPred2(HierarchySystem *hs) {
		hs_ = hs;
	}
	bool operator()(const RenderSystem::_Node *pnode1, const RenderSystem::_Node *pnode2) const {
		const RenderSystem::_Node &node1 = *pnode1;
		const RenderSystem::_Node &node2 = *pnode2;
		int worldlayer1 = node1.renderer ? node1.renderer->world_layer_ : 0;
		int worldlayer2 = node2.renderer ? node2.renderer->world_layer_ : 0;
		// レイヤー番号が大きいものから並べる
		if (worldlayer1 != worldlayer2) {
			return worldlayer1 > worldlayer2;
		}
		// 順序が定義できない場合は同値とみなして false を返す。
		// （同値のときに true を返してしまうと std::sort のチェックに引っかかり、エラーになる）
		Vec3 pos1 = hs_->getPositionInWorld(node1.entity);
		Vec3 pos2 = hs_->getPositionInWorld(node2.entity);
		// Z座標の大きなものから並べる
		if (pos1.z != pos2.z) {
			return pos1.z > pos2.z;
		}
		// Z座標が同一であれば、Y高度の低いほうから並べる
		if (pos1.y != pos2.y) {
			return pos1.y < pos2.y;
		}
		// Z, Y が同一ならば、順序値の小さい方から並べる（先に作ったものが先に描画される）
		int o1, o2;
		hs_->getTraversalIndex(node1.entity, &o1, NULL, NULL);
		hs_->getTraversalIndex(node1.entity, &o2, NULL, NULL);
		return o1 < o2;
	}
};

#pragma region RendererCo
RendererCo::RendererCo() {
	static int s_index = 0;
	enabled_ = true;
	entity_ = NULL;
	world_layer_ = 0;
	render_after_children_ = false;
	tmp_group_rentex_ = NULL;
	master_color_ = Color::WHITE;
	master_specular_ = Color::ZERO;
	inherit_color_ = true;
	inherit_specular_ = true;
	group_rentex_w_ = 0;
	group_rentex_h_ = 0;
	group_rentex_pivot_ = Vec3();
	group_rentex_autosize_ = true;
	group_enabled_ = false;
	group_rentex_name_ = Path::fromFormat("_group_%d.tex", ++s_index);
	adj_snap_ = MO_VIDEO_ENABLE_HALF_PIXEL;
	adj_half_ = MO_VIDEO_ENABLE_HALF_PIXEL;
}
RendererCo::~RendererCo() {
	TextureBank *tex_bank = getVideoBank()->getTextureBank();
	if (tex_bank) {
		tex_bank->delAsset(group_rentex_name_);
	}
//	Video::deleteTexture(tmp_group_rentex_);
}

#pragma region group rendering
void RendererCo::setGroupingImageSize(int w, int h, const Vec3 &pivot) {
	if (w > 0 && h > 0) {
		// レンダーターゲットのサイズが奇数になっていると、ドットバイドットでの描画が歪む
		Log_assert(w % 2 == 0);
		Log_assert(h % 2 == 0);
		group_rentex_w_ = w;
		group_rentex_h_ = h;
		group_rentex_pivot_ = pivot;
		group_rentex_autosize_ = false;
	} else {
		group_rentex_autosize_ = true;
	}
}
void RendererCo::getGroupingImageSize(int *w, int *h, Vec3 *pivot) {
	if (group_rentex_autosize_) {
		onQueryGroupingImageSize(w, h, pivot);
	} else {
		if (w) *w = group_rentex_w_;
		if (h) *h = group_rentex_h_;
		if (pivot) *pivot = group_rentex_pivot_;
	}
}
void RendererCo::setGrouping(bool value) {
	group_enabled_ = value;
}
bool RendererCo::isGrouping() const {
	return group_enabled_;
}
Material * RendererCo::getGroupingMaterial() {
	return &group_material_;
}
#pragma endregion // group rendering


TEXID RendererCo::getRenderTexture() {
	if (rentex_path_.empty()) return NULL;
	TextureBank *tex_bank = getVideoBank()->getTextureBank();
	TEXID tex = tex_bank->findTextureRaw(rentex_path_, false);
	TexDesc desc;
	Video::getTextureDesc(tex, &desc);
	return (tex && desc.is_render_target) ? tex : NULL;
}
void RendererCo::setRenderTexture(const Path &asset_path) {
	rentex_path_ = asset_path;
}
const Matrix4 & RendererCo::getTransformInRenderTexture() const {
	return transform_in_render_texture_;
}
void RendererCo::setTransformInRenderTexture(const Matrix4 &matrix) {
	transform_in_render_texture_ = matrix;
}
void RendererCo::updateGroupRenderTextureAndMesh() {
	TextureBank *tex_bank = getVideoBank()->getTextureBank();

	// グループ描画に必要なレンダーテクスチャサイズを得る
	int ren_w = 256;
	int ren_h = 256;
	getGroupingImageSize(&ren_w, &ren_h, NULL);
	if (ren_w <= 0 || ren_h <= 0) {
		// テクスチャなし
		return;
	}
	// レンダーテクスチャ
	TEXID target_tex = tex_bank->findTextureRaw(group_rentex_name_, false);
	TexDesc desc;
	Video::getTextureDesc(target_tex, &desc);
	if (target_tex && !desc.is_render_target) {
		// レンダーテクスチャではない
		Log_errorf("'%s' is not a render texture", group_rentex_name_.u8());
		return;
	}
	if (target_tex == NULL) {
		// まだ存在しない
		target_tex = tex_bank->addRenderTexture(group_rentex_name_, ren_w, ren_h);

	} else if (desc.w != ren_w || desc.h != ren_h) {
		Log_assert(desc.is_render_target);
		// サイズが違う
		// 一度削除して再生成する
		tex_bank->delAsset(group_rentex_name_);
		target_tex = tex_bank->addRenderTexture(group_rentex_name_, ren_w, ren_h);
	}
	setGroupRenderTexture(target_tex);

	// レンダーターテクスチャを描画するためのメッシュ
	if (group_mesh_.getSubMeshCount() == 0) {
		MeshShape::makeRect(&group_mesh_, 0, 0, ren_w, ren_h, 0.0f, 0.0f, 1.0f, 1.0f, Color::WHITE);
	} else {
		Vec3 p0, p1;
		group_mesh_.getAabb(&p0, &p1);
		int mesh_w = (int)(p1.x - p0.x);
		int mesh_h = (int)(p1.y - p0.y);
		if (mesh_w != ren_w || mesh_h != ren_h) {
			// メッシュサイズがレンダーテクスチャとあっていない。頂点を再設定する
			MeshShape::makeRect(&group_mesh_, 0, 0, ren_w, ren_h, 0.0f, 0.0f, 1.0f, 1.0f, Color::WHITE);
		}
	}
}
TEXID RendererCo::getGroupRenderTexture() {
	updateGroupRenderTextureAndMesh();
	return tmp_group_rentex_;
}
void RendererCo::setGroupRenderTexture(TEXID rentex) {
	if (rentex) {
		TexDesc desc;
		Video::getTextureDesc(rentex, &desc);
		Log_assert(desc.is_render_target);
		tmp_group_rentex_ = rentex;
	} else {
		tmp_group_rentex_ = NULL;
	}
}
KMesh * RendererCo::getGroupRenderMesh() {
	updateGroupRenderTextureAndMesh();
	return &group_mesh_;
}
void RendererCo::setGroupRenderMesh(const KMesh &mesh) {
	group_mesh_ = mesh;
}
VideoBank * RendererCo::getVideoBank() {
	return g_engine->videobank();
}
const Vec3 & RendererCo::getGroupRenderPivot() const {
	return tmp_group_rentex_pivot_;
}
void RendererCo::setGroupRenderPivot(const Vec3 &p) {
	tmp_group_rentex_pivot_ = p;
}
void RendererCo::onInspector() {
#ifndef NO_IMGUI
	RenderSystem *rs = g_engine->findSystem<RenderSystem>();
	if (1) {
		ImGui::DragInt("World Layer", &world_layer_);
	}
	if (1) {
		ImGui::DragFloat3("Offset", (float*)&offset_, 1);
	}
	if (ImGui::TreeNode(ImGui_ID(2), "Grouping : %s", (group_enabled_ ? "on" : "off"))) {
		ImGui::Checkbox("Enable Grouping", &group_enabled_);
		if (ImGui::TreeNode("Material")) {
			Data_guiMaterialInfo(&group_material_);
			ImGui::TreePop();
		}
		TEXID gtex = getGroupRenderTexture();
		ImGui::Text("Target Tex: %s", group_rentex_name_.u8());
		if (gtex) {
			int tex_w;
			int tex_h;
			Video::getTextureSize(gtex, &tex_w, &tex_h);
			ImGui::Text("Tex size: %d x %d", tex_h, tex_h);
		} else {
			ImGui::Text("Tex size: %d x %d", group_rentex_w_, group_rentex_h_);
		}
		ImGui::Text("Tex auto sizing: %s", group_rentex_autosize_ ? "Yes" : "No");
		ImGui::Text("Render pos in tex: (%g, %g)", tmp_group_rentex_pivot_.x, tmp_group_rentex_pivot_.y);
		ImGui::Text("Pivot for output texture: (%g, %g)", group_rentex_pivot_.x, group_rentex_pivot_.y);
		if (ImGui::TreeNode("Mesh")) {
			Data_guiMeshInfo(&group_mesh_);
			ImGui::TreePop();
		}
		ImGui::TreePop();
	}
	if (ImGui::TreeNode(ImGui_ID(3), "Render in texture: %s", (!rentex_path_.empty() ? rentex_path_.u8() : "off"))) {
		// Render target
		Path new_path;
		if (TextureBank_guiRenderTextureSelector("Render in texture", rentex_path_, &new_path)) {
			setRenderTexture(new_path);
		}
		// Render offset
		Matrix4 m = getTransformInRenderTexture();
		bool xedit = ImGui::DragFloat("Draw In Render Tex X", &m._41);
		bool yedit = ImGui::DragFloat("Draw In Render Tex Y", &m._42);
		if (xedit || yedit) {
			setTransformInRenderTexture(m);
		}
		ImGui::TreePop();
	}
	if (1) {
		ImGui::Separator();
		ImGui::Checkbox("Render after children", &render_after_children_);
		ImGui::Indent();
		ImGui::Text("* This option works only when Camera::sorting setted 'HIERARCHY'");
		ImGui::Unindent();
	}
	if (1) {
		ImGui::Separator();
		ImGui::Checkbox("Inherit Color", &inherit_color_);
		ImGui::Checkbox("Inherit Specular", &inherit_specular_);
		ImGui::ColorEdit4("Master Color", (float*)&master_color_);
		ImGui::ColorEdit4("Master Specular", (float*)&master_specular_);

		Color32 mc = rs->getMasterColorInTree(entity_);
		Color32 ms = rs->getMasterColorInTree(entity_);
		ImGui::Text("Final Master Color      : R:%3d G:%3d B:%3d A:%3d", mc.r, mc.g, mc.b, mc.a);
		ImGui::Text("Final Master Specularlor: R:%3d G:%3d B:%3d A:%3d", ms.r, ms.g, ms.b, ms.a);
	}
	if (1) {
		ImGui::Separator();
		ImGui::Checkbox("Snap to integer coordinate", &adj_snap_);
		ImGui::Checkbox("Offset half pixel", &adj_half_);
		if (!rs->master_adj_snap_) {
			ImGui::TextColored(GuiColor_WarningText, "'adj_snap' has been disabled by RenderSystem!");
		}
		if (!rs->master_adj_half_) {
			ImGui::TextColored(GuiColor_WarningText, "'adj_half' has been disabled by RenderSystem!");
		}
	}
	if (1) {
		ImGui_Matrix("Local transform", &local_transform_);
	}
	
	ImGui::Separator();
	if (ImGui::CollapsingHeader(typeid(*this).name(), ImGuiTreeNodeFlags_DefaultOpen)) {
		updateInspectorGui();
	}
#endif // !NO_IMGUI
}
#pragma endregion // RendererCo





#pragma region MeshRendererCo
MeshRendererCo::MeshRendererCo() {
	bound_grouping_rentex_w_ = 0;
	bound_grouping_rentex_h_ = 0;
	invert_order_ = false;
}

KMesh * MeshRendererCo::getMesh() {
	return &mesh_;
}
const KMesh * MeshRendererCo::getMesh() const {
	return &mesh_;
}
const Matrix4 & MeshRendererCo::getSubMeshTransform(int index) const {
	if (index < 0 || (int)submesh_transform_.size() <= index) {
		static const Matrix4 s_identity;
		return s_identity;
	}
	return submesh_transform_[index];
}
void MeshRendererCo::setSubMeshTransform(int index, const Matrix4 &m) {
	if ((int)submesh_transform_.size() <= index) {
		submesh_transform_.resize(index + 1);
	}
	submesh_transform_[index] = m;
}
Material * MeshRendererCo::getSubMeshMaterial(int index) {
	if (index < 0 || (int)submesh_materials_.size() <= index) {
		return NULL;
	}
	return &submesh_materials_[index];
}
int MeshRendererCo::getSubMeshMaterialCount() {
	return (int)submesh_materials_.size();
}
void MeshRendererCo::setMesh(const KMesh &mesh) {
	mesh_ = mesh;
}
void MeshRendererCo::setSubMeshMaterial(int index, const Material &material) {
	if (index < 0) {
		return;
	}
	if ((int)submesh_materials_.size() < index + 1) {
		submesh_materials_.resize(index + 1);
	}
	submesh_materials_[index] = material;
}
void MeshRendererCo::updateInspectorGui() {
#ifndef NO_IMGUI
	ImGui::Text("Mesh Renderer");
	ImGui::Text("Num Vertices  %d", mesh_.getVertexCount());
	ImGui::Text("Num Indices   %d", mesh_.getIndexCount());
	ImGui::Text("Num Submeshes %d", mesh_.getSubMeshCount());
	for (int i=0; i<mesh_.getSubMeshCount(); i++) {
		ImGui::PushID(i);
		if (ImGui::TreeNode(ImGui_ID(0), "Submesh %d", i)) {
			KSubMesh sub;
			mesh_.getSubMesh(i, &sub);
			ImGui::Text("Start %d", sub.start);
			ImGui::Text("Count %d", sub.count);
			Material *mat = getSubMeshMaterialAnimatable(i);
			Data_guiMaterialInfo(mat);
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
#endif // !NO_IMGUI
}
Material * MeshRendererCo::getSubMeshMaterialAnimatable(int index) {
	if (index < 0) {
		return NULL;
	}
	if (index < (int)submesh_materials_.size()) {
		return &submesh_materials_[index];
	}
	submesh_materials_.resize(index + 1);
	return &submesh_materials_[index];
}
const Material * MeshRendererCo::getSubMeshMaterialAnimatable(int index) const {
	if (index < 0) {
		return NULL;
	}
	if (index < (int)submesh_materials_.size()) {
		return &submesh_materials_[index];
	}
	submesh_materials_.resize(index + 1);
	return &submesh_materials_[index];
}
void MeshRendererCo::drawSubmesh(const KMesh *mesh, int index, const RenderParams &params, const Color &mul, const Color &spe, DrawList *drawlist) const {
	const Material *p_mat = getSubMeshMaterialAnimatable(index);
	Log_assert(p_mat);
	Material mat = *p_mat; // COPY

	const Matrix4 &submesh_trans = getSubMeshTransform(index);

	// マスターカラーと合成する
	mat.color *= mul; // MUL
	mat.specular += spe; // ADD

	// 描画
	if (drawlist) {
		drawlist->setProjection(params.projection_matrix);
		drawlist->setTransform(submesh_trans * params.transform_matrix);
		drawlist->setMaterial(mat);
		drawlist->addMesh(mesh, index);
	} else {
		Video::pushRenderState();
		Video::setProjection(params.projection_matrix);
		Video::setTransform(submesh_trans * params.transform_matrix);
		Video::beginMaterial(&mat);
		Video::drawMesh(mesh, index);
		Video::endMaterial(&mat);
		Video::popRenderState();
	}
}
void MeshRendererCo::draw(RenderSystem *rs, const RenderParams &render_params, DrawList *drawlist) {
	// メッシュ未指定なら描画しない
	const KMesh *mesh = getMesh();
	if (mesh == NULL) {
		return;
	}

	// サブメッシュ数
	int num_submesh = mesh->getSubMeshCount();
	if (num_submesh == 0) {
		return;
	}

	// 頂点数
	if (mesh->getVertexCount() == 0) {
		return;
	}

	bool adj_snap = rs->getFlag(entity_, RenderSystem::ADJ_SNAP);
	bool adj_half = rs->getFlag(entity_, RenderSystem::ADJ_HALF);

	// マスターカラー
	Color master_col = this->master_color_;
	Color master_spe = this->master_specular_;

	// 親の設定を継承する場合は合成マスターカラーを得る
	if (inherit_color_) {
		master_col = rs->getMasterColorInTree(entity_);
	}
	if (inherit_specular_) {
		master_spe = rs->getMasterSpecularInTree(entity_);
	}

	if (rs->isGrouping(entity_)) {
		// グループ化する。
		// 一旦別のレンダーテクスチャに書き出し、それをスプライトとして描画する
		int groupW;
		int groupH;
		Vec3 groupPiv;
		rs->getGroupingImageSize(entity_, &groupW, &groupH, &groupPiv);
		TEXID group_tex = rs->getGroupRenderTexture(entity_);

		// groupPivはビットマップ原点（＝レンダーターゲット画像左上）を基準にしていることに注意
		Vec3 pos;
		pos.x -= groupW / 2; // レンダーテクスチャ左上へ
		pos.y += groupH / 2;
		pos.x += groupPiv.x; // さらに groupPivずらす
		pos.y -= groupPiv.y;
		Matrix4 group_transform = Matrix4::fromTranslation(pos); // 描画位置
	
		// ピクセルパーフェクト
		if (MO_VIDEO_ENABLE_HALF_PIXEL) {
			// 前提として、半ピクセルずらす前の座標は整数でないといけない
			if (rs->master_adj_snap_ && adj_snap_) mo::translateFloorInPlace(group_transform);
			if (rs->master_adj_half_ && adj_half_) mo::translateHalfPixelInPlace(group_transform);
		}

		// 描画
		{
	//	drawSubmeshInTexture(render_layers_.data(), render_layers_.size(), group_tex, groupW, groupH, transform);
			RenderParams group_params;
			group_params.projection_matrix = Matrix4::fromOrtho((float)groupW, (float)groupH, (float)(-1000), (float)1000);
			group_params.transform_matrix = group_transform;
	
			Video::pushRenderTarget(group_tex);
			Video::clearColor(Color::ZERO);
			Video::clearDepth(1.0f);
			Video::clearStencil(0);
			if (!invert_order_) {
				// サブメッシュの番号が小さな方から順番に描画する。
				// （サブメッシュ[0]が一番最初＝一番奥に描画される）
				for (int i=0; i<num_submesh; i++) {
					drawSubmesh(mesh, i, group_params, master_col, master_spe, NULL);
				}

			} else {
				// サブメッシュの番号が大きなほうから順番に描画する。
				// （サブメッシュ[0]が一番最後＝一番手前に描画される）
				for (int i=num_submesh-1; i>=0; i--) {
					drawSubmesh(mesh, i, group_params, master_col, master_spe, NULL);
				}
			}
			Video::popRenderTarget();
		}

		// グループ化描画終わり。ここまでで render_target テクスチャにはスプライトの絵が描画されている。
		// これを改めて描画する
		{
			RenderParams params = render_params; // Copy

			// オフセット
			Matrix4 offset_mat;
			{
				Vec3 off;
				off += offset_;
				off.x += -groupPiv.x; // レンダーターゲット内のピヴォットが、通常描画時の原点と一致するように調整する
				off.y +=  groupPiv.y - groupH;
				offset_mat = Matrix4::fromTranslation(off);
			}

			// 変形行列
			params.transform_matrix = offset_mat * local_transform_ * params.transform_matrix;

			// ピクセルパーフェクト
			if (MO_VIDEO_ENABLE_HALF_PIXEL) {
				// 前提として、半ピクセルずらす前の座標は整数でないといけない
				if (rs->master_adj_snap_ && adj_snap_) mo::translateFloorInPlace(params.transform_matrix);
				if (rs->master_adj_half_ && adj_half_) mo::translateHalfPixelInPlace(params.transform_matrix);
			}

			// マテリアル
			Material *p_mat = rs->getGroupingMaterial(entity_);
			Log_assert(p_mat);
			Material mat = *p_mat; // COPY
			mat.texture = group_tex;

			// マスターカラーと合成する
			mat.color    *= master_col; // MUL
			mat.specular += master_spe; // ADD

			// 描画
			KMesh *groupMesh = rs->getGroupRenderMesh(entity_);
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

	} else {

		RenderParams params = render_params; // Copy

		// 変形行列
		params.transform_matrix = Matrix4::fromTranslation(offset_) * local_transform_ * params.transform_matrix;

		// ピクセルパーフェクト
		if (MO_VIDEO_ENABLE_HALF_PIXEL) {
			// 前提として、半ピクセルずらす前の座標は整数でないといけない
			if (rs->master_adj_snap_ && adj_snap_) mo::translateFloorInPlace(params.transform_matrix);
			if (rs->master_adj_half_ && adj_half_) mo::translateHalfPixelInPlace(params.transform_matrix);
		}

		// グループ化しない
		if (!invert_order_) {
			// サブメッシュの番号が小さな方から順番に描画する。
			// （サブメッシュ[0]が一番最初＝一番奥に描画される）
			for (int i=0; i<num_submesh; i++) {
				drawSubmesh(mesh, i, params, master_col, master_spe, drawlist);
			}

		} else {
			// サブメッシュの番号が大きなほうから順番に描画する。
			// （サブメッシュ[0]が一番最後＝一番手前に描画される）
			for (int i=num_submesh-1; i>=0; i--) {
				drawSubmesh(mesh, i, params, master_col, master_spe, drawlist);
			}
		}
	}
}
bool MeshRendererCo::getAabb(Vec3 *min_point, Vec3 *max_point) {
	KMesh *mesh = getMesh();
	if (mesh) {
		return mesh->getAabb(min_point, max_point);
	}
	return false;
}
void MeshRendererCo::onQueryGroupingImageSize(int *w, int *h, Vec3 *pivot) {
	KMesh *mesh = getMesh();

	/// メッシュが毎フレーム書き換わるような場合、ここで毎フレームことなるサイズを返すと、
	/// 毎フレームレンダーテクスチャを再生成することになってしまう。
	/// それを防ぐため、現在のレンダーテクスチャよりも大きなサイズが必要になった場合のみ値を更新する
	Vec3 minp, maxp;
	if (mesh->getAabb(&minp, &maxp)) {
		int aabb_w = (int)(maxp.x - minp.x);
		int aabb_h = (int)(maxp.y - minp.y);
		aabb_w += 2; // margin
		aabb_h += 2; // margin
		if (aabb_w > bound_grouping_rentex_w_) {
			bound_grouping_rentex_w_ = aabb_w;
		}
		if (aabb_w > bound_grouping_rentex_h_) {
			bound_grouping_rentex_h_ = aabb_h;
		}
	}
	if (w) *w = bound_grouping_rentex_w_;
	if (h) *h = bound_grouping_rentex_h_;
	if (pivot) *pivot = Vec3();
}
#pragma endregion // MeshRendererCo












#pragma region RenderSystem
RenderSystem::RenderSystem() {
	master_adj_snap_ = MO_VIDEO_ENABLE_HALF_PIXEL;
	master_adj_half_ = MO_VIDEO_ENABLE_HALF_PIXEL;
	engine_ = NULL;
	hs_ = NULL;
	num_draw_nodes_ = 0;
	num_drawlist_ = 0;
}
RenderSystem::~RenderSystem() {
	onEnd();
}
void RenderSystem::init(Engine *engine) {
	Log_trace;
	Log_assert(engine);
	engine_ = engine;
	engine_->addSystem(this);
	engine_->addScreenCallback(this);
	hs_ = NULL;
}
void RenderSystem::onEntityInspectorGui(EID e) {
	_Node *node = _getNode(e);
	if (node) {
		node->renderer->onInspector();
	}
}
void RenderSystem::onStart() {
	Log_assert(engine_);
	hs_ = engine_->findSystem<HierarchySystem>();
	Log_assert(hs_);
}
void RenderSystem::onEnd() {
	for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
		_Node &node = it->second;
		delete node.renderer;
		// sprite_renderer, mesh_renderer, text_renderer は renderer の別名なので、delete するのは renderer だけ。
	}
	nodes_.clear();
	engine_ = NULL;
	hs_ = NULL;
}
// 描画しないレンダラーをアタッチする。
// inherit color など、描画のためのパラメータをツリー管理するためだけにレンダラーが必要な場合に使う
void RenderSystem::attachNull(EID e) {
	attachMesh(e);
}

void RenderSystem::attachMesh(EID e) {
	if (e == NULL) return;
	_Node *pNode = _getNode(e);
	if (pNode == NULL) {
		_Node node;
		node.entity = e;
		node.renderer = NULL;
		node.sprite_renderer = NULL;
		node.mesh_renderer = NULL;
		nodes_[e] = node;
		pNode = &nodes_[e];
	}
	if (pNode->mesh_renderer == NULL) {
		if (pNode->renderer) {
			Log_errorf(u8"MeshRendererCo をアタッチできません。既に %s がアタッチされています", typeid(*pNode->renderer).name());
			return;
		}
		pNode->mesh_renderer = new MeshRendererCo();
		pNode->mesh_renderer->entity_ = e;
		pNode->renderer = pNode->mesh_renderer;
	}
}
void RenderSystem::attachSprite(EID e) {
	if (e == NULL) return;
	_Node *pNode = _getNode(e);
	if (pNode == NULL) {
		_Node node;
		node.entity = e;
		node.renderer = NULL;
		node.sprite_renderer = NULL;
		node.mesh_renderer = NULL;
		nodes_[e] = node;
		pNode = &nodes_[e];
	}
#if SS_TEST
	if (pNode->sprite_renderer == NULL) {
		pNode->sprite_renderer = new SpriteRendererCo();
		pNode->sprite_renderer->entity_ = e;
	}
#else
	if (pNode->sprite_renderer == NULL) {
		if (pNode->renderer) {
			Log_errorf(u8"SpriteRendererCo をアタッチできません。既に %s がアタッチされています", typeid(*pNode->renderer).name());
			return;
		}
		pNode->sprite_renderer = new SpriteRendererCo();
		pNode->sprite_renderer->entity_ = e;
		pNode->renderer = pNode->sprite_renderer;
	}
#endif
}
SpriteRendererCo * RenderSystem::getSpriteRenderer(EID e) {
	_Node *node = _getNode(e);
	return node ? node->sprite_renderer : NULL;
}
bool RenderSystem::copySpriteRenderer(EID e, EID source) {
	SpriteRendererCo *dst_co = getSpriteRenderer(e);
	SpriteRendererCo *src_co = getSpriteRenderer(source);
	if (dst_co && src_co) {
		dst_co->copyFrom(src_co);
		return true;
	}
	return false;
}
Color RenderSystem::getMasterColor(EID e) const {
	const _Node *node = _getNode(e);
	return node ? node->renderer->master_color_ : Color::WHITE;
}
Color RenderSystem::getMasterColorInTree(EID e) const {
	Color color = Color::WHITE;

	// 親をたどりながら、それぞれのレンダラーのカラー設定を見る。
	// 全ての親がレンダラーを持っているとは限らないことに注意。
	// レンダラーが無い場合でも無視して親をたどっていく
	while (e) {
		const _Node *node = _getNode(e);
		if (node) {
			Color c = node->renderer->master_color_;
			color *= c; // MUL合成
			if (!node->renderer->inherit_color_) {
				break; // 継承がOFFになっているのでこれ以上親を見ない
			}
		}
		e = hs_->getParent(e);
	}
	return color;
}
Color RenderSystem::getMasterSpecular(EID e) const {
	const _Node *node = _getNode(e);
	return node ? node->renderer->master_specular_ : Color::ZERO;
}
Color RenderSystem::getMasterSpecularInTree(EID e) const {
	Color specular = Color::ZERO;

	// 親をたどりながら、それぞれのレンダラーのカラー設定を見る。
	// 全ての親がレンダラーを持っているとは限らないことに注意。
	// レンダラーが無い場合でも無視して親をたどっていく
	while (e) {
		const _Node *node = _getNode(e);
		if (node) {
			Color c = node->renderer->master_specular_;
			specular += c; // スペキュラはADD合成
			if (!node->renderer->inherit_specular_) {
				break; // 継承がOFFになっているのでこれ以上親を見ない
			}
		}
		e = hs_->getParent(e);
	}
	return specular;
}
void RenderSystem::setInheritColor(EID e, bool b) {
	_Node *node = _getNode(e);
	if (node) {
		Log_assert(node->renderer);
		node->renderer->inherit_color_ = b;
	}
}
void RenderSystem::setInheritSpecular(EID e, bool b) {
	_Node *node = _getNode(e);
	if (node) {
		Log_assert(node->renderer);
		node->renderer->inherit_specular_ = b;
	}
}
void RenderSystem::setWorldLayer(EID e, int layer) {
	_Node *node = _getNode(e);
	if (node) {
		node->renderer->world_layer_ = layer;
	}
}
int RenderSystem::getWorldLayer(EID e) const {
	const _Node *node = _getNode(e);
	if (node) {
		return node->renderer->world_layer_;
	}
	return 0;
}

void RenderSystem::setGrouping(EID e, bool value) {
	_Node *node = _getNode(e);
	if (node && node->renderer) {
		node->renderer->setGrouping(value);
	}
}
bool RenderSystem::isGrouping(EID e) const {
	const _Node *node = _getNode(e);
	if (node && node->renderer) {
		return node->renderer->isGrouping();
	}
	return false;
}
Material * RenderSystem::getGroupingMaterial(EID e) {
	_Node *node = _getNode(e);
	if (node && node->renderer) {
		return node->renderer->getGroupingMaterial();
	}
	return NULL;
}
void RenderSystem::setGroupingImageSize(EID e, int w, int h, const Vec3 &pivot) {
	_Node *node = _getNode(e);
	if (node && node->renderer) {
		node->renderer->setGroupingImageSize(w, h, pivot);
	}
}
void RenderSystem::getGroupingImageSize(EID e, int *w, int *h, Vec3 *pivot) {
	_Node *node = _getNode(e);
	if (node && node->renderer) {
		node->renderer->getGroupingImageSize(w, h, pivot);
	}
}
void RenderSystem::setRenderTexture(EID e, const Path &name) {
	_Node *node = _getNode(e);
	if (node && node->renderer) {
		node->renderer->setRenderTexture(name);
	}
}
TEXID RenderSystem::getRenderTexture(EID e) {
	_Node *node = _getNode(e);
	if (node && node->renderer) {
		return node->renderer->getRenderTexture();
	}
	return NULL;
}
void RenderSystem::setTransformInRenderTexture(EID e, const Matrix4 &matrix) {
	_Node *node = _getNode(e);
	if (node && node->renderer) {
		node->renderer->setTransformInRenderTexture(matrix);
	}
}
const Matrix4 & RenderSystem::getTransformInRenderTexture(EID e) const {
	const _Node *node = _getNode(e);
	if (node && node->renderer) {
		return node->renderer->getTransformInRenderTexture();
	}
	static const Matrix4 s_identity;
	return s_identity;
}


bool RenderSystem::getOffsetAabb(EID e, Vec3 *minpoint, Vec3 *maxpoint) {
	_Node *node = _getNode(e);
	if (node && node->renderer) {
		return node->renderer->getOffsetAabb(minpoint, maxpoint);
	}
	return false;
}
TEXID RenderSystem::getGroupRenderTexture(EID e) {
	_Node *node = _getNode(e);
	if (node && node->renderer) {
		return node->renderer->getGroupRenderTexture();
	}
	return NULL;
}
void RenderSystem::setGroupRenderTexture(EID e, TEXID rentex) {
	_Node *node = _getNode(e);
	if (node && node->renderer) {
		node->renderer->setGroupRenderTexture(rentex);
	}
}
KMesh * RenderSystem::getGroupRenderMesh(EID e) {
	_Node *node = _getNode(e);
	if (node && node->renderer) {
		return node->renderer->getGroupRenderMesh();
	}
	return NULL;
}
void RenderSystem::setGroupRenderMesh(EID e, const KMesh &mesh) {
	_Node *node = _getNode(e);
	if (node && node->renderer) {
		node->renderer->setGroupRenderMesh(mesh);
	}
}
const Vec3 & RenderSystem::getGroupRenderPivot(EID e) const {
	const _Node *node = _getNode(e);
	if (node && node->renderer) {
		return node->renderer->getGroupRenderPivot();
	}
	static const Vec3 s_ZERO;
	return s_ZERO;
}
void RenderSystem::setGroupRenderPivot(EID e, const Vec3 &p) {
	_Node *node = _getNode(e);
	if (node && node->renderer) {
		node->renderer->setGroupRenderPivot(p);
	}
}
void RenderSystem::updateGroupRenderTextureAndMesh(EID e) {
	_Node *node = _getNode(e);
	if (node && node->renderer) {
		node->renderer->updateGroupRenderTextureAndMesh();
	}
}
void RenderSystem::setLocalTransform(EID e, const Matrix4 &matrix) {
	_Node *node = _getNode(e);
	if (node && node->renderer) {
		node->renderer->local_transform_ = matrix;
	}
}
const Matrix4 & RenderSystem::getLocalTransform(EID e) const {
	const _Node *node = _getNode(e);
	if (node && node->renderer) {
		return node->renderer->local_transform_;
	}
	static const Matrix4 s_identity;
	return s_identity;
}
void RenderSystem::setOffset(EID e, const Vec3 &offset) {
	_Node *node = _getNode(e);
	if (node && node->renderer) {
		node->renderer->offset_ = offset;
	}
}
const Vec3 & RenderSystem::getOffset(EID e) const {
	const _Node *node = _getNode(e);
	if (node && node->renderer) {
		return node->renderer->offset_;
	}
	static const Vec3 s_ZERO;
	return s_ZERO;
}
void RenderSystem::setFlag(EID e, Flag flag, bool value) {
	_Node *node = _getNode(e);
	if (node) {
		switch (flag) {
		case ADJ_SNAP:
			node->renderer->adj_snap_ = value;
			break;
		case ADJ_HALF:
			node->renderer->adj_half_ = value;
			break;
		case INHERIT_COLOR:
			node->renderer->inherit_color_ = value;
			break;
		case INHERIT_SPECULAR:
			node->renderer->inherit_specular_ = value;
			break;
		}
	}
}
bool RenderSystem::getFlag(EID e, Flag flag) const {
	const _Node *node = _getNode(e);
	if (node) {
		switch (flag) {
		case ADJ_SNAP:
			return node->renderer->adj_snap_;
		case ADJ_HALF:
			return node->renderer->adj_half_;
		case INHERIT_COLOR:
			return node->renderer->inherit_color_;
		case INHERIT_SPECULAR:
			return node->renderer->inherit_specular_;
		}
	}
	return false;
}
KMesh * RenderSystem::getMesh(EID e) {
	_Node *node = _getNode(e);
	if (node && node->mesh_renderer) {
		return node->mesh_renderer->getMesh();
	}
	return NULL;
}
const KMesh * RenderSystem::getMesh(EID e) const {
	const _Node *node = _getNode(e);
	if (node && node->mesh_renderer) {
		return node->mesh_renderer->getMesh();
	}
	return NULL;
}
void RenderSystem::setSubMeshBlend(EID e, int submesh, Blend blend) {
	const _Node *node = _getNode(e);
	if (node && node->mesh_renderer) {
		Material *mat = node->mesh_renderer->getSubMeshMaterialAnimatable(submesh);
		if (mat) {
			mat->blend = blend;
		}
	}
}
void RenderSystem::setSubMeshTexture(EID e, int submesh, TEXID texid) {
	const _Node *node = _getNode(e);
	if (node && node->mesh_renderer) {
		Material *mat = node->mesh_renderer->getSubMeshMaterialAnimatable(submesh);
		if (mat) {
			mat->texture = texid;
		}
	}
}
void RenderSystem::setSubMeshTexture(EID e, int submesh, const Path &texname) {
	Log_assert(engine_);
	Log_assert(engine_->videobank());
	auto tex_bank = engine_->videobank()->getTextureBank();
	Log_assert(tex_bank);
	TEXID texid = tex_bank->findTextureRaw(texname, true);
	setSubMeshTexture(e, submesh, texid);
}
void RenderSystem::setSubMeshFilter(EID e, int submesh, Filter filter) {
	const _Node *node = _getNode(e);
	if (node && node->mesh_renderer) {
		Material *mat = node->mesh_renderer->getSubMeshMaterialAnimatable(submesh);
		if (mat) {
			mat->filter = filter;
		}
	}
}
void RenderSystem::setSubMeshColor(EID e, int submesh, const Color &color) {
	const _Node *node = _getNode(e);
	if (node && node->mesh_renderer) {
		Material *mat = node->mesh_renderer->getSubMeshMaterialAnimatable(submesh);
		if (mat) {
			mat->color = color;
		}
	}
}
void RenderSystem::setSubMeshSpecular(EID e, int submesh, const Color &specular) {
	const _Node *node = _getNode(e);
	if (node && node->mesh_renderer) {
		Material *mat = node->mesh_renderer->getSubMeshMaterialAnimatable(submesh);
		if (mat) {
			mat->specular = specular;
		}
	}
}
Material * RenderSystem::getSubMeshMaterial(EID e, int submesh) {
	_Node *node = _getNode(e);
	if (node && node->mesh_renderer) {
		return node->mesh_renderer->getSubMeshMaterialAnimatable(submesh);
	} else {
		return NULL;
	}
}
const Material * RenderSystem::getSubMeshMaterial(EID e, int submesh) const {
	const _Node *node = _getNode(e);
	if (node && node->mesh_renderer) {
		return node->mesh_renderer->getSubMeshMaterialAnimatable(submesh);
	} else {
		return NULL;
	}
}
int RenderSystem::getSubMeshMaterialCount(EID e) const {
	const _Node *node = _getNode(e);
	if (node && node->mesh_renderer) {
		return node->mesh_renderer->getSubMeshMaterialCount();
	}
	return 0;
}
const Matrix4 & RenderSystem::getSubMeshTransform(EID e, int index) const {
	const _Node *node = _getNode(e);
	if (node && node->mesh_renderer) {
		return node->mesh_renderer->getSubMeshTransform(index);
	}
	static const Matrix4 s_identity;
	return s_identity;
}
void RenderSystem::setSubMeshTransform(EID e, int index, const Matrix4 &m) {
	const _Node *node = _getNode(e);
	if (node && node->mesh_renderer) {
		node->mesh_renderer->setSubMeshTransform(index, m);
	}
}

bool RenderSystem::onAttach(EID e, const char *type) {
	if (strcmp(type, "MeshRenderer") == 0) {
		attachMesh(e);
		return true;
	}
#if 0
	if (strcmp(type, "SpriteRenderer") == 0) {
		attachSprite(e);
		return true;
	}
#endif
	return false;
}
void RenderSystem::onDetach(EID e) {
	Log_assert(e);
	auto it = nodes_.find(e);
	if (it != nodes_.end()) {
		_Node &node = it->second;
		delete node.renderer;

#if SS_TEST
		if (node.sprite_renderer) {
			delete node.sprite_renderer;
		}
#else
		// sprite_renderer, mesh_renderer, text_renderer は renderer の別名なので、delete するのは renderer だけ。
#endif
		nodes_.erase(it);
	}
}
bool RenderSystem::setParameter(EID e, const char *key, const char *val) {
	if (_getNode(e) == NULL) {
		return false;
	}
	if (strcmp(key, "world_layer") == 0) {
		setWorldLayer(e, K_strtoi(val));
		return true;
	}
	if (strcmp(key, "offset.x") == 0) {
		Vec3 offset = getOffset(e);
		offset.x = K_strtof(val);
		setOffset(e, offset);
		return true;
	}
	if (strcmp(key, "offset.y") == 0) {
		Vec3 offset = getOffset(e);
		offset.y = K_strtof(val);
		setOffset(e, offset);
		return true;
	}
	if (strcmp(key, "offset.z") == 0) {
		Vec3 offset = getOffset(e);
		offset.z = K_strtof(val);
		setOffset(e, offset);
		return true;
	}
	return false;
}
RenderSystem::_Node * RenderSystem::_getNode(EID e) {
	auto it = nodes_.find(e);
	return (it != nodes_.end()) ? &it->second : NULL;
}
const RenderSystem::_Node * RenderSystem::_getNode(EID e) const {
	auto it = nodes_.find(e);
	return (it != nodes_.end()) ? &it->second : NULL;
}
void RenderSystem::onFrameEnd() {
}
class RenderInspectorSortPred {
	HierarchySystem *hs_;
public:
	RenderInspectorSortPred(HierarchySystem *h) { 
		hs_ = h;
	}
	bool operator()(const RenderSystem::_Node *n1, const RenderSystem::_Node *n2) const {
		const Path &s1 = hs_->getName(n1->entity);
		const Path &s2 = hs_->getName(n2->entity);
		return s1.compare(s2) < 0;
	}
};
void RenderSystem::onInspectorGui() {
#ifndef NO_IMGUI
	ImGui::Text("Total Entities: %d", nodes_.size());

	if (1) {
		ImGui::Separator();
		ImGui::Checkbox("Snap to integer coordinate", &master_adj_snap_);
		ImGui::Checkbox("Offset half pixel", &master_adj_half_);
	}

	if (1) {
		HierarchySystem *hs = g_engine->findSystem<HierarchySystem>();
		EID camera = hs->findEntity("MainCamera");
		int x, y;
		g_engine->getWindow()->getMousePosition(&x, &y, false);
		Vec3 scr = g_engine->getScreen()->windowClientToScreenPoint(Vec3(x, y, 0));
		EntityList list;
		getEntityByScreenPoint(scr, camera, &list);
		for (size_t i=0; i<list.size(); i++) {
			EID e = list[i];
			ImGui::BulletText("[%d] %s", i, hs->getNameInTree(e).u8());
		}
		if (g_engine->getInspector() && list.size()>0) {
			g_engine->getInspector()->setHighlightedEntity(list.front());
		}
	}
	int dc = 0;
	Video::getParameter(Video::DRAWCALLS, &dc);
	ImGui::Text("Draw calls  = %d", dc);
	ImGui::Text("Draw nodes  = %d", num_draw_nodes_);
	ImGui::Text("Draw list   = %d", num_drawlist_);
	num_draw_nodes_ = 0;
	num_drawlist_ = 0;

	if (ImGui::TreeNode("Nodes")) {
		tmp_nodes_.clear();
		for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
			tmp_nodes_.push_back(&it->second);
		}
		std::sort(tmp_nodes_.begin(), tmp_nodes_.end(), RenderInspectorSortPred(hs_));
		for (auto it=tmp_nodes_.begin(); it!=tmp_nodes_.end(); ++it) {
			const _Node *node = *it;
			Path name = hs_->getNameInTree(node->entity);
			ImGui::Text("[#0x%x] %s (%.2fms)", node->entity, name.u8(), node->debug_time_spent_msec);
			if (ImGui::IsItemHovered()) {
				Path path = hs_->getNameInTree(node->entity);
				ImGui::SetTooltip("EntityPath: %s", path.u8());
			}
		}
		ImGui::TreePop();
	}
#endif // !NO_IMGUI
}
void RenderSystem::_dirtyMeshTree(EID e) {
	int num = hs_->getChildCount(e);
	for (int i=0; i<num; i++ ) {
		EID sub = hs_->getChild(e, i);
		_Node *subnode = _getNode(sub);
		_dirtyMeshTree(sub);
	}
}
void RenderSystem::setMasterColor(EID e, const Color &color) {
	_Node *node = _getNode(e);
	if (node == NULL) return;
	node->renderer->master_color_ = color;

	// 子エンティティのレンダラーも影響を受ける
	_dirtyMeshTree(e);
}
void RenderSystem::setMasterAlpha(EID e, float a) {
	_Node *node = _getNode(e);
	if (node == NULL) return;
	node->renderer->master_color_.a = a;

	// 子エンティティのレンダラーも影響を受ける
	_dirtyMeshTree(e);
}
void RenderSystem::setMasterSpecular(EID e, const Color &specular) {
	_Node *node = _getNode(e);
	if (node == NULL) return;
	node->renderer->master_specular_ = specular;

	// 子エンティティのレンダラーも影響を受ける
	_dirtyMeshTree(e);
}
void RenderSystem::onRenderWorld_make_tmp_nodes(RenderWorldParameters *params) {

	// インスペクターによる選別を行う。
	// インスペクターが定義かつ表示されていれば、その設定にしたがって
	// 描画するべきノードを抽出する
	tmp_nodes2_.clear();
	{
		bool copy_all = true;

		#ifndef NO_IMGUI
		{
			Inspector *inspector = engine_->getInspector();
			if (inspector) {
				copy_all = false;
				if (params->debug_selections_only) {
					// 選択済みノードのみ表示する
					for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
						_Node &node = it->second;
						if (inspector->entityShouldRednerAsSelected(node.entity)) {
							tmp_nodes2_.push_back(&node);
						}
					}
				} else {
					// 表示設定されているノードのみ表示する
					for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
						_Node &node = it->second;
						if (inspector->isEntityVisible(node.entity)) {
							tmp_nodes2_.push_back(&node);
						}
					}
				}
			}
		}
		#endif // !NO_IMGUI

		// インスペクターによる選別なし。
		// 無条件でコピーする
		if (copy_all) {
			for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
				_Node &node = it->second;
				tmp_nodes2_.push_back(&node);
			}
		}
	}

	// エンティティ設定による選別を行う
	tmp_nodes_.clear();
	for (auto it=tmp_nodes2_.begin(); it!=tmp_nodes2_.end(); ++it) {
		_Node &node = *(*it);
		// 描画コンポーネントがないとダメ
		if (node.renderer == NULL) {
			continue;
		}
		// 描画コンポーネントが無効化されていたらダメ
		if (! node.renderer->enabled_) {
			continue;
		}
		// エンティティが無効化されていたらダメ
		if (! hs_->isEnabledInTree(node.entity)) {
			continue;
		}
		// エンティティが非表示だったらダメ
		if (! hs_->isVisibleInTree(node.entity)) {
			continue;
		}
		// カメラと描画グループが一致しないとダメ
		int e_layer = hs_->getLayerInTree(node.entity);
		if (e_layer != params->view.object_layer) {
			continue;
		}
		tmp_nodes_.push_back(&node);
	}

	// ソートする
	switch (params->view.object_order) {
	case RenderingOrder_TimeStamp:
	case RenderingOrder_Hierarchy:
		// ヒエラルキー上で親→子の順番になるようにソートする
		std::sort(tmp_nodes_.begin(), tmp_nodes_.end(), RendererHierarchySortPred2(hs_));
		break;
	case RenderingOrder_ZDepth:
		// エンティティのワールド座標が奥(Z大)→手前(Z小)の順番になるようにソートする。
		std::sort(tmp_nodes_.begin(), tmp_nodes_.end(), RendererDepthSortPred2(hs_));
		break;
	case RenderingOrder_Custom:
		// ユーザー定義のソートを行う
		onCustomSorting2(tmp_nodes_);
		break;
	}
}
void RenderSystem::onRenderWorld(RenderWorldParameters *params) {
	// tmp_nodes_ を更新する
	// tmp_nodes_ には本当に描画するべきノードが入る
	onRenderWorld_make_tmp_nodes(params);
	num_draw_nodes_ += tmp_nodes_.size(); // onRenderWorld はカメラごとに呼ばれるので、代入ではなく加算する

#if USING_DRAWLIST
	// 描画リスト使う
//	MO_CODETIME_BEGIN;
	{
		s_drawlist.clear();
		KClock clock;
		for (auto it=tmp_nodes_.begin(); it!=tmp_nodes_.end(); ++it) {
			_Node *node = *it;
			clock.reset();

			Matrix4 world_matrix;
			hs_->getLocalToWorldMatrix(node->entity, &world_matrix);

			RenderParams rp = params->render_params; // copy
			rp.transform_matrix = world_matrix * rp.transform_matrix; // エンティティ座標を適用
			mo::translateInPlace(rp.transform_matrix, params->view.rendering_offset); // カメラの描画オフセットを適用
			node->renderer->draw(this, rp, &s_drawlist);
			node->debug_time_spent_msec = clock.getTimeMsecF();
		}
		s_drawlist.endlist();
		s_drawlist.draw();
	}
//	MO_CODETIME_END;
	num_drawlist_ += s_drawlist.size(); // onRenderWorld はカメラごとに呼ばれるので、代入ではなく加算する
	return;
#else
	// 描画リスト使わない
//	MO_CODETIME_BEGIN;
	{
		KClock clock;
		for (auto it=tmp_nodes_.begin(); it!=tmp_nodes_.end(); ++it) {
			_Node *node = *it;
			clock.reset();
			Matrix4 world_matrix;
			hs_->getWorldMatrix(node->entity, &world_matrix);
			RenderParams rp = params->render_params; // copy
			rp.transform_matrix = world_matrix * rp.transform_matrix; // エンティティ座標を適用
			mo::translateInPlace(rp.transform_matrix, params->view.rendering_offset); // カメラの描画オフセットを適用
			node->renderer->draw(this, rp, NULL);
			node->debug_time_spent_msec = clock.getTimeMsecF();
		}
	}
//	MO_CODETIME_END;
	return;
#endif
}

Vec3 vmin(const Vec3 &a, const Vec3 &b) {
	return Vec3(
		Num_min(a.x, b.x),
		Num_min(a.y, b.y),
		Num_min(a.z, b.z));
}
Vec3 vmax(const Vec3 &a, const Vec3 &b) {
	return Vec3(
		Num_max(a.x, b.x),
		Num_max(a.y, b.y),
		Num_max(a.z, b.z));
}

EID RenderSystem::getEntityByScreenPoint(const Vec3 &point_in_screen, EID camera_e, EntityList *entities) {
	Log_assert(engine_);
	Inspector *inspector = engine_->getInspector();
	
	CameraSystem *camera_sys = engine_->findSystem<CameraSystem>();
	Log_assert(camera_sys);

	Vec3 pos, dir;
	if (! camera_sys->screenToWorldVector(point_in_screen, camera_e, &pos, &dir)) {
		return NULL;
	}

	std::vector<const _Node *> tmp;

	for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
		const _Node *e_node = &it->second;

		// カメラに映らないモノを除外
		if (! camera_sys->isTargetOf(e_node->entity, camera_e)) continue;
		if (! hs_->isEnabledInTree(e_node->entity)) continue;
		if (! hs_->isVisibleInTree(e_node->entity)) continue;
#ifndef NO_IMGUI
		if (inspector && !inspector->isEntityVisible(e_node->entity)) continue;
#endif
		// AABBを取得
		Vec3 minp, maxp;
		if (! e_node->renderer->getAabb(&minp, &maxp)) continue;

		// AABBをワールド座標系で取得
		Vec3 _wmin = hs_->localToWorldPoint(e_node->entity, minp);
		Vec3 _wmax = hs_->localToWorldPoint(e_node->entity, maxp);
		if (! camera_sys->aabbWillRender(_wmin, _wmax, camera_e)) continue;

		// この時点で _wmin と _wmax の大小関係は正しくないことがある（左右反転していた場合など）
		// それを修正する
		Vec3 minInWorld = vmin(_wmin, _wmax);
		Vec3 maxInWorld = vmax(_wmin, _wmax);

		// 画面座標に変換
		Vec3 minInScreen;
		Vec3 maxInScreen;
		camera_sys->worldToScreenPoint(minInWorld, camera_e, &minInScreen);
		camera_sys->worldToScreenPoint(maxInWorld, camera_e, &maxInScreen);

		if (Num_in_range(point_in_screen.x, minInScreen.x, maxInScreen.x)) {
			if (Num_in_range(point_in_screen.y, minInScreen.y, maxInScreen.y)) {
				tmp.push_back(e_node);
			}
		}
	}
	if (tmp.empty()) {
		return NULL;
	}

	std::sort(tmp.begin(), tmp.end(), RendererDepthSortPred2(hs_));

	// tmpは奥から順番に並んでいるので、これを逆順にし、手前から順に並ぶようにしておく
	if (entities) {
		for (auto it=tmp.rbegin(); it!=tmp.rend(); it++) {
			entities->push_back((*it)->entity);
		}
	}

	// 一番手前を返す
	return tmp.back()->entity;
}
#pragma endregion



} // namespace




