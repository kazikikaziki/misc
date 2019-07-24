#pragma once
#include "GameAction.h"
#include "GameWindowDebugMenu.h"
#include "KLog.h"

namespace mo {

class HierarchySystem;

typedef int CollisionGroupId;

struct HitBox {
	HitBox();

	/// AABBの中心座標（エンティティからの相対座標）
	const Vec3 & center() const;
	void set_center(const Vec3 &p);

	/// AABBの広がり（AABB直方体の各辺の半分のサイズを指定。半径のようなもの）
	const Vec3 & half_size() const;
	void set_half_size(const Vec3 &s);
	/// 衝突判定のグループ分け。グループの組み合わせによって、衝突判定するかどうかが決まる。
	/// 衝突判定を行うグループの組み合わせなどは CollisionSystem::setGroupInfo() で設定する
	CollisionGroupId group_id() const;
	void set_group_id(CollisionGroupId id);

	/// この判定を有効にするかどうか
	bool is_enabled() const;
	void set_enabled(bool b);



	void (*callback)(struct HitBoxEvent &e);
	char callback_data[64];

	/// ユーザーが自由に使えるフィールド。
	/// この判定に何か情報を関連付けたい時に使う
	Path user_tag;
	int user_param0;
	int user_param1;
	int user_param2;
	int user_param3;

	Vec3 center_;
	Vec3 half_size_;
	CollisionGroupId group_id_;
	bool enabled_;
};


struct HitBoxEvent {
	HitBoxEvent() {
		self_entity = NULL;
		self_hitbox = NULL;
		other_entity = NULL;
		other_hitbox = NULL;
		is_repeat = false;
		data = NULL;
	}

	EID     self_entity;
	HitBox *self_hitbox;
	Vec3    self_hitpos;

	EID     other_entity;
	HitBox *other_hitbox;
	Vec3    other_hitpos;

	bool is_repeat; // 接触が継続している場合、2回目以降の呼び出し時に true になる
	const char *data;
};


struct BodyDesc {
	BodyDesc() {
		gravity_ = 0.0f;
		penetratin_response_ = 0.0f;
		skin_width_ = 0.0f;
		snap_height_ = 0.0f;
		climb_height_ = 0.0f;
		bounce_ = 0.0f;
		bounce_horz_ = 0.0f;
		bounce_min_speed_ = 0.0f;
		sliding_friction_ = 0.0f;
		dynamic_collision_enabled_ = true;
	}

	float gravity_;

	// 物理判定同士が衝突したときに、どれだけ押し返されるか。
	// 0.0 だと全く押し返されない（相手を押しのける）
	// 1.0 だと完全に押し返される（相手に道を譲る）
	float penetratin_response_; // 衝突して相手物体にめり込んだ時の反発感度(0.0 - 1.0)。0.0だとめり込んだまま戻らない。1.0だと瞬時に戻る

	float skin_width_; // 左右、奥行き方向の接触誤差。相手がこの範囲内にいる場合は接触状態とみなす

	float snap_height_; // 地面の接触誤差。地面がこの範囲内にある場合は着地状態とみなす

	float climb_height_; // 登ることのできる最大段差。これ以上高い段差だった場合は壁への衝突と同じ扱いになる

	float bounce_; // 高さ方向の反発係数（正の値）
	float bounce_horz_; // 水平方向の反発係数（正の値）
	float bounce_min_speed_; // この速度以下で衝突した場合は反発せずに停止する

	// 物体と接しているときの速度減衰。
	// （１フレーム当たりの速度低下値）
	float sliding_friction_;

	// 動的オブジェクト同士の判定を有効にする。
	// false の場合は、静的オブジェクトにしか当たらない
	bool dynamic_collision_enabled_;
};

enum ShapeType {
	ShapeType_None,
	ShapeType_Sphere,
	ShapeType_Box,
	ShapeType_Ground,
	ShapeType_Plane,
	ShapeType_Capsule,
	ShapeType_ShearedBox,
};

struct BodyShape {
	BodyShape() {
		radius = 0;
		shearx = 0;
		halfsize = Vec3();
		normal = Vec3();
		type = ShapeType_None;
	}
	ShapeType type;

	// 衝突判定形状の半径。カプセルまたは球体で使用する。
	// カプセルの場合は、円柱の上下につながる半球部分の半径の両方を表す。
	float radius;

	// 歪み量。平行四辺形で使用する。
	// halfsize で表される直方体を基本とし、奥側の面を右に shaerx_ だけずらし、手前の面を左に shaerx_ だけずらしてできる形
	// 奥面の左端と、手前面の左端では shaerx_ * 2 だけ x 座標に差があることになる
	float shearx;

	// 衝突判定形状の高さ/2。カプセルまたは直方体で使用する。
	// カプセルの場合、半球部を除いた円柱の高さ/2を表す
	Vec3 halfsize;
	
	// 法線ベクトル。ShapeType_Plane で使用する。
	Vec3 normal;
};


struct BodyShape2 {
	struct SPHERE {
		float r;
	};
	struct BOX {
		float halfx;
		float halfy;
		float halfz;
	};
	struct PLANE {
		float norx;
		float nory;
		float norz;
	};
	struct CAPSULE {
		float halfy;
		float r;
	};
	struct SHEAREDBOX {
		float halfx;
		float halfy;
		float halfz;
		// 奥側の面を右に shaerx だけずらし、手前の面を左に shaerx だけずらす
		// 奥面の左端と、手前面の左端では shaerx * 2 だけ x 座標に差があることになる
		float shearx;
	};

	ShapeType type;

	BodyShape2();
	union {
		SPHERE u_sphere;
		BOX u_box;
		PLANE u_plane;
		CAPSULE u_capsule;
		SHEAREDBOX u_sheared_box;
	};

	bool isSphere()     const { return type == ShapeType_Sphere; }
	bool isBox()        const { return type == ShapeType_Box; }
	bool isGround()     const { return type == ShapeType_Ground; }
	bool isPlane()      const { return type == ShapeType_Plane; }
	bool isCapsule()    const { return type == ShapeType_Capsule; }
	bool isShearedBox() const { return type == ShapeType_ShearedBox; }

	const SPHERE * asSphere() const;
	const BOX * asBox() const;
	const PLANE * asPlane() const;
	const CAPSULE * asCapsule() const;
	const SHEAREDBOX * asShearedBox() const;

	void clear();
	void setSphere(const SPHERE &s);
	void setSphere(float r);
	void setBox(float half_x, float half_y, float half_z);
	void setBox(const BOX &s);
	void setPlane(float nor_x, float nor_y, float nor_z);
	void setPlane(const PLANE &s);
	void setCapsule(float halfy, float r);
	void setCapsule(const CAPSULE &s);
	void setShearedBox(float half_x, float half_y, float half_z, float shear_x);
	void setShearedBox(const SHEAREDBOX &s);
	void setGround();
};

struct BodyState {
	BodyState() {
		altitude = 0;
		has_altitude = false;
		ground_entity = NULL;
		awake_time = -1;
		bounce_counting = 0;
		is_static = 0;
	}
	// 剛体の高度。剛体の底面高さと、その直下にある地面高さの差になる
	// 地面が全く存在せずに高度が定義できない場合、altitude は 0 になり has_altitude が false にセットされる
	float altitude;

	// altitude に有効な値が入っているなら true になる。
	// 地面が全く存在しない場合は false になる。
	bool has_altitude;

	EID ground_entity;
	int awake_time;

	// バウンド回数
	int bounce_counting;

	int is_static; // -1, 0, 1 の値を取る事に注意
};


struct CollisionGroup {
	CollisionGroup() {
		mask = 0; // 初期設定では誰とも衝突しない
		gizmo_visible = true;
	}

	/// 全てのグループとの衝突を無効にする
	void clear_mask() {
		mask = 0;
	}

	/// 全てのグループとの衝突を有効にする
	void collide_with_any() {
		mask = -1;
	}

	/// 指定グループとの衝突を有効にする
	void collide_with(CollisionGroupId group_id) {
		Log_assert(0 <= group_id && group_id < 32);
		mask |= (1 << group_id);
	}

	/// 指定グループとの衝突を無効にする
	void dont_collide_with(CollisionGroupId group_id) {
		Log_assert(0 <= group_id && group_id < 32);
		mask &= ~(1 << group_id);
	}

	/// 指定されたグループとの衝突が有効かどうか
	bool is_collidable_with(CollisionGroupId group_id) const {
		Log_assert(0 <= group_id && group_id < 32);
		return (mask & (1 << group_id)) != 0;
	}

	void set_name(const Path &n) {
		name = n;
	}

	void set_name_on_inspector(const Path &n) {
		name_on_inspector = n;
	}

	int   mask; /// 判定対象のグループ番号目のビットを立てたマスク整数を指定する。つまり現在の時点では 32 グループまで対応可能である
	Path  name; /// グループ名（内部管理のための名前
	Path  name_on_inspector; /// グループ名（デバッガに表示する時のグループ名
	Color gizmo_color; /// デバッグモードで衝突判定を描画する時の色
	bool  gizmo_visible;
};

class ICollisionCallback {
public:
	/// 衝突判定処理の開始時に一度だけ呼ばれる
	virtual void on_collision_update_start() {}

	/// 衝突判定処理の終了時に一度だけ呼ばれる
	virtual void on_collision_update_end() {}

	/// 移動物体同士の衝突判定時に呼ばれる。
	/// 判定を無視するなら deny に true をセットする
	/// deny の値を true にすると衝突判定を無視する
	virtual void on_collision_update_body_each(EID e1, EID e2, bool *deny) {}

	/// 移動物体と固定物体の衝突判定時に呼ばれる。
	/// 判定を無視するなら deny に true をセットする
	/// deny の値を true にすると衝突判定を無視する
	virtual void on_collision_update_body_and_static(EID dy, EID st, bool *deny) {}
	
	/// センサー領域同士の判定時に呼ばれる。
	/// e1 の判定 box1 と、e2 の判定 box2 が衝突可能な組み合わせであれば true を返す。
	/// 判定を無視するなら deny に true をセットする
	/// deny の値を true にすると衝突判定を無視する
	virtual void on_collision_update_sensor(EID e1, const HitBox *box1, EID e2, const HitBox *hitbox2, bool *deny) {}

	/// 特定の組み合わせにおける衝突応答
	/// e1 衝突物体1
	/// e2 衝突物体2
	/// response1: 衝突の結果、物体1が押し戻される量（物体１を押し戻す向きに正の値）。書き換えることができる
	/// response2: 衝突の結果、物体2が押し戻される量（物体２を押し戻す向きに正の値）。書き換えることができる
	virtual void on_collision_response(EID e1, EID e2, float *response1, float *response2) {}
};

class CollisionSystem: public System, public IScreenCallback, public IDebugMenuCallback {
public:
	struct HitObject {
		HitObject() {
			entity = NULL;
			hitbox = NULL;
		}
		EID entity; ///< エンティティ
		HitBox *hitbox; ///< コライダー
		Vec3 hitbox_pos; ///< 衝突検出の瞬間における hitbox のワールド座標（衝突後の座標修正に影響されない）
	};
	struct HitPair {
		HitPair() {
			timestamp_enter = -1;
			timestamp_exit = -1;
			timestamp_lastupdate = -1;
		}
		/// 衝突した瞬間であれば true を返す
		bool isEnter() const {
			return timestamp_enter == timestamp_lastupdate;
		}
		/// 離れた瞬間であれば true を返す
		bool isExit() const {
			return timestamp_exit == timestamp_lastupdate;
		}
		/// 接触状態であれば true を返す
		bool isStay() const {
			return timestamp_enter < timestamp_lastupdate;
		}
		/// 指定されたエンティティ同士の衝突ならば、
		/// e1 の衝突情報を hit1 に、e2 の衝突情報を hit2 にセットして true を返す。
		/// e1 または e2 に NULL を指定した場合、それはエンティティが何でもよい事を示す。
		/// e1 も e2 も NULL の場合は順不同で hit1, hit2 に衝突情報をセットして true を返す。この場合戻り値は必ず true になる。
		/// 衝突情報が不要な場合は hit1, hi2 に NULL を指定できる
		bool isPairOf(const EID e1, const EID e2, HitObject *hit1=NULL, HitObject *hit2=NULL) const {
			bool match1, match2;

			// 順番通りでチェック
			match1 = (e1 == NULL || object1.entity == e1);
			match2 = (e2 == NULL || object2.entity == e2);
			if (match1 && match2) {
				if (hit1) *hit1 = object1;
				if (hit2) *hit2 = object2;
				return true;
			}
			// 入れ替えてチェック
			match1 = (e1 == NULL || object2.entity == e1);
			match2 = (e2 == NULL || object1.entity == e2);
			if (match1 && match2) {
				if (hit1) *hit1 = object2;
				if (hit2) *hit2 = object1;
				return true;
			}
			return false;
		}

		/// 指定されたグループ同士の衝突ならば true を返す
		bool isPairOfGroup(int group_index1, int group_index2) const {
			if (object1.hitbox && object2.hitbox) {
				if (object1.hitbox->group_id() == group_index1 && object2.hitbox->group_id() == group_index2) return true;
				if (object1.hitbox->group_id() == group_index2 && object2.hitbox->group_id() == group_index1) return true;
			}
			return false;
		}

		HitObject object1;        ///< 能動的判定側。攻撃側、イベントセンサー側など
		HitObject object2;        ///< 受動的判定側。ダメージ側、センサーに感知される物体など
		int timestamp_enter;      ///< 衝突開始時刻 (CollisionSystem::frame_counter_ の値)
		int timestamp_exit;       ///< 衝突解消時刻。衝突が継続している場合は -1
		int timestamp_lastupdate; ///< 最終更新時刻
	};

	CollisionSystem();
	virtual ~CollisionSystem();
	void init(Engine *engine);
	virtual void onStart() override;
	virtual void onEnd() override;
	virtual bool setParameter(EID e, const char *key, const char *val) override;
	virtual void onEntityInspectorGui(EID e) override;
	virtual bool onAttach(EID e, const char *type) override;
	virtual void onFrameStart() override;
	virtual void onDetach(EID e) override;
	virtual void onFrameUpdate() override;
	virtual void onInspectorGui() override;
	virtual void onDrawDebugInfo(const DebugRenderParameters &p) override;
	virtual bool onIsAttached(EID e) override { return _getNode(e) != NULL; }
	/// IDebugMenuCallback
	virtual void onDebugMenuSystemMenuItemClicked(IDebugMenuCallback::Params *p) override;
	virtual void onDebugMenuSystemMenuItemShow(IDebugMenuCallback::Params *p) override;

	/////////////////////////////////////////////
	void attachVelocity(EID e);
	bool isVelocityAttached(EID e) const;
	void updateVelocityInspector(EID e);

	/////////////////////////////////////////////
	void     attachCollider(EID e);
	HitBox * getHitBoxByIndex(EID e, int index);
	int      getHitBoxCount(EID e);
	HitBox * getHitBox(EID e, CollisionGroupId group_id);
	HitBox * addHitBox(EID e, CollisionGroupId group_id);
	HitBox * addHitBox(EID e, const HitBox &hitbox);
	bool     isColliderAttached(EID e) const;
	void     setColliderEnabled(EID e, bool b);
	void     setHitBoxEnabled(EID e, CollisionGroupId id, bool value);
	void     updateCollisionInspector(EID e);
	Vec3 getHitBoxCenterPosInWorld(EID e, const HitBox *hb) const;

	/////////////////////////////////////////////
	void attachBody(EID e);
	bool getBodyAabb(EID e, Vec3 *_min, Vec3 *_max) const;// エンティティ座標を原点としたときのAABBを得る

	/// Body を持っているなら、その高度を得る。
	/// ※高度とは、剛体の底面高さと、その直下にある地面高さの差を表す
	/// Bodyが存在しないか、Bodyの真下に地面が全く存在せずに高度を定義できない場合、false を返す
	bool getBodyAltitude(EID e, float *alt) const;

	const Vec3 & getBodyCenter(EID e) const;
	bool getBodyDesc(EID e, BodyDesc *desc) const;

	// isGrounded() が true の場合、地面として接触しているエンティティを返す
	EID getBodyGroundEntity(EID e) const;

	Vec3 getBodyCenterPosInWorld(EID e) const;
	bool getBodyShape(EID e, BodyShape *shape) const;

	// ボディ形状の中心を原点としたときのAABBを得る
	bool getBodyShapeLocalAabb(EID e, Vec3 *_min, Vec3 *_max) const;

	int  getBodyBounceCount(EID e) const;
	bool isBodyAttached(EID e) const;
	bool isBodyEnabled(EID e) const;
	bool isBodySleeping(EID e) const;
	bool isBodyStatic(EID e);
	bool isBodyGrounded(EID e) const;
	// XZ平面に対して垂直な壁を作る
	// x0, z0, x1, z1 はワールド座標で、この線分をＹ軸方向に無限に伸ばした壁を定義する
	// このメソッドは必要に応じて transform コンポーネントの値を変更する
	void makeBodyXZWall(EID e, float x0, float z0, float x1, float z1);
	void setBodyCenter(EID e, const Vec3 &value);
	void setBodyDesc(EID e, const BodyDesc *desc);
	void setBodyEnabled(EID e, bool b);
	void setBodyShape(EID e, const BodyShape *shape);

	// 剛体に対してかかる重力加速度を指定する（Y軸下向きの値）
	void setBodyGravity(EID e, float grav);

	// 剛体の形を ShapeType_None にする。
	// 他の剛体とは衝突しないが、決められた速度、重力にしたがって運動する
	void setBodyShapeNone(EID e);

	// 剛体の形を ShapeType_Box にする
	void setBodyShapeBox(EID e, const Vec3 &halfsize);
	bool getBodyShapeBox(EID e, Vec3 *halfsize) const;

	// 剛体の形を ShapeType_ShearedBox にする
	void setBodyShapeShearedBox(EID e, const Vec3 &halfsize, float shearx);
	bool getBodyShapeShearedBox(EID e, Vec3 *halfsize, float *shearx) const;

	// 剛体の形を ShapeType_Capsule にする
	void setBodyShapeCapsule(EID e, float radius, float halfheight);
	bool getBodyShapeCapsule(EID e, float *radius, float *halfheight) const;

	// 剛体の形を ShapeType_Ground にする。
	// 地面の法線は常にＹ軸上向きで、衝突が発生した場合には着地扱いになる
	void setBodyShapeGround(EID e);

	// 剛体の形を ShapeType_Plane にする。
	// 無限に続く平面で、法線の向きは norma になる
	void setBodyShapePlane(EID e, const Vec3 &normal);
	bool getBodyShapePlane(EID e, Vec3 *normal) const;

	// 剛体の形を ShapeType_Shpere にする
	void setBodyShapeSphere(EID e, float radius);
	bool getBodyShapeSphere(EID e, float *radius) const;

	void setBodySleep(EID e, int duration);
	void updateBodyInspector(EID e);

	// オブジェクトの高度情報を更新する。
	// 通常ならば高度情報はコリジョン状態を１フレーム進めないと更新されないが、
	// ステージをロードして地形を構築した直後（同じフレーム内で）ステージ内に配置したオブジェクトの高度を
	// 知りたい場合に使う
	void refreshAltitudes();

	/////////////////////////////////////////////
	void setCallback(ICollisionCallback *cb);


	/// 指定された範囲内にあるエンティティ一覧を得る
	int findEntitiesInRange(EntityList &entities, const Vec3 &center, float radius) const;

	const HitPair * isSensorHit(EID e1, EID e2) const;
	int getSensorHitPairCount();
	const HitPair * getSensorHitPair(int index) const;
	const HitPair * isBodyHit(EID e1, EID e2) const;

	/// 指定された地点 point の真下にある地面を探し、そこからの高度を得る。
	/// 点の真下に地面が存在しない場合は -1 を返す。（それ以外にこの関数が負の値を返すことはない）
	/// ただし、max_penetration 距離までならば指定地点の「上側」の地面も検出する。つまり地面から max_penetration までならば「めり込んで」いても良い。
	/// max_penetration には 0 以上の値を指定する。
	float getAltitudeAtPoint(const Vec3 &point, float max_penetration=1.0f) const;

	/// 指定された地点 point の真下にある地面を探す。
	/// 地面が存在すれば、その地面の y 座標を out_ground_y にセットして true を返す。
	/// 点の真下に地面が存在しない場合は何もせずに false を返す。
	/// この関数は pos よりも「下側」の地面を探すが、max_penetration 距離までならば指定地点の「上側」の地面も検出する。
	/// つまり地面から max_penetration までならば「めり込んで」いても良い。
	/// max_penetration には 0 以上の値を指定する。
	bool getGroundPoint(const Vec3 &pos, float max_penetration, float *out_ground_y) const;

	const CollisionGroup * getGroupInfo(CollisionGroupId group_id) const;
	const CollisionGroupId getGroupIdByName(const Path &name) const;

	/// 衝突判定のグループ情報をセットする
	/// グループの組み合わせによって衝突判定するかどうかを決めることができる
	void setGroupInfo(CollisionGroupId group_id, const CollisionGroup &info);

	void setSpeed(EID e, const Vec3 &spd);
	const Vec3 & getSpeed(EID e) const;

	// 実際に座標を移動させるときに使う速度。
	// getSpeed() による設定上の速度と、実際に移動させるべき速度が異なるときに使う
	// （例えば、特定のキャラクタのみがスローモーションになる時など）
	// すなわち、getSpeedFactor() の影響を受ける。
	// もともと設定されていた速度が 10.0 だった場合、setSpeedFacotr(0.4) で移動速度を40%に設定すると
	// getRealSpeed() は 4.0 を返すが、getSpeed() は 10.0 のままである
	Vec3 getRealSpeed(EID e);

	void setSpeedFactor(EID e, float f);
	float getSpeedFactor(EID e) const;

	// 前フレームでの座標
	Vec3 getPrevPosition(EID e) const;

	// 前々フレームでの座標
	Vec3 getPrevPrevPosition(EID e) const;

	// 前フレームでの速度から算出した、実際の加速度
	Vec3 getActualAccel(EID e) const;

	// 前フレームでの位置から算出した、実際の移動量
	Vec3 getActualSpeed(EID e);

	// 「前フレームでの位置」として使う値を更新する
	void _setPrevPosition(EID e, const Vec3 &pos);

	// 手前から奥に向かう（あるいは逆）壁に当たった時、
	// Z方向にスライドしないようにする
	bool no_slide_z_;

private:
	// エンティティノード。１エンティティにつき１ノード作成される
	struct _Vel {
		_Vel() {
			speed_scale = 1.0f;
			enabled = true;
			is_attached = false;
		}
		Vec3 speed;
		Vec3 prev_pos;
		Vec3 prev_prev_pos;
		float speed_scale;
		bool enabled;
		bool is_attached; // 「速度」がエンティティにアタッチされているかどうか。false の場合、速度に関する一切の操作は無効になる
	};
	struct _Col {
		_Col() {
			enabled = true;
			is_attached = false;
		}
		std::vector<HitBox> hitboxes;
		bool enabled;
		bool is_attached; // 「衝突判定」がエンティティにアタッチされているかどうか。false の場合、衝突判定に関する一切の操作は無効になる
	};
	/// 物理ボディコンポーネントを与えたエンティティ同士は、
	/// 互いに衝突し、位置や速度に影響を及ぼすことができる
	struct _Body {
		_Body() {
			enabled = true;
		}
		BodyState state;
		BodyDesc desc;
		BodyShape shape;
		Vec3 center;
		bool enabled;
	};
	struct _Node {
		_Node() {
			entity = NULL;
			body = NULL;
		}
		bool is_static() { return !vel.is_attached; }
		EID entity;
		_Body *body;
		_Vel vel;
		_Col col;

		Vec3 tmp_pos;
		bool tmp_is_statc;
	};
	// コリジョンノード。１ヒットボックスにつき１ノード作成される。
	// エンティティが複数のヒットボックスを持つ場合があることに注意
	struct _HitBoxNode {
		_HitBoxNode() {
			entity = NULL;
			hitbox = NULL;
		};
		EID entity;
		HitBox *hitbox;
	};
	bool getAltitudeBaseFloorY(const _Node &dyNode, const _Node &stNode, float *floor_y);
	void processLanding(_Node &dyNode, const _Node &stNode, const Vec3 &dy_spd, bool no_bounce);
	bool getAltitudeAtPointEx(const Vec3 &point, float max_penetration, float *alt, _Node *ground) const;
	bool getGroundPointEx(const Vec3 &pos, float max_penetration, float *out_ground_y, _Node *out_ground_node) const;
	bool _isStatic(const _Node *node) const;

	_Body * _getBody(EID e);
	const _Body * _getBody(EID e) const;
	void _remove(EID e);
	Vec3 _cnGetAabbCenterInWorld(const _Node &cNode) const;
	Vec3 _cnGetPrevAabbCenterInWorld(const _Node &cNode) const;
	Vec3 _cnGetPos(EID e);
	void _cnSetPos(EID e, const Vec3 &pos);
	Vec3 _cnGetScale(EID e);
	Vec3 _cnGetSpeed(EID e);
	void _cnMove(EID e, float dx, float dy, float dz);
	void _cnSetSpeed(EID e, const Vec3 &speed);
	void _cnSetPrevPos(EID e, const Vec3 &pos);
	Vec3 _cnGetPrevPos(EID e);

	bool collideStaticBoxAndBody(const _Node &stNode, const _Node &dyNode, Vec3 *delta);
	bool collideStaticXShearedBoxAndBody(const _Node &stNode, const _Node &dyNode, Vec3 *delta, int *hit_edge);
	bool collideStaticYAxisWallAndBody(const _Node &stNode, const _Node &dyNode, const Vec3 &dySpeed, Vec3 *delta);
	void updateHitboxInspector(_Node *node, HitBox *hitbox);
	bool isDebugVisible(EID e, const View &view);
	_Node * _getNode(EID e);
	const _Node * _getNode(EID e) const;

	// 高度情報を初期化する
	void clearBodyAltitudes();

	// 地形用オブジェクトリストを更新する。
	// これを呼ぶと、内部の地形オブジェクトリストが最新の状態になる
	void updateStaticBodyList();

	void drawGizmo_AabbVerticalDashLines(Gizmo *gizmo, const Matrix4 &matrix, const Vec3 &min_point_in_world, const Vec3 &max_point_in_world, const Color &color);
	void drawGizmo_PhyiscsStaticNode(Gizmo *gizmo, _Node &cNode, const View &view);
	void drawGizmo_PhyiscsDynamicNode(Gizmo *gizmo, const _Node &cNode, const View &view);
	bool _isHit(const HitBox *hb, const EID e) const;
	void drawGizmo_SensorBoxes(Gizmo *gizmo, EID entity, const _Node &eNode, const View &view);
	Vec3 getHitBoxCenterPosInWorld(const _HitBoxNode &cNode) const;
	void updatePhysicsNodeList();
	void updatePhysicsPositions();
	void physicalCollisionEnter(HitPair &pair);
	void physicalCollisionStay(HitPair &pair);
	void physicalCollisionExit(HitPair &pair);
	void updatePhysicsBodyCollision();
	void updateTerrainCollision_BoxWall();
	void updateTerrainCollision_ShearedBoxWall();
	void updateTerrainCollision_PlaneWall();
	void updateTerrainCollision_Ground(bool no_bounce);
	void updateTerrainCollision();
	void updatePhysicsCollisionPair(const _Node &body_node, const _Node &wall_node);
	int getSensorHitPairIndex(const HitBox *hitbox1, const HitBox *hitbox2) const;
	int getBodyHitPairIndex(EID e1, EID e2) const;
	void updateSensorNodeList();
	void updateSensorCollision();
	void updateSensorCollision_group(CollisionGroupId group_id1, CollisionGroupId group_id2);
	void updateSensorCollision_node(const _HitBoxNode &cNode1, const _HitBoxNode &cNode2);
	void sensorCollisionEnter(HitPair &pair);
	void sensorCollisionStay(HitPair &pair);
	void sensorCollisionExit(HitPair &pair);
	void beginUpdate();
	void endUpdate();
	void simpleMove(_Node &node);

	std::vector<_Node> tmp_table_;
	std::vector<_Node> moving_nodes_;
	std::vector<_Node> dynamic_bodies_;
	std::vector<_Node> static_bodies_;
	std::vector<std::vector<_HitBoxNode>> group_nodes_;
	std::vector<_Node> tmp_static_boxes_;
	std::vector<_Node> tmp_static_shearedboxes_;
	std::vector<_Node> tmp_static_walls_;
	std::vector<_Node> tmp_static_floors_;
	std::unordered_map<EID , _Node> nodes_;
	int frame_counter_;
	std::vector<HitPair> body_hitpairs_;
	std::vector<HitPair> sensor_hitpairs_;
	std::vector<CollisionGroup> groups_;
	ICollisionCallback *cb_;
	Engine *engine_;
	HierarchySystem *hs_;
	bool show_physics_gizmos_;
	bool show_sensor_gizmos_;
	bool highlight_collision_;
};

void Test_collision();

} // namespace
