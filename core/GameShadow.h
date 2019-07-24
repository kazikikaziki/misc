#pragma once
#include "KVec3.h"
#include "KVideoDef.h"
#include "system.h"

namespace mo {

class Engine;
class CollisionSystem;
class HierarchySystem;
class RenderSystem;
class Matrix4;

/// 2D用の簡易影管理
class ShadowSystem: public System {
public:
	ShadowSystem();
	void init(Engine *engine);
	void attach(EID e);
	virtual bool onAttach(EID e, const char *type) override;
	virtual void onStart() override;
	virtual void onDetach(EID e) override;
	virtual bool onIsAttached(EID e) override { return _getNode(e) != NULL; }
	virtual void onFrameUpdate() override;
	virtual void onInspectorGui() override;
	virtual void onEntityInspectorGui(EID e) override;

	void setEnabled(EID e, bool value);
	void setOffset(EID e, const Vec3 &value);
	void setRadius(EID e, float horz, float vert);
	bool getRadius(EID e, float *horz, float *vert);
	void setScaleFactor(EID e, float value);
	void setMatrix(EID e, const Matrix4 &matrix);

	// 高度による自動スケーリング値を使う
	// 有効にした場合、max_height の高さでの影のサイズを 0.0、
	// 接地しているときの影のサイズを 1.0 としてサイズが変化する
	// maxheight が 0 の場合は規定値を使う
	void setScaleByHeight(EID e, bool value, float maxheight=0);

	EID createShadowEntity(EID parent);
	bool getAltitude(EID e, float *altitude) const;
	void updateMesh();

	float default_radius_x_;
	float default_radius_y_;
	int world_layer_;
	bool use_y_position_as_altitude_;

private:
	struct _Node {
		EID shadow_e;
		Vec3 offset;
		float radius_x;
		float radius_y;
		float scale;
		float max_height;
		bool scale_by_height;
		bool enabled;
	};
	_Node * _getNode(EID e);
	bool _computeShaodwPositionAndScale(EID owner_e, _Node &node, Vec3 *p, Vec3 *s) const;
	std::unordered_map<EID , _Node> nodes_;
	Engine *engine_;
	HierarchySystem *hs_;
	CollisionSystem *cs_;
	RenderSystem *rs_;
	int vertex_count_; // 影の楕円の頂点数
	float max_height_; // 影が映る最大高度のデフォルト値
	Color color_; // 影の色
	Blend blend_; // 影の合成方法
	bool scale_by_height_; // 高度によって影の大きさを変える
};


} // namespace