#include "GameCollision.h"
//
#include "__Config.h" // MO_STRICT_CHECK
#include "GameCamera.h"
#include "GameGizmo.h"
#include "GameHierarchy.h"
#include "GameRender.h"
#include "GameWindowDebugMenu.h"
#include "Engine.h"
#include "inspector.h"
#include "KClock.h"
#include "KCollisionMath.h"
#include "KEasing.h"
#include "KImgui.h"
#include "KLog.h"
#include "KNum.h"
#include "KQuat.h"
#include "KStd.h"
#include "KToken.h"

#define HUGE_LENGTH (1024*8)
#define MID_SHOW_LINES 100
#define SLIDE_Z_THRESHOLD 0.1f

#define IGNORE_ROTATION_AND_SCALING 0
#define USE_XSCALE  1 // Ｘの符号によって反転させる

// 0=移動方向に関係なく、必ず壁との接触テストを行う。（壁にめり込んだ状態で静止している場合でも押し出すことができる）
// 1=壁に向かって移動している物体とだけ壁判定する（壁にめり込んだ状態で静止している場合は何もしない）
// 2=壁に向かって移動または停止している物体とだけ判定する（壁から離れる方向に移動しているときに限り、何もしない）
#define TERREIN_COLLISION_FILTER_BY_SPEED  0


// goblin の r7116 での修正ミスを解消するなら、古いコードを使うように COLLISION_COMPATIBLE を定義しておく。
// 未定義の場合は新しいコードをそのまま使う（地形BOXとの衝突判定が正しくなくなる）
#define COLLISION_COMPATIBLE


// 衝突すり抜け現象に対応する
#define AVOID_TUNNELING 0 // 1にしてもうまくいかなかった...

#define HITEDGE_XMIN  1
#define HITEDGE_ZMAX  2
#define HITEDGE_XMAX  4
#define HITEDGE_ZMIN  8


// 接地チェックする時、カプセル底面の中央だけでなく
// その周辺の点についても調べる？
#define DETECT_GROUND_USING_MULTIPLE_POINTS 0

// 底面中央の周辺についても調べる場合、どのくらい離れた点を調べるか。
// 0.0 で中央に一致、1.0で円周上に一致する（カプセル形状）
#define DETECT_GROUND_POSITION_K  0.2f

namespace mo {


static Vec3 _ExtractTranslation(const Matrix4 &m) {
	/*
		(* * * *)
		(* * * *)
		(* * * *) 
		(x y z 1) <-- この部分を得る
	*/
	return Vec3(
		m.get(3, 0),
		m.get(3, 1),
		m.get(3, 2)
	);
}


static ShapeType mo__StrToShape(const Path &s, ShapeType def) {
	if (s.compare("none"       )==0) return ShapeType_None;
	if (s.compare("box"        )==0) return ShapeType_Box;
	if (s.compare("ground"     )==0) return ShapeType_Ground;
	if (s.compare("plane"      )==0) return ShapeType_Plane;
	if (s.compare("capsule"    )==0) return ShapeType_Capsule;
	if (s.compare("sphere"     )==0) return ShapeType_Sphere;
	if (s.compare("sheared_box")==0) return ShapeType_ShearedBox;
	if (s.empty())      return def;
	Log_errorf("E_UNKNOWN_BODY_SHAPE: '%s'", s.u8());
	return def;
}





#pragma region BodyShape2
BodyShape2::BodyShape2() {
	clear();
}
const BodyShape2::SPHERE * BodyShape2::asSphere() const {
	return type==ShapeType_Sphere ? &u_sphere : NULL;
}
const BodyShape2::BOX * BodyShape2::asBox() const { 
	return type==ShapeType_Box ? &u_box : NULL;
}
const BodyShape2::PLANE * BodyShape2::asPlane() const { 
	return type==ShapeType_Plane ? &u_plane : NULL;
}
const BodyShape2::CAPSULE * BodyShape2::asCapsule() const {
	return type==ShapeType_Capsule ? &u_capsule : NULL;
}
const BodyShape2::SHEAREDBOX * BodyShape2::asShearedBox() const {
	return type==ShapeType_ShearedBox ? &u_sheared_box : NULL;
}

void BodyShape2::clear() {
	memset(this, 0, sizeof(*this));
}

void BodyShape2::setSphere(const SPHERE &s) {
	clear();
	type = ShapeType_Sphere;
	u_sphere = s;
}
void BodyShape2::setSphere(float r) {
	SPHERE s = {r};
	setSphere(s);
}

void BodyShape2::setBox(float half_x, float half_y, float half_z) {
	BOX s = {half_x, half_y, half_z};
	setBox(s);
}
void BodyShape2::setBox(const BOX &s) {
	clear();
	type = ShapeType_Box;
	u_box = s;
}

void BodyShape2::setPlane(float nor_x, float nor_y, float nor_z) {
	PLANE s = {nor_x, nor_y, nor_z};
	setPlane(s);
}
void BodyShape2::setPlane(const PLANE &s) {
	clear();
	type = ShapeType_Plane;
	u_plane = s;
}

void BodyShape2::setCapsule(float halfy, float r) {
	CAPSULE s = {halfy, r};
	setCapsule(s);
}
void BodyShape2::setCapsule(const CAPSULE &s) {
	clear();
	type = ShapeType_Capsule;
	u_capsule = s;
}

void BodyShape2::setShearedBox(float half_x, float half_y, float half_z, float shear_x) {
	SHEAREDBOX s = {half_x, half_y, half_z, shear_x};
	setShearedBox(s);
}
void BodyShape2::setShearedBox(const SHEAREDBOX &s) {
	clear();
	type = ShapeType_ShearedBox;
	u_sheared_box = s;
}
void BodyShape2::setGround() {
	clear();
	type = ShapeType_Ground;
}
#pragma endregion // BodyShape2









#pragma region HitBox
HitBox::HitBox() {
	enabled_ = false;
	half_size_ = Vec3(16, 16, 16);
	group_id_ = 0;
	user_param0 = 0;
	user_param1 = 0;
	user_param2 = 0;
	user_param3 = 0;
	callback = NULL;
	callback_data[0] = 0;
}

/// AABBの中心座標（エンティティからの相対座標）
const Vec3 & HitBox::center() const {
	return center_;
}
void HitBox::set_center(const Vec3 &p) {
	center_ = p;
}

/// AABBの広がり（AABB直方体の各辺の半分のサイズを指定。半径のようなもの）
const Vec3 & HitBox::half_size() const {
	return half_size_;
}
void HitBox::set_half_size(const Vec3 &s) {
	half_size_ = s;
}

/// 衝突判定のグループ分け。グループの組み合わせによって、衝突判定するかどうかが決まる。
/// 衝突判定を行うグループの組み合わせなどは CollisionSystem::setGroupInfo() で設定する
CollisionGroupId HitBox::group_id() const {
	return group_id_;
}
void HitBox::set_group_id(CollisionGroupId id) {
	group_id_ = id;
}

/// この判定を有効にするかどうか
bool HitBox::is_enabled() const {
	return enabled_;
}
void HitBox::set_enabled(bool b) {
	if (enabled_ != b) {
		enabled_ = b;
	}
}
#pragma endregion // HitBox










#pragma region CollisionSystem
static float mo__blink_gizmo_alpha(HierarchySystem *hs, EID e) {
#ifndef NO_IMGUI
	Log_assert(e);
	float alpha;
	if (hs->isEnabledInTree(e) && hs->isVisibleInTree(e)) {
		alpha = 1.0f;
	} else {
		alpha = 0.2f;
	}
	const float BLINK_CYCLE_SEC = 0.4f;
	Inspector *inspector = g_engine->getInspector();
	if (inspector && inspector->isEntitySelected(e)) {
		float t = KClock::getSystemTimeMsec() * 0.001f / BLINK_CYCLE_SEC;
		alpha *= KEasing::wave(t, 0.3f, 1.0f);
	}
	return alpha;
#else // !NO_IMGUI
	return 0.0f;
#endif
}



CollisionSystem::CollisionSystem() {
	frame_counter_ = -1;
	show_physics_gizmos_ = true;
	show_sensor_gizmos_ = true;
	highlight_collision_ = true;
	cb_ = NULL;
	engine_ = NULL;
	hs_ = NULL;
}
CollisionSystem::~CollisionSystem() {
	onEnd();
}
void CollisionSystem::init(Engine *engine) {
	Log_trace;
	engine->addSystem(this);
	engine->addScreenCallback(this); // IScreenCallback
	// まだ engine に HierarchySystem がアタッチされているとは限らないので、
	// ここでは HierarchySystem 取得しない
	engine_ = engine;
	no_slide_z_ = false;
}
void CollisionSystem::onStart() {
	Log_assert(engine_);
	hs_ = engine_->findSystem<HierarchySystem>();
	Log_assert(hs_);
	if (!MO_PUBLISHING_BUILD) {
		auto debugmenu = engine_->findSystem<DebugMenuSystem>();
		if (debugmenu) {
			debugmenu->addCallback(this); // IDebugMenuCallback
			debugmenu->addSparator();
			debugmenu->addUserMenuItem(L"衝突判定", MID_SHOW_LINES);
		}
	}
}
void CollisionSystem::onEnd() {
	hs_ = NULL;
	if (engine_) {
		auto debugmenu = engine_->findSystem<DebugMenuSystem>();
		if (debugmenu) {
			debugmenu->removeCallback(this); // IDebugMenuCallback
		}
		engine_->removeScreenCallback(this); // IScreenCallback
		engine_ = NULL;
	}
}
void CollisionSystem::onEntityInspectorGui(EID e) {
#ifndef NO_IMGUI
	_Node *node = _getNode(e);
	if (node == NULL) return;

	// 速度属性
	if (node->vel.is_attached) {
		ImGui::Separator();
		if (ImGui::TreeNode("Velocity")) {
			updateVelocityInspector(e);
			ImGui::TreePop();
		}
	}

	// 衝突属性
	if (node->col.is_attached) {
		ImGui::Separator();
		if (ImGui::TreeNode("Collider")) {
			updateCollisionInspector(e);
			ImGui::TreePop();
		}
	}

	// 物理属性
	if (node->body) {
		ImGui::Separator();
		if (ImGui::TreeNode("PhysicalBody")) {
			updateBodyInspector(e);
			ImGui::TreePop();
		}
	}
#endif // !NO_IMGUI
}
void CollisionSystem::updateVelocityInspector(EID e) {
#ifndef NO_IMGUI
	_Node *node = _getNode(e);
	if (node == NULL) return;

	_Vel *vel = &node->vel;
	if (!vel->is_attached) return;

	ImGui::Checkbox("Enabled", &vel->enabled);
	ImGui::DragFloat3("Speed", (float*)&vel->speed);
	ImGui::Text("Length: %g", vel->speed.getLength());
	ImGui::DragFloat("Speed Scale", &vel->speed_scale);

	Vec3 act_speed = getActualSpeed(e);
	Vec3 act_accel = getActualAccel(e);
	ImGui::DragFloat3("Prev Prev pos", (float*)&vel->prev_prev_pos);
	ImGui::DragFloat3("Prev pos", (float*)&vel->prev_pos);

	ImGui::Text("Actual speed: %g", act_speed.getLength());
	ImGui::Text("Actial accel: %g", act_accel.getLength());
	ImGui::DragFloat3("Actial speed", (float*)&act_speed);
	ImGui::DragFloat3("Actial accel", (float*)&act_accel);
#endif // !NO_IMGUI
}
void CollisionSystem::updateCollisionInspector(EID e) {
#ifndef NO_IMGUI
	_Node *node = _getNode(e);
	if (node == NULL) return;

	_Col *col = &node->col;
	if (!col->is_attached) return;

	ImGui::Checkbox("Enabled", &col->enabled);
	for (auto it=col->hitboxes.begin(); it!=col->hitboxes.end(); ++it) {
		updateHitboxInspector(node, &(*it));
	}
#endif // !NO_IMGUI
}
void CollisionSystem::updateBodyInspector(EID e) {
#ifndef NO_IMGUI
	_Node *node = _getNode(e);
	if (node == NULL) return;

	_Body *body = node->body;
	ImGui::Checkbox("Enabled", &body->enabled);
	ImGui::DragFloat3("Center", (float*)&body->center);
	ImGui::DragFloat("Skin width", &body->desc.skin_width_);
	ImGui::DragFloat("Snap height", &body->desc.snap_height_);
	ImGui::DragFloat("Penetration response", &body->desc.penetratin_response_);
	ImGui::DragFloat("Climb height", &body->desc.climb_height_);
	ImGui::DragFloat("Bounce (Vert)", &body->desc.bounce_);
	ImGui::DragFloat("Bounce (Horz)", &body->desc.bounce_horz_);
	ImGui::DragFloat("Bounce min speed", &body->desc.bounce_min_speed_);
	ImGui::Checkbox("Collide with dynamic body", &body->desc.dynamic_collision_enabled_);
	ImGui::DragFloat("Gravity", &body->desc.gravity_);
	if (body->state.has_altitude) {
		ImGui::Text("Altitude: %g", body->state.altitude);
	} else {
		ImGui::Text("Altitude: N/A");
	}
	int slp = Num_max(body->state.awake_time - engine_->getStatus(Engine::ST_FRAME_COUNT_GAME), 0);
	ImGui::Text("Sleep time left: %d", slp);
	ImGui::Text("Ground: %s", body->state.ground_entity ? hs_->getName(body->state.ground_entity).u8() : "(null)");
//	ImGui::Text("Touched : %s", touched_  ? "YES" : "NO");
	switch (body->shape.type) {
	case ShapeType_None:
		ImGui::Text("Shape: NONE");
		break;

	case ShapeType_Box:
		ImGui::Text("Shape: BOX");
		ImGui::DragFloat3("Half size", (float*)&body->shape.halfsize);
		break;
	
	case ShapeType_Ground:
		ImGui::Text("Shape: GROUND");
		break;
	
	case ShapeType_Plane:
		ImGui::Text("Shape: PLANE");
	//	ImGui_DragFloat3("Normal", &shape_normal_);
		ImGui::Text("Normal %.2f %.2f %.2f", body->shape.normal.x, body->shape.normal.y, body->shape.normal.z);
		if (ImGui::Button("Y-")) body->shape.normal = Quat(Vec3(0, 1, 0), -10).rot(body->shape.normal);
		ImGui::SameLine();
		if (ImGui::Button("Y+")) body->shape.normal = Quat(Vec3(0, 1, 0), 10).rot(body->shape.normal);
		break;
	
	case ShapeType_Capsule:
		ImGui::Text("Shape: CAPSULE");
		ImGui::DragFloat("Radius", &body->shape.radius);
		ImGui::DragFloat("Half height", &body->shape.halfsize.y);
		break;

	case ShapeType_Sphere:
		ImGui::Text("Shape: SPHERE");
		ImGui::DragFloat("Radius", &body->shape.radius);
		break;

	case ShapeType_ShearedBox:
		ImGui::Text("Shape: SHEARED_BOX");
		ImGui::DragFloat3("Half size", (float*)&body->shape.halfsize);
		ImGui::DragFloat("ShearX", &body->shape.shearx);
		break;
	
	default:
		ImGui::Text("Shape: ?");
		break;
	}
	for (auto it=body_hitpairs_.begin(); it!=body_hitpairs_.end(); ++it) {
		HitObject other;
		if (it->isPairOf(e, NULL, NULL, &other)) {
			ImGui::PushID(ImGui_ID(&it));
			ImGui::Text("Collide with: {}", hs_->getName(other.entity).u8());
			ImGui::PopID();
		}
	}
#endif // !NO_IMGUI
}

/// IDebugMenuCallback
void CollisionSystem::onDebugMenuSystemMenuItemClicked(IDebugMenuCallback::Params *p) {
#ifndef NO_IMGUI
	Log_assert(engine_);
	if (p->item_id == MID_SHOW_LINES) {
		Inspector *inspector = engine_->getInspector();
		if (inspector == NULL) return;

		SystemEditorParams *editorP = inspector->getSystemEditorParams(this);
		Log_assert(editorP);

		editorP->always_show_debug = !editorP->always_show_debug;
	}
#endif // !NO_IMGUI
}
/// IDebugMenuCallback
void CollisionSystem::onDebugMenuSystemMenuItemShow(IDebugMenuCallback::Params *p) {
#ifndef NO_IMGUI
	Log_assert(engine_);
	if (p->item_id == MID_SHOW_LINES) {
		Inspector *inspector = engine_->getInspector();
		if (inspector == NULL) return;

		SystemEditorParams *editorP = inspector->getSystemEditorParams(this);
		Log_assert(editorP);

		p->checked = editorP->always_show_debug;
	}
#endif // !NO_IMGUI
}
void CollisionSystem::setCallback(ICollisionCallback *cb) {
	cb_ = cb;
}

int CollisionSystem::getHitBoxCount(EID e) {
	_Node *node = _getNode(e);
	return (node && node->col.is_attached) ? node->col.hitboxes.size() : 0;
}
HitBox * CollisionSystem::getHitBoxByIndex(EID e, int index) {
	_Node *node = _getNode(e);
	return (node && node->col.is_attached) ? &node->col.hitboxes[index] : NULL;
}

HitBox * CollisionSystem::getHitBox(EID e, CollisionGroupId group_id) {
	if (MO_STRICT_CHECK) {
		if (getGroupInfo(group_id) == NULL) {
			Log_errorf("E_COLLISION_GROUP_NOT_EXISTS: No collision group '%d'. Use CollisionSystem::setGroupInfo()", group_id);
		}
	}
	_Node *node = _getNode(e);
	if (node == NULL) return NULL;
	if (!node->col.is_attached) return NULL;

	for (size_t i=0; i<node->col.hitboxes.size(); i++) {
		auto hbox = &node->col.hitboxes[i];
		if (hbox->group_id() == group_id) {
			return hbox;
		}
	}
	return NULL;
}
HitBox * CollisionSystem::addHitBox(EID e, CollisionGroupId group_id) {
	HitBox hitbox;
	hitbox.set_group_id(group_id);
	return addHitBox(e, hitbox);
}
HitBox * CollisionSystem::addHitBox(EID e, const HitBox &hitbox) {
	_Node *node = _getNode(e);
	if (node == NULL) return NULL;
	if (!node->col.is_attached) return NULL;
	node->col.hitboxes.push_back(hitbox);
	return &node->col.hitboxes.back();
}

void CollisionSystem::onFrameStart() {
	frame_counter_++;
}
void CollisionSystem::setColliderEnabled(EID e, bool b) {
	_Node *node = _getNode(e);
	if (node && node->col.is_attached) node->col.enabled = b;
}
void CollisionSystem::setBodyEnabled(EID e, bool b) {
	_Node *node = _getNode(e);
	if (node) node->body->enabled = b;
}
bool CollisionSystem::isBodyEnabled(EID e) const {
	const _Body *body = _getBody(e);
	return body ? body->enabled : false;
}

void CollisionSystem::attachVelocity(EID e) {
	_Node *pNode = _getNode(e);
	if (pNode == NULL) {
		_Node node;
		nodes_[e] = node;
		pNode = &nodes_[e];
	}
	pNode->entity = e;
	pNode->vel.is_attached = true;
	pNode->vel.enabled = true;
}
void CollisionSystem::attachCollider(EID e) {
	_Node *pNode = _getNode(e);
	if (pNode == NULL) {
		_Node node;
		nodes_[e] = node;
		pNode = &nodes_[e];
	}
	pNode->entity = e;
	pNode->col.is_attached = true;
	pNode->col.enabled = true;
}
void CollisionSystem::attachBody(EID e) {
	_Node *pNode = _getNode(e);
	if (pNode == NULL) {
		_Node node;
		nodes_[e] = node;
		pNode = &nodes_[e];
	}
	if (pNode->body == NULL) {
		pNode->entity = e;
		pNode->body = new _Body();
		pNode->body->desc.gravity_ = 0.4f;
		pNode->body->desc.skin_width_ = 4; // 左右、奥行き方向の接触誤差。相手がこの範囲内にいる場合は接触状態とみなす
		pNode->body->desc.snap_height_ = 4; // 地面の接触誤差。地面がこの範囲内にある場合は着地状態とみなす
		pNode->body->desc.penetratin_response_ = 0.5f;
		pNode->body->desc.climb_height_ = 16;
		pNode->body->desc.bounce_ = 0.5f;
		pNode->body->desc.bounce_horz_ = 0.8f;
		pNode->body->desc.bounce_min_speed_ = pNode->body->desc.gravity_ * 2; // 重力加速度よりも大きな値にしておかないと、静止しているときでもバウンドしてしまう
		pNode->body->desc.sliding_friction_ = 0.0f;
		pNode->body->desc.dynamic_collision_enabled_ = true;
		pNode->body->shape.type = ShapeType_Sphere;
		pNode->body->shape.radius = 32;
		pNode->body->shape.halfsize = Vec3(64, 64, 64);
		pNode->body->shape.normal = Vec3(0, 1, 0);
		pNode->body->state.altitude = 0;
		pNode->body->state.has_altitude = false; // 初期値では高度を無効値にしておく
		pNode->body->state.bounce_counting = 0;
		pNode->body->state.is_static = -1; // dirty
		pNode->body->state.awake_time = 0;
		pNode->body->state.ground_entity = NULL;
	}
}
void CollisionSystem::onDetach(EID e) {
	auto it = nodes_.find(e);
	if (it != nodes_.end()) {
		_remove(e);
		_Node &node = it->second;
		if (node.body) delete node.body;
		nodes_.erase(it);
	}
}
bool CollisionSystem::isVelocityAttached(EID e) const {
	const _Node *node = _getNode(e);
	return node ? node->vel.is_attached : false;
}
bool CollisionSystem::isColliderAttached(EID e) const {
	const _Node *node = _getNode(e);
	return node ? node->col.is_attached : false;
}
bool CollisionSystem::isBodyAttached(EID e) const {
	const _Node *node = _getNode(e);
	return node ? node->body != NULL : false;
}
CollisionSystem::_Body * CollisionSystem::_getBody(EID e) {
	_Node *node = _getNode(e);
	return node ? node->body : NULL;
}
const CollisionSystem::_Body * CollisionSystem::_getBody(EID e) const {
	const _Node *node = _getNode(e);
	return node ? node->body : NULL;
}
bool CollisionSystem::getBodyDesc(EID e, BodyDesc *desc) const {
	const _Body *body = _getBody(e);
	if (body) {
		*desc = body->desc;
		return true;
	}
	return false;
}
void CollisionSystem::setBodyDesc(EID e, const BodyDesc *desc) {
	_Body *body = _getBody(e);
	if (body) body->desc = *desc;
}
bool CollisionSystem::getBodyAltitude(EID e, float *alt) const {
	const _Body *body = _getBody(e);
	if (body && body->state.has_altitude) { // altitude に有効な値が入っているかどうか必ず確認する
		if (alt) *alt = body->state.altitude;
		return true;
	}
	return false;
}
Vec3 CollisionSystem::getBodyCenterPosInWorld(EID e) const {
	const _Body *body = _getBody(e);
	return body ? (body->center + hs_->getPosition(e)) : Vec3();
}
const Vec3 & CollisionSystem::getBodyCenter(EID e) const {
	static const Vec3 zero;
	const _Body *body = _getBody(e);
	return body ? body->center : zero;
}
void CollisionSystem::setBodyCenter(EID e, const Vec3 &value) {
	_Body *body = _getBody(e);
	if (body) body->center = value;
}
bool CollisionSystem::getBodyShape(EID e, BodyShape *shape) const {
	const _Body *body = _getBody(e);
	if (body) {
		if (shape) *shape = body->shape;
		return true;
	}
	return false;
}
void CollisionSystem::setBodyShape(EID e, const BodyShape *shape) {
	_Body *body = _getBody(e);
	if (body) body->shape = *shape;
}
void CollisionSystem::setBodyShapeNone(EID e) {
	_Body *body = _getBody(e);
	if (body) {
		body->shape.radius = 0;
		body->shape.shearx = 0;
		body->shape.halfsize = Vec3();
		body->shape.normal = Vec3();
		body->shape.type = ShapeType_None;
	}
}
void CollisionSystem::setBodyShapeGround(EID e) {
	_Body *body = _getBody(e);
	if (body) {
		body->shape.radius = 0;
		body->shape.shearx = 0;
		body->shape.halfsize = Vec3();
		body->shape.normal = Vec3();
		body->shape.type = ShapeType_Ground;
	}
}
void CollisionSystem::setBodyShapePlane(EID e, const Vec3 &normal) {
	if (normal.isZero()) {
		Log_error("E_NORMALIZE_ZERO_VECTOR: CollisionSystem::setBodyShapePlane");
		return;
	}
	_Body *body = _getBody(e);
	if (body) {
		body->shape.radius = 0;
		body->shape.shearx = 0;
		body->shape.halfsize = Vec3();
		body->shape.normal = normal.getNormalized();
		body->shape.type = ShapeType_Plane;
	}
}
bool CollisionSystem::getBodyShapePlane(EID e, Vec3 *normal) const {
	const _Body *body = _getBody(e);
	if (body && body->shape.type == ShapeType_Plane) {
		if (normal) *normal = body->shape.normal;
		return true;
	}
	return false;
}
void CollisionSystem::setBodyShapeBox(EID e, const Vec3 &halfsize) {
	_Body *body = _getBody(e);
	if (body) {
		body->shape.radius = 0;
		body->shape.shearx = 0;
		body->shape.halfsize = halfsize;
		body->shape.normal = Vec3();
		body->shape.type = ShapeType_Box;
	}
}
bool CollisionSystem::getBodyShapeBox(EID e, Vec3 *halfsize) const {
	const _Body *body = _getBody(e);
	if (body && body->shape.type == ShapeType_Box) {
		if (halfsize) *halfsize = body->shape.halfsize;
		return true;
	}
	return false;
}
void CollisionSystem::setBodyShapeShearedBox(EID e, const Vec3 &halfsize, float shearx) {
	_Body *body = _getBody(e);
	if (body) {
		body->shape.radius = 0;
		body->shape.shearx = shearx;
		body->shape.halfsize = halfsize;
		body->shape.normal = Vec3();
		body->shape.type = ShapeType_ShearedBox;
	}
}
bool CollisionSystem::getBodyShapeShearedBox(EID e, Vec3 *halfsize, float *shearx) const {
	const _Body *body = _getBody(e);
	if (body && body->shape.type == ShapeType_ShearedBox) {
		if (halfsize) *halfsize = body->shape.halfsize;
		if (shearx) *shearx = body->shape.shearx;
		return true;
	}
	return false;
}
void CollisionSystem::setBodyShapeCapsule(EID e, float radius, float halfheight) {
	_Body *body = _getBody(e);
	if (body) {
		body->shape.radius = radius;
		body->shape.shearx = 0;
		body->shape.halfsize = Vec3(0.0f, halfheight, 0.0f);
		body->shape.normal = Vec3();
		body->shape.type = ShapeType_Capsule;
	}
}
bool CollisionSystem::getBodyShapeCapsule(EID e, float *radius, float *halfheight) const {
	const _Body *body = _getBody(e);
	if (body && body->shape.type == ShapeType_Capsule) {
		if (radius) *radius = body->shape.radius;
		if (halfheight) *halfheight = body->shape.halfsize.y;
		return true;
	}
	return false;
}
void CollisionSystem::setBodyShapeSphere(EID e, float radius) {
	_Body *body = _getBody(e);
	if (body) {
		body->shape.radius = radius;
		body->shape.shearx = 0;
		body->shape.halfsize = Vec3();
		body->shape.normal = Vec3();
		body->shape.type = ShapeType_Sphere;
	}
}
bool CollisionSystem::getBodyShapeSphere(EID e, float *radius) const {
	const _Body *body = _getBody(e);
	if (body && body->shape.type == ShapeType_Sphere) {
		if (radius) *radius = body->shape.radius;
		return true;
	}
	return false;
}
void CollisionSystem::makeBodyXZWall(EID e, float x0, float z0, float x1, float z1) {
	_Body *body = _getBody(e);
	if (body) {
		float delta_x = x1 - x0;
		float delta_z = z1 - z0;

		// center position in world
		float center_x = (x0 + x1) * 0.5f;
		float center_z = (z0 + z1) * 0.5f;

		// normal vector
		float normal_x =  delta_z;
		float normal_z = -delta_x;
		Vec3 normal(normal_x, 0.0f, normal_z);
		if (normal.isZero()) {
			Log_error("E_NORMALIZE_ZERO_VECTOR: CollisionSystem::setBodyShapePlane");
			return;
		}

		float half_wall_width = hypotf(delta_x, delta_z) * 0.5f;

		if (1) {
			Vec3 pos = hs_->getPosition(e);
			pos.x = center_x;
			pos.z = center_z;
			hs_->setPosition(e, pos);
		}
		body->shape.radius = half_wall_width;
		body->shape.shearx = 0;
		body->shape.halfsize = Vec3();
		body->shape.normal = normal.getNormalized();
		body->shape.type = ShapeType_Plane;
	}
}
bool CollisionSystem::getBodyShapeLocalAabb(EID e, Vec3 *_min, Vec3 *_max) const {
	const _Body *body = _getBody(e);
	if (body) {
		const BodyShape &bs = body->shape;
		switch (body->shape.type) {
		case ShapeType_Sphere:
			if (_min) *_min = Vec3(-bs.radius,-bs.radius,-bs.radius);
			if (_max) *_max = Vec3( bs.radius, bs.radius, bs.radius);
			return true;
		case ShapeType_Capsule:
			if (_min) *_min = Vec3(-bs.radius,-bs.halfsize.y,-bs.radius);
			if (_max) *_max = Vec3( bs.radius, bs.halfsize.y, bs.radius);
			return true;
		case ShapeType_Box:
			if (_min) *_min = -bs.halfsize;
			if (_max) *_max =  bs.halfsize;
			return true;
		case ShapeType_ShearedBox:
			if (_min) *_min = -bs.halfsize - Vec3(bs.shearx, 0.0f, 0.0f);
			if (_max) *_max =  bs.halfsize + Vec3(bs.shearx, 0.0f, 0.0f);
			return true;
		}
	}
	return false;
}
void CollisionSystem::setBodyGravity(EID e, float grav) {
	BodyDesc desc;
	if (getBodyDesc(e, &desc)) {
		desc.gravity_ = grav;
		setBodyDesc(e, &desc);
	}
}
bool CollisionSystem::getBodyAabb(EID e, Vec3 *_min, Vec3 *_max) const {
	const _Body *body = _getBody(e);
	if (getBodyShapeLocalAabb(e, _min, _max)) {
		const _Body *body = _getBody(e);
		if (_min) *_min +=body->center;
		if (_max) *_max +=body->center;
		return true;
	}
	return false;
}
void CollisionSystem::setBodySleep(EID e, int duration) {
	_Body *body = _getBody(e);
	if (body) {
		body->state.awake_time = g_engine->getStatus(Engine::ST_FRAME_COUNT_GAME) + duration;
	}
}
bool CollisionSystem::isBodySleeping(EID e) const {
	const _Body *body = _getBody(e);
	if (body) {
		int time = g_engine->getStatus(Engine::ST_FRAME_COUNT_GAME);
		if (time < body->state.awake_time) {
			return true;
		}
	}
	return false;
}
bool CollisionSystem::isBodyStatic(EID e) {
	const _Node *node = _getNode(e);
	if (node && node->body) {
		return _isStatic(node);
	}
	return false;
}
bool CollisionSystem::_isStatic(const _Node *node) const {
	Log_assert(node && node->body);
	if (node->body->state.is_static < 0) {
		node->body->state.is_static = node->vel.is_attached ? 0 : 1;
	}
	return node->body->state.is_static != 0;
}

bool CollisionSystem::isBodyGrounded(EID e) const {
	const _Body *body = _getBody(e);
	if (body && body->state.ground_entity) {
		return true;
	}
	return false;
}
EID CollisionSystem::getBodyGroundEntity(EID e) const {
	const _Body *body = _getBody(e);
	return body ? body->state.ground_entity : NULL;
}
int CollisionSystem::getBodyBounceCount(EID e) const {
	const _Body *body = _getBody(e);
	return body ? body->state.bounce_counting : 0;
}
bool CollisionSystem::onAttach(EID e, const char *type) {
	if (strcmp(type, "Velocity") == 0) {
		attachVelocity(e);
		return true;
	}
	if (strcmp(type, "Collider") == 0) {
		attachCollider(e);
		return true;
	}
	if (strcmp(type, "PhysicalBody") == 0) {
		attachBody(e);
		return true;
	}
	return false;
}
void CollisionSystem::_remove(EID e) {
	// 削除されるエンティティが関係している内部情報をすべて削除する
	for (auto it=sensor_hitpairs_.begin(); it!=sensor_hitpairs_.end(); /*no expr*/) {
		if (it->object1.entity == e || it->object2.entity == e) {
			sensorCollisionExit(*it);
			it = sensor_hitpairs_.erase(it); // 衝突エンティティが消えた場合、EXIT イベントが発生しないことに注意
		} else {
			++it;
		}
	}
	for (auto it=body_hitpairs_.begin(); it!=body_hitpairs_.end(); /*no expr*/) {
		if (it->object1.entity == e || it->object2.entity == e) {
			physicalCollisionExit(*it);
			it = body_hitpairs_.erase(it); // 衝突エンティティが消えた場合、EXIT イベントが発生しないことに注意
		} else {
			++it;
		}
	}
	// 動的衝突リストから削除
	// ※このリストは判定処理時に一時変数として使うもので、判定処理の後はどこからも利用されない。
	// 　そのため判定処理後に無効になった Entity を保持していたとしても問題はないのだが、
	// 　デバッグ用の表示をONにした場合に限り、デバッグ描画に利用されることに注意。
	for (int i=(int)dynamic_bodies_.size()-1; i>=0; i--) {
		if (dynamic_bodies_[i].entity == e) {
			dynamic_bodies_.erase(dynamic_bodies_.begin() + i);
		}
	}
	// 静的衝突リストから削除
	for (int i=(int)static_bodies_.size()-1; i>=0; i--) {
		if (static_bodies_[i].entity == e) {
			static_bodies_.erase(static_bodies_.begin() + i);
		}
	}
	// 非衝突リストから削除
	for (int i=(int)moving_nodes_.size()-1; i>=0; i--) {
		if (moving_nodes_[i].entity == e) {
			moving_nodes_.erase(moving_nodes_.begin() + i);
		}
	}
}
void CollisionSystem::onFrameUpdate() {
	beginUpdate();
	if (1) {
		if (cb_) cb_->on_collision_update_start();
		// 衝突処理を行う必要があるエンティティだけをリストアップする
		updatePhysicsNodeList();

		// 速度コンポーネントにしたがって位置更新（衝突考慮しない）
		updatePhysicsPositions();

		// 物理判定同士の衝突処理
		updatePhysicsBodyCollision();

		// 地形判定と物理判定の衝突処理
		updateTerrainCollision();

		// センサー判定の衝突処理
		updateSensorNodeList();
		updateSensorCollision();

		if (cb_) cb_->on_collision_update_end();
	} else {
		// 座標だけ更新する。衝突判定しない
		updatePhysicsNodeList();
		updatePhysicsPositions();
	}
	endUpdate();
}
int CollisionSystem::findEntitiesInRange(EntityList &entities, const Vec3 &center, float radius) const {
	int num = 0;
	for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
		EID e = it->first;
		Vec3 p = hs_->getPosition(e);
		Vec3 delta = p - center;
		if (delta.getLength() <= radius) {
			entities.push_back(e);
			num++;
		}
	}
	return num;
}
void CollisionSystem::onInspectorGui() {
#ifndef NO_IMGUI
	Log_assert(engine_);
	ImGui::Text("Frames: %d", frame_counter_);
	ImGui::Checkbox("Physics gizmos", &show_physics_gizmos_);
	ImGui::Checkbox("Sensor gizmos", &show_sensor_gizmos_);

	ImGui::Text("Groups: %d", (int)groups_.size());
	ImGui::PushID(ImGui_ID(0));
	for (size_t i=0; i<groups_.size(); i++) {
		auto it = &groups_[i];
		if (ImGui::TreeNode(ImGui_ID(i), "[%d] %s", i, it->name_on_inspector.u8())) {
			ImGui::Text("Name: %s", it->name.u8());
			ImGui::Checkbox("Gizmo visible", &it->gizmo_visible);
			ImGui::ColorEdit4("Gizmo color", reinterpret_cast<float*>(&it->gizmo_color));
			ImGui::TreePop();
		}
	}
	ImGui::PopID();

	ImGui::Text("HitPairs(Body): %d", (int)body_hitpairs_.size());
	ImGui::PushID(ImGui_ID(1));
	for (size_t i=0; i<body_hitpairs_.size(); i++) {
		auto it = &body_hitpairs_[i];
		ImGui::SetNextTreeNodeOpen(true, ImGuiSetCond_Appearing);
		if (ImGui::TreeNode(ImGui_ID(i), "[%d]", i)) {
			ImGui::Text("Entity1   : %s", hs_->getName(it->object1.entity).u8());
			ImGui::Text("Entity2   : %s", hs_->getName(it->object2.entity).u8());
			ImGui::Text("State     : %s", (it->timestamp_enter==it->timestamp_lastupdate) ? "Enter" : (it->timestamp_exit==it->timestamp_lastupdate) ? "Exit" : "Stay");
			ImGui::Text("Enter time: %d(F)", it->timestamp_enter);
			ImGui::Text("Exit  time: %d(F)", it->timestamp_exit);
			ImGui::Text("Stay  time: %d(F)", it->timestamp_lastupdate - it->timestamp_enter);
			ImGui::TreePop();
		}
	}

	ImGui::Text("HitPairs(Sensor): %d", (int)sensor_hitpairs_.size());
	for (size_t i=0; i<sensor_hitpairs_.size(); i++) {
		auto it = &sensor_hitpairs_[i];
		ImGui::SetNextTreeNodeOpen(true, ImGuiSetCond_Appearing);
		if (ImGui::TreeNode(ImGui_ID(i), "[%d]", i)) {
			ImGui::Text("Entity1   : %s", hs_->getName(it->object1.entity).u8());
			ImGui::Text("Entity2   : %s", hs_->getName(it->object2.entity).u8());
			ImGui::Text("State     : %s", (it->timestamp_enter==it->timestamp_lastupdate) ? "Enter" : (it->timestamp_exit==it->timestamp_lastupdate) ? "Exit" : "Stay");
			ImGui::Text("Enter time: %d(F)", it->timestamp_enter);
			ImGui::Text("Exit  time: %d(F)", it->timestamp_exit);
			ImGui::Text("Stay  time: %d(F)", it->timestamp_lastupdate - it->timestamp_enter);
			ImGui::TreePop();
		}
	}
	ImGui::PopID();
#endif // !NO_IMGUI
}
// Body判定AABBの中心座標（ワールド）
Vec3 CollisionSystem::_cnGetAabbCenterInWorld(const _Node &cNode) const {
	Log_assert(cNode.body);
	Vec3 wpos = hs_->getPosition(cNode.entity);
	Vec3 cpos = cNode.body->center;
	return wpos + cpos;
}
// 前フレームでの、Body判定AABBの中心座標（ワールド）
Vec3 CollisionSystem::_cnGetPrevAabbCenterInWorld(const _Node &cNode) const {
	Log_assert(cNode.body);
	Vec3 wpos = getPrevPosition(cNode.entity);
	Vec3 cpos = cNode.body->center;
	return wpos + cpos;
}
Vec3 CollisionSystem::_cnGetPos(EID e) {
	return hs_->getPosition(e);
}
void CollisionSystem::_cnSetPos(EID e, const Vec3 &pos) {
	hs_->setPosition(e, pos);
}
Vec3 CollisionSystem::_cnGetScale(EID e) {
	return hs_->getScale(e);
}
void CollisionSystem::_cnMove(EID e, float dx, float dy, float dz) {
	Vec3 pos = hs_->getPosition(e);
	pos += Vec3(dx, dy, dz);
	hs_->setPosition(e, pos);
}
Vec3 CollisionSystem::_cnGetSpeed(EID e) {
	return getRealSpeed(e);
}
void CollisionSystem::_cnSetSpeed(EID e, const Vec3 &speed) {
	setSpeed(e, speed);
}
void CollisionSystem::_cnSetPrevPos(EID e, const Vec3 &pos) {
	_setPrevPosition(e, pos);
}
Vec3 CollisionSystem::_cnGetPrevPos(EID e) {
	return getPrevPosition(e);
}
void CollisionSystem::updateHitboxInspector(_Node *node, HitBox *hitbox) {
#ifndef NO_IMGUI
	ImGui::Separator();
	ImGui::PushID(hitbox);
	Path typestr = getGroupInfo(hitbox->group_id())->name_on_inspector;
	ImGui::SetNextTreeNodeOpen(true, ImGuiSetCond_Once);
	if (ImGui::TreeNode(typestr.u8())) {
		bool b = hitbox->is_enabled();
		if (ImGui::Checkbox("Enabled", &b)) {
			hitbox->set_enabled(b);
		}
		Vec3 sz = hitbox->half_size();
		if (ImGui::DragFloat3("Half size", (float*)&sz)) {
			hitbox->set_half_size(sz);
		}
		Vec3 cn = hitbox->center();
		if (ImGui::DragFloat3("Center", (float*)&cn)) {
			hitbox->set_center(cn);
		}
		ImGui::Text("Hits with:");
		ImGui::Indent();
		for (size_t i=0; i<sensor_hitpairs_.size(); i++) {
			const CollisionSystem::HitPair *hitpair = &sensor_hitpairs_[i];
			if (hitpair->object1.hitbox == hitbox) {
				CollisionGroupId gid = hitpair->object2.hitbox->group_id();
				const Path &label = getGroupInfo(gid)->name_on_inspector;
				ImGui::BulletText("%s.%s", hs_->getName(hitpair->object2.entity).u8(), label.u8());
			}
			if (hitpair->object2.hitbox == hitbox) {
				CollisionGroupId gid = hitpair->object1.hitbox->group_id();
				const Path &label = getGroupInfo(gid)->name_on_inspector;
				ImGui::BulletText("%s.%s", hs_->getName(hitpair->object1.entity).u8(), label.u8());
			}
		}
		ImGui::Unindent();
		ImGui::TreePop();
	}
	ImGui::PopID();
#endif // !NO_IMGUI
}
bool CollisionSystem::isDebugVisible(EID e, const View &view) {
	if (e == NULL) return false;

	Log_assert(engine_);
	auto hi = engine_->findSystem<HierarchySystem>();
	if (! hi->isEnabledInTree(e)) return false;
	if (! hi->isVisibleInTree(e)) return false;

#ifndef NO_IMGUI
	{
		Inspector *ins = engine_->getInspector();
		if (ins) {
			if (! ins->isEntityVisible(e)) return false;
		}
	}
#endif

	CameraSystem *cs = engine_->findSystem<CameraSystem>();
	if (cs->isTargetOf(e, view)) {
		return true;
	}
	return false;
}
void CollisionSystem::onDrawDebugInfo(const DebugRenderParameters &p) {
#ifndef NO_IMGUI
	Inspector *inspector = engine_->getInspector();
	if (inspector == NULL) return;

	// インスペクター上でシステムペインが開いているか、デバッグ表示が ON になっていればデバッグ情報を描画する
	bool show_debug = false;
	SystemEditorParams *info = inspector->getSystemEditorParams(this);
	if (inspector->isVisible()) {
		show_debug = info->is_open;
	} else {
		show_debug = info->always_show_debug;
	}
	if (!show_debug) {
		return; // 描画しない
	}

	if (show_physics_gizmos_) {
		for (auto it=static_bodies_.begin(); it!=static_bodies_.end(); ++it) {
			auto &cNode = *it;
			if (isDebugVisible(cNode.entity, p.view)) {
				drawGizmo_PhyiscsStaticNode(p.gizmo, cNode, p.view);
			}
		}
		for (auto it=dynamic_bodies_.begin(); it!=dynamic_bodies_.end(); ++it) {
			auto &cNode = *it;
			if (isDebugVisible(cNode.entity, p.view)) {
				drawGizmo_PhyiscsDynamicNode(p.gizmo, cNode, p.view);
			}
		}
	}
	if (show_sensor_gizmos_) {
		for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
			if (isDebugVisible(it->first, p.view)) {
				drawGizmo_SensorBoxes(p.gizmo, it->first, it->second, p.view);
			}
		}
	}
#endif
}
bool CollisionSystem::setParameter(EID e, const char *key, const char *val) {
	_Node *node = _getNode(e);
	if (node == NULL) {
		return false;
	}
	if (strcmp(key, "type")==0) {
		ShapeType shape = mo__StrToShape(val, node->body->shape.type);
		switch (shape) {
		case ShapeType_None:
			setBodyShapeNone(e);
			return true;
		case ShapeType_Ground:
			setBodyShapeGround(e);
			return true;
		case ShapeType_Plane:
			setBodyShapePlane(e, Vec3(1, 0, 0));
			return true;
		case ShapeType_Box:
			setBodyShapeBox(e, Vec3());
			return true;
		case ShapeType_Capsule:
			setBodyShapeCapsule(e, 0, 0);
			return true;
		case ShapeType_Sphere:
			setBodyShapeSphere(e, 0);
			return true;
		case ShapeType_ShearedBox:
			setBodyShapeShearedBox(e, Vec3(), 0);
			return true;
		}
		Log_errorf("Invlaid body shape '%s'", val);
		return false;
	}
	if (node->col.is_attached) {
		KToken tok(key, ".");
		const char *boxname = tok.get(0);
		const char *subkey = strchr(key, '.');
		if (subkey) subkey++; // subkey は . を指しているので隣の文字に進める
		CollisionGroupId group_id = getGroupIdByName(boxname);
		if (group_id >= 0 && subkey) {
			// ヒットボックスの属性を設定する
			HitBox *hitbox = getHitBox(e, group_id);
			if (hitbox == NULL) {
				hitbox = addHitBox(e, HitBox());
				hitbox->set_group_id(group_id);
			}
			if (strcmp(subkey, "enabled" )==0) {
				hitbox->set_enabled(K_strtoi(val) != 0);
				return true;
			}
			if (strcmp(subkey, "halfsize.x")==0) {
				Vec3 vec = hitbox->half_size(); vec.x = K_strtof(val); hitbox->set_half_size(vec);
				return true;
			}
			if (strcmp(subkey, "halfsize.y")==0) {
				Vec3 vec = hitbox->half_size(); vec.y = K_strtof(val); hitbox->set_half_size(vec);
				return true;
			}
			if (strcmp(subkey, "halfsize.z")==0) {
				Vec3 vec = hitbox->half_size(); vec.z = K_strtof(val); hitbox->set_half_size(vec);
				return true;
			}
			if (strcmp(subkey, "center.x")==0) {
				Vec3 vec = hitbox->center(); vec.x = K_strtof(val); hitbox->set_center(vec);
				return true;
			}
			if (strcmp(subkey, "center.y")==0) {
				Vec3 vec = hitbox->center(); vec.y = K_strtof(val); hitbox->set_center(vec);
				return true;
			}
			if (strcmp(subkey, "center.z")==0) {
				Vec3 vec = hitbox->center(); vec.z = K_strtof(val); hitbox->set_center(vec);
				return true;
			}
		}
	}
	if (node->body) {
		ShapeType tp = node->body->shape.type;
		if (strcmp(key, "center.x")==0) {
			node->body->center.x = K_strtof(val);
			return true;
		}
		if (strcmp(key, "center.y")==0) {
			node->body->center.y = K_strtof(val);
			return true;
		}
		if (strcmp(key, "center.z")==0) {
			node->body->center.z = K_strtof(val);
			return true;
		}
		if (strcmp(key, "halfsize.x")==0) {
			Log_assert(tp == ShapeType_Box || tp == ShapeType_ShearedBox);
			node->body->shape.halfsize.x = K_strtof(val);
			return true;
		}
		if (strcmp(key, "halfsize.y")==0) {
			Log_assert(tp == ShapeType_Box || tp == ShapeType_ShearedBox);
			node->body->shape.halfsize.y = K_strtof(val);
			return true;
		}
		if (strcmp(key, "halfsize.z")==0) {
			Log_assert(tp == ShapeType_Box || tp == ShapeType_ShearedBox);
			node->body->shape.halfsize.z = K_strtof(val);
			return true;
		}
		if (strcmp(key, "normal.x")==0) {
			Log_assert(tp == ShapeType_Plane);
			node->body->shape.normal.x = K_strtof(val);
			return true;
		}
		if (strcmp(key, "normal.y")==0) {
			Log_assert(tp == ShapeType_Plane);
			node->body->shape.normal.y = K_strtof(val);
			return true;
		}
		if (strcmp(key, "normal.z")==0) {
			Log_assert(tp == ShapeType_Plane);
			node->body->shape.normal.z = K_strtof(val);
			return true;
		}
		if (strcmp(key, "shear.x")==0) {
			Log_assert(tp == ShapeType_ShearedBox);
			node->body->shape.shearx = K_strtof(val);
			return true;
		}
	}
	return false;
}
const CollisionSystem::_Node * CollisionSystem::_getNode(EID e) const {
	auto it = nodes_.find(e);
	if (it != nodes_.end()) {
		return &it->second;
	}
	return NULL;
}
CollisionSystem::_Node * CollisionSystem::_getNode(EID e) {
	auto it = nodes_.find(e);
	if (it != nodes_.end()) {
		return &it->second;
	}
	return NULL;
}
void CollisionSystem::setSpeed(EID e, const Vec3 &spd) {
	_Node *node = _getNode(e);
	if (node && !node->is_static()) {
		if (node->vel.speed != spd) {
			node->vel.speed = spd;
		}
	}
}
const Vec3 & CollisionSystem::getSpeed(EID e) const {
	static const Vec3 s_def;

	const _Node *node = _getNode(e);
	return node ? node->vel.speed: s_def;
}
Vec3 CollisionSystem::getRealSpeed(EID e) {
	_Node *node = _getNode(e);
	if (node) {
		return node->vel.speed * node->vel.speed_scale;
	}
	return Vec3();
}
void CollisionSystem::setSpeedFactor(EID e, float value) {
	_Node *node = _getNode(e);
	if (node && !node->is_static()) {
		node->vel.speed_scale = value;
	}
}
float CollisionSystem::getSpeedFactor(EID e) const {
	const _Node *node = _getNode(e);
	return node ? node->vel.speed_scale : 1.0f;
}
Vec3 CollisionSystem::getPrevPosition(EID e) const {
	const _Node *node = _getNode(e);
	return node ? node->vel.prev_pos : Vec3();
}
Vec3 CollisionSystem::getPrevPrevPosition(EID e) const {
	const _Node *node = _getNode(e);
	return node ? node->vel.prev_prev_pos : Vec3();
}
Vec3 CollisionSystem::getActualAccel(EID e) const {
	const _Node *node = _getNode(e);
	if (node) {
		auto ts = engine_->findSystem<HierarchySystem>();
		Vec3 curr_pos = ts->getPosition(e);
		Vec3 last_spd = node->vel.prev_pos - node->vel.prev_prev_pos;
		Vec3 curr_spd = curr_pos  - node->vel.prev_pos;
		return curr_spd - last_spd;
	}
	return Vec3();
}
Vec3 CollisionSystem::getActualSpeed(EID e) {
	_Node *node = _getNode(e);
	if (node) {
		auto ts = engine_->findSystem<HierarchySystem>();
		Vec3 pos = ts->getPosition(e);
		return pos - node->vel.prev_pos;
	}
	return Vec3();
}
void CollisionSystem::_setPrevPosition(EID e, const Vec3 &pos) {
	_Node *node = _getNode(e);
	if (node) {
		node->vel.prev_prev_pos = node->vel.prev_pos;
		node->vel.prev_pos = pos;
	}
}






void CollisionSystem::setHitBoxEnabled(EID e, CollisionGroupId id, bool value) {
	_Node *node = _getNode(e);
	if (node == NULL) return;

	HitBox *hb = getHitBox(e, id);
	if (hb) hb->set_enabled(value);
}
const CollisionSystem::HitPair * CollisionSystem::isSensorHit(EID e1, EID e2) const {
	Log_assert(e1 != e2);
	for (size_t i=0; i<sensor_hitpairs_.size(); i++) {
		const HitPair *pair = &sensor_hitpairs_[i];
		if (pair->isPairOf(e1, e2, NULL, NULL)) {
			return pair;
		}
	}
	return NULL;
}
const CollisionSystem::HitPair * CollisionSystem::isBodyHit(EID e1, EID e2) const {
	Log_assert(e1 != e2);
	for (size_t i=0; i<body_hitpairs_.size(); i++) {
		const HitPair *pair = &body_hitpairs_[i];
		if (pair->isPairOf(e1, e2, NULL, NULL)) {
			return pair;
		}
	}
	return NULL;
}

const CollisionGroup * CollisionSystem::getGroupInfo(CollisionGroupId group_id) const {
	if (0 <= group_id && group_id < (int)groups_.size()) {
		const CollisionGroup *info = &groups_[group_id];
		// info の name が "" だった場合、配列の穴を埋めるためのダミー項目である。
		// その場合、グループが存在しないのと同じ扱いにする
		if (info && !info->name.empty()) {
			return info;
		}
	}
	return NULL;
}
const CollisionGroupId CollisionSystem::getGroupIdByName(const Path &name) const {
	for (int i=0; i<(int)groups_.size(); i++) {
		if (name.compare(groups_[i].name) == 0) {
			return i;
		}
	}
	return -1;
}
void CollisionSystem::setGroupInfo(CollisionGroupId group_id, const CollisionGroup &info) {
	Log_assert(! info.name.empty());
	if (group_id >= (int)groups_.size()) {
		groups_.resize(group_id + 1);
	}
	groups_[group_id] = info;
}
int CollisionSystem::getSensorHitPairCount() {
	return (int)sensor_hitpairs_.size();
}
const CollisionSystem::HitPair * CollisionSystem::getSensorHitPair(int index) const {
	return &sensor_hitpairs_[index];
}
int CollisionSystem::getSensorHitPairIndex(const HitBox *hitbox1, const HitBox *hitbox2) const {
	for (size_t i=0; i<sensor_hitpairs_.size(); i++) {
		const HitPair &pair = sensor_hitpairs_[i];
		if (pair.object1.hitbox == hitbox1 && pair.object2.hitbox == hitbox2) {
			return (int)i;
		}
		if (pair.object1.hitbox == hitbox2 && pair.object2.hitbox == hitbox1) {
			return (int)i;
		}
	}
	return -1;
}
int CollisionSystem::getBodyHitPairIndex(EID e1, EID e2) const {
	for (size_t i=0; i<body_hitpairs_.size(); i++) {
		const HitPair &pair = body_hitpairs_[i];
		if (pair.object1.entity == e1 && pair.object2.entity == e2) {
			return (int)i;
		}
		if (pair.object1.entity == e2 && pair.object2.entity == e1) {
			return (int)i;
		}
	}
	return -1;
}






void CollisionSystem::drawGizmo_AabbVerticalDashLines(Gizmo *gizmo, const Matrix4 &matrix, const Vec3 &min_point_in_world, const Vec3 &max_point_in_world, const Color &color) {
	// AABBの底面座標
	Vec3 p0(min_point_in_world.x, min_point_in_world.y, max_point_in_world.z);
	Vec3 p1(max_point_in_world.x, min_point_in_world.y, max_point_in_world.z);
	Vec3 p2(min_point_in_world.x, min_point_in_world.y, min_point_in_world.z);
	Vec3 p3(max_point_in_world.x, min_point_in_world.y, min_point_in_world.z);
	Vec3 p0_ground(p0.x, p0.y - getAltitudeAtPoint(p0), p0.z);
	Vec3 p1_ground(p1.x, p1.y - getAltitudeAtPoint(p1), p1.z);
	Vec3 p2_ground(p2.x, p2.y - getAltitudeAtPoint(p2), p2.z);
	Vec3 p3_ground(p3.x, p3.y - getAltitudeAtPoint(p3), p3.z);
//	gizmo->addLineDash(matrix, p0, p0_ground, color);
//	gizmo->addLineDash(matrix, p1, p1_ground, color);
//	gizmo->addLineDash(matrix, p2, p2_ground, color);
	gizmo->addLineDash(matrix, p3, p3_ground, color);
}
void CollisionSystem::drawGizmo_PhyiscsStaticNode(Gizmo *gizmo, CollisionSystem::_Node &cNode, const View &view) {
	Log_assert(engine_);
	const float line_alpha = mo__blink_gizmo_alpha(hs_, cNode.entity);
	Color color(0.0f, 1.0f, 1.0f, line_alpha);
#ifndef NO_IMGUI
	if (highlight_collision_) {
		// インスペクターで選択中のエンティティと接触しているもの色を変える
		EID sel_e = engine_->getInspector() ? engine_->getInspector()->getSelectedEntity(0) : NULL;
		if (sel_e && sel_e != cNode.entity) {
			if (isBodyHit(sel_e, cNode.entity)) {
				color = Color(1, 1, 1, line_alpha);
			}
		}
	}
#endif // !NO_IMGUI
	Matrix4 mat;
	hs_->getLocalToWorldMatrix(cNode.entity, &mat);
	mat = mat * view.transform_matrix;

	Matrix4 matrix;
	if (IGNORE_ROTATION_AND_SCALING) {
		// 衝突判定は回転もスケーリングも無視する
		// このままでは mat は回転拡大縮小成分を含んでいるため、平行移動のみの行列を得る
		Vec3 off = _ExtractTranslation(mat);
		matrix = Matrix4::fromTranslation(off);

		if (USE_XSCALE) {
			// ただし、左右反転に関してだけは符号を反映させる
			if (_cnGetScale(cNode.entity).x < 0) {
				matrix = matrix * Matrix4::fromScale(-1, 1, 1);
			}
		}
	} else {
		matrix = mat;
	}

	if (cNode.body->shape.type == ShapeType_Ground) {
		// 地面を上向き矢印（法線）で表す。矢印先端の点が地面中心に一致する
		const Vec3 p = cNode.body->center;
		const Vec3 from(p.x, p.y - 32, p.z);
		const Vec3 to  (p.x, p.y, p.z);
		gizmo->addArrow(matrix, from, to, 8, color);
	}
	if (cNode.body->shape.type == ShapeType_Plane) {
		// 平面を法線矢印で表す。矢印起点ではなく、矢印先端の点が平面中心に一致する
		const Vec3 n = cNode.body->shape.normal;
		const Vec3 p = cNode.body->center;
		const Vec3 from = p - n;
		const Vec3 to   = p;
		gizmo->addArrow(matrix, from, to, 8, color);

		if (cNode.body->shape.normal.y == 0) {
			// XZ平面に垂直な平面の場合は、Ｙ＝０な場所にラインを引く
			// 範囲付き平面の場合は、平面幅が定義されていることに注意
			float r = cNode.body->shape.radius;
			float len = (r > 0) ? r : HUGE_LENGTH;
			Vec3 vec(-n.z, 0.0f, n.x);
			Vec3 p0 = p + vec * len;
			Vec3 p1 = p - vec * len;
			gizmo->addLine(matrix, p0, p1);
		}
	}
	if (cNode.body->shape.type == ShapeType_Box) {
		Vec3 box_halfsize;
		getBodyShapeBox(cNode.entity, &box_halfsize);
		gizmo->addAabb(matrix,
			cNode.body->center - box_halfsize,
			cNode.body->center + box_halfsize,
			color);
	}
	if (cNode.body->shape.type == ShapeType_ShearedBox) {
		Vec3 halfsize;
		float xShear;
		getBodyShapeShearedBox(cNode.entity, &halfsize, &xShear);
		Vec3 c = cNode.body->center;
		// 上側の平行四辺形
		Vec3 a0(-halfsize.x+xShear, halfsize.y, halfsize.z), a1(halfsize.x+xShear, halfsize.y, halfsize.z);
		Vec3 a3(-halfsize.x-xShear, halfsize.y,-halfsize.z), a2(halfsize.x-xShear, halfsize.y,-halfsize.z);
		gizmo->addLine(matrix, c+a0, c+a1);
		gizmo->addLine(matrix, c+a1, c+a2);
		gizmo->addLine(matrix, c+a2, c+a3);
		gizmo->addLine(matrix, c+a3, c+a0);

		// 下側の平行四辺形
		Vec3 b0(-halfsize.x+xShear, -halfsize.y, halfsize.z), b1(halfsize.x+xShear, -halfsize.y, halfsize.z);
		Vec3 b3(-halfsize.x-xShear, -halfsize.y,-halfsize.z), b2(halfsize.x-xShear, -halfsize.y,-halfsize.z);
		gizmo->addLineDash(matrix, c+b0, c+b1);
		gizmo->addLine(matrix, c+b1, c+b2);
		gizmo->addLine(matrix, c+b2, c+b3);
		gizmo->addLineDash(matrix, c+b3, c+b0);

		// 上と下を結ぶ垂直線
		gizmo->addLineDash(matrix, c+a0, c+b0);
		gizmo->addLine(matrix, c+a1, c+b1);
		gizmo->addLine(matrix, c+a2, c+b2);
		gizmo->addLine(matrix, c+a3, c+b3);
	}
}
void CollisionSystem::drawGizmo_PhyiscsDynamicNode(Gizmo *gizmo, const CollisionSystem::_Node &cNode, const View &view) {
	Log_assert(engine_);
	Matrix4 mat;
	hs_->getLocalToWorldMatrix(cNode.entity, &mat);
	mat = mat * view.transform_matrix;

	Matrix4 matrix;
	if (IGNORE_ROTATION_AND_SCALING) {
		// 衝突判定は回転もスケーリングも無視する
		// このままでは mat は回転拡大縮小成分を含んでいるため、平行移動のみの行列を得る
		Vec3 off = _ExtractTranslation(mat);
		matrix = Matrix4::fromTranslation(off);
	} else {
		matrix = mat;
	}

	const float line_alpha = mo__blink_gizmo_alpha(hs_, cNode.entity);
	Color color1(0.0f, 0.0f, 1.0f, line_alpha);
	Color color2(0.4f, 0.4f, 0.4f, line_alpha);

#ifndef NO_IMGUI
	if (highlight_collision_) {
		// インスペクターで選択中のエンティティと接触しているもの色を変える
		EID sel_e = engine_->getInspector() ? engine_->getInspector()->getSelectedEntity(0) : NULL;
		if (sel_e && sel_e != cNode.entity) {
			if (isBodyHit(sel_e, cNode.entity)) {
				color1 = Color(1, 1, 1, line_alpha);
			}
		}
	}
#endif

	// 高度（判定底面から地面までの符号付距離）
	// 有効な値が入っているかどうか、必ず cNode.body->state.has_altitude を確認すること
	float alt = cNode.body->state.altitude;

	Vec3 center = cNode.body->center; // 判定中心
		
	// AABB 判定の描画
	if (cNode.body->shape.type == ShapeType_Box) {
		// サイズを取得
		Vec3 halfsize;
		getBodyShapeBox(cNode.entity, &halfsize);

		// AABB 描画
		Vec3 minpoint = center - halfsize;
		Vec3 maxpoint = center + halfsize;
		gizmo->addAabb(matrix, minpoint, maxpoint, color2);

		// 高度を示す矢印の描画
		if (cNode.body->state.has_altitude) {
			Vec3 p0 = center; // 矢印の始点は判定の中心に
			Vec3 p1 = p0 + Vec3(0.0f, -halfsize.y - alt, 0.0f); // 矢印の終点はオブジェクト真下の地面に
			gizmo->addArrow(matrix, p0, p1, 6, color1);
		}
	}

	// カプセル判定の描画
	if (cNode.body->shape.type == ShapeType_Capsule) {
		// サイズを取得
		float radius, halfheight;
		getBodyShapeCapsule(cNode.entity, &radius, &halfheight);

		// カプセル外接円柱を描画
		Vec3 top(center.x, center.y+halfheight, center.z);
		Vec3 btm(center.x, center.y-halfheight, center.z);
		gizmo->addCircle(matrix, top, radius, 1/*Y-Axis*/, color1);
		gizmo->addCircle(matrix, btm, radius, 1/*Y-Axis*/, color1);
		gizmo->addLine(matrix, top+Vec3(-radius, 0.0f, 0.0f), btm+Vec3(-radius, 0.0f, 0.0f), color1);
		gizmo->addLine(matrix, top+Vec3( radius, 0.0f, 0.0f), btm+Vec3( radius, 0.0f, 0.0f), color1);

		// 高度を示す矢印の描画
		if (cNode.body->state.has_altitude) {
			Vec3 p0 = center; // 矢印の始点は判定の中心に
			Vec3 p1 = p0 + Vec3(0.0f, -halfheight - radius - alt, 0.0f); // 矢印の終点はオブジェクト真下の地面に
			gizmo->addArrow(matrix, p0, p1, 6, color1);
		}
	}

	// 球判定の描画
	if (cNode.body->shape.type == ShapeType_Sphere) {
		// サイズを取得
		float radius;
		getBodyShapeSphere(cNode.entity, &radius);

		Vec3 btm(center.x, center.y-radius, center.z);
		gizmo->addCircle(matrix, center, radius, 1/*Y-Axis*/, color1);

		// 高度を示す矢印の描画
		if (cNode.body->state.has_altitude) {
			Vec3 p0 = center; // 矢印の始点は判定の中心に
			Vec3 p1 = p0 + Vec3(0.0f, -radius - alt, 0.0f); // 矢印の終点はオブジェクト真下の地面に
			gizmo->addArrow(matrix, p0, p1, 6, color1);
		}
	}
}
bool CollisionSystem::_isHit(const HitBox *hb, const EID e) const {
	for (size_t i=0; i<sensor_hitpairs_.size(); i++) {
		auto pair = &sensor_hitpairs_[i];
		if (pair->object1.hitbox == hb && pair->object2.entity == e) return true;
		if (pair->object2.hitbox == hb && pair->object1.entity == e) return true;
	}
	return false;
}
void CollisionSystem::drawGizmo_SensorBoxes(Gizmo *gizmo, EID entity, const _Node &eNode, const View &view) {
	Log_assert(engine_);
	bool col_actived = eNode.col.is_attached && eNode.col.enabled;
	if (!col_actived) return;
	
	Matrix4 mat;
	hs_->getLocalToWorldMatrix(entity, &mat);
	mat = mat * view.transform_matrix;

	Matrix4 matrix;
	if (IGNORE_ROTATION_AND_SCALING) {
		// 衝突判定は回転もスケーリングも無視する
		// このままでは mat は回転拡大縮小成分を含んでいるため、平行移動のみの行列を得る
		Vec3 off = _ExtractTranslation(mat);
		matrix = Matrix4::fromTranslation(off);
	} else {
		matrix = mat;
	}
#ifndef NO_IMGUI
	EID sel_e = engine_->getInspector() ? engine_->getInspector()->getSelectedEntity(0) : NULL;
#else
	EID sel_e = NULL;
#endif
	const float line_alpha = mo__blink_gizmo_alpha(hs_, entity);
	for (size_t i=0; i<eNode.col.hitboxes.size(); i++) {
		const HitBox *hitbox = &eNode.col.hitboxes[i];
		if (! hitbox->is_enabled()) continue;
		auto groupinfo = getGroupInfo(hitbox->group_id());
		if (! groupinfo->gizmo_visible) continue;
		Color color = groupinfo->gizmo_color;
		color.a = line_alpha;
		// インスペクターで選択中のエンティティと接触しているもの色を変える
		if (highlight_collision_ && sel_e && sel_e != entity) {
			if (_isHit(hitbox, sel_e)) {
				color = Color(1, 1, 1, line_alpha);
			}
		}
		// エンティティのローカル座標系における AABB の min, max 座標
		Vec3 minp_l = hitbox->center() - hitbox->half_size();
		Vec3 maxp_l = hitbox->center() + hitbox->half_size();
		gizmo->addAabb(matrix, minp_l, maxp_l, color);

		if (1) {
			// 地面からの高さが分かるよう、点線を地面まで伸ばす
			// ワールド座標系における AABB の min, max 座標
			Vec3 minp_w = matrix.transform(minp_l);
			Vec3 maxp_w = matrix.transform(maxp_l);
			drawGizmo_AabbVerticalDashLines(gizmo, matrix, minp_w, maxp_w, color); // 地面への投影
		}
	}
}
Vec3 CollisionSystem::getHitBoxCenterPosInWorld(EID e, const HitBox *hb) const {
	Vec3 basepos = hs_->getPosition(e);
	Vec3 center = hb->center();
#if USE_XSCALE
	center.x *= hs_->getScale(e).x;
#endif
	return basepos + center;
}
Vec3 CollisionSystem::getHitBoxCenterPosInWorld(const _HitBoxNode &cNode) const {
	Vec3 basepos = hs_->getPosition(cNode.entity);
	Vec3 center = cNode.hitbox->center();
#if USE_XSCALE
	center.x *= hs_->getScale(cNode.entity).x;
#endif
	return basepos + center;
}

/// 物理衝突処理を行う必要があるエンティティだけをリストアップする
void CollisionSystem::updatePhysicsNodeList() {
	Log_assert(engine_);
	Log_assert(hs_);
	int game_time = engine_->getStatus(Engine::ST_FRAME_COUNT_GAME);

	moving_nodes_.clear();
	dynamic_bodies_.clear();
	static_bodies_.clear();
	for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
		EID e = it->first;
		_Node &eNode = it->second;

		// Entity がツリー内で disabled だったら判定しない
		// （paused 状態は考慮しない。オブジェクトの動きが停止している時でも、勝手に衝突判定を突き抜けられては困る）
		if (! hs_->isEnabledInTree(e)) continue;
		
		if (eNode.body && eNode.body->enabled && eNode.body->shape.type != ShapeType_None) {
			// 衝突が有効かつ衝突形状を持っている
			eNode.tmp_is_statc = _isStatic(&eNode) || hs_->isPaused(e) || !eNode.vel.is_attached;

			if (eNode.body->state.awake_time > 0) {
				if (game_time < eNode.body->state.awake_time) {
					eNode.tmp_is_statc = true; // まだスリープ解除時間に達していない
				} else {
					eNode.body->state.awake_time = 0; // スリープ解除
				}
			}
			if (eNode.tmp_is_statc) {
				// 移動できない
				static_bodies_.push_back(eNode);
			} else {
				// 移動できる
				dynamic_bodies_.push_back(eNode);
			}
		} else if (eNode.vel.is_attached) {
			// 衝突判定なし
			if (!hs_->isPaused(e)) {
				// 衝突処理なしで移動する。
				// この場合は paused を考慮する。pause中に勝手に移動されては困る
				if (!hs_->isPausedInTree(e)) {
					eNode.tmp_is_statc = false;
					moving_nodes_.push_back(eNode);
				}
			}
		}
	}
}
void CollisionSystem::simpleMove(_Node &cNode) {
	if (hs_->isPausedInTree(cNode.entity)) {
		return;
	}
	Vec3 pos = _cnGetPos(cNode.entity);
	Vec3 spd = _cnGetSpeed(cNode.entity);

	// 変更前の座標を記録しておく
//	cNode.oldpos = pos; <-- こうやって衝突処理の時に使おうと思ってもダメ。この cNode は moving_nodes_ の cNode かつ非ポインタなので、dynamic_bodies_ 側の cNode::oldpos の値は変化しない！
	_cnSetPrevPos(cNode.entity, pos);

	// 注意！！ ここでの newspd は、speed_scale の影響を受けた値である。
	// speed_scale 適用後の速度に対する操作の結果として得られた新速度を、
	// そのまま setSpeed してはいけない。
	// setSpeed でセットしないといけないのは、 speed_scale する前の設定速度値である
	float speed_scale = getSpeedFactor(cNode.entity);

	// 座標変更
	Vec3 newpos = pos + spd;
	Vec3 newspd = spd;
	if (cNode.body) {
		float grav = cNode.body->desc.gravity_;
		newspd.y -= grav * speed_scale; // 重力も SpeedScale の影響を受ける
	}
	_cnSetPos(cNode.entity, newpos);

	// newspd は、speed_scale の影響を受けた値であることに注意。
	// setSpeed でセットしないといけないのは、 speed_scale する前の設定速度値である
	if (speed_scale > 0.0f) {
		_cnSetSpeed(cNode.entity, newspd / speed_scale);
	}
}

/// updatePhysicsNodeList() で作成されたリストのうち、移動可能物体の位置を更新する
void CollisionSystem::updatePhysicsPositions() {
	Log_assert(engine_);
	for (auto it=moving_nodes_.begin(); it!=moving_nodes_.end(); ++it) {
		_Node &cNode = *it;
		simpleMove(cNode);
	}
	for (auto it=dynamic_bodies_.begin(); it!=dynamic_bodies_.end(); ++it) {
		_Node &cNode = *it;
		simpleMove(cNode);
	}
}
void CollisionSystem::physicalCollisionEnter(HitPair &pair) {
}
void CollisionSystem::physicalCollisionStay(HitPair &pair) {
}
void CollisionSystem::physicalCollisionExit(HitPair &pair) {
}

/// 動的剛体同士で相互作用するものを処理する
void CollisionSystem::updatePhysicsBodyCollision() {
	Log_assert(engine_);
	
	// 相互作用する可能性のある剛体リストを作成
	tmp_table_.clear();
	tmp_table_.reserve(dynamic_bodies_.size());
	for (auto it=dynamic_bodies_.begin(); it!=dynamic_bodies_.end(); ++it) {
		_Node &node = *it;
		if (! node.body->desc.dynamic_collision_enabled_) {
			// 移動物体同士の接触判定が off になっている
			continue;
		}
		// 最新の位置を取得しておく
		node.tmp_pos = hs_->getPosition(node.entity);
		// ※これは Hierarchy::getPositionInWorld で取得したいところだが、そうすると
		// 衝突後の新しい座標も Hierarchy::setPositionInWorld で設定しないといけなくなる。
		// しかし Hierarchy::setPositionInWorld メソッドはまだ用意していない。
		
		tmp_table_.push_back(node);
	}

	for (int i=0; i<(int)tmp_table_.size()-1; i++) {
		_Node &cNode1 = tmp_table_[i];
		for (int j=i+1; j<(int)tmp_table_.size(); j++) {
			_Node &cNode2 = tmp_table_[j];
			Log_assert(cNode1.entity != cNode2.entity);

			// 2体間の中心距離
			Vec3 delta = cNode2.tmp_pos - cNode1.tmp_pos;
			float delta_r = delta.getLength();

			// 2体がめり込んでいる場合、その深さ。
			// めり込んでいるなら正の値になる
			float penetration_depth = cNode1.body->shape.radius + cNode2.body->shape.radius - delta_r;
			
			// 2体の接触許容範囲のうち、大きい方の値
			float skin = Num_max(cNode1.body->desc.skin_width_, cNode2.body->desc.skin_width_);

			// めり込み深さの絶対値が skin 未満であれば「表面接触」である。
			// 「深さ < -skin」なら確実に離れている。
			// 「-skin <= 深さ < skin」なら接触している（2体間の隙間が skin 未満になっているということ）
			// 「skin <= 深さ」ならば確実にめり込んでいる。
			if (penetration_depth < -skin) {
				continue;
			}

			// 2体が衝突状態にある。
			// この衝突状態を解決するか、それとも無視するか問い合わせる
			bool deny = false;
			if (cb_) cb_->on_collision_update_body_each(cNode1.entity, cNode2.entity, &deny);
			if (deny) {
				continue;
			}

			// 衝突履歴を調べ、衝突開始か、衝突継続かを判断する
			int pair_index = getBodyHitPairIndex(cNode1.entity, cNode2.entity);
			if (pair_index >= 0) {
				// 同じ衝突ペアが既に存在している。
				// 今回よりも以前のフレームで衝突開始している
				HitPair &pair = body_hitpairs_[pair_index];
				pair.timestamp_lastupdate = frame_counter_;
				pair.object1.hitbox_pos = cNode1.tmp_pos;
				pair.object2.hitbox_pos = cNode2.tmp_pos;
				physicalCollisionStay(pair); // 衝突継続

			} else {
				// 同じ衝突ペアが見つからない。
				// このフレームで新しく衝突した
				HitPair pair;
				pair.object1.hitbox     = NULL;
				pair.object2.hitbox     = NULL;
				pair.object1.entity     = cNode1.entity;
				pair.object2.entity     = cNode2.entity;
				pair.object1.hitbox_pos = cNode1.tmp_pos;
				pair.object2.hitbox_pos = cNode2.tmp_pos;
				pair.timestamp_enter    = frame_counter_;
				pair.timestamp_exit     = -1;
				pair.timestamp_lastupdate = frame_counter_;
				body_hitpairs_.push_back(pair);
				physicalCollisionEnter(pair); // 衝突開始
			}
			if (penetration_depth <= 0) {
				// 表面が接触しているだけで、互いにめり込んではいない。
				// 座標修正する必要はない
				continue;
			}
			// 2体が互いに真正面から衝突した場合、互いに横にそれることができず、
			// お互いを避けることができない。少しだけ座標をずらしておく
			if (delta.x == 0) delta.x = 0.001f;
			if (delta.z == 0) delta.z = 0.001f;

			// めり込みを解決する
			{
				float response1 = cNode1.body->desc.penetratin_response_;
				float response2 = cNode2.body->desc.penetratin_response_;
				if (cb_) cb_->on_collision_response(cNode1.entity, cNode2.entity, &response1, &response2);

				const float cos_xz = (delta_r > 0) ? delta.x / delta_r : 1;
				const float sin_xz = (delta_r > 0) ? delta.z / delta_r : 0;
				const float penet_x = penetration_depth * 0.5f * cos_xz;
				const float penet_z = penetration_depth * 0.5f * sin_xz;

				cNode1.tmp_pos.x -= penet_x * response1;
				cNode1.tmp_pos.z -= penet_z * response1;
				hs_->setPosition(cNode1.entity, cNode1.tmp_pos);

				cNode2.tmp_pos.x += penet_x * response2;
				cNode2.tmp_pos.z += penet_z * response2;
				hs_->setPosition(cNode2.entity, cNode2.tmp_pos);
			}
		}
	}
}

/// BOXの側面との壁判定
void CollisionSystem::updateTerrainCollision_BoxWall() {
	if (tmp_static_boxes_.empty()) return;
	for (size_t i=0; i<dynamic_bodies_.size(); i++) {
		_Node &dyNode = dynamic_bodies_[i];
		for (size_t j=0; j<tmp_static_boxes_.size(); j++) {
			const _Node &stNode = tmp_static_boxes_[j];
			Log_assert(stNode.entity != dyNode.entity);
			Log_assert(stNode.body->shape.type == ShapeType_Box); // すでにフィルタリングされているはず

			// 判定するかどうかを問い合わせる
			{
				bool deny = false;
				if (cb_) cb_->on_collision_update_body_and_static(dyNode.entity, stNode.entity, &deny);
				if (deny) continue;
			}

			// 段差チェック
			Vec3 delta;
			if (collideStaticBoxAndBody(stNode, dyNode, &delta)) {
				Vec3 pos = _cnGetPos(dyNode.entity);
				pos += delta;
				_cnSetPos(dyNode.entity, pos);
				updatePhysicsCollisionPair(dyNode, stNode);
			}
		}
	}
}

/// ShearedBoxの側面との壁判定
void CollisionSystem::updateTerrainCollision_ShearedBoxWall() {
	if (tmp_static_shearedboxes_.empty()) return;
	for (size_t i=0; i<dynamic_bodies_.size(); i++) {
		_Node &dyNode = dynamic_bodies_[i];
		Vec3 dySpeed = _cnGetSpeed(dyNode.entity);
		for (size_t j=0; j<tmp_static_shearedboxes_.size(); j++) {
			const _Node &stNode = tmp_static_shearedboxes_[j];
			Log_assert(stNode.entity != dyNode.entity);
			Log_assert(stNode.body->shape.type == ShapeType_ShearedBox); // すでにフィルタリングされているはず

			// 判定するかどうかを問い合わせる
			{
				bool deny = false;
				if (cb_) cb_->on_collision_update_body_and_static(dyNode.entity, stNode.entity, &deny);
				if (deny) continue;
			}

			// 段差チェック
			Vec3 adj;
			int hit_edge = 0;
			if (collideStaticXShearedBoxAndBody(stNode, dyNode, &adj, &hit_edge)) {
				Vec3 pos = _cnGetPos(dyNode.entity);
				// ShearedBox は側面がZ軸に対して斜めになっているので、
				// X軸に平行に進んでいても衝突するとZ方向に押し出されてしまう。
				// no_slide_z_ が有効な場合、元々の速度に Z 成分が含まれていなければ、
				// 補正後の速度にも Z が含まれないようにする（勝手に奥行き方向にスライドしないように）
				if (no_slide_z_) {
					// 左または右側面に接触した時のみ有効。
					// 奥又は手前に接触している場合は、Ｚ方向に補正してもらわないと困る
					if ((hit_edge & HITEDGE_XMIN) || (hit_edge & HITEDGE_XMAX)) {
						if (fabsf(dySpeed.z) < SLIDE_Z_THRESHOLD) {
							adj.z = 0;
						}
					}
				}
				pos += adj;
				_cnSetPos(dyNode.entity, pos);
				updatePhysicsCollisionPair(dyNode, stNode);
			}
		}
	}
}

bool CollisionSystem::collideStaticBoxAndBody(const _Node &stNode, const _Node &dyNode, Vec3 *delta) {
	Log_assert(delta);

	Vec3  dy_pos   = _cnGetAabbCenterInWorld(dyNode);
	Vec3  dy_aabb_min;
	getBodyShapeLocalAabb(dyNode.entity, &dy_aabb_min, NULL);
	float dy_r     = dyNode.body->shape.radius; // Sphere と Cylinder, Capsule どれでも対応できるように直接 radius を得る
	float dy_skin  = dyNode.body->desc.skin_width_;
	float dy_climb = dyNode.body->desc.climb_height_;

	// 判定の半径が skin 未満になってしまうとすり抜けリスクがあるので、
	// 半径の下限を skin にしておく
	if (dy_r < dy_skin) {
		dy_r = dy_skin;
	}

	Vec3 st_pos = _cnGetAabbCenterInWorld(stNode);
	Vec3 st_size;
	getBodyShapeBox(stNode.entity, &st_size);

	if (st_pos.y + st_size.y < dy_pos.y + dy_aabb_min.y + dy_climb) {
		return false; // 段差が小さいので接触判定を無視する（→上に登ることができる）
	}

	Vec3 dy_oldpos = _cnGetPrevAabbCenterInWorld(dyNode);
	Vec3 dy_step_delta = dy_pos - dy_oldpos;

	// XZ方向だけの移動量を見る。
	// ここでは　XZ 方向での衝突しか見ないので、
	// Y方向にだけ移動している場合 (XZ速度が0) は逐次判定する必要はない
	Vec3 step_xz = Vec3(dy_step_delta.x, 0.0f, dy_step_delta.z);

	bool static_detect = true;
	if (AVOID_TUNNELING) {
		int numstep = (int)ceilf(step_xz.getLength() / dy_r);
		if (numstep > 0) {
			static_detect = false;
			for (int i=0; i<=numstep; i++) { // iの範囲に注意。0とnumstepを含む
				float t = (float)i / numstep;
				Vec3 test_pos = dy_oldpos + dy_step_delta * t;
				Vec3 test_adj;
				if (KCollisionMath::collisionCircleWithRect2D(
					test_pos.x, test_pos.z, dy_r,
					st_pos.x, st_pos.z, st_size.x, st_size.z,
					dy_skin, &test_adj.x, &test_adj.z))
				{
					if (i == 0) {
						// 元の位置から動かさずに判定に引っかかった場合は静的判定する
						static_detect = true;
						break;
					}
					// ここで、test_adj というのは test_pos からの補正量であることに注意。
					// それに対して、delta として呼び出し元に戻すのは dy_pos からの補正量である
					delta->x = test_pos.x + test_adj.x - dy_pos.x;
					delta->z = test_pos.z + test_adj.z - dy_pos.z;
					return true;
				}
			}
		}
	}
	if (static_detect) {
		if (KCollisionMath::collisionCircleWithRect2D(
			dy_pos.x, dy_pos.z, dy_r,
			st_pos.x, st_pos.z, st_size.x, st_size.z,
			dy_skin, &delta->x, &delta->z))
		{
			return true;
		}
	}
	return false;
}


// delta    衝突を解消するために dyNode に加えるべき移動量
// hit_edge どの辺と衝突したか。HITEDGE_XMIN, HITEDGE_ZMAX, HITEDGE_XMAX, HITEDGE_ZMIN の論理和
bool CollisionSystem::collideStaticXShearedBoxAndBody(const _Node &stNode, const _Node &dyNode, Vec3 *delta, int *hit_edge) {
	Log_assert(delta);
	Log_assert(hit_edge);

	Vec3  dy_pos   = _cnGetAabbCenterInWorld(dyNode);
	Vec3  dy_aabb_min;
	getBodyShapeLocalAabb(dyNode.entity, &dy_aabb_min, NULL);
	float dy_r     = dyNode.body->shape.radius; // Sphere と Cylinder, Capsule どれでも対応できるように直接 radius を得る
	float dy_skin  = dyNode.body->desc.skin_width_;
	float dy_climb = dyNode.body->desc.climb_height_;

	// 判定の半径が skin 未満になってしまうとすり抜けリスクがあるので、
	// 半径の下限を skin にしておく
	if (dy_r < dy_skin) {
		dy_r = dy_skin;
	}

	Vec3  st_pos = _cnGetAabbCenterInWorld(stNode);
	Vec3  st_size;
	float st_shear_x;
	getBodyShapeShearedBox(stNode.entity, &st_size, &st_shear_x);

	if (st_pos.y + st_size.y < dy_pos.y + dy_aabb_min.y + dy_climb) {
		return false; // 段差が小さいので接触判定を無視する（→上に登ることができる）
	}

	Vec3 dy_oldpos = _cnGetPrevAabbCenterInWorld(dyNode);
	Vec3 dy_step_delta = dy_pos - dy_oldpos;

	// XZ方向だけの移動量を見る。
	// ここでは　XZ 方向での衝突しか見ないので、
	// Y方向にだけ移動している場合 (XZ速度が0) は逐次判定する必要はない
	Vec3 step_xz = Vec3(dy_step_delta.x, 0.0f, dy_step_delta.z);

	bool static_detect = true;
	if (AVOID_TUNNELING) {
		int numstep = (int)ceilf(step_xz.getLength() / dy_r);
		if (numstep > 0) {
			static_detect = false;
			for (int i=0; i<=numstep; i++) { // iの範囲に注意。0とnumstepを含む
				float t = (float)i / numstep;
				Vec3 test_pos = dy_oldpos + dy_step_delta * t;
				Vec3 test_adj;
				int hit = KCollisionMath::collisionCircleWithXShearedRect2D(
					test_pos.x, test_pos.z, dy_r,
					st_pos.x, st_pos.z, st_size.x, st_size.z, st_shear_x, 
					dy_skin, &test_adj.x, &test_adj.z
				);
				if (hit) {
					if (i == 0) {
						// 元の位置から動かさずに判定に引っかかった場合は静的判定する
						static_detect = true;
						break;
					}
					// ここで、test_adj というのは test_pos からの補正量であることに注意。
					// それに対して、adj として呼び出し元に戻すのは dy_pos からの補正量である
					delta->x = test_pos.x + test_adj.x - dy_pos.x;
					delta->z = test_pos.z + test_adj.z - dy_pos.z;
					*hit_edge = 0;
					if (hit & 1) *hit_edge |= HITEDGE_XMIN;
					if (hit & 2) *hit_edge |= HITEDGE_ZMAX;
					if (hit & 4) *hit_edge |= HITEDGE_XMAX;
					if (hit & 8) *hit_edge |= HITEDGE_ZMIN;
					return true;
				}
			}
		}
	}
	if (static_detect) {
		int hit = KCollisionMath::collisionCircleWithXShearedRect2D(
			dy_pos.x, dy_pos.z, dy_r, 
			st_pos.x, st_pos.z, st_size.x, st_size.z, st_shear_x, 
			dy_skin, &delta->x, &delta->z
		);
		if (hit) {
			*hit_edge = 0;
			if (hit & 1) *hit_edge |= HITEDGE_XMIN;
			if (hit & 2) *hit_edge |= HITEDGE_ZMAX;
			if (hit & 4) *hit_edge |= HITEDGE_XMAX;
			if (hit & 8) *hit_edge |= HITEDGE_ZMIN;
			return true;
		}
	}
	return false;
}
bool CollisionSystem::collideStaticYAxisWallAndBody(const _Node &stNode, const _Node &dyNode, const Vec3 &dySpeed, Vec3 *delta) {
	Log_assert(delta);

	Vec3  dy_pos   = _cnGetAabbCenterInWorld(dyNode);
	float dy_r     = dyNode.body->shape.radius; // Sphere と Cylinder, Capsule どれでも対応できるように直接 radius を得る
	float dy_skin  = dyNode.body->desc.skin_width_;

	// 判定の半径が skin 未満になってしまうとすり抜けリスクがあるので、
	// 半径の下限を skin にしておく
	if (dy_r < dy_skin) {
		dy_r = dy_skin;
	}

	Vec3  st_pos     = _cnGetAabbCenterInWorld(stNode);
	Vec3  st_normal  = stNode.body->shape.normal;
	float st_wall_dx =  st_normal.z; // 壁の面に沿うようなベクトル。左手に法線ベクトルが見えるようにする
	float st_wall_dz = -st_normal.x;
	float st_width   = stNode.body->shape.radius;

	if (st_width > 0) {
		// 壁に幅が設定されている。有限幅の壁として扱い、線分との判定を行う
		Vec3 r = Vec3(st_wall_dx, 0.0f, st_wall_dz) * st_width;
		Vec3 a = st_pos - r;
		Vec3 b = st_pos + r;

		Vec3 dy_oldpos = _cnGetPrevAabbCenterInWorld(dyNode);
		Vec3 dy_step_delta = dy_pos - dy_oldpos;

		// XZ方向だけの移動量を見る。
		// ここでは　XZ 方向での衝突しか見ないので、
		// Y方向にだけ移動している場合 (XZ速度が0) は逐次判定する必要はない
		Vec3 step_xz = Vec3(dy_step_delta.x, 0.0f, dy_step_delta.z);

		bool static_detect = true;
		if (AVOID_TUNNELING) {
			int numstep = (int)ceilf(step_xz.getLength() / dy_r);
			if (numstep > 0) {
				static_detect = false;
				for (int i=0; i<=numstep; i++) { // iの範囲に注意。0とnumstepを含む
					float t = (float)i / numstep;
					Vec3 test_pos = dy_oldpos + dy_step_delta * t;
					Vec3 test_adj;
					if (KCollisionMath::collisionCircleWithSegment2D(
						test_pos.x, test_pos.z, dy_r,
						a.x, a.z, b.x, b.z,
						dy_skin, &test_adj.x, &test_adj.z))
					{
						if (i == 0) {
							// 元の位置から動かさずに判定に引っかかった場合は静的判定する
							static_detect = true;
							break;
						}
						// ここで、test_adj というのは test_pos からの補正量であることに注意。
						// それに対して、delta として呼び出し元に戻すのは dy_pos からの補正量である
						delta->x = test_pos.x + test_adj.x - dy_pos.x;
						delta->z = test_pos.z + test_adj.z - dy_pos.z;
						return true;
					}
				}
			}
		}
		if (static_detect) {
			if (KCollisionMath::collisionCircleWithSegment2D(
				dy_pos.x, dy_pos.z, dy_r,
				a.x, a.z, b.x, b.z, dy_skin, &delta->x, &delta->z))
			{
				return true;
			}
		}

	} else {
		// 壁に幅が設定されていない。無限に続く壁として扱い、直線との判定を行う
		Vec3 r = Vec3(st_wall_dx, 0.0f, st_wall_dz);
		Vec3 a = st_pos - r;
		Vec3 b = st_pos + r;
		if (KCollisionMath::collisionCircleWithLinearArea2D(
			dy_pos.x, dy_pos.z, dy_r,
			a.x, a.z, b.x, b.z, dy_skin, &delta->x, &delta->z))
		{
			return true;
		}
	}
	return false;
}

// 法線付き平面との壁判定
void CollisionSystem::updateTerrainCollision_PlaneWall() {
	if (tmp_static_walls_.empty()) return;
	for (size_t i=0; i<dynamic_bodies_.size(); i++) {
		_Node &dyNode = dynamic_bodies_[i];
		const float dySensorR = dyNode.body->shape.radius + dyNode.body->desc.skin_width_;
		const Vec3 dyCenter = _cnGetAabbCenterInWorld(dyNode);
		const Vec3 dyAabbMin = dyCenter - Vec3(dySensorR, dySensorR, dySensorR);
		const Vec3 dyAabbMax = dyCenter + Vec3(dySensorR, dySensorR, dySensorR);
		int num_penetrates = 0;
		Vec3 wall_normal0;
		Vec3 wall_normal1;
		// 初期状態での dyNode の位置と速度
		Vec3 dySpeed = _cnGetSpeed(dyNode.entity);

		for (size_t j=0; j<tmp_static_walls_.size(); j++) {
			const _Node &stNode = tmp_static_walls_[j];
			if (stNode.entity == dyNode.entity) continue; // 同キャラ判定を排除
			bool deny = false;
			if (cb_) cb_->on_collision_update_body_and_static(dyNode.entity, stNode.entity, &deny);
			if (deny) continue;
			Log_assert(stNode.body->shape.type == ShapeType_Plane); // 既にフィルタリングされている
			const Vec3 stCenter = _cnGetAabbCenterInWorld(stNode);
			const Vec3 stNormal = stNode.body->shape.normal;
			bool touch_wall = false;

			// XZ平面に対して垂直な壁への衝突を解決
			if (stNormal.y == 0) {
				Vec3 delta;
				if (collideStaticYAxisWallAndBody(stNode, dyNode, dySpeed, &delta)) {
					Vec3 newpos = _cnGetPos(dyNode.entity);
					// no_slide_z_ が有効な場合、元々の速度に Z 成分が含まれていなければ、
					// 補正後の速度にも Z が含まれないようにする（勝手に奥行き方向にスライドしないように）
					if (no_slide_z_) {
						if (fabsf(dySpeed.z) < SLIDE_Z_THRESHOLD) {
							delta.z = 0;
						}
					}
					newpos += delta;
					_cnSetPos(dyNode.entity, newpos);
					touch_wall = true;
					if (num_penetrates == 0) {
						wall_normal0 = stNormal;
					} else {
						wall_normal1 = stNormal;
					}
					dySpeed = newpos - _cnGetPrevPos(dyNode.entity); // 衝突のため見かけの速度が変化した。次の壁と判定する時は、変更後の速度と、壁の法線を比較することに注意せよ
					num_penetrates++;
				}
			}
			if (num_penetrates > 1) {
				// 同時に二か所以上のY軸壁に接触した。
				// この、２つの壁の法線が向かい合っている場合、
				// １つの壁に対しては正しく反発処理されていても、
				// もう１つの壁に対して正しく処理されていない場合がある。事故を防ぐため、
				// 法線が向かい合っているような複数の壁に接触した場合は完全停止させる
				if (wall_normal0.dot(wall_normal1) < 0) {
					// 脱出方向を得る。
					//
					// 向かい合った壁の法線ベクトルを合成する
					// 内積が非ゼロの法線ベクトル同士を合成しているので、
					// 必ず壁が開けたほうに向かうベクトルが得られるはず。
					Vec3 escape_vec = wall_normal0 + wall_normal1;
					if (escape_vec.isZero()) {
						Log_error("E_NORMALIZE_ZERO_VECTOR: CollisionSystem::updateTerrainCollision_PlaneWall");
						return;
					}
					Vec3 escape_dir = escape_vec.getNormalized();
					escape_dir.y = 0; // 元々ゼロのはずだが、念のため。

					// 直前の座標に戻す
					Vec3 prev_pos = _cnGetPrevPos(dyNode.entity);
					Vec3 pos = _cnGetPos(dyNode.entity);
					pos.x = prev_pos.x;
				//	pos.y = <-- Y軸壁との判定ではY座標は無関係なので，Y値操作しない
					pos.z = prev_pos.z;

					// 念のため、さらに脱出方向に押し返す
					pos += escape_dir;

					_cnSetPos(dyNode.entity, pos);
				}
			}
			// Y+ 向きの平面への衝突を解決（地面との衝突）
			if (stNormal == Vec3(0, 1, 0)) {
				if (dyAabbMin.y <= stCenter.y) {
					float penetration_depth = stCenter.y - dyAabbMin.y;
					_cnMove(dyNode.entity, 0, penetration_depth, 0);
					touch_wall = true;
					num_penetrates++;
				}
			}
			// Y- 向きの平面への衝突を解決（天井との衝突）
			if (stNormal == Vec3(0, -1, 0)) {
				if (stCenter.y <= dyAabbMax.y) {
					float penetration_depth = dyAabbMax.y - stCenter.y;
					_cnMove(dyNode.entity, 0, -penetration_depth, 0);
					touch_wall = true;
					num_penetrates++;
				}
			}
			if (touch_wall) {
				updatePhysicsCollisionPair(dyNode, stNode);
			}
		}
	}
}

// 高度計測の基準になる地面表面のY座標を得る
bool CollisionSystem::getAltitudeBaseFloorY(const _Node &dyNode, const _Node &stNode, float *floor_y) {
	Log_assert(floor_y);

	Vec3 dy_pos = _cnGetAabbCenterInWorld(dyNode);
	Vec3 st_pos = _cnGetAabbCenterInWorld(stNode);
	switch (stNode.body->shape.type) {
	case ShapeType_Box:
		// BOX との判定
		// XZ 面の真上にいるかどうかを調べる
		{
			Vec3 halfsize;
			getBodyShapeBox(stNode.entity, &halfsize);
			if (!KCollisionMath::isPointInRect2D(dy_pos.x, dy_pos.z, st_pos.x, st_pos.z, halfsize.x, halfsize.z)) {
				return false;
			}
			*floor_y = st_pos.y + halfsize.y;
			break;
		}
	case ShapeType_ShearedBox:
		// 平行四辺形 との判定
		// XZ 面の真上にいるかどうかを調べる
		{
			Vec3 halfsize;
			float xshear;
			getBodyShapeShearedBox(stNode.entity, &halfsize, &xshear);
			if (!KCollisionMath::isPointInXShearedRect2D(dy_pos.x, dy_pos.z, st_pos.x, st_pos.z, halfsize.x, halfsize.z, xshear)) {
				return false;
			}
			*floor_y = st_pos.y + halfsize.y;
			break;
		}
	case ShapeType_Ground:
	case ShapeType_Plane:
		*floor_y = st_pos.y;
		break;
	}
	return true;
}

// 高度を再計算する
void CollisionSystem::refreshAltitudes() {
	updatePhysicsNodeList();
	updateStaticBodyList();
	clearBodyAltitudes();
	updateTerrainCollision_Ground(true);
}

// 地面との着地判定
void CollisionSystem::updateTerrainCollision_Ground(bool no_bounce) {
	for (size_t i=0; i<dynamic_bodies_.size(); i++) {
		_Node &dyNode = dynamic_bodies_[i];
		// ボディ底面の y 座標を得る
		Vec3 dy_center = _cnGetAabbCenterInWorld(dyNode);

		// ボディ中央を基準にしたときの高度を測る。ボディ底面から測るのはダメ。そこを基準にしてしまうと、
		// 基準点より上の地面がすべて無視される。
		float alt = 0;
		_Node stNode;
		if (!getAltitudeAtPointEx(dy_center, dyNode.body->desc.snap_height_, &alt, &stNode)) {
			// 地面が存在しない。高度値無効
			dyNode.body->state.altitude = 0;
			dyNode.body->state.has_altitude = false;
			continue;
		}

		// 底面はある程度の大きさを持っているので
		// 底面の中心だけでなく、その周辺の点についても高度を調べる。
		// 最も低い高度値を選ぶ
		if (DETECT_GROUND_USING_MULTIPLE_POINTS) {
			if (dyNode.body->shape.type == ShapeType_Capsule) {
				float r = dyNode.body->shape.radius * DETECT_GROUND_POSITION_K; // 底面の中心からどのくらい離れた点について調べる？
				float alt0 = getAltitudeAtPoint(dy_center+Vec3(-r, 0.0f, 0.0f), dyNode.body->desc.snap_height_);
				float alt1 = getAltitudeAtPoint(dy_center+Vec3( r, 0.0f, 0.0f), dyNode.body->desc.snap_height_);
				float alt2 = getAltitudeAtPoint(dy_center+Vec3(0.0f, 0.0f, -r), dyNode.body->desc.snap_height_);
				float alt3 = getAltitudeAtPoint(dy_center+Vec3(0.0f, 0.0f,  r), dyNode.body->desc.snap_height_);
				if (alt0 >= 0 && alt0 < alt) { alt = alt0; }
				if (alt1 >= 0 && alt1 < alt) { alt = alt1; }
				if (alt2 >= 0 && alt2 < alt) { alt = alt2; }
				if (alt3 >= 0 && alt3 < alt) { alt = alt3; }
			}
		}

		// ボディ底面からの高度値になおす
		Vec3 dy_aabb_min;
		getBodyShapeLocalAabb(dyNode.entity, &dy_aabb_min, NULL);
		alt += dy_aabb_min.y;

		// 上昇中であれば着地しない
		Vec3 dy_spd = _cnGetSpeed(dyNode.entity);
		if (dy_spd.y > 0) {
			dyNode.body->state.altitude = alt;
			dyNode.body->state.has_altitude = true;
			continue;
		}

		// 高度がスナップ距離以上あれば着地しない
		if (alt >= dyNode.body->desc.snap_height_) {
			dyNode.body->state.altitude = alt;
			dyNode.body->state.has_altitude = true;
			continue;
		}

		// めり込まないように移動
		Vec3 pos = _cnGetPos(dyNode.entity);
		pos.y -= alt;
		_cnSetPos(dyNode.entity, pos);

		// バウンドまたは地面の上をすべる処理
		processLanding(dyNode, stNode, dy_spd, no_bounce);
	}
}

// 地形判定用のオブジェクトリストを更新する
void CollisionSystem::updateStaticBodyList() {
	// 静止オブジェクト側のフィルタリング
	tmp_static_walls_.clear();
	tmp_static_boxes_.clear();
	tmp_static_shearedboxes_.clear();
	tmp_static_floors_.clear();
	for (size_t j=0; j<static_bodies_.size(); j++) {
		_Node &stNode = static_bodies_[j];

		Vec3 normal;
		switch (stNode.body->shape.type) {
		case ShapeType_Plane:
			getBodyShapePlane(stNode.entity, &normal);
			tmp_static_walls_.push_back(stNode);

			if (normal.y > 0) {
				if (stNode.body->shape.radius > 0) {
					// 地面として使える平面は、無限サイズのみ対応する。
					// 有限サイズの地面を使いたい場合は、
					// ShapeType_Plane ではなく ShapeType_Box を使う
					Log_error("E_INVLAID_SHAPE: limited plane cannot be used as a ground. use box or unlimited plane");
				}
				tmp_static_floors_.push_back(stNode);
			}
			break;
		case ShapeType_Ground:
			tmp_static_floors_.push_back(stNode);
			break;
		case ShapeType_Box:
			tmp_static_boxes_.push_back(stNode);
			tmp_static_floors_.push_back(stNode);
			break;
		case ShapeType_ShearedBox:
			tmp_static_shearedboxes_.push_back(stNode);
			tmp_static_floors_.push_back(stNode);
		break;
		}
	}
}

// 高度情報を初期化する
void CollisionSystem::clearBodyAltitudes() {
	for (int i=0; i<(int)dynamic_bodies_.size(); i++) {
		const _Node &dyNode = dynamic_bodies_[i];
		dyNode.body->state.altitude = 0;
		dyNode.body->state.has_altitude = false;
		dyNode.body->state.ground_entity = NULL;
	}
}

// 地形との判定
void CollisionSystem::updateTerrainCollision() {
	// 高度情報を初期化する
	clearBodyAltitudes();

	// 地形用オブジェクトリストを更新
	updateStaticBodyList();

	// BOX側面との壁判定
	updateTerrainCollision_BoxWall();

	// ShearedBox側面との壁判定
	updateTerrainCollision_ShearedBoxWall();

	// 法線付き平面との接触判定
	updateTerrainCollision_PlaneWall();

	// 地面への着地判定
	updateTerrainCollision_Ground(false);
}
void CollisionSystem::updatePhysicsCollisionPair(const _Node &body_node, const _Node &wall_node) {
	int pair_index = getBodyHitPairIndex(body_node.entity, wall_node.entity);
	if (pair_index >= 0) {
		// 同じ衝突ペアが見つかった。衝突状態が持続している
		HitPair &pair = body_hitpairs_[pair_index];
		pair.timestamp_lastupdate = frame_counter_;
		pair.object1.hitbox_pos = _cnGetAabbCenterInWorld(body_node);
		pair.object2.hitbox_pos = _cnGetAabbCenterInWorld(wall_node);
		physicalCollisionStay(pair);
	} else {
		// 同じ衝突ペアが見つからなかった。
		// このフレームで新しく衝突した
		HitPair pair;
		pair.object1.hitbox     = NULL;
		pair.object2.hitbox     = NULL;
		pair.object1.entity     = body_node.entity;
		pair.object2.entity     = wall_node.entity;
		pair.object1.hitbox_pos = _cnGetAabbCenterInWorld(body_node);
		pair.object2.hitbox_pos = _cnGetAabbCenterInWorld(wall_node);
		pair.timestamp_enter    = frame_counter_;
		pair.timestamp_exit     = -1;
		pair.timestamp_lastupdate = frame_counter_;
		body_hitpairs_.push_back(pair);
		physicalCollisionEnter(pair);
	}
}

bool CollisionSystem::getGroundPoint(const Vec3 &pos, float max_penetration, float *out_ground_y) const {
	return getGroundPointEx(pos, max_penetration, out_ground_y, NULL);
}
bool CollisionSystem::getGroundPointEx(const Vec3 &pos, float max_penetration, float *out_ground_y, _Node *out_ground_node) const {
	const float INVALID_Y = -1000000;

	float ground_y = INVALID_Y;
	for (auto it=tmp_static_floors_.begin(); it!=tmp_static_floors_.end(); ++it) {
		const _Node &gnd_node = *it;
		const Vec3 gnd_pos = _cnGetAabbCenterInWorld(gnd_node);
		// 座標 (pos.x, pos.z) における地面の y 座標を得る
		float gnd_y = 0;
		if (gnd_node.body->shape.type == ShapeType_Ground) {
			gnd_y = gnd_pos.y;
		}
		if (gnd_node.body->shape.type == ShapeType_Plane) {
			// 平面の法線が Y 軸上向きならば地面とする
			Vec3 plane_normal;
			getBodyShapePlane(gnd_node.entity, &plane_normal);
			if (plane_normal.y > 0) {
				gnd_y = gnd_pos.y;
			}
		}
		if (gnd_node.body->shape.type == ShapeType_Box) {
			// BOX との判定
			// BOX の XZ 面の真上にいるかどうかを調べる
			Vec3 halfsize;
			getBodyShapeBox(gnd_node.entity, &halfsize);
			const Vec3 size = halfsize;
			if (KCollisionMath::isPointInRect2D(pos.x, pos.z, gnd_pos.x, gnd_pos.z, size.x, size.z)) {
				gnd_y = gnd_pos.y + halfsize.y;
			}
		}
		if (gnd_node.body->shape.type == ShapeType_ShearedBox) {
			// XZ平面上の平行四辺形との判定
			Vec3 halfsize;
			float xShear;
			getBodyShapeShearedBox(gnd_node.entity, &halfsize, &xShear);
			const Vec3 size = halfsize;
			if (KCollisionMath::isPointInXShearedRect2D(pos.x, pos.z, gnd_pos.x, gnd_pos.z, size.x, size.z, xShear)) {
				gnd_y = gnd_pos.y + halfsize.y;
			}
		}

		// 検索開始位置に最も近い地面 y を得られるようにする。
		if (gnd_y < pos.y + max_penetration) {
			if (ground_y < gnd_y) {
				// 既知の地面よりも上側にあるので、この点を仮採用する
				ground_y = gnd_y;
				if (out_ground_node) *out_ground_node = gnd_node;
			}
		} else {
			// 地面が調査点よりも上側にある。無視する
		}
	}
	if (ground_y > INVALID_Y) {
		if (out_ground_y) *out_ground_y = ground_y;
		return true;
	}
	return false;
}


float CollisionSystem::getAltitudeAtPoint(const Vec3 &point, float max_penetration) const {
	float alt = -1;
	getAltitudeAtPointEx(point, max_penetration, &alt, NULL);
	return alt;
}
bool CollisionSystem::getAltitudeAtPointEx(const Vec3 &point, float max_penetration, float *out_alt, _Node *out_ground) const {
	float gnd_y;
	if (getGroundPointEx(point, max_penetration, &gnd_y, out_ground)) {
		if (out_alt) *out_alt = point.y - gnd_y;
		return true;
	}
	return false;
}
// 接地処理を行う。バウンドまたは接地状態気をを維持しながら地面の上を滑るように動かす
void CollisionSystem::processLanding(_Node &dyNode, const _Node &stNode, const Vec3 &dy_spd, bool no_bounce) {
	// 地面との接触処理する
	updatePhysicsCollisionPair(dyNode, stNode);

	// 高度は必ず 0 になる。地面情報を更新する
	dyNode.body->state.altitude = 0;
	dyNode.body->state.has_altitude = true;
	dyNode.body->state.ground_entity = stNode.entity;

	if (no_bounce) {
		_cnSetSpeed(dyNode.entity, Vec3());
		return;
	}
	
	// 新しい速度を決める。
	// まだバウンドは考慮しない
	Vec3 newspeed(dy_spd.x, 0.0f, dy_spd.z);

	// バウンドするための最低Ｙ速度を上回っている場合だけバウンド考慮する
	float min_bounce = dyNode.body->desc.bounce_min_speed_;
	float gravity = dyNode.body->desc.gravity_;
	Log_assert(min_bounce > 0);
	if (min_bounce < gravity) {
		// 重力加速度よりも小さなバウンドは注意が必要。
		// 静止していたとしても重力加速度分だけ下向きに速度がかかっているため、バウンドしてしまう
		Log_warning("min_bounce < gravity");
		min_bounce = gravity; // 補正
	}
	if (fabsf(dy_spd.y) >= min_bounce) {
		newspeed.y = dy_spd.y * -1 * dyNode.body->desc.bounce_;
		newspeed.x *= dyNode.body->desc.bounce_horz_;
		newspeed.z *= dyNode.body->desc.bounce_horz_;
		dyNode.body->state.bounce_counting++;

	} else {
		float fric = dyNode.body->desc.sliding_friction_;
		if (fric > 0) {
			float L = newspeed.getLength();
			float k = 0.0f;
			if (fric < L) {
				k = (L - fric) / L;
			}
			newspeed *= k;
		}
	}
	// 注意！！ ここでの newspd は、speed_scale の影響を受けた値である。
	// speed_scale 適用後の速度に対する操作の結果として得られた新速度を、
	// そのまま setSpeed してはいけない。
	// setSpeed でセットしないといけないのは、 speed_scale する前の設定速度値である
	float speed_scale = getSpeedFactor(dyNode.entity);
	if (speed_scale > 0.0f) {
		_cnSetSpeed(dyNode.entity, newspeed / speed_scale);
	}
}

/// センサーボックス（互いに貫通し、相互作用しない）を持つエンティティだけを集めたリストを作成する
void CollisionSystem::updateSensorNodeList() {
	Log_assert(engine_);
	group_nodes_.clear();
	group_nodes_.resize(groups_.size());
	for (auto it=nodes_.begin(); it!=nodes_.end(); ++it) {
		EID e = it->first;
		_Node &eNode = it->second;
		
		// コライダーがアタッチされていなければ判定しない
		if (! eNode.col.is_attached) continue;

		// コライダーが Disabled だったら判定しない
		if (! eNode.col.enabled) continue;

		// Entity がツリー内で Disabled だったら判定しない
		// （paused 状態は考慮しない。オブジェクトの動きが停止している時でも、勝手に衝突判定を突き抜けられては困る）
		if (!hs_->isEnabledInTree(e)) continue;
		
		for (size_t i=0; i<eNode.col.hitboxes.size(); i++) {
			HitBox *hitbox = &eNode.col.hitboxes[i];
			if (hitbox->is_enabled()) {
				_HitBoxNode cNode;
				cNode.entity = e;
				cNode.hitbox = hitbox;
			//	cNode.is_statc = !eNode.vel.is_attached;
				group_nodes_[hitbox->group_id()].push_back(cNode);
			}
		}
	}
}
/// センサーボックス（互いに貫通し、相互作用しない）同士の衝突処理
/// ここでの判定対象となるのは、予め updateSensorNodeList() で作成されたリストに含まれるエンティティのみ。それ以外は判定処理しない
void CollisionSystem::updateSensorCollision() {
	for (CollisionGroupId i=0; i<(int)groups_.size(); i++) {
		for (CollisionGroupId j=i+1; j<(int)groups_.size(); j++) {
			const CollisionGroup &group1 = groups_[i];
			const CollisionGroup &group2 = groups_[j];
			// グループの組み合わせによるフィルタリングを通過したら、それぞれのグループに属するセンサーボックス同士で衝突処理を行う
			if (group1.is_collidable_with(j) && group2.is_collidable_with(i)) {
				updateSensorCollision_group(i, j);
			}
		}
	}
}
/// グループ group_id1 と group_id2 のそれぞれのグループに属するセンサーボックス同士で衝突処理を行う
void CollisionSystem::updateSensorCollision_group(CollisionGroupId group_id1, CollisionGroupId group_id2) {
	Log_assert(group_id1 != group_id2); // 同一グルーブ同士の比較はダメ
	const std::vector<_HitBoxNode> &nodes1 = group_nodes_[group_id1];
	const std::vector<_HitBoxNode> &nodes2 = group_nodes_[group_id2];
	for (size_t i=0; i<nodes1.size(); i++) {
		for (size_t j=0; j<nodes2.size(); j++) {
			const _HitBoxNode &cNode1 = nodes1[i];
			const _HitBoxNode &cNode2 = nodes2[j];
			if (cNode1.entity == cNode2.entity) continue; // 同キャラ判定を排除
			updateSensorCollision_node(cNode1, cNode2);
		}
	}
}
/// 衝突ノード node1 と node2 の衝突判定と処理を行う
void CollisionSystem::updateSensorCollision_node(const _HitBoxNode &cNode1, const _HitBoxNode &cNode2) {
	bool deny = false;
	if (cb_) cb_->on_collision_update_sensor(cNode1.entity, cNode1.hitbox, cNode2.entity, cNode2.hitbox, &deny);
	if (deny) return;
	const Vec3 node1_pos = getHitBoxCenterPosInWorld(cNode1);
	const Vec3 node2_pos = getHitBoxCenterPosInWorld(cNode2);
	const Vec3 delta = node1_pos - node2_pos;
	const Vec3 r = cNode1.hitbox->half_size() + cNode2.hitbox->half_size();
	if (fabsf(delta.x) < r.x && fabsf(delta.y) < r.y && fabsf(delta.z) < r.z) {
		int pair_index = getSensorHitPairIndex(cNode1.hitbox, cNode2.hitbox);
		if (pair_index >= 0) {
			// 同じ衝突ペアが見つかった。衝突状態が持続している
			HitPair &pair = sensor_hitpairs_[pair_index];
			pair.timestamp_lastupdate = frame_counter_;
			pair.object1.hitbox_pos   = node1_pos;
			pair.object2.hitbox_pos   = node2_pos;
			sensorCollisionStay(pair);
		} else {
			// 同じ衝突ペアが見つからなかった。
			// このフレームで新しく衝突した
			HitPair pair;
			pair.object1.hitbox     = cNode1.hitbox;
			pair.object2.hitbox     = cNode2.hitbox;
			pair.object1.entity     = cNode1.entity;
			pair.object2.entity     = cNode2.entity;
			pair.object1.hitbox_pos = node1_pos;
			pair.object2.hitbox_pos = node2_pos;
			pair.timestamp_enter    = frame_counter_;
			pair.timestamp_exit     = -1;
			pair.timestamp_lastupdate = frame_counter_;
			sensor_hitpairs_.push_back(pair);
			sensorCollisionEnter(pair);
		}
	}
}

static void _CallHitboxCallback(const CollisionSystem::HitObject &self, const CollisionSystem::HitObject &other, bool is_repeat) {
	if (self.hitbox->callback) {
		HitBoxEvent e;

		e.self_entity = self.entity;
		e.self_hitbox = self.hitbox;
		e.self_hitpos = self.hitbox_pos;

		e.other_entity = other.entity;
		e.other_hitbox = other.hitbox;
		e.other_hitpos = other.hitbox_pos;

		e.is_repeat = is_repeat;
		e.data = self.hitbox->callback_data; // 文字列アドレスだけコピー

		self.hitbox->callback(e);
	}
}

void CollisionSystem::sensorCollisionEnter(HitPair &pair) {
	_CallHitboxCallback(pair.object1, pair.object2, false);
	_CallHitboxCallback(pair.object2, pair.object1, false);
}
void CollisionSystem::sensorCollisionStay(HitPair &pair) {
	_CallHitboxCallback(pair.object1, pair.object2, true);
	_CallHitboxCallback(pair.object2, pair.object1, true);
}
void CollisionSystem::sensorCollisionExit(HitPair &pair) {
}
void CollisionSystem::beginUpdate() {
	// 前フレームで衝突が解消されたペアを削除する
	for (auto it=sensor_hitpairs_.begin(); it!=sensor_hitpairs_.end(); /*++it*/) {
		if (it->timestamp_exit >= 0) {
			it = sensor_hitpairs_.erase(it);
		} else {
			++it;
		}
	}
	for (auto it=body_hitpairs_.begin(); it!=body_hitpairs_.end(); /*++it*/) {
		if (it->timestamp_exit >= 0) {
			it = body_hitpairs_.erase(it);
		} else {
			++it;
		}
	}
}
void CollisionSystem::endUpdate() {
	// 衝突解消の検出
	// ただし、衝突中にエンティティ自体が消えた場合は EXIT イベントが発生しないことに注意
	for (auto it=sensor_hitpairs_.begin(); it!=sensor_hitpairs_.end(); ++it) {
		HitPair &pair = *it;
		if (pair.timestamp_lastupdate < frame_counter_) {
			// タイムスタンプが古いままになっているという事は、このフレームで衝突が検出されなかったことを意味する
			// 衝突解消時刻を記録しておく
			pair.timestamp_exit = frame_counter_;
			pair.timestamp_lastupdate = frame_counter_; // 衝突解消時刻をもって最後の更新時刻とする。これをしないと、HitPair::isExit() の条件式が正しく動作しない
			sensorCollisionExit(pair);
		}
	}
	for (auto it=body_hitpairs_.begin(); it!=body_hitpairs_.end(); ++it) {
		HitPair &pair = *it;
		if (pair.timestamp_lastupdate < frame_counter_) {
			// タイムスタンプが古いままになっているという事は、このフレームで衝突が検出されなかったことを意味する
			// 衝突解消時刻を記録しておく
			pair.timestamp_exit = frame_counter_;
			pair.timestamp_lastupdate = frame_counter_; // 衝突解消時刻をもって最後の更新時刻とする。これをしないと、HitPair::isExit() の条件式が正しく動作しない
			physicalCollisionExit(pair);
		}
	}
}



void Test_collision() {
#ifdef _DEBUG
	Vec3 c, s;

	Log_assert(!KCollisionMath::isAabbIntersected(Vec3(0, 0, 0), Vec3(10, 10, 0), Vec3( 100, 10, 0), Vec3(5, 5, 0), &c, &s));
	Log_assert(!KCollisionMath::isAabbIntersected(Vec3(0, 0, 0), Vec3(10, 10, 0), Vec3(-100, 10, 0), Vec3(5, 5, 0), &c, &s));
	Log_assert(!KCollisionMath::isAabbIntersected(Vec3(0, 0, 0), Vec3(10, 10, 0), Vec3( 10, 100, 0), Vec3(5, 5, 0), &c, &s));
	Log_assert(!KCollisionMath::isAabbIntersected(Vec3(0, 0, 0), Vec3(10, 10, 0), Vec3( 10,-100, 0), Vec3(5, 5, 0), &c, &s));

	// 片方が片方を完全に内包している
	Log_assert(KCollisionMath::isAabbIntersected(Vec3(0, 0, 0), Vec3(10, 10, 10), Vec3(0, 0, 0), Vec3(20, 20, 20), &c, &s));
	Log_assert(c == Vec3(0, 0, 0));
	Log_assert(s == Vec3(10, 10, 10));

	// めり込み深さ0の場合、つまり接しているだけの場合は衝突とみなす。
	Log_assert(KCollisionMath::isAabbIntersected(Vec3(0, 0, 0), Vec3(10, 10, 0), Vec3(15, 15, 0), Vec3(5, 5, 0), &c, &s));
	Log_assert(c == Vec3(10, 10, 0));
	Log_assert(s == Vec3( 0,  0, 0));

	Log_assert(KCollisionMath::isAabbIntersected(Vec3(0, 0, 0), Vec3(10, 10, 0), Vec3(15, 15, 0), Vec3(9, 9, 0), &c, &s));
	Log_assert(c == Vec3(8, 8, 0));
	Log_assert(s == Vec3(2, 2, 0));
	
	Log_assert(Num_equals(KCollisionMath::getSignedDistanceOfLinePoint2D(0, 0, 0, 100, 100, 0), -hypotf(50, 50))); // 点P(0, 0) と、A(0, 100) B(100, 0) を通る直線の距離。A から B を見た時、点Pは右側にあるので正の距離を返す
	Log_assert(Num_equals(KCollisionMath::getSignedDistanceOfLinePoint2D(0, 0, 100, 0, 0, 100),  hypotf(50, 50))); // 点P(0, 0) と、A(100, 0) B(0, 100) を通る直線の距離。A から B を見た時、点Pは左側にあるので負の距離を返す

	// 円(0, 0, R=80) と点(50, 50) の衝突と解決。
	// 衝突解決には、円を左下に向かって 80-hypotf(50, 50) だけ移動させる
	float adj_x, adj_y;
	Log_assert(KCollisionMath::collisionCircleWithPoint2D(0,0,80,  50,50,  1.0f, &adj_x, &adj_y));
	Log_assert(Num_equals(hypot(adj_x, adj_y), 80-hypotf(50, 50)));
	Log_assert(adj_x < 0 && adj_y < 0); // 左下に移動させるので両方とも負の値になる

	// 円(0, 0, R=80) と直線点(0, 90)-(90, 0) の衝突と解決。
	// 衝突解決には、円を左下に向かって 80-hypotf(90, 90)/2 だけ移動させる
	Log_assert(KCollisionMath::collisionCircleWithLine2D(0,0,80,  0,90,  90,0,  1.0f, &adj_x, &adj_y));
	Log_assert(Num_equals(hypot(adj_x, adj_y), 80-hypotf(90, 90)/2));
	Log_assert(adj_x < 0 && adj_y < 0); // 左下に移動させるので両方とも負の値になる

	// 円１(0, 0, R=50) と円２(90, 0, R=50) の衝突と解決。
	// 衝突解決には円１を左側に 10 だけ移動させる
	Log_assert(KCollisionMath::collisionCircleWithCircle2D(0,0,50,  90,0,50,  1.0f, &adj_x, &adj_y));
	Log_assert(Num_equals(adj_x, -10));
	Log_assert(Num_equals(adj_y,  0));
#endif // _DEBUG
}

#pragma endregion // CollisionSystem


} // namespace

