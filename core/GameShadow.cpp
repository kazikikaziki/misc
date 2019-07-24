#include "GameShadow.h"
#include "Engine.h"
#include "GameCollision.h"
#include "GameRender.h"
#include "KImgui.h"
#include "KLog.h"
#include "KMatrix4.h"
#include "KNum.h"
#include "KVideoDef.h"

namespace mo {

#define GRADIENT_FILL 1 // グラデーション影を使う？
#define SHADOW_BASIC_RADIUS  64

static void _MakeCircleMesh(KMesh *mesh, float x, float y, float z, float xradius, float yradius, int numvertices, const Color &center_color, const Color &outer_color) {
	Log_assert(mesh);
	Log_assert(numvertices >= 3);
	mesh->clear();
	Vertex *v = mesh->lock(0, 1+numvertices);
	if (v) {
		// 中心
		{
			v[0].pos = Vec3();
			v[0].tex = Vec2();
			v[0].dif = center_color;
		}
		// 円周
		for (int i=0; i<numvertices; i++) {
			float t = Num_PI * 2 * i / (numvertices-1);
			v[1+i].pos = Vec3(xradius * cosf(t), yradius * sinf(t), 0.0f);
			v[1+i].tex = Vec2();
			v[1+i].dif = outer_color;
		}
		mesh->unlock();
	}
	{
		KSubMesh sub;
		sub.start = 0;
		sub.count = numvertices+1;
		sub.primitive = Primitive_TriangleFan;
		mesh->addSubMesh(sub);
	}
}


ShadowSystem::ShadowSystem() {
	vertex_count_ = 24;
	max_height_ = 400;
	color_ = Color(0.0f, 0.0f, 0.0f, 0.5f);
	blend_ = Blend_Alpha;
	scale_by_height_ = true;
	world_layer_ = 1;
	use_y_position_as_altitude_ = false;
	engine_ = NULL;
	default_radius_x_ = 32;
	default_radius_y_ = 12;
	cs_ = NULL;
	hs_ = NULL;
	rs_ = NULL;
}
void ShadowSystem::init(Engine *engine) {
	Log_trace;
	engine->addSystem(this);
	engine_ = engine;
}
void ShadowSystem::onStart() {
	Log_assert(engine_);
	cs_ = engine_->findSystem<CollisionSystem>();
	rs_ = engine_->findSystem<RenderSystem>();
	hs_ = engine_->findSystem<HierarchySystem>();
	Log_assert(cs_);
	Log_assert(rs_);
	Log_assert(hs_);
	updateMesh();
}
void ShadowSystem::attach(EID e) {
	if (_getNode(e)) return;
	_Node node;
	node.shadow_e = createShadowEntity(e);
	node.radius_x = default_radius_x_;
	node.radius_y = default_radius_y_;
	node.scale = 1.0f;
	node.scale_by_height = true;
	node.max_height = 0;
	nodes_[e] = node;
}
bool ShadowSystem::onAttach(EID e, const char *type) {
	if (type==NULL || strcmp(type, "ShadowCast") == 0) {
		attach(e);
		return true;
	}
	return false;
}
void ShadowSystem::onDetach(EID e) {
	Log_assert(engine_);

	auto it = nodes_.find(e);
	if (it != nodes_.end()) {
		_Node &node = it->second;

		// 影も一緒に消す
		hs_->removeTree(node.shadow_e);
		nodes_.erase(it);
	}
}
void ShadowSystem::setEnabled(EID e, bool value) {
	_Node *node = _getNode(e);
	if (node) node->enabled = value;
}
void ShadowSystem::setOffset(EID e, const Vec3 &value) {
	_Node *node = _getNode(e);
	if (node) node->offset = value;
}
void ShadowSystem::setRadius(EID e, float horz, float vert) {
	_Node *node = _getNode(e);
	if (node) {
		node->radius_x = horz;
		node->radius_y = vert;
	}
}
bool ShadowSystem::getRadius(EID e, float *horz, float *vert) {
	_Node *node = _getNode(e);
	if (node) {
		if (horz) *horz = node->radius_x;
		if (vert) *vert = node->radius_y;
		return true;
	}
	return false;
}
void ShadowSystem::setScaleFactor(EID e, float value) {
	_Node *node = _getNode(e);
	if (node) node->scale = value;
}
void ShadowSystem::setScaleByHeight(EID e, bool value, float maxheight) {
	_Node *node = _getNode(e);
	if (node) {
		node->scale_by_height = value;
		node->max_height = maxheight;
	}
}
void ShadowSystem::setMatrix(EID e, const Matrix4 &matrix) {
	_Node *node = _getNode(e);
	if (node) {
		rs_->setLocalTransform(node->shadow_e, matrix);
	}
}

bool ShadowSystem::getAltitude(EID e, float *alt) const {
	Log_assert(engine_);

	// Ｙ座標がそのまま高度を表す？
	if (use_y_position_as_altitude_) {
		if (alt) *alt = hs_->getPosition(e).y;
		return true;
	}

	// Body を持つなら、Body の高度情報を利用する
	// ただし、enabled=false の場合高度情報が更新されないので注意
	if (cs_->isBodyEnabled(e)) {
		return cs_->getBodyAltitude(e, alt);
	}

	// Body が利用できないなら、コリジョンワールドに高度を問い合わせる
	{
		Vec3 pos = hs_->getPositionInWorld(e);
		float ground_y;
		float max_penetration = 16;
		if (cs_->getGroundPoint(pos, max_penetration, &ground_y)) {
			if (alt) *alt = pos.y - ground_y;
			return true;
		}
	}

	// 地面がない。奈落。
	return false;
}
ShadowSystem::_Node * ShadowSystem::_getNode(EID e) {
	auto it = nodes_.find(e);
	if (it != nodes_.end()) {
		return &it->second;
	}
	return NULL;
}
bool ShadowSystem::_computeShaodwPositionAndScale(EID owner_e, _Node &node, Vec3 *p, Vec3 *s) const {
	Log_assert(engine_);
	Log_assert(p);

	if (node.scale <= 0) return false;
	if (node.radius_x == 0) return false;
	if (node.radius_y == 0) return false;

	// 高度を得る
	float alt;
	if (getAltitude(owner_e, &alt)) {
		// 影サイズを設定
		{
			// 標準影サイズから各エンティティの標準サイズへのスケーリング
			float lscale_x = node.scale * node.radius_x / SHADOW_BASIC_RADIUS;
			float lscale_y = node.scale * node.radius_y / SHADOW_BASIC_RADIUS;
			// 高度による自動スケーリング値
			float hscale = 1.0f;
			if (scale_by_height_ && node.scale_by_height) {

				// 最大高度。
				// ノードに設定されている値を使う。それが 0 なら規定値を使う
				float maxheight = node.max_height;
				if (maxheight <= 0) {
					maxheight = max_height_;
				}

				float k = Num_normalize(alt, maxheight, 0.0f);
				hscale = Num_lerp(0.0f, 1.0f, k);
			}
			// 設定
			*s = Vec3(lscale_x*hscale, lscale_y*hscale, 1.0f);
		}

		// キャラクターオブジェクトのAABB底面
		float owner_body_bottom = 0;
		if (cs_->isBodyEnabled(owner_e)) {
			BodyShape shape;
			Vec3 _min;
			cs_->getBodyAabb(owner_e, &_min, NULL);
			owner_body_bottom = _min.y;
		}

		// 影の位置
		Vec3 owner_pos = hs_->getPositionInWorld(owner_e);
		Vec3 pos = owner_pos + node.offset;
		pos.y += owner_body_bottom - alt; // alt はAABB底面から地面までの距離であることに注意
		pos.z += 1; // 親オブジェクトと完全に座標が一致した場合でも奥側に描画されるよう、少しだけ座標をずらす

		*p = pos;
		return true;
	}
	return false;
}
void ShadowSystem::onFrameUpdate() {
	Log_assert(engine_);

	// 影用のエンティティ
	for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
		_Node &node = it->second;
		EID owner_e = it->first;
		Vec3 p, s;
		if (_computeShaodwPositionAndScale(owner_e, node, &p, &s)) {
			hs_->setScale(node.shadow_e, s);
			hs_->setPosition(node.shadow_e, p);
		} else {
			// 影はどこにも映らない
			// visible = false とやりたいところだが、影の表示・非表示が外部から直接指定されている可能性もあるので、
			// エンティティの visible や enabled はなるべく弄りたくない。サイズをゼロにしておく
			hs_->setScale(node.shadow_e, Vec3());
		}
	}
}
void ShadowSystem::onEntityInspectorGui(EID e) {
#ifndef NO_IMGUI
	_Node *node = _getNode(e);
	if (node == NULL) return;

	bool chg = false;
	chg |= ImGui::DragFloat("RadiusX", &node->radius_x);
	chg |= ImGui::DragFloat("RadiusY", &node->radius_y);
	chg |= ImGui::DragFloat("Scale",   &node->scale);
	chg |= ImGui::DragFloat3("Offset", (float*)&node->offset);
	chg |= ImGui::Checkbox("Scale by height(True)", &node->scale_by_height);
	if (ImGui::DragFloat("MaxHeight", &node->max_height)) {
		node->max_height = Num_max(node->max_height, 0.0f);
		chg = true;
	}
	if (chg) {
		onFrameUpdate();
	}
#endif // !NO_IMGUI
}
void ShadowSystem::onInspectorGui() {
#ifndef NO_IMGUI
	Log_assert(engine_);

	bool changed = false;
	if (ImGui::ColorEdit4("Color", reinterpret_cast<float*>(&color_))) {
		changed = true;
	}
	if (ImGui_BlendCombo("Blend", &blend_)) {
		changed = true;
	}
	if (ImGui::DragInt("Vertices", &vertex_count_)) {
		vertex_count_ = Num_max(3, vertex_count_);
		changed = true;
	}
	if (ImGui::DragFloat("Max Height", &max_height_)) {
		changed = true;
	}
	if (ImGui::Checkbox("Scale by height", &scale_by_height_)) {
		changed = true;
	}
	if (ImGui::DragInt("World layer", &world_layer_)) {
		for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
			EID e = it->second.shadow_e;
			if (hs_->exists(e)) {
				rs_->setWorldLayer(e, world_layer_);
			}
		}
	}
	if (changed) {
		updateMesh();
	}
#endif // !NO_IMGUI
}
EID ShadowSystem::createShadowEntity(EID parent) {
	Log_assert(engine_);
	
	EID e = engine_->newId();
	//
	hs_->attach(e);
	hs_->setIndependent(e, true);
	hs_->setName(e, "__shadow__");
	hs_->setParent(e, parent);
	//
	rs_->attachMesh(e);
	KMesh *mesh = rs_->getMesh(e);
	{
		Color center_color = color_;
		Color outer_color = color_;
		if (GRADIENT_FILL) {
			outer_color.a = 0;
		}
		_MakeCircleMesh(mesh, 0.0f, 0.0f, 0.0f, SHADOW_BASIC_RADIUS, SHADOW_BASIC_RADIUS, vertex_count_, center_color, outer_color);
	}	
	rs_->setWorldLayer(e, this->world_layer_);
	rs_->getSubMeshMaterial(e, 0)->blend = blend_;
	rs_->setInheritSpecular(e, false); // スペキュラは継承しない
	return e;
}
void ShadowSystem::updateMesh() {
}


} // namespace
