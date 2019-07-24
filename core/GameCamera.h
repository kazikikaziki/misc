#pragma once
#include "KRect.h"
#include "Screen.h"
#include "system.h"
#include <unordered_map>

namespace mo {

struct Material;
class Engine;
class HierarchySystem;

class CameraSystem : public System {
public:
	CameraSystem();

	void init(Engine *engine);
	virtual void onEntityInspectorGui(EID e) override;
	virtual void onInspectorGui() override;
	virtual void onFrameUpdate() override;
	virtual void onAppFrameUpdate() override;

	void attach(EID e);
	virtual void onStart() override;
	virtual bool onAttach(EID e, const char *type) override;
	virtual void onDetach(EID e) override;
	virtual bool onIsAttached(EID e) override { return _getNode(e) != NULL; }

	void setSort(EID e, RenderingOrder order);

	/// 指定されたオブジェクトレイヤーに所属するカメラを返す
	/// カメラが無い場合は NULL を返す
	/// @see CameraCo::setTargetFilter()
	EID getCameraForObjectLayer(int layer) const;

	/// 指定されたワールド座標をカメラで映したときの位置を、
	/// スクリーン座標(中央が(0, 0)で右上を(1, 1)とする)で取得して pos にセットする。
	/// 成功すれば true を返す。カメラが NULL だったり、camera が非カメラだった場合は false を返す
	bool worldToScreenPoint(const Vec3 &worldpos, EID camera, Vec3 *point_in_screen) const;

	/// スクリーン座標(中央が(0, 0)で右上を(1, 1)とする) を
	/// カメラローカル座標に変換する。
	/// （ただし、入力座標のZ値は近クリップ面からの距離とみなす）
	Vec3 screenToWorldPoint(const Vec3 &point_in_screen, EID camera_e) const;

	bool screenToWorldVector(const Vec3 &point_in_screen, EID camera_e, Vec3 *point_in_world, Vec3 *normalized_direction_in_world) const;

	/// e の描画を担当しているカメラを返す
	EID getCameraForEntity(EID e) const;

	/// 指定されたＡＡＢＢ範囲を持つ物体が camera_e によって描画される可能性があるなら true を返す
	bool aabbWillRender(const Vec3 &minpoint_inworld, const Vec3 &maxpoint_inworld, EID camera_e) const;

	/// エンティティがカメラ camera_e によって描画される可能性があるなら true を返す
	bool isTargetOf(EID e, EID camera) const;
	bool isTargetOf(EID e, const View &view) const;

	/// カメラを描画順に並べたものを返す。
	/// @see CameraCo::setOrderNumber()
	int getCameraInOrder(EntityList &camera_in_order) const;
	int getViewInOrder(std::vector<View> &viewlist) const;

	/// カメラ設定をコピーする
	/// dst コピー先
	/// src コピー元
	void copyCamera(EID dst, EID src);


	/// カメラの描画序列。値の低いカメラが最初に働き、値が高いほど後から描画される。
	/// この値はソート時の比較関数内で使われるだけなので、値の大小関係さえ間違っていなければどんな値でもよい
	void setOrderNumber(EID e, int n);
	int getOrderNumber(EID e) const;

	bool getView(EID e, View *view) const;
	void setView(EID e, const View *view);

	void setZoom(EID e, float value);
	float getZoom(EID e) const;

private:
	struct Node {
		Node() {
			entity_ = NULL;
			enabled_ = true;
		}
		View view_;
		EID entity_;
		bool enabled_;
	};

	// カメラを描画順に並べる
	class SortPred {
	public:
		const CameraSystem *cs_;

		bool operator()(Node *co1, Node *co2) const {
			return cs_->sortPred(co1, co2);
		}
	};

	bool sortPred(Node *co1, Node *co2) const;
	Node * _getNode(EID e);
	const Node * _getNode(EID e) const;
	mutable std::vector<Node *> tmp_;
	mutable std::unordered_map<EID , Node> nodes_;
	Engine *engine_;
	HierarchySystem *hs_;
};


}