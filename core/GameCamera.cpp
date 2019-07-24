#include "GameCamera.h"
//
#include "GameHierarchy.h"
#include "Engine.h"
#include "KImgui.h"
#include "KNum.h"
#include "KLog.h"
#include <algorithm>

namespace mo {

CameraSystem::CameraSystem() {
	engine_ = NULL;
	hs_ = NULL;
}
void CameraSystem::init(Engine *engine) {
	Log_trace;
	engine->addSystem(this);
	engine_ = engine;
	hs_ = NULL;
}
void CameraSystem::onStart() {
	hs_ = engine_->findSystem<HierarchySystem>();
}
void CameraSystem::attach(EID e) {
	if (e == NULL) return;
	Log_assert(e);
	if (_getNode(e)) return;

	Node node;
	node.entity_ = e;
	nodes_[e] = node;
}
bool CameraSystem::onAttach(EID e, const char *type) {
	if (type==NULL || strcmp(type, "Camera") == 0) {
		attach(e);
		return true;
	}
	return false;
}
void CameraSystem::onDetach(EID e) {
	nodes_.erase(e);
}
/// @internal
/// 斜投影行列作成
/// w, h, zn, zf: Ortho 行列作成のパラメータと同じ
/// dxdz: 剪断係数。Z増加に対するXの増加割合
/// dydz: 剪断係数。Z増加に対するYの増加割合
static Matrix4 _GetCabinetOrthoMatrix(float w, float h, float zn, float zf, float dxdz, float dydz) {
	// Cabinet projection
	// http://en.wikipedia.org/wiki/Oblique_projection
	// 遠クリップ面の設定に関係なく、画面上で一定の傾きにする為のスケーリング定数
	const float scale = (zf - zn) / (h * 0.5f);
	// この行列は幅高さが 1.0 に正規化された空間で適用されるため、
	// アスペクト比の補正が必要になることに注意
	// https://www.opengl.org/discussion_boards/showthread.php/164200-Cavalier-oblique-projection
	const float a = dxdz * scale * h / w;
	const float b = dydz * scale;
	const Matrix4 shear(
	    1, 0, 0, 0, // <-- input x
	    0, 1, 0, 0, // <-- input y
	    a, b, 1, 0, // <-- input z
	    0, 0, 0, 1  // <-- input w
	//  x  y  z  w  // <-- output
	);
	const Matrix4 ortho = Matrix4::fromOrtho(w, h, zn, zf);
	return ortho * shear;
}

static void _UpdateProjectionMatrix(View *view) {
	float w = view->projection_w();
	float h = view->projection_h();
	switch (view->projection_type) {
	case ProjectionType_Ortho:
		view->projection_matrix = Matrix4::fromOrtho(w, h, view->znear, view->zfar);
		break;
	case ProjectionType_OrthoCabinet:
		view->projection_matrix = _GetCabinetOrthoMatrix(w, h, view->znear, view->zfar, view->cabinet_dx_dz, view->cabinet_dy_dz);
		break;
	case ProjectionType_Frustum:
		view->projection_matrix = Matrix4::fromFrustum(w, h, view->znear, view->zfar);
		break;
	}
}

static bool _ViewGui(View *view) {
	bool changed = false;
#ifndef NO_IMGUI
	const char *proj_names[] = {
		"Ortho",
		"OrthoCabinet",
		"Frustum",
	};
	ImGui::Text("Projection Size X: %g", view->projection_w());
	ImGui::Text("Projection Size Y: %g", view->projection_h());
	changed |= ImGui::DragFloat("Width",  &view->width);
	changed |= ImGui::DragFloat("Height", &view->height);
	changed |= ImGui::DragFloat("ZNear",  &view->znear);
	changed |= ImGui::DragFloat("ZFar",   &view->zfar);
	changed |= ImGui::DragFloat("Zoom",   &view->zoom);
	changed |= ImGui::DragFloat("Cabinet dx/dz", &view->cabinet_dx_dz);
	changed |= ImGui::DragFloat("Cabinet dy/dz", &view->cabinet_dy_dz);
	changed |= ImGui::DragFloat4("ViewportXY", (float*)&view->viewport);
	changed |= ImGui::DragInt("Rendering Order", &view->render_order);
	changed |= ImGui::Combo("Projection", (int *)&view->projection_type, proj_names, 3);
	changed |= ImGui::DragFloat3("Rendering offset", (float*)&view->rendering_offset);
	changed |= ImGui::ColorEdit4("Background", reinterpret_cast<float*>(&view->bgcolor));
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Clear backbuffer with specified color before rendering this camera");
	}
	if (1) {
		const char *names = "AUTO\0ITEMSTAMP\0HIERARCHY\0ZDEPTH\0\0";
		changed |= ImGui::Combo("Sort by", (int*)&view->object_order, names);
	}
	if (changed) {
		_UpdateProjectionMatrix(view);
	}
#endif // !NO_IMGUI
	return changed;
}

void CameraSystem::onEntityInspectorGui(EID e) {
#ifndef NO_IMGUI
	View view;
	if (getView(e, &view)) {
		if (_ViewGui(&view)) {
			setView(e, &view);
		}
	}
#endif // !NO_IMGUI
}
void CameraSystem::onInspectorGui() {
#ifndef NO_IMGUI
	Log_assert(engine_);
	ImGui::Text("Camera order");
	EntityList cameras;
	getCameraInOrder(cameras);
	for (size_t i=0; i<cameras.size(); i++) {
		ImGui::Text("[%d] %s", i, hs_->getName(cameras[i]).u8());
	}
#endif // !NO_IMGUI
}
CameraSystem::Node * CameraSystem::_getNode(EID e) {
	auto it = nodes_.find(e);
	return (it != nodes_.end()) ? &it->second : NULL;
}
const CameraSystem::Node * CameraSystem::_getNode(EID e) const {
	auto it = nodes_.find(e);
	return (it != nodes_.end()) ? &it->second : NULL;
}
void CameraSystem::onAppFrameUpdate() {
	for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
		EID e = it->first;

		View view;
		getView(e, &view);

		// 毎フレーム設定するのはほとんど無駄になるが、
		// レイヤーが変わった時にすぐに反映されるように、こうしている。
		// 本当はレイヤー切り替えイベントを取得して、その時にだけ書き換えるようにしたい
		view.object_layer = hs_->getLayerInTree(e); // カメラのレイヤー番号が、そのまま描画対象レイヤー番号になる。
		hs_->getWorldToLocalMatrix(e, &view.transform_matrix);
		setView(e, &view);
	}
}
void CameraSystem::onFrameUpdate() {
}
int CameraSystem::getCameraInOrder(EntityList &camera_in_order) const {
	Log_assert(engine_);
	tmp_.clear();
	for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
		EID e = it->first;
		Node *co = &it->second;
		if (co && co->enabled_ && hs_->isEnabled(e)) {
			tmp_.push_back(co);
		}
	}
	SortPred pred;
	pred.cs_ = this;
	std::sort(tmp_.begin(), tmp_.end(), pred);

	for (auto it=tmp_.begin(); it!=tmp_.end(); ++it) {
		Node *co = *it;
		camera_in_order.push_back(co->entity_);
	}
	return (int)tmp_.size();
}
int CameraSystem::getViewInOrder(std::vector<View> &viewlist) const {
	Log_assert(engine_);
	tmp_.clear();
	for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
		EID e = it->first;
		Node *co = &it->second;
		if (co && co->enabled_ && hs_->isEnabled(e)) {
			tmp_.push_back(co);
		}
	}
	SortPred pred;
	pred.cs_ = this;
	std::sort(tmp_.begin(), tmp_.end(), pred);

	for (auto it=tmp_.begin(); it!=tmp_.end(); ++it) {
		Node *co = *it;
		viewlist.push_back(co->view_);
	}
	return (int)tmp_.size();
}
EID CameraSystem::getCameraForObjectLayer(int layer) const {
	for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
		EID e = it->first;
		if (hs_->getLayerInTree(e) == layer) {
			return e;
		}
	}
	return NULL;
}

// カメラを描画順にソートする時の比較関数
bool CameraSystem::sortPred(Node *co1, Node *co2) const {
	int ord1 = co1->view_.render_order;
	int ord2 = co2->view_.render_order;
	if (ord1 != ord2) {
		// 優先度の低い方を先に描画する
		return ord1 < ord2;
	} else {
		// 優先度が等しい場合は、カメラをワールドに追加した順番で描画する
		int ser1 = hs_->getSerialIndex(co1->entity_);
		int ser2 = hs_->getSerialIndex(co2->entity_);
		return ser1 < ser2;
	}
}
EID CameraSystem::getCameraForEntity(EID e) const {
	return getCameraForObjectLayer(hs_->getLayerInTree(e));
}
bool CameraSystem::screenToWorldVector(const Vec3 &point_in_screen, EID camera_e, Vec3 *point_in_world, Vec3 *normalized_direction_in_world) const {
	Vec3 s0 = point_in_screen;
	Vec3 s1 = point_in_screen + Vec3(0, 0, 1); // 画面奥に向かうベクトル

	Vec3 p0 = screenToWorldPoint(s0, camera_e);
	Vec3 p1 = screenToWorldPoint(s1, camera_e);

	Vec3 delta = p1 - p0;
	if (! delta.isZero()) {
		if (point_in_world) *point_in_world = p0;
		if (normalized_direction_in_world) *normalized_direction_in_world = delta.getNormalized();
		return true;
	}
	return false;
}
Vec3 CameraSystem::screenToWorldPoint(const Vec3 &point_in_screen, EID camera_e) const {
	View view;
	if (!getView(camera_e, &view)) {
		return Vec3();
	}

	float w = view.projection_w();
	float h = view.projection_h();

	// SCREEN --> CAMERA LOCAL
	Vec3 lpos;
	lpos.x = Num_lerp(-w/2, w/2, Num_normalize(point_in_screen.x, -1.0f, 1.0f));
	lpos.y = Num_lerp(-h/2, h/2, Num_normalize(point_in_screen.y, -1.0f, 1.0f));
	lpos.z = view.znear + point_in_screen.z;

	// CAMERA LOCAL --> WORLD
	Vec3 wpos;
	{
		Vec3 campos = hs_->getPosition(camera_e);
		if (view.projection_type == ProjectionType_OrthoCabinet) {
			wpos.x = campos.x + lpos.x - lpos.z * view.cabinet_dx_dz;
			wpos.y = campos.y + lpos.y - lpos.z * view.cabinet_dy_dz;
			wpos.z = campos.z + lpos.z;
		} else {
			wpos.x = campos.x + lpos.x;
			wpos.y = campos.y + lpos.y;
			wpos.z = campos.z + lpos.z;
		}
	}
	return wpos;
}
bool CameraSystem::worldToScreenPoint(const Vec3 &p, EID camera_e, Vec3 *pos) const {
	View view;
	if (!getView(camera_e, &view)) {
		return false;
	}

	Matrix4 lm;
	hs_->getWorldToLocalMatrix(camera_e, &lm);
	Matrix4 proj_mat = view.projection_matrix;
	Vec3 lpos = lm.transform(p);
	Vec3 spos = proj_mat.transform(lpos);

	if (pos) *pos = spos;
	return true;
}
bool CameraSystem::aabbWillRender(const Vec3 &minpoint_inworld, const Vec3 &maxpoint_inworld, EID camera_e) const {
	View view;
	if (!getView(camera_e, &view)) {
		return false;
	}
	Matrix4 lm;
	hs_->getWorldToLocalMatrix(camera_e, &lm);
	Vec3 p0 = lm.transform(minpoint_inworld);
	Vec3 p1 = lm.transform(maxpoint_inworld);
		
	Matrix4 pm = view.projection_matrix;
	Vec3 s0 = pm.transform(p0);
	Vec3 s1 = pm.transform(p1);

	RectF obj_rect(s0.x, s0.y, s1.x, s1.y);
	RectF scr_rect = RectF(-1, -1, 1, 1).getInflatedRect(0.2f, 0.2f); // 念のため少し広めに判定しておく
	if (obj_rect.isIntersectedWith(scr_rect)) {
		return true;
	}
	return false;
}
bool CameraSystem::isTargetOf(EID e, const View &view) const {
	Log_assert(engine_);

	if (e == NULL) return false;

	// 描画グループが一致しないとダメ
	int L1 = view.object_layer;
	int L2 = hs_->getLayerInTree(e);
	if (L1 != L2) {
		return false;
	}
	return true;
}
bool CameraSystem::isTargetOf(EID e, EID camera_e) const {
	Log_assert(engine_);

	if (camera_e == NULL) return false;
	if (e == NULL) return false;

	// 描画グループが一致しないとダメ
	int L1 = hs_->getLayerInTree(camera_e);
	int L2 = hs_->getLayerInTree(e);
	if (L1 != L2) {
		return false;
	}
	return true;
}
void CameraSystem::copyCamera(EID dst, EID src) {
	Node *dnode = _getNode(dst);
	Node *snode = _getNode(src);
	if (dnode == NULL || snode == NULL || dnode == snode) return;
	
	// 名前までコピーしないように注意
	Path tmp = dnode->view_.name;
	dnode->view_ = snode->view_;
	dnode->view_.name = tmp;
	hs_->copyTransform(dst, src);
}
void CameraSystem::setOrderNumber(EID e, int o) {
	View view;
	if (getView(e, &view)) {
		view.render_order = o;
		setView(e, &view);
	}
}
int CameraSystem::getOrderNumber(EID e) const {
	View view;
	getView(e, &view);
	return view.render_order;
}
bool CameraSystem::getView(EID e, View *view) const {
	const Node *node = _getNode(e);
	if (node) {
		if (view) *view = node->view_;
		return true;
	}
	return false;
}
void CameraSystem::setView(EID e, const View *view) {
	Log_assert(view);
	Node *node = _getNode(e);
	if (node) {
		node->view_ = *view;
		_UpdateProjectionMatrix(&node->view_);
	}
}
void CameraSystem::setSort(EID e, RenderingOrder order) {
	View view;
	if (getView(e, &view)) {
		view.object_order = order;
		setView(e, &view);
	}
}
void CameraSystem::setZoom(EID e, float value) {
	View view;
	if (getView(e, &view)) {
		view.zoom = value;
		setView(e, &view);
	}
}
float CameraSystem::getZoom(EID e) const {
	View view;
	if (getView(e, &view)) {
		return view.zoom;
	}
	return 0.0f;
}



}
